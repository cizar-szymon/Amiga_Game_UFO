# Plan: natywna wyższa rozdzielczość w AmiXcom (RTG)

**Cel:** na ekranie RTG gra działa w prawdziwej rozdzielczości bazowej **640×400** (na początek), a nie w 320×200 powiększonym 2x. Battlescape pokazuje 4 razy większy obszar mapy, a Geoscape większy fragment globu. Menu i ekrany informacyjne zostają w oryginalnym rozmiarze, wyśrodkowane, tak jak w OpenXcom na PC.

**Platforma testowa:** Warp1260X (68060 @ 100 MHz, FPU), AmigaOS 3.2.2.1, karta RTG, binarka `openxcom-rtg-fpu`. Do szybkich iteracji służy WinUAE.

**Zasada nadrzędna:** AGA (320×200, c2p) i obecny tryb RTG muszą działać **bez zmian**. Nowa rozdzielczość jest **opcją tylko dla RTG**.

---

## Faza 0 – Rozpoznanie (bez zmian w kodzie)

Cel: wiedzieć, co blokuje wyższą rozdzielczość, zanim cokolwiek zmienimy.

Zadania:
1. W działającej grze sprawdzić **Options → Video**: czy widać opcje skali Geoscape i Battlescape, a jeśli tak, co się dzieje po ich zmianie.
2. Ręcznie ustawić w `user/options.cfg` wartości `geoscapeScale`, `battlescapeScale`, `baseXResolution` i `baseYResolution` i sprawdzić, czy port je respektuje, ignoruje, czy się wysypuje. Zebrać logi.
3. Przejrzeć kod portu i spisać każde miejsce, gdzie rozmiar ekranu jest stały:
   - otwieranie ekranu (AGA i RTG), wybór trybu i głębia,
   - rozmiar surface'u i bufora chunky,
   - c2p (tylko AGA) i ścieżka kopiowania na RTG,
   - mapowanie myszy (tryb 640×480 z pasami),
   - cache 8 klatek mapy, dirty rectangles, przewijanie,
   - blity sprite'ów w asemblerze (pitch, pitch zapisany na sztywno?),
   - Geoscape: tablice cieni i normalnych z `earthfix.dat` (czy zależą od rozmiaru ekranu lub promienia globu?), cień dnia i nocy w połowie rozdzielczości,
   - ekran ładowania (splash) i pasek tytułu.
   Szukać m.in.: `320`, `200`, `ORIGINAL_WIDTH`, `ORIGINAL_HEIGHT`, `baseXResolution`, `baseYResolution`, `baseXGeoscape`, `baseXBattlescape`, `updateScale`.
4. **Pomiary bazowe** w obecnym RTG 320×200 na prawdziwej 060 (i dla porównania w WinUAE). Ich wyniki wpisać do tabeli na końcu tego pliku.

Wynik fazy: plik `docs/hires-analysis.md` z listą miejsc do zmiany, oceną ryzyka każdego z nich i pomiarami bazowymi.

**Kryterium zakończenia:** wiemy, które zmiany są w warstwie Amigi, które w optymalizacjach portu, a które ewentualnie w samym OpenXcom.

## Faza 1 – Uzgodnienie z autorem

1. Założyć issue w `angree/AmiXcom` z opisem pomysłu, wynikami fazy 0 i proponowanym zakresem (tylko RTG, opcja w ustawieniach).
2. Zapytać, czy autor coś już planuje i czy przyjmie taki PR. Dowiedzieć się, jak woli nazwać opcję i gdzie ją umieścić: zakładka Amiga czy AmiXcomPrefs.
3. Uwzględnić odpowiedź w planie. Jeśli autor robi to sam, pomagamy testami na sprzęcie, a nie kodem.

## Faza 2 – Warstwa ekranu RTG w 640×400

Cel: gra uruchamia się na ekranie RTG 640×400 8-bit z obrazem 1:1, a menu działają.

Zadania:
1. Nowa opcja, na przykład `amigaRtgResolution: 320x200 | 640x400-scaled | 640x400`. Domyślnie zostaje obecne zachowanie.
2. Otwieranie ekranu RTG 640×400 w 8 bitach (wybór trybu przez API Picasso96/CyberGraphX z fallbackiem i czytelnym komunikatem w logu, jeśli trybu nie ma).
3. Rozmiar surface'u i bufora brany z opcji, a nie ze stałych.
4. Kopiowanie bufora na ekran 1:1, bez podwajania.
5. Mapowanie myszy 1:1 (bez pasów i bez skalowania).
6. Ustawienie `baseXResolution`/`baseYResolution` i skal tak, żeby OpenXcom sam rozłożył UI.

**Kryterium:** menu główne, opcje, nowa gra i Basescape działają. Kliknięcia trafiają w przyciski na całym ekranie, a AGA i stary tryb RTG działają bez zmian.

## Faza 3 – Battlescape w 640×400

Cel: grywalna bitwa z 4 razy większym widokiem.

