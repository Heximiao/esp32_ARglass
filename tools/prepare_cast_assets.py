"""Embed the browser sender as gzip, using only Python standard libraries."""
from pathlib import Path
import gzip
Import('env')
root = Path(env.subst('$PROJECT_DIR'))
folder = root/'.pio/cast_generated'; folder.mkdir(parents=True, exist_ok=True)
html = (root/'web/cast/index.html').read_text(encoding='utf-8')
html = html.replace('/*__APP__*/', (root/'web/cast/app.js').read_text(encoding='utf-8'))
content = gzip.compress(html.encode('utf-8'), compresslevel=9, mtime=0)
text = '#pragma once\n#include <Arduino.h>\nstatic const uint8_t CastPageGzip[] PROGMEM = {\n'
text += ','.join(str(byte) for byte in content)+'\n};\n'
target = folder/'CastWebPage.h'
if not target.exists() or target.read_text(encoding='utf-8') != text: target.write_text(text, encoding='utf-8')
env.Append(CPPPATH=[str(folder)])
if not (root/'src/cast/private/TlsMaterial.h').exists():
    raise RuntimeError('Run tools/generate_cast_tls.py with Python + cryptography before building HTTPS.')
