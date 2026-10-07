// Run the packaged shell/assets in the pinned Electron runtime, with a separate
// profile. No dev server, remote debugging port, Playwright or sandbox overrides.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { parseArgs } = require('node:util');
const { spawn } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const { values } = parseArgs({ args: process.argv.slice(2), options: {
  help: { type: 'boolean' }, measure: { type: 'boolean' }, movie: { type: 'boolean' },
  x11: { type: 'boolean' }, wayland: { type: 'boolean' },
  'power-state': { type: 'string', default: 'not recorded' },
} });
if (values.help) {
  console.log(`Usage: node tools/check-desktop.cjs [--movie] [--measure] [--x11|--wayland] [--power-state "AC, balanced"]
Run from a normal desktop terminal after rebuilding the package.
Checks four scenes, reset, PNG encoding/save/cancel, cinema keys and resize.
--measure adds 3 s warmup + at least 10 s visible RAF timing per fixed scenario.
--movie renders small 12-second 60/144 FPS clips through the actual preload/IPC path,
checks repeatable frames and state restoration, plus the movie dialog. Allow 30 minutes.
Reports, screenshots and logs go into a new out/desktop-check-* folder.
Downloads use a test destination; the native Save dialog still needs a manual check.
Hardware graphics and a working Electron sandbox are required.`);
} else if (values.x11 && values.wayland) {
  throw new Error('Choose either --x11 or --wayland.');
} else if (!process.versions.electron) {
  fs.mkdirSync(path.join(root, 'out'), { recursive: true });
  const output = fs.mkdtempSync(path.join(root, 'out/desktop-check-'));
  const runtime = process.platform === 'linux'
    ? path.join(root, '.tools/electron-linux/electron')
    : path.join(root, 'web-demo/node_modules/electron/dist/electron.exe');
  console.log(`Desktop evidence: ${output}`);
  const env = { ...process.env, KERRLAB_CHECK_OUTPUT: output };
  delete env.ELECTRON_RUN_AS_NODE;
  const child = spawn(runtime, [__filename, ...process.argv.slice(2)], { env, stdio: ['ignore', 'pipe', 'pipe'] });
  const log = fs.createWriteStream(path.join(output, 'runtime.log'));
  child.stdout.on('data', data => { log.write(data); process.stdout.write(data); });
  child.stderr.on('data', data => { log.write(data); process.stderr.write(data); });
  child.on('error', error => { console.error(error.message); process.exitCode = 1; });
  child.on('close', (code, signal) => {
    log.end();
    let reportStatus = 'missing';
    try { reportStatus = JSON.parse(fs.readFileSync(path.join(output, 'report.json'), 'utf8')).status; } catch {}
    fs.writeFileSync(path.join(output, 'launcher.json'), JSON.stringify({ code, signal, runtime, reportStatus }, null, 2));
    process.exitCode = code === 0 && reportStatus === 'passed' ? 0 : 1;
  });
} else {
  void runElectron();
}

