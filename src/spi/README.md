# ST7789P3 172x320

Wiring: GND -> G, VDD -> 3V3, SCL -> GPIO1, SDA -> GPIO2,
RES -> GPIO3, DC -> GPIO4, CS -> GPIO5, BL -> GPIO6.
BL must be a logic control input. GPIO3 affects JTAG selection at reset.

Defaults: landscape 320x172, RGB565, mode 0, 40MHz, row offset 34, inversion on.
Exact supplier initialization has not been provided. Analog settings and offset
are a baseline requiring hardware validation. Adjust Config::xOffset for shifted
edges (xOffset/yOffset), bgr for swapped red/blue, inverted for incorrect display polarity.

Driver owns SPI2. Two 10240-byte internal DMA buffers overlap conversion and
transmission. drawRect accepts packed native uint16_t RGB565 in SRAM or PSRAM.
Public calls block until DMA completion; source can be reused on return.
Call from one task only. end releases the device, bus and buffers.

main.cpp starts the LVGL UI, with pages and asset loading in src/ui.
USB serial 115200 accepts preview controls; see ../../docs/UI.md.
Backlight uses LEDC channel 0, 20kHz, 8-bit duty, default 75% brightness.
Updates are paced at 30 FPS; this is a target, not a hardware-verified result.
40MHz pixel-only transfer time is 22.016ms. No TE synchronization is available.
Browser casting uses HTTPS/WSS in src/cast; see ../../docs/CASTING.md.
Joystick GPIO12..16 is handled in src/input. Display access remains single-task.

Build using PlatformIO: pio run.
