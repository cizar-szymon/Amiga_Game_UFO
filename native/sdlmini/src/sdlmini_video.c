/*
 * SDLmini - surfaces, blitting and the video "mode".
 *
 * The whole port is 8 bits per pixel by design: 320x200x8 is what X-COM is,
 * what AGA can push through a c2p at a playable rate, and what an 8-bit RTG
 * screen takes without conversion. The 32bpp paths OpenXcom carries for HQX
 * and OpenGL are removed from the game tree, not emulated here - a 32bpp blit
 * that silently half-works would be worse than a link error.
 *
 * The one exception is surface ALLOCATION: screenshots and the debug map
 * dumper allocate 24bpp surfaces and write their pixels by hand without ever
 * blitting them. Those are allowed, and only the blitters refuse anything that
 * is not 8 -> 8.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "sdlmini.h"
#include "amiga_gfx.h"
#include "amiga_copy.h"
#include "amiga_watchdog.h"

static SDL_Surface *s_screen;

/* loading splash (native/amiga_splash.c) */
extern void AmigaSplash_Show(void);
extern int  AmigaSplash_Active(void);
extern void AmigaSplash_End(void);

int SDLmini_diag_armed; /* TEMP: set by the game after the globe blit */
unsigned long SDLmini_flips;  /* one per rendered frame (SDL_Flip) */
static int          s_backend = 0;            /* AMIGAGFX_BACKEND_AGA */
static int          s_video_ready;

void SDLmini_SetBackend(int backend) { s_backend = backend; }
int  SDLmini_GetBackend(void)        { return s_backend; }

SDL_Surface *SDLmini_ScreenSurface(void) { return s_screen; }

void SDLmini_VideoInit(void) { s_video_ready = 1; }

void SDLmini_VideoQuit(void)
{
	if (s_screen != NULL) {
		/* The screen surface never owns its pixels - they belong to
		 * amiga_gfx.c chunky buffer - so only the wrapper goes. */
		free(s_screen->format->palette->colors);
		free(s_screen->format->palette);
		free(s_screen->format);
		free(s_screen);
		s_screen = NULL;
	}
	if (s_video_ready) {
		amigagfx_close();
		s_video_ready = 0;
	}
}

/* ------------------------------------------------------------- pixel fmt -- */

static SDL_PixelFormat *alloc_format(int bpp, Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask)
{
	SDL_PixelFormat *fmt = (SDL_PixelFormat *)calloc(1, sizeof(SDL_PixelFormat));
	if (fmt == NULL) return NULL;

	fmt->BitsPerPixel  = (Uint8)bpp;
	fmt->BytesPerPixel = (Uint8)((bpp + 7) / 8);
	fmt->alpha         = SDL_ALPHA_OPAQUE;

	if (bpp == 8) {
		SDL_Palette *pal = (SDL_Palette *)calloc(1, sizeof(SDL_Palette));
		if (pal == NULL) { free(fmt); return NULL; }
		pal->ncolors = 256;
		pal->colors  = (SDL_Color *)calloc(256, sizeof(SDL_Color));
		if (pal->colors == NULL) { free(pal); free(fmt); return NULL; }
		fmt->palette = pal;
		return fmt;
	}

	/* Truecolour surfaces exist only to be written into and handed to
	 * lodepng, so the masks are recorded and nothing more is derived from
	 * them than the shifts those writers use. */
	fmt->Rmask = rmask; fmt->Gmask = gmask; fmt->Bmask = bmask; fmt->Amask = amask;
	if (rmask == 0x0000ffUL) { fmt->Rshift = 0;  fmt->Gshift = 8;  fmt->Bshift = 16; }
	else                     { fmt->Rshift = 16; fmt->Gshift = 8;  fmt->Bshift = 0;  }
	fmt->Ashift = 24;
	return fmt;
}

static void free_format(SDL_PixelFormat *fmt)
{
	if (fmt == NULL) return;
	if (fmt->palette != NULL) {
		free(fmt->palette->colors);
		free(fmt->palette);
	}
	free(fmt);
}

/* ------------------------------------------------------------- surfaces -- */

/* LISTA-ROBOT pkt 2, wariant C: per-surface span cache for colorkey blits.
 * Built once per surface CONTENT (rebuilt after any write), it lists for each
 * row the (offset, length) runs of NON-key pixels. A colorkey blit is then
 * pure memcpy per run - transparent pixels cost nothing at all, and the
 * per-pixel compares of variant A happen only once, at build time.
 * unused1: 0 unknown, 1 has key pixels but too noisy to cache (A path),
 * 2 fully opaque (memcpy path), 3 span cache attached (this). */
extern unsigned long SDLmini_ProfClassifyUs;
unsigned long amiga_uclock_us(void);
struct private_hwdata {
	Uint32 *rowoff;   /* [h] index into runs[] where the row starts */
	Uint16 *runs;     /* per row: count n, then n * (offset, length) */
};

/* unchanged-frame skip, defined with the dirty rectangles further down */
static void skip_forget(SDL_Surface *s);
static void skip_reading(SDL_Surface *s);
static void skip_invalidate(void);

static void spans_free(SDL_Surface *s)
{
	if (s->hwdata != NULL) {
		free(s->hwdata->rowoff);
		free(s->hwdata->runs);
		free(s->hwdata);
		s->hwdata = NULL;
	}
}

/* Every write path calls this: content changed, classification is stale -
 * and, for the unchanged-frame skip, the content has a new generation. */
static void surf_touch(SDL_Surface *s)
{
	s->unused1 = 0;
	if (s->hwdata != NULL) spans_free(s);
	SDLmini_Written(s);
}

/* One pass over the pixels: classify, and build the span cache on the fly.
 * If the surface turns out noisy (more runs than pixels/8 - alternating
 * pixels, dithered fills), the cache is dropped and variant A handles it. */
static Uint32 surf_classify(SDL_Surface *s)
{
	Uint8  key = (Uint8)(s->format->colorkey & 0xff);
	Uint32 maxpairs = (Uint32)s->w * (Uint32)s->h / 8U + (Uint32)s->h + 16U;
	Uint32 cap = (Uint32)s->h * 4U + 64U, idx = 0;
	int y, sawkey = 0;
	Uint32 *rowoff = (Uint32 *)malloc((size_t)s->h * sizeof(Uint32));
	Uint16 *runs   = (Uint16 *)malloc((size_t)cap * sizeof(Uint16));

	if (rowoff == NULL || runs == NULL) {
		free(rowoff); free(runs);
		return 1;                       /* no memory: A path, still correct */
	}
	for (y = 0; y < s->h; y++) {
		const Uint8 *p = (const Uint8 *)s->pixels + (size_t)y * s->pitch;
		Uint32 nidx;
		Uint16 n = 0;
		int x = 0;

		if (idx + 1 + 2U * ((Uint32)s->w / 2U + 1U) > cap) {
			Uint16 *grown;
			cap = cap * 2U + (Uint32)s->w;
			grown = (Uint16 *)realloc(runs, (size_t)cap * sizeof(Uint16));
			if (grown == NULL) { free(rowoff); free(runs); return 1; }
			runs = grown;
		}
		rowoff[y] = idx;
		nidx = idx++;                   /* count written after the row */
		while (x < s->w) {
			int start;
			while (x < s->w && p[x] == key) { x++; sawkey = 1; }
			if (x >= s->w) break;
			start = x;
			while (x < s->w && p[x] != key) x++;
			runs[idx++] = (Uint16)start;
			runs[idx++] = (Uint16)(x - start);
			n++;
		}
		runs[nidx] = n;
		if ((idx / 2U) > maxpairs) {    /* too noisy - not worth caching */
			free(rowoff); free(runs);
			return 1;
		}
	}
	if (!sawkey) { free(rowoff); free(runs); return 2; }

	s->hwdata = (struct private_hwdata *)malloc(sizeof(struct private_hwdata));
	if (s->hwdata == NULL) { free(rowoff); free(runs); return 1; }
	s->hwdata->rowoff = rowoff;
	s->hwdata->runs   = runs;
	return 3;
}

static SDL_Surface *new_surface(int w, int h, int bpp, int pitch,
                                Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask,
                                void *pixels)
{
	SDL_Surface *s = (SDL_Surface *)calloc(1, sizeof(SDL_Surface));
	if (s == NULL) { SDL_SetError("out of memory (surface header)"); return NULL; }

	s->format = alloc_format(bpp, rmask, gmask, bmask, amask);
	if (s->format == NULL) { free(s); SDL_SetError("out of memory (pixel format)"); return NULL; }

	s->w        = w;
	s->h        = h;
	s->pitch    = (Uint16)pitch;
	s->refcount = 1;
	s->clip_rect.x = 0;
	s->clip_rect.y = 0;
	s->clip_rect.w = (Uint16)w;
	s->clip_rect.h = (Uint16)h;
	s->amgen = ++SDLmini_Gen;

	if (pixels != NULL) {
		s->pixels = pixels;
		s->flags |= SDL_PREALLOC;
	} else {
		s->pixels = calloc(1, (size_t)pitch * (size_t)h);
		if (s->pixels == NULL) {
			free_format(s->format);
			free(s);
			SDL_SetError("out of memory (%d x %d x %dbpp surface)", w, h, bpp);
			return NULL;
		}
	}
	return s;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
	int bpp   = (depth + 7) / 8;
	int pitch = (width * bpp + 3) & ~3;         /* SDL aligns rows to 4 bytes */
	SDL_Surface *s = new_surface(width, height, depth, pitch, Rmask, Gmask, Bmask, Amask, NULL);
	if (s != NULL) s->flags |= (flags & SDL_SRCCOLORKEY);
	return s;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch,
                                      Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
	return new_surface(width, height, depth, pitch, Rmask, Gmask, Bmask, Amask, pixels);
}

