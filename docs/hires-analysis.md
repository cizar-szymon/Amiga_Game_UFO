# Faza 0: natywne 640×400 na RTG – analiza

Stan na 2026-10-19. Kod: AmiXcom 0.9.14 (`f562103`), build Docker bez zmian w kodzie autora.
Plan: `PLAN-HIRES.md`. Decyzja użytkownika: **tylko natywne 320×200 i 640×400, bez skalowania**.

## Najważniejsze: to już działa, bez zmian w kodzie

OpenXcom (baza 2016) ma rozdzielczość bazową niezależną od 320×200, a port jej nie zablokował:

- `Screen::updateScale()` jest nietknięte. `geoscapeScale` / `battlescapeScale` = **5 (`SCALE_SCREEN`)**
  przy `displayWidth: 640`, `displayHeight: 400` (i `nonSquarePixelRatio: false`) daje rozdzielczość
  bazową 640×400. Gra rysuje wtedy natywnie w 640×400: większy glob, 4× większy widok bitwy,
  a okna 320×200 (menu, raporty) są wyśrodkowane w skali 1:1.
- **Options → Video** w grze jest nietknięte: lista rozdzielczości (z `SDL_ListModes`: 320×200,
  640×400, 640×480) i listy skal dla Geoscape i Battlescape. Gracz może to wybrać już dziś.
- sdlmini otwiera ekran RTG 640×400×8 (`amigagfx_open(640,400)`, LockBitMap + memcpy, bez c2p).

Sprawdzone w WinUAE (68060, uaegfx, OS 3.2) przez `winuae/perf-run.ps1 -Res n640`:
menu, Geoscape (wybór miejsca na bazę, obrót, zoom) i bitwa (kursor, przewijanie) działają, a kliknięcia
trafiają poprawnie (myszka 1:1, bez pasów). Zrzuty: `winuae/perf/*-n640-*.png`.

Na prawdziwym Warpie: patrz „Pomiary” niżej.

## Przegląd kodu: miejsca zależne od rozmiaru

| Miejsce | Co robi | Ryzyko dla 640×400 | Co zrobić |
|---|---|---|---|
| `native/sdlmini/src/sdlmini_backend.c:44,52` | wybór RTG/AGA sprawdza tylko, czy karta ma tryb **320×200** (`amigagfx_rtg_has_mode(320, 200)`) | karta bez 320×200, a z 640×400 spadnie na AGA | sprawdzać tryb, który gracz wybrał (`displayWidth/Height`), z fallbackiem |
| `native/oxc-replace/Engine/Zoom.cpp` | tryb 640×480: obraz 320×200 skalowany 2× z pasami; w każdej klatce `SDL_FillRect` całego ekranu i skalowanie z dzieleniem na piksel | **na Warpie 5–6 kl./s** (flip 146 ms) | zgodnie z decyzją nie skalujemy; ukryć 640×480 dla RTG albo zostawić jako „legacy” (do uzgodnienia z autorem) |
| `native/sdlmini/src/sdlmini_video.c:965` | lista trybów `320×200, 640×400, 640×480` | – | ewentualnie usunąć 640×480 z listy RTG |
| `native/amiga_splash.c:34`, `native/amiga_gfx.c:1797` | plansza ładowania 320×200; na ekranie o wysokości ≥ 400 jest raz powiększana 2× (`scale = (g_height >= 400) ? 2 : 1`) | brak: wypełnia ekran 640×400, koszt jednorazowy | – |
| `build/apply-amiga-patches.py` (Screen.cpp, 6amU) | `cursorInBlackBands = true` – myszka mapowana jak obraz | przy natywnym 640×400 brak pasów, skala 1:1 | działa (sprawdzone kliknięciami) |
| cache klatek bitwy (blok 6z w patcherze) | 8 klatek mapy, siatka brudnych kafli | bufory alokowane z `w_ * h_` i z wymiarów mapy, nie na sztywno | działa; pamięć 8 × 640×400 ≈ 2 MB |
| blity w asemblerze (`amiga_span_blit.s`, LUT span blit) | dostają `spitch`/`dpitch` jako parametry | – | działa |
| łatki w patcherze ogólnie | prawie wszędzie `->pitch`, `w_`, `h_`; stałe 320/200 tylko w kodzie upstreamu (np. `Mod.cpp: newWidth = 320 - 64`) | – | – |
| AGA (`open_screen` z c2p) | 640×400 na AGA = hires interlace, c2p 4× więcej | poza zakresem (tylko RTG) | nie dotykać |

