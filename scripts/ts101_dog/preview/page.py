"""One page with every rendered screen beside its board, the differing pixels, the other states and the button table:
page.py <rendered dir> <board dir> <out.html>"""
import base64, glob, html, io, os, sys
from PIL import Image, ImageChops

rendered, boards, out = sys.argv[1:4]
BOARDS = [
    ('Detail, stop, temperature, sleep', [('g_detail', 'Detail view', 'Menu switch DETAILANSICHT'), ('g_stopp', 'Stop, rear held', ''), ('g_schlaf', 'Sleep', '')]),
    ('Menu', [('g_menu', 'Section Schlaf', ''), ('g_wert', 'Value being changed', '')]),
]
OTHER = [('h1_ruhig', 'Start'), ('h6_umgedreht', 'Start, turned'), ('h9_runter', 'Start, cooling'), ('h2_links20', 'Start, tilted left'), ('h4_spitze_unten', 'Start, tip down'), ('h5_geschuettelt_0', 'Start, shaken'),
         ('k1_heizt_65w', 'Heating at 65 W'), ('k3_heizt_fast_da', 'Heating, nearly there'), ('k4_heizt_gedreht', 'Heating, turned'), ('l1_bereit_350', 'Soldering, ready 350°'), ('l2_plus_370_heizt', 'After +: 370°, heating'),
         ('l3_bereit_370', 'Ready 370°'), ('l4_minus_350_kuehlt', 'After −: cooling'), ('l5_platine_bleifrei_heizt', 'Longest name'), ('l10_gedreht', 'Soldering, turned'), ('b1_boost', 'Boost'), ('b2_boost_gedreht', 'Boost, turned'), ('r4_detail_gedreht', 'Detail, turned'), ('r5_schlaf_gedreht', 'Sleep, turned'), ('r6_stopp_gedreht', 'Stop, turned'), 
         ('s4_druck', 'Front pressed'), ('s5_halten', 'Front held'), ('z2_schlaf_aus', 'Sleep, auto-off disabled'),
         ('i1_strom', 'Info 1/5'), ('i4_statistik', 'Info 4/5, total time'), ('m2_menu_info', 'Menu root'), ('m10_hilfe', 'Help text after 3 s'), ('m14_anzeige_schalter', 'Switches'), ('m9_halten', 'Rear held in the menu'), ('w5_sperre', 'Lock notice'), ('w4_griff', 'Handle too hot')]
TABLE = [('Start', 'Solder', 'Menu', 'Nothing', '*Info'), ('Soldering', 'One solder hotter', '*One solder colder', '*Boost, 30 s at most', '*Stop after 0.5 s'),
         ('Menu list', '*Open or change', '*Next row', '*Nothing', 'Back'), ('Value', '+', '*−', 'Faster +', 'Done'), ('Info', 'Next page', '*Previous page', 'On System: debug', 'Back'), ('Sleep', 'Wake', 'Wake', 'Wake', '*Heater off, to Start')]
CHOICES = ['Lock: hold both buttons on Start to switch it on or off. The SPERRE menu entry must not be off. To unlock: both buttons stop, then the same gesture on Start.',
           'A set point that is no preset shows Soll on Start. Presets read SnBi, Bleilot, Bleifrei, SnCu, Masse.',
           'Clicks change the solder at once while soldering, no view in between, nothing at the ends. There is no Temperature view any more.',
           'The dog runs only while heating up the first time and during boost; once the set point is reached it only comes back after sleep.']

def load(path, invert):
    img = Image.open(path).convert('L')
    return img.point(lambda v: 0 if v else 255) if invert else img

def uri(img):
    buf = io.BytesIO()
    img.save(buf, 'PNG')
    return 'data:image/png;base64,' + base64.b64encode(buf.getvalue()).decode()

def shot(img, alt):
    return f'<img class="px" src="{uri(img)}" alt="{html.escape(alt)}" width="384" height="96">'

parts, exact, total = [], 0, 0
for title, items in BOARDS:
    cards = []
    for name, label, note in items:
        real, ref = load(f'{rendered}/{name}.pbm', True), load(f'{boards}/c_{name}.png', False)
        diff = sum(1 for v in ImageChops.difference(real, ref).getdata() if v)
        exact += diff == 0
        total += 1
        chip = '<span class="chip ok">Exact</span>' if diff == 0 else f'<span class="chip near">{diff} px differ</span>'
        cards.append(f'<article class="card"><header><h3>{html.escape(label)}</h3>{chip}</header><div class="pair"><figure>{shot(real, label + ", firmware")}<figcaption>Firmware</figcaption></figure><figure>{shot(ref, label + ", board")}<figcaption>Board</figcaption></figure></div>' + (f'<p class="note">{html.escape(note)}</p>' if note else '') + '</article>')
    parts.append(f'<section><h2>{html.escape(title)}</h2><div class="cards">{"".join(cards)}</div></section>')
