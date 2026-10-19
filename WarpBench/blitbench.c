/* blitbench - sprites and tiles on a Warp RTG screen: blitter or CPU?
 *
 * For 2D games (tiles, scrolling, sprites) at RTG resolutions. On an 8-bit
 * and a 16-bit screen (640x400) it times:
 *   - BltBitMap card memory -> screen for blocks 8x8 .. 640x400: the cost of
 *     ONE call (setup included) and the throughput;
 *   - scrolling the whole screen by BltBitMap within the screen bitmap;
 *   - a masked blit (BltMaskBitMapRastPort): a sprite with transparency done
 *     by the system;
 *   - a 32x32 sprite with transparency drawn by the CPU (writes only, skipping
 *     transparent pixels), into the locked screen and into Fast RAM;
 *   - LockBitMapTagList + UnLockBitMap alone.
 * E-clock timing, best of REPS. No OS calls while a bitmap is locked.
 * Build: build.sh (amiga-gcc + NDK 3.2 + cybergraphics headers). */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <devices/timer.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/timer.h>
#include <proto/cybergraphics.h>

#define W 640
#define H 400
#define REPS 5
#define SPR 32                       /* the CPU sprite: SPR x SPR */
#define NSPR 200                     /* sprites per measurement */

struct Library *CyberGfxBase;
struct Device *TimerBase;
static ULONG eclk;
static ULONG now_ticks(void) { struct EClockVal ev; eclk = ReadEClock(&ev); return ev.ev_lo; }
static ULONG ticks_us(ULONG dt) { return (dt / eclk) * 1000000UL + ((dt % eclk) * 1000UL) / (eclk / 1000UL); }

static struct Screen *scr;
static struct BitMap *sheet;         /* off-screen "sprite sheet" in card memory */
static PLANEPTR mask;                /* 1-bit mask for BltMaskBitMapRastPort (Chip RAM) */
static UBYTE *fspr;                  /* sprite pixels in Fast RAM (bpp bytes per pixel) */
static UBYTE *fbuf;                  /* a Fast RAM frame */
static int bpp;                      /* bytes per pixel of the screen */

static char out[8192];
static ULONG outn;
#define OUT(...) (outn += (ULONG)snprintf(out + outn, sizeof out - outn, __VA_ARGS__))

/* best of REPS of fn(arg) -> us */
static ULONG best_us(void (*fn)(int), int arg)
{
	ULONG best = ~0UL, i;
	for (i = 0; i < REPS; i++) {
		ULONG t0 = now_ticks(), t;
		fn(arg);
		t = ticks_us(now_ticks() - t0);
		if (t < best) best = t;
	}
	return best;
}

/* ---- blitter ----------------------------------------------------------------- */
static int bw;                                   /* block size for the next test */
static void blit_blocks(int n)
{
	int i;
	for (i = 0; i < n; i++) {
		int x = (i * 37) % (W - bw + 1), y = (i * 23) % (H - bw + 1);
		BltBitMap(sheet, 0, 0, scr->RastPort.BitMap, x, y, bw, bw, 0xC0, 0xFF, NULL);
	}
	WaitBlit();
}
static void blit_full(int n) { int i; for (i = 0; i < n; i++) BltBitMap(sheet, 0, 0, scr->RastPort.BitMap, 0, 0, W, H, 0xC0, 0xFF, NULL); WaitBlit(); }
static void scroll_screen(int n)
{
	int i;
	for (i = 0; i < n; i++) BltBitMap(scr->RastPort.BitMap, 0, 8, scr->RastPort.BitMap, 0, 0, W, H - 8, 0xC0, 0xFF, NULL);
	WaitBlit();
}
static void blit_masked(int n)
{
	int i;
	for (i = 0; i < n; i++) {
		int x = (i * 37) % (W - SPR + 1), y = (i * 23) % (H - SPR + 1);
		BltMaskBitMapRastPort(sheet, 0, 0, &scr->RastPort, x, y, SPR, SPR, (ABC | ABNC | ANBC), mask);
	}
	WaitBlit();
}

