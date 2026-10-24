/*
 * AMIGA-PORT: the UFO intro (data/UFO/UFOINTRO/UFOINT.FLI) played WHILE the
 * game loads, in place of the loading picture (the player's design, 2026-10-24).
 *
 * Loading takes ~30 s on a Warp 060 and the intro ~90 s; played one after the
 * other that was a progress bar, a black pause and only then the intro. Here a
 * DOS process of its own decodes the FLI and writes each frame straight into
 * the screen's bitmap (amigagfx_intro_put, the way the save/load box draws),
 * so the game task loads undisturbed but for the decoding time.
 *
 *  - The pace and the sound are the game's own: VideoState.cpp's
 *    introSoundTrack (frame -> effect, tune or frame speed) and its sound
 *    sets (INTRO.CAT and/or SAMPLE3.CAT, the "hybrid" set when both exist).
 *    The tunes GMINTRO1..3 are the streams the port pre-renders to
 *    user/music/<NAME>.raw (8-bit signed, MUSIC_RATE mono).
 *  - Esc or a mouse button (seen by an input handler, only while our screen
 *    is in front) stops the sound and jumps to the last picture - the UFO
 *    title - which stays, with "LOADING nn %" in its corner, until the game
 *    is loaded. The same when the intro ends before the load does.
 *  - When loading is done first, the intro plays on; AmigaSplash_End waits
 *    for it (amiga_intro_end) and fades it out.
 *
 * Sound: the game has opened audio.device before this starts. Its requests
 * reply to a port of the game task, which another task must never wait on,
 * so the game CLOSES it for the intro (AmigaAudio_Close - nothing plays
 * while loading) and this process opens audio.device for itself; afterwards
 * the game opens it again. Effects play on Paula 0/1, the tune on 2+3.
 *
 * Memory: the file, the frame and the effects (Chip) are allocated in the
 * game task (amiga_intro_begin) and freed there (amiga_intro_end) once the
 * process is gone; the process only allocates its own audio requests.
 */
#include <string.h>
#include <stdio.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/audio.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

#include "amiga_gfx.h"
#include "amiga_audio.h"
#include "amiga_uclock.h"

#define INTRO_W 320
#define INTRO_H 200
#define INTRO_DIR "PROGDIR:data/UFO/UFOINTRO/"
#define SOUND_DIR "PROGDIR:data/UFO/SOUND/"
#define MUS_RATE  22050          /* amiga_music.h MUSIC_RATE: the .raw files */

/* ---------------------------------------------- VideoState.cpp's tables -- */

/* introSoundTrack: frame -> 0x00..0x19 effect, 0x200+n tune GMINTRO<n+1>,
 * 0x4nn frame speed nn (1/70 s) */
static const unsigned short s_track[][2] = {
	{ 0, 0x200 }, { 149, 0x011 }, { 173, 0x00C }, { 183, 0x00E }, { 205, 0x015 }, { 211, 0x201 },
	{ 211, 0x407 }, { 223, 0x007 }, { 250, 0x001 }, { 253, 0x001 }, { 255, 0x001 },
	{ 257, 0x001 }, { 260, 0x001 }, { 261, 0x003 }, { 262, 0x001 }, { 264, 0x001 },
	{ 268, 0x001 }, { 270, 0x001 }, { 272, 0x005 }, { 272, 0x001 }, { 274, 0x001 },
	{ 278, 0x001 }, { 280, 0x001 }, { 282, 0x008 }, { 282, 0x001 }, { 284, 0x001 },
	{ 286, 0x001 }, { 288, 0x001 }, { 290, 0x001 }, { 292, 0x006 }, { 292, 0x001 },
	{ 296, 0x001 }, { 298, 0x001 }, { 300, 0x001 }, { 302, 0x001 }, { 304, 0x001 },
	{ 306, 0x001 }, { 308, 0x001 }, { 310, 0x001 }, { 312, 0x001 }, { 378, 0x202 },
	{ 378, 0x009 }, { 386, 0x009 }, { 393, 0x009 }, { 399, 0x017 }, { 433, 0x017 },
	{ 463, 0x012 }, { 477, 0x012 }, { 487, 0x013 }, { 495, 0x016 }, { 501, 0x016 },
	{ 512, 0x00D }, { 514, 0x00D }, { 522, 0x00B }, { 523, 0x00D }, { 525, 0x00D },
	{ 534, 0x018 }, { 535, 0x405 }, { 560, 0x407 }, { 577, 0x014 }, { 582, 0x405 },
	{ 582, 0x019 }, { 613, 0x407 }, { 615, 0x010 }, { 635, 0x014 }, { 638, 0x014 },
	{ 639, 0x014 }, { 644, 0x002 }, { 646, 0x002 }, { 648, 0x002 }, { 650, 0x002 },
	{ 652, 0x002 }, { 654, 0x002 }, { 656, 0x002 }, { 658, 0x002 }, { 660, 0x002 },
	{ 662, 0x002 }, { 664, 0x002 }, { 666, 0x002 }, { 668, 0x401 }, { 681, 0x406 },
	{ 687, 0x402 }, { 689, 0x407 }, { 694, 0x00A }, { 711, 0x407 }, { 711, 0x000 },
	{ 714, 0x000 }, { 716, 0x004 }, { 717, 0x000 }, { 720, 0x000 }, { 723, 0x000 },
	{ 726, 0x005 }, { 726, 0x000 }, { 729, 0x000 }, { 732, 0x000 }, { 735, 0x000 },
	{ 738, 0x000 }, { 741, 0x000 }, { 742, 0x006 }, { 744, 0x000 }, { 747, 0x000 },
	{ 750, 0x000 }, { 753, 0x000 }, { 756, 0x000 }, { 759, 0x000 }, { 762, 0x000 },
	{ 765, 0x000 }, { 768, 0x000 }, { 771, 0x000 }, { 774, 0x000 }, { 777, 0x000 },
	{ 780, 0x000 }, { 783, 0x000 }, { 786, 0x000 }, { 790, 0x015 }, { 790, 0x015 },
	{ 807, 0x002 }, { 810, 0x002 }, { 812, 0x002 }, { 814, 0x002 }, { 816, 0x000 },
	{ 819, 0x000 }, { 822, 0x000 }, { 824, 0x40A }, { 824, 0x005 }, { 827, 0x006 },
	{ 835, 0x00F }, { 841, 0x00F }, { 845, 0x00F }, { 855, 0x407 }, { 879, 0x00C },
	{ 65535, 0xFFFF }
};

