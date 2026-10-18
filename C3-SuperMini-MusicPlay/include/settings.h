#pragma once

// 掉电记忆：基于 NVS(Preferences) 持久化音量、曲目索引、开机自动播放开关。

#include <Preferences.h>

class Settings {
public:
  void begin();  // 打开存储命名空间（setup 中调用一次）

  float loadVolume();
  void saveVolume(float volume);

  int loadTrackIndex();
  void saveTrackIndex(int index);

  bool loadAutoPlay();
  void saveAutoPlay(bool enabled);

  // AP 连接密码（默认值见 config.h 的 AP_PASSWORD_DEFAULT）
  String loadApPassword();
  void saveApPassword(const String& password);

  // 重置所有设置为默认值（音量/曲目索引/自动播放/AP密码），不影响 SPIFFS 中的文件
  void resetAll();

private:
  Preferences _prefs;
};