## Wydajność: co będzie kosztowało w 640×400

Z sond `amigaPerfLog` (WinUAE, tylko porównawczo; Warp – niżej):

- **Geoscape**: rysowanie globu rośnie z powierzchnią. W WinUAE blit globu w bezruchu 16,7 ms
  (320×200: ~0), przy zoomie 78 ms na klatkę. Już w 320×200 na Warpie każde pełne przerysowanie
  globu to pojedyncza klatka ~300 ms (szarpnięcie przy obrocie/zoomie).
- **Bitwa**: cache klatek trzyma bezruch tanim; `flip` (wysłanie obrazu) rośnie z ~2,6 do ~10 ms
  w WinUAE, bo kopiowany jest 4× większy obszar.
- Kopia na kartę RTG (memcpy wiersz po wierszu, bajtowo wyrównana) – kandydat na MOVE16 według
  „Warp RTG Developer notes” (60 MB/s zamiast ~36 MB/s).

## Ograniczenia metody pomiaru

- Bitwa z `amigaAutoBattle` to **początek misji**: odkryty jest tylko teren wokół Skyrangera, reszta mapy
  jest czarna (mgła wojny), więc rysowanie mapy kosztuje mniej niż w środku walki. Lepiej mierzyć na
  zapisie gry z połowy misji (do przygotowania).
- Przewijanie przy krawędzi nie reaguje na ruchy wstrzyknięte przez autoinput; przewijamy strzałkami.
- Na AmigaOS 3.2 działająca gra trzyma `oxc.log` i `sdlmini.log` na wyłączność – logi czytamy po wyjściu.

## Pomiary na Warpie (68060, CSGFX)

| Scena | 320×200 | natywne 640×400 | 640×480 skalowane |
|---|---|---|---|
| Bitwa – bezruch | 50 kl./s (14,4 ms) | **26 kl./s** (37,1 ms = blit 18,4 + flip 18,1) | 6 kl./s |
| Bitwa – kursor | 43–46 kl./s (19–20 ms) | **18 kl./s** (53,3 ms = blit 34,4 + flip 18,3) | 5 kl./s |
| Bitwa – przewijanie | 45–46 kl./s (17 ms) | **16–17 kl./s** (58–59 ms) | 6 kl./s |
| Geoscape – bezruch | 50 kl./s (13,4 ms) | **17 kl./s** (57,4 ms = glob 36,1 + flip 17,9) | 6 kl./s |
| Geoscape – obrót | 42 kl./s (18,3 ms) | **11 kl./s** (86,2 ms = glob 64,3 + flip 18,1) | 6 kl./s |
| Geoscape – zoom | 46 kl./s (15,5 ms) | **4 kl./s** (229 ms = glob 207,9 + flip 17,2) | 6 kl./s |
| Start (do `splash: 99%`) | 47 s | 47 s | 47 s |

(ms = praca na klatkę; gra ma limit 50 kl./s; bitwa = początek misji z autobattle, `Tab` na żołnierza;
logi: `winuae/perf-real/*-n640-*`, `*-320-*`)

### Gdzie idzie czas w 640×400 (liczniki `perf:` sdlmini, na klatkę)

- **`flip` ~18 ms w każdej klatce** (w 320×200 ~4,7 ms, czyli proporcjonalnie do powierzchni).
  `Screen::flip()` przepisuje cały obraz gry (640×400) na ekran SDL **porównując piksel po pikselu**
  (`plain … px diff`), żeby sdlmini wiedziało, co się zmieniło. Kopia na kartę jest potem pomijana
  w 96–99 klatkach na 100 (`skipped`), więc to nie VRAM jest wąskim gardłem, tylko samo porównanie.
- **Cały ekran gry jest czyszczony w każdej klatce** (`fills` = 256 000 px na klatkę), a wszystkie stany
  są rysowane od nowa (upstreamowy `Game::run`). Autor opisał to już w `LEFTOFF.md` („Teach Game::run
  not to clear/re-blit states when NOTHING invalidated”) – w 640×400 waży to 4× więcej.
