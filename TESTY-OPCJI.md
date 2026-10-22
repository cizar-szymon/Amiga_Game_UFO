# Testy opcji gry na prawdziwych Amigach (stan na 22.10.2026)

Lista 16 opcji, które mogły kolidować z dzisiejszymi zmianami portu RTG, czyli z pamięcią mapy w
bitwie, warstwą VRAM i rozdzielczością 640×400. Opcje sprawdzamy pojedynczo na prawdziwym sprzęcie
z włączonym `amigaMapVerify: 1`. W tym trybie gra porównuje każdą klatkę z pamięci mapy z pełnym
przerysowaniem i zapisuje w `oxc.log` każdą niezgodność.

Sprawdzone: punkty 1–5. Do sprawdzenia: punkty 6–16 (testy wstrzymane na prośbę użytkownika).

Maszyny:
- **Amiga 1:** A1200, Warp1260, `C:/amiga-dev/amiagent.cfg`, gra w `Work:AmiXcom`.
- **Amiga 2:** A4000, Warp4070, `C:/amiga-dev/amiagent-2.cfg`, gra w `WorkSD:AmiXcom`.

## Wyniki: punkty 1–5

| # | Opcja | Wynik | Gdzie |
|---|---|---|---|
| 1 | VIDEO, skale Geoscape/Battlescape (1.5×, 1/3, 1/2) razem z „Mixed” | **Działa.** Geoscape 1.5× = 480×300, tryb `$770a1000`, `verr 0`, czyste wyjście. Kolory jak przy skali oryginalnej. Przybliżony globus ok. 29 kl./s (głównie blity z kluczem koloru). | Amiga 1 |
| 2 | VIDEO, Display Mode (Windowed / Borderless / Resizable) | **Bez wpływu w porcie.** sdlmini pomija te flagi, a sposób wyświetlania wybiera binarka (lub `-aga` / `-rtg` / `-ask`). Borderless+Resizable i domyślne dają to samo: 400 i 600 sprawdzeń, 0 niezgodności. | Amiga 2 |
| 3 | VIDEO, Filter (Scale / HQx / xBRZ) | **Poprawione.** Port nie obsługuje filtrów, więc menu pokazuje tylko „Disabled”. Commit `a722fa9`. | kod |
| 4 | ADVANCED, `battleSmoothCamera` | **Znaleziony i poprawiony błąd.** Kamera goniła pocisk wewnątrz `drawTerrain()`, już po tym, jak pamięć mapy dopasowała przesunięcie ekranu. Przed: 1 niezgodność na 80 519 px przy strzale (misja4). Po: 800 sprawdzeń, 0 niezgodności, z płynną kamerą i bez niej. Commit `89b76dc`. | Amiga 2 |
| 5 | ADVANCED, `battleExplosionHeight` / wybuchy | **Samego „ducha” po wybuchu nie odtworzono** (trzy gry z granatami, 0 niezgodności przy wybuchach). Przy okazji znaleziono i poprawiono dwie rzeczy, opisane niżej. `battleExplosionHeight` zmienia tylko zasięg obrażeń w pionie, nie rysowanie. | Amiga 1 |

### Poprawki z punktu 5

- **„Hidden Movement” i pamięć mapy (błąd widoczny, commit `224dbf4`).** W turze obcych, gdy na
  ekranie była plansza „Hidden Movement”, naprawa zaznaczonych pól rysowała planszę zamiast
  mapy, a mimo to kasowała zaznaczenia. Po powrocie mapy przez chwilę widać było stary obraz, na
  przykład teren sprzed strzału obcego. Użytkownik też to zauważył: „gra nie przerysowuje
  dokładnie ekranu”. Teraz, dopóki plansza jest na ekranie, zaznaczenia czekają na mapę.
  - Przed: 33 niezgodności, do 26 565 px, wszystkie w turze obcych (`misja1`).
  - Po: 0 na ponad 1200 sprawdzeń, 2 tury obcych ze strzałami w teren.
- **Zmiany terenu zgłaszają pole (zabezpieczenie, commit `4376ae0`).** Zniszczona część pola
  (`Tile::destroy` → `setMapData`) nie trafiała na listę zmienionych pól. Wybuch naprawiał więc
  tylko pola w odległości ±2 od środka i pola ze zmienionym światłem, a granat niszczy ściany do
  4–5 pól. Lukę znaleziono w kodzie, w grze jej nie zaobserwowano. Po zmianie: 600 sprawdzeń, 0
  niezgodności (2 granaty i tura obcych).
- **Log `mapverify`** podaje teraz do 300 niezgodności (wcześniej 30), wpisy „explosion at …”
  oraz przełączenia „map shown” / „hidden-movement screen”. Wpis o wybuchu pomija wybuchy, które
  zaczynają się w klatce z pełnym przerysowaniem; widać je wtedy tylko w licznikach `cache: … expl N`.

## Do sprawdzenia: punkty 6–16

