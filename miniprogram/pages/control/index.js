const { BleClient } = require('../../utils/ble')
const { HttpClient } = require('../../utils/http')
const { humanizeError } = require('../../utils/errors')

const STORAGE_HOST = 'photomech_host'
const STORAGE_TOKEN = 'photomech_token'
const STORAGE_MODE = 'photomech_mode'
const POLL_MS = 2000

const CMD_LABELS = {
  open_camera: '打开相机',
  close_camera: '关闭相机',
  start_capture: '开始采集',
  stop_capture: '停止采集',
  save_one: '保存单张',
  start_stage: '开始阶段',
  stop_stage: '停止阶段'
}

const CMD_KEYS = Object.keys(CMD_LABELS)

const bleClient = new BleClient()
const httpClient = new HttpClient()

function defaultBtnState(disabled) {
  const s = {}
  CMD_KEYS.forEach((k) => { s[k] = !!disabled })
  return s
}

Page({
  data: {
    mode: 'wifi',
    connState: 'disconnected',
    statusTitle: '未连接',
    statusDetail: '填写 PC 地址后点「连接」',
    errorHint: '',
    connected: false,
    pendingAction: '',
    linkLive: false,
    deviceName: '',
    statusText: '—',
    queueText: '—',
    token: '1234',
    host: '',
    devices: [],
    selectedDeviceId: '',
    btnState: defaultBtnState(true),
    connConnectDisabled: false,
    connDisconnectDisabled: true,
    uiLocked: false
  },

  onLoad() {
    this._alive = true
    this._lastStatus = {}
    this._actionLock = false

    const onStatus = (status) => {
      if (!this._alive) return
      this.applyRemoteStatus(status)
    }
    bleClient.onStatus = onStatus
    httpClient.onStatus = onStatus

    httpClient.onConnectionLost = (reason) => {
      if (!this._alive || this.data.mode !== 'wifi') return
      this.handleConnectionLost(reason)
    }

    bleClient.onStateChange = (state, detail) => {
      if (!this._alive || this.data.mode !== 'ble') return
      this.applyConnState(state, detail)
    }

    try {
      const host = wx.getStorageSync(STORAGE_HOST) || ''
      const token = wx.getStorageSync(STORAGE_TOKEN) || '1234'
      const mode = wx.getStorageSync(STORAGE_MODE) || 'wifi'
      this._safeSetData({
        host,
        token,
        mode,
        statusDetail: mode === 'wifi'
          ? '地址与 PC 须同一 WiFi'
          : '刷新列表后点选电脑；不稳定请用 WiFi 模式'
      })
    } catch (e) {}
  },

  onShow() {
    if (this.data.mode === 'wifi' && this.data.connected && httpClient.connected) {
      this._startWifiPoll()
    }
  },

  onHide() {
    this._stopWifiPoll()
  },

  onUnload() {
    this._alive = false
    this._actionLock = false
    this._stopWifiPoll()
    bleClient.onStatus = null
    bleClient.onStateChange = null
    httpClient.onStatus = null
    httpClient.onConnectionLost = null
    bleClient.disconnect()
    bleClient.closeAdapter()
    httpClient.disconnect()
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
    const patch = { pendingAction: pending, uiLocked }
    const s = this._lastStatus || {}
    const open = !!s.cameraOpen
    const acq = !!s.acquisitionActive
    const stage = !!s.stageRunning
    const live = !!s.liveViewActive
    const locked = uiLocked || !this.data.connected
    const btnMap = {
      open_camera: locked || open,
      close_camera: locked || !open || stage,
      start_capture: locked || !open || acq || stage,
      stop_capture: locked || !open || !acq || stage,
      save_one: locked || !open || !acq || !live || stage,
      start_stage: locked || !open || acq || stage,
      stop_stage: locked || !stage
    }
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
    const nextConnect = pending || connected
    const nextDisconnect = pending || !connected
    const patch = {}
    if (this.data.connConnectDisabled !== nextConnect) patch.connConnectDisabled = nextConnect
    if (this.data.connDisconnectDisabled !== nextDisconnect) patch.connDisconnectDisabled = nextDisconnect
    if (Object.keys(patch).length) this._safeSetData(patch)
  },

  _refreshBtnState() {
    const s = this._lastStatus || {}
    const open = !!s.cameraOpen
    const acq = !!s.acquisitionActive
    const stage = !!s.stageRunning
    const live = !!s.liveViewActive
    const pending = !!this.data.pendingAction
    const locked = pending || !this.data.connected

    const next = {
      open_camera: locked || open,
      close_camera: locked || !open || stage,
      start_capture: locked || !open || acq || stage,
      stop_capture: locked || !open || !acq || stage,
      save_one: locked || !open || !acq || !live || stage,
      start_stage: locked || !open || acq || stage,
      stop_stage: locked || !stage
    }

    const patch = {}
    CMD_KEYS.forEach((k) => {
      if (this.data.btnState[k] !== next[k]) patch['btnState.' + k] = next[k]
    })
    if (Object.keys(patch).length) this._safeSetData(patch)
  },

  applyRemoteStatus(status) {
    if (!status || typeof status !== 'object') return
    this._lastStatus = status

    const msg = status.message || status.msg || '—'
    const q = status.queueSize !== undefined ? status.queueSize : status.q
    const qc = status.queueCapacity !== undefined ? status.queueCapacity : status.qc
    const queue = q !== undefined && q !== null ? `${q}/${qc || '?'}` : '—'

    const patch = {}
    if (msg !== this.data.statusText) patch.statusText = msg
    if (queue !== this.data.queueText) patch.queueText = queue
    if (this.data.connected && !this.data.linkLive) patch.linkLive = true
    if (Object.keys(patch).length) this._safeSetData(patch)

    if (!this.data.pendingAction) this._refreshBtnState()
  },

  async runAction(action, work) {
    if (this._actionLock) return
    this._actionLock = true
    this._stopWifiPoll()
    this._setPending(action)
    try {
      await work()
    } finally {
      this._actionLock = false
      this._setPending('')
      if (this.data.mode === 'wifi' && this.data.connected && httpClient.connected) {
        this._startWifiPoll()
      }
    }
  },

  handleConnectionLost(reason) {
    this._stopWifiPoll()
    httpClient.disconnect()
    this._lastStatus = {}
    const hint = humanizeError(reason, 'lost')
    this._safeSetData({
      connected: false,
      connState: 'error',
      statusTitle: '已断开',
      statusDetail: hint,
      errorHint: hint,
      statusText: '—',
      queueText: '—',
      linkLive: false,
      pendingAction: '',
      uiLocked: false,
      btnState: defaultBtnState(true),
      connConnectDisabled: false,
      connDisconnectDisabled: true
    })
  },

  _startWifiPoll() {
    this._stopWifiPoll()
    if (this.data.mode !== 'wifi' || !this.data.connected) return
    const tick = () => {
      if (!this.data.connected || !httpClient.connected || this.data.pendingAction) return
      const token = (this.data.token || '').trim()
      httpClient.fetchStatus(token).catch(() => {})
    }
    tick()
    this._pollTimer = setInterval(tick, POLL_MS)
  },

  _stopWifiPoll() {
    if (this._pollTimer) {
      clearInterval(this._pollTimer)
      this._pollTimer = null
    }
  },

  applyConnState(state, detail) {
    const map = {
      idle: { connState: 'disconnected', statusTitle: '未连接', connected: false, linkLive: false },
      scanning: { connState: 'scanning', statusTitle: '搜索中' },
      connecting: { connState: 'connecting', statusTitle: '连接中' },
      connected: { connState: 'connected', statusTitle: '已连接', connected: true, linkLive: true },
      error: { connState: 'error', statusTitle: '连接失败', connected: false, linkLive: false }
    }
    const patch = Object.assign({ statusDetail: detail || '' }, map[state] || {})
    if (state === 'idle' && detail && detail.indexOf('断开') >= 0) {
      patch.statusTitle = '已断开'
      patch.connState = 'error'
      patch.errorHint = detail
    }
    if (state === 'connected') patch.errorHint = ''
    else if (state === 'error') patch.errorHint = detail || ''
    else if (state !== 'idle' || !detail) patch.errorHint = ''
    if (!patch.connected) {
      patch.statusText = '—'
      patch.queueText = '—'
      patch.btnState = defaultBtnState(true)
      this._lastStatus = {}
    }
    this._safeSetData(patch)
    this._refreshConnBtns()
    if (!this.data.pendingAction && patch.connected) this._refreshBtnState()
  },

  onModeChange(e) {
    if (this.data.uiLocked) return
    const mode = e.currentTarget.dataset.mode
    if (!mode || mode === this.data.mode) return
    this._stopWifiPoll()
    bleClient.disconnect()
    bleClient.closeAdapter()
    httpClient.disconnect()
    this._lastStatus = {}
    this._safeSetData({
      mode,
      connected: false,
      deviceName: '',
      selectedDeviceId: '',
      devices: [],
      statusText: '—',
      queueText: '—',
      errorHint: '',
      statusTitle: '未连接',
      linkLive: false,
      pendingAction: '',
      uiLocked: false,
      btnState: defaultBtnState(true),
      connConnectDisabled: false,
      connDisconnectDisabled: true,
      statusDetail: mode === 'wifi'
        ? '地址与 PC 同一 WiFi'
        : '刷新列表后点选电脑；不稳定请用 WiFi模式',
      connState: 'disconnected'
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
      this._safeSetData({ errorHint: '', connState: 'scanning', statusTitle: '搜索中' })
      try {
        const list = await bleClient.refreshDeviceList((msg) => {
          if (this.data.pendingAction !== 'scan') return
          this._safeSetData({ statusDetail: msg })
        })
        this._safeSetData({ devices: list, connState: 'disconnected', statusTitle: '未连接' })
        if (list.length === 0) {
          const err = bleClient.getLastError()
          if (err && err.message) this._safeSetData({ errorHint: err.message })
        }
      } catch (err) {
        this._safeSetData({
          errorHint: humanizeError(err, 'scan'),
          connState: 'error',
          statusTitle: '搜索失败',
          statusDetail: '请按下方说明检查后重试'
        })
      }
    })
  },

  onConnect() {
    if (this.data.mode === 'ble' || this.data.uiLocked) return
    const host = (this.data.host || '').trim()
    if (!host) {
      this._safeSetData({ errorHint: '请填写 PC 地址，如 192.168.x.x:18765' })
      return
    }
    this.runAction('connect', async () => {
      this._safeSetData({
        errorHint: '',
        connState: 'connecting',
        statusTitle: '连接中',
        statusDetail: `连接 ${host}…`
      })
      try {
        const token = (this.data.token || '').trim()
        await httpClient.connect(host, token)
        this._savePrefs()
        this._lastStatus = {}
        this._safeSetData({
          connected: true,
          connState: 'connected',
          statusTitle: '已连接',
          statusDetail: host,
          deviceName: host,
          errorHint: '',
          linkLive: true
        })
        this._refreshConnBtns()
        this._refreshBtnState()
        this._startWifiPoll()
      } catch (err) {
        const hint = humanizeError(err, 'connect')
        this._safeSetData({
          connState: 'error',
          statusTitle: '连接失败',
          statusDetail: '请按下方说明检查后重试',
          errorHint: hint,
          linkLive: false
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
        statusDetail: `连接 ${name}…`,
        errorHint: ''
      })
      try {
        if (this.data.connected) await bleClient.disconnect()
        const res = await bleClient.connectToDevice(deviceId)
        this._lastStatus = {}
        this._safeSetData({
          deviceName: res.name,
          selectedDeviceId: deviceId,
          errorHint: '',
          linkLive: true
        })
        this._refreshBtnState()
      } catch (err) {
        this._safeSetData({
          errorHint: humanizeError(err, 'ble'),
          statusTitle: '连接失败',
          statusDetail: '请按下方说明检查后重试',
          linkLive: false
        })
      }
    })
  },

  onDisconnect() {
    if (this.data.uiLocked || !this.data.connected) return
    this.runAction('disconnect', async () => {
      this._stopWifiPoll()
      if (this.data.mode === 'wifi') httpClient.disconnect()
      else await bleClient.disconnect()
      this._lastStatus = {}
      this._safeSetData({
        connected: false,
        deviceName: '',
        selectedDeviceId: '',
        statusText: '—',
        queueText: '—',
        connState: 'disconnected',
        statusTitle: '未连接',
        statusDetail: '已断开',
        errorHint: '',
        linkLive: false,
        btnState: defaultBtnState(true)
      })
      this._refreshConnBtns()
    })
  },

  onCmdTap(e) {
    const cmd = e.currentTarget.dataset.cmd
    if (!cmd || this.data.uiLocked || this.data.btnState[cmd]) return
    this.runAction(cmd, async () => {
      this._safeSetData({ errorHint: '' })
      const token = (this.data.token || '').trim()
      try {
        if (this.data.mode === 'wifi') {
          await httpClient.sendCommand(cmd, token)
        } else {
          await bleClient.sendCommand(cmd, token)
        }
      } catch (err) {
        this._safeSetData({ errorHint: humanizeError(err, 'command') })
      }
    })
  }
})
