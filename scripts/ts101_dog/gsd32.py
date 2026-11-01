"""German Shepherd sprites, drawn as supersampled polygons."""
from PIL import Image, ImageDraw
K = 8

def render(shapes, size, thr=0.5, holes=()):
    big = Image.new('L', (size[0] * K, size[1] * K), 0)
    d = ImageDraw.Draw(big)
    for s in shapes:
        if s[0] == 'poly':
            d.polygon([(x * K, y * K) for x, y in s[1]], fill=255)
        elif s[0] == 'line':
            d.line([(x * K, y * K) for x, y in s[1]], fill=255, width=int(s[2] * K), joint='curve')
        elif s[0] == 'ell':
            x0, y0, x1, y1 = s[1]
            d.ellipse((x0 * K, y0 * K, x1 * K, y1 * K), fill=255)
    for s in holes:
        if s[0] == 'poly':
            d.polygon([(x * K, y * K) for x, y in s[1]], fill=0)
        elif s[0] == 'ell':
            x0, y0, x1, y1 = s[1]
            d.ellipse((x0 * K, y0 * K, x1 * K, y1 * K), fill=0)
        elif s[0] == 'line':
            d.line([(x * K, y * K) for x, y in s[1]], fill=0, width=int(s[2] * K))
    small = big.resize(size, Image.BOX)
    return small.point(lambda v: 255 if v > 255 * thr else 0)

# ---------- standing / walking, facing right, 80 x 32 ----------
HEAD = [(56, 8), (59, 5), (60, 0), (63.5, 3.5), (65, 3.5), (67.5, 0), (69, 5),
        (71, 7.5), (77.5, 10), (79, 11.5), (78.5, 13), (76, 14), (70, 14.5), (66, 16)]
BODY = [(66, 16), (63, 19), (60, 21.5), (54, 22), (46, 21), (38, 20), (30, 20),
        (24, 19.5), (19, 17), (17.5, 14), (20, 11.5), (28, 10.5), (40, 10.5), (50, 10), (56, 8)]
EYE = ('ell', (65.2, 6.6, 67.4, 8.8))
MOUTH = ('line', [(76.5, 13.2), (71, 13.6)], 0.6)

def hind(dx):  # angulated hind leg: thigh, hock, foot
    return [('line', [(24, 16), (22.5 + dx * 0.3, 23)], 6.5), ('line', [(22.5 + dx * 0.3, 23), (18.5 + dx, 27.5)], 3.2),
            ('line', [(18.5 + dx, 27.5), (19.5 + dx, 31)], 2.6), ('line', [(19.5 + dx, 31), (22.5 + dx, 31)], 2.2)]

def front(dx):
    return [('line', [(56, 18), (56.5 + dx * 0.4, 25)], 4.8), ('line', [(56.5 + dx * 0.4, 25), (56 + dx, 31)], 3),
            ('line', [(56 + dx, 31), (59 + dx, 31)], 2.2)]

TAILS = {
    'down': [('line', [(19, 13), (13, 17), (9.5, 22), (8, 27)], 4.4)],
    'mid':  [('line', [(19, 13), (12, 15), (6, 18), (2.5, 20)], 4.4)],
    'up':   [('line', [(19, 13), (13, 10), (8, 7), (5, 6)], 4.4)],
}
WALK = [(-5, 4), (-2, 1), (2, -2), (5, -5), (2, -2), (-2, 1)]

def walking(phase, tail='down', stride=WALK):
    nh, fh = stride[phase % len(stride)]
    shapes = [('poly', HEAD), ('poly', BODY)] + TAILS[tail]
    shapes += hind(fh) + front(nh) + hind(nh) + front(fh)
    return render(shapes, (80, 32), holes=[EYE, MOUTH])

RUN = [(-10, 9), (-5, 4), (4, -5), (9, -10)]

def running(phase):
    """Running dog, tail out, 60x24 (the 80x32 drawing with long strides, scaled down); phases 0..3 swap the legs over."""
    return walking(phase, 'mid', RUN).resize((60, 24), Image.BOX).point(lambda v: 255 if v > 110 else 0)

# ---------- lying, sleeping, facing right, 72 x 24 ----------
LHEAD = [(46, 9), (49, 5.5), (50, 0.5), (53.5, 4), (55, 4), (57.5, 0.5), (59, 5.5),
         (61, 8.5), (68, 12), (70, 13.5), (69.5, 15.5), (66, 17), (58, 17.5), (52, 17)]
LBODY = [(52, 17), (54, 23.8), (9, 23.8), (5, 20), (7, 14), (14, 10.5), (26, 9.5), (38, 10), (46, 9)]
def lying(awake=False):
    shapes = [('poly', LHEAD), ('poly', LBODY), ('line', [(52, 22.6), (69, 22.6)], 2.6),
              ('line', [(8, 21.5), (2.5, 22.8), (0.5, 23)], 3.6), ('ell', (6, 12, 24, 23.9))]
    eye = ('ell', (54.6, 7.4, 57, 9.8)) if awake else ('line', [(54.5, 8.6), (57.8, 9.0)], 0.9)
    holes = [eye,
             ('line', [(50, 23.8), (53, 20.8)], 0.7),
             ('line', [(25, 23.8), (22, 18), (16, 15.5)], 0.7)]
    return render(shapes, (72, 24), holes=holes)