#define N_FX 26
enum { CAT_INTRO, CAT_SAMPLE3 };
/* {cat, index, volume 0..128}: hybridIntroSounds, introCatOnlySounds,
 * sample3CatOnlySounds - tried in that order, as AudioSequence does */
static const unsigned char s_sets[3][N_FX][3] = {
	{ {1,24,32}, {1,5,32}, {1,23,32}, {0,3,32}, {0,4,64}, {0,5,64}, {0,6,64}, {0,7,32}, {1,8,32},
	  {1,11,32}, {1,4,32}, {0,11,32}, {1,19,48}, {0,13,32}, {0,14,32}, {1,30,32}, {1,21,32},
	  {0,17,64}, {1,13,32}, {1,14,32}, {1,19,64}, {0,21,32}, {1,15,128}, {1,12,32}, {1,18,32},
	  {1,20,32} },
	{ {0,0,32}, {0,1,32}, {0,2,32}, {0,3,32}, {0,4,32}, {0,5,32}, {0,6,32}, {0,7,32}, {0,8,32},
	  {0,9,32}, {0,10,32}, {0,11,32}, {0,12,32}, {0,13,32}, {0,14,32}, {0,15,32}, {0,16,32},
	  {0,17,32}, {0,18,32}, {0,19,32}, {0,20,32}, {0,21,32}, {0,22,32}, {0,23,32}, {0,24,32},
	  {0,24,32} },
	{ {1,24,32}, {1,5,32}, {1,23,32}, {1,6,32}, {1,9,64}, {1,7,64}, {1,27,64}, {1,4,32}, {1,8,32},
	  {1,11,32}, {1,4,32}, {0,11,32}, {1,19,48}, {0,13,32}, {1,2,32}, {1,30,32}, {1,21,32},
	  {1,0,64}, {1,13,32}, {1,14,32}, {1,19,64}, {1,3,32}, {1,15,128}, {1,12,32}, {1,18,32},
	  {1,20,32} }
};

/* ---------------------------------------------------------------- state -- */

enum { INTRO_OFF, INTRO_PLAYING, INTRO_LAST, INTRO_GONE };

static UBYTE *s_file;
static ULONG s_size;
static UBYTE *s_pix;
static UBYTE s_pal[768];
static volatile int s_state = INTRO_OFF;
static volatile int s_quit, s_skip, s_running;
static volatile int s_loadpct = -1;        /* AmigaSplash_Progress, for "LOADING" */
static volatile int s_loaded;              /* the game is loaded: no "LOADING" any more */
static int s_frames, s_speed;
static ULONG s_efreq;

/* the effects, ready for Paula (Chip, 8-bit signed) */
static BYTE *s_fx[N_FX];
static ULONG s_fxlen[N_FX];
static UWORD s_fxper[N_FX];
static UBYTE s_fxvol[N_FX];
static BPTR s_tune[3];                      /* GMINTRO1..3.raw */
static int s_lent;                          /* the game's audio closed for us */

/* from the game (Screen.cpp, with the splash style) */
static int s_mute, s_music, s_vol = 128;
static char s_userdir[128] = "user/";

