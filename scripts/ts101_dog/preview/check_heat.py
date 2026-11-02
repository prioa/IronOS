"""Checks the soldering screen pixel by pixel against the spec, with its own copy of the digit definition: check_heat.py <rendered dir>"""
import os, sys

out = sys.argv[1]

def load(name):
    with open(os.path.join(out, name + '.pbm')) as f:
        tokens = f.read().split()
    width, height, bits = int(tokens[1]), int(tokens[2]), tokens[3:]
    return {(x, y) for y in range(height) for x in range(width) if bits[y * width + x] == '1'}

def area(pixels, x0, x1, y0=0, y1=31):
    return {(x, y) for x, y in pixels if x0 <= x <= x1 and y0 <= y <= y1}

failures = []
def check(ok, text):
    print(('ok    ' if ok else 'FAIL  ') + text)
    if not ok:
        failures.append(text)

SEG = {'a': (0, 10, 0, 2), 'g': (0, 10, 8, 10), 'd': (0, 10, 15, 17), 'f': (0, 2, 0, 10), 'e': (0, 2, 8, 17), 'b': (8, 10, 0, 10), 'c': (8, 10, 8, 17)}
DIGITS = dict(zip('0123456789', ['abcdef', 'bc', 'abged', 'abgcd', 'fgbc', 'afgcd', 'afgedc', 'abc', 'abcdefg', 'abcdfg']))
RING = {(1, 0), (2, 0), (0, 1), (3, 1), (0, 2), (3, 2), (1, 3), (2, 3)}
TICK = ['......#', '.....##', '#...##.', '##.##..', '.###...', '..#....', '.......']
UP, DOWN = ['..#..', '.###.', '#.#.#', '..#..', '..#..'], ['..#..', '..#..', '#.#.#', '.###.', '..#..']

def digit(ch, x0):
    pixels = {(x, y) for s in DIGITS[ch] for x in range(SEG[s][0], SEG[s][1] + 1) for y in range(SEG[s][2], SEG[s][3] + 1)}
    pixels -= {(0, 0), (10, 0), (0, 17), (10, 17)}
    if ch == '1':
        pixels |= {(7, 0), (7, 1), (6, 1)}
    return {(x + x0, y) for x, y in pixels}

def bitmap(rows, x0, y0):
    return {(x0 + x, y0 + y) for y, row in enumerate(rows) for x, c in enumerate(row) if c == '#'}

def number(text):
    pixels = set()
    for i, ch in enumerate(text):
        pixels |= digit(ch, i * 13)
    return pixels

def big_block(name, text):
    """The number with its ring: the pixels above the sparkline left of the readings"""
    width = len(text) * 13 - 2
    img = load(name)
    expected = number(text) | {(x + width + 1, y) for x, y in RING}
    return area(img, 0, width + 4, 0, 22) == expected, img, width

SCENES = {'n1_aufheizen_170': '170', 'n2_kurz_vor_ziel': '338', 'n3_bereit': '350', 'n4_lotwechsel': '358', 'n5_abkuehlen': '345', 'n7_start': '26', 'n7b_keine_schaetzung': '200', 'n8_snbi_bereit': '240', 'n8c_masse_lange': '400', 'n9c_ziffer_9': '199'}
seen = set()
for name, text in SCENES.items():
    ok, _, _ = big_block(name, text)
    check(ok, f'{name}: {text} with the ring pixel by pixel, 2 px between the digits, 1 px to the ring')
    seen |= set(text)
check(seen == set(DIGITS), 'all ten digits were checked')

# Next to the number, 2 px after the ring
def right_of_number(name, width):
    return area(load(name), width + 5, 62, 0, 22)
