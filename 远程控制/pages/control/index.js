/**
 * 远程控制主页面：独立小程序；命令表与 PC knownCommands 一致。
 */
const {
  CMD_KEYS, defaultBtnState, computeBtnState, isRemoteOffline, formatMetrics
} = require('../../utils/remote-buttons')
const WifiLink = require('../../utils/wifi-link')
const BleLink = require('../../utils/ble-link')
const PreviewStream = require('../../utils/preview-stream')
const { humanizeError } = require('../../utils/errors')

const STORAGE_HOST = 'rc_host'
const STORAGE_TOKEN = 'rc_token'
const STORAGE_MODE = 'rc_mode'

const PREVIEW_HINT = {
  NEED_CONNECT: '须先建立主机连接',
  WIFI_ONLY: '图像预览仅支持 WiFi 通道',
  PREVIEW_OFF: '预览已关闭',
  NEED_OPEN_CAMERA: '须执行「打开相机」',
  LIVE_NOT_READY: '实时预览未就绪',
  NO_FRAME: '暂无图像数据'
}

const EMPTY_METRICS = {
  metricCam: '—',
  metricCalc: '—',
  metricMsg: '—',
  previewUrlA: '',
  previewUrlB: '',
  previewActive: 0,
  previewHint: PREVIEW_HINT.NEED_CONNECT
}

