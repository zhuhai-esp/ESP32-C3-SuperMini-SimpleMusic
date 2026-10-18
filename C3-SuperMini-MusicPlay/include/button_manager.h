#pragma once

// 按键管理器：封装 OneButton，当前支持单击与长按回调。
// 注意：tick() 需在 loop() 中高频调用才能可靠检测按键动作；
// 内部使用静态蹦床转发回调，同一时刻只应存在一个实例。

#include <Arduino.h>
#include <OneButton.h>
#include <functional>

class ButtonManager {
public:
  using ClickCallback = std::function<void()>;
  using LongPressCallback = std::function<void()>;

  ButtonManager(uint8_t pin, bool activeLow);

  void begin();
  void attachClick(ClickCallback callback);
  void attachLongPressStop(LongPressCallback callback);  // 长按（默认约1秒）松开后触发
  void tick();

private:
  static void onClickTrampoline();
  static void onLongPressTrampoline();

  OneButton _button;
  ClickCallback _onClick;
  LongPressCallback _onLongPress;
  static ButtonManager* _instance;
};
