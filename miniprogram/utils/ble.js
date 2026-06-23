const { SERVICE_UUID, CMD_UUID, STATUS_UUID, normalizeUuid, parseAdvertisData, buildCommand, parseStatus, str2ab } = require('./protocol')

const SCAN_MS = 12000

const ErrorCode = {
  ADAPTER_OFF: 'ADAPTER_OFF',
  AUTH_DENIED: 'AUTH_DENIED',
  SCAN_FAILED: 'SCAN_FAILED',
  CONNECT_FAILED: 'CONNECT_FAILED',
  NO_SERVICE: 'NO_SERVICE',
  NO_GATT: 'NO_GATT',
  NOT_CONNECTED: 'NOT_CONNECTED'
}

function classifyBleError(err) {
  const msg = (err && (err.errMsg || err.message)) || String(err || '')
  if (msg.indexOf('auth') >= 0 || msg.indexOf('authorize') >= 0) {
    return { code: ErrorCode.AUTH_DENIED, message: '未授权蓝牙/定位：请在系统设置中允许微信使用蓝牙，Android 还需开启定位' }
  }
  if (msg.indexOf('not available') >= 0 || msg.indexOf('10001') >= 0) {
    return { code: ErrorCode.ADAPTER_OFF, message: '手机蓝牙未开启或不可用' }
  }
  if (msg.indexOf('no service') >= 0 || msg.indexOf('10004') >= 0) {
    return { code: ErrorCode.NO_SERVICE, message: '该设备没有 PhotoMech 遥控服务（可能不是目标电脑）' }
  }
  if (msg.indexOf('no connection') >= 0 || msg.indexOf('10006') >= 0) {
    return { code: ErrorCode.NOT_CONNECTED, message: '蓝牙连接已断开' }
  }
  return { code: ErrorCode.CONNECT_FAILED, message: msg }
}

function promisify(fn) {
  return (options = {}) => new Promise((resolve, reject) => {
    fn({
      ...options,
      success: resolve,
      fail: reject
    })
  })
}

class BleClient {
  constructor() {
    this.deviceId = ''
    this.serviceId = ''
    this.cmdCharId = ''
    this.statusCharId = ''
    this.connected = false
    this.state = 'idle'
    this.onStatus = null
    this.onStateChange = null
    this._devices = new Map()
    this._lastError = null
    this._connStateHandler = null
  }

  _bindConnectionStateListener() {
    this._unbindConnectionStateListener()
    this._connStateHandler = (res) => {
      if (!res || res.deviceId !== this.deviceId) return
      if (res.connected) return
      this.connected = false
      const id = this.deviceId
      this.deviceId = ''
      this.serviceId = ''
      this.cmdCharId = ''
      this.statusCharId = ''
      this._setState('idle', '蓝牙连接已断开')
      if (id) wx.closeBLEConnection({ deviceId: id, complete: () => {} })
    }
    wx.onBLEConnectionStateChange(this._connStateHandler)
  }

  _unbindConnectionStateListener() {
    if (!this._connStateHandler) return
    wx.offBLEConnectionStateChange(this._connStateHandler)
    this._connStateHandler = null
  }

  _setState(state, detail) {
    this.state = state
    if (this.onStateChange) this.onStateChange(state, detail || '')
  }

  _serviceUuids(dev) {
    const parsed = parseAdvertisData(dev.advertisData)
    const listed = dev.advertisServiceUUIDs || []
    return listed.concat(parsed)
  }

  _remember(dev) {
    if (!dev || !dev.deviceId) return
    const prev = this._devices.get(dev.deviceId) || {}
    const name = dev.name || dev.localName || prev.name || ''
    const merged = Object.assign({}, prev, dev, { displayName: name || '未知设备' })
    merged.serviceUuids = this._serviceUuids(merged)
    merged.isRemote = this._isPreferred(merged)
    this._devices.set(dev.deviceId, merged)
  }

