#!/bin/sh
# amiga-gcc; plain 68020 code (MOVE16 is emitted as .word), libnix, no floats at run time.
set -e
cd "$(dirname "$0")"
m68k-amigaos-gcc -mcpu=68020 -msoft-float -O2 -noixemul -fomit-frame-pointer -Wall -Wno-format -o vrambench vrambench.c -lamiga
m68k-amigaos-gcc -mcpu=68020 -msoft-float -O2 -noixemul -fomit-frame-pointer -Wall -Wno-format -o copybench copybench.c -lamiga
N=../../AmiXcom/native
m68k-amigaos-gcc -mcpu=68020 -msoft-float -O1 -noixemul -fomit-frame-pointer -Wall -Wno-format -I$N -o copytest copytest.c $N/amiga_copy.c -lamiga
m68k-amigaos-gcc -mcpu=68020 -msoft-float -O1 -noixemul -fomit-frame-pointer -Wall -Wno-format -I$N -o movetest movetest.c $N/amiga_copy.c -lamiga
m68k-amigaos-gcc -mcpu=68020 -msoft-float -O2 -noixemul -fomit-frame-pointer -Wall -Wno-format -I$N -o rowbench rowbench.c $N/amiga_copy.c -lamiga
ls -la vrambench copybench copytest movetest rowbench
