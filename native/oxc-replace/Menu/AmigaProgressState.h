#pragma once
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
#include <string>
#include "../Engine/State.h"

namespace OpenXcom
{

class Window;
class Text;

/**
 * AMIGA-PORT: the picture of the progress box shown while a game is saved or
 * loaded. Never pushed: it builds a small window in the style of the Options
 * window (its frame, background and the buttons' font) and hands the pixels,
 * the palette and the digits to amiga_gfx.c, whose own process draws the box
 * while the game is busy.
 */
class AmigaProgressState : public State
{
private:
	Window *_window;
	Text *_txtLabel;
public:
	AmigaProgressState(const std::wstring &label);
	~AmigaProgressState();
	/// Builds the box and starts showing it.
	static void begin(const std::wstring &label);
	/// Takes the box down; the caller's next frame is drawn in full.
	static void end();
};

}
