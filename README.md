# AR Glass — SPI 屏幕 UI

ESP32-S3 N8R8 + ST7789 320×172 横屏，Arduino / PlatformIO / LVGL 8.4.0。

开机显示深色卡片首页，包括相册、图片反弹动画、投屏占位和设置。
每个页面独立文件，显示适配、图片读取、公共样式和输入分发也分别拆分。
中文使用完整 Noto Sans SC 字库，30,890 个字符，16 像素、2 位灰度。

完整说明、目录结构、串口操作和字库体积见 [docs/UI.md](docs/UI.md)。
屏幕接线见 [src/spi/README.md](src/spi/README.md)。

## 构建与安装

在本目录运行：

```powershell
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

首次安装需要固件和文件系统；设备已有原来的图片文件时，更新 UI 只需上传固件。
上传文件系统会替换该分区里的文件，不会在挂载失败时自动格式化。
构建脚本已处理 Windows 中文项目路径导致 linker map 写入失败的问题。

## 暂无摇杆时切换页面

USB 串口 115200，发送 `0/1/2/3/4` 分别打开首页、相册、动画、投屏、设置。
`WASD` 移动，`E` 确认，`B` 返回；回车换行忽略。
本版没有摇杆 GPIO 和网络功能，输入接口已预留。

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
