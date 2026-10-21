// Local movie writer. One acknowledged PNG at a time, followed by an external
// FFmpeg process. No encoder source/binaries are vendored into KerrLab.
const fs = require('node:fs/promises');
const path = require('node:path');
const { spawn } = require('node:child_process');

function validateOptions(input) {
  if (!input || typeof input !== 'object') throw new Error('Missing export settings.');
  const { width, height, fps, duration, format, event } = input;
  if (!Number.isInteger(width) || !Number.isInteger(height) || width < 16 || height < 16 ||
      width > 3840 || height > 2160 || width % 2 || height % 2) {
    throw new Error('Use even dimensions between 16×16 and 3840×2160.');
  }
  if (![24, 30, 60, 144].includes(fps) || !Number.isInteger(duration) || duration < 1 || duration > 120) {
    throw new Error('Use 24, 30, 60 or 144 FPS and a duration of 1–120 whole seconds.');
  }
  if (!['mp4', 'png'].includes(format)) throw new Error('Unsupported export format.');
  if (typeof event !== 'string' || !/^GW[\w-]{1,60}$/.test(event)) throw new Error('Invalid merger event.');
  return { width, height, fps, duration, format, event, frameCount: fps * duration };
}

function encoderArgs(config, output = 'movie.partial.mp4') {
  return ['-hide_banner', '-nostdin', '-v', 'error', '-n', '-framerate', String(config.fps),
    '-start_number', '0', '-i', 'frames/frame-%06d.png', '-frames:v', String(config.frameCount),
    '-an', '-c:v', 'libx264', '-preset', 'medium', '-crf', '17', '-pix_fmt', 'yuv420p',
    '-movflags', '+faststart', output];
}

function runProcess(command, args, { cwd, job, timeout = 30 * 60_000 } = {}) {
  return new Promise((resolve, reject) => {
    if (job?.cancelled) return reject(new Error('Export cancelled.'));
    const child = spawn(command, args, { cwd, windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] });
    if (job) job.child = child;
    let stdout = '', stderr = '', timedOut = false;
    child.stdout.on('data', data => { stdout = (stdout + data).slice(-128_000); });
    child.stderr.on('data', data => { stderr = (stderr + data).slice(-32_000); });
    const timer = setTimeout(() => { timedOut = true; child.kill('SIGKILL'); }, timeout);
    child.once('error', error => { clearTimeout(timer); reject(error); });
    child.once('close', (code, signal) => {
      clearTimeout(timer);
      if (job?.child === child) job.child = null;
      if (job?.cancelled) reject(new Error('Export cancelled.'));
      else if (timedOut) reject(new Error(`${command} timed out.`));
      else if (code !== 0) reject(new Error(`${command} failed (${code ?? signal}): ${stderr.trim()}`));
      else resolve({ stdout, stderr });
    });
  });
}

class MovieExporter {
  constructor({ ffmpeg = 'ffmpeg', ffprobe = 'ffprobe' } = {}) {
    this.ffmpeg = ffmpeg;
    this.ffprobe = ffprobe;
    this.job = null;
    this.preparing = false;
    this.latestFolder = null;
  }

  get active() { return this.preparing || Boolean(this.job && !this.job.terminal); }

  async capabilities() {
    try {
      const version = await runProcess(this.ffmpeg, ['-version'], { timeout: 10_000 });
      const encoders = await runProcess(this.ffmpeg, ['-hide_banner', '-encoders'], { timeout: 10_000 });
      if (!/\blibx264\b/.test(encoders.stdout)) throw new Error('FFmpeg has no libx264 encoder.');
      const probe = await runProcess(this.ffprobe, ['-version'], { timeout: 10_000 });
      return { mp4: true, ffmpeg: version.stdout.split('\n')[0], ffprobe: probe.stdout.split('\n')[0] };
    } catch (error) {
      return { mp4: false, reason: `MP4 needs FFmpeg with libx264 and ffprobe on PATH. ${error.message}` };
    }
  }

  async start(parent, input, metadata = {}) {
    if (this.active) throw new Error('An export is already running.');
    this.preparing = true;
    let created = null;
    try {
      const config = validateOptions(input);
      const metadataText = JSON.stringify(metadata);
      if (!metadataText || metadataText.length > 128_000) throw new Error('Invalid scene metadata.');
      const encoder = config.format === 'mp4' ? await this.capabilities() : null;
      if (encoder && !encoder.mp4) throw new Error(encoder.reason);
      const disk = await fs.statfs(parent);
      const estimate = config.width * config.height * 4 * config.frameCount;
      // Keep the lossless sequence plus room for the movie and filesystem overhead.
      if (disk.bavail * disk.bsize < estimate * 1.2 + 256 * 1024 ** 2) {
        throw new Error(`Allow approximately ${(estimate * 1.2 / 1024 ** 3 + 0.25).toFixed(1)} GiB free for this export.`);
      }
      const folder = await fs.mkdtemp(path.join(parent, `KerrLab-${config.event}-`));
      this.latestFolder = folder;
      await fs.mkdir(path.join(folder, 'frames'));
      this.job = { folder, config, metadata: JSON.parse(metadataText), encoder, next: 0,
        status: 'rendering', cancelled: false, terminal: false, operation: null, child: null,
        startedAt: new Date().toISOString() };
      created = this.job;
      await this.manifest(this.job);
      return { folder, ...config };
    } catch (error) {
      if (created) {
        created.terminal = true; created.status = 'failed'; created.error = error.message;
        await this.manifest(created).catch(() => {});
      }
      throw error;
    } finally { this.preparing = false; }
  }

