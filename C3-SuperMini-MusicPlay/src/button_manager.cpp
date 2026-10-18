#include "button_manager.h"

ButtonManager* ButtonManager::_instance = nullptr;

ButtonManager::ButtonManager(uint8_t pin, bool activeLow)
    : _button(pin, activeLow) {
  _instance = this;
}

void ButtonManager::begin() {
  // 若回调已设置，确保 OneButton 已注册蹦床函数
  if (_onClick) {
    _button.attachClick(onClickTrampoline);
  }
  if (_onLongPress) {
    _button.attachLongPressStop(onLongPressTrampoline);
  }
}

void ButtonManager::attachClick(ClickCallback callback) {
  _onClick = callback;
  _button.attachClick(onClickTrampoline);
}

void ButtonManager::attachLongPressStop(LongPressCallback callback) {
  _onLongPress = callback;
  _button.attachLongPressStop(onLongPressTrampoline);
}

void ButtonManager::tick() {
  _button.tick();
}

// OneButton 只接受无参函数指针，通过静态蹦床转发到当前实例的回调
void ButtonManager::onClickTrampoline() {
  if (_instance && _instance->_onClick) {
    _instance->_onClick();
  }
}

void ButtonManager::onLongPressTrampoline() {
  if (_instance && _instance->_onLongPress) {
    _instance->_onLongPress();
  }
}
