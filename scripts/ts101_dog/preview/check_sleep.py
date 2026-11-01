"""Checks the sleep scenes against the coordinates of the spec: check_sleep.py <rendered dir>"""
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

base = load('s10_verschoben_0')
DOG = 2  # the dog's column 0
HEAD_DROP = 1
eye = lambda x, y: (DOG + x, y + HEAD_DROP)
for x in (6, 9):
    check(eye(x, 7) in base and eye(x, 9) in base, f'asleep: the eye gap at column {x} is filled on rows 7 and 9')
    check(eye(x, 8) not in base, f'asleep: row 8 stays open at column {x}')
check(min(y for x, y in area(base, 0, 15, 0, 13)) == 1, 'asleep: the head is a pixel lower, its top on row 1')
check(min(x for x, y in area(base, 40, 88, 1, 7)) == 44, 'SCHLAF starts at x=44')
check(min(y for x, y in area(base, 44, 88, 0, 8)) == 1, 'SCHLAF starts at y=1')
check(min(y for x, y in area(base, 44, 88, 10, 18)) == 11, 'AUS IN starts at y=11')
check(min(y for x, y in area(base, 50, 127, 20, 31)) == 25, 'BEWEGEN WECKT starts at y=25')
check(max(x for x, y in area(base, 50, 127, 25, 31)) == 127, 'and ends at the right edge')
check(min(x for x, y in area(load('s30_kuehlt_noch'), 95, 127, 0, 9)) == 97, 'the temperature starts at x=97 (280: the 1 of 150 has an empty column)')
check(not area(base, 90, 94, 0, 10), 'at the temperature held: no arrow')
check(bool(area(load('s30_kuehlt_noch'), 90, 94, 0, 10)), 'still cooling: arrow at x=90')

def z_pixels(name):
    return area(load(name), 15, 43, 0, 12)
counts = [z_pixels(n) for n in ('s01_schlaf_z_0', 's02_schlaf_z_1', 's03_schlaf_z_2', 's05_schlaf_z_leer')]
check(counts[0] and len(counts[1]) > len(counts[0]) and len(counts[2]) > len(counts[1]), 'the three z appear one after the other')
check(not area(load('s05_schlaf_z_leer'), 16, 43, 0, 11), 'the loop ends empty')

for name, dx in (('s10_verschoben_0', 0), ('s11_verschoben_1', -1), ('s12_verschoben_2', -2), ('s13_verschoben_3', -1)):
    check(min(x for x, y in area(load(name), 40, 88, 1, 7)) == 44 + dx, f'{name}: content moved by {dx}')

w1, w2, w3 = load('s20_wach_bild1'), load('s21_wach_bild2'), load('s22_wach_bild3')
check(eye(6, 7) in w1 and eye(6, 9) not in w1, 'wake 1: the lower lid is open')
check(eye(6, 7) not in w2 and eye(6, 9) not in w2, 'wake 2: both open')
check(min(y for x, y in area(w2, 0, 15, 0, 13)) == 1 and min(y for x, y in area(w3, 0, 15, 0, 13)) == 0, 'wake 3: the head back up')
check(not area(w1, 16, 43, 0, 11), 'waking: no z')
print(f'{len(failures)} failed')
sys.exit(1 if failures else 0)