void AmigaIntro_Settings(int mute, int music, int musicVol, int soundVol, const char *userFolder)
{
	s_mute = mute;
	s_music = music;
	s_vol = musicVol > soundVol ? musicVol : soundVol;   /* as VideoState: the louder */
	if (userFolder != NULL) {
		strncpy(s_userdir, userFolder, sizeof s_userdir - 1);
		s_userdir[sizeof s_userdir - 1] = 0;
	}
}

static UWORD rd16(const UBYTE *p) { return (UWORD)(p[0] | (p[1] << 8)); }
static ULONG rd32(const UBYTE *p) { return (ULONG)p[0] | ((ULONG)p[1] << 8) | ((ULONG)p[2] << 16) | ((ULONG)p[3] << 24); }

/* Milliseconds from the E-clock, read here and not through amiga_uclock_us:
 * that one keeps state of its own and belongs to the game task. TimerBase is
 * opened by amiga_uclock.c (amiga_uclock_freq() in amiga_intro_begin). */
static ULONG intro_ms(void)
{
	struct EClockVal ev;
	unsigned long long t;
	ReadEClock(&ev);
	t = ((unsigned long long)ev.ev_hi << 32) | ev.ev_lo;
	return (ULONG)((t * 1000ULL) / s_efreq);
}

/* ------------------------------------------------------------ decoding -- */

/* one chunk of a frame; returns 1 when the palette changed */
static int chunk(const UBYTE *c, const UBYTE *end, int type)
{
	UBYTE *pix = s_pix;
	int x, y, n, i;

	switch (type) {
	case 4:                                   /* COLOR_256 */
	case 11: {                                /* COLOR_64  */
		int shift = type == 4 ? 0 : 2, packets = rd16(c), ci = 0;
		c += 2;
		while (packets-- > 0 && c + 2 <= end) {
			ci += c[0];
			n = c[1] ? c[1] : 256;
			c += 2;
			for (i = 0; i < n && ci < 256 && c + 3 <= end; i++, ci++, c += 3) {
				s_pal[ci * 3 + 0] = (UBYTE)(c[0] << shift);
				s_pal[ci * 3 + 1] = (UBYTE)(c[1] << shift);
				s_pal[ci * 3 + 2] = (UBYTE)(c[2] << shift);
			}
		}
		return 1;
	}
	case 13:                                  /* BLACK */
		memset(pix, 0, INTRO_W * INTRO_H);
		break;
	case 16:                                  /* COPY */
		if (c + INTRO_W * INTRO_H <= end) memcpy(pix, c, INTRO_W * INTRO_H);
		break;
	case 15:                                  /* BRUN */
		for (y = 0; y < INTRO_H && c < end; y++) {
			UBYTE *row = pix + y * INTRO_W;
			c++;                                  /* packet count: unused */
			for (x = 0; x < INTRO_W && c < end; ) {
				n = (signed char)*c++;
				if (n > 0) {
					if (x + n > INTRO_W) n = INTRO_W - x;
					memset(row + x, *c++, n);
				} else {
					n = -n;
					if (x + n > INTRO_W) n = INTRO_W - x;
					if (c + n > end) return 0;
					memcpy(row + x, c, n);
					c += n;
				}
				x += n;
			}
		}
		break;
	case 12: {                                /* FLI_LC */
		int y0 = rd16(c), lines = rd16(c + 2);
		c += 4;
		for (y = y0; y < y0 + lines && y < INTRO_H && c < end; y++) {
			UBYTE *row = pix + y * INTRO_W;
			int packets = *c++;
			x = 0;
			while (packets-- > 0 && c + 2 <= end) {
				x += *c++;
				n = (signed char)*c++;
				if (n > 0) {
					if (x + n > INTRO_W || c + n > end) return 0;
					memcpy(row + x, c, n);
					c += n;
				} else {
					n = -n;
					if (x + n > INTRO_W) return 0;
					memset(row + x, *c++, n);
				}
				x += n;
			}
		}
		break;
	}
	case 7: {                                 /* FLC SS2 (not in UFOINT.FLI) */
		int lines = rd16(c);
		c += 2;
		y = 0;
		while (lines > 0 && c + 2 <= end && y < INTRO_H) {
			int w = (WORD)rd16(c);
			c += 2;
			if (w < 0) {
				if (w & 0x4000) y += -w;
				else pix[y * INTRO_W + INTRO_W - 1] = (UBYTE)w;
				continue;
			}
			x = 0;
			while (w-- > 0 && c + 2 <= end) {
				x += *c++;
				n = (signed char)*c++;
				if (n > 0) {
					if (x + 2 * n > INTRO_W || c + 2 * n > end) return 0;
					memcpy(pix + y * INTRO_W + x, c, 2 * n);
					c += 2 * n;
				} else {
					n = -n;
					if (x + 2 * n > INTRO_W) return 0;
					for (i = 0; i < n; i++) { pix[y * INTRO_W + x + 2 * i] = c[0]; pix[y * INTRO_W + x + 2 * i + 1] = c[1]; }
					c += 2;
				}
				x += 2 * n;
			}
			y++;
			lines--;
		}
		break;
	}
	default:                                  /* 18 PSTAMP and the rest: skip */
		break;
	}
	return 0;
}

