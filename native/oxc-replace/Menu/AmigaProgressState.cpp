/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "AmigaProgressState.h"
#include <cstring>
#include "../Engine/Game.h"
#include "../Engine/Surface.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "amiga_gfx.h"

extern "C" void SDLmini_ForceFullFrame(void);

namespace OpenXcom
{

/* The box, in screen pixels: the label on top, the figure and the spinner
 * (drawn by amiga_gfx.c) below it. */
static const int BOX_W = 128, BOX_H = 42;
static const int GLYPH_H = 9;
static const char GLYPHS[] = "0123456789%";

/**
 * Builds the box's window and label like the Options window's.
 * @param label What is going on ("Loading game").
 */
AmigaProgressState::AmigaProgressState(const std::wstring &label)
{
	_screen = false;

	// The background is taken from where the window would sit on a screen.
	_window = new Window(this, BOX_W, BOX_H, 96, 79, POPUP_NONE);
	_txtLabel = new Text(BOX_W - 8, GLYPH_H, 4, 9);

	setInterface("pauseMenu", false, _game->getSavedGame() ? _game->getSavedGame()->getSavedBattle() : 0);

	add(_window, "window", "pauseMenu");
	add(_txtLabel, "text", "pauseMenu");

	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));
	_window->setX(0);
	_window->setY(0);

	_txtLabel->setAlign(ALIGN_CENTER);
	_txtLabel->setText(label);
}

AmigaProgressState::~AmigaProgressState()
{
}

/* the colour of the palette nearest to r,g,b */
static int nearest(const SDL_Color *pal, int r, int g, int b)
{
	int best = 0;
	long bd = 0x7fffffffL;
	for (int i = 0; i < 256; ++i)
	{
		long dr = pal[i].r - r, dg = pal[i].g - g, db = pal[i].b - b;
		long d = dr * dr + dg * dg + db * db;
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

/* A capital letter for the label: ASCII, Latin-1 and the Latin Extended-A
 * letters Polish, Czech and the rest use (lower case odd or even by range). */
static wchar_t upper(wchar_t c)
{
	if (c >= L'a' && c <= L'z') return c - 32;
	if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
	if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177)) return (c & 1) ? c - 1 : c;
	if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? c : c - 1;
	return c;
}

/* the surface's pixels into buf (w x h, no pitch) */
static void grab(Surface *s, unsigned char *buf, int w, int h)
{
	SDL_Surface *ss = s->getSurface();
	for (int y = 0; y < h; ++y)
		memcpy(buf + y * w, (const Uint8 *)ss->pixels + y * ss->pitch, w);
}

void AmigaProgressState::begin(const std::wstring &label)
{
	static unsigned char img[BOX_W * BOX_H], rgb[256 * 3];
	static unsigned char glyphs[sizeof GLYPHS - 1][GLYPH_H * 8], widths[sizeof GLYPHS - 1];
	int bright = 0, dim = 0;
	try
	{
		std::wstring caps(label);
		for (size_t i = 0; i < caps.size(); ++i) caps[i] = upper(caps[i]);
		AmigaProgressState art(caps);
		Surface box(BOX_W, BOX_H), text(BOX_W, BOX_H);
		art._window->blit(&box);
		art._txtLabel->blit(&text);
		grab(&box, img, BOX_W, BOX_H);
		const SDL_Color *pal = art.getPalette();
		/* Over a battle (640x400, 640x480) the pauseMenu colours come out
		 * grey on the grey-violet window: the letters are repainted near
		 * white, their shading kept as steps of grey. */
		unsigned char lab[BOX_W * BOX_H], recol[256];
		grab(&text, lab, BOX_W, BOX_H);
		int base = 255;
		for (int i = 0; i < BOX_W * BOX_H; ++i)
			if (lab[i] && lab[i] < base) base = lab[i];
		bool battle = _game->getSavedGame() && _game->getSavedGame()->getSavedBattle();
		for (int i = 0; i < 256; ++i)
		{
			int step = i - base;
			int v = 255 - (step < 0 ? 0 : step) * 36;
			if (v < 120) v = 120;
			recol[i] = (unsigned char)(battle && i ? nearest(pal, v, v, v) : i);
		}
		for (int i = 0; i < BOX_W * BOX_H; ++i)
			if (lab[i]) img[i] = recol[lab[i]];
		for (int i = 0; i < 256; ++i)
		{
			rgb[i * 3 + 0] = pal[i].r;
			rgb[i * 3 + 1] = pal[i].g;
			rgb[i * 3 + 2] = pal[i].b;
		}
		// the spinner: one near-white dot turning among grey ones
		bright = nearest(pal, 255, 255, 255);
		dim = nearest(pal, 104, 104, 104);
		// the figure's characters, one at a time, in the label's font and colour
		for (int g = 0; g < (int)sizeof GLYPHS - 1; ++g)
		{
			wchar_t ch[2] = { (wchar_t)GLYPHS[g], 0 };
			art._txtLabel->setAlign(ALIGN_LEFT);
			art._txtLabel->setX(0);
			art._txtLabel->setY(0);
			art._txtLabel->setText(ch);
			Surface one(16, GLYPH_H);
			art._txtLabel->blit(&one);
			unsigned char cell[16 * GLYPH_H];
			grab(&one, cell, 16, GLYPH_H);
			int w = 0;
			for (int y = 0; y < GLYPH_H; ++y)
				for (int x = 0; x < 16; ++x)
					if (cell[y * 16 + x] && x + 1 > w) w = x + 1;
			if (w > 8) w = 8;
			widths[g] = (unsigned char)w;
			for (int y = 0; y < GLYPH_H; ++y)
				for (int x = 0; x < 8; ++x)
					glyphs[g][y * 8 + x] = recol[cell[y * 16 + x]];
		}
		amigagfx_progress_art(img, BOX_W, BOX_H, rgb, &glyphs[0][0], widths, GLYPH_H, bright, dim);
	}
	catch (...)
	{
		// no picture: amiga_gfx.c draws a plain box instead
	}
	char plain[24];   // for the plain box, should the picture have failed
	size_t n = 0;
	for (size_t i = 0; i < label.size() && n < sizeof plain - 1; ++i)
		plain[n++] = (label[i] >= 32 && label[i] < 127) ? (char)label[i] : '?';
	plain[n] = 0;
	amigagfx_progress_begin(plain);
}

void AmigaProgressState::end()
{
	amigagfx_progress_end();
	SDLmini_ForceFullFrame();   // the box was drawn behind the frame's back
}

}