Page({
  data: {
    mode: 'wifi',
    connState: 'disconnected',
    statusTitle: '未连接',
    statusDetail: '须配置主机地址并建立连接',
    errorHint: '',
    connected: false,
    pendingAction: '',
    token: '1234',
    host: '',
    devices: [],
    selectedDeviceId: '',
    btnState: defaultBtnState(true),
    connConnectDisabled: false,
    connDisconnectDisabled: true,
    uiLocked: false,
    linkPanelOpen: true,
    previewOn: false,
    ...EMPTY_METRICS
  },

  onLoad() {
    this._alive = true
    this._lastStatus = {}
    this._actionLock = false

    this.wifiLink = new WifiLink()
    this.bleLink = new BleLink()
    this._previewStream = new PreviewStream(() => {
      if (!this.wifiLink.isConnected()) return ''
      return this.wifiLink.getPreviewUrl((this.data.token || '').trim())
    })
    this._previewStream.setOnFrame((slot, url) => {
      const key = slot === 0 ? 'previewUrlA' : 'previewUrlB'
      this._safeSetData({ [key]: url })
    })

    this.wifiLink.setOnStatus((status) => {
      if (!this._alive || this.data.mode !== 'wifi') return
      this.applyRemoteStatus(status)
    })
    this.wifiLink.setOnConnectionLost((reason) => {
      if (!this._alive || this.data.mode !== 'wifi') return
      this.handleWifiLost(reason)
    })

    this.bleLink.setOnStatus((status) => {
      if (!this._alive || this.data.mode !== 'ble') return
      this.applyRemoteStatus(status)
    })
    this.bleLink.setOnStateChange((state, detail) => {
      if (!this._alive || this.data.mode !== 'ble') return
      this.applyBleConnState(state, detail)
    })

    try {
      const host = wx.getStorageSync(STORAGE_HOST) || ''
      const token = wx.getStorageSync(STORAGE_TOKEN) || '1234'
      const mode = wx.getStorageSync(STORAGE_MODE) || 'wifi'
      this._safeSetData({
        host,
        token,
        mode,
        statusDetail: mode === 'wifi' ? '客户端与主机须处于同一局域网' : '扫描后选择目标 BLE 设备'
      })
    } catch (e) {}
  },

  onShow() {
    this._resumePollForCurrentMode()
  },

  onHide() {
    this._stopAllPoll()
  },

  onUnload() {
    this._alive = false
    this._actionLock = false
    this._stopAllPoll()
    const token = (this.data.token || '').trim()
    this.wifiLink.setOnStatus(null)
    this.wifiLink.setOnConnectionLost(null)
    this.bleLink.setOnStatus(null)
    this.bleLink.setOnStateChange(null)
    this.wifiLink.disconnect(token).catch(() => {})
    this.bleLink.disconnect(token).catch(() => {})
    this.bleLink.closeAdapter()
  },

  _activeLink() {
    return this.data.mode === 'wifi' ? this.wifiLink : this.bleLink
  },

  _stopAllPoll() {
    this.wifiLink.stopPoll()
    this.bleLink.stopPoll()
    this._previewStream.stop()
  },

  _resumePollForCurrentMode() {
    if (!this.data.connected) return
    const token = (this.data.token || '').trim()
    if (this.data.mode === 'wifi' && this.wifiLink.isConnected()) {
      this.wifiLink.startPoll(token)
      this._syncPreview(this._lastStatus)
    } else if (this.data.mode === 'ble' && this.bleLink.isConnected()) {
      this.bleLink.startPoll(token)
    }
  },

  _safeSetData(patch, cb) {
    if (!this._alive || !patch || !Object.keys(patch).length) return
    this.setData(patch, cb)
  },

  _savePrefs() {
    try {
      wx.setStorageSync(STORAGE_HOST, (this.data.host || '').trim())
      wx.setStorageSync(STORAGE_TOKEN, (this.data.token || '').trim())
      wx.setStorageSync(STORAGE_MODE, this.data.mode)
    } catch (e) {}
  },

  _setPending(action) {
    const pending = action || ''
    const uiLocked = !!action
    const locked = uiLocked || !this.data.connected ||
      !!(this._lastStatus && this._lastStatus.remoteControlBlocked)
    const btnMap = computeBtnState(this._lastStatus, locked)
    const patch = { pendingAction: pending, uiLocked }
    CMD_KEYS.forEach((k) => {
      if (this.data.btnState[k] !== btnMap[k]) patch['btnState.' + k] = btnMap[k]
    })
    const connected = this.data.connected
    const nextConnect = uiLocked || connected
    const nextDisconnect = uiLocked || !connected
    if (this.data.connConnectDisabled !== nextConnect) patch.connConnectDisabled = nextConnect
    if (this.data.connDisconnectDisabled !== nextDisconnect) patch.connDisconnectDisabled = nextDisconnect
    this._safeSetData(patch)
  },

  _refreshConnBtns() {
    const pending = !!this.data.pendingAction
    const connected = this.data.connected
    const patch = {}
    const nextConnect = pending || connected
    const nextDisconnect = pending || !connected
    if (this.data.connConnectDisabled !== nextConnect) patch.connConnectDisabled = nextConnect
    if (this.data.connDisconnectDisabled !== nextDisconnect) patch.connDisconnectDisabled = nextDisconnect
    if (Object.keys(patch).length) this._safeSetData(patch)
  },

  _refreshBtnState() {
    const locked = !!this.data.pendingAction || !this.data.connected ||
      !!(this._lastStatus && this._lastStatus.remoteControlBlocked)
    const next = computeBtnState(this._lastStatus, locked)
    const patch = {}
    CMD_KEYS.forEach((k) => {
      if (this.data.btnState[k] !== next[k]) patch['btnState.' + k] = next[k]
    })
    if (Object.keys(patch).length) this._safeSetData(patch)
  },

  _previewHintIdle(connected, mode) {
    const m = mode !== undefined ? mode : this.data.mode
    const c = connected !== undefined ? connected : this.data.connected
    if (m === 'ble') return PREVIEW_HINT.WIFI_ONLY
    if (!c) return PREVIEW_HINT.NEED_CONNECT
    return PREVIEW_HINT.NEED_OPEN_CAMERA
  },

  _stopPreviewUi(hint) {
    this._previewStream.stop()
    this._safeSetData({
      previewUrlA: '',
      previewUrlB: '',
      previewActive: 0,
      previewHint: hint || PREVIEW_HINT.NEED_CONNECT
    })
  },

  _syncPreview(status) {
    const H = PREVIEW_HINT
    if (!this.data.previewOn) {
      this._stopPreviewUi(this.data.connected ? H.PREVIEW_OFF : H.NEED_CONNECT)
      return
    }
    if (this.data.mode === 'ble') {
      this._stopPreviewUi(H.WIFI_ONLY)
      return
    }
    if (!this.data.connected) {
      this._stopPreviewUi(H.NEED_CONNECT)
      return
    }
    const ready = !!(status && status.cameraOpen && status.liveViewActive)
    if (ready) {
      if (!this._previewStream.isRunning()) {
        this._previewStream.start()
      }
      if (!this.data.previewUrlA && !this.data.previewUrlB && this.data.previewHint !== H.NO_FRAME) {
        this._safeSetData({ previewHint: H.NO_FRAME })
      }
    } else if (status && status.cameraOpen && !status.liveViewActive) {
      this._stopPreviewUi(H.LIVE_NOT_READY)
    } else {
      this._stopPreviewUi(H.NEED_OPEN_CAMERA)
    }
  },

  onPreviewLoad(e) {
    const slot = Number(e.currentTarget.dataset.slot)
    if (slot !== this._previewStream.pendingSlot) return
    this._safeSetData({ previewActive: slot, previewHint: '' })
    this._previewStream.frameDone(true)
  },

  onPreviewError() {
    this._previewStream.frameDone(false)
  },

  onToggleLinkPanel() {
    if (this.data.uiLocked) return
    this._safeSetData({ linkPanelOpen: !this.data.linkPanelOpen })
  },

  _applyLinkConnected(endpoint) {
    this._safeSetData({
      connected: true,
      connState: 'connected',
      statusTitle: '已连接',
      statusDetail: endpoint || '',
      errorHint: '',
      connConnectDisabled: true,
      connDisconnectDisabled: false,
      linkPanelOpen: false,
      previewUrlA: '',
      previewUrlB: '',
      previewActive: 0,
      previewHint: this._previewHintIdle(true)
    })
    this._refreshBtnState()
    this._refreshConnBtns()
    this._resumePollForCurrentMode()
  },

  _applyDisconnectedUi(detail) {
    this.wifiLink.stopPoll()
    this.bleLink.stopPoll()
    this._previewStream.stop()
    this._lastStatus = {}
    this._safeSetData({
      connected: false,
      connState: 'disconnected',
      statusTitle: '未连接',
      statusDetail: detail || '已断开',
      errorHint: '',
      pendingAction: '',
      uiLocked: false,
      previewOn: false,
      btnState: defaultBtnState(true),
      connConnectDisabled: false,
      connDisconnectDisabled: true,
      selectedDeviceId: '',
      linkPanelOpen: true,
      ...EMPTY_METRICS,
      previewHint: this._previewHintIdle(false)
    })
  },

  applyRemoteStatus(status) {
    if (!status || typeof status !== 'object') return

    if (status.remoteEnabled === false) {
      this._stopPreviewUi(PREVIEW_HINT.PREVIEW_OFF)
      this._safeSetData({ previewOn: false })
    }

    if (this.data.mode === 'ble' && this.data.connected && isRemoteOffline(status)) {
      this.handleBlePcOffline(status.message || status.msg || '主机服务已停止')
      return
    }

    this._lastStatus = status
    const patch = formatMetrics(status)

    if (status.remoteControlBlocked) {
      patch.errorHint = status.remoteControlMessage || '命令通道被占用'
    } else if (this.data.errorHint && this.data.errorHint.indexOf('占用') >= 0) {
      patch.errorHint = ''
    }

    this._safeSetData(patch)
    if (!this.data.pendingAction) this._refreshBtnState()
    this._syncPreview(status)
  },

  async runAction(action, work) {
    if (this._actionLock) return
    this._actionLock = true
    this._stopAllPoll()
    this._setPending(action)
    try {
      await work()
    } finally {
      this._actionLock = false
      this._setPending('')
      this._resumePollForCurrentMode()
    }
  },

  handleWifiLost(reason) {
    const token = (this.data.token || '').trim()
    this.wifiLink.disconnect(token).catch(() => {})
    this._previewStream.stop()
    this._lastStatus = {}
    this._safeSetData({
      connected: false,
      connState: 'error',
      statusTitle: '已断开',
      statusDetail: humanizeError(reason, 'lost'),
      errorHint: humanizeError(reason, 'lost'),
      pendingAction: '',
      uiLocked: false,
      previewOn: false,
      btnState: defaultBtnState(true),
      connConnectDisabled: false,
      connDisconnectDisabled: true,
      linkPanelOpen: true,
      ...EMPTY_METRICS,
      previewHint: this._previewHintIdle(false)
    })
  },

  handleBlePcOffline(reason) {
    this.bleLink.stopPoll()
    this._previewStream.stop()
    this._lastStatus = {}
    const hint = reason || '主机服务已停止'
    this._safeSetData({
      connected: false,
      connState: 'error',
      statusTitle: '已断开',
      statusDetail: hint,
      errorHint: hint,
      pendingAction: '',
      uiLocked: false,
      previewOn: false,
      btnState: defaultBtnState(true),
      connConnectDisabled: false,
      connDisconnectDisabled: true,
      linkPanelOpen: true,
      ...EMPTY_METRICS,
      previewHint: this._previewHintIdle(false)
    })
    this.bleLink.disconnect().catch(() => {})
  },

  applyBleConnState(state, detail) {
    if (state === 'connected') return

    const patch = {}
    if (state === 'scanning') {
      patch.connState = 'scanning'
      patch.statusTitle = 'BLE 扫描中'
      patch.errorHint = ''
    } else if (state === 'connecting') {
      patch.connState = 'connecting'
      patch.statusTitle = '连接中'
      patch.errorHint = ''
    } else if (state === 'error') {
      patch.connState = 'error'
      patch.statusTitle = '连接异常'
      patch.connected = false
      patch.errorHint = detail || '连接异常'
      patch.btnState = defaultBtnState(true)
      patch.linkPanelOpen = true
      Object.assign(patch, EMPTY_METRICS)
      patch.previewHint = this._previewHintIdle(false)
      this._lastStatus = {}
      this.bleLink.stopPoll()
      this._previewStream.stop()
    } else if (state === 'idle') {
      if (this.data.connected && detail && detail.indexOf('断开') >= 0) {
        this._applyDisconnectedUi(detail)
        return
      }
      if (!this.data.connected) {
        patch.connState = 'disconnected'
        patch.statusTitle = '未连接'
      }
    }

    if (detail && state !== 'error' && state !== 'idle') patch.statusDetail = detail
    if (Object.keys(patch).length) {
      this._safeSetData(patch)
      this._refreshConnBtns()
    }
  },

  _teardownAllLinks() {
    this._stopAllPoll()
    const token = (this.data.token || '').trim()
    return Promise.all([
      this.wifiLink.disconnect(token).catch(() => {}),
      this.bleLink.disconnect(token).catch(() => {})
    ]).finally(() => {
      this.bleLink.closeAdapter()
    })
  },

  async onModeChange(e) {
    if (this.data.uiLocked) return
    const mode = e.currentTarget.dataset.mode
    if (!mode || mode === this.data.mode) return

    await this._teardownAllLinks()
    this._lastStatus = {}
    this._safeSetData({
      mode,
      connected: false,
      selectedDeviceId: '',
      devices: [],
      errorHint: '',
      statusTitle: '未连接',
      pendingAction: '',
      uiLocked: false,
      btnState: defaultBtnState(true),
      connConnectDisabled: false,
      connDisconnectDisabled: true,
      linkPanelOpen: true,
      statusDetail: mode === 'wifi' ? '客户端与主机须处于同一局域网' : '扫描后选择目标 BLE 设备',
      connState: 'disconnected',
      ...EMPTY_METRICS,
      previewHint: this._previewHintIdle(false, mode)
    })
    this._savePrefs()
  },

  onTokenInput(e) {
    if (this.data.uiLocked) return
    this._safeSetData({ token: e.detail.value })
  },

  onHostInput(e) {
    if (this.data.uiLocked) return
    this._safeSetData({ host: e.detail.value })
  },

  onRefresh() {
    if (this.data.mode !== 'ble' || this.data.uiLocked || this.data.connected) return
    this.runAction('scan', async () => {
      this._safeSetData({ errorHint: '', devices: [], connState: 'scanning', statusTitle: 'BLE 扫描中' })
      try {
        const list = await this.bleLink.scan()
        this._safeSetData({ devices: list, connState: 'disconnected', statusTitle: '未连接' })
        if (list.length === 0) {
          const err = this.bleLink.getLastError()
          if (err && err.message) this._safeSetData({ errorHint: err.message })
        }
      } catch (err) {
        this._safeSetData({
          errorHint: humanizeError(err, 'scan'),
          connState: 'error',
          statusTitle: '扫描异常'
        })
      }
    })
  },

  onConnect() {
    if (this.data.mode !== 'wifi' || this.data.uiLocked) return
    const host = (this.data.host || '').trim()
    if (!host) {
      this._safeSetData({ errorHint: '须配置主机地址（host:port）' })
      return
    }
    this.runAction('connect', async () => {
      this._safeSetData({
        errorHint: '',
        connState: 'connecting',
        statusTitle: '连接中',
        statusDetail: host
      })
      try {
        const token = (this.data.token || '').trim()
        await this.wifiLink.connect(host, token)
        this._savePrefs()
        this._lastStatus = {}
        this._applyLinkConnected(host)
      } catch (err) {
        this._safeSetData({
          connState: 'error',
          statusTitle: '连接异常',
          errorHint: humanizeError(err, 'connect')
        })
      }
    })
  },

  onSelectDevice(e) {
    if (this.data.mode !== 'ble' || this.data.uiLocked) return
    const deviceId = e.currentTarget.dataset.id
    const name = e.currentTarget.dataset.name || deviceId
    if (!deviceId) return
    this.runAction('connect', async () => {
      this._safeSetData({
        selectedDeviceId: deviceId,
        connState: 'connecting',
        statusTitle: '连接中',
        statusDetail: name,
        errorHint: ''
      })
      try {
        if (this.bleLink.isConnected()) await this.bleLink.disconnect()
        const token = (this.data.token || '').trim()
        const res = await this.bleLink.connect(deviceId, token)
        this._savePrefs()
        this._lastStatus = {}
        this._applyLinkConnected(res.name || name)
        this._safeSetData({ selectedDeviceId: deviceId })
      } catch (err) {
        this._safeSetData({
          connState: 'error',
          errorHint: humanizeError(err, 'ble'),
          statusTitle: '连接异常'
        })
      }
    })
  },

  onDisconnect() {
    if (this.data.uiLocked || !this.data.connected) return
    this.runAction('disconnect', async () => {
      const token = (this.data.token || '').trim()
      if (this.data.mode === 'wifi') await this.wifiLink.disconnect(token)
      else await this.bleLink.disconnect(token)
      this._applyDisconnectedUi('已断开')
      this._refreshConnBtns()
    })
  },

  onCmdTap(e) {
    const cmd = e.currentTarget.dataset.cmd
    if (!cmd || this.data.uiLocked || this.data.btnState[cmd]) return
    const link = this._activeLink()
    this.runAction(cmd, async () => {
      this._safeSetData({ errorHint: '' })
      const token = (this.data.token || '').trim()
      try {
        await link.sendCommand(cmd, token)
      } catch (err) {
        this._safeSetData({ errorHint: humanizeError(err, 'command') })
      }
    })
  },

  onStatusTap() {
    if (this.data.uiLocked || !this.data.connected) return
    const link = this._activeLink()
    this.runAction('status', async () => {
      const token = (this.data.token || '').trim()
      try {
        await link.sendCommand('status', token)
      } catch (err) {
        this._safeSetData({ errorHint: humanizeError(err, 'command') })
      }
    })
  },

  onPreviewOn() {
    if (!this.data.connected || this.data.previewOn) return
    this._safeSetData({ previewOn: true })
    this._syncPreview(this._lastStatus)
  },

  onPreviewOff() {
    if (!this.data.previewOn) return
    this._stopPreviewUi(PREVIEW_HINT.PREVIEW_OFF)
    this._safeSetData({ previewOn: false })
  }
})
