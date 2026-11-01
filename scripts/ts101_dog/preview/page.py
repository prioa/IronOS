"""One HTML page with every TS101 screen as rendered from the real code: page.py <render dir> <render dir EN> <out.html>
The screens come from build.sh; this file only arranges them and says what they show."""
import base64, html, io, os, sys
from PIL import Image

RENDER, RENDER_EN, OUT = sys.argv[1:4]
STATUS = sys.argv[4] if len(sys.argv) > 4 else ''


def uri(path):
    img = Image.open(path).convert('L').point(lambda v: 0 if v else 255).convert('1')  # PBM 1 = lit pixel
    buf = io.BytesIO()
    img.save(buf, 'PNG')
    return 'data:image/png;base64,' + base64.b64encode(buf.getvalue()).decode()


RULES = [
    ('Vorne handelt, hinten führt.', 'Vorne (Spitze) startet, öffnet, erhöht. Hinten (Kabel) blättert zurück und verringert.'),
    ('Hinten halten heißt raus.', 'Auf jedem Bildschirm, ohne Ausnahme. Beim Löten stoppt es die Heizung nach einem 1-s-Balken. Im Menü geht schon ein Klick hinten zurück.'),
    ('Die Beschriftung steht neben ihrer Taste.', 'Wird das Bild gedreht, wandert sie mit, die Taste behält ihre Funktion.'),
    ('Ein Klick ändert nie das Heizen.', 'Der erste Klick öffnet nur die Anzeige, Halten springt um höchstens ein Preset, direkt nach dem Boost zählen Klicks nicht (Upstream #1663).'),
    ('Der Bildschirm passt sich an.', 'Aufheizen, Last, schwaches Netzteil, Abkühlen und Schlaf erscheinen von selbst.'),
]

KEYS = [
    ('Start', 'Löten', 'Menü', 'Temperatur', 'nichts'),
    ('Löten', 'Temperatur öffnen', 'Ansicht wechseln', 'Boost, solange gehalten', 'Stopp nach 1-s-Balken'),
    ('Temperatur', '+ 10°', '− 10°', 'nächstes Preset', 'fertig'),
    ('Menüliste', 'nächste Zeile', 'zurück', 'öffnen, umschalten oder ändern', 'zurück'),
    ('Wert', '+', 'fertig', 'schneller +', 'fertig'),
    ('Info', 'nächste Seite', 'zurück', 'auf „System“: Debug-Werte', 'zurück'),
    ('Schlaf', 'aufwecken', 'aufwecken', 'aufwecken', 'aufwecken'),
]

