"""Generate source/Core/Threads/UI/drawing/mono_128x32/ts101_dog_sprites.h from art/*.png, art/home.txt and the drawings in gsd_extra.py."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
from gsd32 import lying, running
from gsd_extra import triangle, FLAME, ARROW_DOWN_8, THERMO_16, THERMO_8, BOLT, PAW, PLAY_8, GEAR_8

ART = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'art')

def art(name):  # 1 bit master made from the photo, see art/README.md
    return Image.open(os.path.join(ART, name)).convert('L').point(lambda v: 255 if v > 127 else 0)

def edit(img, on=(), off=()):  # pixel edits on a copy; rectangles are (x0, y0, x1, y1), inclusive
    out = img.copy()
    for value, rects in ((0, off), (255, on)):
        for x0, y0, x1, y1 in rects:
            out.paste(value, (x0, y0, x1 + 1, y1 + 1))
    return out

# Poses of the head (kopf_b.png, 34x32)
def head_bark(head):
    return edit(head, off=[(12, 25, 22, 31)], on=[(14, 29, 19, 31)])

def head_pant(head):  # tongue further out
    return edit(head, on=[(14, 29, 19, 31)])

def head_wag(head):  # happy: the right ear flicks
    return edit(head, off=[(26, 0, 28, 2)])

RUN_FRAMES = 4

ZZZ_STEPS = [  # sleep "z Z Z" animation, 24x8, one page tall
    [(0, 4, 'z')],
    [(0, 4, 'z'), (7, 2, 'Z')],
    [(0, 4, 'z'), (7, 2, 'Z'), (15, 0, 'ZZ')],
]
GLYPHS = {
    'z':  ["####", "..#.", ".#..", "####"],
    'Z':  ["#####", "...#.", "..#..", ".#...", "#####"],
    'ZZ': ["#######", ".....#.", "....#..", "...#...", "..#....", ".#.....", "#######"],
}

FONT3X5 = {
    'W': ["#.#.#", "#.#.#", "#.#.#", "#.#.#", ".#.#."], 'U': ["#..#", "#..#", "#..#", "#..#", ".##."],
    'F': ["###", "#..", "##.", "#..", "#.."], '!': ["#", "#", "#", ".", "#"],
}

def wuff():  # "WUFF!" speech, 20x8, text on rows 1..5
    img = Image.new('L', (20, 8), 0); x = 0
    for c in 'WUFF!':
        for y, row in enumerate(FONT3X5[c]):
            for i, ch in enumerate(row):
                if ch == '#': img.putpixel((x + i, y + 1), 255)
        x += len(FONT3X5[c][0]) + 1
    return img

CLOCK = ["..###..", ".#...#.", "#..#..#", "#..##.#", "#.....#", ".#...#.", "..###.."]

def clock():  # 8x8 stopwatch icon
    img = Image.new('L', (8, 8), 0)
    for y, row in enumerate(CLOCK):
        for x, c in enumerate(row):
            if c == '#': img.putpixel((x, y + 1), 255)
    return img

# 5x7 label font, one string per row; drawn with 1 px spacing (6 px advance)
FONT5X7 = {
    ' ': ["", "", "", "", "", "", ""],
    'A': [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'B': ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    'C': [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    'D': ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    'E': ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    'F': ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    'G': [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
    'H': ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'I': [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    'J': ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    'K': ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'M': ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    'N': ["#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'P': ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    'Q': [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'T': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    'U': ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'V': ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    'W': ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."],
    'X': ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    'Y': ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    'Z': ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
    'Ä': ["#...#", "", ".###.", "#...#", "#####", "#...#", "#...#"],
    'Ö': ["#...#", "", ".###.", "#...#", "#...#", "#...#", ".###."],
    'Ü': ["#...#", "", "#...#", "#...#", "#...#", "#...#", ".###."],
    '?': [".###.", "#...#", "....#", "...#.", "..#..", "", "..#.."],
    '0': [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    '1': ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    '2': [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    '3': ["####.", "....#", "....#", ".###.", "....#", "....#", "####."],
    '4': ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    '5': ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    '6': [".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."],
    '7': ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    '8': [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    '9': [".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."],
    '.': ["", "", "", "", "", "", "..#.."],
    ':': ["", "..#..", "", "", "", "..#..", ""],
    '-': ["", "", "", "#####", "", "", ""],
    '/': ["....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."],
    '!': ["..#..", "..#..", "..#..", "..#..", "..#..", "", "..#.."],
    '+': ["", "..#..", "..#..", "#####", "..#..", "..#..", ""],
    '%': ["##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##"],
    '°': [".##..", "#..#.", "#..#.", ".##..", "", "", ""],
}

def label_font():  # glyph columns, 5 bytes per glyph, bit 0 = top row
    out = []
    for ch, rows in FONT5X7.items():
        for x in range(5):
            b = 0
            for y, r in enumerate(rows):
                if x < len(r) and r[x] == '#': b |= 1 << y
            out.append(b)
    return out

SIGMA = ["######", "#.....", ".#....", "..#...", ".#....", "#.....", "######"]

def sigma():  # 8x8 sum sign for the lifetime total
    img = Image.new('L', (8, 8), 0)
    for y, row in enumerate(SIGMA):
        for x, c in enumerate(row):
            if c == '#': img.putpixel((x + 1, y + 1), 255)
    return img

def zzz(step):
    img = Image.new('L', (24, 8), 0)
    for x, y, g in ZZZ_STEPS[step]:
        for dy, row in enumerate(GLYPHS[g]):
            for dx, c in enumerate(row):
                if c == '#': img.putpixel((x + dx, y + dy), 255)
    return img

def columns(img):
    px = img.load(); out = []
    for page in range(img.height // 8):
        for x in range(img.width):
            b = 0
            for bit in range(8):
                if px[x, page * 8 + bit]: b |= 1 << bit
            out.append(b)
    return out

def c_array(name, img):
    data = columns(img)
    lines = [', '.join(f'0x{b:02X}' for b in data[i:i + 16]) for i in range(0, len(data), 16)]
    return f'// {img.width}x{img.height}\nconst uint8_t {name}[] = {{\n  ' + ',\n  '.join(lines) + ',\n};\n'

def rows_image(rows):  # '#' rows to an image; the height is padded to whole pages, which is what dogBlit takes
    img = Image.new('L', (len(rows[0]), (len(rows) + 7) // 8 * 8), 0)
    for y, row in enumerate(rows):
        assert len(row) == img.width, row
        for x, c in enumerate(row):
            if c == '#': img.putpixel((x, y), 255)
    return img

def read_home():
    """art/home.txt: the home screen's bitmaps as (name, image) and its 3x5 font as {character: rows}"""
    bitmaps, tiny, current = [], {}, None
    for line in open(os.path.join(ART, 'home.txt'), encoding='utf8'):
        line = line.strip()
        if not line or line.startswith('# '):
            continue
        if line.startswith('@'):
            name, width, height = line[1:].split()
            current = (name, int(width), int(height), [])
            bitmaps.append(current)
        elif line.startswith('tiny '):
            _, char, *rows = line.split()
            assert len(rows) == 5 and all(len(r) == 3 for r in rows), line
            tiny[char] = rows
        else:
            current[3].append(line)
    out = []
    for name, width, height, rows in bitmaps:
        assert len(rows) == height and all(len(r) == width for r in rows), name
        out.append((name, rows_image(rows)))
        if name == 'dogTrendDown':
            out.append(('dogTrendUp', rows_image(rows[::-1])))  # the same rows the other way round
    return out, tiny

