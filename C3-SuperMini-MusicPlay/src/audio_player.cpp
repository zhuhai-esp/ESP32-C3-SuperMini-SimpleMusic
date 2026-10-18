#include "audio_player.h"

#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorAAC.h>
#include <AudioOutputI2S.h>
#include <LittleFS.h>
#include "log.h"

bool MultiAudioPlayer::initialize() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  if (!LittleFS.begin(true)) {
    LOG_E("LittleFS挂载失败!");
    return false;
  }
  LOG_I("LittleFS总空间: %.2f KB", LittleFS.totalBytes() / 1024.0);
  LOG_I("LittleFS可用空间: %.2f KB",
        (LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024.0);
  LOG_I("LittleFS初始化成功!");
  return true;
}

// 扫描指定扩展名的文件并创建播放列表
int MultiAudioPlayer::createPlaylist(const char* extension) {
  LOG_I("=== 扫描音频文件 ===");

  // 第一次扫描：计算文件数量
  int fileCount = 0;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    if (String(file.name()).endsWith(extension)) {
      fileCount++;
    }
    file = root.openNextFile();
  }
  root.close();

  LOG_I("找到 %d 个 %s 文件", fileCount, extension);

  _playlist.clear();
  if (fileCount == 0) {
    LOG_E("未找到音频文件!");
    return 0;
  }

  // 第二次扫描：填充播放列表
  root = LittleFS.open("/");
  file = root.openNextFile();
  while (file) {
    String filename = file.name();
    if (filename.endsWith(extension)) {
      // 确保路径以/开头
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      _playlist.push_back(filename);
      LOG_I("添加到播放列表: %s", filename.c_str());
    }
    file = root.openNextFile();
  }
  root.close();

  return (int)_playlist.size();
}

void MultiAudioPlayer::listFiles() {
  LOG_I("=== 文件列表 ===");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    LOG_I("文件: %s, 大小: %.2f KB", file.name(), file.size() / 1024.0);
    file = root.openNextFile();
  }
  root.close();
}

PlayResult MultiAudioPlayer::startPlaylist(int startIndex) {
  if (_playlist.empty()) {
    LOG_E("播放列表为空!");
    return PlayResult::EMPTY_PLAYLIST;
  }
  if (startIndex < 0 || startIndex >= (int)_playlist.size()) {
    startIndex = 0;  // 索引越界（如文件被删）时从头开始
  }
  _currentTrackIndex = startIndex;
  return playCurrentTrack();
}

PlayResult MultiAudioPlayer::playCurrentTrack() {
  if (_playlist.empty()) {
    notifyError("播放列表为空");
    return PlayResult::EMPTY_PLAYLIST;
  }
  if (_currentTrackIndex < 0 || _currentTrackIndex >= (int)_playlist.size()) {
    notifyError("曲目索引越界");
    return PlayResult::INDEX_OUT_OF_RANGE;
  }

  if (_isPlaying) {
    stopAudio();
  }

  const char* filename = _playlist[_currentTrackIndex].c_str();

  // 初始化音频输出
  _out = new AudioOutputI2S();
  _out->SetPinout(I2S_PIN_BCLK, I2S_PIN_LRC, I2S_PIN_DIN);
  _out->SetGain(_volume);

  // 打开音频文件
  _file = new AudioFileSourceLittleFS(filename);
  _aac = new AudioGeneratorAAC();

  // 开始播放
  if (_aac->begin(_file, _out)) {
    _isPlaying = true;
    _paused = false;
    LOG_I("开始播放: [%d/%d] %s", _currentTrackIndex + 1, (int)_playlist.size(), filename);
    if (_onTrackChanged) {
      _onTrackChanged(_currentTrackIndex, filename);
    }
    return PlayResult::OK;
  }

  notifyError(String("音频播放器初始化失败: ") + filename);
  cleanupAudio();
  return PlayResult::START_FAIL;
}

bool MultiAudioPlayer::update() {
  if (!_isPlaying || !_aac) {
    return false;
  }

  // 暂停中：不推进解码，冻结当前曲目进度（I2S 输出静音）
  if (_paused) {
    return true;
  }

  if (_aac->isRunning()) {
    if (!_aac->loop()) {
      // 当前音频播放完成
      _aac->stop();
      cleanupAudio();
      _isPlaying = false;

      // 播放下一首（顺序循环）
      playNextTrack();
      return true;
    }
    return true;  // 仍在播放中
  }
  return false;  // 播放已停止
}

bool MultiAudioPlayer::playNextTrack() {
  if (_playlist.empty()) return false;

  // 顺序循环播放
  _currentTrackIndex++;
  if (_currentTrackIndex >= (int)_playlist.size()) {
    _currentTrackIndex = 0;  // 回到第一首，实现循环
    LOG_I("=== 播放列表循环 ===");
  }

  return playCurrentTrack() == PlayResult::OK;
}

void MultiAudioPlayer::stopAudio() {
  if (_aac && _aac->isRunning()) {
    _aac->stop();
  }
  cleanupAudio();
  _isPlaying = false;
  _paused = false;
  LOG_I("音频播放已停止");
}

// 按键切换播放/暂停：未播放时启动播放列表，播放中时冻结/恢复解码
void MultiAudioPlayer::togglePlayPause() {
  if (!_isPlaying) {
    LOG_I("按键启动播放列表...");
    startPlaylist();
    return;
  }
  _paused = !_paused;
  LOG_I(_paused ? "已暂停" : "继续播放");
}

void MultiAudioPlayer::setVolume(float volume) {
  if (volume < 0.0f) volume = 0.0f;
  if (volume > 1.0f) volume = 1.0f;
  _volume = volume;
  if (_out) {
    _out->SetGain(_volume);  // 播放中实时生效
  }
  if (_onVolumeChanged) {
    _onVolumeChanged(_volume);
  }
}

const char* MultiAudioPlayer::getCurrentTrackName() const {
  if (_currentTrackIndex >= 0 && _currentTrackIndex < (int)_playlist.size()) {
    return _playlist[_currentTrackIndex].c_str();
  }
  return nullptr;
}

MultiAudioPlayer::~MultiAudioPlayer() {
  stopAudio();  // 释放解码器/文件/I2S，vector 自动释放播放列表
}

void MultiAudioPlayer::cleanupAudio() {
  if (_aac) {
    delete _aac;
    _aac = nullptr;
  }
  if (_file) {
    delete _file;
    _file = nullptr;
  }
  if (_out) {
    delete _out;
    _out = nullptr;
  }
}

void MultiAudioPlayer::notifyError(const String& message) {
  LOG_E("%s", message.c_str());
  if (_onError) {
    _onError(message);
  }
}
