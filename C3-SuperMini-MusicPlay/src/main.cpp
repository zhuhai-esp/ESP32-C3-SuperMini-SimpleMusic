#include <Arduino.h>

#include "audio_player.h"
#include "button_manager.h"
#include "config.h"
#include "led_manager.h"
#include "log.h"
#include "prompt_tone.h"
#include "settings.h"
#include "web_manager.h"

// ========== 全局对象 ==========
// 各功能模块的实现见对应源文件，本文件只负责装配与调度
MultiAudioPlayer audioPlayer;
LEDManager ledManager(PIN_LED);
ButtonManager buttonManager(PIN_BUTTON, /*activeLow=*/true);
Settings settings;
WebManager webManager(&audioPlayer, &settings);

// 开机自动播放开关当前值（Web 修改后下次开机生效）
bool gAutoPlay = true;

// 是否因客户端接入热点而被自动暂停（断开后据此恢复，用户手动操作会清除）
bool gPausedForClient = false;

// 客户端接入/断开时的播放策略：ESP32-C3 单核难以同时兼顾播放与 Web 服务，
// 有客户端接入时自动暂停，全部断开后恢复。
void handleClientPresence() {
  if (webManager.takeStationConnected()) {
    if (audioPlayer.isAudioPlaying() && !audioPlayer.isPaused()) {
      audioPlayer.togglePlayPause();
      gPausedForClient = true;
    }
    LOG_I("客户端接入热点，播放已暂停");
  }
  if (webManager.takeStationDisconnected()) {
    if (gPausedForClient) {
      audioPlayer.togglePlayPause();  // 恢复播放
      gPausedForClient = false;
      LOG_I("客户端已断开，恢复播放");
    }
  }
}

// LED 指示 + 播放异常时的自动重试
void updateLedState() {
  // 无音频文件：快闪（等待通过 Web 上传文件）
  if (audioPlayer.getPlaylistSize() == 0) {
    ledManager.setMode(LedMode::FAST_BLINK);
    return;
  }

  if (audioPlayer.isAudioPlaying()) {
    // 暂停熄灭，正常播放慢闪
    ledManager.setMode(audioPlayer.isPaused() ? LedMode::OFF : LedMode::SLOW_BLINK);
    return;
  }

  if (gAutoPlay) {
    // 有文件但未在播放（异常停止）：快闪并定期重试
    ledManager.setMode(LedMode::FAST_BLINK);
    static unsigned long lastRetryTime = 0;
    if (millis() - lastRetryTime > PLAYLIST_RETRY_INTERVAL) {
      LOG_I("尝试重新开始播放列表...");
      audioPlayer.startPlaylist();
      lastRetryTime = millis();
    }
  } else {
    // 自动播放关闭：待机
    ledManager.setMode(LedMode::OFF);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  LOG_I("Hello ESP32C3!!");

  ledManager.begin();
  buttonManager.attachClick([] {
    audioPlayer.togglePlayPause();
    gPausedForClient = false;  // 用户手动操作，接管播放状态
  });
  buttonManager.attachLongPressStop([] {
    // 长按（约1秒）：暂停播放 → 播放重置提示音 → 恢复默认设置（不动 SPIFFS 文件）
    if (audioPlayer.isAudioPlaying() && !audioPlayer.isPaused()) {
      audioPlayer.togglePlayPause();
    }
    playResetTone(settings.loadVolume());
    settings.resetAll();
    audioPlayer.setVolume(DEFAULT_VOLUME);  // 立即应用默认音量（并落库，与 resetAll 一致）
    gPausedForClient = false;               // 已手动暂停，清除自动暂停标志
    webManager.reapplyAp();                 // AP 密码恢复默认，热点重启
    LOG_I("长按重置完成，设置已恢复默认（音频文件保留）");
  });

  // 事件回调：掉电记忆 + 错误日志（上层不轮询内部状态）
  audioPlayer.setOnTrackChanged([](int index, const String& name) {
    settings.saveTrackIndex(index);
    LOG_I("记录播放位置: [%d] %s", index, name.c_str());
  });
  audioPlayer.setOnVolumeChanged([](float volume) {
    settings.saveVolume(volume);
  });
  audioPlayer.setOnError([](const String& message) {
    LOG_E("%s", message.c_str());
  });

  // 恢复掉电前的设置
  settings.begin();
  gAutoPlay = settings.loadAutoPlay();
  float savedVolume = settings.loadVolume();
  audioPlayer.setVolume(savedVolume);
  LOG_I("恢复设置: 音量=%.2f, 自动播放=%s", savedVolume, gAutoPlay ? "开" : "关");

  if (!audioPlayer.initialize()) {
    LOG_E("音频播放器初始化失败，系统无法启动");
    ledManager.setMode(LedMode::FAST_BLINK);
    while (1) {
      delay(1000);
    }
  }

  // 开机提示音（阻塞约 0.3 秒）
  playStartupTone(savedVolume);

  // 启动 Web 管理（AP 模式，无需配网）
  webManager.begin();

  // 扫描音频文件
  audioPlayer.listFiles();
  int fileCount = audioPlayer.createPlaylist();

  if (fileCount == 0) {
    LOG_E("未找到音频文件，请连接热点 %s 后访问网页上传", webManager.getApSsid().c_str());
    ledManager.setMode(LedMode::FAST_BLINK);
  } else if (gAutoPlay) {
    // 掉电记忆：从上次播放位置继续
    int savedIndex = settings.loadTrackIndex();
    LOG_I("自动播放开启，从索引 %d 开始", savedIndex);
    audioPlayer.startPlaylist(savedIndex);
  } else {
    LOG_I("自动播放已关闭，待机中（按键可启动播放）");
    ledManager.setMode(LedMode::OFF);
  }
}

void loop() {
  buttonManager.tick();      // 按键扫描（高频调用保证单击检测可靠）
  webManager.handleClient(); // Web 管理（分块处理，不阻塞音频）
  handleClientPresence();    // 客户端接入暂停/断开恢复
  audioPlayer.update();      // 音频解码驱动
  updateLedState();          // LED 模式切换（播放/无文件/暂停）
  ledManager.update();       // LED 闪烁驱动（快/慢闪的翻转靠它）
  delay(1);
}
