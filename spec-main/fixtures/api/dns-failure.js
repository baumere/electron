const { BrowserWindow, app } = require('electron')
app.on('ready', async function () {
  let w = new BrowserWindow({
    show: true,
    webPreferences: {
      nodeIntegration: true
    }
  })

  try {
    await w.loadURL('https://err.name.not.resolved')
  } catch (err) {
    console.log('Error loading URL:', err)
  }

  setImmediate(() => {
    app.exit(1)
  })
})
