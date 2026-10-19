#!/bin/sh
# Built like the game (68020, soft float, libnix). Run inside the amixcom-build image.
set -e
cd "$(dirname "$0")"
m68k-amigaos-gcc -mcpu=68020 -msoft-float -O1 -noixemul -fomit-frame-pointer -o mulbench mulbench.c -lm
ls -la mulbench
