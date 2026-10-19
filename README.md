# AmigaGAMES – porty gier na Amigę 1200 (68060, RTG) i PC

Wspólne zasady, narzędzia i dokumentacja dla wszystkich portów. Każda gra i każdy rdzeń procesora
ma **własne repozytorium** w swoim katalogu. To repozytorium ich nie obejmuje (`.gitignore`).

| Co | Gdzie |
|---|---|
| Plan portowania (fazy 0–5, standard klawiszy, wydania, numer wydania, F10) | `PORTING_PLAN.md` |
| Zdalne sterowanie Amigą z PC: program na Amigę, opis i protokół | `AmiAgent/` (`AMIAGENT.md`) |
| Dokumentacja karty Warp (architektura, RTG, rozszerzenia ARM) | `Warp*.pdf` |
| Zmierzone liczby Warpa: pamięć, kopiowanie, VRAM, blitter, sprite'y + zalecenia dla renderera | `WarpBench/` (`README.md`, programy testowe) |
| Rdzenie procesorów do ponownego użycia | `CPU/` – `M6809` (osobne repozytorium) |

## Projekty (osobne repozytoria)

| Katalog | Gra | Sposób |
|---|---|---|
| `Wizard of Wor/wizardofwor-master` | Wizard of Wor (C64) | przepisanie na C |
| `Wastar` | Vastar (automat, 2× Z80) | emulacja, rdzeń Z80 w asm 68k |
| `Xain'd Sleena` | Xain'd Sleena (automat, 3× 6809 + 68705) | emulacja, rdzeń 6809 w asm 68k |
| `Miner 2049er` | Miner 2049er | |
| `TombRider` | (OpenLara) | |
| `ufo/AmiXcom` | AmiXcom – OpenXcom (UFO / TFTD), fork portu angree | port ze źródeł, praca nad RTG 640×400 |

Konfiguracje AmiAgenta z hasłem (`C:\amiga-dev\amiagent*.cfg`) leżą poza repozytoriami i nie
wolno ich tu kopiować.