x = 37 + 1 + 4 + 2
check(right_of_number('n1_aufheizen_170', 37) == bitmap(UP, x, 12), 'heating: arrow up 2 px after the ring, top at y=12')
check(right_of_number('n1_aufheizen_170_wippt', 37) == bitmap(UP, x, 13), 'half a second on: the arrow 1 px lower')
check(right_of_number('n5_abkuehlen', 37) == bitmap(DOWN, x, 12), 'cooling: arrow down')
check(right_of_number('n3_bereit', 37) == bitmap(TICK, x, 11), 'ready: the tick 7x7 at y=11')
check(right_of_number('n7_start', 24) == bitmap(UP, 24 + 1 + 4 + 2, 12), 'two digits: the arrow moves in with the ring')

# Ready shows the set point (tip 351, set 350); 3 C off is not ready
check(big_block('n3_bereit', '350')[0], 'ready at |set - tip| < 3: the number is the set point, not the tip')

# Sparkline
def row(temp):
    return 31 - (min(max(temp, 0), 420) * 8 + 210) // 420

def spark(name):
    return area(load(name), 0, 62, 23, 31)
flat = spark('n8_snbi_bereit')
check(flat == {(x, row(240)) for x in range(57)}, 'sparkline at the set point: one line x 0-56 on the row of the formula')
start = spark('n7_start')
check(start == {(x, row(350)) for x in range(0, 57, 2)} | {(56, row(26))}, 'first sample: dotted set line every 2nd pixel across x 0-56, the sample at the right end')
ramp = spark('n1_aufheizen_170') - {(x, row(350)) for x in range(0, 57, 2)}
temps = [26 + 9 * i for i in range(17)]
expected = set()
for i, t in enumerate(temps):
    y, prev = row(t), row(temps[i - 1]) if i else row(t)
    expected |= {(56 - 16 + i, r) for r in range(min(y, prev), max(y, prev) + 1)}
check(ramp == expected, 'heating: one column per sample, the newest at x=56, neighbours joined vertically, nothing else')
for name in SCENES:
    check(max(x for x, y in spark(name)) <= 56 and not area(load(name), 57, 62), f'{name}: nothing in x 57-62')

# Readings: the left edge of the column, rows of the values and units
for name in ('n1_aufheizen_170', 'n3_bereit', 'n5_abkuehlen'):
    img = load(name)
    column = area(img, 63, 92)
    check(min(x for x, y in column) == 63, f'{name}: readings start at x=63')
    check(not area(img, 63, 92, 0, 0) and not area(img, 63, 92, 9, 11) and not area(img, 63, 92, 20, 22), f'{name}: rows of the readings at y=1, 12, 23')
    check(max(y for x, y in area(img, 63, 92, 1, 8)) == 7, f'{name}: the value at y=1 ends on row 7')
img = load('n1_aufheizen_170')
# "65" then W: 5x7 with 6 px pitch, a px of gap, W in 3x5 two rows lower
W = bitmap(['#.#', '#.#', '###', '###', '#.#'], 63 + 11 + 1, 3)
check(area(img, 75, 77, 0, 10) == W, 'unit W in 3x5, 1 px after the value, 2 px below its top')
A = bitmap(['.#.', '#.#', '###', '#.#', '#.#'], 63 + 17 + 1, 14)
check(area(img, 81, 83, 12, 20) == A, 'unit A likewise, after "3,2"')
units = bitmap(['.#.', '#.#', '.#.', '...', '...'], 63 + 17 + 1, 25) | bitmap(['..#', '..#', '.#.', '#..', '#..'], 63 + 17 + 5, 25) | bitmap(['.##', '#..', '.#.', '..#', '##.'], 63 + 17 + 9, 25)
check(area(img, 81, 95, 23, 31) == units, '°/S likewise, after "+45"')

