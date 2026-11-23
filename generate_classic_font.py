#!/usr/bin/env python3
"""Generate the compact UI's three DejaVu Sans bitmap subsets."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument('font', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
codepoints = list(range(32, 127)) + list(range(160, 256))
lines = ['// Generated from DejaVu Sans; see FONT-LICENSE.txt.', '#pragma once',
         '#include <cstdint>', 'namespace classic::font {',
         'struct Face { int height, line; const std::uint8_t *advance; const std::uint16_t *bits; };']
for name, size, height, leading in [('small', 11, 14, 16), ('body', 14, 18, 20), ('heading', 16, 20, 24)]:
    face = ImageFont.truetype(str(args.font), size)
    advances, rows = [], []
    for codepoint in codepoints:
        char = chr(codepoint)
        bitmap = Image.new('1', (16, height))
        ImageDraw.Draw(bitmap).text((0, -1), char, font=face, fill=1)
        rows.append([sum(0x8000 >> x for x in range(16) if bitmap.getpixel((x, y))) for y in range(height)])
        advances.append(min(16, max(1, round(face.getlength(char)))))
    lines.append('inline constexpr std::uint8_t ' + name + '_advance[] = {' + ','.join(map(str, advances)) + '};')
    lines.append('inline constexpr std::uint16_t ' + name + '_bits[] = {')
    lines += ['  ' + ','.join(f'0x{value:04x}' for value in glyph) + ',' for glyph in rows]
    lines += ['};', f'inline constexpr Face {name}' + '{' + f'{height}, {leading}, {name}_advance, {name}_bits' + '};']
lines += ['} // namespace classic::font', '']
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open('w', encoding='utf-8', newline='\n') as stream:
    stream.write('\n'.join(lines))
