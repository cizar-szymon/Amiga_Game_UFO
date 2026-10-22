"""Raport PDF: poprawki AmiXcom RTG (gałąź feat/battle-scroll-640), liczby z opisów commitów."""
import os, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
                                Image, PageBreak, KeepTogether, CondPageBreak)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

OUT = sys.argv[1]
TMP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "report_img")
os.makedirs(TMP, exist_ok=True)

pdfmetrics.registerFont(TTFont("Arial", r"C:\Windows\Fonts\arial.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Bold", r"C:\Windows\Fonts\arialbd.ttf"))
plt.rcParams["font.family"] = "Arial"
plt.rcParams["font.size"] = 9

C_BEFORE, C_AFTER, C_MID = "#9aa5b1", "#2f6db5", "#e08a2c"

def save(fig, name):
    p = os.path.join(TMP, name)
    fig.tight_layout()
    fig.savefig(p, dpi=200)
    plt.close(fig)
    return p

def label_bars(ax, bars, fmt="{:g}"):
    for b in bars:
        h = b.get_height()
        ax.annotate(fmt.format(h), (b.get_x() + b.get_width() / 2, h), ha="center", va="bottom",
                    fontsize=8, xytext=(0, 2), textcoords="offset points")

# 1. Bitwa 640x400: kl./s przed i po
fig, ax = plt.subplots(figsize=(7.2, 3.3))
scenes = ["stoi", "ruch kursora", "przewijanie", "marsz żołnierza"]
before = [26, 18, 14, 15]
after = [50, 29, 37, 28]
x = range(len(scenes))
b1 = ax.bar([i - 0.2 for i in x], before, 0.4, label="przed gałęzią / przed poprawką", color=C_BEFORE)
b2 = ax.bar([i + 0.2 for i in x], after, 0.4, label="po poprawkach", color=C_AFTER)
label_bars(ax, b1); label_bars(ax, b2)
ax.set_xticks(list(x)); ax.set_xticklabels(scenes)
ax.set_ylabel("klatek / s"); ax.set_ylim(0, 58)
ax.axhline(50, color="#555", lw=0.6, ls="--"); ax.text(3.45, 50.8, "50 kl./s (PAL)", fontsize=7, ha="right", color="#555")
ax.legend(fontsize=8, loc="upper left", bbox_to_anchor=(0.2, 0.99))
ax.set_title("Bitwa 640×400 – klatki na sekundę")
img_battle = save(fig, "battle.png")

# 2. Przewijanie: ms na klatkę krok po kroku
fig, ax = plt.subplots(figsize=(7.2, 3.0))
steps = ["przed\ngałęzią", "mapa trzyma\nprzesunięty obraz", "+ zakres pól\nprzy dorysowaniu", "+ okno\nprzewijania", "+ warstwa VRAM\n(blitter karty)"]
ms = [48.9, 38.9, 35.7, 33.6, 23.5]
bars = ax.bar(steps, ms, color=[C_BEFORE, C_MID, C_MID, C_MID, C_AFTER])
fps = [20, 24, 26, 28, 37]   # zmierzone (opis commitu 8ff2ed4), nie 1000/ms
for b, v, f in zip(bars, ms, fps):
    ax.annotate("%.1f ms\n%d kl./s" % (v, f), (b.get_x() + b.get_width() / 2, v), ha="center",
                va="bottom", fontsize=8, xytext=(0, 2), textcoords="offset points")
ax.set_ylabel("ms na klatkę"); ax.set_ylim(0, 62)
ax.set_title("Ciągłe przewijanie bitwy 640×400 (commit 8ff2ed4, A1200 / Warp1260)")
img_scroll = save(fig, "scroll.png")

# 3. Marsz: podział klatki (środki zmierzonych zakresów)
fig, ax = plt.subplots(figsize=(7.2, 3.4))
stages = ["przed", "po poprawkach 1+2", "po poprawkach 1+2+3"]
parts = [("mapa", [19, 22.5, 11.5], "#2f6db5"),
         ("logika gry", [10, 8.5, 7.5], "#6aa84f"),
         ("interfejs", [16.5, 10, 8], "#e08a2c"),
         ("wysłanie na kartę", [19.5, 6, 4], "#a64d79"),
         ("reszta (zdarzenia, przerwy)", [0, 5, 5], "#999999")]   # do zmierzonej całej klatki: 65, 52, 36
bottom = [0, 0, 0]
for name, vals, col in parts:
    ax.bar(stages, vals, 0.55, bottom=bottom, label=name, color=col)
    for i, v in enumerate(vals):
        if v: ax.text(i, bottom[i] + v / 2, "%g" % v, ha="center", va="center", fontsize=8, color="white")
    bottom = [b + v for b, v in zip(bottom, vals)]
for i, t in enumerate(bottom):
    ax.text(i, t + 1, "%g ms ≈ %d kl./s" % (t, round(1000 / t)), ha="center", fontsize=8)
ax.set_ylabel("ms na klatkę"); ax.set_ylim(0, 78)
ax.legend(fontsize=8, ncol=5, loc="upper center", bbox_to_anchor=(0.5, -0.12), columnspacing=1.0, handlelength=1.2)
ax.set_title("Marsz żołnierza, bitwa 640×400 (commit c66d3c0, A4000 / Warp4070)")
img_walk = save(fig, "walk.png")

