"""German Shepherd sprites at 32 px height, drawn as supersampled polygons."""
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

def walking(phase, tail='down'):
    nh, fh = WALK[phase % 6]
    shapes = [('poly', HEAD), ('poly', BODY)] + TAILS[tail]
    shapes += hind(fh) + front(nh) + hind(nh) + front(fh)
    return render(shapes, (80, 32), holes=[EYE, MOUTH])

# ---------- sitting, facing right, 60 x 32 ----------
SHEAD = [(30, 8), (33, 5), (34, 0), (37.5, 3.5), (39, 3.5), (41.5, 0), (43, 5),
         (45, 7.5), (51.5, 10), (53, 11.5), (52.5, 13), (50, 14), (44, 14.5), (40, 16)]
SBODY = [(40, 16), (42, 21), (42, 26), (40, 29), (30, 30), (24, 23), (24, 17), (27, 11), (30, 8)]
SEYE = ('ell', (39.2, 6.6, 41.4, 8.8))
SMOUTH = ('line', [(50.5, 13.2), (45, 13.6)], 0.6)
SITTAILS = {
    'ground': [('line', [(14, 29.5), (8, 30.2), (3, 30.2), (1, 29.2)], 3.8)],
    'wag':    [('line', [(14, 28.5), (9, 26.5), (5, 23.5), (3, 20.5)], 3.8)],
}
SHEAD_SAD = [(30, 8), (32, 5.5), (35, 4.2), (40, 4), (43, 5), (45, 7.5), (51.5, 10.5), (53, 12), (52.5, 13.5),
             (50, 14.5), (44, 15), (40, 16)]
def sitting(tail='ground', tongue=False, bark=False, sad=False):
    shapes = [('poly', SHEAD_SAD if sad else SHEAD), ('poly', SBODY), ('ell', (12, 17, 32, 31.9))] + SITTAILS[tail]
    shapes += [('line', [(38.5, 20), (39, 31)], 3.4), ('line', [(39, 31), (42.5, 31)], 2.4),
               ('line', [(35, 22), (35.5, 31)], 3), ('line', [(29, 31), (34, 31)], 2.2)]
    holes = [SEYE] + ([('line', [(41, 5.2), (33.5, 6.2)], 0.6)] if sad else []) + [('line', [(37, 23), (37.2, 31.9)], 0.7), ('line', [(22, 21), (19, 25), (19.5, 31.9)], 0.7)]
    if bark:
        holes += [('poly', [(53.5, 12.2), (44.5, 13.4), (53.5, 16.5)])]
        shapes += [('poly', [(44.5, 13.6), (52, 16.5), (51, 17.6), (44.5, 16)])]
    else:
        holes += [SMOUTH]
    if tongue:
        shapes += [('ell', (45.5, 13.5, 49, 19.5))]
        holes += [('line', [(47.2, 14.5), (47.2, 18)], 0.5)]
    return render(shapes, (60, 32), holes=holes)

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

if __name__ == '__main__':
    def show(imgs, out, sc=5):
        H = sum(i.height * sc + 8 for i in imgs) + 8
        sh = Image.new('RGB', (128 * sc + 16, H), (40, 40, 40)); y = 8
        for i in imgs:
            c = Image.new('L', (128, i.height), 0); c.paste(i, ((128 - i.width) // 2, 0))
            sh.paste(c.resize((128 * sc, i.height * sc), Image.NEAREST).convert('RGB'), (8, y)); y += i.height * sc + 8
        sh.save(out)
    show([walking(p) for p in range(0, 6, 2)] + [walking(0, 'up')], 'walk.png')
    show([sitting(), sitting('wag'), sitting(tongue=True), sitting(bark=True), lying()], 'sit.png')
