#!/bin/sh
# How does objdump spell the 64-bit MULS.L that gcc emits for x / 3?
cat > /tmp/t.c <<'EOF'
int d3(int a) { return a / 3; }
unsigned u10(unsigned a) { return a / 10; }
EOF
m68k-amigaos-gcc -mcpu=68020 -msoft-float -O1 -noixemul -fomit-frame-pointer -c /tmp/t.c -o /tmp/t.o
m68k-amigaos-objdump -m m68k:68060 -d /tmp/t.o | grep -v '^$'
echo "== same with -mtune=68060"
m68k-amigaos-gcc -mcpu=68020 -mtune=68060 -msoft-float -O1 -noixemul -fomit-frame-pointer -c /tmp/t.c -o /tmp/t2.o
m68k-amigaos-objdump -m m68k:68060 -d /tmp/t2.o | grep -v '^$'