void SDL_FreeSurface(SDL_Surface *surface)
{
	if (surface == NULL) return;
	if (surface == s_screen) return;            /* owned by SDLmini_VideoQuit */
	if (--surface->refcount > 0) return;
	skip_forget(surface);                       /* queued blits must not read freed pixels */
	spans_free(surface);
	if (!(surface->flags & SDL_PREALLOC)) free(surface->pixels);
	free_format(surface->format);
	free(surface);
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt, Uint32 flags)
{
	SDL_Surface *dst;

	if (src == NULL) return NULL;
	skip_reading(src);   /* the copy below reads the pixels directly */
	if (fmt->BitsPerPixel != src->format->BitsPerPixel) {
		SDL_SetError("SDLmini: conversion between depths is not supported (%d -> %d)",
		             src->format->BitsPerPixel, fmt->BitsPerPixel);
		return NULL;
	}

	dst = SDL_CreateRGBSurface(flags, src->w, src->h, src->format->BitsPerPixel,
	                           fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
	if (dst == NULL) return NULL;

	{
		int y;
		int bytes = src->w * src->format->BytesPerPixel;
		for (y = 0; y < src->h; y++) {
			memcpy((Uint8 *)dst->pixels + (size_t)y * dst->pitch,
			       (Uint8 *)src->pixels + (size_t)y * src->pitch, (size_t)bytes);
		}
	}
	if (src->format->palette != NULL && dst->format->palette != NULL) {
		memcpy(dst->format->palette->colors, src->format->palette->colors,
		       (size_t)src->format->palette->ncolors * sizeof(SDL_Color));
		dst->format->palette->ncolors = src->format->palette->ncolors;
	}
	dst->format->colorkey = src->format->colorkey;
	dst->flags |= (src->flags & SDL_SRCCOLORKEY);
	return dst;
}


/* One-shot markers. The game crashed on its first drawn frame with a bus
 * error and the log stopped at "started successfully", so every entry point
 * the first frame goes through says so exactly once. Cheap enough to leave in
 * until the port is reliable: one comparison per call. */
#define SDLMINI_FIRST(tag) do { 	static int once_; 	if (!once_) { once_ = 1; SDLmini_Log("SDLmini: first " tag); } } while (0)

/* A lock means direct pixel writes we cannot see - whole screen dirty.
 * (Definitions live in the dirty-rectangle block further down.) */
static void dirty_add(int x, int y, int w, int h);
static void dirty_add_lock(SDL_Surface *s);
int SDL_LockSurface(SDL_Surface *surface)    { if (surface != NULL) { surf_touch(surface); dirty_add_lock(surface); } return 0; }
void SDL_UnlockSurface(SDL_Surface *surface) { if (surface != NULL) { surf_touch(surface); dirty_add_lock(surface); } }

int SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key)
{
	if (surface == NULL) return -1;
	{
		static int logged_;
		if (logged_ < 6) {
			char b[128];
			snprintf(b, sizeof b, "SDLmini: SetColorKey(%dx%d, flag %lx, key %lu) flags before %lx", surface->w, surface->h, (unsigned long)flag, (unsigned long)key, (unsigned long)surface->flags);
			SDLmini_Log(b);
			logged_++;
		}
	}
	surf_touch(surface);   /* key changed: reclassify */
	if (flag & SDL_SRCCOLORKEY) {
		surface->flags |= SDL_SRCCOLORKEY;
		surface->format->colorkey = key;
	} else {
		surface->flags &= ~SDL_SRCCOLORKEY;
		surface->format->colorkey = 0;
	}
	return 0;
}

SDL_bool SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect)
{
	SDL_Rect full;
	if (surface == NULL) return SDL_FALSE;

	full.x = 0; full.y = 0;
	full.w = (Uint16)surface->w; full.h = (Uint16)surface->h;
	if (rect == NULL) {
		surface->clip_rect = full;
		return SDL_TRUE;
	}
	{
		int x1 = rect->x, y1 = rect->y;
		int x2 = rect->x + rect->w, y2 = rect->y + rect->h;
		if (x1 < 0) x1 = 0;
		if (y1 < 0) y1 = 0;
		if (x2 > surface->w) x2 = surface->w;
		if (y2 > surface->h) y2 = surface->h;
		if (x2 <= x1 || y2 <= y1) {
			surface->clip_rect.x = surface->clip_rect.y = 0;
			surface->clip_rect.w = surface->clip_rect.h = 0;
			return SDL_FALSE;
		}
		surface->clip_rect.x = (Sint16)x1;
		surface->clip_rect.y = (Sint16)y1;
		surface->clip_rect.w = (Uint16)(x2 - x1);
		surface->clip_rect.h = (Uint16)(y2 - y1);
	}
	return SDL_TRUE;
}

void SDL_GetClipRect(SDL_Surface *surface, SDL_Rect *rect)
{
	if (surface != NULL && rect != NULL) *rect = surface->clip_rect;
}

/* ------------------------------------------------------------- palettes -- */

static unsigned char s_pal[256 * 3]; /* last palette, replayed after a reopen */
static int s_pal_valid = 0;

static void push_palette_to_screen(const SDL_Color *colors, int first, int n)
{
	unsigned char rgb[256 * 3];
	int i;
	if (n > 256) n = 256;
	for (i = 0; i < n; i++) {
		rgb[i * 3 + 0] = colors[i].r;
		rgb[i * 3 + 1] = colors[i].g;
		rgb[i * 3 + 2] = colors[i].b;
	}

	/* Report the first few full-palette loads. A ramp test proved the path
	 * from here to the AGA colour registers is exact, so when the screen
	 * comes up in the wrong hues the question is what the GAME asked for -
	 * and that is only answerable by printing it. Capped, because a full
	 * load happens on every screen change. */
	if (first == 0 && n >= 256) {
		static int dumps;
		if (dumps < 3) {
			char line[160];
			int k;
			dumps++;
			SDLmini_Log("SDLmini: full palette set, sampling it:");
			for (k = 0; k < 256; k += 32) {
				snprintf(line, sizeof(line),
				         "  [%3ld] %3ld,%3ld,%3ld   [%3ld] %3ld,%3ld,%3ld",
				         (long)k,
				         (long)rgb[k*3], (long)rgb[k*3+1], (long)rgb[k*3+2],
				         (long)(k + 16),
				         (long)rgb[(k+16)*3], (long)rgb[(k+16)*3+1], (long)rgb[(k+16)*3+2]);
				SDLmini_Log(line);
			}
		}
	}

	if (first >= 0 && first + n <= 256) {
		memcpy(s_pal + first * 3, rgb, (size_t)n * 3);
		s_pal_valid = 1;
	}
	amigagfx_set_palette(rgb, first, n);
}

int SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors)
{
	SDLMINI_FIRST("SDL_SetColors");
	SDL_Palette *pal;

	if (surface == NULL || surface->format->palette == NULL) return 0;
	pal = surface->format->palette;
	if (firstcolor < 0 || firstcolor + ncolors > pal->ncolors) return 0;

	memcpy(&pal->colors[firstcolor], colors, (size_t)ncolors * sizeof(SDL_Color));
	if (surface == s_screen && !AmigaSplash_Active()) {
		push_palette_to_screen(colors, firstcolor, ncolors);
		/* Truecolour RTG converts pixels through the palette at blit time,
		 * so a palette change must reconvert everything on screen. */
		dirty_add(0, 0, surface->w, surface->h);
	}
	return 1;
}

/* Called by the game (StartState) when loading is done: fade the splash to
 * black, then hand the display back - push the palette the game set while it
 * was suppressed and reconvert the whole frame. */
void SDLmini_SplashFinish(void)
{
	AmigaSplash_End();
	amiga_watchdog_start();   /* loading is over: from here a long silence is news */
	if (s_screen != NULL && s_screen->format->palette != NULL) {
		SDL_SetColors(s_screen, s_screen->format->palette->colors, 0,
		              s_screen->format->palette->ncolors);
		dirty_add(0, 0, s_screen->w, s_screen->h);
	}
	skip_invalidate();   /* the splash was drawn straight into the screen */
}

int SDL_SetPalette(SDL_Surface *surface, int flags, SDL_Color *colors, int firstcolor, int ncolors)
{
	(void)flags;
	return SDL_SetColors(surface, colors, firstcolor, ncolors);
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b)
{
	if (format->palette == NULL) {
		return ((Uint32)r << format->Rshift) | ((Uint32)g << format->Gshift) | ((Uint32)b << format->Bshift);
	}
	{
		/* Nearest match in the palette. Called a handful of times per run,
		 * never per pixel, so a linear scan is the right amount of code. */
		int i, best = 0;
		long bestd = 0x7fffffffL;
		for (i = 0; i < format->palette->ncolors; i++) {
			long dr = (long)format->palette->colors[i].r - r;
			long dg = (long)format->palette->colors[i].g - g;
			long db = (long)format->palette->colors[i].b - b;
			long d  = dr * dr + dg * dg + db * db;
			if (d < bestd) { bestd = d; best = i; if (d == 0) break; }
		}
		return (Uint32)best;
	}
}

void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *format, Uint8 *r, Uint8 *g, Uint8 *b)
{
	if (format->palette != NULL) {
		int i = (int)(pixel & 0xff);
		*r = format->palette->colors[i].r;
		*g = format->palette->colors[i].g;
		*b = format->palette->colors[i].b;
	} else {
		*r = (Uint8)((pixel >> format->Rshift) & 0xff);
		*g = (Uint8)((pixel >> format->Gshift) & 0xff);
		*b = (Uint8)((pixel >> format->Bshift) & 0xff);
	}
}

void SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *format, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
	SDL_GetRGB(pixel, format, r, g, b);
	*a = SDL_ALPHA_OPAQUE;
}

/* --------------------------------------------------------------- blits -- */

/* TEMP perf counters (LISTA-ROBOT pkt 1): exact work counts per 100 frames,
 * logged from SDL_Flip. Pixel counts are exact; times use the 20 ms tick and
 * only mean anything summed over many frames. */
static unsigned long s_perf_blits, s_perf_blits_ck;      /* calls */
static unsigned long s_perf_px, s_perf_px_ck;            /* pixels copied */
static unsigned long s_perf_fills, s_perf_fill_px;
static unsigned long s_perf_c2p_ms, s_perf_last_ms;
static unsigned long s_perf_c2p_px, s_perf_skips;        /* dirty-rect effect */
/* TEMP us-probes (2026-08-19): split the per-frame blit cost */
#include "amiga_uclock.h"
unsigned long SDLmini_ProfBlitBigUs = 0, SDLmini_ProfBlitBigN = 0, SDLmini_ProfBlitSmallUs = 0, SDLmini_ProfBlitSmallN = 0, SDLmini_ProfClassifyUs = 0, SDLmini_ProfFillUs = 0;

