"""Checks the rendered home screen scenes pixel by pixel: check_home.py <rendered dir>"""
import os, re, sys

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

# The header is the source of truth for the dog, the icons and the arrows
header = open(os.path.join(HERE, '..', 'art', 'ts101_home_assets.h'), encoding='utf8').read()
def rows_of(values):
    return {(x, y) for y, v in enumerate(values) for x in range(32) if (v >> (31 - x)) & 1}
heads = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]{8}', re.search(r'TS101_DOG_HEADS\[10\]\[32\]\s*=\s*\{(.*?)\n\};', header, re.S).group(1))]
assert len(heads) == 320
POSE = {n: rows_of(heads[i * 32:(i + 1) * 32]) for i, n in enumerate(['front', 'eyes_left', 'eyes_right', 'turn_left', 'turn_right', 'cock', 'up', 'ears_back', 'blink', 'tongue'])}
def icon(name):
    return rows_of([int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]{8}', re.search(name + r'\[\d\] = \{(.*?)\};', header).group(1))])
sliders, play = icon('TS101_ICON_SLIDERS'), icon('TS101_ICON_PLAY')
arrow_down, arrow_up = icon('TS101_ARROW_DOWN'), icon('TS101_ARROW_UP')

def area(pixels, x0, x1, y0=0, y1=31):
    return {(x, y) for x, y in pixels if x0 <= x <= x1 and y0 <= y <= y1}

def pair(left, front_right):
    box_a, box_b = left + 2, left + 17
    play_box, slider_box = (box_b, box_a) if front_right else (box_a, box_b)
    return rect(left, 17, left + 31, 31) | rect(box_a, 19, box_a + 12, 29) | rect(box_b, 19, box_b + 12, 29) | moved(play, play_box + 5, 21) | moved(sliders, slider_box + 2, 21)

def dog(name, origin=0):
    return area(load(name), origin, origin + 31)

def is_pose(name, pose, origin=0, drop=0):
    expected = {(x + origin, y + drop) for x, y in POSE[pose] if y + drop < 32}
    check(dog(name, origin) == expected, f'{name}: {pose}' + (f', {drop} px down' if drop else ''))

normal, turned = load('h1_ruhig'), load('h6_umgedreht')
check(area(normal, 96, 127, 17, 31) == pair(96, True), 'normal: frame 96-127 x 17-31, boxes 98-110 and 113-125 x 19-29, sliders at (100,21), play at (118,21)')
check(area(turned, 0, 31, 17, 31) == pair(0, False), 'turned: the pair on the left, play in the left box next to the front button, icons not mirrored')
inside = {(x, y) for x in range(99, 110) for y in range(20, 29)}
check(area(load('h10_hinten_gedrueckt'), 99, 109, 20, 28) == inside - moved(sliders, 100, 21), 'rear pressed: the inside of its box inverted')
is_pose('h1_ruhig', 'front')
is_pose('h6_umgedreht', 'front', 96)
check(not area(normal, 32, 36), 'nothing between the dog and the statistics')
# GAZE[0..4]: both turned poses in the header look the same way, the other side is them mirrored
def mirrored(pixels):
    return {(31 - x, y) for x, y in pixels}
GAZE = [mirrored(POSE['turn_right']), mirrored(POSE['turn_left']), POSE['front'], POSE['turn_left'], POSE['turn_right']]
check(POSE['turn_left'] != mirrored(POSE['turn_left']), 'the turned poses are not symmetric: mirroring shows')
def is_gaze(name, step, origin=0):
    check(dog(name, origin) == {(x + origin, y) for x, y in GAZE[step]}, f'{name}: gaze step {step}')
for degrees, near in ((10, 1), (20, 2), (30, 2)):
    is_gaze(f'h2_links{degrees}', 2 - near)
    is_gaze(f'h3_rechts{degrees}', 2 + near)
    is_gaze(f'h6_umgedreht_links{degrees}', 2 - near, 96)
    is_gaze(f'h6_umgedreht_rechts{degrees}', 2 + near, 96)
is_pose('h4_spitze_unten', 'up')
for name, target in (('h2a_links_bild', 0), ('h3a_rechts_bild', 4)):
    # Level, then tilted: one step at most, at most every other frame, and it arrives
    frames = [GAZE.index(dog(f'{name}{i}')) for i in range(1, 9)]
    runs = [frames[i + 1] - frames[i] for i in range(7)]
    check(frames[-1] == target and all(abs(d) <= 1 for d in runs) and all(not (runs[i] and runs[i + 1]) for i in range(6)), f'{name}: turns step by step, {frames}')
for frame, hop in zip((1, 2, 3, 4), (0, 1, 2, 1)):
    is_pose(f'h5_geschuettelt_{frame}', 'ears_back', 0, hop)
for frame in (1, 2, 3, 4, 5):
    is_pose(f'h7_nach_schuetteln_{frame}', 'cock')
is_pose('h8a_blinzelt', 'blink')
is_pose('h8b_blinzelt_2', 'blink')
is_pose('h8c_zunge', 'tongue')

for name, x0, limit in [(n, 37, 93) for n in ('h8_gesamt_max', 'h8_seit_start_38', 'h8_seit_start_100', 'h8_seit_start_999', 'h8_seit_start_max', 'h8_vorgaenge_9999', 'h8_vorgaenge_max', 'h8_boosts_max')] + [('h8_umgedreht_vorgaenge_9999', 36, 92)]:
    img = load(name)
    last = max(x for x, y in area(img, x0, limit - 1, 0, 9))
    check(not area(img, limit, limit, 0, 9), f'{name}: statistics end at x={last}, limit {limit} free before the arrow')

# The row: the label from y=3, the value from y=1, the big digits from y=0
for name in ('h8_gesamt_max', 'h8_seit_start_38', 'h8_vorgaenge_9999', 'h8_boosts_max'):
    img = load(name)
    tops = sorted({y for x, y in area(img, 37, 93, 0, 9)})
    check(tops[0] == 1 and tops[-1] == 7, f'{name}: the row is on y=1-7, {tops[0]}-{tops[-1]}')
    check(min(y for x, y in area(img, 37, 45, 0, 9)) == 3, f'{name}: the label starts on y=3')
check(min(y for x, y in area(load('h1_ruhig'), 101, 121, 0, 12)) == 0, 'big digits start on y=0')

down, down2 = load('h9_runter'), load('h9_runter_wippt')
check(moved(arrow_down, 94, 2) <= down and moved(arrow_down, 94, 3) <= down2, 'arrow down at x=94 (7 left of the first digit), y=2 and then y=3')
check(moved(arrow_up, 94, 2) <= load('h9_hoch'), 'arrow up: y=2')
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