# (section id, title, lead, [(scene, title, caption)])
SECTIONS = [
    ('start', 'Start', 'Der Kolben heizt nicht. Der Hundekopf, die gesamte Lötdauer und unten immer die Stromversorgung.', [
        ('s1_start', 'Bereit', 'GESAMT ist die Lötzeit über die ganze Lebensdauer, gezählt ab Erreichen der Solltemperatur.'),
        ('s7_start_100h', 'Ab 100 Stunden', 'Dann nur noch die Stunden.'),
        ('s2_heiss', 'Spitze noch heiß', 'Warndreieck, Abkühlkurve der letzten 3 Minuten mit Linie bei 50 °C, daneben die Spitzentemperatur.'),
        ('s9_heiss_frisch', 'Gerade abgelegt', 'Die Kurve beginnt neu und wächst von rechts nach links.'),
        ('s3_schwach', 'Schwaches Netzteil', 'Das Ausrufezeichen warnt schon vor dem Start (30 W oder weniger).'),
        ('s4_druck', 'Taste gedrückt', 'Das Feld neben der Taste wird invertiert, sobald sie unten ist.'),
        ('s5_halten', 'Vorne gehalten', 'Der Balken füllt sich von der Taste her und sagt, was gleich passiert. Loslassen bricht ab.'),
        ('s6_temp', 'Temperatur vor dem Start', 'Halten vorne öffnet die Temperatur, die Heizung bleibt aus.'),
    ]),
    ('loeten', 'Löten', 'Die Temperaturkurve ist immer sichtbar. Der Hund läuft nur beim Aufheizen und im Boost; bei Temperatur stehen links Messwerte.', [
        ('l1_aufheizen', 'Heizt auf', 'Der Hund läuft, unten wächst die Kurve, rechts der Sollwert.'),
        ('l5_begrenzt', 'Heizt auf, Netzteil begrenzt', 'Die flache Kurve zeigt, dass es langsam geht, der Text sagt warum.'),
        ('l2_wuff', 'Temperatur erreicht', 'Einmal WUFF! BEREIT, das Display ist der einzige Rückmeldekanal.'),
        ('l3_bereit', 'Bereit', 'Soll, Eingangsspannung und Griff links, die Kurve, groß die Spitze, unten die Sitzungszeit.'),
        ('l4_last', 'Unter Last', 'Der Einbruch in der Kurve, unten Watt und Lastbalken.'),
        ('l6_schlafcount', 'Kurz vor dem Schlaf', 'Die letzten 10 Sekunden werden angezeigt.'),
        ('l7_boost', 'Boost, vorne gehalten', 'Der Hund rennt über den Bildschirm, die Kurve läuft unten klein weiter, daneben Ist und Ziel.'),
        ('l8_stopp', 'Hinten gehalten', 'Nach etwa 1 s ist die Heizung aus.'),
        ('l9_druck_temp', 'Vorne gedrückt', 'Ein Klick öffnet nur die Temperatur, er ändert nichts.'),
        ('d1_details', 'Zweite Ansicht', 'Klick hinten: 72 px breite Kurve, Watt, Volt und berechnete Ampere (Tilde).'),
        ('d2_details_einbruch', 'Zweite Ansicht beim Löten', 'Der Einbruch zeigt, wie gut die Spitze nachkommt.'),
    ]),
    ('temperatur', 'Temperatur', 'Eine Skala von 250 bis 450 °C mit den Presets. Die Heizung läuft weiter, nach 2 s ohne Taste schließt die Anzeige.', [
        ('t1_temp', 'Geöffnet', 'SOLL groß, rechts läuft die Kurve weiter, die Marke zeigt die Lage auf der Skala.'),
        ('t2_preset300', 'Preset 300 °C', 'Auf einem Preset steht 2 s lang der Anwendungshinweis statt der Kurve.'),
        ('t3_preset350', 'Preset 350 °C', ''),
        ('t4_preset400', 'Preset 400 °C', ''),
        ('t5_halten', 'Vorne gehalten', 'Halten springt genau ein Preset weiter, nie bis zum Maximum.'),
    ]),
    ('schlaf', 'Schlaf', 'Bewegung oder jede Taste weckt den Kolben; der weckende Druck tut sonst nichts.', [
        ('z1_schlaf', 'Schlaf', 'Der Hund liegt, rechts Spitzentemperatur, Schlafdauer und was weckt.'),
    ]),
    ('menue', 'Menü', 'Oben fest der Titel mit Position und den Pfeilen an ihren Tasten, darunter drei Einstellungen. Beim Blättern wandert nur die Markierung. Jeder Wert speichert sich beim Verlassen.', [
        ('m1_menu_temp', 'Hauptmenü', 'Sieben Punkte, neben Temperatur gleich der aktuelle Sollwert.'),
        ('m4_item', 'Bereich Schlaf', 'Name links, Wert rechts, die gewählte Zeile invertiert, rechts der Positionsbalken.'),
        ('m13_vorne_halten', 'Vorne gehalten', 'Der Balken in der Titelzeile sagt, was passiert: öffnen, umschalten oder ändern.'),
        ('m6_edit', 'Wert wird geändert', 'Nur der Wert blinkt. Oben + und ←, dazwischen die 2 s bis zum Speichern.'),
        ('m9_halten', 'Hinten gehalten', 'Zurück, wie überall.'),
        ('m10_hilfe', 'Hilfetext', 'Nach 3 s ohne Taste läuft oben eine Erklärung durch.'),
        ('m14_anzeige_schalter', 'Bereich Anzeige', 'Ein/Aus-Einstellungen sind Schalter; Halten vorne schaltet sofort um.'),
        ('m15_system', 'Bereich System', 'Aktionen tragen einen Pfeil, Halten vorne startet sie.'),
        ('m16_strom', 'Bereich Strom', ''),
        ('m11_loeten_spitze', 'Bereich Löten', 'Mit dem Spitzentyp.'),
    ]),
    ('info', 'Info', 'Fünf Seiten statt Entwicklerdaten. Klick vorne blättert, hinten geht zurück.', [
        ('i1_strom', 'Strom', ''),
        ('i2_spitze', 'Spitze', 'Der Spitzentyp aus dem Menü.'),
        ('i3_griff', 'Griff und Lage', 'Der Griff-NTC, verlängert bis 100 °C.'),
        ('i4_statistik', 'Statistik', 'Gesamtzeit und letzte Sitzung.'),
        ('i5_system', 'System', ''),
        ('i6_debug_halten', 'System, vorne gehalten', 'Öffnet die alten Debug-Werte.'),
    ]),
    ('warnungen', 'Unterbrechungen', 'Vollbild, weil sie wichtiger sind als alles andere: was los ist, und unten, was es bedeutet.', [
        ('w1_spitze', 'Spitze fehlt', ''),
        ('w2_strom', 'Unterspannung', ''),
        ('w3_heizung', 'Thermischer Fehler', ''),
        ('w4_griff', 'Griff zu heiß', 'Ab 70 °C für 2 s geht die Heizung aus. Der Start ist gesperrt, bis der Griff kühler ist.'),
        ('w5_sperre', 'Tastensperre', 'Beide Tasten halten sperrt und entsperrt.'),
    ]),
    ('gedreht', 'Andersherum gehalten', 'Das Bild dreht sich mit dem Lagesensor. Die Beschriftung wandert mit und bleibt neben ihrer Taste.', [
        ('r1_start_gedreht', 'Start', ''),
        ('s8_heiss_gedreht', 'Start, Spitze heiß', ''),
        ('r2_bereit_gedreht', 'Löten, bereit', ''),
        ('r3_temp_gedreht', 'Temperatur', ''),
    ]),
]
EN = [('s1_start', 'Start'), ('s2_heiss', 'Spitze heiß'), ('l3_bereit', 'Löten, bereit'), ('l8_stopp', 'Stopp'),
      ('t3_preset350', 'Preset'), ('m4_item', 'Menü'), ('i3_griff', 'Info'), ('w4_griff', 'Griff zu heiß')]

