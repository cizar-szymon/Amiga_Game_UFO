/* amiga_ufomus.c - the Amiga UFO's own music (see amiga_ufomus.h).
 *
 * Followed instruction by instruction from the EaglePlayer "UFO" replayer
 * (its routines named in the comments by their offsets in that player's
 * code hunk), down to its quirks: a channel always plays on voice
 * (channel & 3); a note's key is folded into the period table by -24 and
 * +12 steps; volume envelopes are a jump to the velocity and a release of
 * rate/2 a call; pitch bend is one data byte, 16 = centre, in sixteenths of
 * two semitones. The sequencer runs at 24 ticks a beat, called twice per
 * 1/60 s, the envelopes once.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amiga_ufomus.h"
#include "amiga_music.h"      /* MUSIC_RATE */

#define NINST    40
#define NCHAN    16
#define NVOICE   4
#define NBLOCK   80   /* the block table: 320 bytes at 16(a4) */
#define CALLS_HZ 60
/* Paula's PAL clock * 65536 / MUSIC_RATE: the 16.16 step is this / period
 * (computed here, the product overflows 32 bits) */
#if MUSIC_RATE == 22050
#define STEPK 10541918UL
#else
#error "STEPK is for 22050 Hz"
#endif

/* periods per key (0xd92), with the two words before the table that key 0
 * reads as its "two semitones down" neighbour */
static const unsigned short s_per_raw[] = {
	0x28c0, 0x4e75,
	0x0fbe, 0x0eee, 0x0df7, 0x0d09, 0x0c8c, 0x0bdc, 0x0b1a, 0x0a8f, 0x09f4, 0x0969, 0x08d6, 0x0853,
	0x07df, 0x0766, 0x070a, 0x069c, 0x063a, 0x05e4, 0x058d, 0x053f, 0x04f2, 0x04ae, 0x046b, 0x0429,
	0x03ef, 0x03b7, 0x0381, 0x034e, 0x0320, 0x02f2, 0x02c8, 0x029f, 0x027b, 0x0257, 0x0235, 0x0216,
	0x01f7, 0x01db, 0x01c0, 0x01a7, 0x0190, 0x0179, 0x0164, 0x0150, 0x013d, 0x012b, 0x011a, 0x010a,
	0x00fb, 0x00ed, 0x00e0, 0x00d3, 0x00c7, 0x00bc, 0x00b2, 0x00a8, 0x009e, 0x0095, 0x008d, 0x0086,
	0x00fb, 0x00ed, 0x00e0, 0x00d3, 0x00c7, 0x00bc, 0x00b2, 0x00a8, 0x009e, 0x0095, 0x008d, 0x0086,
	0x00fb, 0x00ed, 0x00e0, 0x00d3, 0x00c7, 0x00bc, 0x00b2, 0x00a8, 0x009e, 0x0095, 0x008d, 0x0086,
	0x0000
};
#define PER(k) ((long)s_per_raw[(k) + 2])
#define MAXKEY 81   /* PER(key + 2) must stay inside the table */

typedef struct {
	const signed char *start, *loop, *end;
	int rel;                  /* release rate (byte 17) */
	int looped;
} Inst;

typedef struct {
	int prog, vol, bend;      /* bend: signed, 0 = none */
	long period;              /* the note's period before the bend */
	const unsigned char *ret; /* where a block call returns */
} Chan;

typedef struct {
	long down, up;            /* period per bend step, up and down */
	int rel;
	signed char env, vol, target;
	int midich, note;
	/* Paula */
	int on;
	const signed char *ptr;   /* playing */
	unsigned long len;        /* bytes */
	unsigned long pos;        /* 16.16 bytes */
	const signed char *nptr;  /* reloaded at the end (loop), NULL = stop */
	unsigned long nlen;
	long period;
	int hwvol;                /* 0..64 */
} Voice;

static unsigned char *s_bank;
static unsigned long s_banklen;
static Inst s_binst[NINST];   /* from the bank */
static Inst s_inst[NINST];    /* this tune's (INST chunks override) */

static unsigned char *s_mus;
static unsigned long s_muslen;
static const unsigned char *s_ptr[NCHAN], *s_start[NCHAN];
static unsigned int s_delay[NCHAN], s_idelay[NCHAN];
static unsigned char s_run[NCHAN];
static const unsigned char *s_block[NBLOCK];
static int s_nblock;
static Chan s_chan[NCHAN];
static Voice s_voice[NVOICE];
static int s_tempo;           /* ticks a second (bpm * 2/5) */
static long s_acc;
static int s_looped, s_playing, s_loop;
static int s_master = 64;
static unsigned long s_callacc;

