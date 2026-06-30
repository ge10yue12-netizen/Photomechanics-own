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

function defaultBtnState(disabled) {
  const s = {}
  CMD_KEYS.forEach((k) => { s[k] = !!disabled })
  return s
}

/** PC 主动离线（如关程序前 Notify ok=0）。 */
function isRemoteOffline(status) {
  if (!status || typeof status !== 'object') return false
  return status.ok === false || status.ok === 0
}

/** 与 PC 主界面一致的命令按钮禁用规则。 */
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

module.exports = {
  CMD_LABELS,
  CMD_KEYS,
  POLL_MS,
  defaultBtnState,
  computeBtnState,
  isRemoteOffline
}