# 4. Geoscape
fig, (a1, a2) = plt.subplots(1, 2, figsize=(7.2, 3.0), gridspec_kw={"width_ratios": [1.1, 1]})
sc = ["czas płynie\n(640×400 → 320×200)","obrót strzałką\nprzytrzymaną", "obrót po\nprzybliżeniu"]
bf, af = [14, 11, 7], [49, 15, 10]
xs = range(3)
r1 = a1.bar([i - 0.2 for i in xs], bf, 0.4, color=C_BEFORE, label="przed")
r2 = a1.bar([i + 0.2 for i in xs], af, 0.4, color=C_AFTER, label="po")
label_bars(a1, r1); label_bars(a2 if False else a1, r2)
a1.set_xticks(list(xs)); a1.set_xticklabels(sc, fontsize=8); a1.set_ylim(0, 58)
a1.set_ylabel("klatek / s"); a1.legend(fontsize=8); a1.set_title("Geoscape – kl./s")
items = ["przerysowanie\nobróconego globu", "granice państw\n(przybliżenie)", "cień globu\n640×400", "cień globu\n320×200"]
bms, ams = [150, 100, 170, 46], [40, 12, 54, 17]
ys = range(4)
a2.barh([i + 0.2 for i in ys], bms, 0.4, color=C_BEFORE, label="przed")
a2.barh([i - 0.2 for i in ys], ams, 0.4, color=C_AFTER, label="po")
for i, (b, a) in enumerate(zip(bms, ams)):
    a2.text(b + 2, i + 0.2, "%d" % b, va="center", fontsize=8)
    a2.text(a + 2, i - 0.2, "%d" % a, va="center", fontsize=8)
a2.set_yticks(list(ys)); a2.set_yticklabels(items, fontsize=8); a2.invert_yaxis()
a2.set_xlabel("ms"); a2.set_xlim(0, 200); a2.set_title("Geoscape – koszt rysowania")
img_geo = save(fig, "geo.png")

# 3b. Strzelanie: koszt jednej kuli (środki zakresów)
fig, ax = plt.subplots(figsize=(7.2, 2.8))
items = ["pole widzenia jednostek\npo trafieniu", "światło terenu + cień", "pełne przerysowanie mapy\npo trafieniu i wybuchu", "klatka zatrzymania\nprzy trafieniu"]
bms, ams = [355, 55, 300, 460], [128, 63, 0, 280]
ys = range(len(items))
ax.barh([i + 0.2 for i in ys], bms, 0.4, color=C_BEFORE, label="przed (c66d3c0)")
ax.barh([i - 0.2 for i in ys], ams, 0.4, color=C_AFTER, label="po (6df1036)")
for i, (b, a) in enumerate(zip(bms, ams)):
    ax.text(b + 5, i + 0.2, "%g" % b, va="center", fontsize=8)
    ax.text(a + 5, i - 0.2, "%g" % a, va="center", fontsize=8)
ax.set_yticks(list(ys)); ax.set_yticklabels(items, fontsize=8); ax.invert_yaxis()
ax.set_xlabel("ms na jedną kulę"); ax.set_xlim(0, 540); ax.legend(fontsize=8, loc="lower right")
ax.set_title("Strzelanie, misja4 – koszt jednej kuli (A4000 / Warp4070)")
img_shot = save(fig, "shot.png")

# 4b. Lot pocisku (9924b7f)
fig, (a1, a2) = plt.subplots(1, 2, figsize=(7.2, 2.6))
r1 = a1.bar(["przed", "po"], [66, 36], color=[C_BEFORE, C_AFTER], width=0.55)
label_bars(a1, r1)
a1.set_ylim(0, 80); a1.set_ylabel("ms"); a1.set_title("Dorysowanie prostokąta z pociskiem", fontsize=9)
r2 = a2.bar(["przed", "po"], [24, 30], color=[C_BEFORE, C_AFTER], width=0.55)
label_bars(a2, r2)
a2.set_ylim(0, 40); a2.set_ylabel("klatek / s"); a2.set_title("Okna 100 klatek ze strzałami", fontsize=9)
img_bullet = save(fig, "bullet.png")

# 6. Kompilacja pod 68060
fig, ax = plt.subplots(figsize=(7.2, 2.6))
items = ["pole widzenia obcego\n(2–3 sprawdzenia widoczności)", "pole widzenia\npo trafieniu kuli", "zatrzymanie logiki\nprzy trafieniu"]
bms, ams = [100, 213, 320], [10, 91, 0]
ys = range(len(items))
ax.barh([i + 0.2 for i in ys], bms, 0.4, color=C_BEFORE, label="-mcpu=68020")
ax.barh([i - 0.2 for i in ys], ams, 0.4, color=C_AFTER, label="+ -mtune=68060")
for i, (b, a) in enumerate(zip(bms, ams)):
    ax.text(b + 4, i + 0.2, "%g" % b, va="center", fontsize=8)
    ax.text(a + 4, i - 0.2, "%g" % a if a else "< 300 (nie zalogowane)", va="center", fontsize=8)
ax.set_yticks(list(ys)); ax.set_yticklabels(items, fontsize=8); ax.invert_yaxis()
ax.set_xlabel("ms (środki zakresów)"); ax.set_xlim(0, 400); ax.legend(fontsize=8, loc="lower right")
ax.set_title("Ten sam kod, kompilacja pod 68020 i pod 68060 (misja4, strzelanie)")
img_tune = save(fig, "tune.png")

