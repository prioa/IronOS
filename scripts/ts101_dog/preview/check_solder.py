"""Checks the heating, soldering and boost scenes against the coordinates of the spec: check_solder.py <rendered dir>"""
import os, sys

out = sys.argv[1]

def load(name):
    with open(os.path.join(out, name + '.pbm')) as f:
        tokens = f.read().split()
    width, height, bits = int(tokens[1]), int(tokens[2]), tokens[3:]
    return {(x, y) for y in range(height) for x in range(width) if bits[y * width + x] == '1'}

def area(pixels, x0, x1, y0=0, y1=31):
    return {(x, y) for x, y in pixels if x0 <= x <= x1 and y0 <= y <= y1}

def rect(x0, y0, x1, y1):
    return {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1) if x in (x0, x1) or y in (y0, y1)}

def line(x0, y0, x1, y1):
    return {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)}

failures = []
def check(ok, text):
    print(('ok    ' if ok else 'FAIL  ') + text)
    if not ok:
        failures.append(text)

minus_plus = rect(96, 17, 110, 31) | rect(113, 17, 127, 31) | line(98, 24, 108, 24) | line(115, 24, 125, 24) | line(120, 19, 120, 29)
for name in ('k1_heizt_65w', 'l1_bereit_350', 'l2_plus_370_heizt', 'l4_minus_350_kuehlt'):
    check(area(load(name), 96, 127, 17, 31) == minus_plus, name + ': boxes at 96 and 113, - and + 11 px long, no outer frame')
check(area(load('l11_minus_gedrueckt'), 97, 109, 18, 30) == line(97, 18, 109, 30) - line(98, 24, 108, 24), 'rear pressed: the inside of the - box inverted')
check(area(load('k5_heizt_plus_gedrueckt'), 114, 126, 18, 30) == line(114, 18, 126, 30) - line(115, 24, 125, 24) - line(120, 19, 120, 29), 'front pressed while heating: + box inverted')

for name, arrow in (('l1_bereit_350', False), ('l2_plus_370_heizt', True), ('l4_minus_350_kuehlt', True), ('k4_heizt_gedreht', None), ('k5_heizt_plus_gedrueckt', True)):
    if arrow is None:
        continue
    img = load(name)
    check(bool(area(img, 90, 94, 0, 10)) == arrow, f'{name}: arrow at x=90 {"shown" if arrow else "absent"}')
    check(min(x for x, y in area(img, 95, 127, 0, 9)) == 97, f'{name}: the temperature starts at x=97')

for name in ('l1_bereit_350', 'l5_platine_bleifrei_heizt', 'l6_platine_bleifrei_wechsel', 'l4_minus_350_kuehlt'):
    img = load(name)
    name_px = area(img, 0, 95, 0, 10)
    check(min(x for x, y in name_px) == 0 and min(y for x, y in name_px) == 0, f'{name}: solder name from (0,0)')
widest = max(x for x, y in area(load('l5_platine_bleifrei_heizt'), 0, 89, 0, 10))
check(widest <= 88, f'"Platine bleifrei" ends at x={widest}, before the arrow at x=90 (1 px gap needed)')

check(min(y for x, y in area(load('l1_bereit_350'), 0, 89, 11, 18)) == 12, 'line 2 starts at y=12')
dots = area(load('l7_empfindlich_wechsel'), 54, 89, 13, 17)
check(min(x for x, y in dots) == 54 and len({x for x, y in dots}) >= 5, '"240° SnBi": the five dots from x=54, the first filled')

for name in ('l1_bereit_350', 'l3_bereit_370', 'l2_plus_370_heizt', 'k1_heizt_65w'):
    curve = area(load(name), 0, 95, 19, 31)
    check(max(x for x, y in curve) <= 92, f'{name}: curve and set line within x=0-92, y=19-31')
img = load('l2_plus_370_heizt')
set_row = min(y for x, y in area(img, 0, 92, 19, 31))
check(area(img, 0, 92, set_row, set_row) == {(x, set_row) for x in range(0, 93, 3)}, f'after + the dotted set line jumps to the new set point (row {set_row}), every 3rd pixel')

boost = load('b1_boost')
check(len(area(boost, 0, 39, 0, 9)) > 300, 'boost: inverted badge at the top left')
ground = [area(load(n), 0, 92, 31, 31) for n in ('b1_boost', 'b1_boost_bild1', 'b1_boost_bild2')]
check(ground[0] != ground[1] != ground[2] and all(ground), 'boost: the dashed ground moves from frame to frame')
legs = [area(load(n), 40, 70, 26, 30) for n in ('b1_boost', 'b1_boost_bild1', 'b1_boost_bild2', 'b1_boost_bild3')]
check(len({frozenset(l) for l in legs}) == 4, 'boost: four different leg frames, one per 25 ms (40 a second)')
heat = [area(load(n), 0, 92, 9, 31) for n in ('k1_heizt_65w', 'k1_heizt_65w_bild1', 'k1_heizt_65w_bild2', 'k1_heizt_65w_bild3')]
check(len({frozenset(l) for l in heat}) >= 3, 'heating at 65 W: the legs change every 50 ms (20 a second)')

print(f'{len(failures)} failed')
sys.exit(1 if failures else 0)