async function runElectron() {
  const assert = require('node:assert/strict');
  const { app, BrowserWindow, dialog } = require('electron');
  const { PNG } = require(path.join(root, 'web-demo/node_modules/pngjs'));
  const output = process.env.KERRLAB_CHECK_OUTPUT;
  if (!output || !fs.statSync(output).isDirectory()) throw new Error('Start this check with Node.');
  const profile = path.join(output, 'profile');
  fs.mkdirSync(profile);
  app.setPath('userData', profile);
  app.setPath('sessionData', profile);
  if (values.x11 || values.wayland) app.commandLine.appendSwitch('ozone-platform', values.x11 ? 'x11' : 'wayland');
  const packaged = path.join(root, 'out/desktop', process.platform === 'linux' ? 'KerrLab-Arch' : 'KerrLab');
  const report = {
    startedAt: new Date().toISOString(), status: 'running',
    host: { platform: process.platform, release: os.release(), arch: process.arch,
      electron: process.versions.electron, powerState: values['power-state'],
      requestedBackend: values.x11 ? 'x11' : values.wayland ? 'wayland' : 'default' },
    scope: 'Packaged shell/assets in pinned Electron, isolated profile; automated download destination. Native Save dialog, physical keyboard focus, minimize/restore, display scaling and HQ stills remain manual checks.',
    checks: [], measurements: [], console: [],
  };
  const saveReport = () => fs.writeFileSync(path.join(output, 'report.json'), JSON.stringify(report, null, 2) + '\n');
  const watchdog = setTimeout(() => {
    report.status = 'failed'; report.error = 'Desktop check exceeded its time limit.';
    saveReport(); app.exit(1);
  }, values.movie ? 30 * 60_000 : 300_000);
  // A native error box must not leave an unattended test waiting forever.
  // Record exactly what the real shell would show; interactive appearance is untested.
  dialog.showErrorBox = (title, message) => {
    report.console.push({ level: 'error', title, message });
  };
  const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
  async function waitFor(check, description, timeout = 45_000) {
    const deadline = Date.now() + timeout;
    do {
      if (await check()) return;
      await delay(100);
    } while (Date.now() < deadline);
    throw new Error(`Timed out: ${description}`);
  }
  let win;
  try {
    report.build = JSON.parse(fs.readFileSync(path.join(packaged, 'BUILD-INFO.json'), 'utf8'));
    assert.equal(process.versions.electron, report.build.electron, 'Pinned Electron version');
    app.on('browser-window-created', (_event, window) => {
      window.webContents.on('console-message', details => {
        if (details.level === 'error' || details.level === 'warning') {
          report.console.push({ level: details.level, message: details.message, sourceId: details.sourceId });
        }
      });
    });
    require(path.join(packaged, 'resources/app/desktop/main.cjs'));
    await app.whenReady();
    await waitFor(() => (win = BrowserWindow.getAllWindows()[0]), 'window creation');
    const contents = win.webContents;
    const evaluate = code => contents.executeJavaScript(code, true);
    await waitFor(() => evaluate('Boolean(window.__bh?.diagnostics && !document.getElementById("render-status"))'), 'renderer readiness');
    win.setContentSize(960, 640);
    win.show();
    const frame = () => evaluate('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))');
    const click = label => evaluate(`(() => {
      const button = [...document.querySelectorAll('#sceneDock button')].find(b => b.textContent === ${JSON.stringify(label)});
      if (!button) throw new Error('Missing scene/action button');
      button.click();
    })()`);
    await frame();
    report.gpu = await app.getGPUInfo('complete');
    report.initial = await evaluate('window.__bh.diagnostics()');
    assert(!/swiftshader|llvmpipe|software rasterizer/i.test(report.initial.graphics.renderer),
      `Hardware graphics required; got ${report.initial.graphics.renderer}`);
    const preferences = contents.getLastWebPreferences();
    assert.equal(preferences.sandbox, true);
    assert.equal(preferences.contextIsolation, true);
    assert.equal(preferences.nodeIntegration, false);
    assert(contents.getURL().startsWith('file:'));
    report.checks.push('Sandboxed packaged file renderer loaded');

    function decode(buffer, expected) {
      const png = PNG.sync.read(buffer);
      assert.equal(png.width, expected.width);
      assert.equal(png.height, expected.height);
      let min = 255, max = 0;
      for (let i = 0; i < png.data.length; i += 4) {
        const bright = Math.max(png.data[i], png.data[i + 1], png.data[i + 2]);
        min = Math.min(min, bright); max = Math.max(max, bright);
      }
      assert(max - min > 8, 'Image must contain more than a uniform blank color');
      return { width: png.width, height: png.height, min, max };
    }
    for (const [name, spin] of [['Event horizon', 0.6], ['Gargantua', 0.92], ['Polar orbit', 0.8], ['Extreme Kerr', 0.998]]) {
      await click(name);
      await evaluate('window.__bh.params.autoOrbit = false');
      await frame();
      const state = await evaluate('window.__bh.diagnostics()');
      assert.equal(state.settings.mode, 'kerr');
      assert.equal(state.settings.spin, spin);
      const url = await evaluate('window.__bh.capture()');
      const bytes = Buffer.from(url.split(',')[1], 'base64');
      const image = decode(bytes, state.output);
      fs.writeFileSync(path.join(output, `${name.replaceAll(' ', '-').toLowerCase()}.png`), bytes);
      report.checks.push({ scene: name, image });
    }

    // Start real DownloadItems, redirecting only their native dialog destination.
    async function preview() {
      await click('Save PNG');
      await waitFor(() => evaluate(`Boolean(document.querySelector('#capturePreview a:not([hidden])') &&
        document.querySelector('#capturePreview img').naturalWidth > 0)`), 'PNG preview');
    }
    async function download(cancel) {
      const destination = path.join(output, 'saved-preview.png');
      let done;
      const completion = new Promise(resolve => { done = resolve; });
      contents.session.once('will-download', (_event, item) => {
        item.once('done', (_doneEvent, state) => done(state));
        if (cancel) item.cancel();
        else item.setSavePath(destination);
      });
      await evaluate(`document.querySelector('#capturePreview a').click()`);
      let state;
      completion.then(value => { state = value; });
      await waitFor(() => state !== undefined, 'PNG download completion');
      assert.equal(state, cancel ? 'cancelled' : 'completed');
      if (!cancel) {
        const expected = await evaluate('window.__bh.diagnostics().output');
        decode(fs.readFileSync(destination), expected);
      }
      await evaluate(`document.getElementById('capturePreview').close()`);
      await waitFor(() => evaluate(`!document.getElementById('capturePreview')`), 'preview cleanup');
    }
    await preview(); await download(false);
    await preview(); await download(true);
    report.checks.push('Repeated PNG preview, decoded saved download and cancelled download; native dialogs bypassed');

    await evaluate(`window.__bh.merger.select('GW151226'); window.__bh.merger.setPlaying(true)`);
    await frame(); await click('Reset'); await frame();
    let state = await evaluate('window.__bh.diagnostics()');
    assert.equal(state.merger.event, null); assert.equal(state.merger.playing, false);
    await evaluate('window.__bh.release(); window.__bh.params.gridOn = true; window.__bh.params.resolutionScale = 0.25');
    await frame(); await click('Reset'); await frame();
    state = await evaluate('window.__bh.diagnostics()');
    assert.equal(state.freefall, false); assert.equal(state.settings.gridOn, false);
    assert.equal(state.settings.resolutionScale, 0.6);
    report.checks.push('Reset exits merger and free fall, restores overlays and quality');
    for (const [key, hidden] of [['h', true], ['Escape', false]]) {
      await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown', { key: ${JSON.stringify(key)}, bubbles: true }))`);
      assert.equal(await evaluate(`document.body.classList.contains('cinema-view')`), hidden);
    }
    report.checks.push('Cinema H/Escape DOM handlers');
    const beforeResize = state.viewport;
    win.setContentSize(1024, 700); await frame();
    state = await evaluate('window.__bh.diagnostics()');
    assert.equal(state.output.width, Math.round(state.viewport.width * state.viewport.dpr));
    assert.equal(state.output.height, Math.round(state.viewport.height * state.viewport.dpr));
    report.checks.push({ resize: 'Canvas dimensions agree with actual viewport/DPR', before: beforeResize,
      after: state.viewport, compositorResized: beforeResize.width !== state.viewport.width || beforeResize.height !== state.viewport.height });
    win.setContentSize(960, 640); await frame();

    if (values.movie) {
      const originalOpen = dialog.showOpenDialog;
      dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [output] });
      try {
        await click('Render movie');
        await waitFor(() => evaluate(`Boolean(document.getElementById('movieExport') &&
          !document.getElementById('movieStart').disabled)`), 'movie dialog readiness');
        assert.equal(await evaluate(`document.getElementById('movieDuration').value`), '12');
        assert.equal(await evaluate(`Boolean(document.querySelector('#movieFps option[value="144"]'))`), true);
        await evaluate(`document.getElementById('movieClose').click()`);
        for (const fps of [60, 144]) {
          console.log(`Rendering actual 12-second merger at ${fps} FPS (128×72 integration check)…`);
          await click('Reset');
          await evaluate(`window.__bh.params.autoOrbit = false; window.__bh.merger.select('GW150914')`);
          const before = await evaluate('window.__bh.diagnostics()');
          const clip = await evaluate(`(async () => {
            const options = { event: 'GW150914', width: 128, height: 72, duration: 12, fps: ${fps}, format: 'mp4' };
            const renderer = window.__bh.movie, writer = window.kerrMovie;
            const metadata = renderer.begin(options);
            let result;
            try {
              const job = await writer.start(options, metadata);
              if (!job) throw new Error('Test output folder was cancelled');
              const a = await renderer.render(0);
              const b = await renderer.render(0);
              if (a.length !== b.length || a.some((v, i) => v !== b[i])) throw new Error('Repeated frame is not deterministic');
              for (let i = 0; i < job.frameCount; i++) await writer.append(i, await renderer.render(i));
              result = await writer.finish();
            } catch (error) { await writer.cancel(); throw error; }
            finally { renderer.end(); }
            return result;
          })()`);
          await frame();
          const after = await evaluate('window.__bh.diagnostics()');
          assert.deepEqual(after.settings, before.settings);
          assert.deepEqual(after.camera, before.camera);
          assert.deepEqual(after.merger, before.merger);
          const first = fs.readFileSync(path.join(clip.folder, 'frames/frame-000000.png'));
          const last = fs.readFileSync(path.join(clip.folder, 'frames', `frame-${String(fps * 12 - 1).padStart(6, '0')}.png`));
          decode(first, { width: 128, height: 72 });
          decode(last, { width: 128, height: 72 });
          assert(!first.equals(last), 'Merger must animate');
          report.checks.push({ movie: clip, resolution: '128×72 integration check, not full-resolution acceptance' });
          saveReport();
        }
      } finally { dialog.showOpenDialog = originalOpen; }
    }

    if (values.measure) {
      for (const scenario of ['single-kerr', 'gw151226-auto', 'gw151226-manual', 'gw151226-retarded-auto']) {
        console.log(`Measuring ${scenario} (3 s warmup, at least 10 s visible intervals)…`);
        await click('Reset');
        await evaluate(`window.__bh.params.autoOrbit = false;
          ${scenario === 'single-kerr' ? '' : `window.__bh.merger.select('GW151226'); window.__bh.merger.setPlaying(false); window.__bh.merger.seek(0);`}
          window.__bh.params.adaptiveMerger = ${scenario !== 'gw151226-manual'};
          window.__bh.params.retarded = ${scenario === 'gw151226-retarded-auto'};`);
        await frame(); await delay(3000);
        const before = await evaluate('window.__bh.diagnostics()');
        await evaluate('window.__bh.resetFrameTimings()');
        const sizes = [];
        await waitFor(async () => {
          const progress = await evaluate(`(() => { const d = window.__bh.diagnostics();
            return { timing: d.timing, internal: d.internal, visible: document.visibilityState === 'visible' }; })()`);
          assert(progress.visible, 'Keep the test window visible during measurement');
          sizes.push({ elapsedMs: progress.timing.durationMs, internal: progress.internal });
          await delay(400);
          return progress.timing.durationMs >= 10_000;
        }, `10 seconds of ${scenario} frames`, 60_000);
        const after = await evaluate('window.__bh.diagnostics()');
        assert(after.timing.sampleCount >= 2);
        report.measurements.push({ scenario, warmupMs: 3000, requestedSampleMs: 10_000,
          before, after, sizes, processMetrics: app.getAppMetrics() });
        saveReport();
      }
    }
    const unexpected = report.console.filter(item => item.level === 'error');
    assert.equal(unexpected.length, 0, JSON.stringify(unexpected));
    report.status = 'passed';
  } catch (error) {
    report.status = 'failed'; report.error = error.stack || String(error);
    console.error(report.error);
  } finally {
    clearTimeout(watchdog);
    report.finishedAt = new Date().toISOString();
    saveReport();
    console.log(`Desktop check ${report.status}: ${path.join(output, 'report.json')}`);
    // This isolated process owns only the test window; the user's normal app is untouched.
    app.exit(report.status === 'passed' ? 0 : 1);
  }
}
