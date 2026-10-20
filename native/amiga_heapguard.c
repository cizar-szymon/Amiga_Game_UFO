/*
 * amiga_heapguard.c - a checking allocator for hunting memory corruption on
 * the real machine (build with AMIGA_HEAPGUARD=1; binaries get "-hg").
 *
 * Replaces libnix's malloc/free/calloc/realloc (their one object, malloc.o,
 * is then never linked). Every block comes from an exec memory pool with a
 * header in front (magic, size, allocation site, list links, serial) and an 8-byte
 * pattern behind it. Freed blocks are filled with 0xDD and kept in a
 * quarantine ring for a while before they really go, so a write into freed
 * memory shows up as a damaged pattern. amiga_heapguard_check(), called once
 * per frame from SDL_Flip, walks every live block: a header or tail that is
 * not intact is logged with the block, its allocation site and the live
 * block just below it in memory (the usual culprit is whoever runs off the
 * end of that one). Sites are return addresses; the log names amiga_trap_land
 * so they can be mapped with nm (tools/hangmap.py does the same for stacks).
 *
 * It is slow - a walk over all blocks each frame - and found the 64000-byte
 * write into the freed 320x200 back buffer (sdlmini skip_forget, 2026-10-20)
 * on the first run. A diagnostic, never a release build.
 */
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>

extern void SDLmini_Log(const char *msg);
extern void amiga_trap_land(void);

#define HG_LIVE   0x4C495645UL   /* 'LIVE' */
#define HG_DEAD   0x44454144UL   /* 'DEAD' */
#define HG_TAIL   8
#define HG_QN     4096           /* freed blocks held back */
#define HG_QBYTES (24UL << 20)   /* ... or this many bytes of them */

typedef struct HgHdr {
	ULONG magic;
	ULONG size;
	void *site;
	struct HgHdr *prev, *next;
	ULONG serial;
} HgHdr;                         /* 24 bytes: user data stays 8-aligned */

static HgHdr s_list;             /* circular, sentinel */
static struct SignalSemaphore s_sem;
static int s_ready;
static unsigned long s_serial, s_live, s_liveBytes, s_reports, s_frame;
static HgHdr *s_q[HG_QN];
static void *s_qfree[HG_QN];     /* who freed it */
static int s_qhead, s_qn;
static unsigned long s_qbytes;
static char s_msg[8][200];
static int s_nmsg;

static const UBYTE s_tailpat[HG_TAIL] = { 0xFE, 0xED, 0xFA, 0xCE, 0xFE, 0xED, 0xFA, 0xCE };

/* Blocks come from an exec memory pool, as libnix's own do: AllocVec per
 * block fragmented exec's free list with the few hundred thousand small
 * allocations of the ruleset load, and every AllocMem then walked it - the
 * load crawled. Pool memory goes back to the system with the pool at exit. */
static APTR s_pool;

static void hg_exit(void)
{
	if (s_pool) { DeletePool(s_pool); s_pool = NULL; }
}

static void hg_init(void)
{
	if (s_ready) return;
	InitSemaphore(&s_sem);
	s_list.next = s_list.prev = &s_list;
	s_pool = CreatePool(MEMF_ANY, 64 * 1024, 16 * 1024);
	s_ready = 1;
}

/* libnix's startup calls these (they lived in its malloc.o) */
void __initmalloc(void) { hg_init(); }
void __exitmalloc(void) { hg_exit(); }

static APTR hg_get(ULONG n)
{
	ULONG *p = (ULONG *)AllocPooled(s_pool, n + 8);
	if (p == NULL) return NULL;
	p[0] = n + 8;              /* FreePooled needs the size back */
	return p + 2;              /* 8-byte aligned like the pool */
}

static void hg_put(APTR q)
{
	ULONG *p = (ULONG *)q - 2;
	FreePooled(s_pool, p, p[0]);
}

static void note(const char *fmt, unsigned long a, unsigned long b, unsigned long c, unsigned long d, unsigned long e)
{
	if (s_nmsg < 8) snprintf(s_msg[s_nmsg++], sizeof s_msg[0], fmt, a, b, c, d, e);
}

static void flush_notes(void)
{
	int i, n = s_nmsg;
	s_nmsg = 0;
	for (i = 0; i < n; i++) SDLmini_Log(s_msg[i]);
}

static UBYTE *user(HgHdr *h) { return (UBYTE *)(h + 1); }

static int tail_ok(HgHdr *h)
{
	return memcmp(user(h) + h->size, s_tailpat, HG_TAIL) == 0;
}

/* the live block whose end is closest below p */
static HgHdr *below(const void *p)
{
	HgHdr *h, *best = NULL;
	for (h = s_list.next; h != &s_list; h = h->next)
		if ((const void *)h < p && (best == NULL || h > best)) best = h;
	return best;
}

static void really_free(HgHdr *h, void *freer)
{
	/* the quarantine pattern must have survived */
	ULONG i;
	UBYTE *u = user(h);
	for (i = 0; i < h->size; i++) if (u[i] != 0xDD) break;
	if (i < h->size && s_reports < 200) {
		s_reports++;
		note("heapguard: WRITE AFTER FREE in %lu-byte block %08lx at +%lu (allocated at %08lx, freed at %08lx)",
		     h->size, (unsigned long)u, i, (unsigned long)h->site, (unsigned long)freer);
	}
	h->magic = 0;
	hg_put(h);
}

