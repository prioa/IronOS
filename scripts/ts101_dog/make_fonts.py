"""Generate source/Core/Threads/UI/drawing/mono_128x32/ts101_dog_fonts.h/.cpp: bitmap tables of the fonts in fonts/ (OFL, see the licence files there) and the key row icons."""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'source', 'Core', 'Threads', 'UI', 'drawing', 'mono_128x32')
sys.path.insert(0, os.path.join(HERE, '..', '..', 'Translations'))
import font_tables  # the IronOS 6x8 font
THRESHOLD = 120  # coverage that turns into a lit pixel

DIGITS = '0123456789'
UPPER = ' ABCDEFGHIJKLMNOPQRSTUVWXYZ' + DIGITS + '.:/-+!%°'
SMALL = UPPER + ',()~?·' + 'ÄÖÜ' + 'Solet' + 's' + 'nuibax'  # the lower case is for the alloys: SnBi, SnPb, Max
# name: (ttf, size, characters, pages, row of the baseline inside the glyph)
FONTS = {
    'Big':   ('BarlowCondensed-SemiBold.ttf', 28, DIGITS, 3, 22),  # the large temperature
    'Head':  ('Barlow-SemiBold.ttf', 19, UPPER + 'ÄÖÜ', 3, 22),    # full screen notices
}

def ironos_small():
    """The small row and the menu: the IronOS 6x8 font, columns from the top bit down"""
    table = {**font_tables.get_small_font_map_ascii_basic(), **font_tables.get_small_font_map_latin_extended()}
    glyphs = {}
    for char in dict.fromkeys(SMALL):
        pixels = {(x, bit - 7) for x, col in enumerate(table[char]) for bit in range(8) if col >> bit & 1}
        glyphs[char] = (pixels, 6)
    return glyphs, 1, 7

def label_font():
    """Mixed-case labels, from fonts/label8x10.txt"""
    lines = [l.rstrip('\n') for l in open(os.path.join(HERE, 'fonts', 'label8x10.txt'), encoding='utf8') if not l.startswith('# ')]
    glyphs = {' ': (set(), 3)}
    for i in range(0, len(lines), 14):
        char = lines[i]
        pixels = {(x, y - 10) for y, row in enumerate(lines[i + 1:i + 14]) for x, c in enumerate(row) if c == '#'}
        if char in DIGITS + '°':
            glyphs[char] = (pixels, 8)  # the digits keep their cells: the screens place them one by one
            continue
        left = min(x for x, _ in pixels)  # letters are proportional: no empty columns in front, 1 px gap behind
        pixels = {(x - left, y) for x, y in pixels}
        glyphs[char] = (pixels, max(x for x, _ in pixels) + 2)
    return glyphs, 2, 10

def render(font, char):
    width = int(font.getlength(char)) + 3
    img = Image.new('L', (width, 48), 0)
    ImageDraw.Draw(img).text((0, 36), char, fill=255, font=font, anchor='ls')
    px = img.load()
    return {(x, y - 36) for x in range(width) for y in range(48) if px[x, y] >= THRESHOLD}

def code_of(char):  # one byte per glyph: ASCII, or the second UTF-8 byte of Ä Ö Ü °
    return char.encode('utf8')[-1]

def build(glyphs, pages, baseline):
    codes, offsets, widths, advances, bitmap = [], [], [], [], []
    for char, (pixels, advance) in glyphs.items():
        width = max((x for x, _ in pixels), default=-1) + 1
        codes.append(code_of(char)); offsets.append(len(bitmap)); widths.append(width); advances.append(advance)
        for page in range(pages):
            for col in range(width):
                byte = 0
                for bit in range(8):
                    if (col, page * 8 + bit - baseline) in pixels:
                        byte |= 1 << bit
                bitmap.append(byte)
    return codes, offsets, widths, advances, bitmap

def from_ttf(name, ttf, size, chars, pages, baseline):
    font = ImageFont.truetype(os.path.join(HERE, 'fonts', ttf), size)
    glyphs = {c: (render(font, c), int(font.getlength(c))) for c in chars}
    if name == 'Big':  # the 1 sits one column further right on the boards
        pixels, advance = glyphs['1']
        glyphs['1'] = ({(x + 1, y) for x, y in pixels}, advance)
    return glyphs, pages, baseline

def rows(values, per=24, fmt='0x{:02X}'):
    return ',\n'.join('    ' + ', '.join(fmt.format(v) for v in values[i:i + per]) for i in range(0, len(values), per))