static unsigned long rd32(const unsigned char *p)
{
	return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static int in_mus(const unsigned char *p, unsigned long n)
{
	return p >= s_mus && p + n <= s_mus + s_muslen;
}

/* ---- bank (0x358 / 0xd22) ---------------------------------------------- */

int UfoMus_HaveBank(void) { return s_bank != NULL; }

void UfoMus_FreeBank(void)
{
	UfoMus_Stop();
	free(s_bank);
	s_bank = NULL;
	s_banklen = 0;
}

int UfoMus_LoadBank(const char *path)
{
	FILE *f;
	long n;
	unsigned long off;
	int i;
	if (s_bank != NULL) return 1;
	f = fopen(path, "rb");
	if (f == NULL) return 0;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 20 + NINST * 10 + 84) { fclose(f); return 0; }
	s_bank = (unsigned char *)malloc((size_t)n);
	if (s_bank == NULL) { fclose(f); return 0; }
	if (fread(s_bank, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s_bank); s_bank = NULL; return 0; }
	fclose(f);
	s_banklen = (unsigned long)n;
	if (memcmp(s_bank, "FORM", 4) || memcmp(s_bank + 8, "ADAT", 4) || memcmp(s_bank + 12, "BODY", 4)) {
		free(s_bank); s_bank = NULL; return 0;
	}
	/* 40 records of length, loop offset and two bytes, 84 bytes more, then
	 * the samples one after another */
	off = 20 + NINST * 10 + 84;
	for (i = 0; i < NINST; i++) {
		const unsigned char *r = s_bank + 20 + i * 10;
		unsigned long len = rd32(r), loop = rd32(r + 4);
		Inst *in = &s_binst[i];
		memset(in, 0, sizeof *in);
		in->rel = r[9];
		if (len == 0) continue;
		if (off + len > s_banklen) break;
		in->start = (const signed char *)s_bank + off;
		in->end = in->start + len;
		in->looped = loop != 0;
		in->loop = in->start + loop;
		off += len;
	}
	return 1;
}

/* ---- tune (0x618) ------------------------------------------------------ */

static unsigned int readvar(const unsigned char **pp)
{
	unsigned int v = 0;
	const unsigned char *p = *pp;
	unsigned char b;
	do {
		if (!in_mus(p, 1)) { *pp = p; return 0; }
		b = *p++;
		v = ((v << 7) + (b & 0x7f)) & 0xffff;
	} while (b & 0x80);
	*pp = p;
	return v;
}

static int parse(void)
{
	const unsigned char *a = s_mus + 20;
	int i, bpm = 120;
	memset(s_ptr, 0, sizeof s_ptr);
	memset(s_start, 0, sizeof s_start);
	memset(s_delay, 0, sizeof s_delay);
	memset(s_idelay, 0, sizeof s_idelay);
	s_nblock = 0;
	memcpy(s_inst, s_binst, sizeof s_inst);
	while (in_mus(a, 10)) {
		const unsigned char *s = a + 4;
		unsigned long size = rd32(s);   /* not one for DRUM and "BPM " */
		if (size > s_muslen) size = s_muslen;
		if (!memcmp(a, "CHAN", 4)) {
			int ch = s[5] & (NCHAN - 1);
			const unsigned char *p = s + 6;
			unsigned int d = readvar(&p);
			s_delay[ch] = s_idelay[ch] = d;
			s_start[ch] = s_ptr[ch] = p;
			a = s + size + 4;
		} else if (!memcmp(a, "DRUM", 4)) {
			a = s + 84;
		} else if (!memcmp(a, "BPM ", 4)) {
			bpm = s[1];
			a = s + 2;
		} else if (!memcmp(a, "BLCK", 4)) {
			if (s_nblock < NBLOCK) s_block[s_nblock++] = s + 4;
			a = s + size;
		} else if (!memcmp(a, "INST", 4)) {
			int n = s[5];
			unsigned long len = rd32(s + 6), loop = rd32(s + 10);
			if (n < NINST && in_mus(s + 16, len)) {
				Inst *in = &s_inst[n];
				in->start = (const signed char *)s + 16;
				in->end = in->start + len;
				in->rel = s[15];
				in->looped = loop != 0;
				in->loop = in->start + loop;
			}
			a = s + size;
		} else {
			break;
		}
		if (a <= s) break;   /* a size that would not move on */
	}
	s_tempo = bpm * 24 / 60;
	s_acc = s_tempo;
	for (i = 0; i < NCHAN; i++) { memset(&s_chan[i], 0, sizeof s_chan[i]); s_chan[i].vol = 127; }
	memset(s_voice, 0, sizeof s_voice);
	for (i = 0; i < NCHAN; i++) if (s_ptr[i]) return 1;
	return 0;
}

/* ---- notes (0x7a2, 0x806) ---------------------------------------------- */

static long bent(long period, int ch, int voice)
{
	int b = s_chan[ch].bend;
	if (b > 0) return period - s_voice[voice].down * b;
	if (b < 0) return period + s_voice[voice].up * -b;
	return period;
}

