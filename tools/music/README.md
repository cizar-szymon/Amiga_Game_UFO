# tools/music - the X-COM music for AmiXcom, on the Amiga or on a PC

The game plays the PC tunes from disk: `user/music/<NAME>.raw` (14 tunes,
headerless 8-bit signed mono at 22050 Hz) and `user/music/source.txt` (what
they were made from). They are made from the player's own `GM.CAT` (samples,
through the port's instrument bank `data/common/music.bnk`) or `ADLIB.CAT`
(the YM3812 emulator). Being made from the game's data, they are never in the
repository or in a release archive (`PUBLISHING.md`).

Two ways to make them, with the game's own code:

| | What | Where it runs |
|---|---|---|
| `MakeMusic` | `makemusic.cpp`: the same conversion as a program of its own, icon in the release | the Amiga (68020+, no FPU) |
| `make_music.py` | builds `makemusic.cpp` for the PC and runs it | Windows (MSYS2 g++), Linux (g++ -m32), any (Docker) |

The game itself no longer converts anything at its start (patch 6ane,
2026-11-01: up to a quarter of an hour before the main menu read as a hang);
with no music it says "NO MUSIC YET - run MakeMusic" on the loading screen.
Only `-amigaConvertOnly 1` still converts in the game.

`makemusic.cpp` uses the game's own sources: `native/amiga_music.c` for
GM.CAT, the port's patched `Engine/Adlib` for ADLIB.CAT (`build/build.sh`
links the tree's own copy for the Amiga and warns when `tools/music/adlib/`,
the PC's copy, differs from it). It converts the same tunes as the game - those
of the game's `music.rul` (`standard/xcom1`, `xcom2` for Terror from the Deep)
that have a catPos, 14 for UFO - in the same order (the AdLib chips keep their
state from tune to tune), with the same per-tune gain, the same `CatFile`
reading and the same `source.txt`. UFO's list is also built in.

    MakeMusic [GAME=UFO|TFTD] [SOUND=dir] [BANK=file] [RULES=music.rul] [TO=dir]
              [SOURCE=SAMPLES|ADLIB] [QUALITY=SMOOTH|PLAIN] [ONLY=NAME]
    python tools/music/make_music.py --game AmiXcom --sound <X-COM dir> [--tftd] [--source adlib] [--compare dir]
    python tools/music/compare_raw.py DIR_A DIR_B

Defaults on the Amiga: the game's active one (`xcom1`/`xcom2` in `user/options.cfg`),
`PROGDIR:data/UFO/SOUND` (`data/TFTD/SOUND`), `PROGDIR:data/common/music.bnk`,
`PROGDIR:data/standard/xcom1/music.rul` (`xcom2`), `PROGDIR:user/music`, the source
and quality of the game's `user/options.cfg`. From the icon the same keys come
from its Tool Types (`(KEY=...)` is a hint).

**Terror from the Deep is not tested** (no TFTD data here): its 22-tune list
is read correctly; the conversion is the same code the game ran for TFTD.

## Verified (2026-11-01, Amiga 1: A1200, Warp1260 68060 @ 100 MHz, OS 3.2.2.1)

| | Samples (GM.CAT) | AdLib (ADLIB.CAT) |
|---|---|---|
| MakeMusic on the Amiga vs the game's own files (WinUAE and the real Amiga) | **identical**, 14/14 + source.txt | same lengths, envelope 0.997-1.000 (the game here is the FPU build) |
| PC (Windows MSYS2, Linux Docker -m32) vs the game | **identical**, 14/14 + source.txt | same lengths, envelope 0.997-1.000 |
| MakeMusic on the Amiga vs the PC | **identical** | same lengths, envelope 1.000, 12 tunes ~100% equal bytes, 2 at 94% |
| Time, MakeMusic on the Amiga | 27 s to RAM:, 23 s to the SD card | 871 s (14.5 min) |
| Time, the game's own conversion | - | 14 min 24 s with loading |

AdLib is not byte-identical across machines: the OPL emulator computes its
tables (sin, pow) and the note frequencies in floating point, and a 68060 FPU,
the soft-float library and a PC's libm round some of them differently. The
music is the same (length, loudness, envelope).

**A 32-bit `long` is required.** The conversion counts in `unsigned long`
(16.16 sample positions) and relies on it wrapping at 32 bits, as on the
Amiga. Built with a 64-bit long (Linux, macOS without -m32) 8 of the 14
sampled tunes came out different (other lengths, envelope down to 0.87).
MakeMusic refuses to run in such a build; `make_music.py` builds with `-m32`
outside Windows and in Docker.
