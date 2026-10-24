/*
 * AMIGA-PORT: the "Amiga" options tab (first tab of the options screen).
 *
 * A scrolling TextList in the style of the ADVANCED tab: one row per
 * option, left click cycles the value forward, right click backward, and
 * the row's description shows in the shared tooltip area at the bottom.
 * The strings are in bin/common/Language/en-US.yml (added by the patch
 * script). The interface colours reuse "videoMenu".
 *
 * At the top, not options: the Warp card's model, firmware and CPU clock,
 * as SYS:C/WarpTool reported them at startup (amigastartup_warpinfo), then
 * an empty row. NONE where WarpTool is missing or reported nothing.
 *
 * Rows:
 *   Map reveal                    Fast/Accurate/Test  Options::amigaAccurateFov
 *   Split movement calculation    Off / On            Options::amigaSplitWalk
 *   Music                         Off / Amiga / PC    Options::amigaMusic
 *   Music source                  Samples / AdLib     Options::amigaMusicSource
 *   Sound effects                 PC game / Amiga     Options::amigaSfx
 *   Language from Workbench       Off / On            Options::amigaLangAuto
 *   Show FPS                      Off / On            Options::fpsCounter
 *
 * The title bar (off: the 2x2 corner does its job), the pointer (Amiga),
 * the battle animation speed (normal), the music quality (nothing is mixed
 * live) and the display standard (Auto) are no longer here; Options::load
 * fixes them (2026-10-23, at the player's request). The loading screen
 * style went too (2026-10-24): with the intro playing while the game loads
 * the picture is rarely seen; Options::amigaSplashStyle keeps its value.
 */
#include "OptionsAmigaState.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Interface/TextList.h"
#include "../Interface/Text.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Engine/Language.h"
#include "../Interface/FpsCounter.h"
#include "amiga_startup.h"
#include <cctype>
#include <algorithm>

