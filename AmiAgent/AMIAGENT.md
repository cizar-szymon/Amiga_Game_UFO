# AmiAgent – zdalne sterowanie Amigą z PC (instrukcja dla Claude i dla nowych projektów)

AmiAgent to mały serwer TCP działający na Amidze (prawdziwej albo w WinUAE). Przez niego z PC można
wgrać plik, uruchomić program, zatrzymać go, zrobić zrzut ekranu, przeczytać tytuły okien i
nacisnąć klawisz. Dzięki temu Claude może sam testować program na Amidze: zbudować go, wgrać, uruchomić,
odczytać wynik i zobaczyć ekran.

W nowym projekcie wystarczy w `CLAUDE.md` dopisać linię:

```
@C:/Workprojects/AmigaGAMES/AmiAgent/AMIAGENT.md
```

## Gdzie co jest

| Co | Gdzie |
|---|---|
| Program na Amigę (gotowy, 20 KB) | `C:\Workprojects\AmigaGAMES\AmiAgent\AmiAgent` (+ `ReadMe.txt`) |
| Źródło agenta (vbcc, Roadshow) | `C:\Workprojects\AmigaGAMES\Wizard of Wor\wizardofwor-master\amiagent\AmiAgent.c` + `Makefile` |
| Klient na PC (Python 3, bez zależności; PIL opcjonalnie do PNG) | `tools/amiga_remote.py` w repo Wizard of Wor i Vastar (identyczne kopie) |
| Konfiguracja: prawdziwa Amiga (domyślna) | `C:\amiga-dev\amiagent.cfg` |
| Konfiguracja: WinUAE (127.0.0.1) | `C:\amiga-dev\amiagent-winuae.cfg` |
| Konfiguracje WinUAE z siecią | `C:\amiga-dev\winuae\*_net.uae` (np. `Vastar_A1200_OS32_net.uae`) |
| Opis protokołu i historia testów | `Wizard of Wor\wizardofwor-master\docs\PORTING.md`, rozdział 8b |

Pliki `.cfg` zawierają hasło – leżą poza repozytoriami i **nie wolno ich kopiować do repo ani
wypisywać haseł w dokumentacji/commitach**. Format:

```
host = 192.168.5.115
port = 6800
password = ...
```

Kolejność ustawień (późniejsze wygrywa): plik `--config` (domyślnie `C:/amiga-dev/amiagent.cfg`),
zmienne `AMIAGENT_HOST` / `AMIAGENT_PORT` / `AMIAGENT_PASSWORD`, opcje `--host` / `--port` / `--password`.

## Docelowa maszyna

- Prawdziwa A1200: 68060 @ 50 MHz, AmigaOS 3.x, Picasso96 (Workbench 16-bit), AHI, Roadshow.
  Adres w `amiagent.cfg`. Wolno wgrywać do katalogów projektu w `Work:` (np. `Work:Vastar`, `Work:WoW`);
  przy nowym projekcie zapytaj użytkownika o katalog docelowy.
- WinUAE: A1200 68060, OS 3.2, RTG, `bsdsocket_emu=true` (stos TCP/IP emulowany siecią PC).
  Katalog `build/amiga` projektu jest podmontowany jako wolumin (np. `VASTAR:`), więc po zbudowaniu
  nie trzeba nic wgrywać – wystarczy `start`.

## Klient PC: polecenia

Uruchamiaj z katalogu repo projektu. Dla WinUAE dodaj `--config C:/amiga-dev/amiagent-winuae.cfg`
(opcje globalne stoją **przed** nazwą polecenia).

```sh
python tools/amiga_remote.py ping                         # test połączenia -> "OK ..."
python tools/amiga_remote.py put build/amiga/Gra Work:Gra/Gra
python tools/amiga_remote.py get S:User-Startup out.txt
python tools/amiga_remote.py run "List Work:Gra"          # czeka na koniec; wypisuje wyjście i [rc N]
python tools/amiga_remote.py start "Work:Gra/Gra STATS"   # w tle, nie czeka
python tools/amiga_remote.py stop Gra                     # Ctrl-C do procesów o tej nazwie
python tools/amiga_remote.py titles                       # "ekran | * okno" (* = aktywne)
python tools/amiga_remote.py shot ekran.png [x y w h]     # zrzut przedniego ekranu
python tools/amiga_remote.py activate "Tytuł"             # aktywuje okno po początku tytułu
python tools/amiga_remote.py key ctrl [ms] --window "Tytuł"   # klawisz (nazwa albo kod 0x63)
python tools/amiga_remote.py deploy --local build/amiga/Gra --remote-dir Work:Gra --args "STATS"
python tools/amiga_remote.py stats [--wait 5]             # tytuł okna ze statystykami
```

Nazwy klawiszy: `esc space return enter tab backspace up down left right ctrl lshift rshift capslock
lalt ralt lamiga ramiga del help f1..f10 0..9 a..z`; inne – surowy kod Amigi (`key 0x45`).

`deploy` = `stop` (i 1,5 s przerwy na zamknięcie okna/AHI) → `put` → `start`. Domyślny katalog to `RAM:`.

## Protokół (gdyby trzeba było napisać własnego klienta)

TCP, port 6800, linie tekstowe zakończone `\n`, kodowanie latin-1. Pierwsza linia `AUTH <hasło>`.
Odpowiedź zaczyna się od `OK` albo `ERR <opis>`.

