# AmiXcom – praca nad portem OpenXcom (UFO: Enemy Unknown) na Amigę 68k

Pracujemy na forku projektu **AmiXcom** (https://github.com/angree/AmiXcom). To port OpenXcom (UFO: Enemy Unknown / X-COM: UFO Defense / Terror from the Deep) na klasyczną Amigę z AmigaOS 3.x. Autorem portu jest Grzegorz Korycki (angree). Licencja: GPL-3.0.

Odpowiadaj użytkownikowi **po polsku**. Kod, komentarze, opisy commitów i PR-ów pisz **po angielsku**, w stylu upstreamu.

## Cel

1. Uruchomić i ustabilizować wersję **RTG** (binarki `openxcom-rtg` / `openxcom-rtg-fpu`) na prawdziwym sprzęcie użytkownika.
2. Znaleźć i naprawić błędy specyficzne dla RTG i 68060.
3. Dopiero potem wprowadzać ulepszenia, na przykład wydajność na RTG albo lepsze wykorzystanie wyższych rozdzielczości.
4. Poprawki wysyłamy do autora jako pull requesty. Fork nie jest osobnym projektem.

## Sprzęt docelowy (maszyna użytkownika)

| | |
|---|---|
| Akcelerator | Amiga 1: A1200 + Warp1260X, **MC68060 @ 100 MHz** z FPU, firmware 3.054 (WarpTool); Amiga 2: A4000 + Warp4070 (060). Koprocesora ARM **nie używamy** |
| System | **AmigaOS 3.2.2.1** |
| Grafika | RTG wbudowane w kartę Warp (FPGA, to samo na obu Amigach), Picasso96 2.455, HDMI |
| Workbench | RTG 1280×1024 (Amiga 1) |
| Sieć na Amidze | tak: stos TCP/IP i AmiAgent na obu (Amiga 1 przez VPN – bywa wolna, Amiga 2 w LAN); wgrywanie buildów i pobieranie logów przez `tools/amiga_remote.py` |
| Locale | polskie ustawienia Workbencha (przecinek dziesiętny!) |

Właściwa binarka dla tej maszyny to **`UFO`** (wariant `-fpu-fm`: RTG, FPU, szybki alokator) w `Work:AmiXcom` (Amiga 1) / `WorkSD:AmiXcom` (Amiga 2); od 0.86 wydanie ma też `UFO-noFPU`.

## Środowisko pracy (PC)

- Windows z PowerShellem. Claude Code działa na PC, a Amiga jest osobną maszyną.
- Cross-kompilacja: toolchain m68k-amigaos (amiga-gcc bebbo) w **WSL** lub **Dockerze**. **Przed pierwszym buildem przeczytaj README i skrypty budowania w repozytorium**, bo to one określają wymagany toolchain, SDK (w tym nagłówki CyberGraphX dla RTG) i krok generowania `data/common/earthfix.dat`. Nie zgaduj tego.
- Emulator: **WinUAE** z konfiguracją możliwie bliską sprzętowi: 68060 + FPU, dużo Fast RAM (Z3), karta RTG (UAEGFX) i AmigaOS 3.2.

## Pierwsze kroki (kolejność ma znaczenie)

1. Sklonuj fork i dodaj `upstream` = `angree/AmiXcom`. Pracuj na gałęziach `fix/...` lub `feat/...`, nigdy na `main`.
2. Przeczytaj README, notatki do wydań i istniejący kod warstwy Amiga (SDL-mini, c2p/RTG, audio, timer).
3. Zbuduj **niezmienioną** aktualną wersję i porównaj zachowanie z oficjalnym wydaniem. Dopiero wtedy wprowadzaj zmiany.
4. Przygotuj plik konfiguracyjny WinUAE (`tools/winuae/amixcom-060-rtg.uae`) i skrypt PowerShell, który uruchamia test i zbiera logi.

## Dane gry – zasady

- **Nigdy** nie commituj ani nie rozpowszechniaj danych X-COM. Gra ich wymaga, ale nie są częścią repozytorium.
- Dane użytkownika (Steam/GOG) leżą lokalnie w `data/UFO/` i `data/TFTD/`. Potrzebne katalogi to GEODATA, GEOGRAPH, MAPS, ROUTES, SOUND, TERRAIN, UFOGRAPH, UNITS. Opcjonalnie dochodzą GM.CAT i ADLIB.CAT z muzyką.
- Upewnij się, że `.gitignore` wyklucza katalogi danych, pliki `.ybc` (cache), skonwertowaną muzykę i logi.

## Testy

### W WinUAE (robi to Claude)
- Uruchamiaj grę skryptem i zbieraj logi: `sdlmini.log`, `user/openxcom.log`, `hang.log` (zapisywany, gdy gra przez 30 s nie narysuje klatki), `oxc.log` (jeśli włączono logowanie wydajności opcją `amigaPerfLog`).
- `amigaAutoBattle: 1` w `options.cfg` uruchamia grę od razu w wygenerowanej bitwie, co przydaje się do testów battlescape.
- Ekranu nie widzisz. Wnioski wyciągaj z logów i ewentualnych zrzutów ekranu z WinUAE, a nie z założeń.

### Na prawdziwej Amidze (robi to użytkownik)
- Przygotuj paczkę testową: binarkę, zmienione pliki i krótką instrukcję po polsku, co kliknąć, co obserwować i które logi odesłać.
- Użytkownik odsyła `sdlmini.log`, `user/openxcom.log`, `user/options.cfg` i ewentualnie `hang.log`.
- Amiga jest w sieci: kopiowanie buildów i pobieranie logów robi Claude przez AmiAgenta (patrz wyżej).
- **Wynik z WinUAE nie jest dowodem dla prawdziwego sprzętu.** Szczególnie dotyczy to timingów, audio.device, FPU 68060 i sterownika RTG.

## Zasady pracy (styl upstreamu)

Autor pisze notatki do wydań bardzo rzetelnie: mierzy, zamiast zgadywać, i opisuje przyczynę błędu, a nie tylko objaw. Trzymaj się tego samego:

- **Mierz przed i po.** Każda optymalizacja wymaga liczb (ms na klatkę, fps, czas wczytywania) i informacji, na jakiej konfiguracji je zmierzono.
- **Najpierw reprodukcja.** Błąd zgłoszony z Amigi najpierw odtwórz w WinUAE albo przez test na sprzęcie, potem naprawiaj.
- **Minimalne zmiany.** Nie refaktoryzuj kodu OpenXcom bez potrzeby, bo port opiera się na commicie upstreamu z 2016 roku.
- **Nie łam AGA.** Każda zmiana w RTG musi zostawić działające buildy AGA i AGA-FPU. Zbuduj wszystkie warianty przed PR-em.
- Jeden PR to jeden problem. Opis PR-a zawiera przyczynę, poprawkę, pomiary i sposób weryfikacji.

## Znane pułapki tej platformy (z historii projektu)

- **Wbudowane funkcje matematyczne gcc:** własne wrappery `floorf`/`sqrtf`/`ceilf` gcc zamieniło w nieskończoną rekurencję (crash w 0.9.3). Pliki z własną matematyką kompiluj z `-fno-builtin`.
- **Locale:** `strtod` i `snprintf` w tej bibliotece C biorą separator dziesiętny z `locale.library` przy każdym wywołaniu, a `setlocale` tego nie wyłącza. Polski Workbench używa przecinka. Liczby zmiennoprzecinkowe parsuj i formatuj funkcjami portu, nigdy funkcjami z libc. **Użytkownik ma polskie locale**, więc testuj na nim.
- **audio.device:** `AbortIO()` nie przerywa trwającego `CMD_WRITE`, a następujący po nim `WaitIO()` wisi w nieskończoność. Kolejkę czyść przez `CMD_FLUSH`. Ten błąd częściej trafia się na szybkim CPU, czyli właśnie na 060.
- **Nazwy plików:** limit AmigaDOS (FFS) to 30 znaków. Zapisy mają maksymalnie 26 znaków plus `.sav`.
- **Mapowanie myszy:** w trybie 640×480 obraz jest rysowany 2x z pasami 40 linii. Wszelkie zmiany skalowania muszą też aktualizować mapowanie kursora.
- **Miskompilacja przy -O1:** odnotowano przypadek tablicy keycode'ów. Podejrzane zachowanie sprawdzaj na różnych poziomach optymalizacji.
- **Soft-float a FPU:** build bez FPU na maszynie z FPU korzystał z systemowych `mathieee*.library`, co dawało niestabilne zachowanie. Port ma teraz własną matematykę i tak ma zostać.
- **Pamięć:** gra potrzebuje dużo Fast RAM (dziesiątki MB). Obserwuj zużycie, gdy dodajesz cache lub bufory.
- **Stos:** gra wymaga stosu 1 MB, który ustawiają ikony i skrypt `run`.

## Kierunki prac na RTG (do zweryfikowania pomiarami)

- Potwierdzić, że ścieżka RTG pomija c2p i kopiuje bufor chunky bezpośrednio, a następnie zmierzyć koszt klatki na RTG i na AGA.
- Sprawdzić zachowanie przy ekranie RTG 8-bit oraz przy 15/16/24/32-bit (konwersja palety).
- **Natywna wyższa rozdzielczość na RTG (640×400 i więcej):** szczegółowy plan jest w `PLAN-HIRES.md`. Silnik OpenXcom obsługuje większą rozdzielczość bazową (`baseXResolution`, `geoscapeScale`, `battlescapeScale`), więc praca dotyczy głównie warstwy Amigi i optymalizacji portu. Realizuj plan fazami, zaczynając od fazy 0 (rozpoznanie bez zmian w kodzie). Po każdej fazie aktualizuj tabelę pomiarów w `PLAN-HIRES.md`.
- Ewentualne użycie koprocesora ARM na Warp1260X jest **poza zakresem**.

## Czego nie robić

- Nie publikuj binarek z danymi gry ani niczego wyprowadzonego z tych danych.
- Nie deklaruj, że coś „działa na Amidze”, dopóki użytkownik tego nie potwierdził na sprzęcie.
- Nie zgaduj wyników kompilacji ani testów. Jeśli czegoś nie da się uruchomić, powiedz to wprost.
