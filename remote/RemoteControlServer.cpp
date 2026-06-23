#include "RemoteControlServer.h"
#include "NetConfigHelper.h"
#include "RemoteCommandList.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QTcpSocket>
#include <QUrlQuery>
#include <utility>

namespace
{
const char *kRequestDataProperty = "remoteRequestData";

QString requestPath(const QString &target)
{
    const int queryPos = target.indexOf(QLatin1Char('?'));
    return queryPos >= 0 ? target.left(queryPos) : target;
}

QString requestQuery(const QString &target)
{
    const int queryPos = target.indexOf(QLatin1Char('?'));
    return queryPos >= 0 ? target.mid(queryPos + 1) : QString();
}

}

RemoteControlServer::RemoteControlServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &RemoteControlServer::onNewConnection);
}

bool RemoteControlServer::start(quint16 port, const QString &token, const QHostAddress &bindAddress)
{
    if (m_server.isListening())
        stop();
    m_token = token.trimmed();
    m_lastError.clear();
    if (NetConfigHelper::tryListen(m_server, bindAddress, port, &m_lastError))
        return true;
    if (m_lastError.isEmpty())
        m_lastError = m_server.errorString();
    return false;
}

void RemoteControlServer::stop()
{
    m_server.close();
}

bool RemoteControlServer::isListening() const
{
    return m_server.isListening();
}

quint16 RemoteControlServer::serverPort() const
{
    return m_server.serverPort();
}

void RemoteControlServer::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_statusProvider = std::move(provider);
}

void RemoteControlServer::onNewConnection()
{
    while (auto *socket = m_server.nextPendingConnection())
    {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onSocketReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void RemoteControlServer::onSocketReadyRead(QTcpSocket *socket)
{
    QByteArray request = socket->property(kRequestDataProperty).toByteArray();
    request += socket->readAll();
    socket->setProperty(kRequestDataProperty, request);

    const int headerEnd = request.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    int contentLength = 0;
    const QList<QByteArray> headerLines = request.left(headerEnd).split('\n');
    for (QByteArray line : headerLines)
    {
        line = line.trimmed();
        if (line.toLower().startsWith("content-length:"))
            contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
    }

    if (request.size() < headerEnd + 4 + contentLength)
        return;

    handleRequest(socket, request.left(headerEnd + 4 + contentLength));
}

void RemoteControlServer::handleRequest(QTcpSocket *socket, const QByteArray &request)
{
    const int headerEnd = request.indexOf("\r\n\r\n");
    const QByteArray header = request.left(headerEnd);
    const QByteArray bodyBytes = request.mid(headerEnd + 4);
    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty())
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("\u8bf7\u6c42\u683c\u5f0f\u9519\u8bef")}}, 400, QByteArrayLiteral("Bad Request"));
        return;
    }

    const QList<QByteArray> firstLine = lines.first().trimmed().split(' ');
    if (firstLine.size() < 2)
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("\u8bf7\u6c42\u884c\u9519\u8bef")}}, 400, QByteArrayLiteral("Bad Request"));
        return;
    }

    const QString method = QString::fromLatin1(firstLine.at(0)).toUpper();
    const QString target = QString::fromUtf8(firstLine.at(1));
    const QString path = requestPath(target);
    const QString query = requestQuery(target);

    QJsonObject body;
    if (!bodyBytes.trimmed().isEmpty())
    {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(bodyBytes, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
        {
            writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("JSON \u683c\u5f0f\u9519\u8bef")}}, 400, QByteArrayLiteral("Bad Request"));
            return;
        }
        body = doc.object();
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/"))
    {
        writeHtml(socket, htmlPage());
        return;
    }

    if (path == QStringLiteral("/favicon.ico"))
    {
        writeResponse(socket, 204, QByteArrayLiteral("No Content"), QByteArrayLiteral("text/plain"), QByteArray());
        return;
    }

    if (!hasValidToken(query, body))
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("\u9065\u63a7 token \u65e0\u6548")}}, 403, QByteArrayLiteral("Forbidden"));
        return;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/status"))
    {
        writeJson(socket, currentStatus());
        return;
    }

    if (method == QStringLiteral("POST") && path == QStringLiteral("/api/command"))
    {
        const QString cmd = body.value(QStringLiteral("cmd")).toString().trimmed();
        if (!isKnownRemoteCommand(cmd))
        {
            writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("\u672a\u77e5\u547d\u4ee4")}}, 400, QByteArrayLiteral("Bad Request"));
            return;
        }

        emit commandReceived(cmd);
        QJsonObject response = currentStatus();
        response.insert(QStringLiteral("ok"), true);
        response.insert(QStringLiteral("cmd"), cmd);
        response.insert(QStringLiteral("message"), QStringLiteral("\u547d\u4ee4\u5df2\u63a5\u6536"));
        writeJson(socket, response);
        return;
    }

    writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("\u63a5\u53e3\u4e0d\u5b58\u5728")}}, 404, QByteArrayLiteral("Not Found"));
}

