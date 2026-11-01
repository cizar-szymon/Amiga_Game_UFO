#!/usr/bin/env python3
"""Converts the X-COM music for AmiXcom on a PC - the same files MakeMusic
makes on the Amiga (the sampled music byte for byte; AdLib the same music,
see README.md).

The game plays its music from user/music/<NAME>.raw (14 tunes for UFO, ~34 MB,
8-bit signed mono, 22050 Hz, no header) plus user/music/source.txt; it does
not convert anything itself any more. The files are made from the player's
own X-COM files, so they are never in the repository or the release archive.
On the Amiga MakeMusic takes half a minute (AdLib: a quarter of an hour on a
68060); here seconds, and the files are then copied to the Amiga.

This builds tools/music/makemusic.cpp - the Amiga program MakeMusic, from the
same sources the game uses (native/amiga_music.c, the port's Engine/Adlib copy
in tools/music/adlib) - for the PC and runs it.

  python tools/music/make_music.py --game AmiXcom [--sound X-COM/SOUND] [--tftd]
                                   [--source samples|adlib] [--quality smooth|plain]
  python tools/music/make_music.py --sound X-COM/SOUND --bank music.bnk --to out

  --game     the unpacked AmiXcom drawer (the release archive): the bank is
             taken from data/common/music.bnk, the tune list from
             data/standard/xcom1 (xcom2) /music.rul, the music goes to
             user/music, the X-COM files are looked for in data/UFO (TFTD) /SOUND
  --tftd     Terror from the Deep (needs --game or --rules)
  --rules    the tune list (music.rul); without one UFO's built-in list is used
  --sound    the SOUND directory of X-COM for the PC (GM.CAT, ADLIB.CAT), or
             the game directory that holds it
  --bank     music.bnk (only for --source samples)
  --to       where the music goes
  --source   samples (GM.CAT, default) or adlib (ADLIB.CAT) - the same choice as
             Options -> Amiga -> Music source in the game
  --quality  smooth (default) or plain - only written to source.txt, as the
             game's own setting is (the files do not differ)
  --compare  a directory with music made by the game: every file is compared

Needs a C/C++ compiler: g++ on the PATH (on Linux with gcc-multilib: the
build is 32-bit, see makemusic.cpp), MSYS2 (C:/msys64) or Docker.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CACHE = os.path.join(HERE, ".build")


def find_compiler():
    """(gcc, g++, extra PATH) of a native compiler, or None."""
    for d in ([""] + [r"C:\msys64\ucrt64\bin", r"C:\msys64\mingw64\bin", r"C:\msys64\clang64\bin"]):
        gxx = os.path.join(d, "g++") if d else "g++"
        gcc = os.path.join(d, "gcc") if d else "gcc"
        if d and not os.path.exists(gxx + ".exe"):
            continue
        if shutil.which(gxx) or os.path.exists(gxx + ".exe"):
            return gcc, gxx, d
    return None


def build():
    """tools/music/.build/makemusic[.exe]; the path, or how to run it in Docker."""
    os.makedirs(CACHE, exist_ok=True)
    exe = os.path.join(CACHE, "makemusic.exe" if os.name == "nt" else "makemusic")
    srcs = [os.path.join(HERE, "makemusic.cpp"), os.path.join(REPO, "native", "amiga_music.c")] + \
        glob.glob(os.path.join(HERE, "adlib", "*"))
    if os.path.exists(exe) and all(os.path.getmtime(exe) >= os.path.getmtime(s) for s in srcs):
        return [exe]
    cc = find_compiler()
    if cc:
        gcc, gxx, d = cc
        env = dict(os.environ)
        if d:
            env["PATH"] = d + os.pathsep + env["PATH"]
        obj = os.path.join(CACHE, "amiga_music.o")
        inc = ["-I" + os.path.join(REPO, "native"), "-I" + os.path.join(HERE, "adlib")]
        # the conversion needs a 32-bit long, as on the Amiga (makemusic.cpp
        # explains); Windows has one, Linux and macOS need -m32
        m32 = [] if os.name == "nt" else ["-m32"]
        print("building MakeMusic for this PC (%s %s)" % (gxx, " ".join(m32)))
        try:
            subprocess.run([gcc, "-O2", "-w"] + m32 + inc + ["-c", os.path.join(REPO, "native", "amiga_music.c"), "-o", obj],
                           check=True, env=env)
            subprocess.run([gxx, "-O2", "-w"] + m32 + inc + ["-o", exe, os.path.join(HERE, "makemusic.cpp"),
                            os.path.join(HERE, "adlib", "adlplayer.cpp"), os.path.join(HERE, "adlib", "fmopl.cpp"), obj],
                           check=True, env=env)
            return [exe]
        except subprocess.CalledProcessError:
            if not shutil.which("docker"):
                sys.exit("The build failed. On Linux a 32-bit build needs gcc-multilib and g++-multilib "
                         "(or Docker).")
            print("the native build failed - using Docker instead")
    if shutil.which("docker"):
        print("building and running MakeMusic in Docker (Debian, g++ -m32)")
        return None   # run() does it in the container
    sys.exit("No C++ compiler: install g++ (on Windows MSYS2: pacman -S mingw-w64-ucrt-x86_64-gcc) or Docker.")


def run_docker(args):
    """Build and run in a Linux container; every path is mounted under /m<N>."""
    mounts, cargs, n = [], [], 0
    for a in args:
        k, v = a.split("=", 1)
        n += 1
        full = os.path.abspath(v)
        if k in ("BANK", "RULES"):
            mounts += ["-v", "%s:/m%d:ro" % (os.path.dirname(full), n)]
            cargs.append("%s=/m%d/%s" % (k, n, os.path.basename(full)))
        elif k in ("SOUND", "TO"):
            if k == "TO":
                os.makedirs(full, exist_ok=True)
            mounts += ["-v", "%s:/m%d" % (full, n)]
            cargs.append("%s=/m%d" % (k, n))
        else:
            cargs.append(a)
    sh = ("apt-get update -qq >/dev/null && apt-get install -y -qq g++-multilib >/dev/null 2>&1 && "
          "cd /src/tools/music && gcc -m32 -O2 -w -I../../native -c ../../native/amiga_music.c -o /tmp/am.o && "
          "g++ -m32 -O2 -w -I../../native -Iadlib -o /tmp/makemusic makemusic.cpp adlib/adlplayer.cpp adlib/fmopl.cpp /tmp/am.o && "
          "/tmp/makemusic " + " ".join("'%s'" % c for c in cargs))
    return subprocess.run(["docker", "run", "--rm", "-v", "%s:/src:ro" % REPO] + mounts +
                          ["debian:bookworm-slim", "sh", "-c", sh]).returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--game")
    ap.add_argument("--sound")
    ap.add_argument("--bank")
    ap.add_argument("--to")
    ap.add_argument("--source", choices=["samples", "adlib"], default="samples")
    ap.add_argument("--quality", choices=["smooth", "plain"], default="smooth")
    ap.add_argument("--tftd", action="store_true", help="Terror from the Deep instead of UFO")
    ap.add_argument("--rules", help="the tune list, music.rul (default: the game's, with --game)")
    ap.add_argument("--compare")
    a = ap.parse_args()

    sound, bank, to, rules = a.sound, a.bank, a.to, a.rules
    if a.game:
        bank = bank or os.path.join(a.game, "data", "common", "music.bnk")
        to = to or os.path.join(a.game, "user", "music")
        sound = sound or os.path.join(a.game, "data", "TFTD" if a.tftd else "UFO", "SOUND")
        rules = rules or os.path.join(a.game, "data", "standard", "xcom2" if a.tftd else "xcom1", "music.rul")
    if a.tftd and not (rules and os.path.isfile(rules)):
        sys.exit("Terror from the Deep needs its tune list: --game (the AmiXcom archive) or --rules "
                 "data/standard/xcom2/music.rul")
    if sound and not glob.glob(os.path.join(sound, "*.[Cc][Aa][Tt]")):
        for sub in ("SOUND", "sound"):          # the game directory instead of its SOUND
            if os.path.isdir(os.path.join(sound, sub)):
                sound = os.path.join(sound, sub)
                break
    if not sound or not to or (a.source == "samples" and not bank):
        ap.error("give --game, or --sound, --to (and --bank for samples)")
    if a.source == "samples" and not os.path.isfile(bank):
        sys.exit("no instrument bank %s - it is data/common/music.bnk of the AmiXcom archive" % bank)

    args = ["SOUND=" + sound, "TO=" + to, "SOURCE=" + a.source.upper(), "QUALITY=" + a.quality.upper(),
            "GAME=" + ("TFTD" if a.tftd else "UFO")]
    if a.source == "samples":
        args.insert(1, "BANK=" + bank)
    if rules and os.path.isfile(rules):
        args.append("RULES=" + rules)
    exe = build()
    if exe is None:
        rc = run_docker(args)
    else:
        os.makedirs(to, exist_ok=True)
        rc = subprocess.run(exe + args).returncode
    if rc != 0:
        sys.exit(rc)

    if a.compare:
        same = diff = 0
        for f in sorted(glob.glob(os.path.join(a.compare, "*.raw"))):
            mine = os.path.join(to, os.path.basename(f))
            ok = os.path.isfile(mine) and open(f, "rb").read() == open(mine, "rb").read()
            print("%-14s %s" % (os.path.basename(f), "identical" if ok else "DIFFERENT"))
            same += ok
            diff += not ok
        print("%d identical, %d different" % (same, diff))
    print("\nCopy %s to the Amiga as <AmiXcom drawer>/user/music." % os.path.abspath(to))


if __name__ == "__main__":
    main()
