/* movetest - is amiga_move() (native/amiga_copy.c) an exact memmove for
 * overlapping blocks, both directions, every distance class (MOVE16 when the
 * distance is a multiple of 16, longwords otherwise), with the data cache in
 * copyback holding freshly written lines? And how fast is the battlescape
 * scroll shift (640x400 picture moved inside its own buffer) against libnix
 * memmove? Build: tools/vrambench/build.sh. */
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

#define N (640 * 400)
#define PAD 4096
static UBYTE *buf, *ref;

static void fill(UBYTE *p, ULONG n, ULONG seed) { ULONG i; for (i = 0; i < n; i++) p[i] = (UBYTE)((i * 13) ^ (seed + (i >> 7))); }

/* reference memmove, byte by byte in the safe direction */
static void ref_move(UBYTE *d, const UBYTE *s, ULONG n)
{
	ULONG i;
	if (d < s) for (i = 0; i < n; i++) d[i] = s[i];
	else for (i = n; i-- > 0; ) d[i] = s[i];
}

static ULONG check(long base, long off, ULONG n, ULONG seed)
{
	ULONG i, bad = 0;
	fill(buf, N + 2 * PAD, seed);   /* fresh writes: dirty lines in the cache */
	memcpy(ref, buf, N + 2 * PAD);
	if (off >= 0) { ref_move(ref + base + off, ref + base, n); amiga_move(buf + base + off, buf + base, n); }
	else          { ref_move(ref + base, ref + base - off, n); amiga_move(buf + base, buf + base - off, n); }
	if (memcmp(buf, ref, N + 2 * PAD) != 0)
		for (i = 0; i < N + 2 * PAD; i++) if (buf[i] != ref[i]) bad++;
	return bad;
}

int main(void)
{
	struct timerequest tr;
	APTR m1, m2;
	ULONG bad = 0, cases = 0, worst = 0;
	static const long offs[] = { 1, 2, 3, 4, 5, 8, 15, 16, 17, 32, 48, 640, 641, 639, 1280, 6400, 6408, 12800, 3, 7 };
	static const ULONG lens[] = { 0, 1, 3, 15, 16, 17, 63, 64, 65, 100, 1000, 4097, N - 12800 };
	static const long bases[] = { PAD, PAD + 1, PAD + 7, PAD + 13 };
	unsigned o, l, b;
	int sgn;

	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0) != 0) return 20;
	TimerBase = tr.tr_node.io_Device;
	m1 = AllocVec(N + 2 * PAD + 32, MEMF_FAST); m2 = AllocVec(N + 2 * PAD + 32, MEMF_FAST);
	if (!m1 || !m2) { printf("setup failed\n"); return 20; }
	buf = (UBYTE *)(((ULONG)m1 + 15) & ~15UL); ref = (UBYTE *)(((ULONG)m2 + 15) & ~15UL);
	printf("move16 path: %s\n", amiga_copy_move16() ? "yes" : "no");

	for (sgn = 1; sgn >= -1; sgn -= 2)
		for (o = 0; o < sizeof offs / sizeof offs[0]; o++)
			for (l = 0; l < sizeof lens / sizeof lens[0]; l++)
				for (b = 0; b < sizeof bases / sizeof bases[0]; b++) {
					ULONG e = check(bases[b], sgn * offs[o], lens[l], o * 31 + l * 7 + b);
					cases++;
					if (e) { bad++; if (e > worst) worst = e; if (bad <= 10) printf("BAD: base %ld off %ld len %lu: %lu bytes\n", bases[b], sgn * offs[o], lens[l], e); }
				}
	printf("exactness: %lu cases, %lu wrong (worst %lu bytes)\n", cases, bad, worst);

	/* speed: the whole picture shifted by one scroll step, best of 8 */
	{
		static const long steps[] = { 640 * 8, -640 * 8, 8, -8, 640 * 8 + 8, 640 * 8 - 8, 3, -3, 640 * 5 + 2 };
		unsigned s, r;
		for (s = 0; s < sizeof steps / sizeof steps[0]; s++) {
			long off = steps[s];
			ULONG n = N - (ULONG)(off < 0 ? -off : off);
			ULONG best1 = 0xffffffffUL, best2 = 0xffffffffUL;
			for (r = 0; r < 8; r++) {
				ULONG t0, t1;
				fill(buf, N + 2 * PAD, r);
				t0 = now_ticks();
				if (off >= 0) amiga_move(buf + PAD + off, buf + PAD, n); else amiga_move(buf + PAD, buf + PAD - off, n);
				t1 = ticks_us(now_ticks() - t0); if (t1 < best1) best1 = t1;
				fill(buf, N + 2 * PAD, r);
				t0 = now_ticks();
				if (off >= 0) memmove(buf + PAD + off, buf + PAD, n); else memmove(buf + PAD, buf + PAD - off, n);
				t1 = ticks_us(now_ticks() - t0); if (t1 < best2) best2 = t1;
			}
			printf("shift %6ld: amiga_move %5lu us   libnix memmove %6lu us\n", off, best1, best2);
		}
		{
			ULONG best = 0xffffffffUL;
			for (r = 0; r < 8; r++) {
				ULONG t0, t1;
				fill(buf, N + 2 * PAD, r);
				t0 = now_ticks();
				memcpy(ref, buf, N);
				t1 = ticks_us(now_ticks() - t0); if (t1 < best) best = t1;
			}
			printf("memcpy of the picture, other buffer: %lu us\n", best);
		}
	}
	FreeVec(m1); FreeVec(m2);
	CloseDevice((struct IORequest *)&tr);
	return bad ? 10 : 0;
}
