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
    if (!this.host) throw new Error('须配置主机地址（host:port）')
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
    if (!this.connected || !this.host) throw new Error('主机未连接')
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
    throw new Error(humanizeHttpStatus(res.statusCode, bodyMsg))
  }

  async release(token) {
    if (!this.host) return
    try {
      await this.request({
        url: `${this.baseUrl()}/api/release?token=${encodeURIComponent(token || '')}`,
        method: 'POST',
        header: { 'content-type': 'application/json' },
        data: { token: token || '' }
      })
    } catch (_) {}
  }

  /** 请求 PC 关闭远程服务（路径 /api/remote/off，与 PC 端约定一致）。 */
  async remoteOff(token) {
    if (!this.host) return
    await this.request({
      url: `${this.baseUrl()}/api/remote/off?token=${encodeURIComponent(token || '')}`,
      method: 'POST',
      header: { 'content-type': 'application/json' },
      data: { token: token || '' }
    })
  }

  async disconnect(token) {
    if (this.connected && this.host)
      await this.release(token)
    this.connected = false
    this.host = ''
  }

  previewUrl(token) {
    if (!this.host) return ''
    return `${this.baseUrl()}/api/preview.jpg?token=${encodeURIComponent(token || '')}&t=${Date.now()}`
  }
}

module.exports = { HttpClient, REQUEST_TIMEOUT_MS }
