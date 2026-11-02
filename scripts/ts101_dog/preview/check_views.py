"""Checks the home views against the design page: check_views.py <rendered dir> [frames of homeV3, default design/homeV3]"""
import os, sys
from PIL import Image

out = sys.argv[1]
design = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'design', 'homeV3')

def load(name):
    with open(os.path.join(out, name + '.pbm')) as f:
        tokens = f.read().split()
    width, height, bits = int(tokens[1]), int(tokens[2]), tokens[3:]
    return {(x, y) for y in range(height) for x in range(width) if bits[y * width + x] == '1'}

def frame(index):
    img = Image.open(os.path.join(design, f'{index:03d}.png')).convert('1')
    return {(x, y) for y in range(32) for x in range(128) if img.getpixel((x, y))}

def area(pixels, x0, x1, y0=0, y1=31):
    return {(x, y) for x, y in pixels if x0 <= x <= x1 and y0 <= y <= y1}

failures = []
def check(ok, text):
    print(('ok    ' if ok else 'FAIL  ') + text)
    if not ok:
        failures.append(text)

# The letters S and G of SP and GR come from the firmware's 3x5 font, whose S and G are rounder than the design's
for index in (0, 8, 20, 30, 31, 50, 78):
    real, ref = area(load(f'v3_schaeferhund_{index:02d}'), 8, 127), area(frame(index), 8, 127)
    check(real == ref, f'Schäferhund frame {index}: temperatures, lines and dog pixel for pixel as on the design ({len(real ^ ref)} differ)')
hot = load('v3_schaeferhund_heiss')
check(min(x for x, y in area(hot, 7, 24, 4, 10)) >= 8, 'three digits stay a pixel clear of SP')
check(not area(hot, 29, 29, 0, 29), 'and the ring a pixel clear of the dotted line')

for name in ('v1_symbole', 'v4_abkuehlen'):
    img = load(name)
    check(len(area(img, 0, 31, 0, 31)) > 150 and not area(img, 32, 33, 0, 31) - area(load('h1_ruhig'), 32, 33, 0, 31), name + ': the head on the left as on the home screen')
for name in ('v1_symbole_gedreht', 'v4_gedreht'):
    check(len(area(load(name), 96, 127, 0, 31)) > 150, name + ': turned, the head on the right')
cool = load('v4_abkuehlen')
check(area(cool, 36, 127, 30, 30) == {(x, 30) for x in range(36, 128, 3)}, 'Abkühlen: the 50 C line dotted every 3rd pixel from x=36')
for name in ('v5_kolben', 'v5_kolben_links20', 'v5_kolben_rechts20', 'v5_kolben_rechts40'):
    img = load(name)
    check(len(img) > 100 and area(img, 0, 127, 31, 31) == {(x, 31) for x in range(0, 128, 2)}, name + ': the iron drawn, the dotted ground at y=31')
# The lie comes from the sensor: turning about the long axis moves the display window, tilting the long axis tilts the drawing
window = lambda name: area(load(name), 40, 80, 0, 30)
check(window('v5_kolben_flach') != window('v5_kolben_seite_a') != window('v5_kolben_seite_b'), 'Kolben: flat, on one side and on the other the display shows differently')
def ends(name):  # the lowest row of the iron at its left and right quarter
    img = area(load(name), 0, 127, 0, 30)
    left = [y for x, y in img if 20 <= x <= 40]
    right = [y for x, y in img if 87 <= x <= 107]
    return (max(left) if left else 0), (max(right) if right else 0)
check(ends('v5_kolben_flach_spitze_runter')[1] > ends('v5_kolben_flach_spitze_runter')[0], 'Kolben: tip down, the tip end (right) is lower')
check(ends('v5_kolben_links20')[1] < ends('v5_kolben_links20')[0], 'Kolben: tip up, the tip end is higher')

print(f'{len(failures)} failed')
sys.exit(1 if failures else 0)
