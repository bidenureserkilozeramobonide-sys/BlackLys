const { contextBridge } = require('electron');

// Expose safe APIs to renderer
contextBridge.exposeInMainWorld('electronAPI', {
    isElectron: true,
    platform: process.platform,
});