/* the frame at *pos; returns its delay override (0: none), -1 at the end */
static int frame(ULONG *pos, int *palchg)
{
	const UBYTE *f, *end;
	ULONG fsize;
	int chunks, override;

	for (;;) {
		if (*pos + 16 > s_size) return -1;
		f = s_file + *pos;
		fsize = rd32(f);
		if (fsize < 16 || *pos + fsize > s_size) return -1;
		*pos += fsize;
		if (rd16(f + 4) == 0xF1FA) break;      /* skip prefix chunks */
	}
	chunks = rd16(f + 6);
	override = rd16(f + 8);
	end = f + fsize;
	f += 16;
	while (chunks-- > 0 && f + 6 <= end) {
		ULONG csize = rd32(f);
		if (csize < 6 || f + csize > end) break;
		if (chunk(f + 6, f + csize, rd16(f + 4))) *palchg = 1;
		f += csize;
	}
	return override;
}

/* --------------------------------------------------- Paula, our own -- */

#define MUS_BUFS  4
#define MUS_CHUNK 4096

static struct MsgPort *a_port;
static struct IOAudio *a_open, *a_fx[2], *a_mus[2][MUS_BUFS];
static int a_fxbusy[2], a_musbusy[2][MUS_BUFS], a_ok;
static BYTE *a_musbuf[MUS_BUFS];
static BPTR a_tune;                          /* the stream playing, 0: none */
static int a_musnext;
static UBYTE a_map[] = { 15 };

static void a_reap(struct IOAudio *io, int *busy)
{
	if (*busy && CheckIO((struct IORequest *)io) != NULL) {
		WaitIO((struct IORequest *)io);
		*busy = 0;
	}
}

/* CMD_FLUSH, then every request is back (see amiga_audio.c: never AbortIO) */
static void a_flush(ULONG mask)
{
	struct IOAudio fl = *a_open;
	int c, b;
	fl.ioa_Request.io_Command = CMD_FLUSH;
	fl.ioa_Request.io_Flags = 0;
	fl.ioa_Request.io_Unit = (struct Unit *)mask;
	DoIO((struct IORequest *)&fl);
	for (c = 0; c < 2; c++) {
		if ((mask & (1UL << c)) && a_fxbusy[c]) { WaitIO((struct IORequest *)a_fx[c]); a_fxbusy[c] = 0; }
		for (b = 0; b < MUS_BUFS; b++)
			if ((mask & (4UL << c)) && a_musbusy[c][b]) { WaitIO((struct IORequest *)a_mus[c][b]); a_musbusy[c][b] = 0; }
	}
}

static void a_close(void)
{
	int c, b;
	if (a_ok) {
		a_flush(15);
		CloseDevice((struct IORequest *)a_open);
		a_ok = 0;
	}
	for (c = 0; c < 2; c++) {
		if (a_fx[c]) FreeVec(a_fx[c]);
		a_fx[c] = NULL;
		for (b = 0; b < MUS_BUFS; b++) { if (a_mus[c][b]) FreeVec(a_mus[c][b]); a_mus[c][b] = NULL; }
	}
	for (b = 0; b < MUS_BUFS; b++) { if (a_musbuf[b]) FreeMem(a_musbuf[b], MUS_CHUNK); a_musbuf[b] = NULL; }
	if (a_open) FreeVec(a_open);
	a_open = NULL;
	if (a_port) DeleteMsgPort(a_port);
	a_port = NULL;
}

static int a_init(void)
{
	int c, b;
	a_port = CreateMsgPort();
	a_open = (struct IOAudio *)AllocVec(sizeof(struct IOAudio), MEMF_PUBLIC | MEMF_CLEAR);
	if (a_port == NULL || a_open == NULL) { a_close(); return 0; }
	a_open->ioa_Request.io_Message.mn_ReplyPort = a_port;
	a_open->ioa_Request.io_Message.mn_Length = sizeof(struct IOAudio);
	a_open->ioa_Request.io_Flags = ADIOF_NOWAIT;
	a_open->ioa_Data = a_map;
	a_open->ioa_Length = sizeof a_map;
	if (OpenDevice((CONST_STRPTR)AUDIONAME, 0, (struct IORequest *)a_open, 0) != 0) { a_close(); return 0; }
	a_ok = 1;
	for (c = 0; c < 2; c++) {
		a_fx[c] = (struct IOAudio *)AllocVec(sizeof(struct IOAudio), MEMF_PUBLIC | MEMF_CLEAR);
		if (a_fx[c] == NULL) { a_close(); return 0; }
		*a_fx[c] = *a_open;
		a_fx[c]->ioa_Request.io_Unit = (struct Unit *)(1UL << c);
		for (b = 0; b < MUS_BUFS; b++) {
			a_mus[c][b] = (struct IOAudio *)AllocVec(sizeof(struct IOAudio), MEMF_PUBLIC | MEMF_CLEAR);
			if (a_mus[c][b] == NULL) { a_close(); return 0; }
			*a_mus[c][b] = *a_open;
			a_mus[c][b]->ioa_Request.io_Unit = (struct Unit *)(4UL << c);
		}
	}
	for (b = 0; b < MUS_BUFS; b++) {
		a_musbuf[b] = (BYTE *)AllocMem(MUS_CHUNK, MEMF_CHIP);
		if (a_musbuf[b] == NULL) { a_close(); return 0; }
	}
	return 1;
}