/* -------------------------------------------------- dirty rectangles ------
 * LISTA-ROBOT pkt 1. s_screen->pixels IS amiga_gfx's chunky buffer, so the
 * planar bitplanes (AGA) / display bitmap (RTG) keep showing the OLD content
 * of any region SDL_Flip does not convert - skipping c2p on unchanged pixels
 * is therefore safe by construction.
 *
 * The game (Screen::flip) blits its whole 320x200 back buffer onto s_screen
 * every frame, changed or not, so simply recording blit rectangles would mark
 * the full screen every time. Instead the plain full blit onto s_screen runs
 * as a DIFF-copy: compare in 32-byte cells (the c2p x grid), copy only cells
 * that differ, and grow the dirty union only around them. A clean frame costs
 * one read pass over both buffers and no copy, no c2p, no flip work at all.
 * All other writes to s_screen (fills, colorkey blits, locks) dirty their
 * rectangle the ordinary way. One union rectangle, not a list: the usual
 * changes (FPS counter, blink markers) cluster, and c2p_rect snaps x to 32
 * anyway. */
static int s_dirty_x0, s_dirty_y0, s_dirty_x1, s_dirty_y1; /* x1/y1 exclusive; empty when x1<=x0 */

static void dirty_add(int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0) return;
	if (s_dirty_x1 <= s_dirty_x0) {
		s_dirty_x0 = x; s_dirty_y0 = y;
		s_dirty_x1 = x + w; s_dirty_y1 = y + h;
		return;
	}
	if (x < s_dirty_x0) s_dirty_x0 = x;
	if (y < s_dirty_y0) s_dirty_y0 = y;
	if (x + w > s_dirty_x1) s_dirty_x1 = x + w;
	if (y + h > s_dirty_y1) s_dirty_y1 = y + h;
}

static void dirty_add_lock(SDL_Surface *s)
{
	if (s == s_screen) { dirty_add(0, 0, s->w, s->h); skip_invalidate(); }
}

/* --------------------------------------------- unchanged-frame skip ------
 * The game rebuilds its whole back buffer every frame - Game::run clears it
 * and every state blits all of its surfaces again - and Screen::flip then
 * diff-copies it onto the screen. When nothing moves (the Geoscape at rest,
 * a menu, the battlescape between two animation phases) all of that makes the
 * same picture again. Measured on a Warp 060 at 640x400, Geoscape at rest:
 * clear 4.3 ms, globe and its layers 11.8 ms, the rest of the state ~18 ms,
 * flip 7-9 ms - every frame, for a picture that did not change.
 *
 * So fills and blits onto the back buffer are not done when asked for: each
 * is queued as (source surface, its content generation, rectangles, colour
 * key or fill colour). When Screen::flip reads the back buffer, the queue is
 * compared with the previous frame's. The same list means the same picture,
 * which the back buffer and the screen already hold: the queue, the diff-copy
 * and the flip work are all skipped. Otherwise the queue runs in order and
 * everything goes on as before.
 *
 * That is exact only if every write to the pixels of every surface gives it a
 * new generation (SDLmini_Written: the SDL calls and lock/unlock here, and in
 * the game's Surface code setPixel, the draw methods and the direct-write
 * fast paths). Whatever cannot be compared - a write to the back buffer that
 * is not a queued fill or blit, a write to a surface whose blit is still
 * queued, a read of the back buffer, a full queue - runs the queue at once and
 * marks the frame "forced": it is drawn normally and is not the reference for
 * the next one.
 *
 * Options::amigaFrameSkip: 0 off, 1 on, 2 verify - a frame judged unchanged is
 * drawn anyway, and a checksum of the back buffer before and after says
 * whether the judgement was right ("fskip: VERIFY MISMATCH" when it was not). */
int SDLmini_FrameSkip = 1;
Uint32 SDLmini_Gen = 0;
Uint32 SDLmini_QueueSerial = 1;

typedef struct {
	SDL_Surface *src;   /* NULL = fill */
	Uint32 gen;         /* src->amgen when queued */
	Uint32 key;         /* bit 31 + colour key of a keyed blit; fill colour */
	SDL_Rect sr, dr;    /* already clipped */
} SkipOp;

#define SKIP_QMAX 512
static SkipOp s_q[SKIP_QMAX], s_qprev[SKIP_QMAX];
static int s_qn;               /* queued this frame */
static int s_qprevn = -1;      /* the reference frame's count; -1 = there is none */
static int s_qforced;          /* this frame cannot be compared */
static int s_qflushing;
static SDL_Surface *s_back;    /* the game's back buffer, recognised at the flip */
static unsigned long s_perf_fskip, s_perf_fverr;

