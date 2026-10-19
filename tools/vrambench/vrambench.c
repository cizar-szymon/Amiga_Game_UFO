/* vrambench - where should AmiXcom compose a 640x400x8 frame on a Warp?
 *
 * Opens an 8-bit RTG screen, allocates a bitmap "friend" of it (Picasso96
 * puts that in the card's memory) and times, with the E-clock, the work a
 * frame needs when the picture is built
 *   - in Fast RAM and copied to the screen (what the port does now), or
 *   - straight in the off-screen bitmap and blitted to the screen by the
 *     card (BltBitMap - the Warp's hardware blitter).
 * Every test runs REPS times; the best time counts. Prints ms per frame
 * (640x400 = 256000 bytes) and MB/s. No floats: integer arithmetic only.
 * Build: tools/vrambench/build.sh (amiga-gcc, 68060 code: MOVE16 test). */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/execbase.h>
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
#define REPS 8

struct Library *CyberGfxBase;
struct Device *TimerBase;
static ULONG eclk;

/* E-clock ticks, low word: differences of a few seconds at most are taken,
 * so the wrap (every ~100 minutes) costs one sample at worst. */
static ULONG now_ticks(void)
{
	struct EClockVal ev;
	eclk = ReadEClock(&ev);
	return ev.ev_lo;
}

/* ticks -> microseconds without 64-bit maths (dt < ~4 s) */
static ULONG ticks_us(ULONG dt)
{
	return (dt / eclk) * 1000000UL + ((dt % eclk) * 1000UL) / (eclk / 1000UL);
}

typedef void (*TestFn)(void);
static UBYTE *fast, *fast2;          /* 16-aligned 640x400 buffers in Fast RAM */
static APTR fastmem, fastmem2;
static struct Screen *scr;
static struct BitMap *off;           /* off-screen, friend of the screen */

static void report(const char *name, TestFn fn, ULONG bytes)
{
	ULONG best = 0xFFFFFFFFUL, t, i;
	for (i = 0; i < REPS; i++) {
		ULONG t0 = now_ticks();
		fn();
		t = ticks_us(now_ticks() - t0);
		if (t < best) best = t;
	}
	printf("%-46s %6lu.%02lu ms  %4lu MB/s\n", name, best / 1000UL, (best % 1000UL) / 10UL,
	       best ? (bytes / best) : 0UL);   /* bytes/us = MB/s */
}

/* ---- the screen bitmap and the off-screen bitmap, locked ---------------- */
static UBYTE *lock_base(struct BitMap *bm, ULONG *bpr, APTR *handle)
{
	APTR base = NULL;
	struct TagItem tags[] = { { LBMI_BASEADDRESS, (ULONG)&base }, { LBMI_BYTESPERROW, (ULONG)bpr }, { TAG_DONE, 0 } };
	*handle = LockBitMapTagList(bm, tags);
	return (UBYTE *)base;
}

/* ---- composition-like work on a 640x400 buffer -------------------------- */
static void clear_buf(UBYTE *p, ULONG bpr) { ULONG y; for (y = 0; y < H; y++) memset(p + y * bpr, 0, W); }
static void copy_rows(UBYTE *d, ULONG dbpr, const UBYTE *s, ULONG sbpr)
{ ULONG y; for (y = 0; y < H; y++) memcpy(d + y * dbpr, s + y * sbpr, W); }
static void copy_longs(UBYTE *d, ULONG dbpr, const UBYTE *s, ULONG sbpr)
{
	ULONG y, x;
	for (y = 0; y < H; y++) {
		ULONG *dl = (ULONG *)(d + y * dbpr); const ULONG *sl = (const ULONG *)(s + y * sbpr);
		for (x = 0; x < W / 16; x++) { dl[0] = sl[0]; dl[1] = sl[1]; dl[2] = sl[2]; dl[3] = sl[3]; dl += 4; sl += 4; }
	}
}
static void copy_move16(UBYTE *d, ULONG dbpr, const UBYTE *s, ULONG sbpr)
{
	ULONG y;
	for (y = 0; y < H; y++) {
		register const UBYTE *a0 __asm("a0") = s + y * sbpr;
		register UBYTE *a1 __asm("a1") = d + y * dbpr;
		register ULONG d0 __asm("d0") = W / 64;
		__asm__ volatile (
			"1:\n\t"
			".word 0xf620,0x9000\n\t"   /* move16 (a0)+,(a1)+ */
			".word 0xf620,0x9000\n\t"
			".word 0xf620,0x9000\n\t"
			".word 0xf620,0x9000\n\t"
			"subq.l #1,%2\n\t"
			"bne.s 1b"
			: "+a" (a0), "+a" (a1), "+d" (d0) : : "cc", "memory");
	}
}
/* sprite-like: 32-byte runs scattered over the frame, byte stores (the
 * game's colour-key blits copy runs of opaque pixels with memcpy) */
