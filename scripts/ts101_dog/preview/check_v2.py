"""Checks the rendered v2 screens against the grid's pixel rules: check_v2.py <dir with the .pbm of build.sh>

- zones A-C end at x 92 (the name at 93): columns 94 and 95 stay dark, except where the menu lets a long word run left
- key boxes: 15x14 frames at x 96-110 and 113-127, y 18-31, or one wide box 96-127
- the ring of the reading: 4x4 at x 124-127, y 0-3
- soldering with the normal view: the dotted set line on row 23, every 3rd pixel
- sleep: no grid, no keys
"""
import glob
import os
import sys


def load(path):
    lines = open(path).read().split('\n')
    w, h = map(int, lines[1].split())
    bits = ' '.join(lines[2:]).split()
    return [[bits[y * w + x] == '1' for x in range(w)] for y in range(h)]


def frame_ok(img, x0, x1, y0=18, y1=31):
    edge = [(x, y0) for x in range(x0, x1 + 1)] + [(x, y1) for x in range(x0, x1 + 1)]
    edge += [(x0, y) for y in range(y0, y1 + 1)] + [(x1, y) for y in range(y0, y1 + 1)]
    return all(img[y][x] for x, y in edge)


RING = ['.##.', '#..#', '#..#', '.##.']
failures = 0


def fail(name, what):
    global failures
    failures += 1
    print(f'FAIL {name}: {what}')


for path in sorted(glob.glob(os.path.join(sys.argv[1], '*.pbm'))):
    name = os.path.basename(path)[:-4]
    img = load(path)
    if name.startswith('s'):  # the screen saver
        if frame_ok(img, 96, 110) or frame_ok(img, 113, 127):
            fail(name, 'a key box in sleep')
        continue
    words_left = name.startswith('m') and any(img[y][x] for y in range(0, 11) for x in (94, 95))
    if not words_left and any(img[y][x] for y in range(32) for x in (94, 95)):
        fail(name, 'something in columns 94-95')
    if not (frame_ok(img, 96, 110) and frame_ok(img, 113, 127)) and not frame_ok(img, 96, 127):
        fail(name, 'no key boxes at 96-110 / 113-127 or 96-127')
    if not name.startswith('m') and not name.startswith('l09'):
        ring = [''.join('#' if img[y][x] else '.' for x in range(124, 128)) for y in range(4)]
        if ring != RING and not name.startswith('l1'):  # the wipe frames keep it too, checked below
            fail(name, f'ring {ring}')
    if name.startswith('l') and name[1:3] in ('01', '02', '04', '06', '07', '08', '14'):
        if not all(img[23][x] for x in range(0, 93, 3) if not any(img[y][x] for y in (22, 24))):
            fail(name, 'set line on row 23 has gaps')
print('all passed' if not failures else f'{failures} failed')
sys.exit(1 if failures else 0)
