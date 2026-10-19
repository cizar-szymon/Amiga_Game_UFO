/* copybench - which way of moving memory is fastest on a Warp 060?
 *
 * Times copy, write-only and read-only with several instruction shapes:
 * libnix memcpy/memset, exec CopyMem/CopyMemQuick, longword loops, MOVEM.L
 * blocks, MOVE16. Fast RAM -> Fast RAM and Fast RAM -> VRAM (an 8-bit RTG
 * screen's bitmap, locked). Two sizes: 256000 bytes (a 640x400 frame, more
 * than the Warp's 96 KB L2) and 16 KB (fits the L2). Best of REPS, E-clock.
 * Build: tools/vrambench/build.sh. */
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

#define REPS 8
#define BIG   256000UL
#define SMALL 16384UL

struct Library *CyberGfxBase;
struct Device *TimerBase;
static ULONG eclk;

static ULONG now_ticks(void) { struct EClockVal ev; eclk = ReadEClock(&ev); return ev.ev_lo; }
static ULONG ticks_us(ULONG dt) { return (dt / eclk) * 1000000UL + ((dt % eclk) * 1000UL) / (eclk / 1000UL); }

static UBYTE *src, *dst, *vram;      /* 16-aligned */
static ULONG n;                      /* bytes per call: a multiple of 640 and of 16 */
static ULONG reps_inner;             /* small blocks are repeated to make the time measurable */
static volatile ULONG sink;

/* ---- the shapes -------------------------------------------------------------- */
static void c_memcpy(UBYTE *d, const UBYTE *s, ULONG len) { memcpy(d, s, len); }
static void c_copymem(UBYTE *d, const UBYTE *s, ULONG len) { CopyMem((APTR)s, d, len); }
static void c_copymemq(UBYTE *d, const UBYTE *s, ULONG len) { CopyMemQuick((APTR)s, d, len); }
static void c_long(UBYTE *d, const UBYTE *s, ULONG len)
{ ULONG *dl = (ULONG *)d; const ULONG *sl = (const ULONG *)s; ULONG i; for (i = len / 4; i; i--) *dl++ = *sl++; }
static void c_long4(UBYTE *d, const UBYTE *s, ULONG len)
{ ULONG *dl = (ULONG *)d; const ULONG *sl = (const ULONG *)s; ULONG i;
  for (i = len / 16; i; i--) { dl[0] = sl[0]; dl[1] = sl[1]; dl[2] = sl[2]; dl[3] = sl[3]; dl += 4; sl += 4; } }