void RemoteControlServer::writeHtml(QTcpSocket *socket, const QString &html) const
{
    writeResponse(socket, 200, QByteArrayLiteral("OK"), QByteArrayLiteral("text/html; charset=utf-8"), html.toUtf8());
}

void RemoteControlServer::writeJson(QTcpSocket *socket, const QJsonObject &obj, int statusCode, const QByteArray &statusText) const
{
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    writeResponse(socket, statusCode, statusText, QByteArrayLiteral("application/json; charset=utf-8"), body);
}

void RemoteControlServer::writeResponse(QTcpSocket *socket,
                                        int statusCode,
                                        const QByteArray &statusText,
                                        const QByteArray &contentType,
                                        const QByteArray &body) const
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "Cache-Control: no-store\r\n";
    response += "\r\n";
    response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

bool RemoteControlServer::hasValidToken(const QString &query, const QJsonObject &body) const
{
    if (m_token.isEmpty())
        return true;
    const QUrlQuery urlQuery(query);
    const QString queryToken = urlQuery.queryItemValue(QStringLiteral("token"));
    const QString bodyToken = body.value(QStringLiteral("token")).toString();
    return queryToken == m_token || bodyToken == m_token;
}

QJsonObject RemoteControlServer::currentStatus() const
{
    if (m_statusProvider)
        return m_statusProvider();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("message"), QStringLiteral("鐘舵€佹湭鎺ュ叆")}};
}

