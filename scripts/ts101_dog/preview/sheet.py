"""Combine the rendered .pbm screens into one labelled PNG: sheet.py <dir> <out.png>"""
import glob, os, sys
from PIL import Image, ImageDraw
d, out = sys.argv[1], sys.argv[2]
files = sorted(glob.glob(os.path.join(d, '*.pbm')))
sc, pad = 4, 22
sheet = Image.new('RGB', (128 * sc + 20, len(files) * (32 * sc + pad) + 8), (34, 34, 34))
dr = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    y = 4 + i * (32 * sc + pad)
    dr.text((10, y), os.path.basename(f)[:-4], fill=(210, 210, 210))
    img = Image.open(f).convert('L').point(lambda v: 0 if v else 255)  # PBM 1 (pixel on) reads as black: invert to OLED white
    img = img.resize((128 * sc, 32 * sc), Image.NEAREST).convert('RGB')
    sheet.paste(img, (10, y + 14))
sheet.save(out)
