#pragma once

// 分级日志宏：编译期可整体关闭（LOG_ENABLE 置 0）
// 用法与 Serial.printf 相同：LOG_I("值=%d", x);

#include <Arduino.h>

#define LOG_ENABLE 1

#if LOG_ENABLE
#define LOG_I(...)                          \
  do {                                      \
    Serial.printf("[I] ");                  \
    Serial.printf(__VA_ARGS__);             \
    Serial.printf(" (%s:%d)\r\n", __FILE__, __LINE__); \
  } while (0)
#define LOG_E(...)                          \
  do {                                      \
    Serial.printf("[E] ");                  \
    Serial.printf(__VA_ARGS__);             \
    Serial.printf(" (%s:%d)\r\n", __FILE__, __LINE__); \
  } while (0)
#else
#define LOG_I(...) \
  do {             \
  } while (0)
#define LOG_E(...) \
  do {             \
  } while (0)
#endif
