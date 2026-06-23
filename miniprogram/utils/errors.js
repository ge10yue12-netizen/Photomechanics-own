function rawMessage(err) {
  if (!err) return ''
  if (typeof err === 'string') return err
  if (err.message) return err.message
  if (err.errMsg) return err.errMsg
  return String(err)
}

function defaultHint(scene) {
  switch (scene) {
    case 'connect':
      return '无法连接 PC：请确认软件已启动，手机与 PC 同一 WiFi'
    case 'lost':
      return '与 PC 失去连接：请检查 WiFi 或 PC 是否仍在运行'
    case 'command':
      return '遥控命令发送失败，请稍后重试'
    case 'scan':
      return '蓝牙搜索失败：请打开手机蓝牙，Android 需开启定位权限'
    case 'ble':
      return '蓝牙连接失败：请靠近 PC，并确认 PC 日志显示「BLE 遥控已启动」'
    default:
      return '操作失败，请稍后重试'
  }
}

function humanizeHttpStatus(statusCode, bodyMsg) {
  const body = (bodyMsg || '').trim()
  if (body && /[\u4e00-\u9fff]/.test(body)) return body
  const code = Number(statusCode) || 0
  if (code === 401 || code === 403) {
    return '口令错误：请与 PC 端配置保持一致'
  }
  if (code === 404) {
    return 'PC 端未找到遥控服务：请确认程序遥控已启动'
  }
  if (code >= 500) return 'PC 端服务异常，请查看 PC 软件日志后重试'
  if (code === 0) return '无法连接 PC：请检查 IP、端口及防火墙'
  if (code > 0) return '连接失败：服务器返回异常（' + code + '）'
  return defaultHint('connect')
}

function humanizeError(err, scene) {
  let msg = rawMessage(err).trim()
  msg = msg.replace(/^Error:\s*/i, '')
  const lower = msg.toLowerCase()

  if (!msg || lower === 'error' || lower === 'fail' || lower === 'failed') {
    return defaultHint(scene || 'connect')
  }

  if (msg.indexOf('domain list') >= 0 || lower.indexOf('url not in domain') >= 0) {
    return '无法访问该地址：请在开发者工具勾选「不校验合法域名、web-view、TLS 版本」后重新预览'
  }
  if (lower.indexOf('timeout') >= 0 || msg.indexOf('超时') >= 0) {
    return '连接超时：请确认 PC 软件已启动，手机与 PC 在同一 WiFi，IP 与端口填写正确'
  }
  if (lower.indexOf('connect fail') >= 0 || lower.indexOf('connection refused') >= 0 || lower.indexOf('econnrefused') >= 0) {
    return '无法连接到 PC：请检查 IP 与端口，并确认 PC 防火墙已放行该端口'
  }
  if (lower.indexOf('network') >= 0 || lower.indexOf('net::') >= 0 || msg.indexOf('网络请求失败') >= 0) {
    return '网络不通：请确认手机与 PC 在同一 WiFi，IP 与端口填写正确'
  }
  if (msg.indexOf('token') >= 0 && (msg.indexOf('无效') >= 0 || msg.indexOf('403') >= 0 || msg.indexOf('401') >= 0)) {
    return '口令错误：请与 PC 端 token 保持一致'
  }
  if (/HTTP\s*403/i.test(msg) || /HTTP\s*401/i.test(msg)) {
    return '口令错误：请与 PC 端 token 保持一致'
  }
  if (/HTTP\s*404/i.test(msg) || msg.indexOf('接口不存在') >= 0) {
    return 'PC 端未找到遥控服务：请确认 QtProject_1 已启动且日志有「HTTP 遥控已启动」'
  }
  if (/HTTP\s*5\d\d/i.test(msg)) {
    return 'PC 端服务异常，请查看 PC 软件日志后重试'
  }
  if (/^HTTP\s*\d+/i.test(msg)) {
    return humanizeHttpStatus(parseInt(msg.replace(/\D/g, ''), 10) || 0, '')
  }
  if (msg.indexOf('request:fail') >= 0) {
    return defaultHint(scene || 'connect')
  }
  if (/[\u4e00-\u9fff]/.test(msg)) return msg
  return defaultHint(scene || 'connect')
}

module.exports = { humanizeError, humanizeHttpStatus, defaultHint }