# 5. Elementy klatki (ms)
fig, ax = plt.subplots(figsize=(7.2, 2.6))
items = ["porównanie i kopia ekranu\n(Screen::flip, 640×400)", "cała klatka na kartę\n(640×400)", "wysłanie na kartę\nprzy marszu"]
bms, ams = [18, 8.2, 19.5], [8, 5.0, 4]
ys = range(len(items))
ax.barh([i + 0.2 for i in ys], bms, 0.4, color=C_BEFORE, label="przed")
ax.barh([i - 0.2 for i in ys], ams, 0.4, color=C_AFTER, label="po")
for i, (b, a) in enumerate(zip(bms, ams)):
    ax.text(b + 0.6, i + 0.2, "%g" % b, va="center", fontsize=8)
    ax.text(a + 0.6, i - 0.2, "%g" % a, va="center", fontsize=8)
ax.set_yticks(list(ys)); ax.set_yticklabels(items, fontsize=8); ax.invert_yaxis()
ax.set_xlabel("ms"); ax.set_xlim(0, 24); ax.legend(fontsize=8, loc="upper right")
ax.set_title("Obraz na kartę RTG – wybrane koszty")
img_parts = save(fig, "parts.png")

# ---- dokument
ss = {
    "h1": ParagraphStyle("h1", fontName="Arial-Bold", fontSize=17, leading=21, spaceAfter=4),
    "h2": ParagraphStyle("h2", fontName="Arial-Bold", fontSize=12.5, leading=16, spaceBefore=8, spaceAfter=4),
    "p": ParagraphStyle("p", fontName="Arial", fontSize=9.3, leading=12.5, spaceAfter=4),
    "small": ParagraphStyle("s", fontName="Arial", fontSize=8, leading=10.5, textColor=colors.HexColor("#444444")),
    "cell": ParagraphStyle("c", fontName="Arial", fontSize=8, leading=10),
    "cellb": ParagraphStyle("cb", fontName="Arial-Bold", fontSize=8, leading=10),
}
P = lambda t, s="p": Paragraph(t, ss[s])
C = lambda t, b=False: Paragraph(t, ss["cellb" if b else "cell"])

def table(rows, widths, head=True):
    data = [[C(c, head and r == 0) for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if head else 0)
    st = [("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#b0b7c0")),
          ("VALIGN", (0, 0), (-1, -1), "TOP"),
          ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 3)]
    if head:
        st.append(("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dfe7f2")))
    for r in range(1 if head else 0, len(rows)):
        if r % 2 == 0:
            st.append(("BACKGROUND", (0, r), (-1, r), colors.HexColor("#f5f7fa")))
    t.setStyle(TableStyle(st))
    return t

def img(path, w=180):
    from reportlab.lib.utils import ImageReader
    iw, ih = ImageReader(path).getSize()
    return Image(path, width=w * mm, height=w * mm * ih / iw)

story = []
story += [P("AmiXcom na RTG – poprawki i pomiary", "h1"),
          P("Gałąź <b>feat/battle-scroll-640</b>, commity z 20–22.10.2026. Wszystkie liczby zmierzone na prawdziwych "
            "Amigach przez AmiAgenta (sondy <i>amigaPerfLog</i>, sceny sterowane skryptem), nie w emulatorze."),
          P("<b>Maszyny:</b> Amiga 1 – A1200, Warp1260 (68060), AmigaOS 3.2, Picasso96 2.455; Amiga 2 – A4000, "
            "Warp4070 (68060), AmigaOS 3.2. Obie karty mają tę samą grafikę RTG (FPGA), wyniki obu maszyn są zbliżone "
            "(np. przewijanie 23,5 ms na A1200 i 24,4 ms na A4000).", "p"),
          Spacer(1, 3)]

