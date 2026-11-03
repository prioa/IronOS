"""Contact sheet of the v2 screens, grouped and side by side: sheet_v2.py <dir> <out.png>"""
import glob
import os
import sys
from PIL import Image, ImageDraw, ImageFont

d, out = sys.argv[1], sys.argv[2]
GROUPS = [('h', 'Home'), ('l', 'Löten'), ('w', 'Warnungen'), ('m', 'Menü'), ('s', 'Schlaf')]
SC, COLS, PAD, LABEL = 3, 4, 12, 14
cell_w, cell_h = 128 * SC + 2, 32 * SC + 2 + LABEL
rows = []
for prefix, title in GROUPS:
    files = sorted(glob.glob(os.path.join(d, prefix + '*.pbm')))
    rows.append((title, files))
height = sum(24 + ((len(f) + COLS - 1) // COLS) * (cell_h + PAD) for _, f in rows) + PAD
sheet = Image.new('RGB', (COLS * (cell_w + PAD) + PAD, height), (30, 30, 30))
dr = ImageDraw.Draw(sheet)
FONT = ImageFont.truetype(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fonts', 'Barlow-SemiBold.ttf'), 16)
y = PAD
for title, files in rows:
    dr.text((PAD, y), title, fill=(255, 210, 120), font=FONT)
    y += 24
    for i, f in enumerate(files):
        x, yy = PAD + (i % COLS) * (cell_w + PAD), y + (i // COLS) * (cell_h + PAD)
        dr.text((x, yy), os.path.basename(f)[:-4], fill=(200, 200, 200))
        img = Image.open(f).convert('L').point(lambda v: 0 if v else 255)  # PBM 1 = lit: white on black like the OLED
        sheet.paste((70, 70, 70), (x, yy + LABEL, x + cell_w, yy + LABEL + 32 * SC + 2))
        sheet.paste(img.resize((128 * SC, 32 * SC), Image.NEAREST).convert('RGB'), (x + 1, yy + LABEL + 1))
    y += ((len(files) + COLS - 1) // COLS) * (cell_h + PAD)
sheet.save(out)
