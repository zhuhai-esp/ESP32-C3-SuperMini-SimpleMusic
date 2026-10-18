#include "web_manager.h"

#include <LittleFS.h>
#include <WiFi.h>
#include "config.h"
#include "log.h"

// 管理页面：由 platformio.ini 的 board_build.embed_files 嵌入固件
// （objcopy 二进制嵌入，符号名由文件相对路径 src/html/index.html 转换而来）
extern const char index_html_start[] asm("_binary_src_html_index_html_start");
extern const char index_html_end[] asm("_binary_src_html_index_html_end");

namespace {

String jsonEscape(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

}  // namespace

WebManager::WebManager(MultiAudioPlayer* player, Settings* settings)
    : _player(player), _settings(settings) {}

// 完整热点名称：前缀 + 芯片硬件 ID（eFuse MAC 低 16 位）后 4 位十六进制
String WebManager::buildApSsid() const {
  uint64_t mac = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", (uint32_t)(mac & 0xFFFF));
  return String(AP_SSID_PREFIX) + "-" + suffix;
}

String WebManager::getApSsid() const {
  return buildApSsid();
}

void WebManager::begin() {
  // 仅 AP 模式：设备自身发射 WiFi，不连接路由器
  WiFi.mode(WIFI_AP);
  String ssid = buildApSsid();
  String password = _settings->loadApPassword();
  WiFi.softAP(ssid.c_str(), password.c_str());
  IPAddress ip = WiFi.softAPIP();

  // 强制门户：所有域名解析到本机 IP，连接热点后自动弹出管理页面
  _dnsServer.start(DNS_SERVER_PORT, "*", ip);

  // 未匹配的请求统一跳转到管理页面（配合 captive portal 检测）
  _server.onNotFound([this]() {
    _server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    _server.send(302, "text/plain", "");
  });

  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server.on("/api/files", HTTP_GET, [this]() { handleFiles(); });
  _server.on("/api/upload", HTTP_POST,
             [this]() { handleUploadDone(); },
             [this]() { handleUploadData(); });
  _server.on("/api/delete", HTTP_GET, [this]() { handleDelete(); });
  _server.on("/api/volume", HTTP_GET, [this]() { handleVolumeGet(); });
  _server.on("/api/volume", HTTP_POST, [this]() { handleVolumeSet(); });
  _server.on("/api/autoplay", HTTP_GET, [this]() { handleAutoPlayGet(); });
  _server.on("/api/autoplay", HTTP_POST, [this]() { handleAutoPlaySet(); });
  _server.on("/api/apinfo", HTTP_GET, [this]() { handleApInfo(); });
  _server.on("/api/appwd", HTTP_POST, [this]() { handleApPwdSet(); });
  _server.on("/api/format", HTTP_POST, [this]() { handleFormat(); });
  _server.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });

  _server.begin();
  LOG_I("AP 已启动: SSID=%s IP=%s", ssid.c_str(), ip.toString().c_str());
  LOG_I("Web 服务已启动: http://%s:%d", ip.toString().c_str(), WEB_SERVER_PORT);
}

void WebManager::handleClient() {
  // 轮询客户端接入数，产生 0<->1 边缘事件（在 loop 线程消费，避免竞态）
  uint8_t stations = WiFi.softAPgetStationNum();
  if (stations > 0 && _lastStationCount == 0) {
    _staConnected = true;
  } else if (stations == 0 && _lastStationCount > 0) {
    _staDisconnected = true;
  }
  _lastStationCount = stations;

  _dnsServer.processNextRequest();  // 强制门户 DNS
  _server.handleClient();
}

bool WebManager::takeStationConnected() {
  if (_staConnected) {
    _staConnected = false;
    return true;
  }
  return false;
}

bool WebManager::takeStationDisconnected() {
  if (_staDisconnected) {
    _staDisconnected = false;
    return true;
  }
  return false;
}

void WebManager::reapplyAp() {
  String ssid = buildApSsid();
  String password = _settings->loadApPassword();
  WiFi.softAP(ssid.c_str(), password.c_str());
  LOG_I("AP 已重启: SSID=%s", ssid.c_str());
}

void WebManager::handleRoot() {
  size_t len = (size_t)(index_html_end - index_html_start);
  _server.setContentLength(len);
  _server.send(200, "text/html", "");
  _server.sendContent(index_html_start, len);
}

