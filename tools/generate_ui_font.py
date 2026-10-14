"""Rasterize ALL mapped characters of a local OpenType font for LVGL 8.

Requires Pillow only. No network requests. Source: Noto Sans SC (OFL).
Example: python tools/generate_ui_font.py .pio/NotoSansSC-Regular.otf --bpp 2
"""
import argparse
import json
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


def mapped_codepoints(path):
    """Read Unicode cmap formats 4/12 without adding a fontTools dependency."""
    data = path.read_bytes()
    u16 = lambda offset: struct.unpack_from('>H', data, offset)[0]
    u32 = lambda offset: struct.unpack_from('>I', data, offset)[0]
    tables = {}
    for index in range(u16(4)):
        offset = 12 + index * 16
        tables[data[offset:offset + 4]] = u32(offset + 8)
    base = tables[b'cmap']
    candidates = []
    for index in range(u16(base + 2)):
        record = base + 4 + index * 8
        platform, encoding = u16(record), u16(record + 2)
        offset = base + u32(record + 4)
        fmt = u16(offset)
        if platform == 0 or (platform == 3 and encoding in (1, 10)):
            if fmt in (4, 12):
                candidates.append((fmt, offset))
    fmt, offset = max(candidates)
    points = set()
    if fmt == 12:
        for index in range(u32(offset + 12)):
            start, end, glyph = struct.unpack_from('>III', data, offset + 16 + index * 12)
            points.update(cp for cp in range(start, end + 1) if glyph + cp - start)
    else:
        count = u16(offset + 6) // 2
        ends = offset + 14
        starts = ends + count * 2 + 2
        deltas = starts + count * 2
        offsets = deltas + count * 2
        for index in range(count):
            for cp in range(u16(starts + index * 2), u16(ends + index * 2) + 1):
                delta = u16(deltas + index * 2)
                relative = u16(offsets + index * 2)
                if relative:
                    glyph = u16(offsets + index * 2 + relative + 2 * (cp - u16(starts + index * 2)))
                    glyph = (glyph + delta) & 0xffff if glyph else 0
                else:
                    glyph = (cp + delta) & 0xffff
                if glyph:
                    points.add(cp)
    return sorted(cp for cp in points if 32 <= cp < 0x110000 and cp != 0xffff)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('font', type=Path)
    parser.add_argument('--bpp', choices=(2, 4), type=int, default=2)
    parser.add_argument('--size', type=int, default=16)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    points = mapped_codepoints(args.font)
    if len(points) >= 65535:
        parser.error('LVGL 8 supports fewer than 65535 glyphs per font')
    font = ImageFont.truetype(str(args.font), args.size)
    bitmap = bytearray()
    descriptors = [(0, 0, 0, 0, 0, 0)]
    bytes_4bpp = 0
    for cp in points:
        char = chr(cp)
        x0, y0, x1, y1 = font.getbbox(char, anchor='ls')
        width, height = x1 - x0, y1 - y0
        pixels = Image.new('L', (max(1, width), max(1, height)))
        ImageDraw.Draw(pixels).text((-x0, -y0), char, font=font, fill=255, anchor='ls')
        values = pixels.tobytes() if width and height else b''
        start = len(bitmap)
        levels = (1 << args.bpp) - 1
        per_byte = 8 // args.bpp
        for index in range(0, len(values), per_byte):
            byte = 0
            for slot, pixel in enumerate(values[index:index + per_byte]):
                byte |= ((pixel * levels + 127) // 255) << (8 - args.bpp * (slot + 1))
            bitmap.append(byte)
        bytes_4bpp += (width * height + 1) // 2
        descriptors.append((start, round(font.getlength(char) * 16), width, height, x0, -y1))

    groups = []
    for index, cp in enumerate(points):
        if not groups or cp - groups[-1][0][1] >= 65535:
            groups.append([])
        groups[-1].append((index + 1, cp))
    target = root / 'src/ui/fonts'
    target.mkdir(parents=True, exist_ok=True)
    with (target / 'ui_font_16.c').open('w', encoding='utf-8', newline='\n') as out:
        out.write('/* Generated from Noto Sans SC, SIL OFL 1.1. See OFL.txt. */\n#include <lvgl.h>\n\n')
        out.write('static const uint8_t bitmap[] = {\n')
        for offset in range(0, len(bitmap), 24):
            out.write('    ' + ','.join(f'0x{byte:02x}' for byte in bitmap[offset:offset + 24]) + ',\n')
        out.write('};\nstatic const lv_font_fmt_txt_glyph_dsc_t glyphs[] = {\n')
        for index, (offset, advance, width, height, dx, dy) in enumerate(descriptors):
            out.write(f'    {{{offset},{advance},{width},{height},{dx},{dy}}},\n')
        out.write('};\n')
        for index, group in enumerate(groups):
            out.write(f'static const uint16_t unicode_{index}[] = {{\n')
            for offset in range(0, len(group), 20):
                out.write('    ' + ','.join(str(cp - group[0][1]) for _, cp in group[offset:offset + 20]) + ',\n')
            out.write('};\n')
        out.write('static const lv_font_fmt_txt_cmap_t cmaps[] = {\n')
        for index, group in enumerate(groups):
            out.write(f'    {{.range_start={group[0][1]},.range_length={group[-1][1]-group[0][1]+1},'
                      f'.glyph_id_start={group[0][0]},.unicode_list=unicode_{index},'
                      f'.list_length={len(group)},.type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}},\n')
        out.write('};\nstatic lv_font_fmt_txt_glyph_cache_t cache;\n')
        out.write('static const lv_font_fmt_txt_dsc_t descriptor = {\n'
                  f'    .glyph_bitmap=bitmap,.glyph_dsc=glyphs,.cmaps=cmaps,.cmap_num={len(groups)},\n'
                  f'    .bpp={args.bpp},.bitmap_format=0,.cache=&cache\n}};\n')
        ascent, descent = font.getmetrics()
        out.write('const lv_font_t ui_font_16 = {\n'
                  '    .get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,\n'
                  '    .get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,\n'
                  f'    .line_height={ascent+descent},.base_line={descent},\n'
                  '    .underline_position=-2,.underline_thickness=1,.dsc=&descriptor\n};\n')
    stats = {
        'family': 'Noto Sans SC Regular', 'size': args.size, 'bpp': args.bpp,
        'glyphs': len(points), 'bitmap_bytes': len(bitmap),
        'estimated_font_bytes': len(bitmap) + len(descriptors) * 16 + len(points) * 2,
        'estimated_4bpp_font_bytes': bytes_4bpp + len(descriptors) * 16 + len(points) * 2,
        'line_height': ascent + descent,
        'basic_cjk_glyphs': sum(0x4e00 <= cp <= 0x9fff for cp in points),
        'extension_a_glyphs': sum(0x3400 <= cp <= 0x4dbf for cp in points),
        'supplementary_glyphs': sum(cp > 0xffff for cp in points),
    }
    (target / 'ui_font_16.json').write_text(json.dumps(stats, indent=2) + '\n', encoding='utf-8')
    used = set()
    for file in (root / 'src/ui').rglob('*.cpp'):
        used.update(ord(char) for char in file.read_text(encoding='utf-8') if '\u3400' <= char <= '\u9fff')
    missing = used - set(points)
    if missing:
        raise RuntimeError('Missing UI glyphs: ' + ''.join(map(chr, sorted(missing))))
    print(json.dumps(stats, indent=2))
    print('All Chinese UI characters covered.')


if __name__ == '__main__':
    main()
