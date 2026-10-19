# WarpBench – pomiary pamięci, kopiowania i blittera na karcie Warp (68060, RTG)

Wspólne dla wszystkich portów gier w `AmigaGAMES`. Trzy małe programy mierzą na prawdziwym Warpie,
gdzie i jak opłaca się rysować obraz gry: w Fast RAM czy w pamięci karty (VRAM), jaką instrukcją
kopiować i co naprawdę robi sprzętowy blitter. Uzupełniają `Warp RTG Developer notes.pdf`
(w katalogu nadrzędnym) o liczby potrzebne przy projektowaniu renderera.

Wyniki z **19.10.2026**, Warp1260X, 68060 rev 6 @ ~100 MHz (potwierdzone pomiarem `DIVS.L` = 38 cykli),
AmigaOS 3.2 (Kickstart 47.96), Picasso96 2.455, cybergraphics.library 42.7, monitor CSGFX.
Czasy: zegar E-clock, najlepszy z 5–8 powtórzeń.

## Programy

| Program | Co mierzy |
|---|---|
| `vrambench` | składanie klatki 640×400×8 w Fast RAM i w bitmapie w VRAM, przeniesienie na ekran (`memcpy`, long, MOVE16, `WriteChunkyPixels`, `BltBitMap`) |
| `copybench` | kopiowanie, sam zapis i sam odczyt różnymi instrukcjami (`memcpy`, `CopyMem`, `CopyMemQuick`, long, `movem.l`, MOVE16), Fast→Fast (256 KB i 16 KB) i Fast→VRAM |
| `blitbench` | `BltBitMap` dla bloków 8×8…640×400 (koszt jednego wywołania), przewijanie ekranu, sprite z maską (`BltMaskBitMapRastPort`), sprite rysowany przez CPU, koszt blokady bitmapy – na ekranie 8- i 16-bitowym |

Budowa (bebbo amiga-gcc, NDK 3.2, nagłówki CGX): `build.sh`; na tym PC z PowerShella:
`docker run --rm -v "C:/Workprojects/AmigaGAMES/WarpBench:/w" amixcom-build sh /w/build.sh`
(obraz `amixcom-build`: `ufo/docker/Dockerfile`). Uruchomienie przez AmiAgenta:
`amiga_remote.py put WarpBench/blitbench RAM:blitbench` i `amiga_remote.py run RAM:blitbench`.
Programy otwierają na kilka sekund własny ekran RTG 640×400 (8- i/lub 16-bit). Wyniki są wypisywane
dopiero po zwolnieniu blokad bitmap (pod blokadą P96 nie wolno wołać systemu).

## Wyniki

### 1. Pamięć: zapis, odczyt, kopia (`copybench`), MB/s

| Sposób | Fast→Fast 256 KB | Fast→Fast 16 KB (w L2) | Fast→VRAM 256 KB |
|---|---|---|---|
| kopia `memcpy` (libnix) | 42 | 63 | **55** |
| kopia `CopyMem` / `CopyMemQuick` | 41–42 | 63 | **55** |
| kopia pętla long / long ×4 | 34–35 | 47–49 | 30–31 |
| kopia `movem.l` 40 B | 35 | 50 | 32 |
| kopia MOVE16 | 41 | **69** | **55** |
| zapis `memset` / long ×4 („WriteL”) | 70 / **72** | 131 / 123 | 55 / 54 |
| zapis `movem.l` | 67 | 123 | 56 |
| zapis MOVE16 (z jednej linii) | 47 | 61 | **66** |
| odczyt long ×4 / `movem.l` | 71 / 77 | 121 / 132 | 46 / 48 |

- Zapis do Fast RAM ~72 MB/s (zgodne z SysSpeed „WriteFastL” ~60), ale **kopia Fast→Fast ~42 MB/s
  niezależnie od instrukcji** – ogranicza DDR3. Blok mieszczący się w L2 (96 KB) jest 1,5–2× szybszy.
