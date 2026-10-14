# AR Glass UI 第一版

使用 LVGL **8.4.0**，适配现有 ST7789 **320×172 横屏**和原来的 RGB565 图片。
SPI 引脚、偏移、颜色顺序保持现有配置。开机进入首页。

## AR 镜片显示范围

界面采用 **256×172** 的居中布局，左右各留 32 像素黑边，
高度保持 172 像素。卡片和列表按较窄的可视区域重新排列，
中文字体仍为原始 16 像素，英文和图标也保持原始尺寸。
显示适配层仅平移坐标，不压缩或重采样画面，避免文字笔画丢失。

在 `src/ui/Viewport.h` 修改 `WidthPercent` 后重新编译、上传固件即可调整。
例如 `85` 增加布局宽度，`75` 留出更多左右边距，`100` 使用满屏宽度。
此参数只改变布局宽度，不改变字体大小或高度。可用范围为 75～100。
具体适合的宽度需要佩戴镜片实测。

## 页面

- 首页：深色卡片菜单，相册 / 动画 / 投屏，右上角设置入口。
- 相册：两列两行缩略图，自动翻页；进入图片后等比例放大查看。
  只显示真实文件，当前只有 `screensaver.rgb565` 时就是一张图片。
- 动画：选择相册图片，碰壁反弹，暂停 / 继续、三档速度、隐藏信息全屏展示。
  目标更新间隔 33 ms，实际帧率需要屏幕实测。
- 投屏：连接 Wi-Fi / 设备热点两个入口和开发中提示。
  本版不会启动 Wi-Fi、热点或画面接收。
- 设置：屏幕亮度、动画速度、动画信息开关、设备型号及分辨率。
  亮度使用 GPIO6 的 LEDC PWM，默认 75%，调整范围 15%～100%。
  LEDC 通道 0 用于背光，后续其他外设应避免占用同一通道。
  设置只保存在内存，重启恢复默认。

## 无摇杆时预览

打开 USB 串口，波特率 **115200**，发送以下字符即可。
可以通过串口工具直接输入字符，也可以发送一行字符；回车换行会忽略。

| 字符 | 动作 |
| --- | --- |
| `0` | 首页 |
| `1` | 相册 |
| `2` | 图片动画 |
| `3` | 投屏占位页 |
| `4` | 设置 |
| `w` / `s` / `a` / `d` | 上 / 下 / 左 / 右 |
| `e` | 确认 |
| `b` | 返回 |
| `?` | 串口打印帮助 |

首页 `w` 选设置，`a/d` 选主卡片，`e` 进入。
相册 `e` 打开图片，`a/d` 切图，`b` 返回缩略图，再次 `b` 返回首页。
动画 `e` 暂停，`a/d` 换图片，`s` 切速度，`w` 显示/隐藏信息，`b` 返回。
设置 `w/s` 选行，`a/d` 改值；`e` 也可以增加/切换值。

摇杆没有接线或轮询代码。下一版由硬件适配层调用
`ui::input(ui::Input::Up/Down/Left/Right/Confirm/Back)`。
所有 UI 和屏幕调用应在同一个任务执行；其他任务通过队列传事件。

## 文件组织

```text
include/lv_conf.h              LVGL 配置
src/main.cpp                  启动和主循环
src/spi/St7789.*               原屏幕驱动及背光 PWM
src/ui/DisplayPort.*           LVGL 显示缓冲、SPI 刷新和时钟
src/ui/UiApp.*                 页面切换与事件分发
src/ui/UiTypes.h               页面、输入事件、设置数据
src/ui/Widgets.*               统一颜色、卡片、文字、图片布局
src/ui/PhotoStore.*            LittleFS 图片校验与 PSRAM 图片缓存
src/ui/SerialPreview.*         USB 串口预览输入
src/ui/pages/HomePage.*        首页
src/ui/pages/AlbumPage.*       相册
src/ui/pages/AnimationPage.*   图片反弹动画
src/ui/pages/CastPage.*        投屏占位
src/ui/pages/SettingsPage.*    设置
```

## 图片与上传

本版读取 LittleFS 根目录下的 `.rgb565` 文件，格式仍是
`R565` + 小端宽高（各 2 字节）+ 小端 RGB565 像素。
图片最大 320×172，等比例缩放显示；动画自动缩小至 112×100 范围。
最多缓存 24 张、像素总计不超过 2 MiB。当前 Flash 文件系统约 1.56 MiB，
实际可容纳数量取决于图片尺寸与文件系统开销。
文件损坏会跳过，缺少图片或文件系统挂载失败仍可显示菜单，不会自动格式化。

保留原图片的转换方式：

```powershell
python tools/convert_image.py "你的图片.png"
```

增加相册图片，使用新的文件名：

```powershell
python tools/convert_image.py "另一张图片.jpg" --output photo02.rgb565 --size 320 172
```

`--output` 必须是 `data/` 根目录中的 `.rgb565` 文件名。
同名文件会被覆盖。源图片不会修改；透明区域仍合成到黑色背景。

在项目目录构建和上传：

```powershell
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

已有 `screensaver.rgb565` 在设备上时，更新 UI 只需上传固件。
新增或更换图片时再上传文件系统；`uploadfs` 会整体替换该分区内容。

## LVGL 接入说明

默认使用一个 256×32（16 KiB）的 PSRAM 绘图缓冲，按脏区域刷屏，不强制每次刷新整屏。
驱动内部的两个 DMA 缓冲继续负责像素字节转换和 SPI 传输。
`drawRect()` 阻塞至 DMA 完成后再调用 `lv_disp_flush_ready()`，
避免 LVGL 提前复用还在传输的数据。
`millis()` 差值更新 LVGL tick，动画也按真实经过时间移动。

## 完整中文字库

中文和图片文件名使用 Noto Sans SC Regular，SIL OFL 1.1 开源许可证
保存在 `src/ui/fonts/OFL.txt`。包含源字体映射的全部 **30,890 个字符**，
其中基本汉字区 20,976 个、扩展 A 区 6,582 个、补充平面字符 445 个。
包含简体、繁体及部分生僻字；不代表覆盖 Unicode 中所有罕见汉字。

字库固定 16 像素、2 位灰度（4 档透明度），约 **2.40 MiB**，
编译进 Flash，通过映射直接读取，不会整份加载到 RAM 或 PSRAM。
英文小字和图标使用 LVGL 内置 Montserrat 字体。
字体统计保存在 `src/ui/fonts/ui_font_16.json`。

字体 C 文件已经生成，正常编译不需要下载字体或安装额外字体工具。
若要重新生成，下载公开的 `NotoSansSC-Regular.otf`，然后在本地运行：

```powershell
python tools/generate_ui_font.py "NotoSansSC-Regular.otf" --bpp 2
```

源字体：[Noto CJK 官方仓库](https://github.com/notofonts/noto-cjk/tree/main/Sans/SubsetOTF/SC)。
生成器只依赖 Pillow，不访问网络，会校验菜单汉字是否全部覆盖。

本版完整字库固件编译占用约 **2.96 MiB / 3.19 MiB**，
当前 OTA 双固件槽和 LittleFS 分区不变。后续加入网络功能时应重新检查体积。
4 位灰度完整字体估计约 4.27 MiB，仅字体就超过当前单个固件槽，
因此本版选择 2 位灰度。

网络配网、软键盘、投屏接收和摇杆接入留到后续版本。
