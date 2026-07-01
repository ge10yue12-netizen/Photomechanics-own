/**
 * WiFi 模式链路：HTTP 状态轮询 + 预览刷新。
 */
const { HttpClient } = require('./http')
const { POLL_MS, PREVIEW_MS } = require('./remote-buttons')

class WifiLink {
  constructor() {
    this._http = new HttpClient()
    this._pollTimer = null
    this._previewTimer = null
    this._pollToken = ''
  }

  setOnStatus(handler) {
    this._http.onStatus = handler
  }

  setOnConnectionLost(handler) {
    this._http.onConnectionLost = handler
  }

  isConnected() {
    return this._http.connected
  }

  async connect(host, token) {
    return this._http.connect(host, token)
  }

  async disconnect(token) {
    this.stopPoll()
    await this._http.disconnect(token)
  }

  async sendCommand(cmd, token) {
    return this._http.sendCommand(cmd, token)
  }

  startPoll(token) {
    this.stopPoll()
    if (!this._http.connected) return
    this._pollToken = token || ''
    const tick = () => {
      if (!this._http.connected) return
      this._http.fetchStatus(this._pollToken).catch(() => {})
    }
    tick()
    this._pollTimer = setInterval(tick, POLL_MS)
  }

  stopPoll() {
    if (this._pollTimer) {
      clearInterval(this._pollTimer)
      this._pollTimer = null
    }
    this.stopPreview()
  }

  startPreview(token, onUrl) {
    this.stopPreview()
    if (!this._http.connected || !onUrl) return
    const t = token || ''
    const tick = () => {
      if (!this._http.connected) return
      onUrl(this._http.previewUrl(t))
    }
    tick()
    this._previewTimer = setInterval(tick, PREVIEW_MS)
  }

  stopPreview() {
    if (this._previewTimer) {
      clearInterval(this._previewTimer)
      this._previewTimer = null
    }
  }
}

module.exports = WifiLink