story.append(P("1. Podsumowanie – co dał każdy commit", "h2"))
story.append(table([
    ["Commit", "Data", "Co zmienia", "Najważniejszy wynik", "Maszyna"],
    ["8eed426", "20.10", "Natywna bitwa 640×400, Geoscape w 320×200, pomijanie niezmienionych klatek, szybsza kopia ekranu, cień globu",
     "Geoscape 14 → 49 kl./s; bitwa stoi 26 → 50, kursor 18 → 29, przewijanie 14 → 20 kl./s", "A1200 / Warp1260"],
    ["8ff2ed4", "20.10", "Przewijanie bitwy blitterem karty (warstwa VRAM, okno przewijania), szybszy globus, 3 poprawki zawieszeń",
     "Przewijanie 48,9 → 23,5 ms (20 → 37 kl./s); przerysowanie globu ~150 → ~40 ms", "A1200 + A4000"],
    ["d812c75", "20.10", "Globus obracany przytrzymaną strzałką: granice, ląd i radary bez liczb zmiennoprzecinkowych",
     "Obrót 11 → 15 kl./s, po przybliżeniu 7 → 10 kl./s", "A4000 / Warp4070"],
    ["35fd59b", "21.10", "Koniec migania panelu walki i okien (Options, Save/Load, menu strzału); amigaAutoLoad",
     "Miganie usunięte, 0 różnic w weryfikacji (505 klatek); test od razu na polu walki", "A4000 / Warp4070"],
    ["c66d3c0", "21.10", "Marsz żołnierza: bez pełnego przerysowania co krok, różnica faz przez VRAM, kopiowanie między fazami",
     "Marsz 14–17 → 25–31 kl./s (60–70 → 32–40 ms)", "A4000 / Warp4070"],
    ["6df1036", "21.10", "Strzelanie: pole widzenia po trafieniu tylko dla jednostek patrzących w to miejsce, bez pełnego przerysowania po kuli i wybuchu; poprawka strzałki",
     "Zatrzymanie przy trafieniu ~0,65 → ~0,28 s na kulę; ślady strzałki 25 → 0 różnic", "A4000 / Warp4070"],
    ["1b93556", "21.10", "Reakcje wrogów po każdym kroku: najpierw sprawdzenie widoczności, promienie celowania tylko dla wrogów, którzy widzą żołnierza",
     "Sprawdzanie reakcji 25–82 ms → poniżej 10 ms na krok (misja4)", "A4000 / Warp4070"],
    ["9924b7f", "21.10", "Lot pocisku: widoczność cząstek i cieni liczona raz na lot; numer wersji pod FPS; ekran bez paska tytułu, niewidoczny przełącznik ekranów w rogu",
     "Dorysowanie z pociskiem 45–88 → 24–48 ms; okna ze strzałami ~24 → ~30 kl./s", "A4000 / Warp4070"],
    ["641b926", "21.10", "Kompilacja pod 68040/68060 (-mtune=68060): bez 64-bitowych mnożeń, które 68060 emuluje w pułapkach",
     "Pole widzenia obcego 60–140 → 0–20 ms; pole widzenia po trafieniu 185–241 → 67–115 ms", "A4000 / Warp4070"],
    ["63c93e0", "21.10", "Menu główne: „AmiXcom” i „Version RTG &lt;numer&gt;”; licznik FPS bez numeru; bez linii o autorze portu",
     "Numer wersji widoczny w menu i w sdlmini.log", "A1200 / Warp1260"],
    ["541852d", "21.10", "Linia wersji mieści się pod tytułem; $VER w binarce (polecenie Version)",
     "„Version UFO” pokazuje numer i datę budowy", "A1200 / Warp1260"],
    ["0a7b71d", "21.10", "Domyślne opcje = ustawienia RTG, z którymi port był mierzony; „Restore Defaults” nie wyłącza modów",
     "Start bez options.cfg zapisuje dokładnie te wartości", "A1200 / Warp1260"],
    ["a722fa9", "22.10", "Filtry obrazu: tylko „Disabled” (HQx/xBRZ wymagają ekranu 32-bit, którego port nie ma)",
     "Brak opcji, która nie działa", "obie"],
    ["2efed5b", "22.10", "Pozycja „Mixed” w Display Resolution: Geoscape 320×200, bitwa natywnie 640×400",
     "Skale 1.5× (480×300): 200 porównań, 0 różnic", "A1200 / Warp1260"],
    ["89b76dc", "22.10", "Kamera goniąca pocisk przesuwa się przed sprawdzeniem pamięci mapy",
     "Daleki strzał: 80 519 px błędu → 0 różnic na 800 porównań", "A4000 / Warp4070"],
    ["224dbf4", "22.10", "Plansza „Hidden Movement” nie kasuje zaznaczeń pamięci mapy",
     "Tura obcych: 33 różnice → 0 na 1200 porównań", "A1200 / Warp1260"],
    ["4376ae0", "22.10", "Każda zmiana terenu (zniszczenie, drzwi) zgłasza pole do przerysowania",
     "Granaty i tura obcych: 0 różnic na 600 porównań", "A1200 / Warp1260"],
], [17, 11, 62, 60, 30]))
story.append(Spacer(1, 6))
story.append(img(img_battle, 175))
story.append(P("Wartości „przed” dla stania, kursora i przewijania pochodzą sprzed gałęzi (commit 8eed426); dla marszu – "
               "sprzed commitu c66d3c0 (marszu wcześniej nie mierzono osobno). Ruchu kursora po 8ff2ed4 nie mierzono ponownie.",
               "small"))

story.append(PageBreak())
story.append(P("2. Bitwa – przewijanie (8ff2ed4)", "h2"))
story.append(P("Mapa przewijana myszką przy krawędzi ekranu, 640×400. Każdy krok to osobna zmiana, mierzona osobno:"))
story.append(img(img_scroll, 175))
story.append(table([
    ["Krok", "ms / klatkę", "kl./s", "Na czym polega"],
    ["przed gałęzią", "48,9", "20", "każda klatka składana i wysyłana w całości"],
    ["mapa trzyma przesunięty obraz", "38,9", "24", "obraz przesuwany w pamięci zamiast składania od nowa"],
    ["+ zakres pól przy dorysowaniu", "35,7", "26", "dorysowanie liczy tylko pola w odsłoniętym pasku"],
    ["+ okno przewijania", "33,6", "28", "obraz jest oknem w większym buforze – bez przepisywania"],
    ["+ warstwa VRAM", "23,5", "37", "blitter karty przesuwa obraz w VRAM, dosyłane są tylko paski"],
], [48, 22, 14, 96]))
story.append(P("Na A4000 / Warp4070 ten sam test: 24,4 ms (36 kl./s). Bitwa stojąca: 50 kl./s.", "small"))