def tiny_font(tiny):  # the 3x5 font as a DogFont, so dogText and dogTextWidth can use it
    tiny = {' ': ['...'] * 5, **tiny}  # a narrower gap than a blank glyph would leave: see the advances
    codes = [ord(c) if ord(c) < 128 else c.encode()[1] for c in tiny]
    bitmap = []
    for rows in tiny.values():
        for x in range(3):
            bitmap.append(sum(1 << y for y, row in enumerate(rows) if row[x] == '#'))
    n = len(tiny)
    fmt = lambda values, f='0x{:02X}': ', '.join(f.format(v) for v in values)
    return [f'static const uint8_t TinyCodes[] = {{{fmt(codes)}}};',
            f'static const uint16_t TinyOffsets[] = {{{fmt([i * 3 for i in range(n)], "{}")}}};',
            f'static const uint8_t TinyWidths[] = {{{fmt([0] + [3] * (n - 1), "{}")}}};',
            f'static const uint8_t TinyAdvances[] = {{{fmt([2] + [4] * (n - 1), "{}")}}};',
            f'static const uint8_t TinyBitmap[] = {{{fmt(bitmap)}}};',
            f'const DogFont dogFontTiny = {{TinyCodes, TinyOffsets, TinyWidths, TinyAdvances, TinyBitmap, {n}, 1, 5}};', '']

