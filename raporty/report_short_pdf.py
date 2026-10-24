"""Krótki raport PDF: zmiany AmiXcom RTG w punktach i zysk w procentach.
Liczby te same co w report_pdf.py (opisy commitów, pomiary na prawdziwych Amigach);
dla zakresów brany jest środek zakresu."""
import sys
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, KeepTogether
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

OUT = sys.argv[1]
pdfmetrics.registerFont(TTFont("Arial", r"C:\Windows\Fonts\arial.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Bold", r"C:\Windows\Fonts\arialbd.ttf"))

ss = {
    "h1": ParagraphStyle("h1", fontName="Arial-Bold", fontSize=16, leading=20, spaceAfter=3),
    "h2": ParagraphStyle("h2", fontName="Arial-Bold", fontSize=11.5, leading=15, spaceBefore=7, spaceAfter=3),
    "p": ParagraphStyle("p", fontName="Arial", fontSize=9, leading=12, spaceAfter=3),
    "li": ParagraphStyle("li", fontName="Arial", fontSize=9, leading=12, leftIndent=10, bulletIndent=0, spaceAfter=1.5),
    "small": ParagraphStyle("s", fontName="Arial", fontSize=7.5, leading=10, textColor=colors.HexColor("#444444")),
    "cell": ParagraphStyle("c", fontName="Arial", fontSize=8.5, leading=10.5),
    "cellb": ParagraphStyle("cb", fontName="Arial-Bold", fontSize=8.5, leading=10.5),
    "gain": ParagraphStyle("g", fontName="Arial-Bold", fontSize=8.5, leading=10.5, alignment=2,
                           textColor=colors.HexColor("#1f5a9e")),
}
P = lambda t, s="p": Paragraph(t, ss[s])
LI = lambda t: Paragraph(t, ss["li"], bulletText="•")


def num(v):
    return ("%g" % v).replace(".", ",")


def faster(before, after, unit, shown=None, atleast=False):
    """Czas: o ile procent krócej. shown: zakres do pokazania (procent ze środków)."""
    val = shown or "%s → %s" % (num(before), num(after))
    return "%s %s" % (val, unit), "%s%d%% krócej" % ("ponad " if atleast else "", round(100 * (1 - after / before)))


def more(before, after, unit="kl./s", shown=None):
    """Klatki na sekundę: o ile procent więcej."""
    val = shown or "%s → %s" % (num(before), num(after))
    return "%s %s" % (val, unit), "+%d%%" % round(100 * (after / before - 1))