Zadania:
1. Sprawdzić i dostosować optymalizacje portu do zmiennego rozmiaru:
   - cache klatek animacji (8 klatek po 640×400 to ok. 2 MB, więc pamięć nie jest problemem),
   - dirty rectangles i przesuwanie cache'u przy przewijaniu,
   - blity w asemblerze (pitch jako parametr, nie stała),
   - pasek ikon na dole (`Map::ICON_HEIGHT`) i wysokość widocznej mapy.
2. Test funkcjonalny: ruch, strzał, przewijanie, poziomy wysokości, tura obcych, zapis i odczyt.
3. Pomiary: fps w bezruchu, przy ruchu kursora, przy strzale i przy przewijaniu oraz czas tury obcych. Porównać z 320×200.
4. Profilowanie najwolniejszego elementu (przy 4 razy większej liczbie pikseli prawdopodobnie przewijanie i pełne odświeżenie mapy).

**Kryterium:** cała misja do końca na prawdziwej 060, bez błędów wizualnych, a wydajność oceniona przez użytkownika jako grywalna. Liczby trafiają do tabeli.

## Faza 4 – Geoscape w 640×400

Zadania:
1. Glob w większym oknie: promień, zoom, tablice z `earthfix.dat`. Jeśli zależą od rozmiaru, trzeba je przeliczyć przy buildzie albo w locie.
2. Cień dnia i nocy, okręgi radarów, linie krajów, markery i panel boczny.
3. Pomiary: rotacja i zoom globu oraz czas pierwszego zoomu.

**Kryterium:** Geoscape działa płynnie, obsługa globu myszą jest poprawna, a czasy są zmierzone.

## Faza 5 – Dopracowanie

Do rozważenia po ukończeniu faz 2–4:
- inne rozdzielczości: 480×300, 800×600, rozdzielczość Workbencha,
- osobne ustawienie dla Geoscape i Battlescape (jak w OpenXcom),
- ekran ładowania i pasek tytułu w nowej rozdzielczości,
- opcja w AmiXcomPrefs,
- dokumentacja w README.

## Faza 6 – Wydanie

1. Zbudować wszystkie warianty (AGA, AGA-FPU, RTG, RTG-FPU, ASK) i przeprowadzić test regresji AGA w WinUAE.
2. Wysłać PR do upstreamu z opisem przyczyn i zmian, pomiarami i instrukcją testu.
3. Uwzględnić uwagi autora.

---

## Ryzyka

| Ryzyko | Skutek | Co robimy |
|---|---|---|
| Optymalizacje portu zakładają 320×200 w wielu miejscach | dużo pracy w fazie 3 | faza 0 je wylicza, zanim zaczniemy |
| Wydajność przewijania w 640×400 | słaba płynność | pomiary w fazie 3; ewentualnie zostawić opcję 480×300 |
| Karta lub sterownik nie oferuje trybu 640×400 8-bit | ekran się nie otworzy | fallback do 320×200 i komunikat w logu |
| `earthfix.dat` zależy od rozmiaru globu | błędne cienie lub spowolnienie | przeliczyć przy buildzie lub w locie |
| Autor nie przyjmie zmiany | utrzymywanie własnego forka | faza 1 przed kodowaniem |
| WinUAE zachowuje się inaczej niż prawdziwa 060 | fałszywe wnioski | decyzje opieramy na pomiarach ze sprzętu |

## Pomiary

Wypełniać na bieżąco. Wpisać konfigurację, wersję i datę.

| Scena | RTG 320×200 (060) | RTG 640×400 (060) | Uwagi |
|---|---|---|---|
| Battlescape – bezruch (fps) | 50 (praca 14,4 ms/kl.) | 26 (37,1 ms/kl.) | 2026-10-19, build Docker 0.9.14 bez zmian; 640×400 = natywne (`SCALE_SCREEN`) |
| Battlescape – ruch kursora (fps) | 43–46 (19–20 ms/kl.) | 18 (53,3 ms/kl.) | |
| Battlescape – strzał (fps) | | | jeszcze nie mierzone |
| Battlescape – przewijanie (fps) | 45–46 (17 ms/kl.) | 16–17 (58–59 ms/kl.) | strzałki, autoinput |
| Tura obcych (s) | | | jeszcze nie mierzone |
| Geoscape – bezruch (fps) | 50 (13,4 ms/kl.) | 17 (57,4 ms/kl.) | |
| Geoscape – obrót globu (fps) | 42 (18,3 ms/kl.) | 11 (86,2 ms/kl.) | w 320×200 pojedyncze klatki ok. 300 ms (pełne przerysowanie globu, `slow frame: blit 300`): widoczne szarpnięcia |
| Geoscape – zoom (fps) | 46 (15,5 ms/kl.) | 4 (229 ms/kl.) | |
| Start gry (s) | 79 (1. start) / 47 | 47 | czas do `splash: 99%`, 1. start buduje cache `.ybc` |