CSS = """
:root { --bg:#111412; --panel:#1a1e1b; --line:#2d342f; --fg:#e6ebe7; --muted:#a0aaa2; --front:#f2b33d; --rear:#6fb7c9;
 --body:"IBM Plex Sans",system-ui,sans-serif; --mono:"IBM Plex Mono",ui-monospace,monospace; color-scheme:dark; }
body { background:var(--bg); color:var(--fg); font:15px/1.55 var(--body); padding-left:max(16px,env(safe-area-inset-left,0px)); padding-right:max(16px,env(safe-area-inset-right,0px)); padding-block:0 56px; }
body::before { content:""; position:fixed; inset:0 0 auto; height:env(safe-area-inset-top,0px); z-index:10; background:var(--bg); }
.top { position:sticky; top:env(safe-area-inset-top,0px); z-index:5; background:var(--bg); border-bottom:1px solid var(--line); padding-block:10px; }
nav { display:flex; gap:6px; overflow-x:auto; } nav a { flex:none; color:var(--fg); text-decoration:none; font-size:13px; padding:3px 10px; border:1px solid var(--line); border-radius:999px; }
nav a:hover, nav a:focus-visible { border-color:var(--front); color:var(--front); outline:none; }
header { padding-block:26px 6px; display:flex; flex-direction:column; gap:8px; }
h1 { font-size:28px; line-height:1.2; margin:0; text-wrap:balance; } h2 { font-size:20px; margin:0; text-wrap:balance; } h3 { font-size:14px; font-weight:600; margin:0; }
p { margin:0; max-width:72ch; } .muted { color:var(--muted); } .status { font:13px var(--mono); color:var(--muted); }
section { padding-top:34px; display:flex; flex-direction:column; gap:10px; scroll-margin-top:56px; }
ol { margin:0; padding-left:22px; display:grid; gap:8px; max-width:76ch; }
.tw { overflow-x:auto; border:1px solid var(--line); border-radius:6px; } table { border-collapse:collapse; min-width:640px; width:100%; font-size:14px; }
th, td { text-align:left; vertical-align:top; padding:8px 10px; border-bottom:1px solid var(--line); } tr:last-child td { border-bottom:0; }
th { font-size:12px; color:var(--muted); font-weight:600; letter-spacing:.04em; text-transform:uppercase; } th.f { color:var(--front); } th.r { color:var(--rear); }
.grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(min(100%,420px),1fr)); gap:14px; }
.item { min-width:0; background:var(--panel); border:1px solid var(--line); border-radius:6px; padding:12px; display:flex; flex-direction:column; gap:8px; }
.oled { background:#000; padding:8px; border-radius:3px; } img.px { image-rendering:pixelated; image-rendering:crisp-edges; display:block; width:384px; max-width:100%; height:auto; }
.native { display:flex; align-items:center; gap:10px; } .native img { image-rendering:pixelated; outline:4px solid #000; flex:none; } .native span { font:12px var(--mono); color:var(--muted); }
.cap { font-size:14px; } .en { display:grid; grid-template-columns:repeat(auto-fill,minmax(min(100%,280px),1fr)); gap:12px; }
.en figure { margin:0; display:flex; flex-direction:column; gap:4px; } .en img { width:256px; max-width:100%; height:auto; image-rendering:pixelated; outline:6px solid #000; margin:6px; } .en figcaption { font-size:12px; color:var(--muted); }
@media (prefers-reduced-motion:reduce) { * { scroll-behavior:auto !important; } }
"""


