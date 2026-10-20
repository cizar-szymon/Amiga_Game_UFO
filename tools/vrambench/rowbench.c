/* rowbench - copying a 640x400 picture out of a wider buffer (pitch 896) at
 * every source alignment 0..15 into an aligned 640-pitch one, Fast -> Fast:
 * amiga_copy_rows (MOVE16 when the offsets line up, else memcpy per row)
 * against a plain C longword loop that reads the source unaligned.
 * Build: tools/vrambench/build.sh. */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include "amiga_copy.h"

struct Device *TimerBase;
static ULONG eclk;
static ULONG now_ticks(void) { struct EClockVal ev; eclk = ReadEClock(&ev); return ev.ev_lo; }
static ULONG ticks_us(ULONG dt) { return (dt / eclk) * 1000000UL + ((dt % eclk) * 1000UL) / (eclk / 1000UL); }

#define W 640
#define H 400
#define SP 896

static void rows_long(UBYTE *d, long dp, const UBYTE *s, long sp, long w, long h)
{
	long y;
	for (y = 0; y < h; y++) {
		ULONG *dl = (ULONG *)d;
		const ULONG *sl = (const ULONG *)s;
		long k = w >> 5;
		while (k--) {
			dl[0] = sl[0]; dl[1] = sl[1]; dl[2] = sl[2]; dl[3] = sl[3];
			dl[4] = sl[4]; dl[5] = sl[5]; dl[6] = sl[6]; dl[7] = sl[7];
			dl += 8; sl += 8;
		}
		k = (w >> 2) & 7;
		while (k--) *dl++ = *sl++;
		{
			UBYTE *db = (UBYTE *)dl; const UBYTE *sb = (const UBYTE *)sl; long r = w & 3;
			while (r--) *db++ = *sb++;
		}
		d += dp; s += sp;
	}
}

int main(void)
{
	struct timerequest tr;
	APTR m1, m2;
	UBYTE *src, *dst;
	int a;

	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0) != 0) return 20;
	TimerBase = tr.tr_node.io_Device;
	m1 = AllocVec(SP * (H + 1) + 64, MEMF_FAST); m2 = AllocVec(W * H + 64, MEMF_FAST);
	if (!m1 || !m2) return 20;
	src = (UBYTE *)(((ULONG)m1 + 15) & ~15UL); dst = (UBYTE *)(((ULONG)m2 + 15) & ~15UL);
	memset(src, 7, SP * H + 32);
	for (a = 0; a < 16; a++) {
		ULONG b1 = 0xffffffffUL, b2 = 0xffffffffUL, t0, t; int r;
		for (r = 0; r < 6; r++) {
			memset(src + a, (UBYTE)r, 4096);   /* some dirty lines */
			t0 = now_ticks(); amiga_copy_rows(dst, W, src + a, SP, W, H); t = ticks_us(now_ticks() - t0); if (t < b1) b1 = t;
			t0 = now_ticks(); rows_long(dst, W, src + a, SP, W, H); t = ticks_us(now_ticks() - t0); if (t < b2) b2 = t;
		}
		printf("src offset %2d: amiga_copy_rows %5lu us   longword loop %5lu us\n", a, b1, b2);
	}
	FreeVec(m1); FreeVec(m2);
	CloseDevice((struct IORequest *)&tr);
	return 0;
}
