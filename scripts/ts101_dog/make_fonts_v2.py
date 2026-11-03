"""Interface v2: generate source/Core/Threads/UI/drawing/mono_128x32/ts101_v2_fonts.h/.cpp.

- dogFontName: helvB08 from u8g2 (fonts/helvB08.bdf), capital height 8: names, row 2, the menu
- dogFontNum:  helvB10 from u8g2 (fonts/helvB10.bdf), the digits only, all 8 px apart: the large temperature
- dogFont3x5: the 3x5 font in art/font3x5.txt, variable width
- the icons in art/icons_v2.txt

The fonts use the DogFont tables of ts101_dog_fonts.h (page format, one byte per glyph code: ASCII, or the second UTF-8 byte).
The BDF fonts are Copyright Adobe / Digital Equipment Corporation, freely redistributable with their notice: see the header of the
.bdf files. The capital umlauts of helvB08 carry their dots two rows above the capitals; those would leave the 10 rows of a zone,
so they are rebuilt from the plain letter: the dots on the top row, a gap, the letter squashed to 6 rows.
Run: python3 scripts/ts101_dog/make_fonts_v2.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'source', 'Core', 'Threads', 'UI', 'drawing', 'mono_128x32')
NAME_CHARS = ' ' + 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' + 'abcdefghijklmnopqrstuvwxyz' + '0123456789' + "!%+-./:()°,'?" + 'ÄÖÜäöüß'
NUM_CHARS = '0123456789.'  # the point for the menu's tenths (PD timeout, keep awake)


def read_bdf(path):
    """{codepoint: (pixels {(x, y)} with y relative to the baseline, negative up; advance)}"""
    glyphs, cur, bits, rows = {}, None, None, None
    for line in open(path, encoding='latin1'):
        parts = line.split()
        if not parts:
            continue
        key = parts[0]
        if key == 'STARTCHAR':
            cur = {'code': -1}
        elif cur is None:
            continue
        elif key == 'ENCODING':
            cur['code'] = int(parts[1])
        elif key == 'DWIDTH':
            cur['advance'] = int(parts[1])
        elif key == 'BBX':
            cur['bbx'] = tuple(int(p) for p in parts[1:5])
        elif key == 'BITMAP':
            bits = []
        elif key == 'ENDCHAR':
            w, h, xo, yo = cur['bbx']
            pixels = set()
            for row, hexrow in enumerate(bits):
                value = int(hexrow, 16)
                nbits = len(hexrow) * 4
                for x in range(w):
                    if value >> (nbits - 1 - x) & 1:
                        pixels.add((x + xo, -(yo + h - row)))  # the top row of the box is yo + h - 1 above the baseline
            glyphs[cur['code']] = (pixels, cur['advance'])
            cur, bits = None, None
        elif bits is not None:
            bits.append(key)
    return glyphs


def squash_rows(rows, count):
    """Drop rows from the longest runs of equal rows until count are left"""
    rows = list(rows)
    while len(rows) > count:
        best, best_len, i = 0, 0, 0
        while i < len(rows):
            j = i
            while j + 1 < len(rows) and rows[j + 1] == rows[i]:
                j += 1
            if j - i + 1 > best_len:
                best, best_len = i, j - i + 1
            i = j + 1
        del rows[best + best_len // 2]
    return rows


def capital_umlaut(glyphs, letter, umlaut):
    """The dots from the umlaut on row -8, a gap on -7, the letter on rows -6..-1"""
    pixels, advance = glyphs[ord(letter)]
    body = [frozenset(x for x, y in pixels if y == r) for r in range(-8, 0)]
    body = squash_rows(body, 6)
    upix, _ = glyphs[ord(umlaut)]
    top = min(y for _, y in upix)
    dots = {x for x, y in upix if y == top}
    out = {(x, -8) for x in dots}
    for i, row in enumerate(body):
        out |= {(x, -6 + i) for x in row}
    return out, advance


def read_art(path):
    """{name: [rows]} from a file of "=name" blocks"""
    blocks, name = {}, None
    for line in open(path, encoding='utf8'):
        line = line.rstrip('\n')
        if line.startswith('#') and name is None:
            continue
        if line.startswith('='):
            name = line[1:]
            blocks[name] = []
        elif name is not None and line and not line.startswith('# '):
            blocks[name].append(line)
    return blocks


def code_of(char):
    return char.encode('utf8')[-1]


def build(glyphs, pages, baseline):
    codes, offsets, widths, advances, bitmap = [], [], [], [], []
    for char, (pixels, advance) in glyphs.items():
        left = 0
        width = max((x for x, _ in pixels), default=-1) + 1
        codes.append(code_of(char))
        offsets.append(len(bitmap))
        widths.append(width)
        advances.append(advance)
        for page in range(pages):
            for col in range(left, width):
                byte = 0
                for bit in range(8):
                    if (col, page * 8 + bit - baseline) in pixels:
                        byte |= 1 << bit
                bitmap.append(byte)
    return codes, offsets, widths, advances, bitmap


def rows(values, per=24, fmt='0x{:02X}'):
    return ',\n'.join('    ' + ', '.join(fmt.format(v) for v in values[i:i + per]) for i in range(0, len(values), per))


def page_format(art):
    """Rows of # into page format: ceil(h / 8) pages of w bytes"""
    w, h = max(len(r) for r in art), len(art)
    data = [0] * (((h + 7) // 8) * w)
    for y, row in enumerate(art):
        for x, c in enumerate(row):
            if c == '#':
                data[(y // 8) * w + x] |= 1 << (y % 8)
    return w, h, data


def main():
    name_bdf = read_bdf(os.path.join(HERE, 'fonts', 'helvB08.bdf'))
    num_bdf = read_bdf(os.path.join(HERE, 'fonts', 'helvB10.bdf'))

    name = {}
    for c in NAME_CHARS:
        name[c] = name_bdf[ord(c)]
    # Ä Ö Ü as helvB08 has them, the dots two rows above the capitals: rows 2 and 3 have a free row above them. Squashed into the
    # capitals' 8 rows under the codes 0x11-0x13, for the inverted labels and the name row (gridState / gridName swap them in)
    for code, letter, umlaut in ((0x11, 'A', 'ä'), (0x12, 'O', 'ö'), (0x13, 'U', 'ü')):
        name[chr(code)] = capital_umlaut(name_bdf, letter, umlaut)
    for c, (pixels, _) in name.items():
        assert all(-10 <= y <= 1 for _, y in pixels), f'helvB08 {c!r} leaves the rows -10..1'

    num = {}
    report = []
    for c in NUM_CHARS:
        pixels, advance = num_bdf[ord(c)]
        assert all(-11 <= y <= -1 for _, y in pixels), f'helvB10 {c!r} leaves the rows -11..-1'
        if c == '.':
            num[c] = (pixels, advance)
            continue
        if advance != 8:
            report.append(f'helvB10 {c}: advance {advance} set to 8')
        num[c] = (pixels, 8)

    tiny = {' ': (set(), 2)}
    for c, art in read_art(os.path.join(HERE, 'art', 'font3x5.txt')).items():
        assert len(art) == 5, f'3x5 {c!r}: {len(art)} rows'
        pixels = {(x, y - 5) for y, row in enumerate(art) for x, ch in enumerate(row) if ch == '#'}
        tiny[c] = (pixels, max(len(r) for r in art) + 1)

    icons = read_art(os.path.join(HERE, 'art', 'icons_v2.txt'))

    h = ['// Generated by scripts/ts101_dog/make_fonts_v2.py - do not edit', '#ifndef TS101_V2_FONTS_H_', '#define TS101_V2_FONTS_H_', '#include "ts101_dog_fonts.h"', '#include <stdint.h>', '',
         'typedef struct {', '  uint8_t        width, height;', '  const uint8_t *data; // page format', '} DogIcon;', '']
    c = ['// Generated by scripts/ts101_dog/make_fonts_v2.py - do not edit',
         '// helvB08 and helvB10 from u8g2 (tools/font/bdf): Copyright 1984-1989, 1994 Adobe Systems Incorporated,',
         '// Copyright 1988, 1994 Digital Equipment Corporation; permission notice in scripts/ts101_dog/fonts/helvB08.bdf',
         '#include "ts101_v2_fonts.h"', '']
    sizes = {}
    # baseline: the row of the baseline inside the glyph. Name: rows -10..1 -> 0..11; Num: -11..-1 -> 0..10; 3x5: -5..-1 -> 0..4
    for label, glyphs, pages, baseline in (('Name', name, 2, 10), ('Num', num, 2, 11), ('3x5', tiny, 1, 5)):
        codes, offsets, widths, advances, bitmap = build(glyphs, pages, baseline)
        assert len(set(codes)) == len(codes), f'{label}: two glyphs share a code'
        sizes[label] = len(codes) * 5 + len(bitmap)
        h.append(f'extern const DogFont dogFont{label};')
        c += [f'static const uint8_t V2{label}Codes[] = {{\n{rows(codes)}}};',
              f'static const uint16_t V2{label}Offsets[] = {{\n{rows(offsets, 16, "{}")}}};',
              f'static const uint8_t V2{label}Widths[] = {{\n{rows(widths, 24, "{}")}}};',
              f'static const uint8_t V2{label}Advances[] = {{\n{rows(advances, 24, "{}")}}};',
              f'static const uint8_t V2{label}Bitmap[] = {{\n{rows(bitmap)}}};',
              f'const DogFont dogFont{label} = {{V2{label}Codes, V2{label}Offsets, V2{label}Widths, V2{label}Advances, V2{label}Bitmap, {len(codes)}, {pages}, {baseline}}};', '']
    h.append('')
    for icon, art in icons.items():
        w, hh, data = page_format(art)
        assert w <= 9 and hh <= 11, f'{icon} is {w}x{hh}'
        h.append(f'extern const DogIcon {icon}; // {w}x{hh}')
        c += [f'static const uint8_t {icon}Data[] = {{\n{rows(data)}}};', f'const DogIcon {icon} = {{{w}, {hh}, {icon}Data}};', '']
        sizes[icon] = len(data) + 6
    h += ['', '#endif // TS101_V2_FONTS_H_', '']
    open(os.path.join(OUT, 'ts101_v2_fonts.h'), 'w', encoding='utf8').write('\n'.join(h))
    open(os.path.join(OUT, 'ts101_v2_fonts.cpp'), 'w', encoding='utf8').write('\n'.join(c))
    print('bytes (approx):', ', '.join(f'{k} {v}' for k, v in sizes.items()), '- total', sum(sizes.values()))
    for line in report:
        print(line)


if __name__ == '__main__':
    main()