others = ''.join(f'<figure>{shot(load(f"{rendered}/{n}.pbm", True), l)}<figcaption>{html.escape(l)}</figcaption></figure>' for n, l in OTHER)
cell = lambda c: f'<td class="new">{html.escape(c[1:])}</td>' if c.startswith('*') else f'<td>{html.escape(c)}</td>'
rows = ''.join(f'<tr><th scope="row">{r[0]}</th>{"".join(cell(c) for c in r[1:])}</tr>' for r in TABLE)
open(out, 'w', encoding='utf8').write(f'''<title>TS101 Gegenentwürfe firmware</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:wght@400;600&display=swap">
<style>
/* One dark look, taken from the boards themselves: every screen sits on a panel like the iron's OLED */
:root {{ --bg:#111412; --panel:#1a1e1b; --line:#2d342f; --fg:#e6ebe7; --muted:#a0aaa2; --front:#f2b33d; --rear:#6fb7c9; --new:#2b2414; --ok:#7fc99a;
  --sans:'IBM Plex Sans',system-ui,sans-serif; --mono:'IBM Plex Mono',ui-monospace,monospace; color-scheme:dark }}
body {{ background:var(--bg); color:var(--fg); font:15px/1.5 var(--sans); padding-block:40px 64px; padding-left:max(16px,env(safe-area-inset-left,0px)); padding-right:max(16px,env(safe-area-inset-right,0px)) }}
main {{ max-width:1010px; margin:auto; display:flex; flex-direction:column; gap:40px }}
h1 {{ font-size:30px; line-height:1.2; margin:0 0 8px; text-wrap:balance }} h2 {{ font-size:19px; margin:0 0 14px }} h3 {{ font-size:15px; margin:0 }}
.lead {{ color:var(--muted); margin:0; max-width:65ch }}
.cards {{ display:flex; flex-direction:column; gap:14px }}
.card {{ background:var(--panel); border:1px solid var(--line); border-radius:6px; padding:16px; display:flex; flex-direction:column; gap:12px }}
.card header {{ display:flex; justify-content:space-between; align-items:baseline; gap:12px; flex-wrap:wrap }}
.pair {{ display:flex; gap:20px; flex-wrap:wrap }}
figure {{ margin:0; display:flex; flex-direction:column; gap:6px; min-width:0 }}
figcaption,.note {{ font-size:12.5px; color:var(--muted); margin:0 }}
.px {{ display:block; width:384px; max-width:100%; height:auto; image-rendering:pixelated; image-rendering:crisp-edges; background:#000 }}
.chip {{ font:500 12px var(--mono); letter-spacing:.04em }} .ok {{ color:var(--ok) }} .near {{ color:var(--front) }}
.gallery {{ display:grid; grid-template-columns:repeat(auto-fill,minmax(min(100%,300px),1fr)); gap:18px }} .gallery .px {{ width:100% }}
.scroll {{ overflow-x:auto }} table {{ border-collapse:collapse; width:100%; min-width:640px; border:1px solid var(--line) }}
th,td {{ text-align:left; padding:9px 12px; border-bottom:1px solid var(--line) }} thead th {{ font-size:12px; letter-spacing:.06em; text-transform:uppercase }}
.f {{ color:var(--front) }} .r {{ color:var(--rear) }} td.new {{ background:var(--new) }}
ul {{ margin:0; padding-left:20px; display:flex; flex-direction:column; gap:7px; max-width:75ch }}
</style>
<main>
<header><h1>TS101 Gegenentwürfe firmware</h1><p class="lead">Each screen is rendered by the real firmware code on the Mac and set beside its board. {exact} of {total} match pixel for pixel; none of this has run on the iron yet.</p></header>
{"".join(parts)}
<section><h2>Button table</h2><div class="scroll"><table><thead><tr><th scope="col">Screen</th><th scope="col" class="f">Front click</th><th scope="col" class="r">Rear click</th><th scope="col" class="f">Front hold</th><th scope="col" class="r">Rear hold</th></tr></thead><tbody>{rows}</tbody></table></div>
<p class="note">Shaded cells changed from the previous firmware. Both buttons while soldering stop at once, also when locked, during boost and in the Temperature view.</p></section>
<section><h2>Choices the boards left open</h2><ul>{"".join(f"<li>{html.escape(c)}</li>" for c in CHOICES)}</ul></section>
<section><h2>States without a board</h2><div class="gallery">{others}</div></section>
</main>
''')
print(f'{exact} of {total} exact, wrote {out}')