story.append(P("3. Bitwa – marsz żołnierza (c66d3c0)", "h2"))
story.append(P("Zapis <i>misja3</i> (ciemna misja TFTD) wczytany przez <i>amigaAutoLoad</i>, pięciu żołnierzy kolejno wysłanych "
               "kilka pól dalej. Zmierzone zakresy w oknach po 100 klatek; na wykresie środki zakresów."))
story.append(img(img_walk, 175))
story.append(table([
    ["Część klatki (ms)", "przed", "po 1+2", "po 1+2+3"],
    ["cała klatka", "60–70", "47–57", "32–40"],
    ["klatek / s", "14–17", "18–21", "25–31"],
    ["mapa", "18–20", "20–25", "10–13"],
    ["logika gry", "9–11", "7–10", "6–9"],
    ["interfejs", "15–18", "8–12", "7–9"],
    ["wysłanie na kartę", "19–20", "5–7", "3–5"],
    ["pełne przerysowanie mapy (~100 ms)", "8–10 na 100 klatek", "0–1", "0–1"],
    ["klatki szybką ścieżką VRAM", "~1%", "60–80%", "60–80%"],
], [62, 38, 38, 38]))
story.append(Spacer(1, 4))
story.append(P("<b>Poprawka 1:</b> krok zmieniał jasność pól wokół żołnierza (jego własne światło) i odkrywał pola – to "
               "unieważniało cały obraz mapy, ~100 ms co krok (widoczne szarpnięcie). Teraz zmienione pola trafiają na listę i "
               "są tylko dorysowane. <b>Poprawka 2:</b> przy zmianie fazy animacji (~10 razy na sekundę) na kartę idą tylko "
               "zmienione paski zamiast całej mapy. <b>Poprawka 3:</b> zmieniony obszar jest dorysowywany raz, a do pozostałych "
               "7 faz animacji kopiowany; w nich dorysowuje się tylko to, co zależy od fazy (animowane pola, strzałka nad "
               "żołnierzem, kursor, ogień)."))

story.append(Spacer(1, 8))
story.append(KeepTogether([P("4. Bitwa – strzelanie (6df1036)", "h2"),
    P("Zapis <i>misja4</i>: trzech żołnierzy oddaje z miejsca serię Auto i strzał Snap. Każda kula w tej misji coś niszczy "
      "(podłogę albo obiekt), więc teren naprawdę się zmienia. Spadki do ~9 kl./s to w większości zatrzymania gry przy "
      "każdym trafieniu, nie równy spadek klatek."),
    img(img_shot, 175)]))
story.append(table([
    ["Na jedną kulę", "przed (c66d3c0)", "po (6df1036)", "Co zmieniono"],
    ["pole widzenia po trafieniu", "340–370 ms", "97–159 ms", "liczone tylko dla jednostek, w których stożku widzenia leży zniszczone pole (2–3 z 9); gdy zmiana światła sięga dalej – wszystkie, jak w oryginale"],
    ["światło terenu + cień", "~55 ms", "~63 ms", "bez zmian; +8 ms na porównanie jasności przed i po"],
    ["pełne przerysowanie mapy", "1–2 × ~200 ms", "0", "wybuch i zmiana światła dorysowują tylko zmienione pola"],
    ["klatka zatrzymania", "440–480 ms + klatki po 200 ms", "~280 ms", "suma powyższych"],
    ["średnio w oknie z serią Auto", "~18 kl./s", "~20 kl./s", "lot kuli i wybuch nadal 30–50 ms na klatkę"],
], [40, 30, 26, 84]))
story.append(P("Losowanie celności i lot kuli bez zmian. Weryfikacja: mapa 0 różnic na 1000 porównań; skrócone pole widzenia "
               "porównane z pełnym, oryginalnym – 0 różnych pól i jednostek (sprawdzone na jednym trafieniu, tryb weryfikacji jest bardzo wolny).",
               "small"))
story.append(Spacer(1, 6))
story.append(KeepTogether([P("<b>Lot pocisku (9924b7f).</b> Sonda rozdzieliła czas dorysowania mapy: przy lecącym pocisku jeden piksel "
      "kosztował ~2,5 µs (marsz ~0,8, wybuch ~0,6). Przyczyna: dla każdej z 35 cząstek pocisku i jej cienia gra sprawdza "
      "promieniem voxelowym, czy są widoczne – od nowa w każdym prostokącie dorysowania i w każdej klatce. Wynik zależy tylko "
      "od punktu toru i terenu, które w czasie lotu się nie zmieniają, więc teraz jest liczony raz na lot. Wykresy: środki zakresów."),
    img(img_bullet, 150),
    table([
        ["Misja4, te same strzały", "przed (1b93556)", "po (9924b7f)"],
        ["dorysowanie prostokąta z pociskiem", "45–88 ms (~2,5 µs/px)", "24–48 ms (1,5–2,0 µs/px)"],
        ["okno 100 klatek ze strzałami", "3,6–4,9 s (~24 kl./s)", "3,0–4,0 s (~30 kl./s)"],
        ["weryfikacja mapy", "–", "800 porównań, 0 różnic"],
    ], [62, 56, 62]),
    P("Pole widzenia po trafieniu trwało w tym przebiegu 185–241 ms (wcześniej 97–159 ms przy tej samej liczbie jednostek); "
      "tej części nie zmieniano, przyczyna (inne miejsca trafień) niesprawdzona.", "small")]))

