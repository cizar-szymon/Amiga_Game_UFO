#!/bin/sh
# Build the Warp benchmarks with bebbo amiga-gcc (NDK 3.2 + cybergraphics headers).
# On this PC, from PowerShell:
#   docker run --rm -v "C:/Workprojects/AmigaGAMES/WarpBench:/w" amixcom-build sh /w/build.sh
# (amixcom-build = sacredbanana/amiga-compiler:m68k-amigaos + Pillow, see ufo/docker).
# 68020 code everywhere; MOVE16 is emitted as .word and only run on a 68040/060.
set -e
cd "$(dirname "$0")"
F="-mcpu=68020 -msoft-float -O2 -noixemul -fomit-frame-pointer -Wall -Wno-format"
for p in vrambench copybench blitbench; do
	m68k-amigaos-gcc $F -o $p $p.c -lamiga
done
ls -la vrambench copybench blitbench