static void a_write(struct IOAudio *io, BYTE *data, ULONG len, UWORD per, UWORD vol)
{
	io->ioa_Request.io_Command = CMD_WRITE;
	io->ioa_Request.io_Flags = ADIOF_PERVOL;
	io->ioa_Data = (UBYTE *)data;
	io->ioa_Length = len & ~1UL;
	io->ioa_Period = per;
	io->ioa_Volume = vol;
	io->ioa_Cycles = 1;
	BeginIO((struct IORequest *)io);        /* BeginIO: see amiga_audio.c */
}

/* an effect on whichever of Paula 0/1 is free, else the one started first */
static void a_fxplay(int n, int *turn)
{
	int c;
	if (!a_ok || n < 0 || n >= N_FX || s_fx[n] == NULL) return;
	a_reap(a_fx[0], &a_fxbusy[0]);
	a_reap(a_fx[1], &a_fxbusy[1]);
	c = !a_fxbusy[*turn] ? *turn : !a_fxbusy[!*turn] ? !*turn : *turn;
	if (a_fxbusy[c]) a_flush(1UL << c);
	a_write(a_fx[c], s_fx[n], s_fxlen[n], s_fxper[n], s_fxvol[n]);
	a_fxbusy[c] = 1;
	*turn = !c;
}

static void a_musstop(void)
{
	if (!a_ok) return;
	a_flush(12);
	a_tune = 0;
}

static void a_musstart(int t)
{
	if (!a_ok) return;
	a_musstop();
	if (t < 0 || t > 2 || s_tune[t] == 0) return;
	Seek(s_tune[t], 0, OFFSET_BEGINNING);
	a_tune = s_tune[t];
	a_musnext = 0;
}

/* refill the stream's free buffers; called every 20 ms or so */
static void a_musservice(void)
{
	int pass, c, b;
	UWORD per, vol;
	if (!a_ok || a_tune == 0) return;
	per = (UWORD)((amigagfx_paula_clock() + MUS_RATE / 2) / MUS_RATE);
	vol = (UWORD)((s_vol * 64) / 128);
	for (pass = 0; pass < MUS_BUFS; pass++) {
		LONG n;
		b = a_musnext;
		for (c = 0; c < 2; c++) a_reap(a_mus[c][b], &a_musbusy[c][b]);
		if (a_musbusy[0][b] || a_musbusy[1][b]) break;
		n = Read(a_tune, a_musbuf[b], MUS_CHUNK);
		if (n < 2) { a_tune = 0; break; }       /* the tune is over */
		for (c = 0; c < 2; c++) {
			a_write(a_mus[c][b], a_musbuf[b], (ULONG)n, per, vol);
			a_musbusy[c][b] = 1;
		}
		a_musnext = (b + 1) % MUS_BUFS;
	}
}

/* -------------------------------------------------------- skip by input -- */

static struct InputEvent *skip_handler(register struct InputEvent *ev __asm("a0"), register APTR data __asm("a1"))
{
	struct InputEvent *e;
	(void)data;
	for (e = ev; e != NULL; e = e->ie_NextEvent) {
		if ((e->ie_Class == IECLASS_RAWKEY && e->ie_Code == 0x45) ||          /* Esc down */
		    (e->ie_Class == IECLASS_RAWMOUSE &&
		     (e->ie_Code == IECODE_LBUTTON || e->ie_Code == IECODE_RBUTTON))) {
			if (amigagfx_is_front()) s_skip = 1;
		}
	}
	return ev;                                /* the game sees them too */
}

/* --------------------------------------------------------- the picture -- */

static int s_white = -1;
static int s_shownpct = -2;

/* A 3x5 font for "LOADING nn %" - smaller than the system's 8 pixels (the
 * player's wish). Each glyph: up to 4 columns, 5 rows, a bit per pixel. */