- **Bitwa w bezruchu: blit 18 ms**, choć cache klatek trafia w 100%: gotowy obraz mapy (640×~350)
  jest kopiowany do powierzchni ekranu w każdej klatce.
- **Geoscape: glob 36 ms w bezruchu, 208 ms przy zoomie** – rysowanie globu rośnie szybciej niż
  powierzchnia (w 320×200 w bezruchu ~0 ms). Do zbadania, czy w 640×400 nie gubi się dławik
  przerysowań (`AMIGA_GLOBE_MIN_MS`) albo cache globu.

### Optymalizacja 1: diff-copy długimi słowami (gałąź `feat/diffcopy-longwords`)

`native/sdlmini/src/sdlmini_video.c`: w `blit8` (kopiowanie obrazu gry na ekran z wykrywaniem zmian)
komórki 32 B porównywane są ośmioma porównaniami długich słów (`cell_differs`) zamiast `memcmp` z libnix
(pętla bajtowa za wywołaniem funkcji). Wynik porównania identyczny, więc identyczny obraz.

Warp, 2026-10-19, binarka `openxcom-rtg-dc` wobec `openxcom-rtg` (obie z tego samego toolchainu):

| Scena | przed | po |
|---|---|---|
| `flip` 640×400 | 18 ms | 7,0–9,5 ms |
| `flip` 320×200 | 4,7 ms | 2,4–2,7 ms |
| Bitwa 640×400 – bezruch | 26 kl./s (37,1 ms) | 35 kl./s (27,5 ms) |
| Bitwa 640×400 – kursor | 18 kl./s (53,3 ms) | 20 kl./s (47,9 ms) |
| Bitwa 640×400 – przewijanie | 16–17 kl./s (58 ms) | 21–22 kl./s (43–46 ms) |
| Geoscape 640×400 – bezruch | 17 kl./s (57,4 ms) | 21 kl./s (46,6 ms) |
| Geoscape 640×400 – obrót | 11 kl./s (86,2 ms) | 13 kl./s (75,9 ms) |
| Geoscape 640×400 – zoom | 4 kl./s (229 ms) | 6 kl./s (165 ms) * |
| Bitwa 320×200 – bezruch | 50 kl./s (14,4 ms) | 50 kl./s (12,9 ms) |
| Bitwa 320×200 – kursor | 43–46 kl./s (19–20 ms) | 49 kl./s (16,3 ms) |
| Bitwa 320×200 – przewijanie | 45–46 kl./s (17 ms) | 46 kl./s (15,7 ms) |

\* zoom zależy głównie od globu (blit 208 → 153 ms) – to raczej rozrzut niż efekt tej zmiany.

### Optymalizacja 2: pomijanie niezmienionych klatek (uzgodnione z autorem)

`native/sdlmini/src/sdlmini_video.c` + `SDL_video.h` + patcher (`6amY`, zgłoszenia zapisu w `Surface`).
Wypełnienia i blity na bufor ekranu gry są odkładane do kolejki (źródło, **wersja zawartości** źródła,
prostokąty, klucz koloru/kolor). Przy `Screen::flip` kolejka jest porównywana z poprzednią klatką;
ta sama lista = ten sam obraz, więc nie wykonuje się ani kolejki, ani porównania ekranu.
Wersja zawartości rośnie przy każdym zapisie pikseli: wywołania SDL i lock/unlock w sdlmini, w grze
`setPixel`, metody rysujące `Surface`, szybki `blitNShade`/blit przebiegów i kopia mapy z cache
(4 miejsca portu piszące bezpośrednio). Zapis do źródła czekającego w kolejce, bezpośredni zapis do
bufora, odczyt bufora, zwolnienie powierzchni → kolejka wykonuje się od razu, klatka liczona normalnie.

Opcja `amigaFrameSkip` (user/options.cfg): 0 wyłączone, **1 włączone (domyślnie)**, 2 weryfikacja:
klatka uznana za niezmienioną i tak jest składana, a suma kontrolna bufora przed/po sprawdza decyzję.

