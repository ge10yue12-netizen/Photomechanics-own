/**
 * 遥控命令与按钮规则（与 PC RemoteCommands knownCommands 对齐）。
 * open_camera / close_camera / start_calculate / stop_calculate / calibrate / status
 */

const CMD_LABELS = {
  open_camera: '打开相机',
  close_camera: '关闭相机',
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
  return status.ok === false || status.ok === 0
}

/** 计算进行中：优先 calculateActive，兼容旧字段 acquisitionActive。 */
function isCalculateRunning(status) {
  const s = status || {}
  if (s.calculateActive !== undefined) return !!s.calculateActive
  if (s.calc !== undefined) return !!s.calc
  return !!s.acquisitionActive
}

function computeBtnState(status, locked) {
  const s = status || {}
  const open = !!s.cameraOpen
  const calc = isCalculateRunning(s)
  return {
    open_camera: locked || open,
    close_camera: locked || !open || calc,
    start_calculate: locked || !open || calc,
    stop_calculate: locked || !open || !calc,
    calibrate: locked || !open || calc
  }
}

function formatMetrics(status) {
  const s = status || {}
  const calc = isCalculateRunning(s)
  return {
    metricCam: s.cameraOpen ? '已打开' : '未打开',
    metricCalc: calc ? '进行中' : (s.liveViewActive ? '预览' : '空闲'),
    metricMsg: s.message || '—',
    cameraOpen: !!s.cameraOpen
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
  isCalculateRunning
}