  _isPreferred(dev) {
    const targetService = normalizeUuid(SERVICE_UUID)
    const ads = dev.serviceUuids || this._serviceUuids(dev)
    if (ads.some((u) => normalizeUuid(u) === targetService)) return true
    const name = (dev.name || dev.localName || '').toUpperCase()
    return name.indexOf('PHOTOMECH') >= 0
  }

  getDeviceList() {
    return Array.from(this._devices.values())
      .sort((a, b) => {
        if (a.isRemote !== b.isRemote) return a.isRemote ? -1 : 1
        return (b.RSSI || -100) - (a.RSSI || -100)
      })
      .map((d) => ({
        deviceId: d.deviceId,
        name: d.displayName,
        subtitle: d.deviceId,
        rssi: d.RSSI,
        isRemote: !!d.isRemote
      }))
  }

  getLastError() {
    return this._lastError
  }

  async openAdapter() {
    try {
      await promisify(wx.openBluetoothAdapter)()
    } catch (e) {
      throw classifyBleError(e)
    }
  }

  async requestPermissions() {
    try {
      const setting = await promisify(wx.getSetting)()
      if (!setting.authSetting['scope.userLocation']) {
        await promisify(wx.authorize)({ scope: 'scope.userLocation' })
      }
    } catch (e) {
      // 用户拒绝定位时仍尝试扫描
    }
  }

  async closeAdapter() {
    try {
      await promisify(wx.closeBluetoothAdapter)()
    } catch (e) {}
  }

  async refreshDeviceList(onProgress) {
    this._lastError = null
    this._setState('scanning', '正在搜索附近蓝牙设备…')
    await this.requestPermissions()
    await this.openAdapter()

    return new Promise((resolve, reject) => {
      let settled = false
      const finish = (fn) => {
        if (settled) return
        settled = true
        wx.offBluetoothDeviceFound(onDevice)
        wx.stopBluetoothDevicesDiscovery({ complete: () => {} })
        fn()
      }

      const onDevice = (res) => {
        const devices = res.devices || []
        for (const dev of devices) this._remember(dev)
        if (onProgress) onProgress(`已发现 ${this._devices.size} 个设备`)
      }

      wx.onBluetoothDeviceFound(onDevice)
      wx.startBluetoothDevicesDiscovery({
        allowDuplicatesKey: true,
        powerLevel: 'high',
        success: () => {},
        fail: (e) => finish(() => {
          const err = classifyBleError(e)
          this._lastError = err
          this._setState('error', err.message)
          reject(err)
        })
      })

      setTimeout(async () => {
        if (settled) return
        try {
          const cached = await promisify(wx.getBluetoothDevices)()
          ;(cached.devices || []).forEach((d) => this._remember(d))
        } catch (e) {}

        const list = this.getDeviceList()
        finish(() => {
          if (list.length === 0) {
            const err = {
              code: 'NO_DEVICES',
              message: '未发现任何蓝牙设备。请打开手机蓝牙、靠近 PC（1～3 米），Android 需开启定位。'
            }
            this._lastError = err
            this._setState('idle', err.message)
            resolve(list)
            return
          }
          this._setState('idle', `共 ${list.length} 个设备，请点选你的电脑`)
          resolve(list)
        })
      }, SCAN_MS)
    })
  }

  async connectToDevice(deviceId) {
    if (!deviceId) {
      const err = { code: ErrorCode.CONNECT_FAILED, message: '未选择设备' }
      this._lastError = err
      throw err
    }

    this._lastError = null
    this._setState('connecting', '正在连接…')
    await this.openAdapter()

    try {
      await this._probeAndSetup(deviceId)
      const dev = this._devices.get(deviceId)
      const name = dev ? dev.displayName : deviceId
      this._setState('connected', `已连接：${name}`)
      return { deviceId, name }
    } catch (e) {
      const err = e.code ? e : classifyBleError(e)
      if (err.code === ErrorCode.NO_SERVICE || (err.message && err.message.indexOf('PhotoMech') >= 0)) {
        err.message = '连接成功但未找到遥控服务。请确认 PC 端软件已启动且日志显示「BLE 遥控已启动」。'
        err.code = ErrorCode.NO_SERVICE
      }
      this._lastError = err
      this._setState('error', err.message)
      await this._safeClose(deviceId)
      throw err
    }
  }

