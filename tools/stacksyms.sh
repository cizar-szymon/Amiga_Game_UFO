#!/bin/sh
# Does the binary carry libnix's stack-swap startup (which honours __stack)?
for f in "$@"; do
	echo "== $f"
	m68k-amigaos-nm "$f" 2>/dev/null | grep -i -E ' ___stack$| __stack$|stkinit|swapstack|StackSwap|__stk' | head
done
echo "== libnix: who uses __stack"
cd /opt/amiga/m68k-amigaos/libnix/lib
for a in libnix.a libstack.a; do
	echo "-- $a"; m68k-amigaos-nm -A $a 2>/dev/null | grep -i -E '___stack|stkinit|swapstack' | head
done
ls *.o