static void sprites(UBYTE *d, ULONG dbpr, const UBYTE *s)
{
	ULONG y, x;
	for (y = 0; y < H; y++) for (x = 0; x + 32 <= W; x += 40) {
		UBYTE *p = d + y * dbpr + x + (y & 7); const UBYTE *q = s + x;
		ULONG i; for (i = 0; i < 32; i++) p[i] = q[i];
	}
}
static ULONG sum_read(const UBYTE *p, ULONG bpr)
{ ULONG y, x, s = 0; for (y = 0; y < H; y++) { const ULONG *l = (const ULONG *)(p + y * bpr); for (x = 0; x < W / 4; x++) s += l[x]; } return s; }
static volatile ULONG sink;

/* ---- the tests ------------------------------------------------------------- */
static void t_fast_clear(void) { clear_buf(fast, W); }
static void t_fast_copy(void) { copy_rows(fast2, W, fast, W); }
static void t_fast_sprites(void) { sprites(fast2, W, fast); }
static void t_fast_read(void) { sink = sum_read(fast, W); }

static void t_scr_memcpy(void) { ULONG bpr; APTR h; UBYTE *b = lock_base(scr->RastPort.BitMap, &bpr, &h); if (h) { copy_rows(b, bpr, fast, W); UnLockBitMap(h); } }
static void t_scr_longs(void)  { ULONG bpr; APTR h; UBYTE *b = lock_base(scr->RastPort.BitMap, &bpr, &h); if (h) { copy_longs(b, bpr, fast, W); UnLockBitMap(h); } }
static void t_scr_move16(void) { ULONG bpr; APTR h; UBYTE *b = lock_base(scr->RastPort.BitMap, &bpr, &h); if (h) { if (((ULONG)b & 15) == 0 && (bpr & 15) == 0) copy_move16(b, bpr, fast, W); UnLockBitMap(h); } }
static void t_scr_wcp(void) { WriteChunkyPixels(&scr->RastPort, 0, 0, W - 1, H - 1, fast, W); }

static void t_off_clear(void)   { ULONG bpr; APTR h; UBYTE *b = lock_base(off, &bpr, &h); if (h) { clear_buf(b, bpr); UnLockBitMap(h); } }
static void t_off_copy(void)    { ULONG bpr; APTR h; UBYTE *b = lock_base(off, &bpr, &h); if (h) { copy_rows(b, bpr, fast, W); UnLockBitMap(h); } }
static void t_off_sprites(void) { ULONG bpr; APTR h; UBYTE *b = lock_base(off, &bpr, &h); if (h) { sprites(b, bpr, fast); UnLockBitMap(h); } }
static void t_off_read(void)    { ULONG bpr; APTR h; UBYTE *b = lock_base(off, &bpr, &h); if (h) { sink = sum_read(b, bpr); UnLockBitMap(h); } }
static void t_off_blit(void)    { BltBitMap(off, 0, 0, scr->RastPort.BitMap, 0, 0, W, H, 0xC0, 0xFF, NULL); WaitBlit(); }
static void t_off_blit_rp(void) { BltBitMapRastPort(off, 0, 0, &scr->RastPort, 0, 0, W, H, 0xC0); WaitBlit(); }

