/**
 * 蓝牙模式链路：仅封装 BleClient，与 WiFi 无交叉。
 */
const { BleClient } = require('./ble')
const { POLL_MS } = require('./remote-buttons')

class BleLink {
  constructor() {
    this._ble = new BleClient()
    this._pollTimer = null
    this._pollToken = ''
  }

  setOnStatus(handler) {
    this._ble.onStatus = handler
  }

  setOnStateChange(handler) {
    this._ble.onStateChange = handler
  }

  isConnected() {
    return this._ble.connected
  }

  async scan(onProgress) {
    return this._ble.refreshDeviceList(onProgress)
  }

  getLastError() {
    return this._ble.getLastError()
  }

  async connect(deviceId, token) {
    return this._ble.connectToDevice(deviceId, token)
  }

  disconnect() {
    this.stopPoll()
    return this._ble.disconnect()
  }

  closeAdapter() {
    return this._ble.closeAdapter()
  }

  /** 下发命令后主动拉取 status，与 WiFi 响应体带状态对齐。 */
  async sendCommand(cmd, token) {
    await this._ble.sendCommand(cmd, token)
    if (cmd !== 'status') {
      await this._ble.fetchStatus(token)
    }
  }

  startPoll(token) {
    this.stopPoll()
    if (!this._ble.connected) return
    this._pollToken = token || ''
    const tick = () => {
      if (!this._ble.connected) return
      this._ble.fetchStatus(this._pollToken).catch(() => {})
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

module.exports = BleLink
