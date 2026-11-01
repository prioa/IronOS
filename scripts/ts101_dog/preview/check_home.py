"""Checks the rendered home screen scenes pixel by pixel: check_home.py <rendered dir>"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
out = sys.argv[1]

def load(name):
    with open(os.path.join(out, name + '.pbm')) as f:
        tokens = f.read().split()
    width, height, bits = int(tokens[1]), int(tokens[2]), tokens[3:]
    return {(x, y) for y in range(height) for x in range(width) if bits[y * width + x] == '1'}

def bitmap(name):
    rows, on = [], False
    for line in open(os.path.join(HERE, '..', 'art', 'home.txt'), encoding='utf8'):
        line = line.strip()
        if line.startswith('@'):
            on = line[1:].split()[0] == name
        elif on and line:
            rows.append(line)
        elif on:
            break
    return {(x, y) for y, row in enumerate(rows) for x, c in enumerate(row) if c == '#'}

def rect(x0, y0, x1, y1):
    return {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1) if x in (x0, x1) or y in (y0, y1)}

def moved(pixels, dx, dy):
    return {(x + dx, y + dy) for x, y in pixels}

failures = []
def check(ok, text):
    print(('ok    ' if ok else 'FAIL  ') + text)
    if not ok:
        failures.append(text)

dog, sliders, play = bitmap('dogHome'), bitmap('iconSliders'), bitmap('iconHomePlay')

def box(x):
    return rect(x, 17, x + 14, 31)

def pair(left, front_right):
    play_box, slider_box = (left + 17, left) if front_right else (left, left + 17)
    return box(left) | box(left + 17) | moved(play, play_box + 5, 19) | moved(sliders, slider_box + 2, 21)

def area(pixels, x0, x1, y0=0, y1=31):
    return {(x, y) for x, y in pixels if x0 <= x <= x1 and y0 <= y <= y1}

def rdiv(a, b):
    return int((a + (-(b // 2) if a < 0 else b // 2)) / b)

def dog_at(origin, s3=0, head_x=None, down=0):
    """The firmware's transform: shear about row 18, s in thirds of a pixel, head rows 0-13 moved on their own"""
    hx = rdiv(2 * s3, 15) if head_x is None else head_x
    hx += rdiv(3 * down, 3)
    hy = rdiv(2 * down, 3)
    out = set()
    for x, y in dog:
        head = y < 14
        nx, ny = origin + x + rdiv(s3 * (18 - y), 54) + (hx if head else 0), y + (hy if head else 0)
        if 0 <= nx < 128 and 0 <= ny < 32:
            out.add((nx, ny))
    return out

normal, turned = load('h1_ruhig'), load('h6_umgedreht')
check(area(normal, 96, 127, 17, 31) == pair(96, True), 'normal: two 15x15 boxes at x=96 and 113, play at (118,19), sliders at (98,21), no outer frame')
check(area(turned, 0, 31, 17, 31) == pair(0, False), 'turned: the boxes on the left, play in the left box next to the front button')
inside = {(x, y) for x in range(97, 110) for y in range(18, 31)}
check(area(load('h10_hinten_gedrueckt'), 97, 109, 18, 30) == inside - moved(sliders, 98, 21), 'rear pressed: the inside of its box inverted')
check(area(normal, 0, 33) == dog_at(2), 'normal: at rest the dog is the bitmap, 2 px further right (x=2)')
check(area(turned, 94, 127) == dog_at(98), 'turned: at rest the dog is the bitmap at x=98, not mirrored')

check(area(load('h2_links20'), 0, 35) == area(dog_at(2, 15), 0, 35), 'tilted left 20: s=+5 (against the tilt), head +2')
check(area(load('h3_rechts20'), 0, 35) == area(dog_at(2, -15), 0, 35), 'tilted right 20: s=-5, head -2')
check(area(load('h2_links20_bild1'), 0, 35) == area(dog_at(2, 5), 0, 35), 'tilt fades in: frame 1 is s=5/3')
check(area(load('h2_links20_bild2'), 0, 35) == area(dog_at(2, 10), 0, 35), 'tilt fades in: frame 2 is s=10/3')
check(area(load('h4_spitze_unten'), 0, 35) == area(dog_at(2, 0, None, 3), 0, 35), 'tip down: head 3 px right, 2 px down, no lean')
# Tip down from level: the lean the filter passes through on the way, then the head goes down one step a frame
levels = []
for i in range(1, 9):
    img = area(load(f'h4_spitze_unten_bild{i}'), 0, 35)
    match = [(s3, d) for s3 in range(-15, 16, 5) for d in range(4) if img == area(dog_at(2, s3, None, d), 0, 35)]
    levels.append(match[0] if match else None)
check(None not in levels, 'tip down: every frame is the dog with a lean in thirds and a head level ' + str(levels))
downs = [d for _, d in levels if levels and _ is not None] if None not in levels else []
check(bool(downs) and downs[-1] == 3 and all(b - a in (0, 1) for a, b in zip(downs, downs[1:])) and 1 in downs and 2 in downs,
      'tip down: the head goes down over 3 frames, one step each, and stays')
for frame, (s, v) in enumerate(zip((3, -3, 3, -3, 3, -3, 2, -2), (2, -2, 2, -2, 2, -2, 1, -1))):
    check(area(load(f'h5_geschuettelt_{frame}'), 0, 35) == area(dog_at(2, 3 * s, v), 0, 35), f'shaken frame {frame}: s={s}, head {v:+d}')

for name, x0, limit in [(n, 37, 93) for n in ('h8_gesamt_max', 'h8_aktuell_38', 'h8_aktuell_999', 'h8_aktuell_max', 'h8_vorgaenge_9999', 'h8_vorgaenge_max', 'h8_boosts_max')] + [('h8_umgedreht_vorgaenge_9999', 36, 92)]:
    img = load(name)
    last = max(x for x, y in area(img, x0, limit - 1, 0, 9))
    check(not area(img, limit, limit, 0, 9), f'{name}: statistics end at x={last}, limit {limit} free before the arrow')

down, down2 = load('h9_runter'), load('h9_runter_wippt')
arrow = bitmap('dogTrendDown')
rows_flipped = {(x, 4 - y) for x, y in arrow}
check(moved(arrow, 94, 3) <= down and moved(arrow, 94, 4) <= down2, 'arrow down at x=94 (7 left of the first digit), y=3 and then y=4')
check(moved(rows_flipped, 94, 3) <= load('h9_hoch'), 'arrow up: the same rows the other way round')
check(not area(normal, 94, 100, 3, 8), 'no arrow at a steady temperature')

curve = load('h1_ruhig')
dots = {(x, 23) for x in range(37, 92, 4)}
check(area(curve, 37, 91, 23, 23) == dots, 'dotted base line: every 4th pixel from x=37')
columns = {x for x, y in area(curve, 37, 91, 12, 21)}
check(columns == set(range(37, 92)), 'the curve fills all 55 columns after 10 minutes')
check(all(12 <= y <= 21 for x, y in area(curve, 37, 91, 10, 22)), 'the curve stays on rows 12-21')
rows = {x: [y for xx, y in area(curve, 37, 91, 12, 21) if xx == x] for x in columns}
check(all(min(rows[a]) <= max(rows[a + 1]) + 1 and min(rows[a + 1]) <= max(rows[a]) + 1 for a in range(37, 91)), 'neighbouring columns join without a gap')

print(f'{len(failures)} failed')
sys.exit(1 if failures else 0)