**Weryfikacja** (WinUAE, `amigaFrameSkip: 2`): Geoscape 640×400 i bitwa 640×400 (bezruch, kursor,
przewijanie strzałkami, zoom/obrót globu) – **0 rozbieżności**; 53–80% klatek niezmienionych.
Na Warpie w trybie 1 – 0 rozbieżności (licznik `verr` w linii `perf:`).

Warp, 2026-10-19, `openxcom-rtg-fs` (optymalizacja 1 + 2):

| Scena | przed | po opt. 1 | **po opt. 1 + 2** | klatek pominiętych |
|---|---|---|---|---|
| Bitwa 640×400 – bezruch | 26 kl./s | 35 | **49 kl./s** (12,9 ms) | 77% |
| Bitwa 640×400 – kursor | 18 | 20 | **26 kl./s** (33,1 ms) | 51% |
| Bitwa 640×400 – przewijanie | 16–17 | 21–22 | **27–29 kl./s** (26–29 ms) | 74–76% |
| Geoscape 640×400 – bezruch | 17 | 21 | **36 kl./s** (22,7 ms) | 70% |
| Geoscape 640×400 – obrót | 11 | 13 | **20 kl./s** (44,7 ms) | 64% |
| Geoscape 640×400 – zoom | 4 | 6 | **10 kl./s** (95,8 ms) | 67% |
| Bitwa 320×200 – bezruch | 50 (14,4 ms) | 50 (12,9 ms) | 50 kl./s (**9,5 ms**) | 78% |
| Bitwa 320×200 – kursor | 43–46 | 49 | 49 kl./s (13,5 ms) | 70% |
| Bitwa 320×200 – przewijanie | 45 | 46 | 46 kl./s (12 ms) | 82–84% |
| Geoscape 320×200 | 50 / 42 / 46 | – | 50 / 42 / 46 (12,2 / 16,6 / 14,5 ms) | 78–79% |

W 320×200 gra już stoi na limicie 50 kl./s; zysk to wolny czas procesora (mniej pracy na klatkę).
Co zostaje w 640×400: przerysowanie globu (cień ~170 ms na raz → obrót/zoom) i rysowanie kafli przy
ruchu kursora (`terr` 12 ms).

### Optymalizacja 3: cień globu przez tablicę (`drawShadowHalfFix`)

Patcher, blok 5u („CreateShadowFix shader”). `shadowApply(dest, v)` zależy tylko od `dest` i `min(v, 31)`
(dla `v <= 0` w ogóle nie od `v`), więc zastępuje ją tablica `[32][256]` (8 KB) zbudowana raz z niej samej
(ponownie, gdy zmieni się `OCEAN_COLOR` rulesetu). Pętla idzie wierszami: najpierw wartości bloków 2×2
pary wierszy, potem piksel po pikselu przez tablicę. **Bit w bit ten sam obraz**: `tools/shadowtest`
(stara i nowa funkcja na hoście, losowe globy 256×200, 576×400 i nieparzyste, 4 kolory oceanu, różne
słońca): 24,5 mln pikseli, 0 różnic.

Warp, `openxcom-rtg-sh` (opt. 1 + 2 + 3), sonda `globe:` (ms na przerysowanie):

| | przed | po |
|---|---|---|
| cień 640×400 | ~170 | **73–85** |
| cień 320×200 | 40–53 | **20** |
| Geoscape 640×400: bezruch / obrót / zoom | 36 / 20 / 10 kl./s | 36 / **23** / 10 kl./s |
| Geoscape 320×200 | 50 / 42 / 46 | 50 / 43 / 45 (limit) |

**3b: przedziały dysku zamiast odczytu `z` na każdy piksel.** Dysk jest wypukły i wyśrodkowany, więc
w każdym wierszu piksele wewnątrz to jeden przedział wokół środka; jego końce znajduje bisekcja (~20
odczytów na wiersz). Tablica normalnych (6 B/piksel, 1,4 MB w 640×400 – więcej niż L2 Warpa) jest czytana
już tylko w wierszach parzystych (punkty zaczepienia bloków) i dla pikseli krawędzi. `tools/shadowtest`:
0 różnic na 24,5 mln pikseli; kod w patcherze jest wstawiany z testu (`tools/shadowtest/sync_patch.py`).