# Right column
def word(text, right, top):
    FONT = {'B': '##. #.# ##. #.# ##.', 'L': '#.. #.. #.. #.. ###', 'E': '### #.. ##. #.. ###', 'I': '### .#. .#. .#. ###', 'F': '### #.. ##. #.. #..', 'R': '##. #.# ##. #.# #.#', 'S': '.## #.. .#. ..# ##.', 'N': '##. #.# #.# #.# #.#', 'C': '.## #.. #.. #.. .##', 'U': '#.# #.# #.# #.# ###', 'M': '#.# ### ### #.# #.#', 'A': '.#. #.# ### #.# #.#', 'O': '.#. #.# #.# #.# .#.', 'T': '### .#. .#. .#. .#.', 'H': '#.# #.# ### #.# #.#', 'Z': '### ..# .#. #.. ###', 'G': '.## #.. #.# #.# .##'}
    x0 = right - len(text) * 4 + 2
    pixels = set()
    for i, c in enumerate(text):
        pixels |= bitmap(FONT[c].split(), x0 + i * 4, top)
    return pixels

for name, solder in (('n1_aufheizen_170', 'BLEIFREI'), ('n8_snbi_bereit', 'SNBI'), ('n8c_masse_lange', 'MASSE'), ('n8d_eigen', 'EIGEN'), ('n3_bereit', 'BLEIFREI')):
    check(area(load(name), 93, 127, 9, 15) == word(solder, 127, 10), f'{name}: {solder} right aligned to x=127 at y=10')
for name, caption in (('n1_aufheizen_170', 'NOCH'), ('n5_abkuehlen', 'NOCH'), ('n3_bereit', 'ZEIT')):
    check(area(load(name), 100, 127, 18, 24) == word(caption, 127, 19), f'{name}: {caption} at y=19')
ring3 = bitmap(['.#.', '#.#', '.#.'], 125, 1)
check(area(load('n3_bereit'), 125, 127, 0, 8) == ring3, 'set point: ° in 3x5 at the right edge')
check(max(x for x, y in area(load('n3_bereit'), 95, 123, 1, 7)) == 123 and min(y for x, y in area(load('n3_bereit'), 95, 124, 0, 8)) == 1, 'set point in 5x7 at y=1, 1 px before the °')
for name in ('n3_bereit', 'n1_aufheizen_170', 'n8c_masse_lange', 'n9b_lange_zeit'):
    bottom = area(load(name), 93, 127, 25, 31)
    check(max(x for x, y in bottom) == 127 and min(y for x, y in bottom) >= 25, f'{name}: the value at the bottom ends at x=127')

# The mark after a change
for name, solder in (('n4_lotwechsel', 'SNCU'), ('n5_abkuehlen', 'BLEILOT'), ('n8b_bleifrei_markiert', 'BLEIFREI')):
    w = len(solder) * 4 - 1
    left = 127 + 1 - w - 2 - 2
    letters = word(solder, 125, 10)
    white = {(x, y) for x in range(left, 128) for y in range(9, 16)} - letters
    check(area(load(name), 88, 127, 9, 15) == white, f'{name}: white rectangle x {left}-127, y 9-15, {solder} dark and 2 px further left')
check(area(load('n4b_lotwechsel_vorbei'), 88, 127, 9, 15) == word('SNCU', 127, 10), 'after 2 s the mark is gone and the name back in place')

# Nothing may touch anything: 1 px of air between the reading column and the right column
for name in ('n9_alles_breit', 'n9b_lange_zeit', 'n8b_bleifrei_markiert', 'n5_abkuehlen'):
    img = load(name)
    columns = {x for x, y in area(img, 63, 127, 23, 31)}
    gap = [x for x in range(63, 128) if x not in columns]
    check(bool(gap), f'{name}: rate and bottom value do not touch (free columns {gap[:3]}...)')

# Turned: the panel turns the picture, the content is the same
for a, b in (('n1_aufheizen_170', 'n6_umgedreht'), ('n3_bereit', 'n6b_umgedreht_bereit')):
    check(load(a) == load(b), f'{b}: the same picture as {a}; the panel turns it by 180°')

print(f'{len(failures)} failed')
sys.exit(1 if failures else 0)