#define FONT_H 5
static const struct { char c; unsigned char w; unsigned char row[FONT_H]; } s_font[] = {
	{ '0', 3, { 7, 5, 5, 5, 7 } }, { '1', 3, { 2, 6, 2, 2, 7 } }, { '2', 3, { 7, 1, 7, 4, 7 } },
	{ '3', 3, { 7, 1, 7, 1, 7 } }, { '4', 3, { 5, 5, 7, 1, 1 } }, { '5', 3, { 7, 4, 7, 1, 7 } },
	{ '6', 3, { 7, 4, 7, 5, 7 } }, { '7', 3, { 7, 1, 2, 2, 2 } }, { '8', 3, { 7, 5, 7, 5, 7 } },
	{ '9', 3, { 7, 5, 7, 1, 7 } }, { 'L', 3, { 4, 4, 4, 4, 7 } }, { 'O', 3, { 7, 5, 5, 5, 7 } },
	{ 'A', 3, { 2, 5, 7, 5, 5 } }, { 'D', 3, { 6, 5, 5, 5, 6 } }, { 'I', 3, { 7, 2, 2, 2, 7 } },
	{ 'N', 4, { 9, 13, 11, 9, 9 } }, { 'G', 4, { 7, 8, 11, 9, 7 } }, { '%', 3, { 5, 1, 2, 4, 5 } },
	{ ' ', 2, { 0, 0, 0, 0, 0 } }
};
static UBYTE s_strip[INTRO_W * (FONT_H + 4)];

/* the text, bottom right of the title, over the picture's own rows */
static void small_text(const char *s, int pen)
{
	const int y0 = INTRO_H - FONT_H - 4;      /* 2 rows above, 2 below */
	const char *p;
	int x, tw = 0, i, r, b;

	memcpy(s_strip, s_pix + y0 * INTRO_W, sizeof s_strip);
	for (p = s; *p; p++)
		for (i = 0; i < (int)(sizeof s_font / sizeof s_font[0]); i++)
			if (s_font[i].c == *p) { tw += s_font[i].w + 1; break; }
	x = INTRO_W - 4 - tw;
	for (p = s; *p; p++)
		for (i = 0; i < (int)(sizeof s_font / sizeof s_font[0]); i++) {
			if (s_font[i].c != *p) continue;
			for (r = 0; r < FONT_H; r++)
				for (b = 0; b < s_font[i].w; b++)
					if (s_font[i].row[r] & (1 << (s_font[i].w - 1 - b)))
						s_strip[(r + 2) * INTRO_W + x + b] = (UBYTE)pen;
			x += s_font[i].w + 1;
			break;
		}
	amigagfx_intro_rows(s_strip, INTRO_W, INTRO_H, y0, FONT_H + 4);
}

/* "LOADING nn %" in white, bottom right of the title, while the game loads */
static void loading_text(void)
{
	char t[24];
	int pct = s_loadpct, i, best = 0;
	long bd = 0x7fffffffL;
	if (pct == s_shownpct) return;
	if (s_loaded && s_shownpct == -2) return;   /* never shown: not now either */
	s_shownpct = pct;
	if (s_white < 0) {
		for (i = 0; i < 256; i++) {
			long d = (long)(255 - s_pal[i * 3]) * (255 - s_pal[i * 3]) +
			         (long)(255 - s_pal[i * 3 + 1]) * (255 - s_pal[i * 3 + 1]) +
			         (long)(255 - s_pal[i * 3 + 2]) * (255 - s_pal[i * 3 + 2]);
			if (d < bd) { bd = d; best = i; }
		}
		s_white = best;
	}
	if (pct < 0) pct = 0;
	if (pct > 100) pct = 100;
	snprintf(t, sizeof t, "LOADING %d %%", pct);
	small_text(t, s_white);
}

/* ------------------------------------------------------------- process -- */

/* sleep until due (ms), keeping the stream fed */
static void wait_until(ULONG due)
{
	for (;;) {
		LONG wait = (LONG)(due - intro_ms());
		a_musservice();
		if (wait <= 0 || s_quit || s_skip) return;
		Delay(1);
	}
}

