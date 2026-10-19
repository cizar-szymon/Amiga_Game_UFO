#!/bin/sh
# Print a function from the patched OpenXcom tree: showfn.sh <file under src/> <pattern> [lines]
F=/b/build/openxcom/src/$1
N=$(grep -n "$2" "$F" | head -1 | cut -d: -f1)
[ -z "$N" ] && { echo "not found: $2 in $1"; exit 1; }
sed -n "${N},$((N + ${3:-80}))p" "$F"
