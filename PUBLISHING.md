# Publishing AmiXcom - what goes to GitHub and what never does

Agreed with the owner on 2026-11-01, after OpenDUNE's rules (its
`docs/PORTING.md`, "Wydanie 0.46, GitHub"). Nothing is pushed, released or sent
as a pull request unless the owner asks for it.

## The repositories

| Repository | What it is | How work gets there |
|---|---|---|
| `github.com/angree/AmiXcom` (remote `upstream`) | the port's author's repository | pull requests, **one problem per PR** (cause, fix, measurements, how it was verified), from `fix/...` or `feat/...` branches; AGA and AGA-FPU must still build |
| `github.com/cizar-szymon/Amiga_Game_UFO` (remote `github`) | this RTG fork: sources and release archives | branches as they are worked on; `main` carries the released state |

## Public: the repository and the release archive

- Sources: `upstream/openxcom-00fbacde.tar.gz` (pristine OpenXcom), the patch
  script `build/apply-amiga-patches.py`, `native/`, `tools/` (with
  `tools/music/`: MakeMusic and `make_music.py`), `release/` (the texts).
- The release archive `AmiXcom-<version>.lha` (the workspace's
  `tools/make_release.py`): `UFO` (FPU), `UFO-noFPU`, `MakeMusic` and their
  icons, `data/common` (with `music.bnk`, `earthfix.dat` and the FluidR3
  licence - all three MUST ship), `data/standard`, the install texts
  (`Install_en.txt`, `Install_pl.txt`, `Install_de.txt`), the release notes,
  the empty `data/UFO`, `data/TFTD`, `data/UFO_AMIGA` and `user/music` with
  their README files.
- The install texts say where the game data comes from (the player's own
  X-COM for the PC - GOG, Steam or the CD; the list of directories, no links)
  and how the music is made: MakeMusic on the Amiga, or
  `tools/music/make_music.py` on a PC.

## Never public

- X-COM / Terror from the Deep data: anything in `data/UFO`, `data/TFTD`,
  `data/UFO_AMIGA` except the README files. `make_release.py` refuses to build
  a public archive that has any.
- **Converted music**: `user/music/*.raw` and `source.txt`. They are made from
  the player's own `GM.CAT` / `ADLIB.CAT`; the archive only explains how to
  make them.
- The Amiga UFO's own music files (`*.mus`, `UFO.bank`), the intro files.
- CyberGraphX headers (`native/cgx-include/`), Kickstart ROMs, `boot.hdf`.
- The private full archive `AmiXcom-<version>-full-PRIVATE.lha`
  (`make_release.py --full`: game data and converted music included) - only in
  the workspace's `releases/` (outside git), for the owner's own machines.

## Version and release notes

- One number: `0.<commit count>` while it is a beta (`docker/build.ps1` writes
  it to `native/amiga_buildid.h`), `1.xx` for official releases. It shows in
  the main menu and in `Version UFO`.
- Every build given to the player gets a new number and an entry in
  `release/AmiXcom_release.txt` / `_en.txt` (what is new and fixed, for a
  player), with how to report a bug at the top (F10 files + release number).

## Licences

The port is GPL-3.0 (OpenXcom). `music.bnk` is made from the FluidR3 GM
soundfont (MIT, `data/common/FluidR3_License.txt`). The AdLib player and the
YM3812 emulator (`Engine/Adlib`, copied to `tools/music/adlib`) come with
OpenXcom (LGPL / GPL compatible).