static void note(int prog, int key, int vel, int ch, int voice)
{
	Voice *v;
	const Inst *in;
	int i;
	key -= 24;
	while (key < 0) key += 12;
	if (vel == 0) {
		for (i = 0; i < NVOICE; i++)
			if (s_voice[i].midich == ch && s_voice[i].note == key) {
				s_voice[i].env = (signed char)-s_voice[i].rel;
				s_voice[i].target = 0;
			}
		return;
	}
	if (key > MAXKEY || prog >= NINST) return;
	v = &s_voice[voice];
	in = &s_inst[prog];
	v->on = 0;                                /* DMACON off */
	if (in->start == NULL) return;
	v->note = key;
	v->midich = ch;
	v->env = 99;
	v->rel = in->rel;
	v->down = (PER(key) - PER(key + 2)) >> 4;
	v->up = (PER(key - 2) - PER(key)) >> 4;
	v->target = (signed char)vel;
	s_chan[ch].period = PER(key);
	v->period = bent(PER(key), ch, voice);
	v->vol = (signed char)vel;
	v->hwvol = ((vel & 127) * s_master) >> 6;
	if (v->hwvol > 64) v->hwvol = 64;
	v->ptr = in->start;
	v->len = (unsigned long)((in->end - in->start) & ~1L);
	v->pos = 0;
	v->on = v->len > 0;                       /* DMACON on */
	if (in->looped && in->end > in->loop) {
		v->nptr = in->loop;
		v->nlen = (unsigned long)((in->end - in->loop) & ~1L);
	} else {
		v->nptr = NULL;
		v->nlen = 0;
	}
}

/* ---- sequencer (0xaca, 0xa2a) ------------------------------------------- */

static unsigned int step_chan(int ch)
{
	Chan *c = &s_chan[ch];
	const unsigned char *p = s_ptr[ch];
	for (;;) {
		unsigned int d;
		unsigned char b;
		if (!in_mus(p, 1)) { s_playing = 0; return 0; }
		b = *p++;
		if (b == 0xfe) {
			int n = in_mus(p, 1) ? *p++ : 0;
			c->ret = p;
			if (n >= s_nblock) { s_playing = 0; return 0; }
			p = s_block[n];
		} else if (b == 0xfd) {
			if (c->ret == NULL) { s_playing = 0; return 0; }
			p = c->ret;
		} else if (b == 0xff) {
			s_playing = 0;
			return 0;
		} else {
			unsigned char st;
			if (b >= 0x80) {
				s_run[ch] = b;
				if (!in_mus(p, 1)) { s_playing = 0; return 0; }
				b = *p++;
			}
			st = s_run[ch] & 0xf0;
			if (st == 0x80) {
				p++;
				note(c->prog, b, 0, ch, 0);
			} else if (st == 0x90) {
				int vel = in_mus(p, 1) ? *p++ : 0;
				note(c->prog, b, (vel * c->vol) >> 8, ch, ch & 3);
			} else if (st == 0xb0) {
				int val = in_mus(p, 1) ? *p++ : 0;
				if (b == 0) s_tempo = (val * 48 / 60) & 0xff;
				else if (b == 7) c->vol = val;
			} else if (st == 0xc0) {
				if (b == 126) { s_looped = 1; return 0; }
				c->prog = b;
			} else if (st == 0xe0) {
				c->bend = (signed char)(b - 16);
				s_voice[ch & 3].period = bent(c->period, ch, ch & 3);
			}
		}
		d = readvar(&p);
		if (d != 0) { s_ptr[ch] = p; return d; }
	}
}

static void seq_tick(void)
{
	int ch;
	if (!s_playing) return;
	s_acc -= s_tempo;
	if (s_acc > 0) return;
	s_acc += 120;
	for (ch = 0; ch < NCHAN; ch++) {
		s_looped = 0;
		if (s_ptr[ch] == NULL) continue;
		if (s_delay[ch] == 0) {
			s_delay[ch] = step_chan(ch);
			if (s_looped) {
				if (!s_loop) { s_playing = 0; return; }
				memcpy(s_ptr, s_start, sizeof s_ptr);
				memcpy(s_delay, s_idelay, sizeof s_delay);
				return;
			}
		}
		s_delay[ch] = (s_delay[ch] - 1) & 0xffff;
	}
}

/* the envelopes (0x9ac) */
static void env_tick(void)
{
	int i;
	for (i = 0; i < NVOICE; i++) {
		Voice *v = &s_voice[i];
		signed char vol = (signed char)(((long)v->vol * 2 + v->env) >> 1);
		v->vol = vol;
		if (v->env < 0) {
			if (vol < 0) { v->vol = 0; v->env = 0; }
		} else if (vol > v->target) {
			v->vol = v->target;
			v->env = 0;
		}
		v->hwvol = s_playing ? (((v->vol & 127) * s_master) >> 6) : 0;
		if (v->hwvol > 64) v->hwvol = 64;
	}
}