# (zmiana, (przed → po, zysk), uwaga do liczb)
SECTIONS = [
    ("Bitwa 640×400", [
        ("Bitwa stoi", more(26, 50), ""),
        ("Ruch kursora", more(18, 29), ""),
        ("Przewijanie mapy (blitter karty, warstwa VRAM)", more(14, 37), "48,9 → 23,5 ms na klatkę"),
        ("Marsz żołnierza", more(15.5, 28, shown="14–17 → 25–31"), ""),
        ("Zatrzymanie przy trafieniu kuli", faster(0.65, 0.28, "s", shown="~0,65 → ~0,28"), ""),
        ("Reakcje wrogów po każdym kroku", faster(53.5, 10, "ms", shown="25–82 → poniżej 10", atleast=True), ""),
        ("Lot pocisku – dorysowanie mapy", faster(66, 36, "ms", shown="45–88 → 24–48"), ""),
        ("Lot pocisku – okna ze strzałami", more(24, 30), ""),
        ("Kompilacja pod 68060 – pole widzenia obcego", faster(100, 10, "ms", shown="60–140 → 0–20"), ""),
        ("Kompilacja pod 68060 – pole widzenia po trafieniu", faster(213, 91, "ms", shown="185–241 → 67–115"), ""),
    ]),
    ("Geoscape", [
        ("Czas płynie (Geoscape w 320×200)", more(14, 49), ""),
        ("Przerysowanie obróconego globu", faster(150, 40, "ms"), ""),
        ("Cień globu 640×400", faster(170, 54, "ms"), ""),
        ("Cień globu 320×200", faster(46, 17, "ms", shown="40–53 → 15–20"), ""),
        ("Obrót globu strzałką", more(11, 15), ""),
        ("Obrót globu po przybliżeniu", more(7, 10), ""),
        ("Granice państw (przybliżenie)", faster(100, 12, "ms"), ""),
        ("1 DAY – spokojny dzień", faster(2.61, 0.32, "s"), "wariant FPU"),
        ("1 DAY – statek w locie", faster(5.46, 0.85, "s"), "wariant FPU"),
        ("1 DAY – UFO w powietrzu", faster(2.95, 0.62, "s", shown="1,88–4,02 → 0,45–0,79"), "wariant FPU"),
    ]),
    ("Obraz na kartę RTG", [
        ("Porównanie i kopia ekranu (Screen::flip 640×400)", faster(18, 8, "ms", shown="18 → 7–9"), ""),
        ("Cała klatka na kartę 640×400 (MOVE16)", faster(8.2, 5.0, "ms"), ""),
    ]),
    ("Zapis i wczytywanie (szybki alokator pamięci)", [
        ("Budowanie autozapisu Geoscape", faster(8.3, 0.14, "s"), ""),
        ("Rozbiór zapisu przy wczytaniu (Terror1)", faster(4.88, 0.62, "s"), ""),
        ("Start gry do wczytania zapisu", faster(47, 30.5, "s"), ""),
    ]),
]

FIXES = [
    "Koniec migania panelu walki i okien (Options, Save/Load, menu strzału).",
    "Okienka komunikatów (panika, „saving game”) nie zostają już na ekranie.",
    "Usunięte zawieszenia i Guru: zapis do zwolnionego bufora po zmianie rozdzielczości, Ctrl-C, podwójny bufor ekranu.",
    "Obraz mapy zawsze zgodny z grą: daleki strzał z płynną kamerą, tura obcych, zniszczony teren, strzałka nad żołnierzem "
    "(w testach 0 różnic).",
    "Zrzut ekranu nie zamyka już gry.",
    "W trybie WARP 640×480 bitwa po wczytaniu zapisu ma właściwą rozdzielczość.",
    "Szybki alokator oddaje pamięć przy wyjściu (wcześniej ~20 MB Fast RAM na każde uruchomienie).",
    "Intro się odtwarza (odtwarzacz FLI czytał plik źle na 68k); bez 10 s czarnego ekranu po wczytaniu; dźwięki intro nieucinane.",
    "Brak okna konsoli na Workbenchu przy starcie z ikony.",
]

FEATURES = [
    "Natywna bitwa 640×400 i 640×480 (pozycje „WARP RTG” w Display Resolution), Geoscape i ekrany informacyjne w 320×200.",
    "Muzyka i efekty dźwiękowe z wersji Amiga (z dysków gracza) do wyboru obok muzyki z PC.",
    "Okienko postępu przy zapisie i wczytywaniu (procent, kręcąca się kropka).",
    "F10 – zgłoszenie błędu (zrzut, opis stanu, zapis gry); Help – zrzut ekranu.",
    "Ekran bez paska tytułu; przełączenie na Workbench kliknięciem w prawy górny róg (2×2 piksele).",
    "Numer wersji w menu głównym i w poleceniu Version (0.86: 0 = beta); domyślne opcje = ustawienia RTG, z którymi port był mierzony.",
    "Intro UFO z oryginalnym dźwiękiem w trakcie wczytywania gry; Esc / klik: obraz tytułowy z „LOADING nn %”.",
    "Options → Amiga po polsku, z kartą Warp (CARD / FIRMWARE / CPU CLOCK) i opcją POKAZUJ FPS.",
]


