# ESP32-C3-SuperMini-SimpleMusic

基于 ESP32-C3 SuperMini 和 MAX98357 开发板的廉价音频播放设备，简单音频场景，上电就响，后台配置参数。

## 功能特性

- 上电自动播放：从 LittleFS 文件系统扫描 `.aac` 音频文件，自动创建播放列表并顺序循环播放
- 按键控制：GPIO9 按键（板载 BOOT 按键）单击切换播放/暂停，暂停时冻结解码进度，恢复时继续
- LED 状态指示：播放音频时 GPIO8 引脚 LED 以 200ms 间隔闪烁，暂停或停止播放时熄灭
- 掉线自恢复：播放异常停止时，每 5 秒自动尝试重启播放列表
- 串口调试：115200 波特率输出文件系统信息、播放进度与播放列表

## 硬件组成

| 组件 | 说明 |
| --- | --- |
| 主控 | ESP32-C3 SuperMini（PlatformIO 板型 `esp32-c3-devkitm-1`，环境名 `esp32-c3-supermini`，Arduino 框架） |
| 功放 | MAX98357 I2S 数字功放 |
| 指示灯 | GPIO8（低电平点亮），播放时闪烁，暂停/停止时熄灭 |
| 按键 | GPIO9（板载 BOOT 按键，低电平有效），单击切换播放/暂停 |

### I2S 引脚连接（MAX98357）

| 信号 | ESP32-C3 引脚 |
| --- | --- |
| BCLK（位时钟） | GPIO2 |
| LRC / WSEL（字选择） | GPIO1 |
| DIN（数据） | GPIO3 |

音量增益固定为 `0.6`，在 `src/main.cpp` 的 `playCurrentTrack()` 中通过 `SetGain(0.6)` 设置。

## 目录结构

```
ESP32-C3-SuperMini-SimpleMusic/
├── README.md                      # 本文件
└── C3-SuperMini-MusicPlay/        # PlatformIO 工程
    ├── platformio.ini             # 工程配置（板型/依赖/分区）
    ├── aac.csv                    # 自定义分区表（音频数据分区约 2.8MB）
    ├── src/
    │   └── main.cpp               # 全部源码：MultiAudioPlayer + LEDManager
    ├── data/                      # 音频文件放置目录（烧录前放入 .aac 文件）
    ├── include/                   # PlatformIO 默认目录（未使用）
    ├── lib/                       # PlatformIO 默认目录（未使用）
    ├── test/                      # PlatformIO 默认目录（未使用）
    └── README.md                  # 音频转换与烧录说明
```

## 核心代码结构

`src/main.cpp` 包含两个类：

- **`MultiAudioPlayer`**：音频播放器。负责 LittleFS 初始化与文件扫描（`createPlaylist()`）、播放列表循环播放（`startPlaylist()` / `playNextTrack()`）、每帧解码驱动（`update()`，需在 `loop()` 中高频调用）与资源释放（`cleanupAudio()`）。支持按键切换的播放/暂停（`togglePlayPause()`）——暂停通过冻结解码实现（停止调用 `aac->loop()`，保留 AAC 解码器状态与曲目进度，恢复时继续）。依赖 [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) 库（`AudioFileSourceLittleFS` + `AudioGeneratorAAC` + `AudioOutputI2S`）。
- **`LEDManager`**：LED 管理器。`begin()` 初始化引脚，`update()` 按设定间隔（默认 200ms）闪烁，`setLEDs()` 直接控制亮灭。

按键部分：全局 `OneButton button(PIN_BUTTON, true)`（GPIO9，低电平有效），`setup()` 中 `button.attachClick(onButtonClick)` 注册单击回调（调用 `audioPlayer.togglePlayPause()`），`loop()` 中 `button.tick()` 高频扫描按键。未播放时按键按下会启动播放列表，播放中按下则暂停/恢复。

## 依赖库（platformio.ini）

| 库 | 版本 | 用途 |
| --- | --- | --- |
| `earlephilhower/ESP8266Audio` | 2.2.0 | AAC 解码、LittleFS 音频源、I2S 输出 |
| `mathertel/OneButton` | ^2.6.2 | 按键输入（GPIO9 单击切换播放/暂停） |

首次编译时以上依赖会自动拉取。

## 编译与烧录

环境要求：[PlatformIO](https://platformio.org/)（VS Code 插件或 CLI）。

```bash
cd C3-SuperMini-MusicPlay

# 编译
pio run

# 烧录固件
pio run --target upload

# 查看串口日志
pio device monitor
```

## 音频文件准备与烧录

音频需预先转换为 AAC 格式放入 LittleFS 数据分区：

1. 文件转换（单声道、96kbps、44.1kHz，适合语音/提示音场景）：

   ```bash
   ffmpeg -i input.mp3 \
     -c:a aac \
     -b:a 96k \
     -ar 44100 \
     -ac 1 \
     -movflags +faststart \
     output.aac
   ```

2. 将转换后的 `.aac` 文件放入 `C3-SuperMini-MusicPlay/data/` 目录。

3. 烧录文件分区（只烧数据、不烧固件）：

   ```bash
   pio run --target uploadfs
   ```

## 分区说明（aac.csv）

| 分区 | 类型 | 偏移 | 大小 | 用途 |
| --- | --- | --- | --- | --- |
| nvs | data/nvs | 0x9000 | 0x5000 | 系统参数存储 |
| phy_init | data/phy | 0xe000 | 0x1000 | PHY 校准数据 |
| factory | app/factory | 0x10000 | 0x100000 (1MB) | 应用程序 |
| spiffs | data/spiffs | 0x110000 | 0x2E0000 (~2.8MB) | 音频文件（LittleFS） |

> 注意：`platformio.ini` 配置为 `board_build.filesystem = littlefs`，音频数据分区约 2.8MB，请估算好音频总时长（96kbps ≈ 12KB/s，约 4 分钟音频）。

## 运行行为

1. 上电后初始化串口（115200）、LED 与 LittleFS，串口打印分区总/可用空间。
2. 扫描根目录全部 `.aac` 文件并打印播放列表。
3. 从第一首开始顺序播放，播完自动切下一首，列表末尾自动回到第一首循环。
4. 播放期间 LED 闪烁；暂停或无音频播放时 LED 熄灭；播放停止（非暂停）时每 5 秒尝试重启播放列表。
5. GPIO9 按键单击切换播放/暂停：播放中按下暂停（冻结当前曲目进度，I2S 输出静音），再次按下恢复；未播放时按下会启动播放列表。
6. 音频文件缺失或播放器初始化失败时，串口打印错误并挂起（`while(1)`）。
