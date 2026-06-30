/**
 * WiFi 模式链路：仅封装 HttpClient，与 BLE 无交叉。
 */
const { HttpClient } = require('./http')
const { POLL_MS } = require('./remote-buttons')

class WifiLink {
  constructor() {
    this._http = new HttpClient()
    this._pollTimer = null
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

  disconnect() {
    this.stopPoll()
    this._http.disconnect()
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
  }
}

module.exports = WifiLink
