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
      return '主机不可达：须确认远程控制服务已启动，客户端与主机处于同一局域网'
    case 'lost':
      return '与主机连接中断：须检查局域网或主机服务状态'
    case 'command':
      return '远程控制命令发送失败'
    case 'scan':
      return 'BLE 扫描失败：须启用客户端蓝牙，Android 须授予定位权限'
    case 'ble':
      return 'BLE 连接失败：须确认主机 BLE 远程控制服务已启动'
    default:
      return '操作失败'
  }
}

function humanizeHttpStatus(statusCode, bodyMsg) {
  const body = (bodyMsg || '').trim()
  if (body && /[\u4e00-\u9fff]/.test(body)) return body
  const code = Number(statusCode) || 0
  if (code === 409) {
    return body || '命令通道被占用，须先释放其他客户端连接'
  }
  if (code === 401 || code === 403) {
    return '认证 token 无效'
  }
  if (code === 404) {
    return '远程控制服务未就绪'
  }
  if (code >= 500) return '主机服务异常，须查看主机日志'
  if (code === 0) return '主机不可达：须检查地址、端口及防火墙'
  if (code > 0) return '连接异常：HTTP ' + code
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
    return '域名校验失败：开发者工具须关闭合法域名校验后重试'
  }
  if (lower.indexOf('timeout') >= 0 || msg.indexOf('超时') >= 0) {
    return '连接超时：须确认主机服务已启动，地址与端口配置正确'
  }
  if (lower.indexOf('connect fail') >= 0 || lower.indexOf('connection refused') >= 0 || lower.indexOf('econnrefused') >= 0) {
    return '主机拒绝连接：须检查地址、端口及防火墙策略'
  }
  if (lower.indexOf('network') >= 0 || lower.indexOf('net::') >= 0 || msg.indexOf('网络请求失败') >= 0) {
    return '网络不可达：须确认客户端与主机处于同一局域网'
  }
  if (msg.indexOf('token') >= 0 && (msg.indexOf('无效') >= 0 || msg.indexOf('403') >= 0 || msg.indexOf('401') >= 0)) {
    return '认证 token 无效'
  }
  if (msg.indexOf('占用') >= 0 || msg.indexOf('remoteControlBlocked') >= 0) {
    return msg.indexOf('占用') >= 0 ? msg : '命令通道被占用，须先释放其他客户端连接'
  }
  if (/HTTP\s*409/i.test(msg)) {
    return '命令通道被占用，须先释放 Web 或其他客户端连接'
  }
  if (/HTTP\s*403/i.test(msg) || /HTTP\s*401/i.test(msg)) {
    return '认证 token 无效'
  }
  if (/HTTP\s*404/i.test(msg) || msg.indexOf('接口不存在') >= 0) {
    return '远程控制服务未就绪'
  }
  if (/HTTP\s*5\d\d/i.test(msg)) {
    return '主机服务异常，须查看主机日志'
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