QString RemoteControlServer::htmlPage() const
{
    return QStringLiteral(R"HTML(<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>鐩告満杩滅▼鎺у埗</title>
<style>
:root{--blue:#0078d4;--red:#c42b1c;--green:#107c10;--gray:#666;--bg:#f4f6f8;--card:#fff;--line:#e5e7eb;--text:#1f2933}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Arial,"Microsoft YaHei",sans-serif}
main{max-width:560px;margin:0 auto;padding:16px 14px 28px}.top{display:flex;justify-content:space-between;gap:12px;align-items:center;margin:4px 0 14px}
h1{font-size:22px;margin:0}.badge{padding:5px 10px;border-radius:999px;background:#e5e7eb;color:#333;font-size:13px;white-space:nowrap}.badge.ok{background:#dff6dd;color:#0b6a0b}.badge.warn{background:#fde7e9;color:#a4262c}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin:12px 0;box-shadow:0 1px 5px rgba(0,0,0,.06)}
.card h2{font-size:17px;margin:0 0 10px}.hint{font-size:13px;color:#64748b;line-height:1.5;margin:6px 0 0}.row{display:flex;gap:10px;align-items:center}.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}.grid3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
button{border:0;border-radius:11px;padding:13px 10px;font-size:16px;color:#fff;background:var(--blue);min-height:46px}button.danger{background:var(--red)}button.secondary{background:var(--gray)}button:disabled{background:#cbd5e1;color:#f8fafc}.full{grid-column:1/-1}
input{width:100%;font-size:16px;border:1px solid #cbd5e1;border-radius:10px;padding:10px}.statusGrid{display:grid;grid-template-columns:1fr 1fr;gap:8px}.kv{background:#f8fafc;border:1px solid #e2e8f0;border-radius:10px;padding:10px}.kv b{display:block;font-size:12px;color:#64748b;margin-bottom:4px}.kv span{font-size:16px}
.progress{height:9px;background:#e2e8f0;border-radius:999px;overflow:hidden;margin-top:8px}.bar{height:100%;width:0;background:var(--green);transition:width .2s}.bar.warn{background:var(--red)}
.log{white-space:pre-wrap;word-break:break-word;background:#111827;color:#d1fae5;border-radius:10px;padding:10px;min-height:84px;font-family:Consolas,monospace;font-size:13px}.small{font-size:12px;color:#64748b}.step{font-size:13px;color:#475569;margin-bottom:8px}
</style>
</head>
<body>
<main>
<div class="top"><h1>鐩告満杩滅▼鎺у埗</h1><span id="conn" class="badge warn">鏈繛鎺?/span></div>

<div class="card">
  <h2>1. 杩炴帴鐩告満</h2>
  <div class="step">鍏堟墦寮€鐩告満锛屾墦寮€鍚庤蒋浠朵細鑷姩淇濇寔棰勮銆?/div>
  <div class="grid">
    <button id="open_camera" onclick="cmd('open_camera')">鎵撳紑鐩告満</button>
    <button id="close_camera" class="danger" onclick="cmd('close_camera')">鍏抽棴鐩告満</button>
  </div>
</div>
<div class="card">
  <h2>2. 鎵嬪姩閲囬泦涓庡崟寮犲瓨鍥?/h2>
  <div class="step">娴佺▼涓庣數鑴戠涓€鑷达細鎵撳紑鐩告満 鈫?寮€濮嬮噰闆?鈫?淇濆瓨鍗曞紶 鈫?鍋滄閲囬泦銆?/div>
  <div class="grid">
    <button id="start_capture" onclick="cmd('start_capture')">寮€濮嬮噰闆?/button>
    <button id="stop_capture" class="danger" onclick="cmd('stop_capture')">鍋滄閲囬泦</button>
    <button id="save_one" class="full" onclick="cmd('save_one')">淇濆瓨鍗曞紶 BMP</button>
  </div>
</div>
<div class="card">
  <h2>3. 杩炵画/闃舵閲囬泦</h2>
  <div class="step">闃舵琛ㄣ€佸抚鐜囥€佷繚瀛樿矾寰勪粛鍦ㄧ數鑴戠璁剧疆锛涙墜鏈哄彧璐熻矗鍚姩/鍋滄銆?/div>
  <div class="grid">
    <button id="start_stage" onclick="cmd('start_stage')">寮€濮嬮樁娈甸噰闆?/button>
    <button id="stop_stage" class="danger" onclick="cmd('stop_capture')">鍋滄闃舵閲囬泦</button>
  </div>
</div>
<div class="card">
  <h2>鐘舵€佺洃鎺?/h2>
  <div class="statusGrid">
    <div class="kv"><b>鐩告満</b><span id="cameraStatus">-</span></div>
    <div class="kv"><b>宸ヤ綔娴?/b><span id="flowStatus">-</span></div>
    <div class="kv"><b>闃舵</b><span id="stageStatus">-</span></div>
    <div class="kv"><b>鎬讳繚瀛?/b><span id="totalSaved">0</span></div>
  </div>
  <div class="kv" style="margin-top:8px"><b>瀛樺浘闃熷垪</b><span id="queueText">0/0</span><div class="progress"><div id="queueBar" class="bar"></div></div></div>
</div>
<div class="card">
  <h2>鎿嶄綔鏃ュ織</h2>
  <div id="out" class="log">绛夊緟鎿嶄綔...</div>
  <p class="small">鐘舵€佹瘡 1 绉掕嚜鍔ㄥ埛鏂帮紱鎿嶄綔鏃ュ織鍙繚鐣欐渶杩?5 鏉°€?/p>
</div>
</main>
<script>
const ids=['open_camera','close_camera','start_capture','stop_capture','save_one','start_stage','stop_stage'];
const commandNames={open_camera:'鎵撳紑鐩告満',close_camera:'鍏抽棴鐩告満',start_capture:'寮€濮嬮噰闆?,stop_capture:'鍋滄閲囬泦',save_one:'淇濆瓨鍗曞紶',start_stage:'寮€濮嬮樁娈甸噰闆?};
const logs=[];
const urlToken=new URLSearchParams(location.search).get('token');
let storedToken=urlToken||localStorage.getItem('remoteToken')||'1234';
if(urlToken)localStorage.setItem('remoteToken',urlToken);
function token(){return encodeURIComponent(storedToken.trim());}
function now(){return new Date().toLocaleTimeString('zh-CN',{hour:'2-digit',minute:'2-digit'});}
function addLog(text){logs.unshift(now()+' '+text);while(logs.length>5)logs.pop();document.getElementById('out').textContent=logs.join('\\n');}
function setConn(ok){const el=document.getElementById('conn');el.textContent=ok?'宸茶繛鎺?:'鏈繛鎺?;el.className='badge '+(ok?'ok':'warn');}
function setDisabled(id,value){const el=document.getElementById(id);if(el)el.disabled=value;}
function render(s){
  setConn(true);
  document.getElementById('cameraStatus').textContent=s.cameraStatus || (s.cameraOpen?'宸叉墦寮€':'鏈繛鎺?);
  document.getElementById('flowStatus').textContent=s.message || '-';
  document.getElementById('stageStatus').textContent=s.stageStatus || '-';
  document.getElementById('totalSaved').textContent=s.totalSaved ?? 0;
  const q=s.queueSize||0, cap=s.queueCapacity||0, pct=cap?Math.min(100,Math.round(q*100/cap)):0;
  document.getElementById('queueText').textContent=q+'/'+cap;
  const bar=document.getElementById('queueBar');bar.style.width=pct+'%';bar.className='bar '+(pct>=80?'warn':'');
  const open=!!s.cameraOpen, acq=!!s.acquisitionActive, stage=!!s.stageRunning, live=!!s.liveViewActive;
  setDisabled('open_camera',open);
  setDisabled('close_camera',!open || stage);
  setDisabled('start_capture',!open || acq || stage);
  setDisabled('stop_capture',!open || !acq || stage);
  setDisabled('save_one',!open || !acq || !live || stage);
  setDisabled('start_stage',!open || acq || stage);
  setDisabled('stop_stage',!stage);
}
async function status(silent=false){
  try{
    const r=await fetch('/api/status?token='+token(),{cache:'no-store'});
    const data=await r.json();
    render(data);
    if(!silent) addLog('鐘舵€佸埛鏂版垚鍔燂紝褰撳墠锛?+(data.message||'鏈煡'));
    return data;
  }catch(e){
    setConn(false);
    ids.forEach(id=>setDisabled(id,true));
    addLog('杩炴帴澶辫触锛岃妫€鏌ョ數鑴戣蒋浠躲€佺儹鐐瑰拰闃茬伀澧?);
  }
}
async function cmd(name){
  ids.forEach(id=>setDisabled(id,true));
  const label=commandNames[name]||name;
  try{
    const r=await fetch('/api/command?token='+token(),{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:name,token:decodeURIComponent(token())})});
    const data=await r.json();
    render(data);
    addLog(r.ok ? ('杩滅▼鍛戒护锛?+label+'锛屽凡鍙戦€?) : ('杩滅▼鍛戒护锛?+label+'锛屽け璐ワ細'+(data.message||'璇锋眰琚嫆缁?)));
    setTimeout(()=>status(true),350);
  }catch(e){
    setConn(false);
    addLog('杩滅▼鍛戒护锛?+label+'锛屽彂閫佸け璐ワ紝璇锋鏌ョ綉缁滆繛鎺?);
  }finally{
    setTimeout(()=>status(true),800);
  }
}
status(true).then(()=>addLog('椤甸潰宸茶繛鎺ワ紝绛夊緟鎿嶄綔'));
setInterval(()=>status(true),1000);
</script>
</body>
</html>)HTML");
}
