#pragma once

// ========== 串口 ==========
#define SERIAL_BAUD 115200

// ========== 引脚定义 ==========
#define PIN_LED 8      // 板载 LED
#define PIN_BUTTON 9   // 板载 BOOT 按键，低电平有效

// ========== LED 极性 ==========
// 本板板载 LED 为低电平点亮（GPIO 写 LOW 亮、写 HIGH 灭）
// 若使用高电平点亮的板子，将下面两个值对调
#define LED_ON LOW
#define LED_OFF HIGH

// ========== LED 指示模式 ==========
#define LED_BLINK_SLOW 500  // 正常播放：慢闪间隔(ms)
#define LED_BLINK_FAST 100  // 无文件/播放异常：快闪间隔(ms)

// ========== I2S（MAX98357）==========
#define I2S_PIN_BCLK 2
#define I2S_PIN_LRC 1
#define I2S_PIN_DIN 3

// ========== 音量 ==========
#define DEFAULT_VOLUME 0.6f  // 默认音量（0.0 ~ 1.0）

// ========== 音频 ==========
// 默认扫描/播放的文件扩展名
#define AUDIO_EXTENSION ".aac"

// ========== 播放列表重试 ==========
// 播放停止后重新启动播放列表的间隔(ms)
#define PLAYLIST_RETRY_INTERVAL 5000

// ========== 提示音 ==========
// 采样率必须与 AudioOutputI2S::begin() 的驱动默认值(44100)一致，
// 否则硬件时钟与内容速率不匹配会导致音高/时长错误
#define PROMPT_TONE_RATE 44100      // 提示音采样率(Hz)
#define PROMPT_TONE_AMPLITUDE 30000 // 提示音峰值幅度(0~32767)，与音乐满幅内容相当
#define PROMPT_TONE_MAX_MS 300      // WAV 缓冲可容纳的最大时长(ms)

// ========== Web 管理（仅 AP 模式，不连接路由器） ==========
#define AP_SSID_PREFIX "C3-Music"      // 热点名称前缀，完整 SSID = 前缀-硬件ID(4位)
#define AP_PASSWORD_DEFAULT "88888888" // 默认 AP 密码（8-63 位，可在网页修改并存 NVS）
#define WEB_SERVER_PORT 80
#define DNS_SERVER_PORT 53             // 强制门户 DNS（连接热点后自动弹出网页）
