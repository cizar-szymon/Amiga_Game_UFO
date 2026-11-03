# AmiXcom for the Amiga with a Warp card

**UFO: Enemy Unknown / X-COM: UFO Defense** (and *Terror from the Deep*) on the Amiga:
[OpenXcom](https://openxcom.org), the open source engine of the PC game, ported to AmigaOS 3.

**This is a special port for the Amiga 500 / 1200 / 3000 / 4000 with a Warp accelerator card:
Warp 560, Warp 1240, Warp 1260, Warp 1260 Extreme or Warp 7060.** It was ported and tested on
these machines - an A1200 with a Warp 1260 Extreme and an A4000 with a Warp 7060, both 68060 -
and it is tuned for them: the battle picture is kept in the card's fast graphics memory and
scrolled by the card's blitter, so only what changed travels to the card. That fast graphics
card is what makes the native high-resolution battles and the frame rates below possible.
Other 68040/68060 Amigas with an RTG card run the game too, but slower graphics cards will not
reach the same speed.

- the whole game: Geoscape, bases, research, interception, battles; UFO and Terror from the Deep
- battles native at 640x400 or 640x480 (the Geoscape on a 320x200 screen); on a Warp 060 at
  100 MHz about 50 fps standing, 33-37 fps scrolling, 28 fps while soldiers walk; the Geoscape
  about 49 fps
- the UFO intro plays with its sound while the game loads (about 30 s to the menu)
- music: the PC music made from your own data by **MakeMusic**, or the music and sound effects
  of the Amiga release of UFO
- 28 languages (OpenXcom's translations), following the Workbench locale or chosen in the game
- quick saving (0.14 s on the Geoscape), F10 bug reports, *Options → Amiga* with the Warp card,
  its firmware and CPU clock

## Play it

The ready-to-unpack release is in [`releases/amiga`](releases/amiga):
`AmiXcom-<version>.zip` (also unpacked next to it; on the Amiga:
`UnZip AmiXcom-<version>.zip -d Work:Games`). All the texts are in the drawer `doc`: follow
`doc/Install_en.txt` (Polish: `doc/Install_pl.txt`, German: `doc/Install_de.txt`) and
double-click **UFO**. The full player's description and the release history are in
[`doc/AmiXcom_release_en.txt`](releases/amiga/AmiXcom/doc/AmiXcom_release_en.txt) (Polish:
[`doc/AmiXcom_release.txt`](releases/amiga/AmiXcom/doc/AmiXcom_release.txt)).

**Requirements:** an Amiga with a Warp card (68040 or 68060; other 68040/68060 cards with RTG
work, slower), an RTG card with an 8-bit screen mode of 640x480 (Picasso96 / CyberGraphX),
64 MB of Fast RAM or more, AmigaOS 3.x (tested 3.2), about 40 MB of disk for the game data and
35 MB for the music. `UFO` uses the FPU; `UFO-noFPU` runs on a 68020/68030, LC060 or EC060.

**Settings:** *Options → Video* has the Warp entries "WARP RTG 320x200 / 640x400" and
"... / 640x480"; *Options → Amiga* has music, music source, sound effects, language from
Workbench, map reveal, split movement calculation and the FPS counter. Everything is kept in
`user/options.cfg`.

### The game data is not included

The graphics, maps, sounds and music are files of the original **X-COM / UFO for the PC**,
copyrighted by their owners. They are not in this repository and we do not link to downloads:
copy them from **your own copy** (CD, GOG or Steam) into `data/UFO/`:

`GEODATA GEOGRAPH MAPS ROUTES SOUND TERRAIN UFOGRAPH UNITS` (+ `UFOINTRO` for the intro).
Terror from the Deep goes into `data/TFTD/`; the Amiga release's music and sound effects
into `data/UFO_AMIGA/` (see `doc/Install_en.txt`).

### Music

The PC music is made once from your own `GM.CAT` (or `ADLIB.CAT`) - it is not distributed
either: on the Amiga double-click **MakeMusic** (about half a minute on a 68060 at 100 MHz, the
AdLib music about 15 minutes), or on a PC run `tools/music/make_music.py` (see
[`tools/music/README.md`](tools/music/README.md)) and copy the files to `user/music/`.

## Build

The repository never contains a modified copy of OpenXcom: the port is the pristine
`upstream/openxcom-00fbacde.tar.gz`, the mechanical patch set `build/apply-amiga-patches.py`
and everything Amiga-specific in `native/` (an SDL 1.2 shim, the graphics, audio and startup
layer).

- `build/build.sh` - unpacks upstream, applies the patches and builds with
  [bebbo's amiga-gcc](https://github.com/bebbo/amiga-gcc) 6.5 (Linux / WSL / Docker);
  `AMIGA_FPU=1` and `AMIGA_FASTMALLOC=1` give the `UFO` and `UFO-noFPU` of the release
- the RTG build needs the CyberGraphX developer headers in `native/cgx-include/` (not
  redistributable, not included)
- `build/gen_music_bank.py` - the instrument bank `data/common/music.bnk` (from the FluidR3
  soundfont); `tools/music/` - MakeMusic and `make_music.py`
- the release: `releases/amiga` is made by the workspace's `make_release.py` (the tree, the
  icons and `tools/make_zip.py`, which keeps the programs' Amiga E bit)
- `winuae/` - WinUAE configs and the test harness

How it was done, the decisions and the measurements: [`docs/dev/`](docs/dev) (`PROGRESS.md`,
`LEFTOFF.md`, `PORT_RESEARCH.md`; what is published: `PUBLISHING.md`).

## Licence and thanks

GPL-3.0, as OpenXcom (see `LICENSE`). OpenXcom is © the OpenXcom developers. X-COM is a
trademark of its owners; this project only contains the engine, not the game.

The Amiga port is **AmiXcom by Grzegorz Korycki (angree)**,
[github.com/angree/AmiXcom](https://github.com/angree/AmiXcom) - this repository is a fork of it
for the Warp cards, made with Claude Code. The graphics and audio layer comes from angree's
[OpenTTD Amiga port](https://github.com/angree/openttd_amiga_68k) (MIT), the c2p routines are
Mikael Kalms', the instrument samples in `music.bnk` come from Frank Wen's FluidR3 soundfont
(MIT, `data/common/FluidR3_License.txt`), and the "Retro" loading pictures are by **Banter**.
