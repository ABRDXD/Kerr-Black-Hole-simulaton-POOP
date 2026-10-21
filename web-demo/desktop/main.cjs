const electron = require('electron');
const { app, BrowserWindow, Menu, dialog, shell } = electron;
const { installMovieIPC } = require('./movie-ipc.cjs');
const path = require('node:path');
const fs = require('node:fs');
const { pathToFileURL } = require('node:url');

const linux = process.platform === 'linux';
const productName = linux ? 'KerrLab(Arch)' : 'KerrLab';
const windowTitle = `${productName} — Black Hole Observatory`;
app.setName(productName);
if (process.platform === 'win32') app.setAppUserModelId('org.kerrlab.observatory');
if (linux) app.commandLine.appendSwitch('class', 'kerrlab-arch');
let mainWindow;
const entry = path.join(__dirname, '../dist/index.html');
const entryUrl = pathToFileURL(entry).href;
let movie;

function log(event, detail) {
  try {
    fs.appendFileSync(path.join(app.getPath('userData'), 'desktop.log'),
      JSON.stringify({ time: new Date().toISOString(), event, detail }) + '\n');
  } catch { /* A diagnostic write must not prevent rendering. */ }
}

function openSource(url) {
  if (url === 'https://github.com/kyleyhw/black_hole') void shell.openExternal(url);
}

async function createWindow() {
  if (!fs.existsSync(entry)) {
    throw new Error('The bundled renderer is missing (dist/index.html). Keep the portable app folder intact, or rebuild it from the KerrLab source folder.');
  }
  mainWindow = new BrowserWindow({
    width: 1440, height: 900, minWidth: 760, minHeight: 540,
    title: windowTitle, backgroundColor: '#06080d',
    show: false, autoHideMenuBar: true,
    webPreferences: { nodeIntegration: false, contextIsolation: true, sandbox: true,
      preload: path.join(__dirname, 'preload.cjs') },
  });
  const contents = mainWindow.webContents;
  mainWindow.on('page-title-updated', event => event.preventDefault());
  contents.setWindowOpenHandler(({ url }) => { openSource(url); return { action: 'deny' }; });
  contents.on('will-navigate', (event, url) => {
    if (url !== entryUrl) { event.preventDefault(); openSource(url); }
  });
  contents.session.setPermissionRequestHandler((_contents, _permission, callback) => callback(false));
  contents.session.setPermissionCheckHandler(() => false);
  // Electron 44 puts the structured details on the first event argument.
  // Select serializable fields: its WebFrameMain must not enter the JSON log.
  contents.on('console-message', details => {
    if (details.level === 'error' || details.level === 'warning') {
      const { level, message, lineNumber, sourceId } = details;
      log('renderer-console', { level, message, lineNumber, sourceId });
    }
  });
  contents.on('render-process-gone', (_event, details) => {
    void movie?.cancel('Renderer stopped.').catch(error => log('movie-cancel-error', error.message));
    log('renderer-exit', details);
    dialog.showErrorBox('KerrLab renderer stopped', 'Close and reopen KerrLab to recreate the graphics context.\n' + details.reason);
  });
  let loadFailureReported = false;
  contents.on('did-start-loading', () => {
    loadFailureReported = false;
    if (movie?.active) void movie.cancel('Renderer reloaded.').catch(error => log('movie-cancel-error', error.message));
  });
  contents.on('did-fail-load', (_event, code, description, url, isMainFrame) => {
    if (code === -3 || isMainFrame === false) return; // Superseded navigation/subframe.
    log('load-error', { code, description, url });
    if (loadFailureReported) return;
    loadFailureReported = true;
    mainWindow?.show();
    dialog.showErrorBox('KerrLab could not load its renderer',
      `The bundled page could not be opened (${code}: ${description}).\n` +
      'Keep the portable app folder intact. Close KerrLab and rebuild from the source folder if files are missing or damaged.');
  });
  contents.session.on('will-download', (_event, item) => {
    // Electron supplies the native Save dialog; no remote service is involved.
    item.setSaveDialogOptions({ title: 'Save black-hole image', filters: [{ name: 'PNG image', extensions: ['png'] }] });
    item.once('done', (_downloadEvent, state) => log('png-download', { state }));
  });
  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
    log('window-shown', { electron: process.versions.electron, gpu: app.getGPUFeatureStatus(), source: 'bundled-files' });
  });
  mainWindow.on('closed', () => { mainWindow = null; });
  mainWindow.on('close', event => {
    if (!movie?.active) return;
    event.preventDefault();
    const closing = mainWindow;
    void movie.cancel('Window closed.').catch(error => log('movie-cancel-error', error.message)).finally(() => closing?.destroy());
  });
  try { await mainWindow.loadFile(entry); }
  catch (error) {
    if (!loadFailureReported) throw error;
    app.quit();
    return;
  }
  log('loaded', 'dist/index.html');
}

if (!app.requestSingleInstanceLock()) app.quit();
else {
  movie = installMovieIPC(electron, () => mainWindow, entryUrl);
  app.on('second-instance', () => { if (mainWindow) { if (mainWindow.isMinimized()) mainWindow.restore(); mainWindow.focus(); } });
  app.whenReady().then(async () => {
    Menu.setApplicationMenu(Menu.buildFromTemplate([
      { label: productName, submenu: [{ role: 'quit' }] },
      { label: 'View', submenu: [{ role: 'reload' }, { role: 'togglefullscreen', accelerator: 'F11' }] },
    ]));
    await createWindow();
  }).catch(error => { dialog.showErrorBox('KerrLab could not start', error.message); app.quit(); });
  app.on('window-all-closed', () => app.quit());
}