# The large digits of the soldering screen: 11x18, closed seven segments as column and row ranges, drawn for the pixel grid
SEGMENTS = {'a': (0, 10, 0, 2), 'g': (0, 10, 8, 10), 'd': (0, 10, 15, 17), 'f': (0, 2, 0, 10), 'e': (0, 2, 8, 17), 'b': (8, 10, 0, 10), 'c': (8, 10, 8, 17)}
SEG_DIGITS = ['abcdef', 'bc', 'abged', 'abgcd', 'fgbc', 'afgcd', 'afgedc', 'abc', 'abcdefg', 'abcdfg']
SEG_WIDTH, SEG_HEIGHT, SEG_GAP = 11, 18, 2

def seg_pixels(segments, flag):
    pixels = {(x, y) for s in segments for x in range(SEGMENTS[s][0], SEGMENTS[s][1] + 1) for y in range(SEGMENTS[s][2], SEGMENTS[s][3] + 1)}
    pixels -= {(0, 0), (SEG_WIDTH - 1, 0), (0, SEG_HEIGHT - 1), (SEG_WIDTH - 1, SEG_HEIGHT - 1)}  # softly rounded corners
    return pixels | flag

def seg_font():  # as a DogFont, so dogText and dogTextWidth can use it
    bitmap = []
    for digit, segments in enumerate(SEG_DIGITS):
        pixels = seg_pixels(segments, {(7, 0), (7, 1), (6, 1)} if digit == 1 else set())
        for page in range(3):
            for x in range(SEG_WIDTH):
                bitmap.append(sum(1 << bit for bit in range(8) if (x, page * 8 + bit) in pixels))
    fmt = lambda values, f='0x{:02X}': ', '.join(f.format(v) for v in values)
    return [f'static const uint8_t SegCodes[] = {{{fmt(range(0x30, 0x3A))}}};',
            f'static const uint16_t SegOffsets[] = {{{fmt([i * 33 for i in range(10)], "{}")}}};',
            f'static const uint8_t SegWidths[] = {{{fmt([SEG_WIDTH] * 10, "{}")}}};',
            f'static const uint8_t SegAdvances[] = {{{fmt([SEG_WIDTH + SEG_GAP] * 10, "{}")}}};',
            f'static const uint8_t SegBitmap[] = {{{fmt(bitmap)}}};',
            f'const DogFont dogFontSeg = {{SegCodes, SegOffsets, SegWidths, SegAdvances, SegBitmap, 10, 3, {SEG_HEIGHT}}};', '']

def sprites():
    head = art('kopf_b.png')
    s = [('dogHead', head), ('dogHeadWag', head_wag(head)), ('dogHeadPant', head_pant(head)), ('dogHeadBark', head_bark(head)),
         ('dogLying', lying()), ('dogLyingAwake', lying(awake=True)),
         ('dogWuff', wuff()), ('dogClock', clock()), ('dogSigma', sigma()), ('iconFlame', FLAME), ('iconArrowDown8', ARROW_DOWN_8),
         ('iconWarning', triangle()), ('iconThermo16', THERMO_16), ('iconThermo8', THERMO_8),
         ('iconBolt', BOLT), ('iconPaw', PAW), ('iconPlay8', PLAY_8), ('iconGear8', GEAR_8)]
    return s + [(f'dogRun{i}', running(i)) for i in range(RUN_FRAMES)] + [(f'dogZzz{i}', zzz(i)) for i in range(len(ZZZ_STEPS))]

