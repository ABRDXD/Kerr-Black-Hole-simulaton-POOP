// Assemble a portable desktop app; cross-package Linux on Windows with --runtime.
// Electron/Chromium notices and the renderer sandbox are preserved.
const fs = require('node:fs');
const path = require('node:path');
const { parseArgs } = require('node:util');
const root = path.resolve(__dirname, '..');
const demo = path.join(root, 'web-demo');
const { values } = parseArgs({ options: {
  platform: { type: 'string', default: process.platform },
  arch: { type: 'string', default: process.arch },
  runtime: { type: 'string' },
} });
if (!['win32', 'linux'].includes(values.platform) || values.arch !== 'x64') {
  throw new Error('This packager supports Windows x64 and Linux x64.');
}
const linux = values.platform === 'linux';
const edition = linux ? 'KerrLab(Arch)' : 'KerrLab';
const executable = linux ? 'kerrlab-arch' : 'KerrLab.exe';
const runtimeExe = linux ? 'electron' : 'electron.exe';
const cachedLinux = path.join(root, '.tools/electron-linux');
const runtime = path.resolve(values.runtime || (linux && fs.existsSync(cachedLinux)
  ? cachedLinux : path.join(demo, 'node_modules/electron/dist')));
const outputParent = path.join(root, 'out/desktop');
const output = path.join(outputParent, linux ? 'KerrLab-Arch' : 'KerrLab');
const previous = `${output}.previous`;
if (fs.existsSync(previous)) throw new Error(`Previous backup still exists: ${previous}. Inspect it before repackaging.`);
const binary = path.join(runtime, runtimeExe);
if (!fs.existsSync(binary)) throw new Error(`Missing ${binary}. Restore the pinned Electron runtime first.`);
const version = require(path.join(demo, 'package.json')).devDependencies.electron;
if (fs.readFileSync(path.join(runtime, 'version'), 'utf8').trim().replace(/^v/, '') !== version) {
  throw new Error('Electron runtime version does not match the pinned package.json version.');
}
const fd = fs.openSync(binary, 'r');
const header = Buffer.alloc(64);
let peX64 = false;
try {
  fs.readSync(fd, header, 0, header.length, 0);
  if (!linux && header.subarray(0, 2).toString() === 'MZ') {
    const pe = Buffer.alloc(6);
    fs.readSync(fd, pe, 0, pe.length, header.readUInt32LE(60));
    peX64 = pe.subarray(0, 4).equals(Buffer.from([80, 69, 0, 0])) && pe.readUInt16LE(4) === 0x8664;
  }
} finally { fs.closeSync(fd); }
if (linux ? !(header.subarray(0, 4).equals(Buffer.from([127, 69, 76, 70])) && header[4] === 2 && header.readUInt16LE(18) === 62)
          : !peX64) {
  throw new Error('Runtime executable is not the requested target format/architecture.');
}
if (!fs.existsSync(path.join(demo, 'dist/index.html'))) throw new Error('Run the web-demo production build first.');
fs.mkdirSync(outputParent, { recursive: true });
const staging = fs.mkdtempSync(path.join(outputParent, '.package-'));
fs.cpSync(runtime, staging, { recursive: true, filter: source => source === runtime ||
  !['electron', 'electron.exe', 'default_app.asar'].includes(path.basename(source)) });
fs.copyFileSync(binary, path.join(staging, executable));
if (linux) for (const file of [executable, 'chrome-sandbox', 'chrome_crashpad_handler']) {
  if (fs.existsSync(path.join(staging, file))) fs.chmodSync(path.join(staging, file), 0o755);
}
const appRoot = path.join(staging, 'resources/app');
fs.mkdirSync(appRoot, { recursive: true });
fs.writeFileSync(path.join(appRoot, 'package.json'), JSON.stringify({ name: linux ? 'kerrlab-arch' : 'kerrlab', productName: edition, version: '0.1.0', main: 'desktop/main.cjs' }, null, 2) + '\n');
fs.cpSync(path.join(demo, 'desktop'), path.join(appRoot, 'desktop'), { recursive: true });
fs.cpSync(path.join(demo, 'dist'), path.join(appRoot, 'dist'), { recursive: true });
for (const name of ['UPSTREAM.md', 'UPSTREAM_FILES.json']) fs.copyFileSync(path.join(demo, name), path.join(appRoot, name));
fs.cpSync(path.join(root, 'third_party/licenses'), path.join(appRoot, 'licenses'), { recursive: true });
const index = path.join(appRoot, 'dist/index.html');
const csp = "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'";
let html = fs.readFileSync(index, 'utf8').replace('<head>', `<head>\n    <meta http-equiv="Content-Security-Policy" content="${csp}">`);
if (linux) html = html.replace('<title>KerrLab', '<title>KerrLab(Arch)');
fs.writeFileSync(index, html);
fs.writeFileSync(path.join(staging, 'START-HERE.txt'), `${edition} local desktop build\n\nKeep this folder intact. No server or separate Node.js is needed to run the packaged app.\n${linux ? 'Use bash start-kerrlab-arch.sh from the source bundle. Arch desktop libraries and a working graphics driver must be installed.' : 'Double-click KerrLab.exe.'}\nF11 toggles fullscreen. H hides/shows controls. Escape restores the interface.\nSave PNG opens a preview; Download PNG opens the native Save dialog.\n\nUpstream provenance: resources/app/UPSTREAM.md. This personal migration package is not a public release.\n`);
fs.writeFileSync(path.join(staging, 'BUILD-INFO.json'), JSON.stringify({ edition, platform: values.platform, arch: values.arch, electron: version, packagedAt: new Date().toISOString(), builtOn: process.platform, runtimeTestedOnTarget: false }, null, 2) + '\n');
// Swap only the fixed generated target, retaining the previous output if assembly fails.
if (fs.existsSync(output)) fs.renameSync(output, previous);
try { fs.renameSync(staging, output); }
catch (error) { if (fs.existsSync(previous)) fs.renameSync(previous, output); throw error; }
if (fs.existsSync(previous)) fs.rmSync(previous, { recursive: true });
console.log(path.join(output, executable));