| Polecenie | Odpowiedź / działanie |
|---|---|
| `PING` | `OK ...` |
| `PUT <rozmiar> <ścieżka>` + bajty | zapis pliku |
| `GET <ścieżka>` | `OK <rozmiar>` + bajty |
| `RUN <polecenie>` | `OK <rc> <rozmiar>` + wyjście (przez `T:`), synchronicznie |
| `START <polecenie>` | uruchomienie asynchroniczne (`SystemTags`) |
| `STOP <nazwa>` | `OK <liczba procesów>` – Ctrl-C do procesów CLI z tym programem |
| `TITLES` | `OK <rozmiar>` + linie `ekran\|okno`, aktywne okno z `*` na początku |
| `SHOT [x y w h]` | `OK <w> <h> <rozmiar>` + surowe RGB 24-bit (RTG: `p96ReadPixelArray`, AGA: `ReadPixel`) |
| `ACTIVATE <początek tytułu>` | aktywuje okno (ERR, gdy brak) |
| `KEY <kod> [ms]` | naciśnięcie przez `input.device` – trafia do **aktywnego** okna |
| `QUIT` | koniec sesji |

## Nowy projekt: przygotowanie

1. Skopiuj `tools/amiga_remote.py` z repo Vastar lub Wizard of Wor do `tools/` nowego projektu.
2. Dopasuj dwie rzeczy specyficzne dla Wizard of Wor:
   - `GAME = ... "build", "amiga", "WizardOfWor"` – domyślny plik dla `deploy`,
   - domyślne `--window "WoW"` w poleceniu `key` (i prefiksy `["WoW", "Wizard of Wor"]`) oraz
     `startswith("WoW ")` w `stats` – na początek tytułu okna nowej gry.
3. Program powinien kończyć się na Ctrl-C (`SetSignal(0,0) & SIGBREAKF_CTRL_C` w pętli głównej),
   inaczej `stop`/`deploy` go nie zamkną.
4. Dobrze mieć opcję `STATS`, która wpisuje do tytułu okna fps/CPU – `titles`/`stats` odczytują to
   także wtedy, gdy na pełnym ekranie tytuł nie jest widoczny.
5. Dla WinUAE: skopiuj `*_net.uae`, zmień `config_description` i linię
   `filesystem2=rw,NAZWA:Nazwa:C:\...\build\amiga,-128` na katalog nowego projektu.
   Start: `winuae64.exe -f C:\amiga-dev\winuae\<plik>.uae -s use_gui=no`.

## Typowy cykl testu (dla Claude)

1. Zbuduj wersję Amigi (vbcc z `C:\amiga-dev\vbcc`, Makefile projektu).
2. `ping` – jeśli nie odpowiada: poproś użytkownika o włączenie Amigi / WinUAE albo sprawdzenie,
   czy agent działa (`Status` w Shellu, `Run >NIL: C:AmiAgent`). Nie zgaduj innego adresu.
3. `deploy` (prawdziwa Amiga) albo `start "NAZWA:Gra OPCJE"` (WinUAE z podmontowanym katalogiem).
4. Odczekaj kilka sekund, potem `titles` / `stats` – czy okno się otworzyło, ile fps.
5. `key ...` do sterowania (najpierw aktywacja okna – `key` robi to sam przez `--window`).
6. `shot` do pliku w katalogu scratchpad i obejrzyj go narzędziem Read (PNG). Bez PIL zapisze `.ppm`.
7. Programy testowe wypisujące tekst uruchamiaj przez `run` – wyjście i kod powrotu wracają od razu.
8. Na koniec `stop <nazwa>`, żeby nie zostawić działającego programu.

## Uwagi i pułapki

- `KEY` idzie do aktywnego okna – jeśli program nie reaguje, sprawdź `titles`, czy jest oznaczony `*`.
- `RUN` blokuje do końca polecenia; programów graficznych/niekończących się nie uruchamiaj przez `run`, tylko `start`.
- Pomiary wydajności rób na prawdziwej A1200 (060/50) – WinUAE bywa znacznie szybszy i ukrywa
  wąskie gardła (np. wolne `ReadJoyPort()`, dostęp do CIA, zapis do karty graficznej).
- Hasło daje pełną kontrolę nad Amigą: agent tylko w domowej sieci, bez przekierowania portu 6800.
- W narzędziu Bash złożone heredoki z Pythonem psują cudzysłowy – skrypty pomocnicze zapisuj do plików.

## Instalacja agenta na nowej Amidze

1. Skopiuj `AmiAgent` do `C:` (wymaga OS 3.0+ i stosu TCP/IP, np. Roadshow).
2. `Echo "TwojeHaslo" >ENVARC:AmiAgent.key` (bez spacji).
3. Na końcu `S:User-Startup`:
   ```
   ;BEGIN AmiAgent
   Run >NIL: C:AmiAgent
   ;END AmiAgent
   ```
4. Restart albo `Run >NIL: C:AmiAgent`; adres IP: `ShowNetStatus`. Inny port: `C:AmiAgent PORT=nnnn`.
5. Wpisz host i hasło do `C:\amiga-dev\amiagent.cfg` na PC.

Budowa agenta ze źródeł: `make` w katalogu `amiagent/` repo Wizard of Wor (vbcc `+aos68k_win`,
NDK 3.2 + netinclude Roadshow) → `build/amiga/AmiAgent`.
