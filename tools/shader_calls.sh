#!/bin/sh
# Every ShaderDraw<> call in pristine upstream, with the nearest lock()/unlock()
# around it, to see which direct pixel writers skip the lock.
mkdir -p /tmp/up && cd /tmp/up
tar xzf /repo/upstream/openxcom-00fbacde.tar.gz --wildcards '*/src/*.cpp'
cd */src
grep -rn 'ShaderDraw<' --include=*.cpp . | grep -v 'Engine/ShaderDraw' | while IFS=: read -r f n rest; do
	lo=$((n - 12)); [ $lo -lt 1 ] && lo=1
	before=$(sed -n "${lo},${n}p" "$f" | grep -c -E 'lock\(\)')
	after=$(sed -n "${n},$((n + 12))p" "$f" | grep -c -E 'unlock\(\)')
	echo "$f:$n lock-before=$before unlock-after=$after"
done
