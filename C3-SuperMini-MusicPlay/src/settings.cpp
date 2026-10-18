#include "settings.h"

#include "config.h"
#include "log.h"

static const char* NS = "music";  // NVS 命名空间（最长 15 字符）

void Settings::begin() {
  if (!_prefs.begin(NS, false)) {  // false = 读写模式
    LOG_E("Preferences 初始化失败");
    return;
  }
  LOG_I("Preferences 就绪（命名空间: %s）", NS);
}

float Settings::loadVolume() {
  float v = _prefs.getFloat("volume", DEFAULT_VOLUME);
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  return v;
}

void Settings::saveVolume(float volume) {
  _prefs.putFloat("volume", volume);
}

int Settings::loadTrackIndex() {
  return _prefs.getInt("trackIdx", 0);
}

void Settings::saveTrackIndex(int index) {
  _prefs.putInt("trackIdx", index);
}

bool Settings::loadAutoPlay() {
  return _prefs.getBool("autoplay", true);
}

void Settings::saveAutoPlay(bool enabled) {
  _prefs.putBool("autoplay", enabled);
}

String Settings::loadApPassword() {
  return _prefs.getString("appwd", AP_PASSWORD_DEFAULT);
}

void Settings::saveApPassword(const String& password) {
  _prefs.putString("appwd", password);
}

void Settings::resetAll() {
  _prefs.putFloat("volume", DEFAULT_VOLUME);
  _prefs.putInt("trackIdx", 0);
  _prefs.putBool("autoplay", true);
  _prefs.putString("appwd", AP_PASSWORD_DEFAULT);
  LOG_I("设置已重置为默认值（音频文件未动）");
}
