/*
 * Rectangle copy, row by row, for the 640x400 RTG frame and everything that
 * moves pixels in rows (the flip to the screen, sdlmini's plain blits, the
 * battlescape frame cache).
 *
 * Measured on a Warp 060 (tools/vrambench in the porting notes), 256000
 * bytes as 400 rows of 640:
 *     memcpy per row, Fast -> VRAM         7.9 ms   32 MB/s
 *     MOVE16 per row,  Fast -> VRAM        4.7 ms   54 MB/s
 *     one memcpy of the whole block        4.6 ms   55 MB/s
 * A large aligned memcpy is as fast as MOVE16 (the system's CopyMem, which
 * libnix's memcpy ends up in, already bursts); what is slow is one memcpy
 * call per 640-byte row. So: rows that follow each other in memory are one
 * memcpy; otherwise each row's 16-byte-aligned middle goes by MOVE16 on a
 * 68040/68060 and the ragged ends by memcpy. A 68020/030 has no MOVE16 and
 * keeps the plain per-row memcpy.
 *
 * MOVE16 reads and writes memory past the data cache. With the cache in
 * copyback mode a line the CPU has just written may not be in memory yet,
 * and a stale line of the destination could later be pushed over the copy.
 * Both ranges are therefore pushed (CacheClearE) before the copy.
 */
#include <string.h>

#include <exec/execbase.h>
#include <proto/exec.h>

#include "amiga_copy.h"

#ifndef AFF_68060
#define AFF_68060 (1L << 7)
#endif

static int s_move16 = -1;   /* -1 not decided yet */

int amiga_copy_move16(void)
{
	if (s_move16 < 0) {
		UWORD attn = SysBase->AttnFlags;
		s_move16 = (attn & (AFF_68040 | AFF_68060)) ? 1 : 0;
	}
	return s_move16;
}

/* n lines of 16 bytes, both pointers 16-aligned */
static void move16_lines(void *dst, const void *src, unsigned long n)
{
	register const void *a0 __asm("a0") = src;
	register void *a1 __asm("a1") = dst;
	register unsigned long d0 __asm("d0") = n;
	__asm__ volatile (
		"1:\n\t"
		".word 0xf620,0x9000\n\t"   /* move16 (a0)+,(a1)+ - as data, so a 68020 */
		"subq.l #1,%2\n\t"          /* assembler takes it; only run on 040/060 */
		"bne.s 1b"
		: "+a" (a0), "+a" (a1), "+d" (d0) : : "cc", "memory");
}

void amiga_copy_push(const void *p, long pitch, long w, long h)
{
	if (w > 0 && h > 0 && amiga_copy_move16())
		CacheClearE((APTR)p, (ULONG)(pitch * (h - 1) + w), CACRF_ClearD);
}

static int s_nopush;   /* amiga_copy_nopush: the caller pushed everything already */

void amiga_copy_nopush(int on)
{
	s_nopush = on;
}

static void copy_rows(void *dst, long dpitch, const void *src, long spitch, long w, long h, int push)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	if (s_nopush) push = 0;
	long y;

	if (w <= 0 || h <= 0) return;
	if (dpitch == w && spitch == w) {          /* one block */
		memcpy(d, s, (size_t)w * (size_t)h);
		return;
	}
	/* MOVE16 pays off only on whole lines in the middle of a row, and needs
	 * source and destination at the same offset within a 16-byte line */
	if (w < 64 || !amiga_copy_move16() ||
	    ((((unsigned long)d ^ (unsigned long)s) & 15) != 0) ||
	    (((unsigned long)dpitch | (unsigned long)spitch) & 15) != 0) {
		for (y = 0; y < h; y++) {
			memcpy(d, s, (size_t)w);
			d += dpitch;
			s += spitch;
		}
		return;
	}
	{
		/* every row has the same head/middle/tail split (pitches are
		 * multiples of 16, so all rows share the first row's alignment) */
		const unsigned long head = (16 - ((unsigned long)s & 15)) & 15;
		const unsigned long lines = ((unsigned long)w - head) >> 4;
		const unsigned long tail = (unsigned long)w - head - (lines << 4);

		if (push) {
			CacheClearE((APTR)s, (ULONG)(spitch * (h - 1) + w), CACRF_ClearD);
			CacheClearE((APTR)d, (ULONG)(dpitch * (h - 1) + w), CACRF_ClearD);
		}
		for (y = 0; y < h; y++) {
			if (head) memcpy(d, s, head);
			move16_lines(d + head, s + head, lines);
			if (tail) memcpy(d + head + (lines << 4), s + head + (lines << 4), tail);
			d += dpitch;
			s += spitch;
		}
	}
}