  async manifest(job) {
    const record = { schemaVersion: 1, ...job.config, status: job.status,
      completedFrames: job.next, startedAt: job.startedAt, updatedAt: new Date().toISOString(),
      scene: job.metadata, encoder: job.encoder, error: job.error, verification: job.verification,
      pngPattern: 'frames/frame-%06d.png', movie: job.status === 'complete' && job.config.format === 'mp4' ? 'merger.mp4' : null,
      recovery: { command: this.ffmpeg, args: encoderArgs({ ...job.config, frameCount: job.next }, 'recovered.mp4'),
        note: 'Run in this folder. A partial sequence produces a shorter clip; original PNGs are retained.' } };
    const temporary = path.join(job.folder, 'manifest.partial.json');
    await fs.writeFile(temporary, JSON.stringify(record, null, 2) + '\n');
    await fs.rename(temporary, path.join(job.folder, 'manifest.json'));
  }

  requireJob() {
    const job = this.job;
    if (!job || job.terminal || job.cancelled) throw new Error('No active export.');
    if (job.operation) throw new Error('Wait for the previous frame or encoding operation.');
    return job;
  }

  async operate(job, work) {
    // Cancellation waits for both the work and its failure journal. Waiting only
    // for work races two atomic-manifest renames when an encoder is killed.
    const operation = (async () => {
      try { return await work(); }
      catch (error) {
        job.error = error.message;
        job.status = job.cancelled ? 'cancelled' : 'failed';
        job.terminal = true;
        await this.manifest(job).catch(() => {});
        throw error;
      } finally { job.operation = null; }
    })();
    job.operation = operation;
    return operation;
  }

  async append(index, bytes) {
    const job = this.requireJob();
    return this.operate(job, async () => {
      const { width, height, frameCount } = job.config;
      if (job.status !== 'rendering' || index !== job.next || index >= frameCount) throw new Error('Frame is missing, duplicated or out of order.');
      if (!(bytes instanceof Uint8Array)) throw new Error('PNG bytes required.');
      const png = Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength);
      if (png.length < 45 || png.length > width * height * 5 + 65536 ||
          !png.subarray(0, 8).equals(Buffer.from([137, 80, 78, 71, 13, 10, 26, 10])) ||
          png.toString('ascii', 12, 16) !== 'IHDR' || png.readUInt32BE(16) !== width || png.readUInt32BE(20) !== height) {
        throw new Error('Frame is not a PNG at the requested output dimensions.');
      }
      const disk = await fs.statfs(job.folder);
      if (disk.bavail * disk.bsize < png.length + 128 * 1024 ** 2) throw new Error('Disk is nearly full; completed frames were retained.');
      const filename = path.join(job.folder, 'frames', `frame-${String(index).padStart(6, '0')}.png`);
      await fs.writeFile(`${filename}.partial`, png, { flag: 'wx' });
      await fs.rename(`${filename}.partial`, filename);
      job.next++;
      await this.manifest(job);
      return { completedFrames: job.next };
    });
  }

  async finish() {
    const job = this.requireJob();
    return this.operate(job, async () => {
      if (job.next !== job.config.frameCount) throw new Error('Cannot finish an incomplete frame sequence.');
      if (job.config.format === 'mp4') {
        job.status = 'encoding';
        await this.manifest(job);
        await runProcess(this.ffmpeg, encoderArgs(job.config), { cwd: job.folder, job });
        job.status = 'verifying';
        await this.manifest(job);
        const probe = await runProcess(this.ffprobe, ['-v', 'error', '-count_frames', '-select_streams', 'v:0',
          '-show_entries', 'stream=width,height,nb_read_frames,avg_frame_rate,duration', '-of', 'json', 'movie.partial.mp4'],
        { cwd: job.folder, job });
        const stream = JSON.parse(probe.stdout).streams?.[0];
        const config = job.config;
        const [numerator, denominator] = String(stream?.avg_frame_rate).split('/').map(Number);
        if (!stream || stream.width !== config.width || stream.height !== config.height ||
            Number(stream.nb_read_frames) !== config.frameCount || numerator / denominator !== config.fps ||
            !Number.isFinite(Number(stream.duration)) || Math.abs(Number(stream.duration) - config.duration) > 0.001) {
          throw new Error('Encoded video failed frame count, dimensions, FPS or duration verification.');
        }
        await runProcess(this.ffmpeg, ['-v', 'error', '-xerror', '-nostdin', '-i', 'movie.partial.mp4', '-f', 'null', '-'],
          { cwd: job.folder, job });
        if (job.cancelled) throw new Error('Export cancelled.');
        job.verification = { ...stream, fullyDecoded: true };
        await fs.rename(path.join(job.folder, 'movie.partial.mp4'), path.join(job.folder, 'merger.mp4'));
      }
      job.status = 'complete';
      job.terminal = true;
      await this.manifest(job);
      return { folder: job.folder, movie: job.config.format === 'mp4' ? path.join(job.folder, 'merger.mp4') : null,
        frames: job.next, verification: job.verification };
    });
  }

  async cancel(reason = 'Cancelled by user.') {
    const job = this.job;
    if (job?.cancellation) return job.cancellation;
    if (!job || job.terminal) return { folder: job?.folder ?? this.latestFolder };
    job.cancelled = true;
    job.cancellation = this.cancelJob(job, reason);
    return job.cancellation;
  }

  async cancelJob(job, reason) {
    job.child?.kill('SIGTERM');
    const child = job.child;
    const force = setTimeout(() => { if (child && job.child === child) child.kill('SIGKILL'); }, 2000);
    try { await job.operation; } catch { /* operation writes its own error */ }
    finally { clearTimeout(force); }
    // Cancellation may arrive just after the last awaited filesystem operation.
    job.status = 'cancelled';
    job.error = reason;
    job.terminal = true;
    await this.manifest(job);
    return { folder: job.folder };
  }
}

module.exports = { MovieExporter, validateOptions, encoderArgs };
