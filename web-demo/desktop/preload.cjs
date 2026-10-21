const { contextBridge, ipcRenderer } = require('electron');

// No arbitrary paths, commands, channel names or Electron objects cross this API.
contextBridge.exposeInMainWorld('kerrMovie', Object.freeze({
  capabilities: () => ipcRenderer.invoke('movie:capabilities'),
  start: (options, metadata) => ipcRenderer.invoke('movie:start', options, metadata),
  append: (index, png) => ipcRenderer.invoke('movie:append', index, png),
  finish: () => ipcRenderer.invoke('movie:finish'),
  cancel: () => ipcRenderer.invoke('movie:cancel'),
  reveal: () => ipcRenderer.invoke('movie:reveal'),
}));
