const { app, BrowserWindow, ipcMain, dialog } = require('electron')
const path = require('path')
const fs = require('fs')
const { spawn } = require('child_process')

let mainWindow
let emuProcess = null
let pendingResolvers = []
let lineBuffer = ''

function getCliPath() {
  if (process.platform === 'win32') {
    return path.join(__dirname, 'build', 'riscv_emu_cli.exe')
  }
  return path.join(__dirname, 'build', 'riscv_emu_cli')
}

function spawnEmulator() {
  if (emuProcess) return true
  const cliPath = getCliPath()
  if (!fs.existsSync(cliPath)) return false

  emuProcess = spawn(cliPath, ['--json'], {
    stdio: ['pipe', 'pipe', 'pipe']
  })

  emuProcess.stdout.on('data', (chunk) => {
    lineBuffer += chunk.toString()
    let nl
    while ((nl = lineBuffer.indexOf('\n')) !== -1) {
      const line = lineBuffer.slice(0, nl).trim()
      lineBuffer = lineBuffer.slice(nl + 1)
      if (line.length === 0) continue
      const resolver = pendingResolvers.shift()
      if (resolver) {
        try { resolver(JSON.parse(line)) }
        catch { resolver({ error: 'parse_error', raw: line }) }
      }
    }
  })

  emuProcess.stderr.on('data', () => {})

  emuProcess.on('close', () => {
    emuProcess = null
    lineBuffer = ''
    for (const r of pendingResolvers) r({ error: 'process_died' })
    pendingResolvers = []
  })

  return true
}

function sendCommand(cmd) {
  return new Promise((resolve) => {
    if (!emuProcess) {
      resolve({ error: 'no_process' })
      return
    }
    pendingResolvers.push(resolve)
    emuProcess.stdin.write(cmd + '\n')
  })
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 860,
    minWidth: 960,
    minHeight: 600,
    title: 'RISC-V RV32I Emulator',
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js')
    },
    backgroundColor: '#1e1e2e',
    show: false
  })

  mainWindow.loadFile(path.join(__dirname, 'web', 'index.html'))
  mainWindow.once('ready-to-show', () => mainWindow.show())
  mainWindow.on('closed', () => {
    if (emuProcess) emuProcess.kill()
    mainWindow = null
  })
}

ipcMain.handle('emu-init', async () => {
  const ok = spawnEmulator()
  return { ok, cliExists: fs.existsSync(getCliPath()) }
})

ipcMain.handle('emu-cmd', async (_, cmd) => {
  if (!emuProcess && !spawnEmulator()) {
    return { error: 'CLI not found. Run: cmake --build build' }
  }
  return await sendCommand(cmd)
})

ipcMain.handle('emu-restart', async () => {
  if (emuProcess) {
    emuProcess.kill()
    emuProcess = null
    lineBuffer = ''
    await new Promise(r => setTimeout(r, 150))
  }
  const ok = spawnEmulator()
  return { ok }
})

ipcMain.handle('open-bin-file', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: 'Open RISC-V Binary',
    defaultPath: path.join(__dirname, 'programs'),
    filters: [
      { name: 'RISC-V Binary', extensions: ['bin'] },
      { name: 'All Files', extensions: ['*'] }
    ],
    properties: ['openFile']
  })
  if (result.canceled) return null
  return result.filePaths[0]
})

ipcMain.handle('list-programs', async () => {
  const dir = path.join(__dirname, 'programs')
  if (!fs.existsSync(dir)) return []
  return fs.readdirSync(dir)
    .filter(f => f.endsWith('.bin'))
    .map(f => ({ name: f.replace('.bin', ''), path: path.join(dir, f) }))
})

app.whenReady().then(createWindow)

app.on('window-all-closed', () => {
  if (emuProcess) emuProcess.kill()
  if (process.platform !== 'darwin') app.quit()
})

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) createWindow()
})
