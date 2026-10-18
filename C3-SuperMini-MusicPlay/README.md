1. 处理文件转换
```
ffmpeg -i input.mp3 \
  -c:a aac \
  -b:a 96k \
  -ar 44100 \
  -ac 1 \
  -movflags +faststart \
  output.aac
```

2. 将文件烧录到文件分区，烧录之前文件需要放到/data目录下
```
pio run --target uploadfs
```

---

也可以通过 Web 页面上传（无需编译烧录）：

1. 设备上电后连接其 WiFi 热点（SSID 为 `C3-Music-XXXX`，XXXX 为设备硬件 ID，默认密码 `88888888`）
2. 连接后管理页面通常自动弹出；未弹出时浏览器打开 `http://192.168.4.1`
3. 在「文件管理」卡片选择 `.aac` 文件上传，上传后自动加入播放列表

Web 页面同时支持删除文件、查看/格式化 SPIFFS 分区、实时调音量、开关开机自动播放、修改热点密码。AP 名称前缀与默认密码在 `include/config.h` 中修改。
