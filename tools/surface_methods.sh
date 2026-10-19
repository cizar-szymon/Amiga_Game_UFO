#!/bin/sh
# Print the bodies of the Surface write methods from the PRISTINE upstream tarball.
mkdir -p /tmp/up && cd /tmp/up
tar xzf /repo/upstream/openxcom-00fbacde.tar.gz --wildcards '*/src/Engine/Surface.cpp' '*/src/Engine/Surface.h'
F=$(ls */src/Engine/Surface.cpp)
for m in 'void Surface::clear' 'void Surface::copy' 'void Surface::drawRect(SDL_Rect' 'void Surface::drawRect(Sint16' 'void Surface::drawLine' 'void Surface::drawString' 'void Surface::offset(' 'void Surface::invert' 'void Surface::resize'; do
	n=$(grep -n "$m" "$F" | head -1 | cut -d: -f1)
	echo "=== $m (line $n)"
	sed -n "${n},$((n + 22))p" "$F" | sed '/^}/q'
done