void WebManager::handleStatus() {
  const char* state = "stopped";
  if (_player->isAudioPlaying()) {
    state = _player->isPaused() ? "paused" : "playing";
  }
  const char* track = _player->getCurrentTrackName();
  String trackName = track ? String(track) : String("");
  int slash = trackName.lastIndexOf('/');
  if (slash >= 0) {
    trackName = trackName.substring(slash + 1);  // 只显示文件名
  }

  uint32_t total = LittleFS.totalBytes();
  uint32_t used = LittleFS.usedBytes();

  String json = "{";
  json += "\"state\":\"" + String(state) + "\",";
  json += "\"track\":\"" + jsonEscape(trackName) + "\",";
  json += "\"index\":" + String(_player->getCurrentTrackIndex()) + ",";
  json += "\"total\":" + String(_player->getPlaylistSize()) + ",";
  json += "\"volume\":" + String(_player->getVolume(), 2) + ",";
  json += "\"autoPlay\":" + String(_settings->loadAutoPlay() ? "true" : "false") + ",";
  json += "\"totalKB\":" + String(total / 1024) + ",";
  json += "\"usedKB\":" + String(used / 1024) + ",";
  json += "\"freeKB\":" + String((total - used) / 1024) + ",";
  json += "\"apSSID\":\"" + buildApSsid() + "\",";
  json += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"clients\":" + String(WiFi.softAPgetStationNum());
  json += "}";
  sendJson(200, json);
}

void WebManager::handleFiles() {
  uint32_t total = LittleFS.totalBytes();
  uint32_t used = LittleFS.usedBytes();

  String json = "{\"files\":[";
  int count = 0;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    if (!file.isDirectory() && name.endsWith(AUDIO_EXTENSION)) {
      if (count > 0) {
        json += ",";
      }
      count++;
      json += "{\"name\":\"" + jsonEscape(name) + "\",\"sizeKB\":" + String(file.size() / 1024.0, 1) + "}";
    }
    file = root.openNextFile();
  }
  root.close();

  json += "],\"count\":" + String(count);
  json += ",\"totalKB\":" + String(total / 1024);
  json += ",\"usedKB\":" + String(used / 1024);
  json += ",\"freeKB\":" + String((total - used) / 1024);
  json += "}";
  sendJson(200, json);
}

void WebManager::handleUploadData() {
  HTTPUpload& upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    _uploadError = "";
    _uploadPath = "";
    String filename = upload.filename;
    int slash = filename.lastIndexOf('/');
    if (slash >= 0) {
      filename = filename.substring(slash + 1);  // 只取 basename，防目录穿越
    }
    filename.toLowerCase();
    if (!filename.endsWith(AUDIO_EXTENSION) || filename.length() <= (int)strlen(AUDIO_EXTENSION)) {
      _uploadError = "仅支持 .aac 文件";
      return;
    }
    if (upload.totalSize > 0 &&
        (LittleFS.totalBytes() - LittleFS.usedBytes()) < upload.totalSize) {
      _uploadError = "存储空间不足";
      return;
    }
    _uploadPath = "/" + filename;
    _uploadFile = LittleFS.open(_uploadPath, FILE_WRITE);
    if (!_uploadFile) {
      _uploadError = "无法创建文件";
      return;
    }
    LOG_I("开始上传: %s (%u 字节)", _uploadPath.c_str(), upload.totalSize);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile) {
      _uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile) {
      _uploadFile.close();
      LOG_I("上传完成: %s", _uploadPath.c_str());
      _player->createPlaylist();  // 刷新播放列表
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (_uploadFile) {
      _uploadFile.close();
    }
    if (_uploadPath.length() > 0) {
      LittleFS.remove(_uploadPath);  // 清理不完整文件
    }
    LOG_E("上传中断: %s", _uploadPath.c_str());
    _uploadPath = "";
  }
}

void WebManager::handleUploadDone() {
  if (_uploadError.length() > 0) {
    sendJson(400, "{\"ok\":false,\"msg\":\"" + _uploadError + "\"}");
    return;
  }
  if (_uploadPath.length() == 0) {
    sendJson(400, "{\"ok\":false,\"msg\":\"上传失败\"}");
    return;
  }
  sendJson(200, "{\"ok\":true,\"msg\":\"上传成功: " + jsonEscape(_uploadPath.substring(1)) + "\"}");
  _uploadPath = "";
}

