# Publishing AmiXcom - what goes to GitHub and what never does

Agreed with the owner on 2026-11-01, after OpenDUNE's rules (its
`docs/PORTING.md`, "Wydanie 0.46, GitHub"). Nothing is pushed, released or sent
as a pull request unless the owner asks for it.

## The repositories

| Repository | What it is | How work gets there |
|---|---|---|
| `github.com/angree/AmiXcom` (remote `upstream`) | the port's author's repository | pull requests, **one problem per PR** (cause, fix, measurements, how it was verified), from `fix/...` or `feat/...` branches; AGA and AGA-FPU must still build |
| `github.com/cizar-szymon/Amiga_Game_UFO` (remote `github`) | this RTG fork: sources and release archives | branches as they are worked on; `main` carries the released state |

## Public: the repository and the release (after OpenDUNE, from 0.106)

- Sources: `upstream/openxcom-00fbacde.tar.gz` (pristine OpenXcom), the patch
  script `build/apply-amiga-patches.py`, `native/`, `tools/` (with
  `tools/music/`: MakeMusic and `make_music.py`), `release/` (the texts).
- **The release lives in the repository**: `releases/amiga/AmiXcom/` (unpacked,
  in git), `releases/amiga/AmiXcom.info` (a plain drawer icon) and
  `releases/amiga/AmiXcom-<version>.zip` - only the current release's ZIP; the
  older ones are removed from git (history: git and the release texts). The
  workspace's `tools/make_release.py` makes all three and refuses a build whose
  `$VER` is not the version given.
- ZIP, not LHA (the owner's choice; LhA in the background in WinUAE ended with
  error 20): `tools/make_zip.py` stores Unix permissions, programs 755, the
  rest 644 - without them Amiga UnZip drops the E bit and the program cannot
  be started.
- The tree: `UFO` (FPU), `UFO-noFPU`, `MakeMusic` and their icons, `data/common`
  (with `music.bnk`, `earthfix.dat` and the FluidR3 licence - all three MUST
  ship), `data/standard`, ONE drawer `doc/` with its drawer icon `doc.info`
  holding all the texts: the install texts (`Install_en.txt`,
  `Install_pl.txt`, `Install_de.txt`) and the release texts with icons that
  open MultiView, `COPYING` (GPL-3.0); the empty `data/UFO`, `data/TFTD`,
  `data/UFO_AMIGA` and `user/music` with their README files (these stay in
  their drawers - they say what goes there).
- The install texts say where the game data comes from (the player's own
  X-COM for the PC - GOG, Steam or the CD; the list of directories, no links)
  and how the music is made: MakeMusic on the Amiga, or
  `tools/music/make_music.py` on a PC.
- Before pushing a release: the public ZIP unpacked with UnZip on a real Amiga
  (RAM:), game data and music copied in, started from its icon, the number in
  the menu and in `Version UFO`.

## Never public

- X-COM / Terror from the Deep data: anything in `data/UFO`, `data/TFTD`,
  `data/UFO_AMIGA` except the README files. `make_release.py` refuses to build
  a public archive that has any.
- **Converted music**: `user/music/*.raw` and `source.txt`. They are made from
  the player's own `GM.CAT` / `ADLIB.CAT`; the archive only explains how to
  make them.
- The Amiga UFO's own music files (`*.mus`, `UFO.bank`), the intro files.
- CyberGraphX headers (`native/cgx-include/`), Kickstart ROMs, `boot.hdf`.
- The private full archive `private/AmiXcom-<version>-full.zip` (game data
  and converted music included) - `private/` is in `.gitignore`, only on the
  owner's PC, for his own machines.

### Układ paczek (3.11.2026, standard portów Settlers i OpenDUNE)

- `releases/amiga/AmiXcom/` + `releases/amiga/AmiXcom-<wersja>.zip` - publiczne, w gicie.
- `private/AmiXcom-<wersja>-full.zip` - pełna paczka z danymi gry i muzyką, tylko na tym PC
  (`/private/` w `.gitignore`). W `private/` tylko bieżąca; starsze skrypt przenosi do
  `private/old/` (nic nie jest kasowane). Tam też dawne archiwa LHA 0.86 i 0.100 (publiczne
  i pełne) i stary katalog `stage` (jako `stage-0.100`) z dawnego `UFO/releases/` workspace'u.
- Jeden skrypt `tools/make_release.py <wersja>` workspace'u robi wszystko: drzewo publiczne,
  publiczny ZIP i pełny ZIP (`tools/make_zip.py`, uprawnienia Unix). `--public-only` - bez
  pełnej paczki; `--full-only` - tylko pełna paczka z istniejącego drzewa publicznego.
  Odmawia, gdy `$VER` binarek nie jest podaną wersją, i gdy w drzewie publicznym są dane gry.
- Wszystkie teksty w paczce w jednej szufladzie `doc/` (z ikoną `doc.info`); ReadMe w pustych
  katalogach danych zostają na miejscu.

## Version and release notes

- One number: `0.<commit count>` while it is a beta (`docker/build.ps1` writes
  it to `native/amiga_buildid.h`), `1.xx` for official releases. It shows in
  the main menu and in `Version UFO`.
- Every build given to the player gets a new number and an entry in
  `release/AmiXcom_release.txt` / `_en.txt` (what the game has and does, for
  a player), with how to report a bug at the top (F10 files + release number).

## What the public texts say (from 2026-11-02, all ports: PORTING_PLAN.md, phase 5)

- **The README starts with the target hardware**: a special port for the
  Amiga 500 / 1200 / 3000 / 4000 with a Warp card (Warp 560, 1240, 1260,
  1260 Extreme, 4070), ported and tested on them; the Warp's fast graphics
  card gives the speeds quoted. Other 68040/68060 Amigas with RTG run the
  game, slower. Name only the machines it was really tested on (now: A1200 +
  Warp 1260 Extreme, A4000 + Warp 4070).
- Then, after OpenDUNE's README: what the game has (a short list), how to
  play, requirements, settings, the game data is not included (where it
  comes from), build, licence and thanks to angree.
- **Public texts describe only how the game works now.** README, release
  history and the player's texts carry no "fixed", "no longer", "before",
  "fixed for the next release" and no before -> after numbers: give today's
  numbers, describe a fix as present behaviour or leave it out. The history
  of bugs and measurements stays in `docs/dev/PROGRESS.md` and
  `docs/dev/LEFTOFF.md`.
- Working notes live in `docs/dev/`; only `CLAUDE.md` stays in the root (that
  is where Claude reads it).

## Licences

The port is GPL-3.0 (OpenXcom). `music.bnk` is made from the FluidR3 GM
soundfont (MIT, `data/common/FluidR3_License.txt`). The AdLib player and the
YM3812 emulator (`Engine/Adlib`, copied to `tools/music/adlib`) come with
OpenXcom (LGPL / GPL compatible).
