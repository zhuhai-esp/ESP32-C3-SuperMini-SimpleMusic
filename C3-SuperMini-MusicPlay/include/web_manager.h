#pragma once

// Web 管理模块：仅 AP 模式（不连路由器），提供：
//   - 音频文件上传 / 删除 / 列表（LittleFS）
//   - SPIFFS 分区使用情况展示与格式化
//   - 音量实时调节
//   - 开机自动播放开关（下次开机生效）
//   - AP 热点密码修改（存 NVS，修改后热点重启）
// 页面由 platformio.ini 的 board_build.embed_files 嵌入固件（src/html/index.html）。
// 连接热点后 DNS 强制门户会自动弹出管理页面。

#include "audio_player.h"
#include "settings.h"
#include <DNSServer.h>
#include <FS.h>
#include <WebServer.h>

class WebManager {
public:
  WebManager(MultiAudioPlayer* player, Settings* settings);

  void begin();        // 启动 AP / DNS / HTTP 服务（setup 中调用）
  void handleClient(); // 处理客户端请求（loop 中调用，分块处理不阻塞播放）

  // 重新应用 AP 设置（修改密码或重置设置后调用，热点随之重启）
  void reapplyAp();

  // 客户端接入/断开边缘事件（轮询检测，查询后清零，供 loop() 做播放策略）
  bool takeStationConnected();
  bool takeStationDisconnected();

  // 完整热点名称：前缀 + 芯片硬件 ID 后 4 位十六进制，如 C3-Music-3F2A
  String getApSsid() const;

private:
  void handleRoot();
  void handleStatus();
  void handleFiles();
  void handleUploadDone();
  void handleUploadData();
  void handleDelete();
  void handleVolumeGet();
  void handleVolumeSet();
  void handleAutoPlayGet();
  void handleAutoPlaySet();
  void handleApInfo();
  void handleApPwdSet();
  void handleFormat();
  void handleReboot();
  void sendJson(int code, const String& json);

  // 完整热点名称：前缀 + 芯片硬件 ID 后 4 位十六进制，如 C3-Music-3F2A
  String buildApSsid() const;

  WebServer _server{WEB_SERVER_PORT};
  DNSServer _dnsServer;
  MultiAudioPlayer* _player;
  Settings* _settings;

  // 上传状态（跨数据块回调保持）
  File _uploadFile;
  String _uploadPath;
  String _uploadError;

  // 客户端接入状态轮询（在 handleClient 中更新，take* 查询后清零）
  uint8_t _lastStationCount = 0;
  volatile bool _staConnected = false;
  volatile bool _staDisconnected = false;
};
