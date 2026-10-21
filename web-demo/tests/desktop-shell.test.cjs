// Exercise the real main-process entry with Electron events and I/O isolated.
// This does not replace a real packaged-window/native-dialog smoke test.
const assert = require('node:assert/strict');
const test = require('node:test');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const { EventEmitter } = require('node:events');
const entry = path.resolve(__dirname, '../desktop/main.cjs');
const source = fs.readFileSync(entry, 'utf8');

async function loadShell({ missing = false, failLoad = false, primary = true } = {}) {
  const logs = [], errors = [], windows = [];
  let quits = 0;
  const app = Object.assign(new EventEmitter(), {
    setName() {}, commandLine: { appendSwitch() {} },
    getPath: () => '/test-user', requestSingleInstanceLock: () => primary,
    whenReady: () => Promise.resolve(), quit: () => { quits++; },
    getGPUFeatureStatus: () => ({}),
  });
  class BrowserWindow extends EventEmitter {
    constructor(options) {
      super();
      this.options = options;
      this.shows = 0;
      this.webContents = Object.assign(new EventEmitter(), {
        setWindowOpenHandler() {},
        session: Object.assign(new EventEmitter(), {
          setPermissionRequestHandler() {}, setPermissionCheckHandler() {},
        }),
      });
      windows.push(this);
    }
    show() { this.shows++; }
    async loadFile(file) {
      this.webContents.emit('did-start-loading');
      if (failLoad) {
        this.webContents.emit('did-fail-load', {}, -6, 'ERR_FILE_NOT_FOUND', file, true);
        throw new Error('ERR_FILE_NOT_FOUND');
      }
    }
  }
  const electron = {
    app, BrowserWindow,
    ipcMain: { handle() {} }, powerSaveBlocker: { start() { return 1; }, stop() {} },
    Menu: { buildFromTemplate: v => v, setApplicationMenu() {} },
    dialog: { showErrorBox: (title, message) => errors.push({ title, message }) },
    shell: { openExternal() {} },
  };
  vm.runInNewContext(source, {
    __dirname: path.dirname(entry),
    process: { platform: 'linux', versions: { electron: '44.7.0' } },
    require(name) {
      if (name === 'electron') return electron;
      if (name === './movie-ipc.cjs') return require('../desktop/movie-ipc.cjs');
      if (name === 'node:fs') return {
        existsSync: () => !missing,
        appendFileSync: (_file, text) => logs.push(JSON.parse(text)),
      };
      if (['node:path', 'node:url'].includes(name)) return require(name);
      throw new Error(`Unexpected dependency: ${name}`);
    },
  }, { filename: entry });
  await new Promise(resolve => setImmediate(resolve));
  return { logs, errors, windows, quits: () => quits };
}

test('Electron 44 console warnings/errors use the event object, without serializing its frame', async () => {
  const shell = await loadShell();
  const frame = {}; frame.self = frame;
  for (const level of ['error', 'warning', 'info', 'debug']) {
    shell.windows[0].webContents.emit('console-message', {
      level, message: `${level} text`, lineNumber: 17, sourceId: 'file:///renderer.js', frame,
    }, 2, 'legacy arguments are not the structured event');
  }
  assert.deepEqual(shell.logs.filter(l => l.event === 'renderer-console').map(l => l.detail), [
    { level: 'error', message: 'error text', lineNumber: 17, sourceId: 'file:///renderer.js' },
    { level: 'warning', message: 'warning text', lineNumber: 17, sourceId: 'file:///renderer.js' },
  ]);
  assert.equal(shell.windows[0].options.webPreferences.sandbox, true);
});

test('missing packaged HTML fails before creating a blank window', async () => {
  const shell = await loadShell({ missing: true });
  assert.equal(shell.windows.length, 0);
  assert.equal(shell.errors.length, 1);
  assert.match(shell.errors[0].message, /bundled renderer is missing/);
  assert.equal(shell.quits(), 1);
});

test('initial load rejection reports once and quits rather than showing two errors', async () => {
  const shell = await loadShell({ failLoad: true });
  assert.equal(shell.errors.length, 1);
  assert.match(shell.errors[0].message, /rebuild/);
  assert.equal(shell.quits(), 1);
  assert.equal(shell.logs.filter(l => l.event === 'loaded').length, 0);
});

test('reload failures are visible; superseded navigation and subframes do not cause dialogs', async () => {
  const shell = await loadShell();
  const contents = shell.windows[0].webContents;
  contents.emit('did-fail-load', {}, -3, 'ERR_ABORTED', 'file:///index.html', true);
  contents.emit('did-fail-load', {}, -6, 'ERR_FILE_NOT_FOUND', 'file:///subframe', false);
  assert.equal(shell.errors.length, 0);
  contents.emit('did-start-loading');
  contents.emit('did-fail-load', {}, -6, 'ERR_FILE_NOT_FOUND', 'file:///index.html', true);
  contents.emit('did-fail-load', {}, -6, 'ERR_FILE_NOT_FOUND', 'file:///index.html', true);
  assert.equal(shell.errors.length, 1);
  assert.equal(shell.windows[0].shows, 1);
  assert.equal(shell.quits(), 0);
});

test('PNG downloads retain the save-dialog filter and completion log', async () => {
  const shell = await loadShell();
  let options;
  const item = Object.assign(new EventEmitter(), { setSaveDialogOptions: value => { options = value; } });
  shell.windows[0].webContents.session.emit('will-download', {}, item);
  assert.equal(options.filters[0].extensions[0], 'png');
  item.emit('done', {}, 'cancelled');
  assert.equal(shell.logs.at(-1).event, 'png-download');
  assert.equal(shell.logs.at(-1).detail.state, 'cancelled');
});

test('a second instance quits without creating a new window', async () => {
  const shell = await loadShell({ primary: false });
  assert.equal(shell.windows.length, 0);
  assert.equal(shell.quits(), 1);
});
