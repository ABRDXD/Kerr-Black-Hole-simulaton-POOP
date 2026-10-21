const { MovieExporter, validateOptions } = require('./movie-export.cjs');

function installMovieIPC({ app, ipcMain, dialog, shell, powerSaveBlocker }, getWindow, entryUrl) {
  const exporter = new MovieExporter();
  let starting = false, owner = null, blocker = null, generation = 0;
  const release = () => {
    if (blocker !== null) { powerSaveBlocker.stop(blocker); blocker = null; }
    if (owner && !owner.isDestroyed()) owner.backgroundThrottling = true;
  };
  const authorize = event => {
    const win = getWindow();
    if (!win || event.sender !== win.webContents || event.senderFrame !== win.webContents.mainFrame ||
        event.senderFrame.url !== entryUrl) throw new Error('Movie export is restricted to the local main window.');
  };
  const handle = (name, fn) => ipcMain.handle(`movie:${name}`, async (event, ...args) => {
    authorize(event);
    try { return await fn(event, ...args); }
    catch (error) { if (!exporter.active) release(); throw error; }
  });
  handle('capabilities', () => exporter.capabilities());
  handle('start', async (event, input, metadata) => {
    if (starting || exporter.active) throw new Error('An export is already running.');
    validateOptions(input);
    starting = true;
    const startGeneration = generation;
    try {
      const result = await dialog.showOpenDialog(getWindow(), { title: 'Choose a folder for the merger clip',
        properties: ['openDirectory', 'createDirectory'] });
      if (result.canceled || !result.filePaths[0]) return null;
      if (startGeneration !== generation) return null;
      authorize(event);
      const started = await exporter.start(result.filePaths[0], input, metadata);
      if (startGeneration !== generation || !getWindow() || event.sender.isDestroyed()) {
        await exporter.cancel('Window closed while preparing export.');
        return null;
      }
      owner = event.sender;
      owner.backgroundThrottling = false;
      blocker = powerSaveBlocker.start('prevent-app-suspension');
      return started;
    } finally { starting = false; }
  });
  handle('append', (_event, index, png) => exporter.append(index, png));
  handle('finish', async () => { try { return await exporter.finish(); } finally { release(); } });
  handle('cancel', () => cancel('Cancelled by user.'));
  handle('reveal', async () => {
    if (exporter.latestFolder) return shell.openPath(exporter.latestFolder);
  });
  const cancel = async reason => { generation++; try { return await exporter.cancel(reason); } finally { release(); } };
  app.on('before-quit', event => {
    generation++;
    if (exporter.job && !exporter.job.terminal) {
      event.preventDefault(); void cancel('Application closed.').catch(() => {}).finally(() => app.quit());
    }
  });
  return { get active() { return starting || exporter.active; }, cancel };
}

module.exports = { installMovieIPC };
