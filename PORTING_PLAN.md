# Plan portowania gry na Amigę (szablon dla nowych projektów)

Ogólny plan, zebrany z dwóch ukończonych portów:

- **Wizard of Wor** (C64, 6502): gra **przepisana na C**, procedura po procedurze z disassembly,
  `Wizard of Wor\wizardofwor-master\docs\PORTING.md`;
- **Vastar** (automat Orca, 2× Z80 + AY-3-8910): **emulator sprzętu** wykonujący oryginalne ROM-y,
  rdzeń Z80 w asemblerze 68k, `Wastar\docs\PORTING.md`.

Kolejność jest zawsze ta sama: **najpierw działający i sprawdzony port na PC, potem Amiga, potem
optymalizacja aż do pełnego tempa**. Szczegóły zależą od platformy źródłowej – w każdym projekcie
trzeba przejść fazę 0 i zapisać decyzje w `docs/PORTING.md` projektu.

W `CLAUDE.md` nowego projektu:

```
@C:/Workprojects/AmigaGAMES/PORTING_PLAN.md
@C:/Workprojects/AmigaGAMES/AmiAgent/AMIAGENT.md
```

---

## Cel i sprzęt docelowy (domyślnie, chyba że użytkownik powie inaczej)

| | Domyślnie |
|---|---|
| Maszyna | Amiga 1200, **68060 @ 50 MHz** (karta Warp 1260), Fast RAM, AmigaOS 3.2 |
| Procesor | **gry mają działać przede wszystkim na MC68060 i MC68040** (karty Warp 1260 / 4070, 040/060 z FPU i bez); 68020/030 tylko wtedy, gdy nie szkodzi to 040/060 |
| Obraz | Picasso96 / RTG (Workbench 16-bit), okno na Workbenchu; opcjonalnie pełny ekran (RTG lub PAL bez RTG) |
| Dźwięk | Paula przez `audio.device` (najtańsza dla CPU) albo AHI |
| Sterowanie | joystick / pad CD32 (`lowlevel.library`) + klawiatura według **standardu klawiszy** (niżej) |
| Testy | WinUAE (szybki cykl) **i prawdziwa A1200 przez AmiAgent** (pomiary wydajności – tylko tu) |
| System TV | **domyślnie PAL (50 Hz)** – gra zachowuje się jak wersja europejska; NTSC jako opcja (`SYSTEM=NTSC`, PC `--ntsc`) |
| Cel wydajności | **pełne tempo gry oryginału (50/60 kl./s logiki) i jak najwięcej obrazów/s**, docelowo każda klatka |