static void intro_main(void)
{
	struct MsgPort *port = CreateMsgPort();
	struct IOStdReq *io = port ? (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq)) : NULL;
	struct Interrupt irq;
	int handler = 0, played = 0, palchg, k = 0, delay, ov, turn = 0, skipped = 0;
	ULONG pos = 128, due;

	if (io != NULL && OpenDevice((CONST_STRPTR)"input.device", 0, (struct IORequest *)io, 0) == 0) {
		memset(&irq, 0, sizeof irq);
		irq.is_Node.ln_Type = NT_INTERRUPT;
		irq.is_Node.ln_Pri = 60;               /* ahead of Intuition (50) */
		irq.is_Node.ln_Name = (char *)"AmiXcom intro";
		irq.is_Code = (void (*)(void))skip_handler;
		io->io_Command = IND_ADDHANDLER;
		io->io_Data = (APTR)&irq;
		DoIO((struct IORequest *)io);
		handler = 1;
	}
	if (s_lent) a_init();

	due = intro_ms();
	while (!s_quit && played < s_frames) {
		/* the sound track up to this frame, as AudioSequence runs it */
		while (s_track[k][0] <= played) {
			int cmd = s_track[k++][1];
			if (skipped) continue;
			if (cmd & 0x400) s_speed = cmd & 0xff;
			else if (cmd & 0x200) { if (s_music) a_musstart(cmd & 3); }
			else if (cmd < N_FX) a_fxplay(cmd, &turn);
		}
		palchg = 0;
		ov = frame(&pos, &palchg);
		if (ov < 0) break;
		played++;
		if (s_skip && !skipped) {             /* silence, then straight to the title */
			skipped = 1;
			if (a_ok) a_flush(15);
			a_tune = 0;
		}
		if (skipped && played < s_frames) continue;
		delay = ov > 0 ? ov : (s_speed * 1000) / 70;
		wait_until(due);
		amigagfx_intro_put(s_pix, INTRO_W, INTRO_H, (palchg || skipped) ? s_pal : NULL);
		due += (ULONG)delay;
		if ((LONG)(intro_ms() - due) > 500L) due = intro_ms();   /* fell behind: no rush */
	}
	s_state = INTRO_LAST;
	/* the title stays; the tune may still be playing out */
	while (!s_quit) {
		a_musservice();
		loading_text();
		Delay(2);
	}

	a_close();
	if (handler) {
		io->io_Command = IND_REMHANDLER;
		io->io_Data = (APTR)&irq;
		DoIO((struct IORequest *)io);
		CloseDevice((struct IORequest *)io);
	}
	if (io != NULL) DeleteIORequest((struct IORequest *)io);
	if (port != NULL) DeleteMsgPort(port);
	Forbid();                                  /* stay alive until torn down */
	s_running = 0;
}

/* ----------------------------------------------------------- game side -- */

/* the whole of a file, or NULL */
static UBYTE *slurp(const char *path, ULONG *len)
{
	BPTR fh = Open((CONST_STRPTR)path, MODE_OLDFILE);
	UBYTE *buf = NULL;
	LONG n;
	if (fh == 0) return NULL;
	Seek(fh, 0, OFFSET_END);
	n = Seek(fh, 0, OFFSET_BEGINNING);
	if (n > 0 && (buf = (UBYTE *)AllocVec((ULONG)n, MEMF_ANY)) != NULL && Read(fh, buf, n) != n) {
		FreeVec(buf);
		buf = NULL;
	}
	Close(fh);
	*len = (ULONG)n;
	return buf;
}

/* One effect from a CAT, converted for Paula as SoundSet::loadCat would load
 * it: INTRO.CAT holds raw 6-bit samples at 8 kHz after 5 name bytes (x4),
 * SAMPLE3.CAT 8-bit WAVs. 0 when the CAT does not have it. */
static int load_fx(int n, const UBYTE *cat, ULONG catlen, int which, int idx, int vol)
{
	ULONG off, size, i, rate = 8000, count;
	const UBYTE *p, *d;
	BYTE *out;

	if (cat == NULL || catlen < 8 || (ULONG)(idx + 1) * 8 > rd32(cat)) return 0;
	off = rd32(cat + idx * 8);
	size = rd32(cat + idx * 8 + 4);
	if (off >= catlen || size == 0) return 0;
	p = cat + off;
	if (p[0] <= 56) p += p[0] + 1;           /* CatFile: a name first */
	if (p + size > cat + catlen) size = (ULONG)(cat + catlen - p);
	if (which == CAT_INTRO) {
		if (size <= 6) return 0;
		d = p + 5;
		count = size - 6;                     /* 5 name bytes, one trailing 0 */
	} else {
		ULONG q = 12;
		if (size < 44 || memcmp(p, "RIFF", 4) != 0) return 0;
		rate = rd32(p + 24);
		if (rd16(p + 34) != 8 || rd16(p + 22) != 1) return 0;   /* 8-bit mono only */
		d = NULL;
		count = 0;
		while (q + 8 <= size) {
			ULONG cl = rd32(p + q + 4);
			if (memcmp(p + q, "data", 4) == 0) {
				d = p + q + 8;
				count = cl;
				if (q + 8 + count > size) count = size - q - 8;
				break;
			}
			q += 8 + cl + (cl & 1);
		}
		if (d == NULL || rate < 2000) return 0;
	}
	count &= ~1UL;
	if (count < 2 || count > 131070) return 0;
	out = (BYTE *)AllocMem(count, MEMF_CHIP);
	if (out == NULL) return 0;
	for (i = 0; i < count; i++)
		out[i] = (BYTE)((which == CAT_INTRO ? (UBYTE)(d[i] * 4) : d[i]) - 128);
	s_fx[n] = out;
	s_fxlen[n] = count;
	s_fxper[n] = (UWORD)((amigagfx_paula_clock() + rate / 2) / rate);
	s_fxvol[n] = (UBYTE)((vol * s_vol) / 256);  /* SDL 0..128 x volume -> Paula 0..64 */
	return 1;
}