story.append(Spacer(1, 8))
story.append(KeepTogether([P("5. Bitwa – reakcje wrogów przy marszu (1b93556)", "h2"),
    P("Po każdym kroku gra sprawdza, czy któryś wróg odda strzał w reakcji. Dla wroga, który ma żołnierza w polu widzenia, "
      "oryginał najpierw liczy promienie celowania (<i>canTargetUnit</i>), a dopiero potem pyta, czy wróg w ogóle widzi "
      "żołnierza (<i>visible</i>). Sonda w misja4 pokazała, że cały koszt to promienie dla wroga, który i tak żołnierza nie widział."),
    table([
        ["Na jeden krok (misja4, marsz)", "przed", "po"],
        ["sprawdzanie reakcji wrogów", "25–82 ms (24 kroki w teście)", "poniżej 10 ms (żaden krok nie przekroczył progu)"],
        ["promienie celowania (canTargetUnit)", "25–82 ms", "tylko gdy wróg widzi żołnierza"],
        ["widoczność (visible)", "< 1 ms", "< 1 ms"],
        ["logika gry średnio na klatkę", "~9,6 ms", "~9,0 ms (w granicach rozrzutu)"],
    ], [62, 56, 62]),
    P("Wynik gry ten sam: oba warunki muszą być spełnione, a żaden nie zmienia tego, co czyta drugi. Weryfikacja "
      "(stara kolejność liczona jeszcze raz na tym samym stanie): 11 porównań, 0 różnic. W teście w zasięgu był jeden wróg; "
      "przy kilku wrogach zysk na krok rośnie – tego nie mierzono.", "small")]))

story.append(Spacer(1, 8))
story.append(KeepTogether([P("6. Kompilacja pod 68040/68060 i sprzętowy FPU (641b926)", "h2"),
    P("Przy <i>-mcpu=68020</i> gcc zamienia każde dzielenie przez stałą na 64-bitowe mnożenie MULS.L. 68060 nie ma tej "
      "instrukcji – każde wywołanie to pułapka emulowana przez 68060.library (~1,3 µs zamiast ~0,4 µs). W binarce było 406 takich "
      "miejsc; najdroższe w <i>voxelCheck</i> (dzielenie przez 24 przy każdym voxelu promieni wzroku i strzału). Z "
      "<i>-mtune=68060</i> gcc używa 32-bitowego DIVS.L. Kod nadal działa na 68020 (wolniej); <i>-mcpu=68040</i> odpada, bo w tym "
      "toolchainie wybiera bibliotekę dla 68881."),
    img(img_tune, 165),
    table([
        ["Misja4, strzelanie", "-mcpu=68020", "+ -mtune=68060"],
        ["pole widzenia obcego (2–3 sprawdzenia widoczności)", "60–140 ms", "0–20 ms"],
        ["pole widzenia po trafieniu kuli", "185–241 ms", "67–115 ms"],
        ["zatrzymanie logiki przy trafieniu", "300–340 ms", "poniżej progu 300 ms"],
        ["dorysowanie z pociskiem", "1,5–2,0 µs/px", "1,15–1,5 µs/px"],
    ], [80, 50, 50])]))
story.append(Spacer(1, 6))
story.append(KeepTogether([P("<b>Sprzętowy FPU 68060 (wariant -fpu) – zmierzony, bez zysku.</b> Ten sam kod (61), obie wersje z "
      "-mtune=68060, te same sceny:"),
    table([
        ["Amiga 2, numer 61", "bez FPU", "z FPU"],
        ["pole widzenia po trafieniu (4 trafienia)", "66–117 ms (zwykle ~68)", "115–120 ms"],
        ["strzelanie, średnio na 100 klatek", "2,76 s", "2,92 s"],
        ["marsz (misja3), średnio na 100 klatek", "3,01 s", "2,97 s"],
    ], [80, 50, 50]),
    P("Średnie w granicach rozrzutu; jedyna wyraźna różnica na niekorzyść FPU. Prawdopodobnie libnix nie ma wersji dla 68881 "
      "i funkcje zwracające liczby zmiennoprzecinkowe idą drogą programową (komentarz w build.sh) – niesprawdzone. "
      "Główna wersja zostaje bez FPU.", "small")]))

story.append(Spacer(1, 8))
story.append(KeepTogether([P("7. Geoscape – globus (8eed426, 8ff2ed4, d812c75)", "h2"), img(img_geo, 178)]))
story.append(table([
    ["Pomiar", "przed", "po", "Commit"],
    ["Geoscape, czas płynie: 640×400 wszędzie → tryb mieszany 320×200", "14 kl./s", "49 kl./s (obrót 46, zoom 42)", "8eed426"],
    ["cień globu na jedno przerysowanie, 640×400", "~170 ms", "~54 ms", "8eed426"],
    ["cień globu na jedno przerysowanie, 320×200", "40–53 ms", "15–20 ms", "8eed426"],
    ["przerysowanie obróconego globu (kopiowanie wielokątów)", "~150 ms", "~40 ms", "8ff2ed4"],
    ["obrót przytrzymaną strzałką, bez przybliżenia", "11 kl./s", "15 kl./s", "d812c75"],
    ["obrót przytrzymaną strzałką, przybliżony", "7 kl./s", "10 kl./s", "d812c75"],
    ["granice państw na przerysowanie (przybliżenie)", "~100 ms", "~12 ms", "d812c75"],
    ["wielokąty lądu", "15–28 ms", "8–18 ms", "d812c75"],
    ["okręgi radarów", "~10 ms", "~6 ms", "d812c75"],
], [88, 24, 42, 26]))
story.append(P("Na wykresie cień 320×200 i ląd jako środki zakresów. Płynność globu po przybliżeniu – odłożona na później.", "small"))

