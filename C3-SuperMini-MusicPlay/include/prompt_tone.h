#pragma once

// 提示音模块：内置合成音，无需音频文件。
//   playStartupTone() —— 开机提示音（上升两音）
//   playResetTone()   —— 长按重置提示音（下降两音，方向相反便于区分）
// 实现方式：在内存中生成 16bit 单声道 WAV，经 AudioGeneratorWAV 播放——
// 与 AAC 音乐完全相同的解码输出路径（解码器负责 SetRate/begin/DMA 填充），
// 音量和听感与音乐播放一致。
// 均为阻塞播放（约 0.3 秒），应在 setup() 或按键回调中调用。

void playStartupTone(float volume);
void playResetTone(float volume);
