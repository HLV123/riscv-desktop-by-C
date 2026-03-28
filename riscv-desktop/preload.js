const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('api', {
  init:         ()       => ipcRenderer.invoke('emu-init'),
  cmd:          (c)      => ipcRenderer.invoke('emu-cmd', c),
  restart:      ()       => ipcRenderer.invoke('emu-restart'),
  openBinFile:  ()       => ipcRenderer.invoke('open-bin-file'),
  listPrograms: ()       => ipcRenderer.invoke('list-programs'),
})