def lock_icon():  # 24x24 padlock, page format
    data = [0] * 72
    def on(x, y): data[(y // 8) * 24 + x] |= 1 << (y % 8)
    for x in range(5, 19):
        for y in range(11, 23): on(x, y)
    for x in range(8, 16):
        on(x, 3); on(x, 4)
    for y in range(4, 11):
        for x in (7, 8, 15, 16): on(x, y)
    for x in (11, 12):
        for y in range(14, 20): data[(y // 8) * 24 + x] &= ~(1 << (y % 8))
    return data

ICONS = {  # key row icons, 8x8 unless noted
    'dogRing':      [0x1E, 0x33, 0x21, 0x33, 0x1E],  # 5x8 degree ring of the large digits
    'iconUpDown':   [0x24, 0x66, 0xE7, 0x66, 0x24, 0, 0, 0],
    'iconInfo':     [0x3C, 0x42, 0x81, 0xF5, 0xF5, 0x81, 0x42, 0x3C],
    'iconMenu':     [0x49] * 7 + [0],
    'iconPlus':     [0x18, 0x18, 0xFF, 0xFF, 0x18, 0x18, 0, 0],
    'iconMinus':    [0x18] * 6 + [0, 0],
    'iconNext':     [0x08, 0x08, 0x08, 0x08, 0x2A, 0x1C, 0x08, 0],
    'iconHot':      [0xE0, 0x98, 0x86, 0xD9, 0x86, 0x98, 0xE0],  # 7x8 warning triangle with '!', the tip is still hot
    'iconReady':    [0x0C, 0x18, 0x30, 0x30, 0x18, 0x0C, 0x06, 0x03],  # the check mark next to "Bereit"
    'iconBack':     [0x08, 0x1C, 0x2A, 0x08, 0x08, 0x08, 0x08, 0],  # iconNext mirrored
    'iconCheck':    [0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02],
    'iconLock':     lock_icon(),
}

def main():
    h = ['// Generated by scripts/ts101_dog/make_fonts.py - do not edit', '#ifndef TS101_DOG_FONTS_H_', '#define TS101_DOG_FONTS_H_', '#include <stdint.h>', '',
         'typedef struct {', '  const uint8_t  *codes;    // per glyph: ASCII, or the second UTF-8 byte of Ä Ö Ü °',
         '  const uint16_t *offsets;  // into bitmap', '  const uint8_t  *widths;   // columns stored', '  const uint8_t  *advances; // pen movement',
         '  const uint8_t  *bitmap;   // page format, pages * width bytes per glyph', '  uint8_t         count;', '  uint8_t         pages;', '  uint8_t         baseline; // row of the baseline inside a glyph',
         '} DogFont;', '']
    c = ['// Generated by scripts/ts101_dog/make_fonts.py - do not edit', '// Barlow and Barlow Condensed are licensed under the SIL OFL 1.1, see scripts/ts101_dog/fonts/', '#include "ts101_dog_fonts.h"', '']
    tables = {name: from_ttf(name, *spec) for name, spec in FONTS.items()}
    tables['Small'] = ironos_small()
    tables['Label'] = label_font()
    for name, (glyphs, pages, baseline) in tables.items():
        codes, offsets, widths, advances, bitmap = build(glyphs, pages, baseline)
        h.append(f'extern const DogFont dogFont{name};')
        c += [f'static const uint8_t {name}Codes[] = {{\n{rows(codes)}}};',
              f'static const uint16_t {name}Offsets[] = {{\n{rows(offsets, 16, "{}")}}};',
              f'static const uint8_t {name}Widths[] = {{\n{rows(widths, 24, "{}")}}};',
              f'static const uint8_t {name}Advances[] = {{\n{rows(advances, 24, "{}")}}};',
              f'static const uint8_t {name}Bitmap[] = {{\n{rows(bitmap)}}};',
              f'const DogFont dogFont{name} = {{{name}Codes, {name}Offsets, {name}Widths, {name}Advances, {name}Bitmap, {len(codes)}, {pages}, {baseline}}};', '']
    h.append('')
    for name, data in ICONS.items():
        h.append(f'extern const uint8_t {name}[{len(data)}];')
        c += [f'const uint8_t {name}[{len(data)}] = {{\n{rows(data)}}};', '']
    h += ['', '#endif // TS101_DOG_FONTS_H_', '']
    open(os.path.join(OUT, 'ts101_dog_fonts.h'), 'w', encoding='utf8').write('\n'.join(h))
    open(os.path.join(OUT, 'ts101_dog_fonts.cpp'), 'w', encoding='utf8').write('\n'.join(c))

if __name__ == '__main__':
    main()