**Po optymalizacjach 1 (diff-copy długimi słowami) i 2 (pomijanie niezmienionych klatek), Warp, 640×400:** bitwa bezruch 49 kl./s, kursor 26, przewijanie 27–29; Geoscape bezruch 36, obrót 20, zoom 10 (szczegóły: `docs/hires-analysis.md`). **Uwaga:** „Geoscape – bezruch” w tych tabelach to ekran wyboru bazy przy **zatrzymanym czasie**, a bitwa to początek misji. **Prawdziwy Geoscape przy czasie 5 s (zapis gracza): 14 kl./s bez zmian → 16–17 kl./s po optymalizacjach** – tyle widzi gracz.

**Faza 0 zrobiona: `docs/hires-analysis.md`.** Natywne 640×400 działa w porcie bez zmian w kodzie (OpenXcom: `displayWidth/Height` 640×400 + `geoscapeScale`/`battlescapeScale` = 5). Praca przesuwa się z „zrobić” na „przyspieszyć”: pełne czyszczenie i porównanie ekranu w każdej klatce, glob i kopia mapy w bitwie.

Konfiguracja: Warp1260X, 68060 rev 6 (zegar według użytkownika 100 MHz, nie sprawdzony programowo), AmigaOS 3.2 (Kickstart 47.96), monitor CSGFX, P96 2.455, CGX 42.7; dane UFO (DOS), muzyka wyłączona, `amigaPerfLog: 1`, binarka `openxcom-rtg`. Narzędzie: `winuae/perf-real.ps1` (AmiAgent + autoinput), logi w `winuae/perf-real/`. Gra ma limit 50 kl./s.

**Obecny tryb 640×480 (skalowanie 2× z pasami) na tym samym Warpie: 5–6 kl./s** w bitwie i na Geoscape. Sam `flip` trwa ok. 146 ms na klatkę: skaler z dzieleniem na każdy piksel plus pełna kopia 640×480 na kartę (10–12 ms). Szacunek ok. 100 ms z analizy kodu potwierdził się z nawiązką. **Decyzja użytkownika (2026-10-19): bez skalowania, tylko natywne 320×200 i 640×400.**

### Pomiary porównawcze w WinUAE (2026-10-19)

**Tylko do porównań między wariantami, nie mówią nic o Warpie.** WinUAE: 68060 + FPU, `cpu_speed=max`, bez JIT, uaegfx, OS 3.2 (`winuae/amixcom-060-rtg.uae`). Binarka: `openxcom-rtg` z buildu Docker (0.9.14, kod upstreamu bez zmian). Dane: UFO (wersja DOS ze Steama). Narzędzia: `winuae/perf-run.ps1` (autoinput + `amigaPerfLog: 1`) i `tools/perf_summary.py`. Gra ma limit 50 kl./s, więc porównujemy **pracę na klatkę** (think + blit + flip), a nie fps. Bitwa jest losowa przy każdym starcie, więc rozrzut między przebiegami bywa duży.

| Scena | 320×200: praca ms/kl. (fps) | 640×480: praca ms/kl. (fps) | z tego `flip` 320 / 640 |
|---|---|---|---|
| Bitwa – bezruch | 10,0 (50) | 29,5 (33) | 2,6 / 27,3 |
| Bitwa – ruch kursora | 30,4 (50) | 35,3 (28) | 6,7 / 27,2 |
| Bitwa – przewijanie | 28,9 (50) | 31,2 (31) | 6,7 / 27,0 |
| Geoscape – bezruch | 11,7 (50) | 37,5 (26) | 0,0 / 31,4 |
| Geoscape – obrót globu | 14,5 (45) | 43,1 (23) | 0,1 / 32,0 |
| Geoscape – zoom | 13,4 (46) | 43,9 (22) | 0,0 / 31,6 |

**Wniosek:** w trybie 640×480 ok. 27–32 ms każdej klatki to `Screen::flip()`, niezależnie od sceny. Sama kopia na kartę RTG (`perf: c2p`) zajmuje 1,6–1,9 ms. Resztę zjada `Zoom::flipWithZoom` z `native/oxc-replace/Zoom.cpp`: w każdej klatce `SDL_FillRect` całego ekranu (cały ekran staje się „brudny”) i skalowanie najbliższym sąsiadem z dzieleniem na każdy piksel (`x * src->w / dst->w`, 256 000 dzieleń na klatkę). Na 68060 to szacunkowo ok. 38 cykli na `DIVS.L`, czyli ok. 100 ms na klatkę przy 100 MHz. Do sprawdzenia na Warpie.

**Kandydat na pierwszy PR (osobny od wyższej rozdzielczości):** szybka ścieżka 2× w `Zoom.cpp`: podwojenie bajtów przez tablicę lub słowa, duplikacja wiersza przez `memcpy`, pasy czyszczone raz, a nie w każdej klatce. Najlepiej tylko dla zmienionych wierszy. AGA i 320×200 bez zmian.