void WebManager::handleDelete() {
  if (!_server.hasArg("name")) {
    sendJson(400, "{\"ok\":false,\"msg\":\"缺少文件名\"}");
    return;
  }
  String name = _server.arg("name");
  int slash = name.lastIndexOf('/');
  if (slash >= 0) {
    name = name.substring(slash + 1);  // 只取 basename，防目录穿越
  }
  if (name.length() == 0 || name.indexOf("..") >= 0) {
    sendJson(400, "{\"ok\":false,\"msg\":\"非法文件名\"}");
    return;
  }
  String path = "/" + name;

  // 正在播放被删文件时先停止，避免读取已删除文件
  const char* current = _player->getCurrentTrackName();
  if (current && String(current) == path) {
    _player->stopAudio();
  }

  if (LittleFS.remove(path)) {
    _player->createPlaylist();  // 刷新播放列表
    LOG_I("已删除文件: %s", path.c_str());
    sendJson(200, "{\"ok\":true,\"msg\":\"已删除 " + jsonEscape(name) + "\"}");
  } else {
    sendJson(500, "{\"ok\":false,\"msg\":\"删除失败（文件不存在?）\"}");
  }
}

void WebManager::handleVolumeGet() {
  sendJson(200, "{\"volume\":" + String(_player->getVolume(), 2) + "}");
}

void WebManager::handleVolumeSet() {
  if (!_server.hasArg("volume")) {
    sendJson(400, "{\"ok\":false,\"msg\":\"缺少 volume 参数\"}");
    return;
  }
  float volume = _server.arg("volume").toFloat();
  _player->setVolume(volume);      // 内部会钳制 0~1 并实时生效
  _settings->saveVolume(volume);   // 掉电记忆
  sendJson(200, "{\"ok\":true,\"volume\":" + String(_player->getVolume(), 2) + "}");
}

void WebManager::handleAutoPlayGet() {
  sendJson(200, "{\"autoPlay\":" + String(_settings->loadAutoPlay() ? "true" : "false") + "}");
}

void WebManager::handleAutoPlaySet() {
  if (!_server.hasArg("enabled")) {
    sendJson(400, "{\"ok\":false,\"msg\":\"缺少 enabled 参数\"}");
    return;
  }
  bool enabled = _server.arg("enabled") == "1" || _server.arg("enabled") == "true";
  _settings->saveAutoPlay(enabled);  // 下次开机生效
  LOG_I("开机自动播放: %s", enabled ? "开" : "关");
  sendJson(200, "{\"ok\":true,\"autoPlay\":" + String(enabled ? "true" : "false") + "}");
}

void WebManager::handleApInfo() {
  String json = "{";
  json += "\"ssid\":\"" + buildApSsid() + "\",";
  json += "\"password\":\"" + jsonEscape(_settings->loadApPassword()) + "\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
  json += "}";
  sendJson(200, json);
}

void WebManager::handleApPwdSet() {
  if (!_server.hasArg("pwd")) {
    sendJson(400, "{\"ok\":false,\"msg\":\"缺少 pwd 参数\"}");
    return;
  }
  String pwd = _server.arg("pwd");
  if (pwd.length() < 8 || pwd.length() > 63) {
    sendJson(400, "{\"ok\":false,\"msg\":\"密码长度需为 8-63 位\"}");
    return;
  }
  _settings->saveApPassword(pwd);
  reapplyAp();  // 热点以新密码重启（已连接的设备会断开）
  LOG_I("AP 密码已修改");
  sendJson(200, "{\"ok\":true,\"msg\":\"密码已保存，热点正在重启，请重新连接\"}");
}

void WebManager::handleFormat() {
  LOG_I("开始格式化 SPIFFS 分区...");
  // 先停止播放，释放文件句柄后再清空分区
  _player->stopAudio();
  if (LittleFS.format()) {
    _player->createPlaylist();  // 播放列表已清空
    LOG_I("格式化完成，文件已清空");
    sendJson(200, "{\"ok\":true,\"msg\":\"格式化完成，文件已清空\"}");
  } else {
    LOG_E("格式化失败");
    sendJson(500, "{\"ok\":false,\"msg\":\"格式化失败\"}");
  }
}

void WebManager::handleReboot() {
  LOG_I("收到重启命令，设备即将重启");
  sendJson(200, "{\"ok\":true,\"msg\":\"设备正在重启...\"}");
  delay(300);  // 等待响应发送完成
  ESP.restart();
}

void WebManager::sendJson(int code, const String& json) {
  _server.send(code, "application/json", json);
}