static int SDL_FillRect_(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
static void blit8(SDL_Surface *src, const SDL_Rect *srcrect,
                  SDL_Surface *dst, const SDL_Rect *dstrect);

/* Fill + same-rectangle blit, elided. Every frame starts with Screen::clear
 * (fill with 0) followed at once by a blit of a full-screen surface - the
 * battlescape Map, the Geoscape background - whose colour key is 0 too. The
 * transparent pixels would show the fill's 0, which is exactly what a plain
 * copy of those pixels gives: so the fill is dropped and the blit done as a
 * plain copy. One full pass over the frame less (a 640x400 fill is ~4 ms on a
 * Warp 060), and no per-pixel key tests. Only when the blit's rectangle is
 * the fill's exactly, so no pixel of the fill survives anywhere else. */
static int s_blit_plain;   /* blit8: treat the source as having no colour key */
static int s_noelide;      /* verify mode: build the reference frame without it */

static int fill_elided_by_next(int i)
{
	const SkipOp *f = &s_q[i], *b;
	if (s_noelide || f->src != NULL || i + 1 >= s_qn) return 0;
	b = &s_q[i + 1];
	if (b->src == NULL || b->src == s_back) return 0;
	if (b->dr.x != f->dr.x || b->dr.y != f->dr.y || b->dr.w != f->dr.w || b->dr.h != f->dr.h) return 0;
	return b->key == 0 || b->key == (0x80000000UL | (f->key & 0xff));
}

/* ---- direct frames ---------------------------------------------------------
 * While much of the picture changes every frame - the battlescape scrolling,
 * 25-85% of the diff-copy's cells at 640x400 depending on how much of the map
 * is still black - composing into the back buffer and diff-copying it onto the
 * screen cost ~7 + ~5 ms on a Warp 060, to save sending the unchanged part to
 * the display (the whole screen: ~4.5 ms). After a diff-copy that found more
 * than a tenth of the cells changed, the next 7 frames run the queue straight
 * onto the screen instead (s_direct_run), and the whole screen goes to the
 * display. That needs a queue that paints
 * every pixel itself - it starts with a fill of the whole back buffer, as
 * every frame does (Game::run clears it) - and leaves the back buffer behind:
 * s_back_stale. Whatever uses it before the next full run gets the screen's
 * picture copied back first (back_restore): a flush with nothing queued (a
 * direct write to it or a read), the screen being written behind the
 * diff-copy's back, a new video mode. A partial frame runs the whole queue
 * clipped to its rectangles, so those come out right in a stale buffer too.
 * Verify mode (amigaFrameSkip 2) also runs the queue onto the back buffer
 * and compares the two ("fskip: VERIFY MISMATCH ... direct"). */
static int s_back_stale;
static unsigned long s_perf_fdirect;
static int s_diff_whole;   /* flips left that copy everything without comparing (blit8) */
static int s_direct_run;   /* direct frames left before the next compared one */

/* After a VRAM layer frame (below) the screen buffer is behind the display as
 * well: whatever needs it gets the display read back first (~5 ms at
 * 640x400 on a Warp 060). */
static int s_screen_stale;

static void screen_restore(void)
{
	if (!s_screen_stale) return;
	s_screen_stale = 0;
	if (s_screen == NULL) return;
	if (!amigagfx_display_read((unsigned char *)s_screen->pixels, s_screen->pitch)) {
		/* unreadable: the next frame is drawn and sent in full */
		dirty_add(0, 0, s_screen->w, s_screen->h);
		s_qprevn = -1;
	}
}

static void back_restore(void)
{
	if (!s_back_stale) return;
	screen_restore();
	s_back_stale = 0;
	if (s_back == NULL || s_screen == NULL || s_back->w != s_screen->w || s_back->h != s_screen->h) return;
	amiga_copy_rows(s_back->pixels, s_back->pitch, s_screen->pixels, s_screen->pitch, s_back->w, s_back->h);
}

/* does the queue begin by painting all of the back buffer? */
static int queue_paints_all(void)
{
	return s_qn > 0 && s_q[0].src == NULL && s_q[0].dr.x == 0 && s_q[0].dr.y == 0 &&
	       s_q[0].dr.w == s_back->w && s_q[0].dr.h == s_back->h;
}

static void skip_run_ops(void);
static void skip_run(void)
{
	if (s_back_stale) {
		if (queue_paints_all()) s_back_stale = 0;   /* repaints all of it */
		else back_restore();
	}
	skip_run_ops();
}

static void skip_run_ops(void)
{
	int i;
	s_qflushing = 1;
	for (i = 0; i < s_qn; i++) {
		SkipOp *o = &s_q[i];
		if (o->src == NULL) {
			if (fill_elided_by_next(i)) {
				SkipOp *b = &s_q[++i];
				s_blit_plain = 1;
				blit8(b->src, &b->sr, s_back, &b->dr);
				s_blit_plain = 0;
			} else {
				SDL_FillRect_(s_back, &o->dr, o->key);
			}
		}
		else blit8(o->src, &o->sr, s_back, &o->dr);
	}
	s_qn = 0;
	s_qflushing = 0;
}

void SDLmini_FlushQueue(void)
{
	if (s_qflushing) return;
	s_qforced = 1;
	if (s_qn > 0) skip_run();
	else back_restore();   /* nothing queued: whoever flushes wants the buffer as shown */
}

static int skip_queueing(SDL_Surface *dst)
{
	return dst == s_back && s_back != NULL && SDLmini_FrameSkip != 0 && !s_qflushing;
}

static void skip_push(SDL_Surface *src, Uint32 key, const SDL_Rect *sr, const SDL_Rect *dr)
{
	SkipOp *o;
	if (s_qn == SKIP_QMAX) SDLmini_FlushQueue();   /* full: run it, frame not comparable */
	o = &s_q[s_qn++];
	memset(o, 0, sizeof(*o));                      /* the lists are compared with memcmp */
	o->src = src;
	o->key = key;
	o->dr  = *dr;
	if (src != NULL) {
		o->gen = src->amgen;
		o->sr  = *sr;
		src->amqueued = SDLmini_QueueSerial;
	}
}

/* Something is about to read a surface's pixels outside the queue. */
static void skip_reading(SDL_Surface *s)
{
	if (s == s_back) SDLmini_FlushQueue();
}

/* A surface is going away. */
static void skip_forget(SDL_Surface *s)
{
	if (s == s_back) {
		/* The back buffer itself goes (a new screen size): nothing queued for
		 * it or owed to it matters any more. Running the queue or putting the
		 * screen's picture back into it wrote 64000 bytes into memory the
		 * game had already freed - its Surface frees the pixels first - over
		 * the heap: the battle's tiles were damaged when a mission started
		 * after the 320x200 Geoscape had used direct frames (save assert
		 * "value < 65536", a looping particle list, "corrupt memory list in
		 * FreeMem"; found with amiga_heapguard.c, 2026-10-20). */
		s_qn = 0;
		s_qforced = 1;
		s_back = NULL;
		s_qprevn = -1;
		s_back_stale = 0;
		return;
	}
	if (s->amqueued == SDLmini_QueueSerial) SDLmini_FlushQueue();
}

/* The screen was written behind the diff-copy's back (splash, a lock, a fill):
 * the next frame must be drawn and diff-copied in full. Called before the
 * write, so a stale back buffer can still take the picture from the screen. */
static void skip_invalidate(void)
{
	back_restore();
	screen_restore();
	s_qprevn = -1;
}

static void skip_next_frame(void)
{
	SDLmini_QueueSerial++;
	s_qforced = 0;
	if (s_back != NULL) s_back->amqueued = SDLmini_QueueSerial;   /* direct writes to it flush */
}

/* ---- partial frames: only the rectangles whose content changed ------------
 * A frame whose list has the same SHAPE as the last one - same sources, keys,
 * colours and rectangles, in the same order - but in which some sources have
 * new content (the Geoscape clock, a blinking marker layer) differs from the
 * last picture only inside the destination rectangles of those operations.
 * Everything outside them is already right in the back buffer and on the
 * screen. So the whole list runs again, in order, clipped to each changed
 * rectangle, and only those rectangles are copied to the screen. Measured
 * before this: on the Geoscape with time running the clock changes almost
 * every frame, only 2-13% of the frames were unchanged, and composing the
 * whole 640x400 screen cost ~45 of the ~60 ms of a frame on a Warp 060. */
#define SKIP_RMAX 8
static SDL_Rect s_dmg[SKIP_RMAX];
static int s_dmgn;
static Uint8 *s_verify;                /* verify mode: the partial result */
static size_t s_verifysz;

static int verify_buf(void)
{
	size_t sz = (size_t)s_back->pitch * (size_t)s_back->h;
	if (s_verifysz < sz) { free(s_verify); s_verify = (Uint8 *)malloc(sz); s_verifysz = s_verify ? sz : 0; }
	return s_verify != NULL;
}
static unsigned long s_perf_fpart, s_perf_fpartpx;

static int ops_same_shape(const SkipOp *a, const SkipOp *b)
{
	return a->src == b->src && a->key == b->key &&
	       a->sr.x == b->sr.x && a->sr.y == b->sr.y && a->sr.w == b->sr.w && a->sr.h == b->sr.h &&
	       a->dr.x == b->dr.x && a->dr.y == b->dr.y && a->dr.w == b->dr.w && a->dr.h == b->dr.h;
}

static int rects_touch(const SDL_Rect *a, const SDL_Rect *b)
{
	return a->x <= b->x + b->w && b->x <= a->x + a->w && a->y <= b->y + b->h && b->y <= a->y + a->h;
}

static void rect_union(SDL_Rect *a, const SDL_Rect *b)
{
	int x0 = a->x < b->x ? a->x : b->x, y0 = a->y < b->y ? a->y : b->y;
	int x1 = a->x + a->w > b->x + b->w ? a->x + a->w : b->x + b->w;
	int y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;
	a->x = (Sint16)x0; a->y = (Sint16)y0; a->w = (Uint16)(x1 - x0); a->h = (Uint16)(y1 - y0);
}

/* Changed rectangles of this frame against the reference one: -1 when the
 * shapes differ (draw the whole frame), else their count (0 = unchanged). */
static int damage_collect(void)
{
	int i, k;
	s_dmgn = 0;
	for (i = 0; i < s_qn; i++) {
		SDL_Rect r;
		if (!ops_same_shape(&s_q[i], &s_qprev[i])) return -1;
		if (s_q[i].gen == s_qprev[i].gen) continue;
		r = s_q[i].dr;
		for (k = 0; k < s_dmgn; k++)                     /* merge into a touching one */
			if (rects_touch(&s_dmg[k], &r)) { rect_union(&s_dmg[k], &r); break; }
		if (k < s_dmgn) continue;
		if (s_dmgn < SKIP_RMAX) { s_dmg[s_dmgn++] = r; continue; }
		for (k = 1; k < s_dmgn; k++) rect_union(&s_dmg[0], &s_dmg[k]);   /* too many: one box */
		rect_union(&s_dmg[0], &r);
		s_dmgn = 1;
	}
	/* a merge can make rectangles touch that did not before: settle once */
	for (i = 0; i < s_dmgn; i++)
		for (k = i + 1; k < s_dmgn; k++)
			if (rects_touch(&s_dmg[i], &s_dmg[k])) {
				rect_union(&s_dmg[i], &s_dmg[k]);
				s_dmg[k--] = s_dmg[--s_dmgn];
			}
	return s_dmgn;
}

/* The whole queue, in order, clipped to r. */
static void skip_run_clipped(const SDL_Rect *r)
{
	int i;
	s_qflushing = 1;
	for (i = 0; i < s_qn; i++) {
		const SkipOp *o = &s_q[i];
		int plain = 0;
		int x0, y0, x1, y1;
		SDL_Rect d;
		if (o->src == NULL && fill_elided_by_next(i)) {   /* same rectangle: same clip */
			o = &s_q[++i];
			plain = 1;
		}
		x0 = o->dr.x > r->x ? o->dr.x : r->x;
		y0 = o->dr.y > r->y ? o->dr.y : r->y;
		x1 = o->dr.x + o->dr.w < r->x + r->w ? o->dr.x + o->dr.w : r->x + r->w;
		y1 = o->dr.y + o->dr.h < r->y + r->h ? o->dr.y + o->dr.h : r->y + r->h;
		if (x1 <= x0 || y1 <= y0) continue;
		d.x = (Sint16)x0; d.y = (Sint16)y0; d.w = (Uint16)(x1 - x0); d.h = (Uint16)(y1 - y0);
		if (o->src == NULL) {
			SDL_FillRect_(s_back, &d, o->key);
		} else {
			SDL_Rect s;
			s.x = (Sint16)(o->sr.x + (x0 - o->dr.x));
			s.y = (Sint16)(o->sr.y + (y0 - o->dr.y));
			s.w = d.w; s.h = d.h;
			s_blit_plain = plain;
			blit8(o->src, &s, s_back, &d);
			s_blit_plain = 0;
		}
	}
	s_qflushing = 0;
}

/* The changed rectangles of the back buffer straight onto the screen. */
static void damage_to_screen(void)
{
	int i;
	for (i = 0; i < s_dmgn; i++) {
		int x0 = s_dmg[i].x, y0 = s_dmg[i].y;
		int x1 = x0 + s_dmg[i].w, y1 = y0 + s_dmg[i].h;
		if (x0 < 0) x0 = 0;
		if (y0 < 0) y0 = 0;
		if (x1 > s_back->w) x1 = s_back->w;
		if (y1 > s_back->h) y1 = s_back->h;
		if (x1 > s_screen->w) x1 = s_screen->w;
		if (y1 > s_screen->h) y1 = s_screen->h;
		if (x1 <= x0 || y1 <= y0) continue;
		amiga_copy_rows((Uint8 *)s_screen->pixels + (size_t)y0 * s_screen->pitch + x0, s_screen->pitch,
		                (const Uint8 *)s_back->pixels + (size_t)y0 * s_back->pitch + x0, s_back->pitch,
		                x1 - x0, y1 - y0);
		dirty_add(x0, y0, x1 - x0, y1 - y0);
		s_perf_fpartpx += (unsigned long)(x1 - x0) * (unsigned long)(y1 - y0);
	}
}

/* checksum of the back buffer's picture - taken from the screen while the
 * back buffer is stale after a direct frame */
static Uint32 back_sum(void)
{
	const SDL_Surface *s = s_back_stale ? s_screen : s_back;
	Uint32 sum = 0;
	int x, y;
	for (y = 0; y < s_back->h; y++) {
		const Uint32 *p = (const Uint32 *)((const Uint8 *)s->pixels + (size_t)y * s->pitch);
		for (x = 0; x < s_back->w / 4; x++) sum = ((sum << 1) | (sum >> 31)) ^ p[x];
	}
	return sum;
}

/* ---- VRAM layer frames -----------------------------------------------------
 * The battlescape scrolling at 640x400 on a Warp 060, after the direct frames
 * above: the map picture onto the screen buffer ~9 ms, the screen buffer to
 * the display ~6 ms - for a picture that is the last one moved by a few
 * pixels. The display already shows that picture. So when the frame is
 *     fill of the whole screen + the layer surface over all of it (elided:
 *     a plain copy) + anything else (the HUD, the pointer),
 * the last frame was made the same way, and the layer reported every change
 * since (SDLmini_Layer*: moved by dx,dy; these rectangles redrawn), the
 * display is moved with the card's blitter (BltBitMap, ~0.5 ms), and only
 *     the strips the move exposed, the reported rectangles, and the last
 *     frame's other operations' rectangles moved along (the HUD pixels the
 *     blitter dragged with the map)
 * are copied from the layer into display memory; then the other operations
 * run straight onto the display. The screen and back buffers are left behind
 * (s_screen_stale, s_back_stale) and read back from the display when anything
 * needs them (screen_restore). Verify mode composes every such frame the
 * ordinary way as well and compares it with the display read back. */
int SDLmini_VramLayer = 1;
static SDL_Surface *s_layer;
static int s_layer_dx, s_layer_dy, s_layer_full = 1;
#define LAYER_RMAX 8
static SDL_Rect s_layer_dmg[LAYER_RMAX];
static int s_layer_ndmg;
static Uint32 s_layer_gen;
static unsigned long s_perf_flayer;

static void layer_reset(void)
{
	s_layer_dx = s_layer_dy = 0;
	s_layer_full = 0;
	s_layer_ndmg = 0;
}

static int layer_is(SDL_Surface *s)
{
	if (s != s_layer) {               /* a new layer: nothing known about it yet */
		s_layer = s;
		layer_reset();
		s_layer_full = 1;
		return 0;
	}
	return 1;
}

void SDLmini_LayerScroll(SDL_Surface *s, int dx, int dy)
{
	int i;
	if (!layer_is(s)) return;
	s_layer_dx += dx;
	s_layer_dy += dy;
	for (i = 0; i < s_layer_ndmg; i++) {      /* earlier damage moved with the picture */
		s_layer_dmg[i].x = (Sint16)(s_layer_dmg[i].x + dx);
		s_layer_dmg[i].y = (Sint16)(s_layer_dmg[i].y + dy);
	}
}

void SDLmini_LayerDamage(SDL_Surface *s, int x, int y, int w, int h)
{
	SDL_Rect r;
	if (!layer_is(s) || w <= 0 || h <= 0) return;
	r.x = (Sint16)x; r.y = (Sint16)y; r.w = (Uint16)w; r.h = (Uint16)h;
	if (s_layer_ndmg < LAYER_RMAX) { s_layer_dmg[s_layer_ndmg++] = r; return; }
	rect_union(&s_layer_dmg[LAYER_RMAX - 1], &r);
}

void SDLmini_LayerFull(SDL_Surface *s)
{
	layer_is(s);
	s_layer_full = 1;
}

void SDLmini_LayerDone(SDL_Surface *s)
{
	layer_is(s);
	s_layer_gen = s->amgen;
}

/* add r, clipped to w x h, to the repair list */
static int layer_add(SDL_Rect *list, int n, int max, int x, int y, int w, int h, int sw, int sh)
{
	int x1 = x + w, y1 = y + h;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x1 > sw) x1 = sw;
	if (y1 > sh) y1 = sh;
	if (x1 <= x || y1 <= y) return n;
	if (n == max) {   /* full: grow the last one */
		SDL_Rect r;
		r.x = (Sint16)x; r.y = (Sint16)y; r.w = (Uint16)(x1 - x); r.h = (Uint16)(y1 - y);
		rect_union(&list[max - 1], &r);
		return n;
	}
	list[n].x = (Sint16)x; list[n].y = (Sint16)y; list[n].w = (Uint16)(x1 - x); list[n].h = (Uint16)(y1 - y);
	return n + 1;
}