void amiga_copy_rows(void *dst, long dpitch, const void *src, long spitch, long w, long h)
{
	copy_rows(dst, dpitch, src, spitch, w, h, 1);
}

void amiga_copy_rows_pushed(void *dst, long dpitch, const void *src, long spitch, long w, long h)
{
	copy_rows(dst, dpitch, src, spitch, w, h, 0);
}

/* n lines of 16 bytes, both pointers 16-aligned, from the LAST line down -
 * for an overlapping move to a higher address */
static void move16_lines_down(void *dst, const void *src, unsigned long n)
{
	register const void *a0 __asm("a0") = (const unsigned char *)src + ((n - 1) << 4);
	register void *a1 __asm("a1") = (unsigned char *)dst + ((n - 1) << 4);
	register unsigned long d0 __asm("d0") = n;
	__asm__ volatile (
		"1:\n\t"
		".word 0xf620,0x9000\n\t"   /* move16 (a0)+,(a1)+ */
		"lea -32(%0),%0\n\t"
		"lea -32(%1),%1\n\t"
		"subq.l #1,%2\n\t"
		"bne.s 1b"
		: "+a" (a0), "+a" (a1), "+d" (d0) : : "cc", "memory");
}

/* memmove for big overlapping blocks (the battlescape scroll shifts a whole
 * 640x400 picture inside its own buffer). libnix's memmove copies an
 * overlapping block upwards byte by byte. Here: MOVE16 when the distance is a
 * multiple of 16 (every vertical scroll step - rows are 640 bytes), else
 * longwords with the destination aligned (the 68020+ reads the unaligned
 * source itself). Each unit is read before it is written and the copy runs
 * away from the overlap, so any distance is safe. */
void amiga_move(void *dst, const void *src, unsigned long n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;

	if (n == 0 || d == s) return;
	if (d + n <= s || s + n <= d) {               /* no overlap */
		memcpy(d, s, n);
		return;
	}
	if (d < s) {                                  /* downwards: from the start */
		unsigned long dist = (unsigned long)(s - d);
		if ((dist & 15) == 0 && n >= 64 && amiga_copy_move16()) {
			unsigned long head = (16 - ((unsigned long)d & 15)) & 15, lines;
			CacheClearE((APTR)d, (ULONG)(dist + n), CACRF_ClearD);
			while (head--) { *d++ = *s++; n--; }
			lines = n >> 4;
			move16_lines(d, s, lines);
			d += lines << 4; s += lines << 4; n &= 15;
			while (n--) *d++ = *s++;
			return;
		}
		while (n && ((unsigned long)d & 3)) { *d++ = *s++; n--; }
		{
			ULONG *dl = (ULONG *)d;
			const ULONG *sl = (const ULONG *)s;
			unsigned long k = n >> 4;
			while (k--) { dl[0] = sl[0]; dl[1] = sl[1]; dl[2] = sl[2]; dl[3] = sl[3]; dl += 4; sl += 4; }
			k = (n >> 2) & 3;
			while (k--) *dl++ = *sl++;
			d = (unsigned char *)dl; s = (const unsigned char *)sl; n &= 3;
		}
		while (n--) *d++ = *s++;
		return;
	}
	/* upwards: from the end */
	{
		unsigned long dist = (unsigned long)(d - s);
		unsigned char *de = d + n;
		const unsigned char *se = s + n;
		if ((dist & 15) == 0 && n >= 64 && amiga_copy_move16()) {
			unsigned long tail = (unsigned long)de & 15, lines;
			CacheClearE((APTR)s, (ULONG)(dist + n), CACRF_ClearD);
			while (tail--) { *--de = *--se; n--; }
			lines = n >> 4;
			move16_lines_down(de - (lines << 4), se - (lines << 4), lines);
			de -= lines << 4; se -= lines << 4; n &= 15;
			while (n--) *--de = *--se;
			return;
		}
		while (n && ((unsigned long)de & 3)) { *--de = *--se; n--; }
		{
			ULONG *dl = (ULONG *)de;
			const ULONG *sl = (const ULONG *)se;
			unsigned long k = n >> 4;
			while (k--) { dl -= 4; sl -= 4; dl[3] = sl[3]; dl[2] = sl[2]; dl[1] = sl[1]; dl[0] = sl[0]; }
			k = (n >> 2) & 3;
			while (k--) *--dl = *--sl;
			de = (unsigned char *)dl; se = (const unsigned char *)sl; n &= 3;
		}
		while (n--) *--de = *--se;
	}
}
