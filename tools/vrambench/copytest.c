/* copytest - is native/amiga_copy.c exact, with the data cache in copyback
 * holding freshly written (dirty) source and destination lines? And how fast
 * is it against memcpy per row? Fast -> Fast and Fast -> VRAM (RTG screen).
 * Build: tools/vrambench/build.sh (links ../../AmiXcom/native/amiga_copy.c). */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <intuition/screens.h>
#include <devices/timer.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/timer.h>
#include <proto/cybergraphics.h>
#include "amiga_copy.h"

struct Library *CyberGfxBase;
struct Device *TimerBase;
static ULONG eclk;
static ULONG now_ticks(void) { struct EClockVal ev; eclk = ReadEClock(&ev); return ev.ev_lo; }
static ULONG ticks_us(ULONG dt) { return (dt / eclk) * 1000000UL + ((dt % eclk) * 1000UL) / (eclk / 1000UL); }

#define SP 768          /* source pitch: rows not contiguous -> the row path */
#define DP 704
#define HH 400
static UBYTE *src, *dst, *ref;

static void fill(UBYTE *p, ULONG n, ULONG seed) { ULONG i; for (i = 0; i < n; i++) p[i] = (UBYTE)((i * 13) ^ (seed + (i >> 7))); }

/* fresh writes right before the copy: the lines are dirty in the cache */
static ULONG check(long so, long dof, long w, long h, ULONG seed)
{
	long y; ULONG bad = 0;
	fill(src, SP * HH + 64, seed);
	fill(dst, DP * HH + 64, seed ^ 0xA5A5UL);
	memcpy(ref, dst, DP * HH + 64);
	for (y = 0; y < h; y++) memcpy(ref + dof + y * DP, src + so + y * SP, (size_t)w);
	amiga_copy_rows(dst + dof, DP, src + so, SP, w, h);
	if (memcmp(dst, ref, DP * HH + 64) != 0) {
		ULONG i; for (i = 0; i < DP * HH + 64; i++) if (dst[i] != ref[i]) bad++;
	}
	return bad;
}