/* The frame end as a layer frame, if it can be one. Returns 1 when done. */
static int layer_frame(void)
{
	SDL_Surface *L;
	int w, h, dx, dy, i, n = 0;
	SDL_Rect rep[SKIP_QMAX / 4];
	unsigned char *base;
	long bpr;
	void *lock;
	SDL_Surface D;

	if (!SDLmini_VramLayer || s_layer == NULL || s_layer_full || s_qforced) return 0;
	if (s_qn < 2 || !queue_paints_all() || !fill_elided_by_next(0)) return 0;
	L = s_q[1].src;
	w = s_back->w; h = s_back->h;
	if (L != s_layer || L->amgen != s_layer_gen) return 0;   /* a change it did not report */
	if (s_q[1].sr.x != 0 || s_q[1].sr.y != 0 || s_q[1].sr.w != w || s_q[1].sr.h != h || L->w != w || L->h != h) return 0;
	if (s_screen == NULL || s_screen->w != w || s_screen->h != h || !amigagfx_display_layer_ok()) return 0;
	/* the display shows the last frame, made the same way */
	if (s_qprevn < 2 || s_qprev[0].src != NULL || s_qprev[0].key != s_q[0].key ||
	    !ops_same_shape(&s_qprev[1], &s_q[1])) return 0;
	dx = s_layer_dx; dy = s_layer_dy;
	if (dx >= w / 2 || -dx >= w / 2 || dy >= h / 2 || -dy >= h / 2) return 0;

	/* what has to come from the layer */
	if (dx > 0) n = layer_add(rep, n, SKIP_QMAX / 4, 0, 0, dx, h, w, h);
	if (dx < 0) n = layer_add(rep, n, SKIP_QMAX / 4, w + dx, 0, -dx, h, w, h);
	if (dy > 0) n = layer_add(rep, n, SKIP_QMAX / 4, 0, 0, w, dy, w, h);
	if (dy < 0) n = layer_add(rep, n, SKIP_QMAX / 4, 0, h + dy, w, -dy, w, h);
	for (i = 0; i < s_layer_ndmg; i++)
		n = layer_add(rep, n, SKIP_QMAX / 4, s_layer_dmg[i].x, s_layer_dmg[i].y, s_layer_dmg[i].w, s_layer_dmg[i].h, w, h);
	for (i = 2; i < s_qprevn; i++)
		n = layer_add(rep, n, SKIP_QMAX / 4, s_qprev[i].dr.x + dx, s_qprev[i].dr.y + dy, s_qprev[i].dr.w, s_qprev[i].dr.h, w, h);

	/* everything the copies below read by MOVE16 goes out of the cache
	 * first: no OS calls under the display lock */
	amiga_copy_push(L->pixels, L->pitch, w, h);
	for (i = 2; i < s_qn; i++)
		if (s_q[i].src != NULL)
			amiga_copy_push((const Uint8 *)s_q[i].src->pixels + (size_t)s_q[i].sr.y * s_q[i].src->pitch + s_q[i].sr.x,
			                s_q[i].src->pitch, s_q[i].sr.w, s_q[i].sr.h);

	if (dx != 0 || dy != 0)
		amigagfx_display_scroll(dx > 0 ? 0 : -dx, dy > 0 ? 0 : -dy, w - (dx > 0 ? dx : -dx), h - (dy > 0 ? dy : -dy), dx, dy);

	lock = amigagfx_display_lock(&base, &bpr);
	if (lock == NULL) {
		/* the display moved but cannot be written: send it all the usual way */
		amigagfx_display_cancel();
		dirty_add(0, 0, w, h);
		s_qprevn = -1;
		return 0;
	}
	/* ---- display locked: copies only ---- */
	amiga_copy_nopush(1);
	for (i = 0; i < n; i++)
		amiga_copy_rows(base + (size_t)rep[i].y * bpr + rep[i].x, bpr,
		                (const Uint8 *)L->pixels + (size_t)rep[i].y * L->pitch + rep[i].x, L->pitch,
		                rep[i].w, rep[i].h);
	memset(&D, 0, sizeof D);
	D.format = s_screen->format;
	D.w = w; D.h = h; D.pitch = (Uint16)bpr; D.pixels = base;
	D.clip_rect.w = (Uint16)w; D.clip_rect.h = (Uint16)h;
	s_qflushing = 1;
	for (i = 2; i < s_qn; i++) {
		SkipOp *o = &s_q[i];
		if (o->src == NULL) SDL_FillRect_(&D, &o->dr, o->key);
		else blit8(o->src, &o->sr, &D, &o->dr);
	}
	s_qflushing = 0;
	amiga_copy_nopush(0);
	amigagfx_display_unlock(lock);
	/* ---- unlocked ---- */
	amigagfx_display_present();   /* two buffers: show the finished frame */

	memcpy(s_qprev, s_q, (size_t)s_qn * sizeof(SkipOp));
	s_qprevn = s_qn;
	s_screen_stale = 1;
	s_back_stale = 1;
	s_perf_flayer++;
	if (SDLmini_FrameSkip == 2 && verify_buf()) {
		/* verify: the frame the ordinary way, against the display */
		int y;
		s_qn = s_qprevn;
		s_back_stale = 0;
		s_noelide = 1;
		skip_run_ops();
		s_noelide = 0;
		if (amigagfx_display_read(s_verify, s_back->pitch)) {
			long bad = 0;
			int x, bx0 = w, by0 = h, bx1 = -1, by1 = -1;
			for (y = 0; y < h; y++) {
				const Uint8 *a = s_verify + (size_t)y * s_back->pitch, *b2 = (const Uint8 *)s_back->pixels + (size_t)y * s_back->pitch;
				if (memcmp(a, b2, (size_t)w) == 0) continue;
				for (x = 0; x < w; x++)
					if (a[x] != b2[x]) {
						bad++;
						if (x < bx0) bx0 = x;
						if (x > bx1) bx1 = x;
						if (y < by0) by0 = y;
						if (y > by1) by1 = y;
					}
			}
			if (bad) {
				s_perf_fverr++;
				if (s_perf_fverr <= 40) {
					char b[160];
					snprintf(b, sizeof b, "fskip: VERIFY MISMATCH #%lu: layer frame, %ld px in %d,%d-%d,%d, scroll %d,%d, %d repairs, %d dmg",
					         s_perf_fverr, bad, bx0, by0, bx1, by1, dx, dy, n, s_layer_ndmg);
					SDLmini_Log(b);
				}
			}
		}
	}
	s_qn = 0;
	return 1;
}

/* Screen::flip is about to diff-copy `back` onto the screen. Returns 1 when
 * the frame equals the previous one and nothing more is to be done. */