- **Duży, wyrównany `memcpy`/`CopyMem` jest tak szybki jak MOVE16** (systemowa `CopyMem` jest już
  przyspieszona). Ręczne pętle long/`movem.l` są wolniejsze – nie warto ich pisać.
- **Wolne jest kopiowanie wierszami** wieloma małymi wywołaniami: obraz 640×400 jako 400 × `memcpy(640)`
  do VRAM to 32 MB/s (7,9 ms), ten sam obraz wierszami przez MOVE16 54 MB/s (4,7–5,0 ms).

### 2. Gdzie składać klatkę: Fast RAM czy VRAM (`vrambench`), 640×400×8 = 256 000 B

| Operacja | Fast RAM | VRAM (bitmapa na karcie) |
|---|---|---|
| czyszczenie klatki | 3,80 ms | 4,59 ms |
| zapis kawałkami po 32 B, bajtami (sprite'y, napisy, kafle) | 9,98 ms | **19,40 ms** |
| odczyt klatki | 3,76 ms | 5,35 ms |

| Przeniesienie klatki na ekran | czas |
|---|---|
| Fast → ekran, lock + `memcpy` wierszami | 7,93 ms |
| Fast → ekran, lock + MOVE16 wierszami | **4,69 ms** |
| Fast → ekran, `WriteChunkyPixels` | 7,96 ms |
| VRAM → ekran, `BltBitMap` (blitter) | **0,44 ms** |

**Składanie bezpośrednio w VRAM jest ~2× wolniejsze** przy zapisie małymi kawałkami, a tak gry rysują
sprite'y i kafle. Strata przy składaniu (+10 ms) przewyższa zysk na kopii (−7 ms).

### 3. Blitter, sprite'y, blokada (`blitbench`), ekran 640×400

| Operacja | 8-bit | 16-bit |
|---|---|---|
| `BltBitMap` VRAM→ekran 8×8 | 16 µs/wywołanie | 17 µs |
| 16×16 | 20 µs | 21 µs |
| 32×32 | 28 µs | 31 µs |
| 64×64 | 45 µs | 54 µs |
| 128×128 | 91 µs | 121 µs |
| 256×256 | 213 µs | 275 µs |
| cała klatka 640×400 | **0,45 ms** (566 MB/s) | **0,85 ms** (600 MB/s) |
| przewinięcie całego ekranu o 8 linii (`BltBitMap` na sobie) | **0,44 ms** | **0,83 ms** |
| sprite 32×32 z maską: `BltMaskBitMapRastPort` | **1 226 µs** | **2 428 µs** |
| sprite 32×32 z przezroczystością przez CPU → ekran (jedna blokada) | 94 µs | 95 µs |
| to samo, blokada na każdy sprite | 101 µs | 100 µs |
| to samo → klatka w Fast RAM | 66 µs | 76 µs |
| `LockBitMapTagList` + `UnLockBitMap` | 7,4 µs | 7,4 µs |

(Sprite CPU: prosta pętla C po pikselach z testem przezroczystości, `-O2`; zoptymalizowana pętla –
np. gotowe przebiegi nieprzezroczystych pikseli – będzie szybsza. Ważne są proporcje.)

## Zalecenia dla renderera na Warpie

1. **Obraz składaj w Fast RAM**, do VRAM wysyłaj tylko zmienione prostokąty, **tylko zapisem**
   (nigdy odczyt VRAM w pętli) – jak w `Warp RTG Developer notes`. W AmiXcom kopia na ekran kosztuje
   wtedy < 1 ms na klatkę; czas idzie na samo składanie.
2. **Kopiowanie prostokątów wierszami:** wiersze ciągłe → jeden `memcpy`; wiersze wyrównane do 16 B
   na 040/060 → MOVE16 (przed kopią `CacheClearE` źródła i celu – MOVE16 omija cache danych w trybie
   copyback; pod blokadą bitmapy wypchnij źródło wcześniej); inaczej `memcpy` wierszami.
   Gotowa, sprawdzona implementacja (test poprawności z „brudnym” cache: 0 błędów):
   `ufo/AmiXcom/native/amiga_copy.c`.
3. **Blitter (`BltBitMap`) – tylko duże, nieprzezroczyste bloki:** każde wywołanie kosztuje ~15 µs
   narzutu. Przewijanie całego ekranu (0,4–0,8 ms), odnawianie tła pod sprite'ami z czystej kopii w
   VRAM, bloki ≥ 64×64. Kafle 8×8 / 16×16 jedno wywołanie na kafel: za drogo (1000 kafli ≈ 20 ms).
4. **Nigdy `BltMaskBitMapRastPort` dla sprite'ów** na RTG: maska planarna, robi to CPU, 1,2–2,4 ms
   na jeden sprite 32×32.
5. **Sprite'y z przezroczystością rysuje CPU**, tylko zapisem (pomijając przezroczyste piksele), do
   bufora w Fast RAM albo pod jedną blokadą do ekranu. Blokada na każdy sprite to tylko +7 µs.
6. **Schemat dla gry 2D z przewijaniem w RTG:** tło (kafle) w bitmapie VRAM, przewijane i odnawiane
   blitterem; sprite'y rysowane przez CPU na ekran; w następnej klatce miejsca po sprite'ach
   przywraca blitter z czystego tła. Pełne przewinięcie ekranu kosztuje wtedy < 1 ms zamiast
   przerysowania całej klatki przez CPU.
7. **16-bit vs 8-bit:** blitter i zapis CPU skalują się z liczbą bajtów (16-bit = 2× danych), narzut
   wywołań taki sam.
8. **Duża plansza w VRAM, ekran = okno blitterem** (programowy odpowiednik sprzętowego przewijania):
   cała plansza (lub jej duży kawałek) jako bitmapa w pamięci karty, każda klatka to jeden `BltBitMap`
   okna 640×400 (0,4–0,8 ms) + postacie rysowane przez CPU; nic nie trzeba dorysowywać przy
   przewijaniu. Pamięć: plansza 4000×400 = 1,6 MB (8-bit) / 3,2 MB (16-bit). **Dobre dla:**
   platformówek, strzelanek z przewijaniem, widoku z góry – tło statyczne, postacie zawsze na wierzchu
   (warstwa przednia, jeśli jest, rysowana po postaciach). **Słabe dla:** widoku izometrycznego (obiekty
   z przodu zasłaniają postacie – gotowe tło tego nie odda), planszy często zmienianej (odkrywanie mgły,
   oświetlenie, zniszczenia – każda zmiana to rysowanie w VRAM, 2× wolniej niż w Fast RAM) i animowanych
   kafli (osobny obraz na każdą fazę). Przykład z AmiXcom: bitwa izometryczna 60×60 kafli to ~1920×1100 px
   na obraz, ×8 faz animacji ≈ 17 MB – zamiast tego port trzyma w Fast RAM obraz widocznego fragmentu
   (8 faz) i przy przewijaniu dorysowuje tylko odsłonięty pas.

## Powiązane

- `Warp RTG Developer notes.pdf` (katalog nadrzędny) – zasady: rysowanie w Fast RAM, zapis do VRAM,
  koszt przełączania odczyt/zapis VRAM.
- 68060 i gcc: każde dzielenie przez stałą przy `-mcpu=68020` to 64-bitowe `MULS.L`, którego 68060
  nie ma (pułapka, ~1,27 µs zamiast 0,39 µs `DIVS.L`); `-mtune=68060` to usuwa. Szczegóły i narzędzie
  `find64.sh` (w tym katalogu; `sh find64.sh <binarka> [funkcja]` w obrazie z amiga-gcc): `ufo/docs/hires-analysis.md`.
- Wyniki w kontekście konkretnej gry (AmiXcom, 640×400): `ufo/docs/hires-analysis.md`.