if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    target = os.path.join(here, '..', '..', 'source', 'Core', 'Threads', 'UI', 'drawing', 'mono_128x32', 'ts101_dog_sprites')
    home, tiny = read_home()
    sp = sprites() + home
    hdr = ['// Generated by scripts/ts101_dog/make_sprites.py - do not edit', '#ifndef TS101_DOG_SPRITES_H_',
           '#define TS101_DOG_SPRITES_H_', '#include "ts101_dog_fonts.h"', '#include <stdint.h>', '',
           '#define DOG_LYING_WIDTH  72', '#define DOG_LYING_HEIGHT 24', '#define DOG_WUFF_WIDTH   20', '#define DOG_CLOCK_WIDTH  8',
           '#define DOG_ZZZ_WIDTH    24', '#define DOG_HEAD_WIDTH   34', '#define DOG_HEAD_HEIGHT  32', '#define DOG_RUN_WIDTH    60', '#define DOG_RUN_HEIGHT   24', f'#define DOG_RUN_FRAMES   {RUN_FRAMES}', f'#define DOG_ZZZ_STEPS    {len(ZZZ_STEPS)}', '']
    hdr += [f'extern const uint8_t {name}[];' for name, _ in sp]
    hdr += ['extern const uint8_t *const dogZzz[DOG_ZZZ_STEPS];', 'extern const uint8_t *const dogRun[DOG_RUN_FRAMES];', '',
            '// 5x7 label font: glyphs in the order of dogLabelChars (UTF-8 for the umlauts and degree sign)',
            f'#define DOG_LABEL_GLYPHS {len(FONT5X7)}', 'extern const char *const dogLabelChars[DOG_LABEL_GLYPHS];',
            'extern const uint8_t dogLabelFont[DOG_LABEL_GLYPHS * 5];', '',
            '// 3x5 labels of the home screen, 4 px advance, from art/home.txt', 'extern const DogFont dogFontTiny;', '',
            '// The large digits of the soldering screen, 11x18', 'extern const DogFont dogFontSeg;', '', '#endif // TS101_DOG_SPRITES_H_', '']
    src = ['// Generated by scripts/ts101_dog/make_sprites.py - do not edit', '#include "configuration.h"',
           '#ifdef TS101_DOG_UI', '#include "ts101_dog_sprites.h"', '']
    src += [c_array(name, img) for name, img in sp]
    src += ['const uint8_t *const dogZzz[DOG_ZZZ_STEPS] = {' + ', '.join(f'dogZzz{i}' for i in range(len(ZZZ_STEPS))) + '};', '']
    src += ['const uint8_t *const dogRun[DOG_RUN_FRAMES] = {' + ', '.join(f'dogRun{i}' for i in range(RUN_FRAMES)) + '};', '']
    chars = ', '.join('"' + ''.join(f'\\x{b:02X}' for b in c.encode()) + '"' for c in FONT5X7)
    src += ['const char *const dogLabelChars[DOG_LABEL_GLYPHS] = {' + chars + '};']
    font = label_font()
    src += ['const uint8_t dogLabelFont[DOG_LABEL_GLYPHS * 5] = {'] + \
           ['  ' + ', '.join(f'0x{b:02X}' for b in font[i:i + 20]) + ',' for i in range(0, len(font), 20)] + ['};', ''] + tiny_font(tiny) + seg_font() + ['#endif', '']
    open(target + '.h', 'w').write('\n'.join(hdr))
    open(target + '.cpp', 'w').write('\n'.join(src))
    print('wrote', os.path.normpath(target) + '.h/.cpp')
