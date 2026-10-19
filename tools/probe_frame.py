#!/usr/bin/env python3
"""TEMPORARY probe, applied to the already-patched tree in the build volume
(never to the repository). Times, with the E-clock, every part of a frame:
Screen::clear, each state's blit (by type name), the FPS counter, the cursor,
and inside Globe::blit the globe surface and its three overlay layers.
Logs "fprobe:" every 100 frames when amigaPerfLog is on (us per frame).
Build afterwards with AMIGA_NO_PATCH=1; undo with build.sh clean.
usage: probe_frame.py <openxcom/src>"""
import os
import sys

src = sys.argv[1]
MARK = "AMIGA-PROBE-FRAME"


def patch(path, old, new):
    p = os.path.join(src, path)
    t = open(p, encoding="latin-1").read()
    if MARK in t and new in t:
        print("already:", path)
        return
    if t.count(old) != 1:
        sys.exit(f"{path}: anchor found {t.count(old)} times")
    open(p, "w", encoding="latin-1", newline="").write(t.replace(old, new))
    print("patched:", path)


# --- Game.cpp: time clear, each state, fps counter, cursor ---------------------
patch("Engine/Game.cpp",
      "\t\t\t\t_screen->clear();\n",
      "\t\t\t\t/* " + MARK + " */\n"
      "\t\t\t\tunsigned long fpT_ = amiga_uclock_us();\n"
      "\t\t\t\t_screen->clear();\n"
      "\t\t\t\tAmFp_clear += amiga_uclock_us() - fpT_;\n")
patch("Engine/Game.cpp",
      "\t\t\t\t\t(*i)->blit();\n",
      "\t\t\t\t\t{ unsigned long fs_ = amiga_uclock_us(); (*i)->blit(); AmFpState(typeid(**i).name(), amiga_uclock_us() - fs_); }\n")
patch("Engine/Game.cpp",
      "\t\t\t\t_fpsCounter->blit(_screen->getSurface());\n",
      "\t\t\t\t{ unsigned long ff_ = amiga_uclock_us(); _fpsCounter->blit(_screen->getSurface()); AmFp_fps += amiga_uclock_us() - ff_; }\n")
patch("Engine/Game.cpp",
      "\t\t\t\t\t_screen->flip();\n",
      "\t\t\t\t\t_screen->flip();\n"
      "\t\t\t\t\tif (++AmFp_n >= 100) { AmFpReport(); }\n")
patch("Engine/Game.cpp",
      "void Game::run()\n",
      "/* " + MARK + ": accumulators, us over AmFp_n frames */\n"
      "unsigned long AmFp_clear = 0, AmFp_fps = 0, AmFp_g[4] = { 0, 0, 0, 0 };\n"
      "static unsigned long AmFp_n = 0;\n"
      "static const char *AmFpName[6]; static unsigned long AmFpUs[6];\n"
      "static void AmFpState(const char *name, unsigned long us)\n"
      "{\n"
      "\tint k;\n"
      "\tfor (k = 0; k < 6; ++k) if (AmFpName[k] == name || AmFpName[k] == 0) break;\n"
      "\tif (k == 6) k = 5;\n"
      "\tAmFpName[k] = name; AmFpUs[k] += us;\n"
      "}\n"
      "static void AmFpReport()\n"
      "{\n"
      "\tchar b[400]; int n = 0, k;\n"
      "\tunsigned long d = AmFp_n ? AmFp_n : 1;\n"
      "\tn += snprintf(b + n, sizeof b - n, \"fprobe: us/frame clear %lu fps %lu globe[surf %lu radar %lu ctry %lu mark %lu]\",\n"
      "\t\tAmFp_clear / d, AmFp_fps / d, AmFp_g[0] / d, AmFp_g[1] / d, AmFp_g[2] / d, AmFp_g[3] / d);\n"
      "\tfor (k = 0; k < 6 && AmFpName[k]; ++k)\n"
      "\t\tn += snprintf(b + n, sizeof b - n, \" | %s %lu\", AmFpName[k], AmFpUs[k] / d);\n"
      "\tif (Options::amigaPerfLog) SDLmini_Log(b);\n"
      "\tAmFp_clear = AmFp_fps = 0; for (k = 0; k < 4; ++k) AmFp_g[k] = 0;\n"
      "\tfor (k = 0; k < 6; ++k) { AmFpName[k] = 0; AmFpUs[k] = 0; }\n"
      "\tAmFp_n = 0;\n"
      "}\n"
      "\n"
      "void Game::run()\n")

# --- Globe.cpp: the globe surface and its three overlays -----------------------
patch("Geoscape/Globe.cpp",
      "\t_radars->blit(surface);\n\t_countries->blit(surface);\n\t_markers->blit(surface);\n}\n",
      "\t/* " + MARK + " */\n"
      "\t{ unsigned long g_ = amiga_uclock_us(); _radars->blit(surface); unsigned long h_ = amiga_uclock_us(); AmFp_g[1] += h_ - g_;\n"
      "\t  _countries->blit(surface); g_ = amiga_uclock_us(); AmFp_g[2] += g_ - h_;\n"
      "\t  _markers->blit(surface); AmFp_g[3] += amiga_uclock_us() - g_; }\n"
      "}\n")
patch("Geoscape/Globe.cpp",
      "void Globe::blit(Surface *surface)\n{\n\tSurface::blit(surface);\n",
      "extern unsigned long AmFp_g[4];   /* " + MARK + " */\n"
      "extern \"C\" unsigned long amiga_uclock_us(void);\n"
      "void Globe::blit(Surface *surface)\n{\n"
      "\t{ unsigned long g0_ = amiga_uclock_us(); Surface::blit(surface); AmFp_g[0] += amiga_uclock_us() - g0_; }\n")
