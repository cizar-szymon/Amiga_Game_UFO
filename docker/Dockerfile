# AmiXcom build image: bebbo amiga-gcc 6.5.0b (sacredbanana image) plus what
# AmiXcom/build/build.sh expects from the author's WSL setup.
FROM sacredbanana/amiga-compiler:m68k-amigaos

# build.sh hard-codes PATH=/opt/amiga/bin and the vasm include path
# /opt/amiga/m68k-amigaos/ndk-include; the base image already links
# /opt/amiga to its toolchain, so nothing to do there.

# gen_splash.py / prep_retro_intro.py need Pillow.
RUN pip install --no-cache-dir --break-system-packages pillow \
 || pip install --no-cache-dir pillow
