const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const { execFile } = require('node:child_process');
const { promisify } = require('node:util');
const { PNG } = require('pngjs');
const { MovieExporter, validateOptions } = require('../desktop/movie-export.cjs');
const execute = promisify(execFile);
const config = { event: 'GW150914', width: 64, height: 36, fps: 60, duration: 12, format: 'png' };

async function workspace(t) {
  const folder = await fs.mkdtemp(path.join(os.tmpdir(), 'kerr-movie-test-'));
  t.after(() => fs.rm(folder, { recursive: true, force: true }));
  return folder;
}
function frame(index, width = config.width, height = config.height) {
  const image = new PNG({ width, height });
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
    const offset = (y * width + x) * 4;
    image.data[offset] = (x + index) % 256;
    image.data[offset + 1] = y * 5;
    image.data[offset + 2] = index % 256;
    image.data[offset + 3] = 255;
  }
  return PNG.sync.write(image);
}

test('export settings reject unbounded workloads, unsafe event names and invalid frame rates', () => {
  for (const invalid of [{ fps: 0 }, { fps: 120 }, { duration: Infinity }, { duration: 0 },
    { duration: 1.5 }, { width: 16384 }, { width: 63 }, { event: '../../x' }, { format: 'exe' }]) {
    assert.throws(() => validateOptions({ ...config, ...invalid }));
  }
  assert.equal(validateOptions({ ...config, fps: 144 }).frameCount, 1728);
});

test('writer refuses duplicate or mismatched frames and retains previously saved PNGs', async t => {
  const parent = await workspace(t);
  const exporter = new MovieExporter();
  const job = await exporter.start(parent, config, { example: true });
  await exporter.append(0, frame(0));
  await assert.rejects(exporter.append(0, frame(1)), /out of order/);
  const manifest = JSON.parse(await fs.readFile(path.join(job.folder, 'manifest.json')));
  assert.equal(manifest.status, 'failed');
  assert.equal(manifest.completedFrames, 1);
  assert.equal(PNG.sync.read(await fs.readFile(path.join(job.folder, 'frames/frame-000000.png'))).width, 64);
  await exporter.start(parent, config);
  await assert.rejects(exporter.append(0, frame(0, 32, 32)), /dimensions/);
});

test('cancellation waits for a pending write, is idempotent and permits a fresh export', async t => {
  const parent = await workspace(t);
  const exporter = new MovieExporter();
  const job = await exporter.start(parent, config);
  const pending = exporter.append(0, frame(0));
  await Promise.all([pending, exporter.cancel(), exporter.cancel()]);
  const manifest = JSON.parse(await fs.readFile(path.join(job.folder, 'manifest.json')));
  assert.equal(manifest.status, 'cancelled');
  assert.equal(manifest.completedFrames, 1);
  assert.equal(exporter.active, false);
  const next = await exporter.start(parent, config);
  assert.notEqual(next.folder, job.folder);
  await exporter.cancel();
});

test('incomplete sequences cannot be declared complete; unavailable encoder leaves PNG export usable', async t => {
  const parent = await workspace(t);
  const exporter = new MovieExporter({ ffmpeg: path.join(parent, 'missing-ffmpeg') });
  assert.equal((await exporter.capabilities()).mp4, false);
  await assert.rejects(exporter.start(parent, { ...config, format: 'mp4' }), /MP4 needs/);
  await exporter.start(parent, config);
  await assert.rejects(exporter.finish(), /incomplete/);
  assert.equal(exporter.active, false);
});

test('PNG-only export finishes without an encoder and keeps original bytes and scene metadata', async t => {
  const parent = await workspace(t);
  const exporter = new MovieExporter({ ffmpeg: path.join(parent, 'absent') });
  const job = await exporter.start(parent, { ...config, fps: 24, duration: 1 }, { camera: { radius: 18 } });
  for (let index = 0; index < 24; index++) await exporter.append(index, frame(index));
  const result = await exporter.finish();
  assert.equal(result.movie, null);
  assert.equal(result.frames, 24);
  assert.deepEqual(await fs.readFile(path.join(job.folder, 'frames/frame-000023.png')), frame(23));
  const manifest = JSON.parse(await fs.readFile(path.join(job.folder, 'manifest.json')));
  assert.equal(manifest.status, 'complete');
  assert.equal(manifest.scene.camera.radius, 18);
});

for (const fps of [60, 144]) {
  test(`real encoder produces exactly 12 seconds at ${fps} FPS and preserves frame presentation order`, async t => {
    const exporter = new MovieExporter();
    if (!(await exporter.capabilities()).mp4) return t.skip('FFmpeg/libx264/ffprobe unavailable');
    const parent = await workspace(t);
    const job = await exporter.start(parent, { ...config, fps, format: 'mp4' });
    for (let index = 0; index < 12 * fps; index++) await exporter.append(index, frame(index));
    const result = await exporter.finish();
    const data = JSON.parse((await execute('ffprobe', ['-v', 'error', '-show_frames',
      '-show_entries', 'frame=best_effort_timestamp_time', '-of', 'json', result.movie], { maxBuffer: 4 * 1024 ** 2 })).stdout);
    assert.equal(data.frames.length, fps * 12);
    for (let index = 0; index < data.frames.length; index++) {
      assert(Math.abs(Number(data.frames[index].best_effort_timestamp_time) - index / fps) < 1e-6);
    }
    const manifest = JSON.parse(await fs.readFile(path.join(job.folder, 'manifest.json')));
    assert.equal(manifest.status, 'complete');
    assert.equal(manifest.verification.fullyDecoded, true);
    assert.equal(Number(manifest.verification.duration), 12);
    assert.equal((await fs.readdir(path.join(job.folder, 'frames'))).length, fps * 12);
  });
}

test('cancelling during video encoding keeps all source frames and does not report completion', async t => {
  const exporter = new MovieExporter();
  if (!(await exporter.capabilities()).mp4) return t.skip('FFmpeg unavailable');
  const parent = await workspace(t);
  const job = await exporter.start(parent, { ...config, fps: 24, duration: 1, format: 'mp4' });
  for (let index = 0; index < 24; index++) await exporter.append(index, frame(index));
  const result = exporter.finish().catch(error => error);
  while (!exporter.job.child && !exporter.job.terminal) await new Promise(resolve => setTimeout(resolve, 1));
  assert(exporter.job.child, 'Cancel an actually running encoder process');
  await exporter.cancel();
  assert.match((await result).message, /cancelled/);
  const manifest = JSON.parse(await fs.readFile(path.join(job.folder, 'manifest.json')));
  assert.equal(manifest.status, 'cancelled');
  assert.equal(manifest.completedFrames, 24);
  assert.equal(exporter.active, false);
});
