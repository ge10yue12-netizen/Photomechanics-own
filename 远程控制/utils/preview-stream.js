/**
 * WiFi 预览流：downloadFile 双缓冲 + 串行拉帧 + watchdog（兼容局域网 HTTP 真机）。
 */
const { PREVIEW_MS } = require('./remote-buttons')

const WATCHDOG_MS = 4000

class PreviewStream {
  constructor(urlFactory) {
    this._urlFactory = urlFactory
    this._polling = false
    this._busy = false
    this._timer = null
    this._watchdog = null
    this._onFrame = null
    this._activeSlot = 0
    this._pendingSlot = null
  }

  setOnFrame(handler) {
    this._onFrame = handler
  }

  isRunning() {
    return this._polling
  }

  get activeSlot() {
    return this._activeSlot
  }

  get pendingSlot() {
    return this._pendingSlot
  }

  /** 已在拉帧时不重复 start，避免状态轮询每 2s 打断预览。 */
  start() {
    if (this._polling) return
    this._polling = true
    this._busy = false
    this._tick()
  }

  stop() {
    this._polling = false
    this._busy = false
    this._pendingSlot = null
    if (this._timer) {
      clearTimeout(this._timer)
      this._timer = null
    }
    this._clearWatchdog()
  }

  _clearWatchdog() {
    if (this._watchdog) {
      clearTimeout(this._watchdog)
      this._watchdog = null
    }
  }

  _armWatchdog() {
    this._clearWatchdog()
    this._watchdog = setTimeout(() => {
      this._busy = false
      this._tick()
    }, WATCHDOG_MS)
  }

  _tick() {
    if (!this._polling || this._busy) return
    const url = this._urlFactory()
    if (!url) return
    this._busy = true
    this._armWatchdog()
    const slot = this._activeSlot === 0 ? 1 : 0
    this._pendingSlot = slot
    wx.downloadFile({
      url,
      success: (res) => {
        if (!this._polling) return
        if (res.statusCode === 200 && res.tempFilePath && this._onFrame) {
          this._onFrame(slot, res.tempFilePath)
        } else {
          this.frameDone(false)
        }
      },
      fail: () => {
        if (this._polling) this.frameDone(false)
      }
    })
  }

  frameDone(ok) {
    this._clearWatchdog()
    this._busy = false
    if (ok && this._pendingSlot !== null) {
      this._activeSlot = this._pendingSlot
    }
    this._pendingSlot = null
    if (this._polling) {
      this._timer = setTimeout(() => this._tick(), PREVIEW_MS)
    }
  }
}

module.exports = PreviewStream