namespace OpenXcom
{

enum
{
	AMIGA_ROW_FOV = 0,
	AMIGA_ROW_SPLITWALK,
	AMIGA_ROW_MUSIC,
	AMIGA_ROW_MUSICSRC,
	AMIGA_ROW_SFX,
	AMIGA_ROW_LANGAUTO,
	AMIGA_ROW_FPS,
	AMIGA_ROW_COUNT
};

/* list rows above the options: card, firmware, CPU clock, an empty one */
static const size_t AMIGA_INFO_ROWS = 4;

static const char *amigaRowLabel_[AMIGA_ROW_COUNT] =
{
	"STR_AMIGA_FOV",
	"STR_AMIGA_SPLIT_WALK",
	"STR_AMIGA_MUSIC",
	"STR_AMIGA_MUSIC_SRC",
	"STR_AMIGA_SFX",
	"STR_AMIGA_LANG_AUTO",
	"STR_AMIGA_FPS"
};

static const char *amigaRowDesc_[AMIGA_ROW_COUNT] =
{
	"STR_AMIGA_FOV_DESC",
	"STR_AMIGA_SPLIT_WALK_DESC",
	"STR_AMIGA_MUSIC_DESC",
	"STR_AMIGA_MUSIC_SRC_DESC",
	"STR_AMIGA_SFX_DESC",
	"STR_AMIGA_LANG_AUTO_DESC",
	"STR_AMIGA_FPS_DESC"
};

/* how many values each row cycles through */
static const int amigaRowVals_[AMIGA_ROW_COUNT] = { 3, 2, 3, 2, 2, 2, 2 };

/* The source only chooses among the PC tunes. */
static bool amigaRowDisabled_(size_t row)
{
	return row == AMIGA_ROW_MUSICSRC && Options::amigaMusic != 2;
}

static int amigaRowGet_(size_t row)
{
	switch (row)
	{
	case AMIGA_ROW_FOV:       return Options::amigaAccurateFov;
	case AMIGA_ROW_SPLITWALK: return Options::amigaSplitWalk ? 1 : 0;
	case AMIGA_ROW_MUSIC:     return Options::amigaMusic;
	case AMIGA_ROW_MUSICSRC:  return Options::amigaMusicSource ? 1 : 0;
	case AMIGA_ROW_SFX:       return Options::amigaSfx ? 1 : 0;
	case AMIGA_ROW_LANGAUTO:  return Options::amigaLangAuto ? 1 : 0;
	case AMIGA_ROW_FPS:       return Options::fpsCounter ? 1 : 0;
	}
	return 0;
}

static void amigaRowSet_(size_t row, int v)
{
	switch (row)
	{
	case AMIGA_ROW_FOV:       Options::amigaAccurateFov = v; break;
	case AMIGA_ROW_SPLITWALK: Options::amigaSplitWalk = (v == 1); break;
	case AMIGA_ROW_MUSIC:     Options::amigaMusic = v; break;
	/* The tunes are converted at the next start; the ones already in
	 * user/music stay as they are until that folder is emptied. */
	case AMIGA_ROW_MUSICSRC:  Options::amigaMusicSource = v; break;
	/* The sounds are loaded while the game starts: the next start. */
	case AMIGA_ROW_SFX:       Options::amigaSfx = (v == 1); break;
	/* Takes effect at the next start: the language is chosen while the
	 * game is loading its data. */
	case AMIGA_ROW_LANGAUTO:  Options::amigaLangAuto = (v == 1); break;
	/* shown or hidden at once (lstOptionsClick) */
	case AMIGA_ROW_FPS:       Options::fpsCounter = (v == 1); break;
	}
}

static const char *amigaRowValue_(size_t row, int v)
{
	switch (row)
	{
	case AMIGA_ROW_FOV:
		return v == 2 ? "STR_AMIGA_FOV_TEST" : (v == 1 ? "STR_AMIGA_FOV_ACCURATE" : "STR_AMIGA_FOV_FAST");
	case AMIGA_ROW_MUSIC:
		return v == 2 ? "STR_AMIGA_MUSIC_PRE"
		     : (v == 1 ? "STR_AMIGA_MUSIC_LIVE" : "STR_AMIGA_MUSIC_OFF");
	case AMIGA_ROW_MUSICSRC:
		return v == 1 ? "STR_AMIGA_MUSIC_SRC_ADLIB" : "STR_AMIGA_MUSIC_SRC_SAMPLES";
	case AMIGA_ROW_SFX:
		return v == 1 ? "STR_AMIGA_MUSIC_LIVE" : "STR_AMIGA_MUSIC_PRE";   /* Amiga / PC game */
	default:
		return v == 1 ? "STR_AMIGA_ON" : "STR_AMIGA_OFF";
	}
}

/* WarpTool reports the firmware as 3054 for version 3.054 */
static std::string amigaFirmware_(const char *f)
{
	std::string s(f);
	if (s.empty()) return "NONE";
	if (s.size() == 4 && s.find_first_not_of("0123456789") == std::string::npos)
		s.insert(1, ".");
	return s;
}

OptionsAmigaState::OptionsAmigaState(OptionsOrigin origin) : OptionsBaseState(origin)
{
	setCategory(_btnAmiga);

	_lstOptions = new TextList(200, 136, 94, 8);
	add(_lstOptions, "optionLists", "advancedMenu");

	centerAllSurfaces();

	/* The value column is as wide as the widest value in this language (as
	 * the ADVANCED tab does for YES/NO); the labels wrap in what is left. */
	Text text = Text(200, 9, 0, 0);
	text.initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
	int rightcol = 0;
	for (size_t row = 0; row < AMIGA_ROW_COUNT; ++row)
	{
		for (int v = 0; v < amigaRowVals_[row]; ++v)
		{
			text.setText(tr(amigaRowValue_(row, v)));
			rightcol = std::max(rightcol, text.getTextWidth() + 4);
		}
	}
	rightcol = std::min(rightcol, 96);
	int leftcol = _lstOptions->getWidth() - rightcol;

	_lstOptions->setAlign(ALIGN_RIGHT, 1);
	_lstOptions->setColumns(2, leftcol, rightcol);
	_lstOptions->setWordWrap(true);
	_lstOptions->setSelectable(true);
	_lstOptions->setBackground(_window);
	_lstOptions->onMouseClick((ActionHandler)&OptionsAmigaState::lstOptionsClick, 0);
	_lstOptions->onMouseOver((ActionHandler)&OptionsAmigaState::lstOptionsMouseOver);
	_lstOptions->onMouseOut((ActionHandler)&OptionsAmigaState::lstOptionsMouseOut);

	{
		char model_[48], firmware_[48], clock_[48];
		amigastartup_warpinfo(model_, firmware_, clock_, sizeof model_);
		std::string card_;
		for (const char *c_ = model_; *c_; ++c_)
		{
			card_ += (char)toupper((unsigned char)*c_);
			if (c_ - model_ == 3 && card_ == "WARP")
				card_ += ' ';                  /* warp1260X -> WARP 1260X */
		}
		const std::wstring info_[3] =
		{
			L"CARD - " + Language::utf8ToWstr(card_.empty() ? "NONE" : card_),
			L"FIRMWARE - " + Language::utf8ToWstr(amigaFirmware_(firmware_)),
			L"CPU CLOCK - " + Language::utf8ToWstr(clock_[0] ? clock_ : "NONE")
		};
		for (int i_ = 0; i_ < 3; ++i_)
		{
			_lstOptions->addRow(2, info_[i_].c_str(), L"");
		}
		_lstOptions->addRow(2, L"", L"");
	}
	for (size_t row = 0; row < AMIGA_ROW_COUNT; ++row)
	{
		_lstOptions->addRow(2, tr(amigaRowLabel_[row]).c_str(),
			tr(amigaRowValue_(row, amigaRowGet_(row))).c_str());
		if (amigaRowDisabled_(row))
		{
			_lstOptions->setRowColor(AMIGA_INFO_ROWS + row, _lstOptions->getSecondaryColor());
		}
	}
}

OptionsAmigaState::~OptionsAmigaState()
{
}

void OptionsAmigaState::updateRow(size_t row)
{
	_lstOptions->setCellText(AMIGA_INFO_ROWS + row, 1, tr(amigaRowValue_(row, amigaRowGet_(row))));
	_lstOptions->setRowColor(AMIGA_INFO_ROWS + row, amigaRowDisabled_(row)
		? _lstOptions->getSecondaryColor() : _lstOptions->getColor());
}

void OptionsAmigaState::lstOptionsClick(Action *action)
{
	Uint8 button = action->getDetails()->button.button;
	if (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT)
	{
		return;
	}
	size_t row = _lstOptions->getSelectedRow();
	if (row < AMIGA_INFO_ROWS)
	{
		return;                                /* the card's report */
	}
	row -= AMIGA_INFO_ROWS;
	if (row >= AMIGA_ROW_COUNT)
	{
		return;
	}
	if (amigaRowDisabled_(row))
	{
		return;
	}
	const int n = amigaRowVals_[row];
	int v = amigaRowGet_(row);
	if (button == SDL_BUTTON_LEFT)
		v = (v + 1) % n;
	else
		v = (v + n - 1) % n;
	amigaRowSet_(row, v);
	updateRow(row);
	if (row == AMIGA_ROW_FPS)
	{
		_game->getFpsCounter()->setVisible(Options::fpsCounter);
	}
	if (row == AMIGA_ROW_MUSIC)
	{
		updateRow(AMIGA_ROW_MUSICSRC);   /* greys out unless the PC tunes play */
	}
}

void OptionsAmigaState::lstOptionsMouseOver(Action *)
{
	size_t row = _lstOptions->getSelectedRow();
	std::wstring desc;
	if (row >= AMIGA_INFO_ROWS && row - AMIGA_INFO_ROWS < AMIGA_ROW_COUNT)
	{
		desc = tr(amigaRowDesc_[row - AMIGA_INFO_ROWS]);
	}
	_txtTooltip->setText(desc);
}

void OptionsAmigaState::lstOptionsMouseOut(Action *)
{
	_txtTooltip->setText(L"");
}

}
