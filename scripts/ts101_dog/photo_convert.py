"""Convert a cut out dog photo to a 1 bit bitmap: photo_convert.py <photo.png>, then call convert(), see art/README.md."""
import sys
from PIL import Image, ImageOps, ImageFilter, ImageChops, ImageDraw
SRC = Image.open(sys.argv[1]).convert('RGBA')
ALPHA = SRC.getchannel('A')
GRAY = Image.composite(SRC.convert('L'), Image.new('L', SRC.size, 0), ALPHA)

def erode(m):
    return m.filter(ImageFilter.MinFilter(3))

def convert(box, size, bias=0.5, local=0.4, outline=True, blur=1.0):
    g = GRAY.filter(ImageFilter.GaussianBlur(blur))
    wide = g.filter(ImageFilter.GaussianBlur(max(SRC.size) / 14))
    diff = ImageChops.subtract(g, wide, scale=1, offset=128)
    mixed = Image.blend(ImageOps.autocontrast(g, cutoff=1), ImageOps.autocontrast(diff, cutoff=1), local).crop(box)
    a = ALPHA.crop(box).resize(size, Image.BOX)
    mask = a.point(lambda v: 255 if v > 127 else 0)
    sm = mixed.resize(size, Image.LANCZOS)
    vals = sorted(v for v, m in zip(sm.getdata(), mask.getdata()) if m)
    t = vals[int(len(vals) * bias)]
    bw = sm.point(lambda v: 255 if v > t else 0)
    bw = ImageChops.multiply(bw, mask)
    if outline:
        edge = ImageChops.subtract(mask, erode(mask))
        bw = ImageChops.lighter(bw, edge)
    # drop isolated pixels
    px = bw.load(); w, h = bw.size; out = bw.copy(); o = out.load()
    for y in range(h):
        for x in range(w):
            n = sum(px[x + dx, y + dy] > 0 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= x + dx < w and 0 <= y + dy < h)
            if px[x, y] and n == 0: o[x, y] = 0
    return out
