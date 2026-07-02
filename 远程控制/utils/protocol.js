const SERVICE_UUID = 'A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D'
const CMD_UUID = 'A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5E'
const STATUS_UUID = 'A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5F'

function normalizeUuid(uuid) {
  return (uuid || '').replace(/-/g, '').toUpperCase()
}

function bytesToUuid128(bytes) {
  const b = bytes
  const hex = (n) => n.toString(16).padStart(2, '0').toUpperCase()
  return [
    hex(b[3]) + hex(b[2]) + hex(b[1]) + hex(b[0]),
    hex(b[5]) + hex(b[4]),
    hex(b[7]) + hex(b[6]),
    hex(b[8]) + hex(b[9]),
    hex(b[10]) + hex(b[11]) + hex(b[12]) + hex(b[13]) + hex(b[14]) + hex(b[15])
  ].join('-')
}

function parseAdvertisData(buffer) {
  if (!buffer) return []
  const data = new Uint8Array(buffer)
  const uuids = []
  let i = 0
  while (i < data.length) {
    const len = data[i]
    if (!len) break
    const type = data[i + 1]
    const start = i + 2
    const end = i + 1 + len
    if (end > data.length) break
    const payload = data.slice(start, end)
    if (type === 0x06 || type === 0x07) {
      for (let j = 0; j + 16 <= payload.length; j += 16) {
        uuids.push(bytesToUuid128(payload.slice(j, j + 16)))
      }
    } else if (type === 0x16 && payload.length >= 2) {
      if (payload.length >= 18) {
        uuids.push(bytesToUuid128(payload.slice(2, 18)))
      }
    }
    i = end
  }
  return uuids
}

function buildCommand(cmd, token) {
  if (token) {
    return `${cmd}:${token}`
  }
  return cmd
}

/** BLE 紧凑键 → WiFi/按钮规则用的完整键（与 PC compactStatusJson 对齐）。 */
function normalizeStatus(raw) {
  if (!raw || typeof raw !== 'object') return raw
  const s = Object.assign({}, raw)
  if (s.cameraOpen === undefined && s.cam !== undefined) s.cameraOpen = !!s.cam
  if (s.liveViewActive === undefined && s.lv !== undefined) s.liveViewActive = !!s.lv
  if (s.acquisitionActive === undefined && s.grab !== undefined) s.acquisitionActive = !!s.grab
  if (s.calculateActive === undefined && s.calc !== undefined) s.calculateActive = !!s.calc
  if (s.remoteEnabled === undefined && s.ren !== undefined) s.remoteEnabled = !!s.ren
  if (s.stageRunning === undefined && s.stg !== undefined) s.stageRunning = !!s.stg
  if (s.message === undefined && s.msg !== undefined) s.message = s.msg
  if (s.queueSize === undefined && s.q !== undefined) s.queueSize = s.q
  if (s.queueCapacity === undefined && s.qc !== undefined) s.queueCapacity = s.qc
  if (s.totalSaved === undefined && s.tot !== undefined) s.totalSaved = s.tot
  return s
}

function parseStatus(buffer) {
  const text = ab2str(buffer)
  try {
    return normalizeStatus(JSON.parse(text))
  } catch (e) {
    return { raw: text }
  }
}

function ab2str(buffer) {
  const data = new Uint8Array(buffer)
  let text = ''
  for (let i = 0; i < data.length; i++) {
    text += String.fromCharCode(data[i])
  }
  try {
    return decodeURIComponent(escape(text))
  } catch (e) {
    return text
  }
}

function str2ab(text) {
  const buf = new ArrayBuffer(text.length)
  const view = new Uint8Array(buf)
  for (let i = 0; i < text.length; i++) {
    view[i] = text.charCodeAt(i)
  }
  return buf
}

module.exports = {
  SERVICE_UUID,
  CMD_UUID,
  STATUS_UUID,
  normalizeUuid,
  parseAdvertisData,
  buildCommand,
  normalizeStatus,
  parseStatus,
  str2ab
}
