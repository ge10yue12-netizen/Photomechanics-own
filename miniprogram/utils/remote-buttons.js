/**
 * 遥控命令与按钮禁用规则（WiFi / BLE 共用，与 PC RemoteCommands 对齐）。
 */

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

function computeBtnState(status, locked) {
  const s = status || {}
  const open = !!s.cameraOpen
  const acq = !!s.acquisitionActive
  const stage = !!s.stageRunning
  const live = !!s.liveViewActive
  return {
    open_camera: locked || open,
    close_camera: locked || !open || stage,
    start_capture: locked || !open || acq || stage,
    stop_capture: locked || !open || !acq || stage,
    save_one: locked || !open || !acq || !live || stage,
    start_stage: locked || !open || acq || stage,
    stop_stage: locked || !stage
  }
}

/** 与扫码页状态格一致的中文摘要。 */
function formatMetrics(status) {
  const s = status || {}
  let grab = '—'
  let stage = '空闲'
  if (s.stageRunning) {
    stage = '运行中'
  } else if (s.acquisitionActive) {
    grab = '进行中'
  } else {
    grab = s.liveViewActive ? '预览' : '空闲'
  }
  return {
    metricCam: s.cameraOpen ? '已打开' : '未打开',
    metricGrab: grab,
    metricStage: stage,
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
  formatMetrics
}
