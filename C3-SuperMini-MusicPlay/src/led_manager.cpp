#include "led_manager.h"

#include "log.h"

LEDManager::LEDManager(uint8_t pin) : _pin(pin) {}

void LEDManager::begin() {
  pinMode(_pin, OUTPUT);
  setLEDs(false);  // 初始状态关闭
}

void LEDManager::setMode(LedMode mode) {
  if (mode == _mode) {
    return;  // 无变化，避免重复写 GPIO
  }
  _mode = mode;
  // 切换模式时从熄灭开始，避免停在亮起状态
  _ledState = false;
  _lastToggleTime = millis();
  switch (mode) {
    case LedMode::OFF:
      setLEDs(false);
      break;
    case LedMode::ON:
      setLEDs(true);
      break;
    default:
      break;  // 闪烁模式由 update() 驱动
  }
}

void LEDManager::update() {
  if (_mode != LedMode::SLOW_BLINK && _mode != LedMode::FAST_BLINK) {
    return;
  }
  unsigned long interval = (_mode == LedMode::SLOW_BLINK) ? _slowInterval : LED_BLINK_FAST;
  unsigned long currentTime = millis();
  if (currentTime - _lastToggleTime >= interval) {
    // 交替闪烁
    _ledState = !_ledState;
    digitalWrite(_pin, _ledState ? LED_ON : LED_OFF);
    _lastToggleTime = currentTime;
  }
}

void LEDManager::setLEDs(bool state) {
  _ledState = state;
  _mode = state ? LedMode::ON : LedMode::OFF;
  digitalWrite(_pin, state ? LED_ON : LED_OFF);
}

void LEDManager::setToggleInterval(unsigned long interval) {
  _slowInterval = interval;
}