static void load_sound(void)
{
	UBYTE *cat[2];
	ULONG len[2] = { 0, 0 };
	char path[160];
	int n, set, t;

	cat[0] = slurp(SOUND_DIR "INTRO.CAT", &len[0]);
	cat[1] = slurp(SOUND_DIR "SAMPLE3.CAT", &len[1]);
	for (n = 0; n < N_FX; n++)
		for (set = 0; set < 3; set++) {
			const unsigned char *e = s_sets[set][n];
			if (load_fx(n, cat[e[0]], len[e[0]], e[0], e[1], e[2])) break;
		}
	if (cat[0]) FreeVec(cat[0]);
	if (cat[1]) FreeVec(cat[1]);
	if (s_music)
		for (t = 0; t < 3; t++) {
			snprintf(path, sizeof path, "%smusic/GMINTRO%d.raw", s_userdir, t + 1);
			s_tune[t] = Open((CONST_STRPTR)path, MODE_OLDFILE);
		}
}

static void free_sound(void)
{
	int n;
	for (n = 0; n < N_FX; n++) {
		if (s_fx[n]) FreeMem(s_fx[n], s_fxlen[n]);
		s_fx[n] = NULL;
	}
	for (n = 0; n < 3; n++) {
		if (s_tune[n]) Close(s_tune[n]);
		s_tune[n] = 0;
	}
}

/* Read the intro and start playing it. 0: no intro (no file, no 8-bit screen
 * of our own) - the caller shows the ordinary loading picture instead. */
int amiga_intro_begin(void)
{
	UBYTE *f;
	ULONG len = 0;

	if (s_state != INTRO_OFF || !amigagfx_intro_ok(INTRO_W, INTRO_H)) return 0;
	s_efreq = amiga_uclock_freq();          /* opens timer.device: TimerBase */
	if (s_efreq == 0) return 0;
	f = slurp(INTRO_DIR "UFOINT.FLI", &len);
	if (f == NULL) return 0;
	if (len <= 128 || (rd16(f + 4) != 0xAF11 && rd16(f + 4) != 0xAF12) ||
	    rd16(f + 8) != INTRO_W || rd16(f + 10) != INTRO_H ||
	    (s_pix = (UBYTE *)AllocVec(INTRO_W * INTRO_H, MEMF_ANY | MEMF_CLEAR)) == NULL) {
		FreeVec(f);
		return 0;
	}
	s_file = f;
	s_size = len;
	s_frames = rd16(s_file + 6);               /* the ring frame after them is not shown */
	s_speed = rd16(s_file + 16);
	memset(s_pal, 0, sizeof s_pal);
	amigagfx_intro_put(s_pix, INTRO_W, INTRO_H, s_pal);   /* black first */

	/* the sound: the game hands Paula over for the intro */
	s_lent = 0;
	if (!s_mute && AmigaAudio_IsOpen()) {
		load_sound();
		AmigaAudio_Close();
		s_lent = 1;
	}

	s_quit = s_skip = 0;
	s_running = 1;
	s_state = INTRO_PLAYING;
	if (!CreateNewProcTags(NP_Entry, (ULONG)intro_main,
	                       NP_Name, (ULONG)"AmiXcom intro",
	                       NP_Priority, 1UL,
	                       NP_StackSize, 16384UL,
	                       NP_Output, 0UL, NP_Input, 0UL,
	                       TAG_END)) {
		s_running = 0;
		s_state = INTRO_OFF;
		free_sound();
		if (s_lent) AmigaAudio_Open();
		FreeVec(s_file);
		FreeVec(s_pix);
		s_file = s_pix = NULL;
		return 0;
	}
	return 1;
}

int amiga_intro_active(void)
{
	return s_state == INTRO_PLAYING || s_state == INTRO_LAST;
}

/* the loading percentage, for the title's "LOADING nn %" */
void amiga_intro_progress(int percent)
{
	s_loadpct = percent;
}

/* Loading is done: wait for the intro's end (or Esc / a click), fade it out
 * in ~0.5 s and give everything back - Paula to the game too. */
void amiga_intro_end(void)
{
	unsigned char tmp[768];
	int i, j;

	if (!amiga_intro_active()) return;
	s_loaded = 1;
	s_loadpct = 100;
	while (s_state == INTRO_PLAYING) Delay(2);
	if (s_shownpct != -2) Delay(5);            /* "LOADING 100 %" for a moment */
	s_quit = 1;
	for (i = 0; i < 300 && s_running; i++) Delay(1);
	for (i = 11; i >= 0; i--) {
		for (j = 0; j < 768; j++) tmp[j] = (unsigned char)((s_pal[j] * i) / 12);
		amigagfx_intro_put(NULL, INTRO_W, INTRO_H, tmp);
		Delay(2);
	}
	if (!s_running) {                          /* never free under a live process */
		free_sound();
		FreeVec(s_file);
		FreeVec(s_pix);
		s_file = s_pix = NULL;
	}
	if (s_lent) AmigaAudio_Open();
	s_lent = 0;
	s_state = INTRO_GONE;
}

/* the game's own CutsceneState("intro") is not needed after this */
int amiga_intro_played(void)
{
	return s_state == INTRO_GONE;
}