def table(rows):
    data = [[Paragraph("Zmiana", ss["cellb"]), Paragraph("Przed → po", ss["cellb"]),
             Paragraph("Zysk", ss["cellb"])]]
    for name, (val, gain), note in rows:
        v = val + ("<br/><font size=7 color='#555555'>%s</font>" % note if note else "")
        data.append([Paragraph(name, ss["cell"]), Paragraph(v, ss["cell"]), Paragraph(gain, ss["gain"])])
    t = Table(data, colWidths=[88 * mm, 62 * mm, 30 * mm], repeatRows=1)
    st = [("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#b0b7c0")),
          ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
          ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dfe7f2")),
          ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 4)]
    for r in range(2, len(data), 2):
        st.append(("BACKGROUND", (0, r), (-1, r), colors.HexColor("#f5f7fa")))
    t.setStyle(TableStyle(st))
    return t


story = [P("AmiXcom na RTG – zmiany w skrócie", "h1"),
         P("Commity z 20–24.10.2026 (gałąź feat/battle-scroll-640), wydanie 0.86 beta. Pomiary na prawdziwych Amigach: "
           "A1200 / Warp1260 i A4000 / Warp4070 (68060, ta sama grafika RTG). Szczegóły w raporcie "
           "„AmiXcom-poprawki-RTG.pdf”.")]
story.append(P("Szybkość", "h2"))
for title, rows in SECTIONS:
    story.append(Paragraph(title, ParagraphStyle("h3", parent=ss["p"], fontName="Arial-Bold", spaceBefore=2)))
    story.append(table(rows))
    story.append(Spacer(1, 4))
story.append(P("Zysk: przy klatkach na sekundę – o ile więcej klatek; przy czasie – o ile procent krócej trwa. "
               "Dla zakresów (np. 25–31 kl./s) procent liczony ze środków zakresów.", "small"))
cmp_rows = [["Scena", "640×400", "640×480", "Różnica"],
            ["Bitwa stoi", "50 kl./s", "50 kl./s", "bez różnicy"],
            ["Ruch kursora", "47 kl./s", "46 kl./s", "−2%"],
            ["Ciągłe przewijanie", "37 kl./s", "34 kl./s", "−8%"],
            ["Przewijanie klawiszami", "30–32 kl./s", "25–28 kl./s", "−14%"]]
ct = Table([[Paragraph(c, ss["cellb" if r == 0 else "cell"]) for c in row] for r, row in enumerate(cmp_rows)],
           colWidths=[88 * mm, 31 * mm, 31 * mm, 30 * mm])
ct.setStyle(TableStyle([("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#b0b7c0")),
                        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dfe7f2")),
                        ("LEFTPADDING", (0, 0), (-1, -1), 3)]))
story.append(KeepTogether([P("Bitwa w 640×480 a 640×400 (Amiga 1, build 95, ten sam dzień)", "h2"), ct,
    P("640×480 ma o 20% więcej pikseli; klatki na sekundę spadają o 0–14%. Marszu i strzelania w 640×480 nie mierzono.",
      "small")]))
story.append(P("Poprawione błędy", "h2"))
story += [LI(t) for t in FIXES]
story.append(P("Nowe funkcje", "h2"))
story += [LI(t) for t in FEATURES]


def footer(c, d):
    c.saveState()
    c.setFont("Arial", 7.5)
    c.setFillColor(colors.HexColor("#666666"))
    c.drawString(15 * mm, 9 * mm, "AmiXcom – zmiany RTG w skrócie, 24.10.2026")
    c.drawRightString(195 * mm, 9 * mm, "str. %d" % d.page)
    c.restoreState()


doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=15 * mm, rightMargin=15 * mm, topMargin=14 * mm,
                        bottomMargin=16 * mm, title="AmiXcom – zmiany w skrócie", author="AmiXcom")
doc.build(story, onFirstPage=footer, onLaterPages=footer)
print(OUT)
