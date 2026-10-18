# ESP32-C3-SuperMini-SimpleMusic

基于 ESP32-C3 SuperMini 和 MAX98357 开发板的廉价音频播放设备，简单音频场景，上电就响，后台配置参数。

## 功能特性

- 上电自动播放：从 LittleFS 文件系统扫描 `.aac` 音频文件，自动创建播放列表并顺序循环播放
- 开机提示音：上电后播放内置合成的两音提示音（无需音频文件，音量与音乐一致）
- 按键控制：GPIO9 按键（板载 BOOT 按键）单击切换播放/暂停，暂停时冻结解码进度，恢复时继续
- 长按重置：GPIO9 长按约 1 秒，暂停播放并播放下降音提示音，然后重置所有设置为默认值（音量/播放位置/自动播放/热点密码），**保留 SPIFFS 中的音频文件**
- LED 状态指示：正常播放慢闪（500ms）、无文件或播放异常快闪（100ms）、暂停/待机熄灭
- 掉电记忆：基于 NVS（Preferences）保存音量、播放位置、开机自动播放开关、热点密码，下次上电自动恢复
- Web 管理：设备发射 WiFi 热点（AP 模式，无需配网），连接后自动弹出管理页面，可上传/删除音频文件、查看并格式化 SPIFFS 分区、实时调音量、开关开机自动播放、修改热点密码、远程重启设备
- 智能播放暂停：有客户端接入热点时自动暂停播放（单核性能不足以同时兼顾播放与 Web 服务），全部客户端断开后自动恢复
- 热点名称：`C3-Music-XXXX`（后缀为芯片硬件 ID 后 4 位十六进制，多台设备并存不重名）
- 掉线自恢复：播放异常停止时，每 5 秒自动尝试重启播放列表
- 串口调试：115200 波特率分级日志输出（`LOG_I` / `LOG_E`，`log.h` 可编译期关闭）

## 硬件组成

| 组件 | 说明 |
| --- | --- |
| 主控 | ESP32-C3 SuperMini（PlatformIO 板型 `esp32-c3-devkitm-1`，环境名 `esp32-c3-supermini`，Arduino 框架） |
| 功放 | MAX98357 I2S 数字功放 |
| 指示灯 | GPIO8（低电平点亮）：播放慢闪 / 无文件或异常快闪 / 暂停或待机熄灭 |
| 按键 | GPIO9（板载 BOOT 按键，低电平有效），单击切换播放/暂停 |

### I2S 引脚连接（MAX98357）

| 信号 | ESP32-C3 引脚 |
| --- | --- |
| BCLK（位时钟） | GPIO2 |
| LRC / WSEL（字选择） | GPIO1 |
| DIN（数据） | GPIO3 |

默认音量 `0.6`（`config.h` 的 `DEFAULT_VOLUME`），实际音量来自 NVS 掉电记忆，可在 Web 页面实时调节（`0% ~ 100%`），播放中即时生效并自动保存。

## 目录结构

```
ESP32-C3-SuperMini-SimpleMusic/
├── README.md                      # 本文件
├── doc/                           # 文档图片（硬件照片等）
└── C3-SuperMini-MusicPlay/        # PlatformIO 工程
    ├── platformio.ini             # 工程配置（板型/依赖/分区）
    ├── aac.csv                    # 自定义分区表（音频数据分区约 2.8MB）
    ├── include/                   # 头文件（各模块对外接口）
    │   ├── config.h               # 所有可配置常量（引脚/极性/音量/AP 参数等）
    │   ├── log.h                  # 分级日志宏（LOG_I / LOG_E，可编译期关闭）
    │   ├── audio_player.h         # 音频播放器接口
    │   ├── led_manager.h          # LED 管理器接口（四种指示模式）
    │   ├── button_manager.h       # 按键管理器接口
    │   ├── settings.h             # 掉电记忆（Preferences/NVS）接口
    │   ├── prompt_tone.h           # 提示音接口
    │   └── web_manager.h          # Web 管理（AP 模式）接口
    ├── src/
    │   ├── main.cpp               # setup()/loop() 编排，不含业务逻辑
    │   ├── audio_player.cpp       # 播放列表/AAC 解码/暂停恢复/音量
    │   ├── led_manager.cpp        # LED 快慢闪驱动
    │   ├── button_manager.cpp     # OneButton 封装（单击/长按，静态蹦床转发回调）
    │   ├── settings.cpp           # NVS 读写（音量/曲目索引/自动播放/热点密码）
    │   ├── prompt_tone.cpp         # 提示音：内存 WAV 经 AudioGeneratorWAV 播放
    │   ├── web_manager.cpp        # AP + DNS 强制门户 + HTTP 服务 + REST API
    │   └── html/
    │       └── index.html         # 管理页面（经 embed_files 编译期内嵌，可独立预览）
    ├── data/                      # 音频文件放置目录（烧录前放入 .aac 文件）
    ├── lib/                       # PlatformIO 默认目录（未使用）
    ├── test/                      # PlatformIO 默认目录（未使用）
    └── README.md                  # 音频转换与烧录说明
```

