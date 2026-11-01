"""Unpack the boards of "TS101 Gegenentwürfe.html" into 128x32 reference images: extract_gegenentwuerfe.py <html> <out dir>
Every screen on a board is saved as c_<name>.png (new design) and ist_<name>.png (the state it replaces), as compare.py expects."""
import base64, gzip, io, json, os, re, sys
from PIL import Image

NAMES = {
    'Bereit, kalt': 'g_start', 'Bereit, mit Hund': 'g_start_hund', 'Spitze noch heiß': 'g_heiss', 'Andersherum gehalten': 'g_start_flip',
    'Heizt auf': 'g_heizt', 'Heizt auf, Netzteil begrenzt': 'g_begrenzt', 'Bereit': 'g_bereit', 'Temperatur erreicht': 'g_erreicht',
    'Unter Last': 'g_last', 'Detailansicht': 'g_detail', 'Boost, vorne gehalten': 'g_boost', 'Stopp, hinten gehalten': 'g_stopp',
    'Temperatur auf Preset': 'g_temp', 'Schlaf': 'g_schlaf', 'Bereich Schlaf': 'g_menu', 'Wert wird geändert': 'g_wert',
}

def block(text, kind):
    return json.loads(re.search(r'<script type="__bundler/%s">(.*?)</script>' % kind, text, re.S).group(1).strip())

def data(entry):
    raw = base64.b64decode(entry['data'])
    return gzip.decompress(raw) if entry.get('compressed') else raw

def save(raw, path):
    Image.open(io.BytesIO(raw)).convert('L').resize((128, 32), Image.NEAREST).point(lambda v: 255 if v > 127 else 0).save(path)

html, out = sys.argv[1:3]
os.makedirs(out, exist_ok=True)
root = open(html, encoding='utf8').read()
manifest = block(root, 'manifest')
for page in block(root, 'page_order'):
    text = data(manifest[page]).decode('utf8')
    res, template = block(text, 'manifest'), block(text, 'template')
    for title, new, old in re.findall(r'font-weight: 600">([^<]*)</span>.*?<img class="px" src="([0-9a-f-]+)".*?<img class="px" src="([0-9a-f-]+)"', template, re.S):
        save(data(res[new]), os.path.join(out, f'c_{NAMES[title]}.png'))
        save(data(res[old]), os.path.join(out, f'ist_{NAMES[title]}.png'))
        print(NAMES[title], title)
