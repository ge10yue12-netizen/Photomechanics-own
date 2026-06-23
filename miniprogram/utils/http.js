const REQUEST_TIMEOUT_MS = 5000
const { humanizeError, humanizeHttpStatus } = require('./errors')

class HttpClient {
  constructor() {
    this.host = ''
    this.connected = false
    this.onStatus = null
    this.onConnectionLost = null
  }

  normalizeHost(host) {
    let h = (host || '').trim().replace(/^https?:\/\//i, '')
    if (!h) return ''
    if (h.indexOf(':') < 0) h += ':18765'
    return h
  }

  baseUrl() {
    return `http://${this.host}`
  }

  _markLost(reason) {
    if (!this.connected) return
    this.connected = false
    if (this.onConnectionLost) this.onConnectionLost(humanizeError(reason, 'lost'))
  }

  request(options) {
    return new Promise((resolve, reject) => {
      wx.request({
        timeout: REQUEST_TIMEOUT_MS,
        ...options,
        success: (res) => resolve(res),
        fail: (e) => reject(new Error(humanizeError(e, 'connect')))
      })
    })
  }

  async connect(host, token) {
    this.host = this.normalizeHost(host)
    if (!this.host) throw new Error('请填写 PC 地址，如 192.168.x.x:18765')
    const status = await this.fetchStatus(token, { probe: true })
    this.connected = true
    return status
  }

  async fetchStatus(token, opts = {}) {
    let res
    try {
      res = await this.request({
        url: `${this.baseUrl()}/api/status`,
        method: 'GET',
        data: { token: token || '' }
      })
    } catch (e) {
      if (!opts.probe) this._markLost(e.message || '网络请求失败')
      throw e
    }
    if (res.statusCode === 200 && res.data) {
      if (this.onStatus) this.onStatus(res.data)
      return res.data
    }
    const bodyMsg = res.data && (res.data.message || res.data.msg)
    const msg = humanizeHttpStatus(res.statusCode, bodyMsg)
    if (!opts.probe && (res.statusCode === 0 || res.statusCode >= 500)) {
      this._markLost(msg)
    }
    throw new Error(msg)
  }

  async sendCommand(cmd, token) {
    if (!this.connected || !this.host) throw new Error('未连接 PC')
    let res
    try {
      res = await this.request({
        url: `${this.baseUrl()}/api/command?token=${encodeURIComponent(token || '')}`,
        method: 'POST',
        header: { 'content-type': 'application/json' },
        data: { cmd, token: token || '' }
      })
    } catch (e) {
      this._markLost(e.message || '网络请求失败')
      throw e
    }
    if (res.statusCode === 200 && res.data) {
      if (this.onStatus) this.onStatus(res.data)
      return res.data
    }
    const bodyMsg = res.data && (res.data.message || res.data.msg)
    const msg = humanizeHttpStatus(res.statusCode, bodyMsg)
    if (res.statusCode === 0 || res.statusCode >= 500) {
      this._markLost(msg)
    }
    throw new Error(msg)
  }

  disconnect() {
    this.connected = false
    this.host = ''
  }
}

module.exports = { HttpClient, REQUEST_TIMEOUT_MS }