static int skip_frame_end(SDL_Surface *back)
{
	int same;

	if (SDLmini_FrameSkip == 0) {
		if (s_back != NULL) { SDLmini_FlushQueue(); s_back = NULL; s_qprevn = -1; }
		return 0;
	}
	if (back != s_back) {
		SDLmini_FlushQueue();   /* whatever an earlier back buffer still owes */
		s_back = back;
		s_qprevn = -1;
		skip_next_frame();
		return 0;
	}
	same = !s_qforced && s_qprevn == s_qn &&
	       memcmp(s_q, s_qprev, (size_t)s_qn * sizeof(SkipOp)) == 0;
	if (same && SDLmini_FrameSkip == 1) {
		s_qn = 0;
		s_perf_fskip++;
				skip_next_frame();
		return 1;
	}
	if (!same && layer_frame()) {
				skip_next_frame();
		return 1;
	}
	screen_restore();   /* everything below works on the screen buffer */
	if (!same && !s_qforced && s_qprevn == s_qn && damage_collect() > 0) {
		/* only some sources changed: redraw just their rectangles - unless
		 * they cover most of the frame, where one full pass is cheaper */
		unsigned long area = 0, full = (unsigned long)s_back->w * (unsigned long)s_back->h;
		int i;
		for (i = 0; i < s_dmgn; i++) area += (unsigned long)s_dmg[i].w * s_dmg[i].h;
		if (area * 10UL < full * 6UL) {
			memcpy(s_qprev, s_q, (size_t)s_qn * sizeof(SkipOp));   /* the new reference */
			for (i = 0; i < s_dmgn; i++) skip_run_clipped(&s_dmg[i]);
			s_perf_fpart++;
			if (SDLmini_FrameSkip == 2) {
				/* verify: keep the partial result, draw the whole frame
				 * again, and compare - then go on as a normal frame */
				size_t sz = (size_t)s_back->pitch * (size_t)s_back->h;
				if (verify_buf()) {
					if (s_back_stale) {
						/* after a direct frame the last picture is on the
						 * screen: it, with the redrawn rectangles on top */
						int k;
						amiga_copy_rows(s_verify, s_back->pitch, s_screen->pixels, s_screen->pitch, s_back->w, s_back->h);
						for (k = 0; k < s_dmgn; k++) {
							int x0 = s_dmg[k].x < 0 ? 0 : s_dmg[k].x, y0 = s_dmg[k].y < 0 ? 0 : s_dmg[k].y;
							int x1 = s_dmg[k].x + s_dmg[k].w, y1 = s_dmg[k].y + s_dmg[k].h;
							if (x1 > s_back->w) x1 = s_back->w;
							if (y1 > s_back->h) y1 = s_back->h;
							if (x1 > x0 && y1 > y0)
								amiga_copy_rows(s_verify + (size_t)y0 * s_back->pitch + x0, s_back->pitch,
								                (const Uint8 *)s_back->pixels + (size_t)y0 * s_back->pitch + x0, s_back->pitch,
								                x1 - x0, y1 - y0);
						}
					} else {
						memcpy(s_verify, s_back->pixels, sz);
					}
					s_noelide = 1;   /* the reference: every op as asked for */
					skip_run();
					s_noelide = 0;
					if (memcmp(s_verify, s_back->pixels, sz) != 0) {
						s_perf_fverr++;
						if (s_perf_fverr <= 20) {
							char b[120];
							snprintf(b, sizeof b, "fskip: VERIFY MISMATCH #%lu: partial frame, %d rects, %d ops",
							         s_perf_fverr, s_dmgn, s_qprevn);
							SDLmini_Log(b);
						}
					}
				}
				s_qn = 0;
				skip_next_frame();
				return 0;            /* the full diff-copy follows */
			}
			damage_to_screen();
						s_qn = 0;
			skip_next_frame();
			return 1;
		}
	}
	if (!same && !s_qforced && s_direct_run > 0 && queue_paints_all() &&
	    s_back->w == s_screen->w && s_back->h == s_screen->h && s_back->pitch == s_screen->pitch) {
		/* a direct frame (see back_restore): the queue paints the screen */
		void *bp = s_back->pixels;
		memcpy(s_qprev, s_q, (size_t)s_qn * sizeof(SkipOp));
		s_qprevn = s_qn;
		s_back->pixels = s_screen->pixels;
		skip_run_ops();
		s_back->pixels = bp;
		s_direct_run--;
		dirty_add(0, 0, s_screen->w, s_screen->h);
		s_perf_fdirect++;
				s_back_stale = 1;
		if (SDLmini_FrameSkip == 2 && verify_buf()) {
			/* verify: the same queue, the ordinary way, into a scratch buffer
			 * standing in for the back buffer (which stays stale, as in mode 1) */
			int y;
			memcpy(s_q, s_qprev, (size_t)s_qprevn * sizeof(SkipOp));
			s_qn = s_qprevn;
			s_noelide = 1;
			s_back->pixels = s_verify;
			skip_run_ops();
			s_back->pixels = bp;
			s_noelide = 0;
			for (y = 0; y < s_back->h; y++)
				if (memcmp(s_verify + (size_t)y * s_back->pitch,
				           (const Uint8 *)s_screen->pixels + (size_t)y * s_screen->pitch, (size_t)s_back->w) != 0) break;
			if (y < s_back->h) {
				s_perf_fverr++;
				if (s_perf_fverr <= 20) {
					char b[96];
					snprintf(b, sizeof b, "fskip: VERIFY MISMATCH #%lu: direct frame, row %d, %d ops",
					         s_perf_fverr, y, s_qprevn);
					SDLmini_Log(b);
				}
			}
		}
		skip_next_frame();
		return 1;
	}
	{
		Uint32 before = same ? back_sum() : 0;   /* verify mode only */
		if (!s_qforced) {
			memcpy(s_qprev, s_q, (size_t)s_qn * sizeof(SkipOp));
			s_qprevn = s_qn;
		} else {
			s_qprevn = -1;
		}
		s_noelide = same;   /* verify: the reference frame without the fill elision */
		skip_run();
		s_noelide = 0;
		if (same) {
			if (back_sum() != before) {
				s_perf_fverr++;
				if (s_perf_fverr <= 20) {
					char b[96];
					snprintf(b, sizeof b, "fskip: VERIFY MISMATCH #%lu: %d ops judged unchanged",
					         s_perf_fverr, s_qprevn);
					SDLmini_Log(b);
				}
			} else {
				s_perf_fskip++;
			}
		}
	}
		skip_next_frame();
	return 0;
}

static int SDL_FillRect_(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
int SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color)
{
	unsigned long ft0_ = amiga_uclock_us();
	int r_ = SDL_FillRect_(dst, dstrect, color);
	SDLmini_ProfFillUs += amiga_uclock_us() - ft0_;
	return r_;
}
static int SDL_FillRect_(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color)
{
	int x, y, w, h, bpp;
	if (dst == NULL) return -1;
	bpp = dst->format->BytesPerPixel;

	if (dstrect == NULL) {
		x = dst->clip_rect.x; y = dst->clip_rect.y;
		w = dst->clip_rect.w; h = dst->clip_rect.h;
	} else {
		int cx1 = dst->clip_rect.x, cy1 = dst->clip_rect.y;
		int cx2 = cx1 + dst->clip_rect.w, cy2 = cy1 + dst->clip_rect.h;
		int x1 = dstrect->x, y1 = dstrect->y;
		int x2 = x1 + dstrect->w, y2 = y1 + dstrect->h;
		if (x1 < cx1) x1 = cx1;
		if (y1 < cy1) y1 = cy1;
		if (x2 > cx2) x2 = cx2;
		if (y2 > cy2) y2 = cy2;
		if (x2 <= x1 || y2 <= y1) return 0;
		x = x1; y = y1; w = x2 - x1; h = y2 - y1;
	}
	if (skip_queueing(dst)) {
		SDL_Rect r_;
		r_.x = (Sint16)x; r_.y = (Sint16)y; r_.w = (Uint16)w; r_.h = (Uint16)h;
		skip_push(NULL, color, NULL, &r_);
		return 0;
	}
	if (dst == s_screen) skip_invalidate();
	s_perf_fills++;
	s_perf_fill_px += (unsigned long)w * h;
	surf_touch(dst);   /* pixels change: reclassify */
	if (dst == s_screen) dirty_add(x, y, w, h);

	{
		/* TEMP diagnostic (see SDL_UpperBlit): large fills of the 320x200
		 * back buffer while it holds real content in the left area. */
		static int logged_;
		if (SDLmini_diag_armed && dst != s_screen && dst->w == 320 && dst->h == 200 && w * h > 20000 && logged_ < 8) {
			long nz = 0; int xx, yy;
			for (yy = 0; yy < 200; yy++) {
				const Uint8 *p = (const Uint8 *)dst->pixels + (size_t)yy * dst->pitch;
				for (xx = 0; xx < 256; xx++) if (p[xx]) nz++;
			}
			if (nz > 15000) {
				char b[128];
				snprintf(b, sizeof b, "SDLmini: FillRect WIPES back buffer (%ld px) %d,%d %dx%d color %lu", nz, x, y, w, h, (unsigned long)color);
				SDLmini_Log(b);
				logged_++;
			}
		}
	}
	if (bpp == 1) {
		Uint8 *row = (Uint8 *)dst->pixels + (size_t)y * dst->pitch + x;
		for (; h > 0; h--, row += dst->pitch) memset(row, (int)(color & 0xff), (size_t)w);
	} else {
		int i;
		Uint8 *row = (Uint8 *)dst->pixels + (size_t)y * dst->pitch + (size_t)x * bpp;
		for (; h > 0; h--, row += dst->pitch) {
			Uint8 *p = row;
			for (i = 0; i < w; i++) {
				memcpy(p, &color, (size_t)bpp);
				p += bpp;
			}
		}
	}
	return 0;
}

/* Do two cells of the diff-copy below differ? libnix memcmp is a byte loop
 * (move.b/move.b/cmp.b/dbf) behind a call, and the full-screen flip asks it
 * once per 32-byte cell: 2000 calls and 64000 byte compares per 320x200 frame,
 * four times that at 640x400, nearly all of them on cells that turn out equal.
 * Eight longword compares do the same work, stop at the first difference and
 * need no call. Taken only for whole, longword-aligned cells - always the case
 * for the flip, whose rows start at x 0 with a pitch that is a multiple of 4;
 * anything else keeps memcmp. */
static int cell_differs(const Uint8 *a, const Uint8 *b, int len)
{
	if (len == 32 && (((unsigned long)a | (unsigned long)b) & 3) == 0) {
		const Uint32 *x = (const Uint32 *)a, *y = (const Uint32 *)b;
		return x[0] != y[0] || x[1] != y[1] || x[2] != y[2] || x[3] != y[3] ||
		       x[4] != y[4] || x[5] != y[5] || x[6] != y[6] || x[7] != y[7];
	}
	return memcmp(a, b, (size_t)len) != 0;
}

/* 8 -> 8 blit, the only one the game needs. Straight memcpy per row unless a
 * colour key is set, in which case it is a per-pixel test - the same shape as
 * SDL blit_1 without the palette-mapping table, since every surface here
 * shares one 256-entry palette. */