int main(void)
{
	struct timerequest tr;
	ULONG modeid, bpr; APTR h; UBYTE *b;
	int rc = 20;

	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0) != 0) { printf("no timer.device\n"); return 20; }
	TimerBase = tr.tr_node.io_Device;
	CyberGfxBase = OpenLibrary((CONST_STRPTR)CYBERGFXNAME, 41);
	if (!CyberGfxBase) { printf("no cybergraphics.library 41\n"); goto out0; }

	fastmem  = AllocVec(W * H + 16, MEMF_FAST | MEMF_CLEAR);
	fastmem2 = AllocVec(W * H + 16, MEMF_FAST | MEMF_CLEAR);
	if (!fastmem || !fastmem2) { printf("no fast ram\n"); goto out1; }
	fast  = (UBYTE *)(((ULONG)fastmem  + 15) & ~15UL);
	fast2 = (UBYTE *)(((ULONG)fastmem2 + 15) & ~15UL);
	{ ULONG i; for (i = 0; i < W * H; i++) fast[i] = (UBYTE)(i * 7 + (i >> 9)); }

	modeid = BestCModeIDTags(CYBRBIDTG_NominalWidth, W, CYBRBIDTG_NominalHeight, H, CYBRBIDTG_Depth, 8, TAG_DONE);
	if (modeid == INVALID_ID) { printf("no 640x400x8 RTG mode\n"); goto out1; }
	scr = OpenScreenTags(NULL, SA_DisplayID, modeid, SA_Width, W, SA_Height, H, SA_Depth, 8,
	                     SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN, TAG_DONE);
	if (!scr) { printf("OpenScreen failed\n"); goto out1; }
	off = AllocBitMap(W, H, 8, BMF_MINPLANES | BMF_DISPLAYABLE, scr->RastPort.BitMap);
	if (!off) { printf("AllocBitMap failed\n"); goto out2; }

	printf("mode $%08lx, screen bitmap cgx=%ld, off-screen cgx=%ld\n", modeid,
	       (long)GetCyberMapAttr(scr->RastPort.BitMap, CYBRMATTR_ISCYBERGFX), (long)GetCyberMapAttr(off, CYBRMATTR_ISCYBERGFX));
	b = lock_base(scr->RastPort.BitMap, &bpr, &h);
	if (h) { printf("screen   base %p bpr %lu memtype %lx\n", (void *)b, bpr, TypeOfMem(b)); UnLockBitMap(h); }
	b = lock_base(off, &bpr, &h);
	if (h) { printf("offscreen base %p bpr %lu memtype %lx (0 = not exec RAM, i.e. card memory)\n", (void *)b, bpr, TypeOfMem(b)); UnLockBitMap(h); }
	printf("fast     base %p memtype %lx\n", (void *)fast, TypeOfMem(fast));
	(void)now_ticks();
	printf("\n640x400x8 = 256000 bytes per frame, best of %d, E-clock %lu Hz\n", REPS, eclk);

	printf("-- in Fast RAM (what the port composes in)\n");
	report("Fast: clear frame (memset rows)", t_fast_clear, W * H);
	report("Fast: copy frame (memcpy rows)", t_fast_copy, W * H);
	report("Fast: 32-byte runs, byte stores (sprites)", t_fast_sprites, W * H * 32 / 40);
	report("Fast: read frame (longs)", t_fast_read, W * H);
	printf("-- Fast RAM -> screen (the port's flip now)\n");
	report("Fast->screen: lock + memcpy rows (now)", t_scr_memcpy, W * H);
	report("Fast->screen: lock + long moves", t_scr_longs, W * H);
	report("Fast->screen: lock + MOVE16", t_scr_move16, W * H);
	report("Fast->screen: WriteChunkyPixels", t_scr_wcp, W * H);
	printf("-- composing in the off-screen bitmap (card memory)\n");
	report("Off: clear frame (memset rows)", t_off_clear, W * H);
	report("Off: copy frame from Fast (memcpy rows)", t_off_copy, W * H);
	report("Off: 32-byte runs, byte stores (sprites)", t_off_sprites, W * H * 32 / 40);
	report("Off: read frame (longs) - a diff would", t_off_read, W * H);
	report("Off->screen: BltBitMap (card blitter)", t_off_blit, W * H);
	report("Off->screen: BltBitMapRastPort", t_off_blit_rp, W * H);
	rc = 0;

	FreeBitMap(off);
out2:
	CloseScreen(scr);
out1:
	if (fastmem) FreeVec(fastmem);
	if (fastmem2) FreeVec(fastmem2);
	CloseLibrary(CyberGfxBase);
out0:
	CloseDevice((struct IORequest *)&tr);
	return rc;
}