## 核心代码结构

代码按功能拆分为七个模块，`src/main.cpp` 只负责装配与调度：

- **`config.h`**：集中管理所有可配置常量——引脚定义（`PIN_LED` / `PIN_BUTTON`）、LED 极性（`LED_ON` / `LED_OFF`）与快慢闪间隔、I2S 引脚、默认音量、提示音参数、AP 名称前缀与默认密码。换板子或调参数只需改这一个文件。
- **`log.h`**：分级日志宏 `LOG_I` / `LOG_E`（带文件名行号），`LOG_ENABLE` 置 0 可编译期整体关闭。
- **`audio_player`**：`MultiAudioPlayer` 音频播放器。LittleFS 初始化与文件扫描（`createPlaylist()`）、播放列表循环播放（`startPlaylist()` / `playNextTrack()`）、每帧解码驱动（`update()`）、资源释放（`cleanupAudio()`）、播放/暂停（`togglePlayPause()`，暂停通过冻结解码保留进度）、音量实时调节（`setVolume()`）。播放列表用 `std::vector<String>` 自动管理内存；操作结果用 `PlayResult` 枚举表达；对外抛 `onTrackChanged` / `onVolumeChanged` / `onError` 事件回调（上层无需轮询内部状态）。头文件用前向声明隐藏 ESP8266Audio 依赖。
- **`led_manager`**：`LEDManager`，四种指示模式 `LedMode::OFF / ON / SLOW_BLINK / FAST_BLINK`，`setMode()` 切换、`update()` 驱动闪烁，极性由 `config.h` 控制。
- **`button_manager`**：`ButtonManager` 封装 OneButton，`attachClick()` / `attachLongPressStop()` 注册单击与长按回调、`tick()` 按键扫描。OneButton 只接受普通函数指针，内部用静态蹦床转发到实例回调，上层可直接写 lambda。单实例设计（单按键够用）。
- **`settings`**：`Settings` 封装 Preferences（NVS），持久化音量 / 曲目索引 / 开机自动播放 / 热点密码；`resetAll()` 一键恢复默认设置（不动音频文件）。
- **`prompt_tone`**：`playStartupTone()` / `playResetTone()` 运行时合成两音提示音（开机为上升两音 B5→E6，重置为下降两音 E6→B5，正弦波 + 淡入淡出包络防爆音），在内存中生成 16bit 单声道 44100Hz WAV，经 `AudioGeneratorWAV` + 内存源播放——与 AAC 音乐**完全相同的解码输出路径**（解码器负责 `SetRate`/`begin()`/DMA 填充），因此音高精确、无噪音，且内容为满幅、增益用用户音量，听感与音乐一致。无需音频文件。
- **`web_manager`**：`WebManager` 启动 AP（`WiFi.softAP`，SSID = `C3-Music-` + 芯片硬件 ID 后 4 位）+ DNS 强制门户（`DNSServer` 劫持全部域名到本机，连接后自动弹页面）+ `WebServer`。管理页面 `src/html/index.html` 经 `platformio.ini` 的 `board_build.embed_files` 由 objcopy 嵌入固件（符号 `_binary_src_html_index_html_start/_end`），修改网页需重新编译。REST API：`/api/files`（列表）、`/api/upload`（分块上传，不阻塞播放）、`/api/delete`（删除，含目录穿越防护）、`/api/format`（先停播再格式化 SPIFFS）、`/api/volume`（音量读写）、`/api/autoplay`（自动播放开关）、`/api/apinfo` + `/api/appwd`（热点密码查看/修改，改后热点重启）、`/api/status`（状态轮询）。

## 依赖库（platformio.ini）

| 库 | 版本 | 用途 |
| --- | --- | --- |
| `earlephilhower/ESP8266Audio` | 2.2.0 | AAC 解码、LittleFS 音频源、I2S 输出 |
| `mathertel/OneButton` | ^2.6.2 | 按键输入（GPIO9 单击切换播放/暂停） |

Web 管理使用的 `WiFi` / `WebServer` / `Preferences` 均为 Arduino-ESP32 核心内置库，无需额外依赖。首次编译时以上依赖会自动拉取。

> 注意：当前固件约 964KB，已占用 app 分区（1MB）的 92.0%。后续若继续加功能导致编译报分区不足，可将 `aac.csv` 中 `factory` 分区从 `0x100000` 调大（如 `0x180000`），并相应调小 `spiffs` 分区。

## Web 管理使用

设备上电后自动发射 WiFi 热点（AP 模式，**不连接路由器**）：

