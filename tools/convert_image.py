"""Convert a source image into a packed little-endian RGB565 screensaver asset."""
import argparse
import struct
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('--output', default='screensaver.rgb565', help='Filename inside data/ (default: screensaver.rgb565)')
parser.add_argument('--size', nargs=2, type=int, default=(112, 112), metavar=('WIDTH', 'HEIGHT'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
output = (root / 'data' / args.output).resolve()
if output.parent != (root / 'data').resolve() or output.suffix != '.rgb565':
    parser.error('--output must be a .rgb565 filename directly inside data/')
if not (1 <= args.size[0] <= 320 and 1 <= args.size[1] <= 172):
    parser.error('--size must fit the 320x172 screen')
image = Image.open(args.source).convert('RGBA')
image.thumbnail(tuple(args.size), Image.Resampling.LANCZOS)
background = Image.new('RGBA', image.size, (0, 0, 0, 255))
image = Image.alpha_composite(background, image).convert('RGB')
data = bytearray(b'R565' + struct.pack('<HH', *image.size))
for r, g, b in image.getdata():
    data.extend(struct.pack('<H', ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)))
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(data)
print(f'{image.width}x{image.height}, {len(data)} bytes -> {output}')