  async _probeAndSetup(deviceId) {
    await promisify(wx.createBLEConnection)({ deviceId, timeout: 12000 })
    this.deviceId = deviceId
    this.connected = true
    await this._setupGatt()
    this._bindConnectionStateListener()
  }

  async _safeClose(deviceId) {
    try {
      await promisify(wx.closeBLEConnection)({ deviceId })
    } catch (e) {}
    if (this.deviceId === deviceId) {
      this.connected = false
      this.deviceId = ''
      this.serviceId = ''
      this.cmdCharId = ''
      this.statusCharId = ''
    }
  }

  async _setupGatt() {
    try {
      if (wx.setBLEMTU) {
        await promisify(wx.setBLEMTU)({ deviceId: this.deviceId, mtu: 185 })
      }
    } catch (e) {}

    const services = await promisify(wx.getBLEDeviceServices)({ deviceId: this.deviceId })
    const service = (services.services || []).find((s) => normalizeUuid(s.uuid) === normalizeUuid(SERVICE_UUID))
    if (!service) {
      const err = { code: ErrorCode.NO_SERVICE, message: '该设备没有 PhotoMech 遥控服务' }
      throw err
    }
    this.serviceId = service.uuid

    const chars = await promisify(wx.getBLEDeviceCharacteristics)({
      deviceId: this.deviceId,
      serviceId: service.uuid
    })
    const list = chars.characteristics || []
    const cmdChar = list.find((c) => normalizeUuid(c.uuid) === normalizeUuid(CMD_UUID))
    const statusChar = list.find((c) => normalizeUuid(c.uuid) === normalizeUuid(STATUS_UUID))
    if (!cmdChar || !statusChar) {
      throw { code: ErrorCode.NO_GATT, message: '未找到命令/状态特征，请更新 PC 端软件' }
    }

    this.cmdCharId = cmdChar.uuid
    this.statusCharId = statusChar.uuid

    await promisify(wx.notifyBLECharacteristicValueChange)({
      deviceId: this.deviceId,
      serviceId: service.uuid,
      characteristicId: statusChar.uuid,
      state: true
    })

    wx.onBLECharacteristicValueChange((res) => {
      if (normalizeUuid(res.characteristicId) !== normalizeUuid(STATUS_UUID)) return
      const status = parseStatus(res.value)
      if (this.onStatus) this.onStatus(status)
    })

    await this.sendCommand('status')
  }

  async disconnect() {
    this._unbindConnectionStateListener()
    if (!this.deviceId) {
      this._setState('idle', '未连接')
      return
    }
    const id = this.deviceId
    try {
      await promisify(wx.closeBLEConnection)({ deviceId: id })
    } catch (e) {}
    this.connected = false
    this.deviceId = ''
    this.serviceId = ''
    this.cmdCharId = ''
    this.statusCharId = ''
    this._setState('idle', '已断开')
  }

  async sendCommand(cmd, token) {
    if (!this.connected || !this.deviceId || !this.cmdCharId || !this.serviceId) {
      const err = { code: ErrorCode.NOT_CONNECTED, message: '尚未连接设备' }
      throw err
    }
    const text = buildCommand(cmd, token)
    const buffer = str2ab(text)
    try {
      await promisify(wx.writeBLECharacteristicValue)({
        deviceId: this.deviceId,
        serviceId: this.serviceId,
        characteristicId: this.cmdCharId,
        value: buffer
      })
    } catch (e) {
      throw classifyBleError(e)
    }
  }
}

module.exports = {
  BleClient,
  ErrorCode
}