1. 手机/电脑连接热点：SSID 为 `C3-Music-XXXX`（XXXX 为该设备硬件 ID 后 4 位，开机串口日志也会打印），默认密码 `88888888`
2. 连接后管理页面通常会自动弹出（强制门户）；未弹出时浏览器手动打开 `http://192.168.4.1`
3. 页面功能：
   - **播放状态**：播放/暂停/停止、当前曲目、播放进度（2 秒自动刷新）
   - **存储分区（SPIFFS）**：已用/总计与占用比例进度条；**格式化分区**（先停止播放，再清空全部音频文件）
   - **文件管理**：上传 `.aac` 文件（带进度条，上传后自动加入播放列表）、删除文件、查看大小
   - **音量**：滑杆调节后点击「保存音量」才保存（不自动保存）
   - **热点设置**：查看 SSID 与管理地址、修改连接密码（8-63 位，点击「修改密码」保存，保存后热点重启，需重新连接）
   - **开机自动播放**：开关切换后点击「保存」，**下次开机生效**
   - **系统**：远程重启设备

> **上传必须使用系统浏览器**：连接 WiFi 时自动弹出的认证页（强制门户页面）通常无法调用文件选择器，不能上传文件。请在 Safari / Chrome 等系统浏览器地址栏手动输入 `http://192.168.4.1` 打开本页再上传（页面内也有醒目提示）。

> 上传/删除/格式化时因 ESP32-C3 单核与 WiFi 栈占用 CPU，播放可能出现轻微卡顿，属正常现象；大文件建议在播放间隙操作。删除正在播放的文件会先自动停止播放。修改热点密码或长按按键重置后，热点会重启，需重新连接。

> **自动暂停策略**：ESP32-C3 单核难以同时流畅播放音频与运行 Web 服务。有客户端接入热点时自动暂停播放，全部客户端断开后自动恢复（仅恢复因接入而暂停的状态，用户手动暂停/播放不受影响）。此策略对页面操作透明，无需手动开关。

## 按键操作

| 操作 | 行为 |
| --- | --- |
| 单击 | 播放 / 暂停切换（未播放时启动播放列表） |
| 长按约 1 秒 | 暂停播放 → 播放下降音提示音 → 重置所有设置为默认（音量 60%、从头播放、自动播放开、热点密码 `88888888`），**音频文件保留** |

## 网页开发（embed_files）

管理页面为独立文件 `src/html/index.html`，可直接用浏览器打开预览（静态页面，无固件依赖）。接入方式：

```ini
; platformio.ini
board_build.embed_files =
	src/html/index.html
```

PlatformIO 通过 `objcopy -I binary` 将其转为目标文件链入固件，代码中用符号访问：

```cpp
extern const char index_html_start[] asm("_binary_src_html_index_html_start");
extern const char index_html_end[] asm("_binary_src_html_index_html_end");
```

**修改网页后必须重新编译烧录固件**（`pio run --target upload`），仅烧文件系统不生效。

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

1. 上电后初始化串口（115200）、LED、按键与 NVS，从 NVS 恢复音量/自动播放设置。
2. 挂载 LittleFS 并播放开机提示音（上升两音，音量取恢复值，与音乐音量一致）。
3. 启动 AP 热点与 Web 服务，扫描根目录 `.aac` 文件创建播放列表。
4. 播放逻辑：自动播放开启时从 NVS 记录的曲目索引继续（越界则从头开始），顺序循环播放；自动播放关闭则待机，按键可启动。
5. LED 指示：

   | 状态 | LED 表现 |
   | --- | --- |
   | 正常播放 | 慢闪（500ms） |
   | 无音频文件 / 播放异常 | 快闪（100ms），后者每 5 秒自动重试 |
   | 暂停 / 自动播放关闭待机 | 熄灭 |

6. GPIO9 按键单击切换播放/暂停：播放中按下暂停（冻结曲目进度，I2S 静音），再次按下恢复；未播放时按下会启动播放列表。
7. GPIO9 长按约 1 秒：暂停播放 → 播放下降音提示音 → 重置所有设置为默认值（SPIFFS 文件保留），热点以默认密码重启。
8. 音量变化/切歌时通过事件回调实时写入 NVS，掉电后下次上电恢复。
9. Web 上传/删除文件后自动刷新播放列表；删除正在播放的文件会先停止播放；格式化会先停止播放再清空分区。
10. 网页改密码后热点立即以新密码重启；连接热点的设备自动弹出管理页面（DNS 强制门户）。
11. 有客户端接入热点时自动暂停播放，全部断开后自动恢复（仅恢复因接入而自动暂停的状态；用户手动暂停后不断开则不恢复）。
12. LittleFS 挂载失败时串口打印错误并挂起（`while(1)`）。
