#!/bin/sh
# How does this binary multiply and divide 32-bit integers?
m68k-amigaos-objdump -d "$1" > /tmp/dis.txt
echo "== calls to libgcc integer helpers"
for f in ___mulsi3 ___divsi3 ___udivsi3 ___modsi3 ___umodsi3 ___muldi3 ___divdi3 ___udivdi3; do
	printf '%-12s %s\n' "$f" "$(grep -c "<$f>" /tmp/dis.txt)"
done
echo "== long mul/div mnemonics anywhere"
grep -c -E 'mul[su]l|div[su]l|div[su]ll' /tmp/dis.txt
grep -E 'mul[su]l|div[su]l' /tmp/dis.txt | head -5
echo "== a small C test compiled with the game's flags"
cat > /tmp/t.c <<'EOF'
int m(int a, int b) { return a * b; }
int d(int a, int b) { return a / b; }
int d3(int a) { return a / 3; }
EOF
m68k-amigaos-gcc -mcpu=68020 -msoft-float -O1 -noixemul -fomit-frame-pointer -c /tmp/t.c -o /tmp/t.o && m68k-amigaos-objdump -d /tmp/t.o | grep -v '^$'