/* ---- CPU sprite, writes only (0 = transparent) -------------------------------- */
static void cpu_sprites(UBYTE *base, ULONG bpr, int n)
{
	int i, y, x;
	for (i = 0; i < n; i++) {
		int px = (i * 37) % (W - SPR + 1), py = (i * 23) % (H - SPR + 1);
		for (y = 0; y < SPR; y++) {
			UBYTE *d = base + (ULONG)(py + y) * bpr + (ULONG)px * bpp;
			const UBYTE *s = fspr + (ULONG)y * SPR * bpp;
			if (bpp == 1) {
				for (x = 0; x < SPR; x++) if (s[x]) d[x] = s[x];
			} else {
				const UWORD *s2 = (const UWORD *)s; UWORD *d2 = (UWORD *)d;
				for (x = 0; x < SPR; x++) if (s2[x]) d2[x] = s2[x];
			}
		}
	}
}
static void cpu_sprites_vram(int n)
{
	APTR base = NULL, h; ULONG bpr = 0;
	struct TagItem t[] = { { LBMI_BASEADDRESS, (ULONG)&base }, { LBMI_BYTESPERROW, (ULONG)&bpr }, { TAG_DONE, 0 } };
	h = LockBitMapTagList(scr->RastPort.BitMap, t);
	if (h) { cpu_sprites((UBYTE *)base, bpr, n); UnLockBitMap(h); }
}
static void cpu_sprites_lock_each(int n)
{
	int i;
	for (i = 0; i < n; i++) {
		APTR base = NULL, h; ULONG bpr = 0;
		struct TagItem t[] = { { LBMI_BASEADDRESS, (ULONG)&base }, { LBMI_BYTESPERROW, (ULONG)&bpr }, { TAG_DONE, 0 } };
		h = LockBitMapTagList(scr->RastPort.BitMap, t);
		if (h) { cpu_sprites((UBYTE *)base, bpr, 1); UnLockBitMap(h); }
	}
}
static void cpu_sprites_fast(int n) { cpu_sprites(fbuf, (ULONG)W * bpp, n); }
static void lock_only(int n)
{
	int i;
	for (i = 0; i < n; i++) {
		APTR base = NULL, h; ULONG bpr = 0;
		struct TagItem t[] = { { LBMI_BASEADDRESS, (ULONG)&base }, { LBMI_BYTESPERROW, (ULONG)&bpr }, { TAG_DONE, 0 } };
		h = LockBitMapTagList(scr->RastPort.BitMap, t);
		if (h) UnLockBitMap(h);
	}
}

static void row(const char *name, ULONG us, ULONG calls, ULONG bytes_per_call)
{
	ULONG per = calls ? us * 100UL / calls : 0;          /* hundredths of a us per call */
	OUT("  %-44s %7lu.%02lu us/call", name, (unsigned long)(per / 100UL), (unsigned long)(per % 100UL));
	if (bytes_per_call && us) OUT("  %5lu MB/s", (unsigned long)(bytes_per_call * calls / us));
	OUT("\n");
}