**Wysokie ryzyko**
6. **MODS: `XcomUtil_Always_Nighttime` i mody TFTD przy UFO.**
   - W nocy światło żołnierzy zmienia jasność wielu pól przy każdym kroku i strzale. Lista
     zmienionych pól (256 pozycji) może się przepełniać, a wtedy gra przerysowuje całą mapę.
     Trzeba sprawdzić, czy jest 0 niezgodności, ile jest przepełnień i ile spada płynność
     względem dnia.
   - Mody TFTD przy UFO: czy gra zgłasza błąd porządnie, a nie zawiesza się.
   - Plan: `misja1` z modem nocnym i `amigaMapVerify`, użytkownik gra 1–2 tury (marsz, strzały,
     granat). Test TFTD bez użytkownika.

**Średnie: znane miejsca, gdzie widać było błędy obrazu**

7. **AMIGA, Mouse pointer „Original”.** Gra sama rysuje kursor. W trybie celowania sprawdzanie
   pokazywało już różnice w okolicy wskaźnika (`amigaFrameSkip 2`, ścieżka VRAM).
8. **BATTLESCAPE, `battleNotifyDeath` i ADVANCED, `autosave`.** Okienka i napis „SAVING GAME”.
   Użytkownik zgłaszał resztki obrazu po zamknięciu okienka.
9. **AMIGA, pasek tytułu ekranu „On”.** Ekran przesuwa się o pasek, a przełącznik w rogu się
   wyłącza. Warstwy VRAM z paskiem nie testowano po ostatnich zmianach.
10. **AMIGA, Split movement calculation „Off”.** Światło przy marszu liczy wtedy inna funkcja.
    Testowane było tylko domyślne „On”.
11. **AMIGA, Display standard PAL/NTSC.** Na RTG może zmienić wybór trybu ekranu.

**Niskie: wpływ na szybkość, nie na poprawność**

12. **AMIGA, Music „Live” / AdLib.** Miksowanie w trakcie gry zabiera procesor.
13. **BATTLESCAPE, Scroll speed / Edge scroll / Drag scroll.** Duże skoki kamery to pełne
    przerysowanie mapy.
14. **BATTLESCAPE, Path preview.** Każda zmiana podglądu przerysowuje całą mapę.
15. **GEOSCAPE, `globeSeasons`.** Czy tablice cienia globusa (`earthfix.dat`) uwzględniają pory roku.
16. **GENERAL, FPS limit.** Ogranicza tylko klatki.

## Inne sprawy otwarte

- **F12 (zrzut ekranu) zamyka grę bez śladu.** Odtworzone w menu głównym na Amidze 2: proces
  znika, nie ma ani `CPU TRAP`, ani `hang.log`. `Screen::screenshot` tworzy powierzchnię
  24-bitową, a sdlmini blituje tylko 8 bpp (zwraca -1), potem idzie `lodepng::encode`. Przyczyny
  jeszcze nie ustalono. Możliwa poprawka: zapis 8-bitowego PNG z paletą.
- **Wysyłka przez AmiAgenta raz uszkodziła plik.** 60 złych bajtów od pozycji 10 158 080 w
  13-MB binarce. `put` zgłasza przekroczenie czasu, choć plik dochodzi. Każdy wgrany plik
  sprawdzać: pobrać (`get`) i porównać sumę SHA-256.
- **Ikony UFO.**
  - Amiga 1: build 68, sprawdzony sumą, zawiera `224dbf4` i `4376ae0`.
  - Amiga 2: starsza wersja, bez obu poprawek. Podmienić na build 68 i sprawdzić sumą.
- **Poza commitem:** sonda czasu przeładowania modów (w `apply-amiga-patches.py`), eksperyment
  fastmalloc (`build.sh`, `native/amiga_fastmalloc.c`, `docker/build.ps1 -FastMalloc`) i `raporty/`.

## Narzędzia testowe (`winuae/perf-real`, poza repo)

| Skrypt | Do czego |
|---|---|
| `opt-test.ps1` | jeden test opcji: zapis opcji gracza, ustawienie testowych, przebieg autoinput, log, przywrócenie (`-Dir` dla innego katalogu gry) |
| `setopt.py` | ustawia wartości w `options.cfg` (puste wartości nie zjadają już następnej linii) |
| `alien-turn.ps1` | `misja1`: lądowanie, dwa końce tury i pełne tury obcych z `amigaMapVerify`; bez kontaktu z wrogiem nie wywołuje błędu „Hidden Movement” |
| `land-steps.ps1` | `misja1` od lądowania klik po kliku ze zrzutami ekranu |
| `far-shot.ps1`, `grenade-test.ps1`, `grenade-steps.ps1` | dalekie strzały i rzuty granatem w `misja4` (Amiga 2) |

Współrzędne w `misja1`:
- lądowanie „Tak” (80,160), odprawa OK (160,172), wyposażenie OK (254,11; ekran 320×200);
- ekran następnej tury (320,200 lub 360,236), koniec tury (418,355);
- zapalnik „0” (237,224).

Testy z udziałem użytkownika: uruchomić grę z opcjami testowymi i `>>oxc.log`. Log jest
zablokowany, dopóki gra działa, więc najpierw `stop <binarka>` (Ctrl-C), potem `get`.
