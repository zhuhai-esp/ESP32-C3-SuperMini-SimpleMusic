#pragma once

// 多文件 AAC 播放器：扫描 LittleFS 中的音频文件，顺序循环播放，
// 支持按键切换的播放/暂停（暂停时冻结解码，保留曲目进度）。

#include <Arduino.h>
#include <functional>
#include <vector>
#include "config.h"

// 前向声明：头文件不依赖 ESP8266Audio，降低编译耦合
class AudioGeneratorAAC;
class AudioFileSourceLittleFS;
class AudioOutputI2S;

// 播放操作结果
enum class PlayResult {
  OK,                 // 成功
  EMPTY_PLAYLIST,     // 播放列表为空
  INDEX_OUT_OF_RANGE, // 曲目索引越界
  START_FAIL          // 文件打开或解码器初始化失败
};

class MultiAudioPlayer {
public:
  // 事件回调（供上层做掉电记忆、日志等，避免上层轮询内部状态）
  using TrackChangedCallback = std::function<void(int index, const String& name)>;
  using VolumeChangedCallback = std::function<void(float volume)>;
  using ErrorCallback = std::function<void(const String& message)>;

  bool initialize();
  void listFiles();

  // 扫描指定扩展名的文件并创建播放列表，返回文件数量（0 表示无文件）
  int createPlaylist(const char* extension = AUDIO_EXTENSION);

  // 从指定索引开始顺序循环播放（默认从头开始，索引越界自动归零）
  PlayResult startPlaylist(int startIndex = 0);
  PlayResult playCurrentTrack();
  bool update();
  bool playNextTrack();
  void stopAudio();

  // 按键切换播放/暂停：未播放时启动播放列表，播放中时冻结/恢复解码
  void togglePlayPause();

  // 音量（0.0 ~ 1.0），设置后立即生效并触发 onVolumeChanged 回调
  void setVolume(float volume);
  float getVolume() const { return _volume; }

  bool isAudioPlaying() const { return _isPlaying; }
  bool isPaused() const { return _paused; }
  int getCurrentTrackIndex() const { return _currentTrackIndex; }
  int getPlaylistSize() const { return (int)_playlist.size(); }
  const char* getCurrentTrackName() const;

  void setOnTrackChanged(TrackChangedCallback cb) { _onTrackChanged = cb; }
  void setOnVolumeChanged(VolumeChangedCallback cb) { _onVolumeChanged = cb; }
  void setOnError(ErrorCallback cb) { _onError = cb; }

  ~MultiAudioPlayer();

private:
  void cleanupAudio();
  void notifyError(const String& message);

  AudioGeneratorAAC* _aac = nullptr;
  AudioFileSourceLittleFS* _file = nullptr;
  AudioOutputI2S* _out = nullptr;

  std::vector<String> _playlist;  // 播放列表（vector 自动管理内存）
  int _currentTrackIndex = 0;     // 当前播放的曲目索引
  bool _isPlaying = false;
  bool _paused = false;           // 暂停标志（冻结解码，保留进度）
  float _volume = DEFAULT_VOLUME;

  TrackChangedCallback _onTrackChanged;
  VolumeChangedCallback _onVolumeChanged;
  ErrorCallback _onError;
};
