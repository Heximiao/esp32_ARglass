# AR Glass — SPI 屏幕 UI

ESP32-S3 N8R8 + ST7789 320×172 横屏，Arduino / PlatformIO / LVGL 8.4.0。

开机显示横向滚动的卡片首页，包括相册、图片反弹动画、浏览器投屏和设置，
同时显示三张卡片，摇杆左右滚动、中键进入。
每个页面独立文件，显示适配、图片读取、公共样式和输入分发也分别拆分。
中文使用完整 Noto Sans SC 字库，30,890 个字符，16 像素、2 位灰度。

完整说明、目录结构、串口操作和字库体积见 [docs/UI.md](docs/UI.md)。
屏幕接线见 [src/spi/README.md](src/spi/README.md)。
投屏首次 HTTPS 设置、浏览器操作和分区变化见 [docs/CASTING.md](docs/CASTING.md)。

## 构建与安装

在本目录运行：

```powershell
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

首次安装需要固件和文件系统；设备已有原来的图片文件时，更新 UI 只需上传固件。
本版固件分区已扩大，首次更新请使用普通 PlatformIO Upload，包含新分区表；图片区位置不变。
构建需要已生成的本地 HTTPS 凭据，缺少时先按投屏文档运行 `tools/generate_cast_tls.py`。
上传文件系统会替换该分区里的文件，不会在挂载失败时自动格式化。
构建脚本已处理 Windows 中文项目路径导致 linker map 写入失败的问题。

## 五维摇杆与串口

COM 接 GND，UP / DOWN / LEFT / RIGHT / MID 分别接 GPIO12 / 13 / 14 / 15 / 16。
方向键移动，中键短按确认，长按约 0.65 秒返回。
已加入 25 ms 双边消抖，方向键长按支持连发。
引脚和时间参数见 `src/input/JoystickConfig.h`。

设置采用上下滚动列表，可调亮度、动画速度和信息显示，并包含“按键调试”。
调试页用红点显示移动方向，同时显示 GPIO 和五个按键的消抖状态；
中键短按回中心，长按返回设置。接线反向时在配置文件中交换对应引脚即可。

USB 串口 115200，发送 `0/1/2/3/4/5` 分别打开首页、相册、动画、投屏、设置、按键调试。
`WASD` 移动，`E` 确认，`B` 返回；回车换行忽略。
串口控制继续保留。设置中新增“投屏设置”，可切换 JPEG / RGB565、目标帧率、画质和显示比例。

## 图片

`data/screensaver.rgb565` 保留原有图片（112×84，18,824 字节），继续用于相册和动画。
转换图片需要 Pillow：

```powershell
python tools/convert_image.py "图片.png"
python tools/convert_image.py "另一张.jpg" --output photo02.rgb565 --size 320 172
pio run --target uploadfs
```

未指定 `--output` 时会替换 `screensaver.rgb565`；源图片不会修改。
本版读取 `data/` 根目录下的 RGB565 文件，按文件名排序。
透明像素合成到黑色背景，图片尺寸最大 320×172。

## 硬件验证

固件编译和文件系统打包可在电脑完成。
实际显示效果、亮度调节、动画帧率和长时间运行仍需要连接设备验证。
