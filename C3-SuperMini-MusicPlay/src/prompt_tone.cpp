#include "prompt_tone.h"

#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <math.h>
#include <string.h>
#include "config.h"
#include "log.h"

namespace {

// ========== 内存音频源 ==========
// 实现 AudioFileSource 接口，把内存缓冲当作"文件"提供给 WAV 解码器
class MemoryBufferSource : public AudioFileSource {
public:
  MemoryBufferSource(const uint8_t* data, uint32_t len)
      : _data(data), _len(len), _pos(0) {}

  bool open(const char* filename) override {
    (void)filename;
    _pos = 0;
    return true;
  }

  uint32_t read(void* dst, uint32_t len) override {
    if (_pos >= _len || !dst) {
      return 0;
    }
    uint32_t n = _len - _pos;
    if (n > len) {
      n = len;
    }
    memcpy(dst, _data + _pos, n);
    _pos += n;
    return n;
  }

  bool seek(int32_t pos, int dir) override {
    int64_t target;
    if (dir == SEEK_SET) {
      target = pos;
    } else if (dir == SEEK_CUR) {
      target = (int64_t)_pos + pos;
    } else {
      target = (int64_t)_len + pos;  // SEEK_END
    }
    if (target < 0 || target > (int64_t)_len) {
      return false;
    }
    _pos = (uint32_t)target;
    return true;
  }

  bool close() override { return true; }
  bool isOpen() override { return true; }
  uint32_t getSize() override { return _len; }
  uint32_t getPos() override { return _pos; }

private:
  const uint8_t* _data;
  uint32_t _len;
  uint32_t _pos;
};

// ========== 旋律定义 ==========
struct ToneNote {
  uint16_t freqHz;
  uint16_t durationMs;
};

// 开机提示音：上升两音（B5 → E6）
const ToneNote STARTUP_MELODY[] = {
    {988, 80},
    {1319, 160},
};

// 重置提示音：下降两音（E6 → B5），方向相反便于区分
const ToneNote RESET_MELODY[] = {
    {1319, 80},
    {988, 160},
};

const int NOTE_GAP_MS = 20;   // 音符间静音间隔
const int FADE_IN_MS = 10;    // 淡入（避免爆音）
const int FADE_OUT_MS = 25;   // 淡出（避免爆音）

// WAV 缓冲（DRAM，仅在播放提示音期间使用）
const uint32_t WAV_MAX_SAMPLES = PROMPT_TONE_RATE * PROMPT_TONE_MAX_MS / 1000;
uint8_t _wavBuf[44 + WAV_MAX_SAMPLES * 2];

void putU16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)((v >> 8) & 0xff);
}

void putU32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)((v >> 8) & 0xff);
  p[2] = (uint8_t)((v >> 16) & 0xff);
  p[3] = (uint8_t)((v >> 24) & 0xff);
}

// 生成 16bit 单声道 PCM WAV（RIFF 头 + 正弦音），返回总字节数
uint32_t buildWav(const ToneNote* melody, int count) {
  int16_t* pcm = reinterpret_cast<int16_t*>(_wavBuf + 44);
  int idx = 0;

  for (int n = 0; n < count; n++) {
    if (n > 0) {
      // 音符间静音间隔
      int gap = PROMPT_TONE_RATE * NOTE_GAP_MS / 1000;
      for (int i = 0; i < gap && idx < (int)WAV_MAX_SAMPLES; i++) {
        pcm[idx++] = 0;
      }
    }
    int total = PROMPT_TONE_RATE * melody[n].durationMs / 1000;
    int fadeIn = PROMPT_TONE_RATE * FADE_IN_MS / 1000;
    int fadeOut = PROMPT_TONE_RATE * FADE_OUT_MS / 1000;
    // 相位用 double 累加，数值稳定
    double phaseInc = 2.0 * PI * melody[n].freqHz / PROMPT_TONE_RATE;
    double phase = 0.0;
    for (int i = 0; i < total && idx < (int)WAV_MAX_SAMPLES; i++) {
      float env = 1.0f;
      if (i < fadeIn) {
        env = (float)i / fadeIn;
      } else if (i > total - fadeOut) {
        env = (float)(total - i) / fadeOut;
      }
      float v = (float)sin(phase);
      phase += phaseInc;
      if (phase >= 2.0 * PI) {
        phase -= 2.0 * PI;
      }
      pcm[idx++] = (int16_t)(PROMPT_TONE_AMPLITUDE * v * env);
    }
  }

  uint32_t dataBytes = (uint32_t)idx * 2;

  // ---- RIFF/WAVE 文件头（44 字节）----
  memcpy(_wavBuf, "RIFF", 4);
  putU32(_wavBuf + 4, 36 + dataBytes);  // 文件总大小 - 8
  memcpy(_wavBuf + 8, "WAVE", 4);
  memcpy(_wavBuf + 12, "fmt ", 4);
  putU32(_wavBuf + 16, 16);                       // fmt 块大小
  putU16(_wavBuf + 20, 1);                        // PCM
  putU16(_wavBuf + 22, 1);                        // 单声道
  putU32(_wavBuf + 24, PROMPT_TONE_RATE);         // 采样率
  putU32(_wavBuf + 28, PROMPT_TONE_RATE * 2);     // 字节率
  putU16(_wavBuf + 32, 2);                        // 块对齐
  putU16(_wavBuf + 34, 16);                       // 位深
  memcpy(_wavBuf + 36, "data", 4);
  putU32(_wavBuf + 40, dataBytes);                // PCM 数据大小

  return 44 + dataBytes;
}

void playMelody(const ToneNote* melody, int count, float volume) {
  uint32_t wavLen = buildWav(melody, count);

  MemoryBufferSource source(_wavBuf, wavLen);
  AudioOutputI2S out;
  out.SetPinout(I2S_PIN_BCLK, I2S_PIN_LRC, I2S_PIN_DIN);
  out.SetGain(volume);  // 与音乐播放相同的用户音量

  AudioGeneratorWAV wav;
  // 由解码器完成 SetRate(44100)/begin()——与 AAC 播放路径完全一致。
  // WAV 采样率与 I2S 驱动安装默认值(44100)相同，音高精确无误。
  if (!wav.begin(&source, &out)) {
    LOG_E("提示音播放失败");
    return;
  }
  while (wav.isRunning()) {
    wav.loop();
    yield();  // 让出 CPU，避免忙等触发任务看门狗
  }
  // wav.stop() 内部已调用 output->stop() 卸载 I2S 驱动
  LOG_I("提示音播放完成");
}

}  // namespace

void playStartupTone(float volume) {
  playMelody(STARTUP_MELODY, sizeof(STARTUP_MELODY) / sizeof(STARTUP_MELODY[0]), volume);
}

void playResetTone(float volume) {
  playMelody(RESET_MELODY, sizeof(RESET_MELODY) / sizeof(RESET_MELODY[0]), volume);
}