story.append(KeepTogether([P("8. Obraz na kartę RTG (8eed426, 8ff2ed4, c66d3c0)", "h2"), img(img_parts, 175)]))
story.append(table([
    ["Element", "przed", "po", "Jak"],
    ["Screen::flip 640×400 (porównanie i kopia)", "18 ms", "7–9 ms", "porównanie długimi słowami, kopia ciągami"],
    ["cała klatka na kartę 640×400", "8,2 ms", "5,0 ms", "MOVE16 na 68060 (amiga_copy)"],
    ["pomijanie niezmienionych klatek", "–", "0 różnic w weryfikacji", "kolejka operacji porównywana z poprzednią klatką"],
], [62, 20, 36, 62]))

story.append(Spacer(1, 8))
story.append(CondPageBreak(60 * mm))
story.append(P("9. Poprawki błędów (bez liczb wydajności)", "h2"))
story.append(table([
    ["Commit", "Błąd", "Przyczyna i poprawka", "Sprawdzenie"],
    ["8ff2ed4", "Assert „value &lt; 65536” przy zapisie bitwy, zawieszenia, Guru 81000005",
     "Zapis 64 000 bajtów do zwolnionego bufora 320×200 po zmianie rozdzielczości (use-after-free); znalezione budową z kontrolą sterty (heapguard)",
     "3 przebiegi na A1200, 1 na A4000, heapguard 0 zgłoszeń"],
    ["8ff2ed4", "Ctrl-C zostawiał otwarty ekran i strażnika w zwolnionym kodzie (Guru)",
     "Ctrl-C jest teraz zdarzeniem wyjścia, strażnik zatrzymywany przy każdym wyjściu", "test na A1200"],
    ["8ff2ed4", "Możliwy samoistny reset przy podwójnym buforze ekranu",
     "Zamiast ChangeScreenBuffer: ukryta bitmapa i jeden blit (ChangeScreenBuffer czekał też ~5 ms na wygaszanie)", "testy na obu Amigach"],
    ["35fd59b", "Miganie panelu walki, okien Options / Save / Load i menu strzału",
     "Klatka bez przesunięcia mapy była rysowana wprost na widocznym ekranie; teraz zawsze w ukrytej bitmapie (+~0,5 ms)",
     "0 różnic na 505 klatkach; potwierdzone na ekranie"],
    ["6df1036", "Strzałka nad wybranym żołnierzem zostawała w obrazie mapy przy strzelaniu (błąd z c66d3c0)",
     "Strzałka pokazuje się lub znika ze zmianą typu kursora (podczas lotu kuli go nie ma); zmiana typu zaznacza teraz strzałkę we wszystkich fazach",
     "test strzelania: 25 różnic przed, 0 po"],
    ["89b76dc", "Daleki strzał z płynną kamerą (battleSmoothCamera): obraz mapy przesunięty względem pamięci",
     "Kamera goniła pocisk wewnątrz rysowania terenu, już po tym, jak pamięć mapy dopasowała przesunięcie ekranu; teraz kamera rusza się pierwsza",
     "misja4: 80 519 px błędu przed; 800 porównań, 0 różnic po (kamera płynna i zwykła)"],
    ["224dbf4", "W turze obcych po powrocie mapy przez chwilę stary obraz (np. teren sprzed strzału) – widoczne dla gracza",
     "Przy planszy „Hidden Movement” naprawa pól rysowała planszę zamiast mapy, ale kasowała zaznaczenia; teraz zaznaczenia czekają na mapę",
     "misja1: 33 różnice (do 26 565 px) przed; ponad 1200 porównań, 0 po"],
    ["4376ae0", "Ściana zniszczona 3–5 pól od wybuchu mogła zostać w obrazie mapy (znalezione w kodzie, nie zaobserwowane)",
     "Zmiana części pola (setMapData) nie trafiała na listę zmienionych pól; teraz każda zmiana terenu ją zgłasza",
     "2 granaty i tura obcych: 600 porównań, 0 różnic"],
], [17, 45, 78, 40]))

story.append(Spacer(1, 8))
story.append(KeepTogether([P("10. Opcje i menu (63c93e0, 541852d, 0a7b71d, a722fa9, 2efed5b)", "h2"),
    table([
        ["Commit", "Zmiana", "Po co"],
        ["0a7b71d", "Domyślne: amigaAppBar false, amigaNativeScale true, battlescapeScale 2, maximizeInfoScreens true; "
                    "„Restore Defaults” zostawia listę modów",
         "„Restore Defaults” wyłączało natywną bitwę 640×400, ekrany 320×200, ukryty pasek i zainstalowany mod (Polska)"],
        ["2efed5b", "Display Resolution: 320×200 → 640×400 → 640×480 → MIXED",
         "Ustawienie, z którym port jest mierzony, wybierane w menu zamiast w options.cfg; Cancel i odliczanie je cofają"],
        ["a722fa9", "Filter: tylko „Disabled”",
         "HQx/xBRZ przełączają na ekran 32-bit, którego warstwa obrazu portu nie pokazuje; „Scale” nic tu nie robi"],
        ["63c93e0, 541852d", "Menu: „AmiXcom” / „Version RTG &lt;numer&gt;”; $VER: AmiXcom &lt;numer&gt; (&lt;data&gt;)",
         "Gracz i polecenie Version widzą, która to wersja"],
    ], [30, 75, 75])]))