/* ---- public ------------------------------------------------------------ */

int UfoMus_Play(const unsigned char *mus, unsigned long len, int loop)
{
	UfoMus_Stop();
	if (s_bank == NULL || len < 28 || memcmp(mus, "FORM", 4) || memcmp(mus + 8, "DDAT", 4) ||
	    memcmp(mus + 12, "BODY", 4))
		return 0;
	s_mus = (unsigned char *)malloc(len);
	if (s_mus == NULL) return 0;
	memcpy(s_mus, mus, len);
	s_muslen = len;
	if (!parse()) { UfoMus_Stop(); return 0; }
	s_loop = loop;
	s_callacc = 0;
	s_playing = 1;
	return 1;
}

void UfoMus_Stop(void)
{
	s_playing = 0;
	memset(s_voice, 0, sizeof s_voice);
	free(s_mus);
	s_mus = NULL;
	s_muslen = 0;
}

int UfoMus_Playing(void) { return s_playing; }

void UfoMus_SetVolume(int vol)
{
	s_master = vol < 0 ? 0 : vol > 64 ? 64 : vol;
}

int UfoMus_Refill(void *ud, signed char *dst, int max)
{
	int n;
	(void)ud;
	if (!s_playing) return 0;
	for (n = 0; n < max; n++) {
		long sum = 0;
		int i;
		s_callacc += CALLS_HZ;
		if (s_callacc >= MUSIC_RATE) {   /* the replayer's 1/60 s (0xaa0) */
			s_callacc -= MUSIC_RATE;
			seq_tick();
			seq_tick();
			env_tick();
			if (!s_playing) break;
		}
		for (i = 0; i < NVOICE; i++) {
			Voice *v = &s_voice[i];
			if (!v->on || v->period <= 0) continue;
			sum += (long)v->ptr[v->pos >> 16] * v->hwvol;
			v->pos += STEPK / (unsigned long)v->period;
			while ((v->pos >> 16) >= v->len) {
				if (v->nptr == NULL || v->nlen == 0) { v->on = 0; break; }
				v->pos -= v->len << 16;
				v->ptr = v->nptr;
				v->len = v->nlen;
			}
		}
		sum >>= 7;   /* four voices at 64: twice the 8-bit range, kept by the clamp */
		dst[n] = (signed char)(sum > 127 ? 127 : sum < -128 ? -128 : sum);
	}
	return n;
}

const char *UfoMus_FileFor(int catpos)
{
	switch (catpos) {
	case 0: case 18: return "Geo.mus";
	case 2:  return "Inter.mus";
	case 3:  return "Defend.mus";
	case 6:  return "EnBase.mus";
	case 8:  return "NewMars.mus";
	case 9:  return "Mars.mus";
	case 10: return "Lose.mus";
	case 11: return "Win.mus";
	case 12: return "Story.mus";
	default: return NULL;
	}
}

#ifdef UFOMUS_HOST_TEST
/* cc -DUFOMUS_HOST_TEST -o ufomus amiga_ufomus.c; ./ufomus UFO.bank Geo.mus 60 out.raw */
#include <stdio.h>
int main(int argc, char **argv)
{
	FILE *f;
	unsigned char *m;
	long n, secs, s, total = 0;
	if (argc < 5) return 1;
	if (!UfoMus_LoadBank(argv[1])) { printf("bank failed\n"); return 1; }
	f = fopen(argv[2], "rb");
	if (!f) return 1;
	fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
	m = malloc(n); fread(m, 1, n, f); fclose(f);
	if (!UfoMus_Play(m, n, 0)) { printf("tune failed\n"); return 1; }
	secs = atol(argv[3]);
	f = fopen(argv[4], "wb");
	for (s = 0; s < secs; s++) {
		signed char buf[MUSIC_RATE];
		int got = UfoMus_Refill(0, buf, MUSIC_RATE), i, peak = 0, clip = 0;
		double e = 0;
		for (i = 0; i < got; i++) {
			int a = buf[i] < 0 ? -buf[i] : buf[i];
			if (a > peak) peak = a;
			if (a >= 127) clip++;
			e += (double)buf[i] * buf[i];
		}
		fwrite(buf, 1, got, f);
		total += got;
		printf("%3ld s: peak %3d rms %5.1f clipped %d\n", s, peak, got ? __builtin_sqrt(e / got) : 0.0, clip);
		if (got < MUSIC_RATE) break;
	}
	fclose(f);
	printf("%ld samples, tempo %d ticks/s\n", total, s_tempo);
	return 0;
}
#endif
