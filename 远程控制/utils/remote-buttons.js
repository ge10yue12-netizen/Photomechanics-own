/**
 * 遥控命令与按钮规则（与 PC RemoteCommands knownCommands 对齐）。
 * 状态字段与 buildRemoteStatusJson / BLE 紧凑 JSON 对齐。
 */

const CMD_LABELS = {
  open_camera: '打开相机',
  close_camera: '关闭相机',
  open_preview: '开启预览',
  close_preview: '关闭预览',
  start_calculate: '开始计算',
  stop_calculate: '停止计算',
  calibrate: '标定'
}

const CMD_KEYS = Object.keys(CMD_LABELS)
const POLL_MS = 2000
const PREVIEW_MS = 120

function defaultBtnState(disabled) {
  const s = {}
  CMD_KEYS.forEach((k) => { s[k] = !!disabled })
  return s
}

function isRemoteOffline(status) {
  if (!status || typeof status !== 'object') return false
  if (status.remoteEnabled === false) return true
  return status.ok === false || status.ok === 0
}

function isCalculateRunning(status) {
  const s = status || {}
  if (s.calculateActive !== undefined) return !!s.calculateActive
  if (s.calc !== undefined) return !!s.calc
  return !!s.acquisitionActive
}

function isRemotePreviewOn(status) {
  const s = status || {}
  if (s.remotePreviewActive !== undefined) return !!s.remotePreviewActive
  if (s.rp !== undefined) return !!s.rp
  return false
}

function formatStateText(status) {
  const s = status || {}
  if (s.stageRunning) return '阶段采集中'
  if (isCalculateRunning(s)) return '计算中'
  if (isRemotePreviewOn(s)) return '远程预览中'
  if (s.cameraOpen) return '可开远程预览'
  return '待机'
}

function formatStateClass(status) {
  const s = status || {}
  if (s.stageRunning || isCalculateRunning(s) || isRemotePreviewOn(s)) return 'metric-ok'
  if (s.cameraOpen) return 'metric-warn'
  return ''
}

/** 将主机 status JSON 映射为界面状态格（链路 / 相机 / 状态）。 */
function formatMetrics(status, opts) {
  const s = status || {}
  const connected = !!(opts && opts.connected)
  const offline = connected && isRemoteOffline(s)

  let metricState = formatStateText(s)
  let metricStateClass = formatStateClass(s)
  if (offline) {
    metricState = s.remoteEnabled === false ? '远程已关闭' : '主机异常'
    metricStateClass = 'metric-err'
  }

  return {
    metricLink: !connected ? '—' : (offline ? '异常' : '正常'),
    metricLinkClass: !connected ? '' : (offline ? 'metric-err' : 'metric-ok'),
    metricCam: s.cameraOpen ? '已打开' : '未打开',
    metricCamClass: s.cameraOpen ? 'metric-ok' : 'metric-warn',
    metricState,
    metricStateClass,
    cameraOpen: !!s.cameraOpen
  }
}

function computeBtnState(status, locked) {
  const s = status || {}
  const offline = isRemoteOffline(s)
  const lock = locked || offline
  const open = !!s.cameraOpen
  const remotePreview = isRemotePreviewOn(s)
  const calc = isCalculateRunning(s)
  return {
    open_camera: lock || open,
    close_camera: lock || !open,
    open_preview: lock || !open || remotePreview,
    close_preview: lock || !open || !remotePreview,
    start_calculate: lock || !open || calc,
    stop_calculate: lock || !open || !calc,
    calibrate: lock || !open || calc
  }
}

module.exports = {
  CMD_LABELS,
  CMD_KEYS,
  POLL_MS,
  PREVIEW_MS,
  defaultBtnState,
  computeBtnState,
  isRemoteOffline,
  formatMetrics,
  formatStateText,
  isCalculateRunning,
  isRemotePreviewOn
}