Gra działa zgodnie z systemem (bez przejmowania sprzętu), więc chodzi z Workbencha i z ikony.
Przejęcie sprzętu (copper, sprite'y, BOB-y) rozważać tylko dla celu OCS/AGA bez RTG.

**Standard klawiszy (od 12.10.2026, wszystkie porty, Amiga i PC):**

| Klawisz | Działanie |
|---|---|
| strzałki | ruch |
| **Z** | ogień 1 (strzał) |
| **X** | ogień 2 (np. skok, dodatkowa broń) |
| **P** | pauza (drugie naciśnięcie: dalej) |
| Esc | koniec gry (Amiga też zamknięcie okna i Ctrl-C) |

Inne klawisze tylko tam, gdzie gra ich potrzebuje (np. automaty: 1 / 2 start, 5 / 6 moneta, F3 reset;
F10 zapis stanu do zgłoszeń błędów) – bez dodatkowych zamienników dla ognia (Ctrl, Alt, Spacja).
Wyjątek: przy joysticku z jednym przyciskiem drugi przycisk joysticka może mieć osobny klawisz
(Xain'd Sleena: `CONTROL=JOY1`, Spacja). Opis klawiszy w opisach dla graczy każdej platformy.
**Klawiatura działa jak joystick:** jeśli joystick z jednym przyciskiem robi coś kierunkiem (np. góra
= skok), strzałka robi to samo; gdy ta sama akcja ma w innej części gry inną funkcję, dostaje
własny klawisz (Xain'd Sleena: strzałka w górę na piechotę = skok, w lotach tylko ruch, a druga
broń w lotach pod **Y**; X zostaje ogniem 2).

---

## Faza 0. Rozpoznanie i decyzja o architekturze

1. **Zbierz materiały:** ROM-y / obraz dysku, disassembly (jeśli istnieje), sterownik MAME,
   dokumentację sprzętu, rdzeń FPGA (MiSTer) – wszystko to jest specyfikacją. Zapisz, skąd co jest.
2. **Opisz platformę źródłową:** procesor(y) i zegar, mapa pamięci, układ obrazu (kafelki, bitmapa,
   sprite'y, warstwy, obrót ekranu), układ dźwięku, przerwania, wejścia, częstotliwość odświeżania.
3. **Wybierz sposób portowania** (najważniejsza decyzja – zapisz uzasadnienie):

| Sposób | Kiedy | Plusy / minusy |
|---|---|---|
| **A. Przepisanie na C** (jak Wizard of Wor) | jest dobre disassembly z symbolami, kod gry mały (do ~10–20 tys. instrukcji), procesor wolny (6502, 6809, 1–2 MHz) | najszybsze na Amidze, łatwe zmiany zasad gry; dużo pracy, trzeba testów różnicowych procedura po procedurze |
| **B. Emulacja sprzętu** (jak Vastar) | automat / konsola, brak disassembly, kilka procesorów, gra zależy od dokładnych czasów | wierne, ROM-y bez zmian; procesor musi być emulowany szybko (rdzeń w asm 68k), więcej pracy przy wydajności |
| **C. Kod natywny 68k** | źródło jest na 68000 (Atari ST, Mega Drive, Neo Geo, X68000) | kod gry może chodzić wprost na 68060; zostaje podmiana sprzętu (obraz, dźwięk, wejścia) – emulowany przez pułapki dostępu lub przepisany |
| **D. Port ze źródeł** | jest kod źródłowy (C, Pascal, DOS) | przenieść na C99 bez FPU, warstwa HAL; pilnować endianness (68k = big endian) i rozmiarów typów |

   Typowe przypadki: C64 / Atari 8-bit / NES → A albo B; automaty Z80 / Spectrum / MSX / CPC → B
   (rdzeń Z80 już jest w Vastar: `tools/gen_z80_68k.py`, `port/src/z80.c`); 68000 → C; DOS → D.
   **Gotowe rdzenie procesorów: `C:\Workprojects\AmigaGAMES\CPU\`** – `M6809` (z Xain'd Sleena: rdzeń
   C + asm 68k z generatorem, testy, instrukcja `README.md`). Przed pisaniem rdzenia sprawdź ten
   katalog; nowy rdzeń po skończonym porcie też tam wydziel (kod, generator, testy, README).
4. **Ustal docelowe tempo** oryginału (np. 60,606 Hz automatu, 59,826 Hz C64 NTSC, 50 Hz PAL)
   i zapisz, co zrobić z różnicą 60↔50 Hz (logika w tempie oryginału z `timer.device`, obraz
   pomijany w razie potrzeby).
   **Gry tworzymy domyślnie dla systemu PAL.** Gdy platforma źródłowa ma wersję PAL (C64, Atari
   8-bit, Spectrum, NES, Amiga…), port domyślnie działa jak ta maszyna PAL: jej liczba klatek
   (C64: 50,125 Hz), zegar procesora i układu dźwięku (wysokość dźwięków), liczba linii i cykli
   na klatkę. Jeśli gra nie czeka na ramkę, budżet klatki PAL trzeba **zmierzyć** w emulatorze
   wzorcowym w trybie PAL (jak WoW: `measure_loop_timing.py --pal`, budżet 2840 → 3330), a nie
   przeliczać. NTSC zostaje jako opcja (`SYSTEM=NTSC`, PC `--ntsc`); testy różnicowe mogą zostać
   na systemie, z którego są migawki. Automaty (bez wersji PAL) chodzą w swoim tempie, ale
   obraz i dźwięk projektujemy pod Amigę PAL. Dźwięk liczyć z dokładną częstotliwością Pauli
   (okres zaokrąglony w górę, PAL 161 = 22 030 Hz), a dla AHI pierścień kilku buforów
   wypełnianych z wyprzedzeniem – inaczej przy 50 Hz pojawiają się przerwy (WoW, `PORTING.md`).
5. Zapisz w `docs/PORTING.md`: tabela stanu prac (kroki z ✅/⏳), architektura (diagram), decyzje.

**Wyjście z fazy 0:** decyzja A/B/C/D zaakceptowana przez użytkownika, lista kroków w `PORTING.md`.

---

## Faza 1. Narzędzia i wzorzec (referencja)

Bez wzorca nie da się sprawdzić portu. Przygotuj (zwykle w Pythonie, `tools/`):

- **Budowa oryginału** z disassembly (np. 64tass, AS, sjasmplus) i potwierdzenie, że daje identyczny ROM.
- **Emulator referencyjny ze zdalnym sterowaniem** (VICE monitor `tools/vice_monitor.py`, MAME
  z Lua/debuggerem, Mesen, Fuse…): zrzuty pamięci, rejestrów, ekranu, nagranie dźwięku.
- **Eksport danych** (grafika, teksty, tablice) do C – sprawdzony bajt w bajt z RAM emulatora
  (`export_data.py`, `export_gfx.py`, `verify_gfx_vice.py`).
- **Migawki procedur** (sposób A): pełna pamięć + rejestry przy wejściu i wyjściu procedury
  (`capture_snapshots.py`, tryb `--event` dla rzadkich zdarzeń).
- **Testy procesora** (sposób B): zexdoc/zexall dla Z80, testy Klausa Dormanna dla 6502 itd.

---

## Faza 2. Port na PC (C99 + SDL2)

Budowa w MSYS2 UCRT64 (`bash -lc`), `make` w `port/` → `build/port/<gra>.exe`.

**Zasady kodu wspólnego (żeby od razu dało się go skompilować vbcc na Amigę):**
- C99, **bez FPU, bez arytmetyki 64-bit**, bez `long long`, bez VLA; liczby stałoprzecinkowe;
- typy o jawnych rozmiarach (`uint8_t`, `uint16_t`, `uint32_t`); dane wielobajtowe składane bajtami
  (port musi działać na big endian);
- cała zależność od platformy za **HAL** (`hal.h`): czekanie na klatkę, wejścia, dźwięk (bufor
  próbek), obraz (bufor indeksów palety + lista zmienionych bloków);
- logika gry / emulator nie wie nic o SDL ani o Amidze.

**Kolejność:**
1. Rdzeń: przepisana logika (A), emulowany procesor + mapa pamięci (B), loader kodu 68k (C).
2. Obraz: najpierw **renderer wzorcowy** – prosty, cała klatka, łatwy do sprawdzenia; porównanie
   z emulatorem referencyjnym piksel w piksel (`compare_render.py`).
3. Tempo: synchronizacja z ramką; jeśli oryginał nie czeka na ramkę (pętla zależna od szybkości CPU),
   budżet cykli/dostępów na klatkę skalibrowany pomiarem w emulatorze (`measure_loop_timing.py`).
4. Dźwięk: model układu (SID, AY, POKEY, YM2151…), liczby całkowite; porównanie nagrań WAV
   (wysokości, rytm, głośność).
5. Wejścia: klawiatura + pad, monety/start dla automatów.

**Opcje testowe wersji PC (od początku – oszczędzają dziesiątki godzin):**
`-headless -frames N -shots KATALOG 100,200 -wav PLIK -script "1200:+coin,1206:-coin,..." -stats`,
`--demo`, `--fast`, `--check-render` (porównanie renderera szybkiego z wzorcowym w każdej klatce).

**Testy:**
- (A) testy różnicowe procedur ze stanu wejściowego do wyjściowego (`make test`) – cel: 0 błędów;
- (B) testy procesora 100% + przebieg skryptowy (demo, moneta, start, rozgrywka) ze zrzutami;
- odsłuch i ocena przez użytkownika (zgodność kolorów, dźwięku, sterowania).

**Wyjście z fazy 2:** gra grywalna na PC od tytułu do game over, wyniki zgodne z wzorcem,
opisane znane różnice (`PORTING.md`, sekcja „Znane różnice”).

---

## Faza 3. Pierwsza wersja na Amigę

Narzędzia w `C:\amiga-dev\`: vbcc (`+aos68k_win`, konfiguracja **bez `-Rshort`** – inaczej „bad load
hunk” na prawdziwej Amidze), NDK 3.2, AHI dev, P96 (`p96inc`), netinclude Roadshow.
`make -f Makefile.amiga` w `port/` → `build/amiga/<Gra>`. Wzorzec: `main_amiga.c` z Vastar lub WoW.

| Element | Rozwiązanie |
|---|---|
| Okno | Intuition, `OpenWindowTags` na Workbenchu, `SCALE=1\|2\|3`; opcja `SCREEN=FULL` |
| Obraz RTG | bitmapa na karcie (`AllocBitMap` z „friend”), zapis zmienionych bloków + jeden blit (`BltBitMapRastPort`); bez RTG `WriteChunkyPixels` lub planarnie |
| Tempo | `timer.device` (`ReadEClock`) w tempie oryginału |
| Dźwięk | Paula (`audio.device`, bufory w Chip RAM) i AHI (`AHI_AllocAudio`, 2 bufory + `AHIA_SoundFunc`); wybór `SOUND=PAULA\|AHI`, `NOSOUND` |
| Wejścia | `lowlevel.library ReadJoyPort` **raz na klatkę**, IDCMP RAWKEY |
| Opcje | argumenty Shella i te same jako **Tool Types ikony** (`parse_option()` dla obu) |
| Wyjście | Ctrl-C w pętli głównej (potrzebne dla `stop`/`deploy` AmiAgenta), Esc |
| Diagnostyka | `STATS` (fps, CPU, dźwięk w tytule okna), `STATS=FULL` (podział czasu), `VERIFY` (odczyt z karty i porównanie bloków), strażnik zawieszeń zapisujący stan do `RAM:<Gra>-hang.txt` |

**Testy:** najpierw WinUAE (`*_net.uae`, `build/amiga` podmontowany jako wolumin), potem prawdziwa
A1200 przez AmiAgent (`deploy` → `key` → `stats` → `shot` → `stop`, patrz `AMIAGENT.md`).
Sprawdzić start z ikony (nie tylko z Shella) – inne środowisko, inny stos, inne katalogi.
Przy starcie z ikony (libnix: `_WBenchMsg != NULL`) nic nie pisać na stdout/stderr – libnix otwiera
wtedy okno konsoli na Workbenchu; log tylko do pliku. Nie przez `__stdiowin = "NIL:"`: to blokuje
start z ikony w ogóle (AmiXcom, 24.10.2026).

**Wyjście z fazy 3:** gra działa poprawnie na prawdziwej A1200 (obraz, dźwięk, joystick), jest
pomiar `STATS` jako punkt wyjścia optymalizacji.

---

## Faza 4. Optymalizacja (cel: pełne tempo, maksimum obrazów/s)

**Metoda – zawsze ta sama pętla:**
1. Zmierz na **prawdziwej A1200** (`STATS=FULL`, ta sama scena, np. demo w 32.–44. s albo opcja
   wymuszająca najcięższą scenę, jak `WORLUK=N`). WinUAE tylko do szybkich prób – zaniża koszty
   CIA, Chip RAM i karty graficznej.
2. Znajdź największą pozycję w podziale czasu (logika/emulacja, składanie obrazu, wysyłanie do
   karty, dźwięk, wejścia).
3. Zmień jedną rzecz, sprawdź zgodność (`--check-render`, `Lockstep`, `VERIFY`, porównanie WAV),
   zmierz ponownie, zapisz wiersz w tabeli wydajności w `PORTING.md` (także nieudane próby).

**Co daje największe efekty (pomiary z WoW, Vastar i AmiXcom na Warp 060):**

1. **Praca na pamięci graficznej (VRAM karty) – największy zysk i najbardziej wydajna droga.**
   Obraz, który już jest na karcie, zostaje na karcie: przewijanie robi blitter karty
   (kopia VRAM→VRAM w pamięci karty), a CPU dosyła tylko nowe paski i zmienione bloki.
   Klatka bez zmian w ogóle nie idzie na kartę. AmiXcom, bitwa 640×400: przewijanie
   48,9 → 23,5 ms na klatkę (20 → 37 kl./s), postój 26 → 50 kl./s, marsz ~15 → ~28 kl./s;
   wysłanie klatki przy marszu 19,5 → 4 ms. Zasady dalej obowiązują: CPU nigdy nie czyta VRAM,
   składa w Fast RAM i zapisuje do VRAM tylko zmiany, długimi słowami.
2. **Kompilacja pod 68040/68060** (`-mtune=68060`, sprawdzenie `find64.sh`): bez 64-bitowych
   MULS.L/DIVS.L, które 060 emuluje w pułapkach. AmiXcom: pole widzenia 60–140 → 0–20 ms.
3. **Nic kosztownego w gorących pętlach:** wywołania systemu (DateStamp 47 µs, `SDL_GetTicks`
   57 µs), sondy pomiarowe, pętle aktywnego czekania (`SDL_Delay(1)` bez snu). AmiXcom 1 DAY:
   2,6 → 0,3 s na dzień gry po usunięciu odczytu zegara z każdego 5-sekundowego kroku.
4. **Własny przydział pamięci** (klasy rozmiarów zamiast puli libnix z first-fit przy setkach
   tysięcy bloków): zapis gry 8,3 → 0,14 s, wczytanie 4,9 → 0,6 s.
5. **FPU 040/060 dla trygonometrii** (osobny wariant programu z FPU): ruch po kuli ziemskiej
   2,97 → 0,85 s na dzień gry. Gdzie liczby zmiennoprzecinkowe nie dominują – bez zysku.
6. **Liczyć tylko to, co się zmieniło:** pola zmienione w kroku, jednostki, które widzą dane
   miejsce, wynik zależny tylko od niezmiennych danych – raz zamiast w każdej klatce.

Każdą z tych zmian mierzyć osobno na prawdziwym 040/060 i sprawdzać trybem porównania
(0 różnic), zanim pójdzie dalej.

**Sprawdzone techniki (w kolejności typowego zysku):**

*Obraz*
- **Renderer przyrostowy w blokach 8×8:** pamiętać stan (kafelki, kolory, przewinięcie, sprite'y),
  przerysowywać tylko bloki ze zmianą lub ze sprite'em (był/jest), wysyłać tylko zmienione bloki.
  WoW: z 13,6 do 59,7 kl./s. Zawsze z trybem porównania z rendererem wzorcowym (0 różnic).
- Zmiana samych kolorów / palety → przemalowanie tylko bloków z pikselami tej klasy (`recolor()`),
  nie całego ekranu.
- Składanie całymi wierszami bloków, gdy zmian w wierszu jest dużo (przewijanie).
- Grafika wstępnie przeliczona (obrócona, spakowana 2 bpp, wersje odbite), mały zestaw roboczy
  mieszczący się w L2 68060.
- **Według „Warp RTG Developer notes”** (`Wastar\docs\Warp RTG Developer notes.pdf`): piksele
  w formacie ekranu powstają w małym buforze w Fast RAM (jeden wiersz bloków), do VRAM **tylko
  zapis** długimi słowami, nigdy odczyt; jeden blit prostokąta zmian (sprzęt Warpa na 16 bitach);
  na 32 bitach kopię VRAM→VRAM robi CPU, więc tam `WPA` zamiast `BLIT`. `MOVE16` tylko po
  `CacheClearE` (copyback).
- **Zmierzone liczby dla Warpa** (zapis/odczyt/kopia różnymi instrukcjami, składanie w Fast RAM vs
  VRAM, koszt `BltBitMap` na wywołanie, sprite'y z maską i przez CPU, 8/16-bit) i wynikające z nich
  zalecenia: **`WarpBench/README.md`** (programy testowe w tym samym katalogu). Najważniejsze:
  składać w Fast RAM; `BltBitMap` tylko dla dużych bloków (~15 µs narzutu na wywołanie, cały ekran
  0,4–0,8 ms); nigdy `BltMaskBitMapRastPort` (1–2,4 ms na sprite 32×32); kopie wierszami przez MOVE16.
- Dwie bitmapy z kopią zawartości (`shadow`) dla migających elementów – blit zamiast zapisu.
- **Pomijanie klatek obrazu** (`FRAMESKIP=AUTO`): logika i dźwięk zawsze w pełnym tempie, obraz
  pomijany przy spóźnieniu > 1/3 klatki (maks. 3 pod rząd).
- Pełny ekran PAL bez RTG: planarnie, tablice rozwijania pikseli na słowa bitplanów.

*Procesor / logika*
- (B) **Rdzeń CPU w asemblerze 68k generowany skryptem** (`gen_z80_68k.py`): rejestry gościa
  w rejestrach 68k, flagi z CCR + tablice, skok przez tablicę na końcu każdej instrukcji, przerwania
  tylko na granicach przebiegu. Weryfikacja **lockstep** z rdzeniem C (instrukcja po instrukcji).
- Pomijanie pętli oczekiwania gościa (wykryć pętlę czekającą na przerwanie i przeskoczyć czas).
- (A) zmienne w rejestrach, stałe koszty zamiast licznika dostępów, wstawki asm w gorących pętlach.
- Pętle obrazu i konwersji formatów w asm (`video_68k.s`, `convert16`).
- vbcc `-O3 -speed`, `-cpu=68060`; brak dzieleń w pętlach (przesunięcia, tablice).
- gcc z `-mcpu=68020`: każde dzielenie przez stałą to 64-bitowe `MULS.L`, którego 68060 nie ma
  (pułapka do `68060.library`, ~1,27 µs zamiast 0,39 µs); `-mtune=68060` to usuwa, kod dalej działa
  na 020. Sprawdzenie binarki: `WarpBench/find64.sh` (objdump z `-m m68k:68060`).

*Dźwięk*
- Paula zamiast AHI (AHI miksuje programowo – ok. 5% CPU 060/50).
- Model układu: w ciszy zera i hurtowe przesuwanie faz; obwiednie partiami; generator po generatorze
  przez cały bufor zamiast pętli po próbkach; faza 24.8 (jedno dodawanie na próbkę); bez dzieleń.
- Alternatywa: Paula gra przebiegi sprzętowo (okres/głośność z modelu co klatkę) zamiast syntezy próbek.

*Wejścia i system*
- `ReadJoyPort()` jest na prawdziwym sprzęcie wolne (CIA) – czytać raz na klatkę (WoW: logika
  z 50% do 5% klatki).
- Nic nie odpytywać w pętli, co dotyka Chip RAM / CIA / karty.

**Wyjście z fazy 4:** na A1200 060/50 pełne tempo gry w najcięższej scenie, tabela wydajności
w `PORTING.md`, `VERIFY` i porównania bez różnic. Dalsze możliwości zapisane jako lista.

---

## Faza 5. Wydanie

- Ikona z kadru gry (`tools/make_icon.py`), Tool Types z wyłączonymi `(OPCJA)` jako podpowiedzi
  (`tools/icon_tooltypes.py`).
- Dodatki poza oryginałem jako opcje (np. `LIVES=`, `FRIENDLYFIRE=OFF`, Free Play, start joystickiem)
  – domyślnie zachowanie oryginału, chyba że użytkownik zdecyduje inaczej.
- Opisy opcji: dla graczy i dla programistów, po polsku i angielsku (`<Gra>.txt`, `<Gra>_en.txt`,
  `<Gra>_dev.txt`, `<Gra>_dev_en.txt`), katalog `releases/amiga/`, README portu. **Osobne opisy
  dla każdej platformy** (źródła np. `docs/release/amiga` i `docs/release/pc`): w paczce Amigi tylko
  ustawienia Amigi (Tool Types), w paczce PC tylko opcje PC.
- **Numer wydania (od 12.10.2026 w każdym projekcie):** jedna stała w kodzie (np. `XAIN_VERSION`
  w `xain.h`), widoczna w grze (np. napis `RELEASE x.y.z` na ekranie startu, Xain'd Sleena), w `$VER`
  programu Amigi (`Version Gra` w Shellu) i w tytule okna PC – żeby przy zgłoszeniu błędu wiedzieć,
  w której rewizji wystąpił i czy nie jest już poprawiony w nowszej. Każda nowa kompilacja dla
  użytkownika podnosi numer. **Schemat (od 24.10.2026): `0.xx` – beta, `1.xx` – wydania oficjalne**
  (AmiXcom: `0.<liczba commitów>`, np. 0.86).
- **Plik wydań `<Gra>_release.txt` / `_release_en.txt` (od 12.10.2026):** w paczce razem z grą; dla
  każdego wydania numer, data, dodatki i poprawki (opisane dla gracza), na początku jak zgłaszać
  błędy (numer wydania + pliki stanu). Uzupełniany przy każdym wydaniu.
- **Zapis stanu do zgłoszeń błędów (F10, od 12.10.2026):** klawisz zapisuje pełny, przenośny stan
  gry / maszyny (big endian) do `RAM:<Gra>-state.bin` + krótki opis `.txt`; wersja PC wczytuje go
  (`-loadstate PLIK`) i odtwarza dalszy przebieg (także z szybkimi ścieżkami i bez). Użytkownik
  może wysłać pliki, a Claude pobrać je przez AmiAgenta (`get RAM:<Gra>-state.bin`). W Xain'd
  Sleena pozwoliło to znaleźć rzadkie zawieszenie (wyścig dwóch CPU) po jednym zapisie.
- ROM-ów nie dołączać – opisać, skąd je wziąć i gdzie położyć (`ROMPATH=`, `PROGDIR:rom`).
- Commity i publikacja tylko na prośbę użytkownika.

---

## Zasady pracy (dla Claude)

- Każdą fazę kończyć **sprawdzeniem** (test, porównanie, pomiar) i aktualizacją `docs/PORTING.md`
  (stan, decyzje, liczby). Podawać wyniki uczciwie, także gdy coś nie wyszło.
- Nie optymalizować przed portem zgodnym ze wzorcem; nie zgadywać wydajności – mierzyć na A1200.
- Każda szybka ścieżka ma tryb porównania z wolną, wzorcową (renderer, rdzeń CPU, dźwięk).
- Błędy oryginału zachowywać (chyba że użytkownik chce poprawki jako opcję).
- Skrypty pomocnicze zapisywać do plików (heredoki Pythona w Bash psują cudzysłowy).
- Wspólne pliki: `amiga_remote.py`, `gen_z80_68k.py`, `make_icon.py`, `icon_tooltypes.py`,
  `main_amiga.c` – kopiować z Vastar / Wizard of Wor i dopasować, nie pisać od zera.