static void blit8(SDL_Surface *src, const SDL_Rect *srcrect,
                  SDL_Surface *dst, const SDL_Rect *dstrect)
{
	int y;
	const Uint8 *sp = (const Uint8 *)src->pixels + (size_t)srcrect->y * src->pitch + srcrect->x;
	Uint8       *dp = (Uint8 *)dst->pixels       + (size_t)dstrect->y * dst->pitch + dstrect->x;
	int w = srcrect->w, h = srcrect->h;
	int keyed;

	if (skip_queueing(dst) && src != s_back) {
		skip_push(src, (src->flags & SDL_SRCCOLORKEY) ? (0x80000000UL | (src->format->colorkey & 0xff)) : 0,
		          srcrect, dstrect);
		return;
	}
	surf_touch(dst);    /* destination pixels change: reclassify before reuse */
	keyed = (src->flags & SDL_SRCCOLORKEY) && !s_blit_plain;   /* see fill_elided_by_next */
	if (keyed && src->unused1 == 0)
		{ unsigned long ct0_ = amiga_uclock_us(); src->unused1 = surf_classify(src); SDLmini_ProfClassifyUs += amiga_uclock_us() - ct0_; }
	/* Colorkey blits onto the screen dirty their rectangle as-is; the plain
	 * path below diff-copies instead and dirties only what really changed. */
	if (dst == s_screen && keyed && src->unused1 != 2) {
		dirty_add(dstrect->x, dstrect->y, w, h);
		skip_invalidate();   /* the screen now differs from the back buffer */
	}
	if (keyed && src->unused1 == 3) {
		/* wariant C: walk the cached runs - memcpy only, no compares */
		struct private_hwdata *hd = src->hwdata;
		int sx0 = srcrect->x, sx1 = srcrect->x + w;
		s_perf_blits_ck++;
		s_perf_px_ck += (unsigned long)w * h;
		for (y = 0; y < h; y++) {
			const Uint16 *r = hd->runs + hd->rowoff[srcrect->y + y];
			const Uint8 *srow = (const Uint8 *)src->pixels + (size_t)(srcrect->y + y) * src->pitch;
			Uint8 *drow = (Uint8 *)dst->pixels + (size_t)(dstrect->y + y) * dst->pitch + dstrect->x;
			int i, n = *r++;
			for (i = 0; i < n; i++) {
				int off = r[0], len = r[1], a = off, b = off + len;
				r += 2;
				if (a < sx0) a = sx0;
				if (b > sx1) b = sx1;
				if (b > a) memcpy(drow + (a - sx0), srow + a, (size_t)(b - a));
			}
		}
	} else if (keyed && src->unused1 != 2) {
		/* Colorkey blit, 4 pixels at a time (LISTA-ROBOT pkt 2, wariant A).
		 * Measured before this change: ~330k colorkey pixels per geoscape
		 * frame (the globe's radar/country/marker layers are ~95% transparent
		 * and full-screen). One longword compare skips 4 transparent pixels;
		 * one longword store copies 4 opaque ones. The mixed case - only at
		 * sprite edges - falls back to bytes. The has-a-key-byte test is the
		 * classic (x-0x01010101) & ~x & 0x80808080 trick, no per-byte
		 * compares. 68020 takes unaligned longword accesses. */
		Uint8  key  = (Uint8)(src->format->colorkey & 0xff);
		Uint32 key4 = (Uint32)key * 0x01010101UL;
		s_perf_blits_ck++;
		s_perf_px_ck += (unsigned long)w * h;
		for (y = 0; y < h; y++) {
			const Uint8 *s = sp;
			Uint8 *d = dp;
			int n = w;
			while (n >= 4) {
				Uint32 v;
				memcpy(&v, s, 4);
				if (v != key4) {
					Uint32 xk = v ^ key4;
					if (((xk - 0x01010101UL) & ~xk & 0x80808080UL) == 0) {
						memcpy(d, &v, 4);
					} else {
						if (s[0] != key) d[0] = s[0];
						if (s[1] != key) d[1] = s[1];
						if (s[2] != key) d[2] = s[2];
						if (s[3] != key) d[3] = s[3];
					}
				}
				s += 4; d += 4; n -= 4;
			}
			while (n-- > 0) {
				Uint8 c = *s++;
				if (c != key) *d = c;
				d++;
			}
			sp += src->pitch;
			dp += dst->pitch;
		}
	} else if (dst == s_screen) {
		/* Diff-copy (see the dirty-rectangle comment above): compare in
		 * 32-byte cells, copy changed cells only, dirty the changed area.
		 * Neighbouring changed cells of a row are copied as one run.
		 * When nearly everything changes (the battlescape scrolling: 99 of
		 * 100 frames at 640x400 on a Warp 060, flip ~32 ms) the comparing
		 * saves nothing, so after a frame with more than 90% of the cells
		 * changed the next 7 are copied whole, then compared again. */
		int dy0 = -1, dy1 = -1, dx0 = w, dx1 = 0;
		unsigned long cells = 0, changed = 0;
		s_perf_blits++;
		if (src != s_back) skip_invalidate();   /* the screen gets something else than the back buffer */
		if (s_diff_whole > 0) {
			s_diff_whole--;
			amiga_copy_rows(dp, dst->pitch, sp, src->pitch, w, h);
			s_perf_px += (unsigned long)w * h;
			dirty_add(dstrect->x, dstrect->y, w, h);
			return;
		}
		for (y = 0; y < h; y++) {
			int c, run = -1;
			for (c = 0; c < w; c += 32) {
				int len = (w - c < 32) ? (w - c) : 32;
				cells++;
				if (cell_differs(sp + c, dp + c, len)) {
					changed++;
					if (run < 0) run = c;
					if (c < dx0) dx0 = c;
					if (c + len > dx1) dx1 = c + len;
					if (dy0 < 0) dy0 = y;
					dy1 = y;
				} else if (run >= 0) {
					memcpy(dp + run, sp + run, (size_t)(c - run));
					s_perf_px += (unsigned long)(c - run);
					run = -1;
				}
			}
			if (run >= 0) {
				memcpy(dp + run, sp + run, (size_t)(w - run));
				s_perf_px += (unsigned long)(w - run);
			}
			sp += src->pitch;
			dp += dst->pitch;
		}
		if (changed * 10UL > cells * 9UL) s_diff_whole = 7;
		if (changed * 10UL > cells) s_direct_run = 7;   /* see back_restore */
		if (dy0 >= 0)
			dirty_add(dstrect->x + dx0, dstrect->y + dy0,
			          dx1 - dx0, dy1 - dy0 + 1);
	} else {
		s_perf_blits++;
		s_perf_px += (unsigned long)w * h;
		/* rows by MOVE16 where they line up (68040/060), memcpy otherwise */
		amiga_copy_rows(dp, dst->pitch, sp, src->pitch, w, h);
	}
}

int SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
	SDLMINI_FIRST("SDL_UpperBlit");
	SDL_Rect sr, dr;
	int cx1, cy1, cx2, cy2;

	if (src == NULL || dst == NULL) return -1;

	/* Screen::flip: the whole back buffer onto the screen. The frame is
	 * complete - compare it with the last one (unchanged-frame skip). */
	if (dst == s_screen && srcrect == NULL && dstrect == NULL &&
	    src->w == s_screen->w && src->format->BitsPerPixel == 8) {
		int done_ = skip_frame_end(src);
		layer_reset();   /* the layer's reports covered this frame; the next start from it */
		if (done_) return 0;
	} else {
		skip_reading(src);
	}

	if (src->format->BitsPerPixel != 8 || dst->format->BitsPerPixel != 8) {
		SDL_SetError("SDLmini: only 8bpp blits exist in this port (%d -> %d)",
		             src->format->BitsPerPixel, dst->format->BitsPerPixel);
		return -1;
	}

	if (srcrect != NULL) {
		sr = *srcrect;
	} else {
		sr.x = 0; sr.y = 0; sr.w = (Uint16)src->w; sr.h = (Uint16)src->h;
	}
	dr.x = (Sint16)(dstrect != NULL ? dstrect->x : 0);
	dr.y = (Sint16)(dstrect != NULL ? dstrect->y : 0);

	/* Clip the source rectangle to the source surface. */
	if (sr.x < 0) { sr.w = (Uint16)(sr.w + sr.x); dr.x = (Sint16)(dr.x - sr.x); sr.x = 0; }
	if (sr.y < 0) { sr.h = (Uint16)(sr.h + sr.y); dr.y = (Sint16)(dr.y - sr.y); sr.y = 0; }
	if (sr.x + sr.w > src->w) sr.w = (Uint16)(src->w - sr.x);
	if (sr.y + sr.h > src->h) sr.h = (Uint16)(src->h - sr.y);
	if ((Sint16)sr.w <= 0 || (Sint16)sr.h <= 0) goto empty;

	/* Clip against the destination clip rectangle, moving the source origin
	 * by the same amount. */
	cx1 = dst->clip_rect.x;
	cy1 = dst->clip_rect.y;
	cx2 = cx1 + dst->clip_rect.w;
	cy2 = cy1 + dst->clip_rect.h;

	if (dr.x < cx1) { int d = cx1 - dr.x; sr.x = (Sint16)(sr.x + d); sr.w = (Uint16)(sr.w - d); dr.x = (Sint16)cx1; }
	if (dr.y < cy1) { int d = cy1 - dr.y; sr.y = (Sint16)(sr.y + d); sr.h = (Uint16)(sr.h - d); dr.y = (Sint16)cy1; }
	if ((Sint16)sr.w <= 0 || (Sint16)sr.h <= 0) goto empty;
	if (dr.x + sr.w > cx2) sr.w = (Uint16)(cx2 - dr.x);
	if (dr.y + sr.h > cy2) sr.h = (Uint16)(cy2 - dr.y);
	if ((Sint16)sr.w <= 0 || (Sint16)sr.h <= 0) goto empty;

	dr.w = sr.w;
	dr.h = sr.h;
	{
		unsigned long bt0_ = amiga_uclock_us();
		blit8(src, &sr, dst, &dr);
		bt0_ = amiga_uclock_us() - bt0_;
		if ((unsigned long)sr.w * sr.h >= 32000UL) { SDLmini_ProfBlitBigUs += bt0_; SDLmini_ProfBlitBigN++; }
		else { SDLmini_ProfBlitSmallUs += bt0_; SDLmini_ProfBlitSmallN++; }
	}
	if (dstrect != NULL) *dstrect = dr;
	{
		/* TEMP diagnostic: who wipes the left 256 columns of the 320x200
		 * back buffer between the globe blit and the flip? */
		static long prev_;
		static int logged_;
		if (SDLmini_diag_armed && dst != s_screen && dst->w == 320 && dst->h == 200 && logged_ < 6) {
			long nz = 0; int x, y;
			for (y = 0; y < 200; y++) {
				const Uint8 *p = (const Uint8 *)dst->pixels + (size_t)y * dst->pitch;
				for (x = 0; x < 256; x++) if (p[x]) nz++;
			}
			if (nz + 2000 < prev_) {
				char b[200];
				snprintf(b, sizeof b, "SDLmini: blit DROPPED left-area count %ld -> %ld: src %dx%d at %d,%d (srcrect %d,%d %dx%d) colorkey %lu flags %lx",
					prev_, nz, src->w, src->h, (int)dr.x, (int)dr.y, (int)sr.x, (int)sr.y, (int)sr.w, (int)sr.h,
					(unsigned long)src->format->colorkey, (unsigned long)src->flags);
				SDLmini_Log(b);
				logged_++;
			}
			prev_ = nz;
		}
	}
	return 0;