int main(void)
{
	struct timerequest tr;
	APTR m1, m2, m3;
	ULONG bad = 0, cases = 0;
	static const long offs[][2] = { { 0, 0 }, { 3, 3 }, { 16, 32 }, { 5, 9 }, { 15, 15 } };
	static const long widths[] = { 1, 63, 64, 100, 640, 641 };
	unsigned o, k;

	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0) != 0) return 20;
	TimerBase = tr.tr_node.io_Device;
	CyberGfxBase = OpenLibrary((CONST_STRPTR)CYBERGFXNAME, 41);
	m1 = AllocVec(SP * HH + 128, MEMF_FAST); m2 = AllocVec(DP * HH + 128, MEMF_FAST); m3 = AllocVec(DP * HH + 128, MEMF_FAST);
	if (!m1 || !m2 || !m3 || !CyberGfxBase) { printf("setup failed\n"); return 20; }
	src = (UBYTE *)(((ULONG)m1 + 15) & ~15UL); dst = (UBYTE *)(((ULONG)m2 + 15) & ~15UL); ref = (UBYTE *)(((ULONG)m3 + 15) & ~15UL);
	printf("MOVE16 path %s\n", amiga_copy_move16() ? "ON (68040/060)" : "off");

	for (o = 0; o < sizeof offs / sizeof offs[0]; o++)
		for (k = 0; k < sizeof widths / sizeof widths[0]; k++) {
			ULONG b = check(offs[o][0], offs[o][1], widths[k], HH, o * 31 + k);
			cases++;
			if (b) { bad += b; printf("  MISMATCH src+%ld dst+%ld w %ld: %lu bytes\n", offs[o][0], offs[o][1], widths[k], b); }
		}
	printf("Fast->Fast correctness: %lu cases, %lu wrong bytes\n", cases, bad);

	{   /* timing, Fast -> Fast, 640 x 400 rows */
		ULONG best1 = ~0UL, best2 = ~0UL, i, y, t;
		for (i = 0; i < 8; i++) {
			ULONG t0 = now_ticks();
			for (y = 0; y < HH; y++) memcpy(dst + y * DP, src + y * SP, 640);
			t = ticks_us(now_ticks() - t0); if (t < best1) best1 = t;
			t0 = now_ticks();
			amiga_copy_rows(dst, DP, src, SP, 640, HH);
			t = ticks_us(now_ticks() - t0); if (t < best2) best2 = t;
		}
		printf("Fast->Fast 640x400 rows: memcpy per row %lu us, amiga_copy_rows %lu us\n", best1, best2);
	}

	{   /* Fast -> VRAM: correctness (read back) and timing */
		ULONG modeid = BestCModeIDTags(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 400, CYBRBIDTG_Depth, 8, TAG_DONE);
		struct Screen *scr = (modeid != INVALID_ID) ? OpenScreenTags(NULL, SA_DisplayID, modeid, SA_Width, 640, SA_Height, 400,
		                     SA_Depth, 8, SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN, TAG_DONE) : NULL;
		ULONG vbad = 0, best1 = ~0UL, best2 = ~0UL, vb = 0;
		if (scr) {
			APTR base = NULL, h; ULONG bpr = 0, i, y, t;
			struct TagItem tags[] = { { LBMI_BASEADDRESS, (ULONG)&base }, { LBMI_BYTESPERROW, (ULONG)&bpr }, { TAG_DONE, 0 } };
			fill(src, SP * HH + 64, 77);
			h = LockBitMapTagList(scr->RastPort.BitMap, tags);
			if (h && base) {
				UBYTE *v = (UBYTE *)base;
				vb = bpr;
				for (i = 0; i < 8; i++) {
					ULONG t0 = now_ticks();
					for (y = 0; y < HH; y++) memcpy(v + y * bpr, src + y * SP, 640);
					t = ticks_us(now_ticks() - t0); if (t < best1) best1 = t;
					t0 = now_ticks();
					amiga_copy_rows(v + 1, bpr, src + 1, SP, 638, HH);  /* ragged ends too */
					t = ticks_us(now_ticks() - t0); if (t < best2) best2 = t;
				}
				for (y = 0; y < HH; y++) if (memcmp(v + 1 + y * bpr, src + 1 + y * SP, 638) != 0) vbad++;
				{   /* aligned rows (the game's RTG dirty rectangles start on 32-pixel cells) and the cache push alone */
					ULONG b3 = ~0UL, b4 = ~0UL;
					for (i = 0; i < 8; i++) {
						ULONG t0 = now_ticks();
						amiga_copy_rows(v, bpr, src, SP, 640, HH);
						t = ticks_us(now_ticks() - t0); if (t < b3) b3 = t;
						t0 = now_ticks();
						CacheClearE(src, SP * (HH - 1) + 640, CACRF_ClearD);
						CacheClearE(v, bpr * (HH - 1) + 640, CACRF_ClearD);
						t = ticks_us(now_ticks() - t0); if (t < b4) b4 = t;
					}
					for (y = 0; y < HH; y++) if (memcmp(v + y * bpr, src + y * SP, 640) != 0) vbad++;
					best1 = best1 | 0; vb = vb | 0;
					printf("  (aligned rows: amiga_copy_rows %lu us; the two CacheClearE alone %lu us)\n", b3, b4);
				}
				UnLockBitMap(h);
			}
			CloseScreen(scr);
			printf("Fast->VRAM 640x400 rows (bpr %lu): memcpy per row %lu us, amiga_copy_rows %lu us; read-back wrong rows %lu\n",
			       vb, best1, best2, vbad);
		} else printf("no RTG screen - VRAM test skipped\n");
	}

	FreeVec(m1); FreeVec(m2); FreeVec(m3);
	CloseLibrary(CyberGfxBase);
	CloseDevice((struct IORequest *)&tr);
	return bad ? 10 : 0;
}
