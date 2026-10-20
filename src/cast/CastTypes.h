#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../ui/Viewport.h"
namespace casting {
enum class Format : uint8_t { Jpeg = 1, Rgb565 = 2 };
enum class NetworkMode : uint8_t { Station, AccessPoint };
struct Options {
    Format format = Format::Jpeg;
    uint8_t fps = 30; // 0 = automatic (sender caps at 50)
    uint8_t quality = 70;
    bool fill = false;
    bool showFps = true;
};
constexpr size_t HeaderBytes = 20;
constexpr size_t MaxPayload = 96 * 1024;
constexpr size_t RawBytes = ui::viewport::Width * ui::viewport::Height * 2;
struct FrameInfo { Format format; uint32_t sequence; size_t bytes; };
// Explicit wire layout, never reinterpret a packed network struct.
inline bool parseFrame(const uint8_t* data, size_t size, FrameInfo& info) {
    if (size < HeaderBytes || data[0]!='A' || data[1]!='R' || data[2]!='S' || data[3]!='1') return false;
    const auto u16 = [&](unsigned p) { return unsigned(data[p]) | unsigned(data[p+1]) << 8; };
    const auto u32 = [&](unsigned p) { return uint32_t(data[p]) | uint32_t(data[p+1])<<8 |
        uint32_t(data[p+2])<<16 | uint32_t(data[p+3])<<24; };
    if (data[5] != 0 || u16(6)!=ui::viewport::Width || u16(8)!=ui::viewport::Height || u16(10)!=HeaderBytes) return false;
    info.format = static_cast<Format>(data[4]);
    info.sequence = u32(12); info.bytes = u32(16);
    if (!info.bytes || info.bytes > MaxPayload || info.bytes != size-HeaderBytes) return false;
    if (info.format == Format::Rgb565) return info.bytes == RawBytes;
    if (info.format == Format::Jpeg) return info.bytes >= 4 && data[20]==0xff && data[21]==0xd8;
    return false;
}
}
