"""Warning triangle and icons for the redesigned screens (see make_sprites.py)."""
from PIL import Image, ImageDraw
from gsd32 import render

def grid(rows):
    img = Image.new('L', (len(rows[0]), len(rows)), 0)
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == '#': img.putpixel((x, y), 255)
    return img

def triangle(inner='!'):
    """Warning triangle 24x24 with '!' or a thermometer inside."""
    shapes = [('poly', [(12, 0.5), (23.8, 23), (0.2, 23)])]
    holes = [('poly', [(12, 5), (20, 21), (4, 21)])]
    img = render(shapes, (24, 24), holes=holes)
    d = ImageDraw.Draw(img)
    if inner == '!':
        d.rectangle((11, 9, 12, 16), fill=255); d.rectangle((11, 18, 12, 19), fill=255)
    return img


THERMO_16 = grid([  # 8x16 thermometer
    "...##...", "..#..#..", "..#..#..", "..#..#..", "..#..#..", "..####..", "..####..", "..####..",
    "..####..", "..####..", ".######.", "########", "########", "########", ".######.", "..####.."])
THERMO_8 = grid(["..#.....", ".#.#....", ".#.#....", ".###....", ".###....", "#####...", "#####...", ".###...."])
BOLT = grid(["....##..", "...##...", "..##....", ".######.", "....##..", "...##...", "..##....", ".#......"])
PAW = grid(["..#..#..", "........", "#..##..#", "........", "..####..", ".######.", ".######.", "..#..#.."])
FLAME = grid(["....#...", "...##...", "..###.#.", ".#.####.", ".#.#.##.", "##...##.", ".##.##..", "..###..."])
ARROW_DOWN_8 = grid(["..#.....", "..#.....", "..#.....", "..#.....", "#.#.#...", ".###....", "..#.....", "........"])
PLAY_8 = grid(["#.......", "###.....", "#####...", "#######.", "#####...", "###.....", "#.......", "........"])
GEAR_8 = grid([".#.##.#.", ".######.", "###..###", ".#....#.", ".#....#.", "###..###", ".######.", ".#.##.#."])