| Warp, cień na przerysowanie | start | tablica | tablica + przedziały |
|---|---|---|---|
| 640×400 | ~170 ms | 73–85 ms | **52–56 ms** |
| 320×200 | 40–53 ms | 20 ms | **15–20 ms** |

Geoscape 640×400 (`openxcom-rtg-sh2`): bezruch 36, obrót **24**, zoom 10 kl./s. Całe przerysowanie
globu w 640×400 ~130 ms: cache wielokątów 20–85 ms, ląd 26–48 ms, cień 52–56 ms – cień przestał być
dominującą pozycją.

### Prawdziwy Geoscape (zapis gracza, czas 5 s) – korekta

Wcześniejsze „Geoscape – bezruch” to ekran wyboru miejsca na bazę przy **zatrzymanym czasie**. Przy
płynącym czasie zegar zmienia się prawie w każdej klatce, więc pomijanie klatek prawie nie działa
(2–13% klatek). Scena `geoq` (`winuae/perf-real.ps1 -Scene geoq`: zapis gracza `geoscape5sec.sav`
wczytany przez F9), Warp, 640×400:

| Wersja | czas płynie | obrót | zoom (duży rozrzut) |
|---|---|---|---|
| bez zmian | 14 kl./s (69 ms) | 12 | 4 |
| opt. 1 | 17 kl./s (60 ms) | 13 | 13 |
| opt. 1 + 2 + 3 | 16 kl./s (61 ms) | 16 | 8 |

Gracz widzi to samo (15–17 kl./s). ~40 ms na klatkę to składanie całego ekranu 640×400 w Fast RAM.
Następny krok: składanie tylko zmienionego obszaru (lista operacji już istnieje).

### Test pamięci: składanie w VRAM + blitter? (`tools/vrambench`)

Pomysł użytkownika: bitmapa w pamięci karty (`AllocBitMap` z bitmapą ekranu jako „friend”) i
przenoszenie jej blitterem Warpa. Warp, ekran 640×400×8, E-clock, najlepszy z 8, 256 000 B na klatkę:

| Operacja | Fast RAM | VRAM (off-screen) |
|---|---|---|
| czyszczenie klatki | 3,80 ms (67 MB/s) | 4,59 ms (55 MB/s) |
| zapis kawałkami po 32 B, bajtami (sprite'y) | 9,98 ms (20 MB/s) | **19,40 ms (10 MB/s)** |
| odczyt klatki (long) | 3,76 ms (67 MB/s) | 5,35 ms (47 MB/s) |
| kopia klatki Fast → Fast (`memcpy` libnix) | 7,07 ms (36 MB/s) | – |

| Na ekran | czas |
|---|---|
| Fast → ekran, lock + `memcpy` wierszami (port dziś) | 7,93 ms (32 MB/s) |
| Fast → ekran, lock + long moves | 8,06 ms |
| Fast → ekran, lock + **MOVE16** | **4,69 ms (54 MB/s)** |
| Fast → ekran, `WriteChunkyPixels` | 7,96 ms |
| VRAM → ekran, **`BltBitMap`** (blitter Warpa) | **0,44 ms (571 MB/s)** |
| VRAM → ekran, `BltBitMapRastPort` | 0,47 ms |

Wnioski: blitter jest bardzo szybki, ale składanie bezpośrednio w VRAM jest 2× wolniejsze przy zapisie
małymi kawałkami (tak gra rysuje sprite'y, napisy, kafle) – strata przy składaniu przewyższa zysk na
kopii. Obecny schemat (składanie w Fast RAM, do VRAM tylko prostokąt zmian, w grze < 1 ms na klatkę)
zostaje. MOVE16 skraca pełną kopię 7,9 → 4,7 ms (przewijanie, obrót globu, zmiana ekranu).
**`memcpy` z libnix kopiuje Fast → Fast tylko 36 MB/s** (zapis i odczyt osobno: 67 MB/s) – a gra kopiuje
dużo (kopia mapy z cache w bitwie ~18 ms na klatkę w 640×400): szybsza kopia dużych bloków to dobry
kandydat. Blitter mógłby się przydać przy przesuwaniu zawartości ekranu (przewijanie mapy) – większa
przebudowa.

### Test sposobów kopiowania (`tools/vrambench/copybench`)

Warp, E-clock, najlepszy z 8; MB/s. Blok 256 000 B (> L2) i 16 KB (w L2), Fast → Fast i Fast → VRAM.

| Sposób | Fast→Fast 256 KB | Fast→Fast 16 KB | Fast→VRAM 256 KB |
|---|---|---|---|
| kopia `memcpy` (libnix) | 42 | 63 | **55** |
| kopia `CopyMem` / `CopyMemQuick` | 41–42 | 63 | **55** |
| kopia pętla long / long ×4 | 34–35 | 47–49 | 30–31 |
| kopia `movem.l` 40 B | 35 | 50 | 32 |
| kopia MOVE16 | 41 | **69** | **55** |
| zapis `memset` / long ×4 (WriteL) | 70 / **72** | 131 / 123 | 55 / 54 |
| zapis `movem.l` | 67 | 123 | 56 |
| zapis MOVE16 (z jednej linii) | 47 | 61 | **66** |
| odczyt long ×4 / `movem.l` | 71 / 77 | 121 / 132 | 46 / 48 |

Wnioski: zapis do Fast RAM ~72 MB/s (zgodne z SysSpeed „WriteFastL” ~60), ale kopia Fast→Fast to
~42 MB/s niezależnie od instrukcji (ograniczenie DDR3). `memcpy`/`CopyMem` na dużym wyrównanym bloku są
tak szybkie jak MOVE16 (systemowa `CopyMem` jest najpewniej przyspieszona). Wolne jest **kopiowanie
wierszami** wieloma wywołaniami `memcpy` (wiersz 640 B: 32–36 MB/s), a tak kopiuje gra (na ekran, blity
sdlmini, kopia mapy z cache); ten sam obraz wierszami przez MOVE16: 54 MB/s (`vrambench`). Ręczne pętle
long/movem są wolniejsze od `memcpy`. Plan: funkcja kopiowania prostokąta wierszami – MOVE16 dla wierszy
wyrównanych do 16 B na 040/060 (sprawdzenie `AttnFlags` przy starcie), jeden `memcpy` dla wierszy
ciągłych, inaczej `memcpy` wierszami (AGA/020 bez zmian).

### Optymalizacja 4: kopiowanie prostokątów wierszami przez MOVE16 (`native/amiga_copy.c`)

`amiga_copy_rows()`: wiersze ciągłe → jeden `memcpy`; wiersze z tym samym przesunięciem względem 16 B
na 040/060 (`AttnFlags`) → środek wiersza MOVE16, brzegi `memcpy`; inaczej `memcpy` wierszami (020/030
bez zmian). MOVE16 omija cache danych, więc przed kopią `CacheClearE` źródła i celu (dla kopii na ekran:
źródło wypychane przed `LockBitMap`, bo pod blokadą nie ma wywołań systemu; VRAM nie jest cache'owany).
Użyte w: `rtg_blit_locked` (kopia na ekran), zwykłe blity sdlmini, kopia mapy z cache w bitwie.

Test na Warpie (`tools/vrambench/copytest`, ta sama `amiga_copy.c`): poprawność przy świeżo zapisanym
(brudnym w cache) źródle i celu, przesunięcia 0/3/5/15/16, szerokości 1–641: **0 błędnych bajtów**;
odczyt kontrolny z VRAM: 0 błędnych wierszy. Czas, 640×400 wierszami: Fast→VRAM 8,2 → **5,0 ms**
(wyrównane; nierówne brzegi 6,0 ms), Fast→Fast 7,7 → 7,1 ms (limit DDR3), sam `CacheClearE` 0,03 ms.

W grze (`openxcom-rtg-cp`, Warp, 640×400): prawdziwy Geoscape 16 → 17 kl./s, obrót 16 → 17; bitwa:
bezruch 49 → 50, kursor 26 → 29, przewijanie 27–29 → 27 – w granicach rozrzutu, bo kopiowanie nie jest
wąskim gardłem (kopia na ekran < 1 ms na klatkę). Zostaje: poprawne, pomaga przy pełnych zmianach ekranu.

### Tryb mieszany: Geoscape 320×200, bitwa natywne 640×400 (`amigaNativeScale`, patch 6amN)

Decyzja użytkownika: Geoscape w 640×400 nie pomaga w grze, a kosztuje najwięcej. Opcja
`amigaNativeScale: true` + `geoscapeScale: 0` + `battlescapeScale: 2`: `Screen::resetDisplay` ustawia
rozdzielczość ekranu równą bazowej bieżącej części gry i otwiera ekran od nowa przy zmianie rozmiaru
(bez skalowania, mysz 1:1). Na Warpie przełączanie działa w obie strony (start → menu 320 → bitwa 640 →
F9 → Geoscape 320).

| Prawdziwy Geoscape (zapis gracza, czas 5 s), Warp | 640×400 (wszystkie optymalizacje) | **tryb mieszany: 320×200** |
|---|---|---|
| czas płynie, bez ruchu | 17 kl./s | **49 kl./s** |
| obrót globu | 16–24 | **46** |
| zoom | 6–10 | **42** |

### Ciągłe przewijanie w bitwie (scena `scroll`: kursor przy krawędzi, tam i z powrotem)

Wcześniejsze „przewijanie” strzałkami nie było ciągłe (krok na naciśnięcie). Przewijanie przy krawędzi
startuje ≤ 5 px od brzegu ekranu (`Camera::SCROLL_BORDER`); kamera staje na brzegu mapy, więc scena
przewija na przemian w lewo i w prawo po 1,5 s – mapa przesuwa się w 99–100 klatkach na 100. Warp:

| | 320×200 | 640×400 |
|---|---|---|
| kl./s | 38 | **14** |
| klatka | 24,6 ms | 71 ms |
| dorysowanie pasa mapy (`terr`) | 6,7 ms | 14,3 ms |
| kopia obrazu mapy do bufora ekranu (`scrblit`) | 3,5 ms | 13,1 ms |
| `flip` (porównanie + kopia do bufora ekranu + kopia do VRAM 7,2 ms) | 7,7 ms | **31,8 ms** |

Krok 1 (`sdlmini_video.c`, diff-copy): sąsiednie zmienione komórki wiersza jedną kopią; po klatce z > 90%
zmienionych komórek 7 kolejnych kopiowanych w całości bez porównania. Warp, przewijanie 640×400:
14 → **15 kl./s** (flip 31,8 → 29,4 ms) – mniej, niż zakładałem: porównanie nie było głównym kosztem.

**Gdzie naprawdę idzie czas:** przy pomijaniu klatek całe składanie bufora ekranu dzieje się w `flip`.
Klatka przewijania w 640×400 to ok. **6 pełnych przejść przez pamięć** (po ~225–256 KB, każde ~5–6 ms
przy ~42 MB/s kopii Fast→Fast): przesunięcie obrazu w cache mapy, kopia cache → powierzchnia mapy,
czyszczenie bufora ekranu, powierzchnia mapy → bufor ekranu, bufor ekranu → bufor sdlmini, sdlmini → VRAM
(6,7 ms), plus dorysowanie pasa kafli (~13 ms). Możliwe skróty: pominąć czyszczenie bufora, gdy mapa
zakrywa ekran (~4 ms); przewijanie blitterem – przesunięcie w VRAM, kopia tylko nowego pasa (~10 ms);
powierzchnia mapy = obraz z cache (~5 ms, głębsza zmiana w kodzie bitwy).

### Odkrycie: 64-bitowe MULS.L/MULU.L, których 68060 nie ma w sprzęcie

`-mcpu=68020` sprawia, że gcc zamienia **każde dzielenie przez stałą** na mnożenie przez odwrotność
64-bitowym `MULS.L/MULU.L` (wynik Dh:Dl). 68060 tej instrukcji nie ma: każde wykonanie to pułapka
„unimplemented integer instruction” i emulacja w `68060.library`. Na 68020/030/040 to szybka instrukcja,
więc w WinUAE (i na 040) problem jest niewidoczny.

- `tools/find64.sh` (objdump z `-m m68k:68060`, inaczej te instrukcje wychodzą jako `.short`):
  **397 miejsc** w `openxcom-rtg` 0.9.14.
- Gorące ścieżki: `Globe::drawShadow` (`val / 3` na każdy piksel lądu – `mulsl #0x55555556`),
  `Map::drawTerrain` (9 miejsc, dzielenie przez 6 i przez 5/10), `Globe::draw`, `Globe::blit`,
  `Globe::mouseOver`, `TileEngine::hit`, `AIModule`, `BattlescapeGenerator` (11 w `init`).
- `-mtune=68060` (przy dalej `-mcpu=68020`, kod działa na 68020) usuwa je: gcc dzieli wtedy
  32-bitowym `DIVS.L/DIVU.L`, które 68060 wykonuje sprzętowo. `build.sh`: przełącznik `AMIGA_TUNE060=1`
  (binarki `*-t060`, osobny katalog obiektów).
- Na 68020/030 `DIVS.L` jest wolniejsze od `MULS.L`, więc dla AGA/020 to lekkie spowolnienie – stąd
  osobny wariant albo binarki RTG/FPU (które i tak celują w 040/060), do uzgodnienia z autorem.
- Zostają wywołania w gotowych bibliotekach (libnix/libgcc: `gmtime_r`, `strftime`, `__udivdi3` …),
  których nie przekompilujemy – rzadkie. Po `-mtune=68060`: 397 → 108 miejsc, żadne w pętlach rysowania.

**Koszt na Warpie** (`tools/mulbench`, 1 mln iteracji, E-clock, Forbid):

| | µs na operację |
|---|---|
| sama pętla | 0,051 |
| `x / 3` przez 64-bitowe `MULS.L` (pułapka + emulacja) | **1,267** (ok. 127 cykli) |
| `x / 3` przez 32-bitowe `DIVS.L` | 0,392 (38 cykli – potwierdza zegar ~100 MHz) |

**Wynik w grze** (`openxcom-rtg-t060` = optymalizacja 1 + `-mtune=68060`, wobec `openxcom-rtg-dc`):
Geoscape 640×400 bez zmian (21 / 13 / 6 kl./s – powtarzalny przebieg), bitwa w granicach rozrzutu
(losowa mapa: kursor 640×400 20 → 23 kl./s, 320×200 49 → 43 kl./s). Pułapka kosztuje ~0,9 µs więcej
niż `DIVS.L`; żeby zyskać 10 ms na klatkę, potrzeba ~11 000 takich dzieleń na klatkę – w mierzonych
scenach jest ich mniej. **Wniosek:** warto, ale to nie jest główne wąskie gardło; tańsza od obu wersji
jest tablica albo przesunięcie w gorącej pętli (np. `val / 3` dla `val` 0..31 w cieniu globu).
Do porównań bitwy potrzebny jest powtarzalny scenariusz (zapis gry zamiast losowej `amigaAutoBattle`).

### Kandydaci na optymalizacje (kolejność według zysku, do potwierdzenia pomiarem)

1. Nie czyścić i nie rysować od nowa ekranu, gdy nic się nie zmieniło; śledzić brudne prostokąty zamiast
   porównywać cały ekran w `flip` (zysk: ~18 ms + fill w każdej klatce, we wszystkich scenach).
2. Geoscape: koszt globu w 640×400 (dławik, cache, rozmiar tablic `earthfix.dat`).
3. Bitwa: nie kopiować mapy z cache do ekranu, gdy klatka się nie zmieniła.
4. Kopia na kartę: MOVE16 według „Warp RTG Developer notes” (mniejszy zysk, bo kopia i tak jest rzadka).

## Wniosek dla planu

Fazy 2–4 z `PLAN-HIRES.md` są w dużej części **już zrobione przez OpenXcom**. Praca przesuwa się na:

1. **wydajność** natywnego 640×400 na Warpie (glob, kopia na kartę, kafle bitwy),
2. **wybór backendu** RTG dla trybu, który gracz wybrał (nie tylko 320×200),
3. **prosty wybór trybu** dla gracza (np. w AmiXcomPrefs: „320×200” / „640×400”, ustawiający od razu
   rozdzielczość i obie skale), zamiast trzech niezależnych list w Options → Video,
4. decyzja z autorem, co z trybem 640×480 (skalowanym).