def esc(t):
    return html.escape(t)


def main():
    out = ['<title>TS101 Bedienung</title>', '<meta name="claude:viewport-fit-sides" content="cover">',
           '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;600&family=IBM+Plex+Mono&display=swap">',
           f'<style>{CSS}</style>']
    nav = [('regeln', 'Regeln'), ('tasten', 'Tasten')] + [(sid, title) for sid, title, _, _ in SECTIONS] + [('englisch', 'Englisch')]
    out.append('<div class="top"><nav aria-label="Abschnitte">' + ''.join(f'<a href="#{i}">{esc(t)}</a>' for i, t in nav) + '</nav></div>')
    out.append('<header><h1>TS101: Zwei Tasten, die sich selbst erklären</h1>'
               '<p>Jeder Bildschirm hier ist mit der echten Zeichenfunktion der Firmware auf dem Mac gerendert, in echten 128 × 32 Pixeln. '
               'Auf dem Kolben ist nur geprüft, was du selbst ausprobiert hast.</p>'
               f'<p class="status">{esc(STATUS)}</p></header>')
    out.append('<section id="regeln"><h2>Fünf Regeln</h2><ol>' + ''.join(f'<li><b>{esc(a)}</b> {esc(b)}</li>' for a, b in RULES) + '</ol></section>')
    rows = ''.join('<tr>' + ''.join(f'<td>{"<b>" + esc(c) + "</b>" if i == 0 else esc(c)}</td>' for i, c in enumerate(r)) + '</tr>' for r in KEYS)
    out.append('<section id="tasten"><h2>Was jede Taste tut</h2><p class="muted">Vorne liegt zur Spitze, hinten zum Kabel. '
               'Beide halten ist die Tastensperre, beide klicken ein zweiter Weg zurück.</p><div class="tw"><table><thead><tr>'
               '<th scope="col">Bildschirm</th><th scope="col" class="f">Klick vorne</th><th scope="col" class="r">Klick hinten</th>'
               f'<th scope="col" class="f">Halten vorne</th><th scope="col" class="r">Halten hinten</th></tr></thead><tbody>{rows}</tbody></table></div></section>')
    for sid, title, lead, items in SECTIONS:
        out.append(f'<section id="{sid}"><h2>{esc(title)}</h2><p class="muted">{esc(lead)}</p><div class="grid">')
        for scene, name, caption in items:
            src = uri(os.path.join(RENDER, scene + '.pbm'))
            out.append(f'<article class="item"><h3>{esc(name)}</h3><div class="oled"><img class="px" src="{src}" width="128" height="32" alt="{esc(name)}"></div>'
                       f'<div class="native"><img src="{src}" width="128" height="32" alt=""><span>Originalgröße</span></div>'
                       + (f'<p class="cap">{esc(caption)}</p>' if caption else '') + '</article>')
        out.append('</div></section>')
    out.append('<section id="englisch"><h2>Englischer Build</h2><p class="muted">Der EN-Build hat eigene englische Texte, alle anderen Sprachen nutzen sie auch, weil die Schriften nur lateinische Buchstaben haben.</p><div class="en">')
    for scene, name in EN:
        out.append(f'<figure><img src="{uri(os.path.join(RENDER_EN, scene + ".pbm"))}" width="128" height="32" alt="{esc(name)}, englisch"><figcaption>{esc(name)}</figcaption></figure>')
    out.append('</div></section>')
    open(OUT, 'w', encoding='utf8').write('\n'.join(out))


if __name__ == '__main__':
    main()
