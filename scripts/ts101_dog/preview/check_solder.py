"""Checks the boost scenes against the coordinates of the spec: check_solder.py <rendered dir>. The soldering screen: check_heat.py"""
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

boost = load('b1_boost')
check(len(area(boost, 0, 39, 0, 9)) > 300, 'boost: inverted badge at the top left')
ground = [area(load(n), 0, 92, 31, 31) for n in ('b1_boost', 'b1_boost_bild1', 'b1_boost_bild2')]
check(ground[0] != ground[1] != ground[2] and all(ground), 'boost: the dashed ground moves from frame to frame')
legs = [area(load(n), 40, 70, 26, 30) for n in ('b1_boost', 'b1_boost_bild1', 'b1_boost_bild2', 'b1_boost_bild3')]
check(len({frozenset(l) for l in legs}) == 4, 'boost: four different leg frames, one per 25 ms (40 a second)')

print(f'{len(failures)} failed')
sys.exit(1 if failures else 0)