empty:
	if (dstrect != NULL) { dstrect->w = 0; dstrect->h = 0; }
	return 0;
}

/* ------------------------------------------------------------ video mode -- */

int SDLmini_show_bar = 0;          /* Options::amigaAppBar, set before SDL_SetVideoMode */
int SDLmini_video_mode = 0;        /* Options::amigaVideoMode: 0 auto, 1 PAL, 2 NTSC */
static int s_opened_once = 0;      /* the splash belongs to the FIRST open only */
static int s_req_w = 0, s_req_h = 0; /* what the game asked for (the screen may be taller) */

SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags)
{
	char msg[128];

	(void)flags;
	if (bpp != 8) {
		SDL_SetError("SDLmini: only 8bpp video modes exist (asked for %d)", bpp);
		return NULL;
	}

	back_restore();   /* a direct frame's picture is in the screen that may go now */
	if (s_screen != NULL) {
		/* Reopening the same geometry is what OpenXcom does on every options
		 * change; there is nothing to do, and reopening the screen would
		 * flash the display for no reason. The display standard is the one
		 * exception: when the player changes PAL/NTSC on the Amiga tab, this
		 * is the moment it takes effect - leaving the options screen calls
		 * Screen::resetDisplay(), which lands here. It deliberately does NOT
		 * happen while the row is being cycled. */
		if (s_req_w == width && s_req_h == height &&
		    amigagfx_video_mode() == SDLmini_video_mode) return s_screen;
		SDLmini_VideoQuit();
		s_video_ready = 1;
	}
	amigagfx_set_video_mode(SDLmini_video_mode);
	s_req_w = width;
	s_req_h = height;

	/* The "Amiga screen title bar" option (SDLmini_show_bar, set by the game
	 * from Options::amigaAppBar): keep Intuition's bar with the depth gadget.
	 * amigagfx opens the screen bar-many lines TALLER in that case, so the
	 * game area below the bar is still exactly width x height and mouse
	 * coordinates stay 1:1. Meaningless for the WB window backend. */
	if (amigagfx_open(width, height, SDLmini_show_bar, s_backend) != 0) {
		SDL_SetError("SDLmini: amigagfx_open(%d, %d, backend %d) failed", width, height, s_backend);
		return NULL;
	}

	s_screen = new_surface(amigagfx_game_width(), amigagfx_game_height(), 8,
	                       amigagfx_pitch(), 0, 0, 0, 0, amigagfx_chunky());
	if (s_screen == NULL) {
		amigagfx_close();
		return NULL;
	}
	s_screen->flags |= SDL_HWSURFACE | SDL_FULLSCREEN;
	s_screen_stale = 0;                           /* a new display: nothing to read back */
	dirty_add(0, 0, s_screen->w, s_screen->h);   /* fresh screen: convert all */
	skip_invalidate();                            /* ... and diff-copy all */
	if (!s_opened_once) {
		s_opened_once = 1;
		AmigaSplash_Show();   /* loading splash, once, right after the first open */
	} else if (s_pal_valid) {
		/* A reopen (the player switched PAL/NTSC) gets a brand new screen and
		 * therefore a black palette; the game will not resend one until it
		 * next changes state, so put the current one back straight away. */
		amigagfx_set_palette(s_pal, 0, 256);
	}

	snprintf(msg, sizeof(msg), "SDLmini: video %ldx%ld 8bpp, backend %ld, pitch %ld, %s (%s)",
	        (long)s_screen->w, (long)s_screen->h, (long)amigagfx_backend(), (long)s_screen->pitch,
	        amigagfx_is_ntsc() ? "NTSC" : "PAL",
	        SDLmini_video_mode == 0 ? "auto" : "forced");
	SDLmini_Log(msg);
	return s_screen;
}

extern void amiga_heapguard_check(const char *where);   /* no-op unless AMIGA_HEAPGUARD=1 */

int SDL_Flip(SDL_Surface *screen)
{
	SDLMINI_FIRST("SDL_Flip");
	amiga_heapguard_check("SDL_Flip");
	/* AMIGA-PORT: keep the streamed music fed. Nothing else in the port
	 * calls this, and a starved music buffer means silence. */
	{
		extern void SDLmini_MixerService(void);
		SDLmini_MixerService();
	}
	if (screen == NULL) return -1;
	if (screen != s_screen) return 0;
	if (AmigaSplash_Active()) { SDLmini_flips++; return 0; }   /* splash owns the display */
	{
		/* TEMP diagnostic: what reaches the flip in the left 256 columns.
		 * SDLmini_flips is exactly one per rendered frame, so anything that
		 * wants to know "am I being called every frame?" can compare against
		 * it (Globe::draw does). */
		unsigned long flips_;
		SDLmini_flips++;
		flips_ = SDLmini_flips;
		if (SDLmini_diag_armed) {
			long nz = 0; int x, y;
			for (y = 0; y < screen->h; y++) {
				const Uint8 *p = (const Uint8 *)screen->pixels + (size_t)y * screen->pitch;
				for (x = 0; x < 256 && x < screen->w; x++) if (p[x]) nz++;
			}
			{
				char b[128];
				snprintf(b, sizeof b, "SDLmini: flip #%lu: %ld non-zero px in left 256 cols (pixels=%p chunky=%p)",
					flips_, nz, screen->pixels, (void *)amigagfx_chunky());
				SDLmini_Log(b);
			}
			if (--SDLmini_diag_armed <= 0) SDLmini_diag_armed = 0;
		}
	}
	{
		/* Convert only the dirty union; a clean frame skips c2p entirely. */
		int maxh = (s_req_h > 0 && s_req_h < screen->h) ? s_req_h : screen->h;
		int dx0 = s_dirty_x0, dy0 = s_dirty_y0, dx1 = s_dirty_x1, dy1 = s_dirty_y1;
		s_dirty_x0 = s_dirty_y0 = s_dirty_x1 = s_dirty_y1 = 0;
		if (dx0 < 0) dx0 = 0;
		if (dy0 < 0) dy0 = 0;
		if (dx1 > screen->w) dx1 = screen->w;
		if (dy1 > maxh) dy1 = maxh;
		if (dx1 > dx0 && dy1 > dy0) {
			Uint32 t0_ = SDL_GetTicks();
			amigagfx_blit(dx0, dy0, dx1 - dx0, dy1 - dy0);
			s_perf_c2p_ms += SDL_GetTicks() - t0_;
			s_perf_c2p_px += (unsigned long)(dx1 - dx0) * (dy1 - dy0);
		} else {
			s_perf_skips++;
		}
	}
	/* TEMP perf report, one line per 100 frames (LISTA-ROBOT pkt 1). */
	if ((SDLmini_flips % 100) == 0) {
		char b_[288];
		Uint32 now_ = SDL_GetTicks();
		snprintf(b_, sizeof b_,
			"perf: 100 frames in %lu ms: c2p %lu ms (%lu px, %lu skipped), blits ck %lu (%lu px) plain %lu (%lu px diff), fills %lu (%lu px), fskip %lu verr %lu fpart %lu (%lu px) fdirect %lu flayer %lu",
			(unsigned long)(now_ - s_perf_last_ms), s_perf_c2p_ms,
			s_perf_c2p_px, s_perf_skips,
			s_perf_blits_ck, s_perf_px_ck, s_perf_blits, s_perf_px,
			s_perf_fills, s_perf_fill_px, s_perf_fskip, s_perf_fverr, s_perf_fpart, s_perf_fpartpx, s_perf_fdirect, s_perf_flayer);
		s_perf_flayer = 0;
		s_perf_fskip = 0;   /* per window; verify mismatches stay cumulative */
		s_perf_fpart = s_perf_fpartpx = 0;
		s_perf_fdirect = 0;
		SDLmini_Log(b_);
		s_perf_last_ms = now_;
		s_perf_c2p_ms = 0;
		s_perf_c2p_px = s_perf_skips = 0;
		s_perf_blits = s_perf_blits_ck = s_perf_px = s_perf_px_ck = 0;
		s_perf_fills = s_perf_fill_px = 0;
	}
	return 0;
}

void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h)
{
	{ extern void SDLmini_MusicPump(void); SDLmini_MusicPump(); }
	if (screen == NULL || screen != s_screen) return;
	if (w == 0 || h == 0) { x = 0; y = 0; w = (Uint32)screen->w; h = (Uint32)screen->h; }
	amigagfx_blit((int)x, (int)y, (int)w, (int)h);
}

void SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects)
{
	SDLMINI_FIRST("SDL_UpdateRects");
	int i;
	for (i = 0; i < numrects; i++) {
		SDL_UpdateRect(screen, rects[i].x, rects[i].y, rects[i].w, rects[i].h);
	}
}

SDL_Surface *SDL_GetVideoSurface(void) { return s_screen; }

/* The resolution list the options screen offers. 320x200 is the only mode the
 * game is drawn for; the others exist because amiga_gfx can open them and a
 * larger RTG screen with the game centred is sometimes what a player wants. */
static SDL_Rect  s_modes[]    = { { 0, 0, 320, 200 }, { 0, 0, 640, 400 }, { 0, 0, 640, 480 } };
static SDL_Rect *s_modelist[] = { &s_modes[0], &s_modes[1], &s_modes[2], NULL };

SDL_Rect **SDL_ListModes(SDL_PixelFormat *format, Uint32 flags)
{
	(void)format; (void)flags;
	return s_modelist;
}

int SDL_VideoModeOK(int width, int height, int bpp, Uint32 flags)
{
	(void)flags;
	if (bpp != 8) return 0;
	if (width <= 0 || height <= 0) return 0;
	return 8;
}

const SDL_VideoInfo *SDL_GetVideoInfo(void)
{
	static SDL_VideoInfo info;
	static SDL_PixelFormat fmt;
	fmt.BitsPerPixel  = 8;
	fmt.BytesPerPixel = 1;
	info.vfmt = &fmt;
	return &info;
}

char *SDL_VideoDriverName(char *namebuf, int maxlen)
{
	static const char *names[] = { "amiga-aga", "amiga-rtg", "amiga-ehb", "amiga-wb" };
	int b = amigagfx_backend();
	if (b < 0 || b > 3) b = 0;
	strncpy(namebuf, names[b], (size_t)maxlen - 1);
	namebuf[maxlen - 1] = '\0';
	return namebuf;
}
