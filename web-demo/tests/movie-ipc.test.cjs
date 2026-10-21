const test = require('node:test');
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const { installMovieIPC } = require('../desktop/movie-ipc.cjs');

test('movie IPC rejects foreign windows, subframes and navigated pages before filesystem access', async () => {
  const handlers = new Map();
  const frame = { url: 'file:///app/dist/index.html' };
  const contents = { mainFrame: frame };
  const api = installMovieIPC({ app: new EventEmitter(),
    ipcMain: { handle: (name, fn) => handlers.set(name, fn) }, dialog: {}, shell: {}, powerSaveBlocker: {} },
  () => ({ webContents: contents }), frame.url);
  for (const event of [{ sender: {}, senderFrame: frame },
    { sender: contents, senderFrame: { url: frame.url } }, { sender: contents, senderFrame: null }]) {
    for (const handler of handlers.values()) await assert.rejects(handler(event), /restricted/);
  }
  frame.url = 'https://example.invalid';
  for (const handler of handlers.values()) await assert.rejects(handler({ sender: contents, senderFrame: frame }), /restricted/);
  assert.equal(api.active, false);
});

test('reload while choosing an output folder cancels preparation before any export begins', async () => {
  const handlers = new Map();
  const frame = { url: 'file:///app/dist/index.html' };
  const contents = { mainFrame: frame };
  let choose;
  const api = installMovieIPC({ app: new EventEmitter(),
    ipcMain: { handle: (name, fn) => handlers.set(name, fn) },
    dialog: { showOpenDialog: () => new Promise(resolve => { choose = resolve; }) },
    shell: {}, powerSaveBlocker: {} }, () => ({ webContents: contents }), frame.url);
  const event = { sender: contents, senderFrame: frame };
  const settings = { event: 'GW150914', width: 64, height: 36, fps: 60, duration: 12, format: 'png' };
  const starting = handlers.get('movie:start')(event, settings, {});
  assert.equal(api.active, true);
  await assert.rejects(handlers.get('movie:start')(event, settings, {}), /already running/);
  await api.cancel('Renderer reloaded.');
  choose({ canceled: false, filePaths: ['/must-not-be-used'] });
  assert.equal(await starting, null);
  assert.equal(api.active, false);
});