story.append(Spacer(1, 8))
story.append(KeepTogether([P("11. Testy opcji na prawdziwych Amigach (22.10)", "h2"),
    P("Lista 16 opcji, które mogły kolidować ze zmianami bitwy i obrazu, sprawdzana pojedynczo z <i>amigaMapVerify 1</i>. "
      "Punkty 1–5 sprawdzone, 6–16 wstrzymane (szczegóły i plan: TESTY-OPCJI.md w repo roboczym)."),
    table([
        ["#", "Opcja", "Wynik"],
        ["1", "Skale Geoscape/Battlescape (1.5×, 1/3, 1/2) z „Mixed”", "Działa: 480×300, verr 0, czyste wyjście (A1200)"],
        ["2", "Display Mode (Windowed / Borderless / Resizable)", "Bez wpływu w porcie (sdlmini pomija flagi); 0 różnic (A4000)"],
        ["3", "Filter (Scale / HQx / xBRZ)", "Usunięte z menu (a722fa9)"],
        ["4", "battleSmoothCamera", "Błąd znaleziony i poprawiony (89b76dc)"],
        ["5", "battleExplosionHeight / wybuchy", "„Duch” po wybuchu nieodtworzony; przy okazji poprawki 224dbf4 i 4376ae0"],
    ], [8, 72, 100]),
    P("Otwarte: F12 (zrzut ekranu) zamyka grę bez śladu – przyczyna nieustalona. Wysyłka przez AmiAgenta raz uszkodziła "
      "60 bajtów binarki; każdy wgrany plik jest teraz sprawdzany sumą SHA-256.", "small")]))

story.append(P("12. Jak mierzono i sprawdzano", "h2"))
story.append(P("• Każda zmiana mierzona osobno na prawdziwej Amidze, ta sama scena sterowana skryptem (autoinput), sondy "
               "<i>amigaPerfLog</i> co 100 klatek: logika, mapa, interfejs, wysłanie na kartę."))
story.append(P("• <b>amigaMapVerify 1</b> – każda klatka z pamięci podręcznej mapy porównywana ze złożoną od zera: "
               "w testach marszu 800 porównań, 0 różnic (po każdej z trzech poprawek)."))
story.append(P("• <b>amigaFrameSkip 2</b> – każda klatka z szybkiej ścieżki (pomijanie, warstwa VRAM) porównywana z obrazem "
               "zrobionym zwykłą drogą i odczytanym z karty: 0 różnic."))
story.append(P("• <b>Numer wersji</b> – liczba commitów: w menu głównym „Version RTG &lt;numer&gt;” (63c93e0; wcześniej, "
               "od 9924b7f, pod licznikiem FPS), w poleceniu Version ($VER, 541852d) i z identyfikatorem, np. „build: 68 224dbf4+”, "
               "w sdlmini.log. Ekran bez paska tytułu (amigaAppBar wyłączone): kliknięcie w prawy górny róg (16×10 pikseli) "
               "przenosi ekran gry za Workbench."))
story.append(P("• <b>Testy z graczem</b> (22.10) – błędy, których skrypt nie wywoła (tura obcych z walką, rzuty granatem), "
               "sprawdzane w grze prowadzonej przez użytkownika z <i>amigaMapVerify</i>; log porównuje każdą klatkę."))
story.append(P("• <b>amigaAutoLoad</b> (35fd59b) – gra wczytuje podany zapis od razu po starcie; test zaczyna się na polu walki "
               "po ~2 minutach zamiast po przejściu menu, globusa i ekranu wyposażenia."))
story.append(Spacer(1, 6))
story.append(P("Zakresy (np. 25–31 kl./s) to rozrzut między oknami pomiarowymi tej samej sceny. Liczby z commitów 8eed426 "
               "i 8ff2ed4 pochodzą z A1200 / Warp1260, kolejne do 641b926 z A4000 / Warp4070, z 22.10 – z obu (kolumna „Maszyna”); "
               "karty graficzne są takie same.",
               "small"))

def footer(c, d):
    c.saveState()
    c.setFont("Arial", 7.5)
    c.setFillColor(colors.HexColor("#666666"))
    c.drawString(15 * mm, 9 * mm, "AmiXcom – poprawki RTG, gałąź feat/battle-scroll-640, 22.10.2026")
    c.drawRightString(195 * mm, 9 * mm, "str. %d" % d.page)
    c.restoreState()

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=15 * mm, rightMargin=15 * mm, topMargin=14 * mm,
                        bottomMargin=16 * mm, title="AmiXcom – poprawki RTG", author="AmiXcom")
doc.build(story, onFirstPage=footer, onLaterPages=footer)
print(OUT)
