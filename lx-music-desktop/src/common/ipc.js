const { ipcMain, ipcRenderer } = require('electron')


exports.mainOn = (event, callback) => {
  ipcMain.on(event, callback)
}
exports.mainOnce = (event, callback) => {
  ipcMain.once(event, callback)
}

exports.mainHandle = (name, callback) => {
  ipcMain.handle(name, callback)
}
exports.mainHandleOnce = (name, callback) => {
  ipcMain.handleOnce(name, callback)
}


exports.rendererSend = (name, params) => {
  ipcRenderer.send(name, params)
}
exports.rendererSendSync = (name, params) => ipcRenderer.sendSync(name, params)

exports.rendererInvoke = (name, params) => ipcRenderer.invoke(name, params)

exports.rendererOn = (name, callback) => {
  ipcRenderer.on(name, callback)
}
exports.rendererOnce = (name, callback) => {
  ipcRenderer.once(name, callback)
}