static void suite(int depth)
{
	ULONG modeid, us;
	static const int sizes[] = { 8, 16, 32, 64, 128, 256 };
	unsigned k;

	modeid = BestCModeIDTags(CYBRBIDTG_NominalWidth, W, CYBRBIDTG_NominalHeight, H, CYBRBIDTG_Depth, depth, TAG_DONE);
	if (modeid == INVALID_ID) { OUT("== %d-bit: no 640x400 mode\n", depth); return; }
	scr = OpenScreenTags(NULL, SA_DisplayID, modeid, SA_Width, W, SA_Height, H, SA_Depth, depth,
	                     SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN, TAG_DONE);
	if (!scr) { OUT("== %d-bit: OpenScreen failed\n", depth); return; }
	bpp = (int)GetCyberMapAttr(scr->RastPort.BitMap, CYBRMATTR_BPPIX);
	sheet = AllocBitMap(W, H, depth, BMF_MINPLANES | BMF_DISPLAYABLE, scr->RastPort.BitMap);
	mask = AllocRaster(SPR, SPR);
	fspr = AllocVec(SPR * SPR * 4, MEMF_FAST | MEMF_CLEAR);
	fbuf = AllocVec((ULONG)W * H * 4, MEMF_FAST | MEMF_CLEAR);
	if (!sheet || !mask || !fspr || !fbuf) { OUT("== %d-bit: allocation failed\n", depth); goto done; }
	{   /* a round sprite: half the pixels opaque, like a typical character */
		int x, y;
		for (y = 0; y < SPR; y++) for (x = 0; x < SPR; x++) {
			int dx = x - SPR / 2, dy = y - SPR / 2, in = dx * dx + dy * dy < (SPR * SPR) / 5;
			if (bpp == 1) fspr[y * SPR + x] = in ? (UBYTE)(1 + ((x + y) & 63)) : 0;
			else ((UWORD *)fspr)[y * SPR + x] = in ? (UWORD)(0x1234 + x * 33 + y) : 0;
			if (x % 8 == 0) mask[y * (SPR / 8) + x / 8] = 0;
			if (in) mask[y * (SPR / 8) + x / 8] |= (UBYTE)(0x80 >> (x & 7));
		}
	}
	{
		ULONG adr = GetCyberMapAttr(sheet, CYBRMATTR_DISPADR);
		OUT("== %d-bit screen (%d byte%s per pixel), mode $%08lx, sprite sheet at $%08lx (%s)\n", depth, bpp,
		    bpp > 1 ? "s" : "", (unsigned long)modeid, (unsigned long)adr,
		    TypeOfMem((APTR)adr) ? "system RAM" : "card memory");
	}
	OUT("  BltBitMap card memory -> screen (one call per block, plain copy):\n");
	for (k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
		char nm[48]; ULONG calls = sizes[k] <= 32 ? 1000 : sizes[k] <= 128 ? 200 : 50;
		bw = sizes[k];
		us = best_us(blit_blocks, (int)calls);
		snprintf(nm, sizeof nm, "    %dx%d", sizes[k], sizes[k]);
		row(nm, us, calls, (ULONG)sizes[k] * sizes[k] * bpp);
	}
	us = best_us(blit_full, 20);       row("    640x400 (whole frame)", us, 20, (ULONG)W * H * bpp);
	us = best_us(scroll_screen, 20);   row("  scroll the screen 8 lines (BltBitMap on itself)", us, 20, (ULONG)W * (H - 8) * bpp);
	us = best_us(blit_masked, NSPR);   row("  32x32 masked: BltMaskBitMapRastPort", us, NSPR, (ULONG)SPR * SPR * bpp);
	us = best_us(cpu_sprites_vram, NSPR);      row("  32x32 CPU sprite -> screen, one lock", us, NSPR, 0);
	us = best_us(cpu_sprites_lock_each, NSPR); row("  32x32 CPU sprite -> screen, lock per sprite", us, NSPR, 0);
	us = best_us(cpu_sprites_fast, NSPR);      row("  32x32 CPU sprite -> Fast RAM frame", us, NSPR, 0);
	us = best_us(lock_only, 1000);             row("  LockBitMapTagList + UnLockBitMap alone", us, 1000, 0);
done:
	if (fbuf) FreeVec(fbuf);
	if (fspr) FreeVec(fspr);
	if (mask) FreeRaster(mask, SPR, SPR);
	if (sheet) FreeBitMap(sheet);
	fbuf = fspr = NULL; mask = NULL; sheet = NULL;
	CloseScreen(scr);
	scr = NULL;
}

int main(void)
{
	struct timerequest tr;
	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0) != 0) return 20;
	TimerBase = tr.tr_node.io_Device;
	CyberGfxBase = OpenLibrary((CONST_STRPTR)CYBERGFXNAME, 41);
	if (!CyberGfxBase) { printf("no cybergraphics.library 41\n"); return 20; }
	(void)now_ticks();
	OUT("blitbench: 640x400 RTG screens, best of %d, E-clock %lu Hz\n", REPS, (unsigned long)eclk);
	suite(8);
	suite(16);
	fputs(out, stdout);
	CloseLibrary(CyberGfxBase);
	CloseDevice((struct IORequest *)&tr);
	return 0;
}
