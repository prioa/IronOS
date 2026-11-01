"""Put rendered scenes next to the mock of the same name and count the differing pixels: compare.py <dir> <mock dir> <out.png> [name prefix]
A scene named k_<mock> only draws part of the screen and is compared there: the key row, or the area in BOX."""
import glob, os, sys
from PIL import Image, ImageChops, ImageDraw
d, mock, out = sys.argv[1:4]
prefix = sys.argv[4] if len(sys.argv) > 4 else ''
BOX = {'k_s6_big': (0, 0, 62, 23), 'k_z1_mid': (92, 0, 122, 16), 'k_m1_head': (0, 0, 128, 23), 'k_m8_zurueck': (0, 0, 128, 23)}
MOCK = {'k_s6_big': 't1_temp', 'k_z1_mid': 'z1_schlaf', 'k_m1_head': 'm1_menu_temp'}
sc, pad = 3, 16
rows = []
for f in sorted(glob.glob(os.path.join(d, prefix + '*.pbm'))):
    name = os.path.basename(f)[:-4]
    mock_name = MOCK.get(name, name[2:] if name.startswith('k_') else name)
    path = os.path.join(mock, f'c_{mock_name}.png')
    real = Image.open(f).convert('L').point(lambda v: 0 if v else 255)
    ref = Image.open(path).convert('L') if os.path.exists(path) else None
    box = BOX.get(name, (0, 23, 128, 32) if name.startswith('k_') else (0, 0, 128, 32))
    count = None
    if ref:
        count = sum(1 for v in ImageChops.difference(real.crop(box), ref.crop(box)).getdata() if v)
    rows.append((name, real, ref, count))
    print(f'{name:30} {"kein Entwurf" if ref is None else str(count) + " Pixel"}')
sheet = Image.new('RGB', (2 * 128 * sc + 30, len(rows) * (32 * sc + pad) + 8), (34, 34, 34))
dr = ImageDraw.Draw(sheet)
for i, (name, real, ref, count) in enumerate(rows):
    y = 4 + i * (32 * sc + pad)
    dr.text((10, y), f'{name}: {"" if count is None else str(count) + " px Abweichung"} (links Code, rechts Entwurf)', fill=(210, 210, 210))
    sheet.paste(real.resize((128 * sc, 32 * sc), Image.NEAREST).convert('RGB'), (10, y + 13))
    if ref:
        sheet.paste(ref.resize((128 * sc, 32 * sc), Image.NEAREST).convert('RGB'), (20 + 128 * sc, y + 13))
sheet.save(out)