static void c_movem(UBYTE *d, const UBYTE *s, ULONG len)
{
	register const UBYTE *a0 __asm("a0") = s;
	register UBYTE *a1 __asm("a1") = d;
	register ULONG d0 __asm("d0") = len / 40;
	__asm__ volatile (
		"1:\n\t"
		"movem.l (%0)+,%%d1-%%d7/%%a2-%%a4\n\t"
		"movem.l %%d1-%%d7/%%a2-%%a4,(%1)\n\t"
		"lea 40(%1),%1\n\t"
		"subq.l #1,%2\n\t"
		"bne.s 1b"
		: "+a" (a0), "+a" (a1), "+d" (d0) : : "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a2", "a3", "a4", "cc", "memory");
}
static void c_move16(UBYTE *d, const UBYTE *s, ULONG len)
{
	register const UBYTE *a0 __asm("a0") = s;
	register UBYTE *a1 __asm("a1") = d;
	register ULONG d0 __asm("d0") = len / 64;
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
/* write only */
static void w_memset(UBYTE *d, const UBYTE *s, ULONG len) { (void)s; memset(d, 0x5a, len); }
static void w_long(UBYTE *d, const UBYTE *s, ULONG len)
{ ULONG *dl = (ULONG *)d; ULONG i; (void)s; for (i = len / 16; i; i--) { dl[0] = 0x5a5a5a5aUL; dl[1] = 0x5a5a5a5aUL; dl[2] = 0x5a5a5a5aUL; dl[3] = 0x5a5a5a5aUL; dl += 4; } }
static void w_movem(UBYTE *d, const UBYTE *s, ULONG len)
{
	register UBYTE *a1 __asm("a1") = d;
	register ULONG d0 __asm("d0") = len / 40;
	(void)s;
	__asm__ volatile (
		"moveq #0,%%d1\n\tmoveq #0,%%d2\n\tmoveq #0,%%d3\n\tmoveq #0,%%d4\n\tmoveq #0,%%d5\n\t"
		"moveq #0,%%d6\n\tmoveq #0,%%d7\n\tsuba.l %%a2,%%a2\n\tsuba.l %%a3,%%a3\n\tsuba.l %%a4,%%a4\n"
		"1:\n\t"
		"movem.l %%d1-%%d7/%%a2-%%a4,(%0)\n\t"
		"lea 40(%0),%0\n\t"
		"subq.l #1,%1\n\t"
		"bne.s 1b"
		: "+a" (a1), "+d" (d0) : : "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a2", "a3", "a4", "cc", "memory");
}
/* MOVE16 from one 16-byte line again and again: write-only with bursts */
static void w_move16(UBYTE *d, const UBYTE *s, ULONG len)
{
	register const UBYTE *a0 __asm("a0") = s;
	register UBYTE *a1 __asm("a1") = d;
	register ULONG d0 __asm("d0") = len / 16;
	__asm__ volatile (
		"1:\n\t"
		"move.l %0,%%d1\n\t"
		".word 0xf620,0x9000\n\t"   /* move16 (a0)+,(a1)+ */
		"move.l %%d1,%0\n\t"         /* back to the same source line */
		"subq.l #1,%2\n\t"
		"bne.s 1b"
		: "+a" (a0), "+a" (a1), "+d" (d0) : : "d1", "cc", "memory");
}
/* read only */
static void r_long(UBYTE *d, const UBYTE *s, ULONG len)
{ const ULONG *sl = (const ULONG *)s; ULONG i, a = 0; (void)d; for (i = len / 16; i; i--) { a += sl[0] + sl[1] + sl[2] + sl[3]; sl += 4; } sink = a; }
static void r_movem(UBYTE *d, const UBYTE *s, ULONG len)
{
	register const UBYTE *a0 __asm("a0") = s;
	register ULONG d0 __asm("d0") = len / 40;
	(void)d;
	__asm__ volatile (
		"1:\n\t"
		"movem.l (%0)+,%%d1-%%d7/%%a2-%%a4\n\t"
		"subq.l #1,%1\n\t"
		"bne.s 1b"
		: "+a" (a0), "+d" (d0) : : "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a2", "a3", "a4", "cc", "memory");
}

typedef void (*Fn)(UBYTE *, const UBYTE *, ULONG);

/* Results are collected here and printed only when no bitmap is locked: no
 * OS calls (printf to a console is one) while a P96 lock is held. */
static char out[8192];
static ULONG outn;
#define OUT(...) (outn += (ULONG)snprintf(out + outn, sizeof out - outn, __VA_ARGS__))

static void run(const char *name, Fn fn, UBYTE *d, const UBYTE *s)
{
	ULONG best = 0xFFFFFFFFUL, i, k;
	for (i = 0; i < REPS; i++) {
		ULONG t0 = now_ticks(), t;
		for (k = 0; k < reps_inner; k++) fn(d, s, n);
		t = ticks_us(now_ticks() - t0);
		if (t < best) best = t;
	}
	{
		ULONG total = n * reps_inner;               /* bytes moved per sample */
		ULONG us_per = best / reps_inner;           /* per call */
		OUT("  %-34s %5lu.%02lu ms  %4lu MB/s\n", name, (unsigned long)(us_per / 1000UL),
		    (unsigned long)((us_per % 1000UL) / 10UL), (unsigned long)(best ? total / best : 0UL));
	}
}

static void suite(const char *title, UBYTE *d, int vram_dst)
{
	OUT("%s\n", title);
	run("copy  memcpy (libnix)", c_memcpy, d, src);
	run("copy  CopyMem (exec)", c_copymem, d, src);
	run("copy  CopyMemQuick (exec)", c_copymemq, d, src);
	run("copy  long loop", c_long, d, src);
	run("copy  long loop x4", c_long4, d, src);
	run("copy  movem.l 40 B", c_movem, d, src);
	run("copy  MOVE16", c_move16, d, src);
	run("write memset (libnix)", w_memset, d, src);
	run("write long x4 (WriteL)", w_long, d, src);
	run("write movem.l 40 B", w_movem, d, src);
	run("write MOVE16 from one line", w_move16, d, src);
	if (!vram_dst) {
		run("read  long x4 (ReadL)", r_long, NULL, src);
		run("read  movem.l 40 B", r_movem, NULL, src);
	} else {
		run("read  long x4 (ReadL) from VRAM", r_long, NULL, d);
		run("read  movem.l 40 B from VRAM", r_movem, NULL, d);
	}
}

int main(void)
{
	struct timerequest tr;
	APTR m1, m2, h = NULL, base = NULL;
	struct Screen *scr = NULL;
	ULONG bpr = 0, modeid;

	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0) != 0) return 20;
	TimerBase = tr.tr_node.io_Device;
	CyberGfxBase = OpenLibrary((CONST_STRPTR)CYBERGFXNAME, 41);
	m1 = AllocVec(BIG + 16, MEMF_FAST | MEMF_CLEAR);
	m2 = AllocVec(BIG + 16, MEMF_FAST | MEMF_CLEAR);
	if (!CyberGfxBase || !m1 || !m2) { printf("setup failed\n"); return 20; }
	src = (UBYTE *)(((ULONG)m1 + 15) & ~15UL);
	dst = (UBYTE *)(((ULONG)m2 + 15) & ~15UL);
	{ ULONG i; for (i = 0; i < BIG; i++) src[i] = (UBYTE)(i * 7 + (i >> 9)); }

	modeid = BestCModeIDTags(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 400, CYBRBIDTG_Depth, 8, TAG_DONE);
	if (modeid != INVALID_ID)
		scr = OpenScreenTags(NULL, SA_DisplayID, modeid, SA_Width, 640, SA_Height, 400, SA_Depth, 8,
		                     SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN, TAG_DONE);

	(void)now_ticks();
	printf("Warp copy shapes, best of %d, E-clock %lu Hz (MB/s = bytes/us)\n", REPS, (unsigned long)eclk);

	n = BIG; reps_inner = 1;
	suite("== Fast -> Fast, 256000 B (a 640x400 frame, > L2)", dst, 0);
	n = SMALL; reps_inner = 16;
	suite("== Fast -> Fast, 16384 B (fits the 96 KB L2)", dst, 0);

	if (scr) {
		struct TagItem tags[] = { { LBMI_BASEADDRESS, (ULONG)&base }, { LBMI_BYTESPERROW, (ULONG)&bpr }, { TAG_DONE, 0 } };
		h = LockBitMapTagList(scr->RastPort.BitMap, tags);
		if (h && base && bpr == 640 && ((ULONG)base & 15) == 0) {
			vram = (UBYTE *)base;
			n = BIG; reps_inner = 1;
			suite("== Fast -> VRAM (screen bitmap, locked), 256000 B", vram, 1);
		} else {
			OUT("VRAM: lock gave base %p bpr %lu - skipped\n", base, (unsigned long)bpr);
		}
		if (h) UnLockBitMap(h);
		CloseScreen(scr);
	} else {
		OUT("no 640x400x8 RTG screen - VRAM tests skipped\n");
	}
	fputs(out, stdout);

	FreeVec(m1); FreeVec(m2);
	CloseLibrary(CyberGfxBase);
	CloseDevice((struct IORequest *)&tr);
	return 0;
}
