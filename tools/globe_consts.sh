#!/bin/sh
mkdir -p /tmp/up && cd /tmp/up
tar xzf /repo/upstream/openxcom-00fbacde.tar.gz --wildcards '*/src/Geoscape/Globe.*' '*/src/Engine/ShaderDraw*' '*/src/Engine/Palette.h'
cd */src
grep -n -E 'ColorGroup|ColorShade|OCEAN_COLOR|blockOffset' Geoscape/Globe.h Geoscape/Globe.cpp Engine/Palette.h | head -20
grep -n -B2 -A40 'shade_gradient\[' Geoscape/Globe.cpp | grep -n -E 'shade_gradient|for|=' | head -30
grep -n -A12 'random_surf_size' Geoscape/Globe.cpp | head -40