void *hg_alloc(size_t n, void *site)
{
	HgHdr *h;
	hg_init();
	ObtainSemaphore(&s_sem);   /* exec pools are not shared-safe */
	h = (HgHdr *)hg_get(sizeof(HgHdr) + n + HG_TAIL);
	ReleaseSemaphore(&s_sem);
	if (h == NULL) return NULL;
	h->magic = HG_LIVE;
	h->size = (ULONG)n;
	h->site = site;
	memcpy(user(h) + n, s_tailpat, HG_TAIL);
	ObtainSemaphore(&s_sem);
	h->serial = ++s_serial;
	h->next = s_list.next; h->prev = &s_list;
	s_list.next->prev = h; s_list.next = h;
	s_live++; s_liveBytes += n;
	ReleaseSemaphore(&s_sem);
	return user(h);
}

void hg_free(void *p, void *site)
{
	HgHdr *h;
	if (p == NULL) return;
	hg_init();
	h = (HgHdr *)p - 1;
	ObtainSemaphore(&s_sem);
	if (h->magic != HG_LIVE) {
		if (s_reports < 200) {
			s_reports++;
			note(h->magic == HG_DEAD ? "heapguard: DOUBLE FREE of %08lx at %08lx (magic %08lx) [%lu %lu]"
			                         : "heapguard: FREE OF A NON-BLOCK %08lx at %08lx (magic %08lx) [%lu %lu]",
			     (unsigned long)p, (unsigned long)site, h->magic, 0, 0);
		}
		ReleaseSemaphore(&s_sem);
		flush_notes();
		return;
	}
	if (!tail_ok(h) && s_reports < 200) {
		HgHdr *b = below(h);
		s_reports++;
		note("heapguard: OVERRUN at free of %lu-byte block %08lx (allocated at %08lx, freed at %08lx) serial %lu",
		     h->size, (unsigned long)p, (unsigned long)h->site, (unsigned long)site, h->serial);
		(void)b;
	}
	h->prev->next = h->next; h->next->prev = h->prev;
	s_live--; s_liveBytes -= h->size;
	h->magic = HG_DEAD;
	memset(user(h), 0xDD, h->size);
	/* into the quarantine; the oldest one leaves */
	if (s_qn == HG_QN || s_qbytes + h->size > HG_QBYTES) {
		while (s_qn > 0 && (s_qn == HG_QN || s_qbytes + h->size > HG_QBYTES)) {
			int t = (s_qhead - s_qn + HG_QN) % HG_QN;
			s_qbytes -= s_q[t]->size;
			really_free(s_q[t], s_qfree[t]);
			s_qn--;
		}
	}
	s_q[s_qhead] = h; s_qfree[s_qhead] = site;
	s_qhead = (s_qhead + 1) % HG_QN; s_qn++;
	s_qbytes += h->size;
	ReleaseSemaphore(&s_sem);
	flush_notes();
}

void *malloc(size_t n) { return hg_alloc(n, __builtin_return_address(0)); }
void free(void *p) { hg_free(p, __builtin_return_address(0)); }

void *calloc(size_t a, size_t b)
{
	size_t n = a * b;
	void *p = hg_alloc(n, __builtin_return_address(0));
	if (p) memset(p, 0, n);
	return p;
}

void *realloc(void *p, size_t n)
{
	void *q;
	HgHdr *h;
	if (p == NULL) return hg_alloc(n, __builtin_return_address(0));
	if (n == 0) { hg_free(p, __builtin_return_address(0)); return NULL; }
	h = (HgHdr *)p - 1;
	q = hg_alloc(n, __builtin_return_address(0));
	if (q == NULL) return NULL;
	if (h->magic == HG_LIVE) memcpy(q, p, h->size < n ? h->size : n);
	hg_free(p, __builtin_return_address(0));
	return q;
}

/* once per frame: every live block intact? */
void amiga_heapguard_check(const char *where)
{
	HgHdr *h;
	unsigned long n = 0;
	static int s_said;
	if (!s_ready) return;
	s_frame++;
	if (!s_said) {
		char b[160];
		s_said = 1;
		snprintf(b, sizeof b, "heapguard: on - textbase: amiga_trap_land is at 0x%08lx", (unsigned long)&amiga_trap_land);
		SDLmini_Log(b);
	}
	ObtainSemaphore(&s_sem);
	for (h = s_list.next; h != &s_list && n < 2000000UL; h = h->next, n++) {
		if (h->magic != HG_LIVE || h->next->prev != h || !tail_ok(h)) {
			if (s_reports < 200) {
				HgHdr *b = below(h);
				s_reports++;
				note("heapguard: DAMAGED %lu-byte block %08lx (magic %08lx, allocated at %08lx) found at frame %lu",
				     h->magic == HG_LIVE ? h->size : 0, (unsigned long)user(h), h->magic, (unsigned long)h->site, s_frame);
				if (b) note("heapguard:   the live block below it: %lu bytes at %08lx, allocated at %08lx, serial %lu [%lu]",
				            b->size, (unsigned long)user(b), (unsigned long)b->site, b->serial, 0);
				note("heapguard:   checked in %s", (unsigned long)0, 0, 0, 0, 0);
			}
			/* do not keep walking a list that may be broken here */
			break;
		}
	}
	ReleaseSemaphore(&s_sem);
	if (s_nmsg) {
		/* "%s" cannot go through note(): put the name in now */
		int i;
		for (i = 0; i < s_nmsg; i++)
			if (strstr(s_msg[i], "checked in")) snprintf(s_msg[i], sizeof s_msg[0], "heapguard:   checked at %s", where);
		flush_notes();
	}
	if ((s_frame % 500) == 0) {
		char b[160];
		snprintf(b, sizeof b, "heapguard: frame %lu, %lu live blocks, %lu KB, %lu in quarantine, %lu reports",
		         s_frame, s_live, s_liveBytes >> 10, (unsigned long)s_qn, s_reports);
		SDLmini_Log(b);
	}
}
