#pragma once

// LED 管理器：支持熄灭/常亮/慢闪/快闪四种指示模式。
// 电平极性由 config.h 的 LED_ON / LED_OFF 定义（本板为低电平点亮）。

#include <Arduino.h>
#include "config.h"

enum class LedMode {
  OFF,         // 熄灭（暂停/待机）
  ON,          // 常亮
  SLOW_BLINK,  // 慢闪：正常播放
  FAST_BLINK   // 快闪：无音频文件或播放异常
};

class LEDManager {
public:
  explicit LEDManager(uint8_t pin);

  void begin();
  void setMode(LedMode mode);  // 切换指示模式（重复设置同一模式无动作）
  void update();               // 驱动闪烁，需在 loop() 中调用
  void setLEDs(bool state);    // 直接开关（等效于 ON/OFF 模式）
  void setToggleInterval(unsigned long interval);  // 自定义慢闪间隔(ms)

private:
  uint8_t _pin;
  LedMode _mode = LedMode::OFF;
  bool _ledState = false;
  unsigned long _lastToggleTime = 0;
  unsigned long _slowInterval = LED_BLINK_SLOW;
};
