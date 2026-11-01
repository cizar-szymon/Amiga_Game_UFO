/*
 * MakeMusic - converts the music of X-COM: UFO Defense or Terror from the
 * Deep (the PC versions) for AmiXcom, from the player's own game files, once.
 * The game plays the result from disk (user/music/<NAME>.raw); since 6ane it
 * no longer converts anything at its start.
 *
 *   MakeMusic [GAME=UFO|TFTD] [SOUND=dir] [BANK=file] [RULES=music.rul]
 *             [TO=dir] [SOURCE=SAMPLES|ADLIB] [QUALITY=SMOOTH|PLAIN] [ONLY=NAME]
 *     GAME     default: the game's active one (xcom1 / xcom2 in
 *              PROGDIR:user/options.cfg), else UFO
 *     SOUND    the SOUND directory of the PC game, with GM.CAT and/or ADLIB.CAT
 *              (default PROGDIR:data/UFO/SOUND, for TFTD PROGDIR:data/TFTD/SOUND)
 *     RULES    the tune list (default PROGDIR:data/standard/xcom1/music.rul,
 *              xcom2 for TFTD; UFO's is built in for when it cannot be read)
 *     BANK     the port's instrument bank (default PROGDIR:data/common/music.bnk;
 *              needed for SOURCE=SAMPLES only)
 *     TO       where the music goes (default PROGDIR:user/music, made if missing)
 *     SOURCE   SAMPLES = GM.CAT through the port's instrument bank,
 *              ADLIB   = ADLIB.CAT through the YM3812 emulator;
 *              default: the game's own setting (amigaMusicSource in
 *              PROGDIR:user/options.cfg), else SAMPLES
 *     QUALITY  only written to source.txt, as the game does (the files are the
 *              same either way); default: the game's amigaMusicQuality
 *     ONLY     one tune only (tests)
 *   From the icon the same keys are read from its Tool Types.
 *
 * It is the game's own conversion (Mod::amigaPrerenderMusic), taken out of the
 * game: the same code (native/amiga_music.c for GM.CAT; the port's patched
 * Engine/Adlib player and YM3812 emulator, tools/music/adlib/, for ADLIB.CAT),
 * the same tunes of the game's music.rul in the same order, the same per-tune
 * gain and the same output: headerless 8-bit signed mono at 22050 Hz, one
 * file per tune, plus source.txt saying what it was made from.
 *
 * Builds for the Amiga (build/build.sh, bebbo m68k-amigaos-g++, no FPU
 * needed) and for a PC (tools/music/make_music.py, g++ with a 32-bit long).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#endif
#if defined(__amigaos__)
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#endif

/* -I native (amiga_music.h) and -I the Adlib directory: the game's own
 * Engine/Adlib for the Amiga build, its copy tools/music/adlib on a PC */
#include "amiga_music.h"
#include "fmopl.h"
#include "adlplayer.h"

extern FM_OPL *opl[2];   /* adlib/adlplayer.cpp: the two chips the player writes */

extern "C" void SDLmini_Log(const char *msg) { (void)msg; }

#if defined(__amigaos__)
#define DEF_DATA  "PROGDIR:data"
#define DEF_BANK  "PROGDIR:data/common/music.bnk"
#define DEF_TO    "PROGDIR:user/music"
#define DEF_OPTS  "PROGDIR:user/options.cfg"
/* the stack also from the icon (libnix links swapstack.o only when
 * __stkinit is referred to) */
unsigned long __stack = 65536;
extern "C" void __stkinit(void);
void (*const makemusic_stackSwap)(void) = __stkinit;
const char version[] = "$VER: MakeMusic 1.1 (01.11.2026) AmiXcom music converter";
/* the output window from the icon: Return closes it (Finish) */
extern "C" { const char *__stdiowin = "CON://640/220/AmiXcom MakeMusic/AUTO/CLOSE"; }
extern "C" { extern struct WBStartup *_WBenchMsg; }
#else
#define DEF_DATA  "data"
#define DEF_BANK  "data/common/music.bnk"
#define DEF_TO    "user/music"
#define DEF_OPTS  "user/options.cfg"
#endif

/* The tunes: those of the game's music.rul (standard/xcom1 for UFO,
 * standard/xcom2 for TFTD) that have a catPos, in the order the game
 * converts them (its std::map, sorted by name) - the AdLib chips keep their
 * state from one tune to the next, so the order matters. UFO's list is built
 * in as well, for when the ruleset cannot be read (a PC without the game's
 * data/standard). */
struct Tune { char name[32]; int catPos; };
static const Tune TUNES_UFO[] = {
	{ "GMDEFEND", 3 }, { "GMENBASE", 6 }, { "GMGEO1", 0 }, { "GMGEO2", 18 },
	{ "GMINTER", 2 }, { "GMINTRO1", 19 }, { "GMINTRO2", 20 }, { "GMINTRO3", 21 },
	{ "GMLOSE", 10 }, { "GMMARS", 9 }, { "GMNEWMAR", 8 }, { "GMSTORY", 12 },
	{ "GMTACTIC", 17 }, { "GMWIN", 11 },
};
#define MAXTUNES 64
static Tune TUNES[MAXTUNES];
static int NTUNES = 0;

static int cmpTune(const void *a, const void *b)
{
	return strcmp(((const Tune *)a)->name, ((const Tune *)b)->name);
}

/* "  - type: NAME" and "    catPos: N" lines of a music.rul; '#' comments */
static int loadRules(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[256];
	int cur = -1;
	if (f == NULL) return 0;
	NTUNES = 0;
	while (fgets(line, sizeof line, f) != NULL) {
		char *p = line, *h = strchr(line, '#');
		if (h) *h = 0;
		while (*p == ' ' || *p == '\t') p++;
		if (strncmp(p, "- type:", 7) == 0) {
			char name[32];
			cur = -1;
			if (sscanf(p + 7, " %31s", name) == 1 && NTUNES < MAXTUNES) {
				cur = NTUNES++;
				snprintf(TUNES[cur].name, sizeof TUNES[cur].name, "%s", name);
				TUNES[cur].catPos = -1;
			}
		} else if (strncmp(p, "catPos:", 7) == 0 && cur >= 0) {
			TUNES[cur].catPos = atoi(p + 7);
		}
	}
	fclose(f);
	{	/* only the tunes with a catPos are in the CATs */
		int n = 0;
		for (int i = 0; i < NTUNES; i++) if (TUNES[i].catPos >= 0) TUNES[n++] = TUNES[i];
		NTUNES = n;
	}
	qsort(TUNES, (size_t)NTUNES, sizeof TUNES[0], cmpTune);
	return NTUNES > 0;
}

static int s_fromIcon = 0;

/* ------------------------------------------------------------- messages */

/* the few that matter to a player, in the three languages of the docs */
static void say3(const char *en, const char *pl, const char *de, const char *arg)
{
	printf(en, arg); printf("\n");
	printf(pl, arg); printf("\n");
	printf(de, arg); printf("\n");
}

static int Finish(int rc)
{
	if (s_fromIcon) {
		printf("\nPress Return to close this window.\n");
		fflush(stdout);
		getchar();
	}
	return rc;
}

/* ---------------------------------------------------------------- files */

static unsigned char *readFile(const char *path, long *len)
{
	FILE *f = fopen(path, "rb");
	unsigned char *buf;
	long n;
	if (f == NULL) return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = (unsigned char *)calloc((size_t)n + 1024, 1);   /* + room past the end, zeroed */
	if (buf == NULL || (long)fread(buf, 1, (size_t)n, f) != n) { fclose(f); free(buf); return NULL; }
	fclose(f);
	*len = n;
	return buf;
}

/* dir/name, matching the name without regard to case (the game's file map
 * does that; a PC copy may have gm.cat or GM.CAT) */
static int findFile(const char *dir, const char *name, char *out, size_t size)
{
	DIR *d;
	struct dirent *e;
	snprintf(out, size, "%s/%s", dir, name);
	{ FILE *f = fopen(out, "rb"); if (f != NULL) { fclose(f); return 1; } }
	d = opendir(dir);
	if (d == NULL) return 0;
	while ((e = readdir(d)) != NULL) {
		const char *a = e->d_name, *b = name;
		while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
		if (*a == 0 && *b == 0) {
			snprintf(out, size, "%s/%s", dir, e->d_name);
			closedir(d);
			return 1;
		}
	}
	closedir(d);
	return 0;
}

static unsigned long le32(const unsigned char *p)
{
	return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* a .CAT file as Engine/CatFile.cpp reads it */
struct Cat {
	unsigned char *buf; long len; unsigned long amount;
	unsigned long offset(unsigned long i) const { return le32(buf + i * 8); }
	unsigned long size(unsigned long i) const { return le32(buf + i * 8 + 4); }
};

static int openCat(const char *dir, const char *name, Cat *c)
{
	char path[600];
	c->buf = NULL; c->amount = 0;
	if (!findFile(dir, name, path, sizeof path)) return 0;
	c->buf = readFile(path, &c->len);
	if (c->buf == NULL || c->len < 8) return 0;
	c->amount = le32(c->buf) / 8;
	return 1;
}

/* the bytes CatFile::load(i, name) hands over, and their length; points
 * into the (zero-padded) file buffer like the game's own copy would */
static const unsigned char *catLoad(const Cat &c, unsigned long i, int withName, unsigned long *len)
{
	unsigned long off = c.offset(i), n = c.size(i);
	unsigned char namesize;
	if (off >= (unsigned long)c.len) return NULL;
	namesize = c.buf[off];
	if (namesize <= 56) {
		if (!withName) off += namesize + 1;
		else n += namesize + 1;
	}
	*len = n;
	return c.buf + off;
}

static int writeMark(const char *dir, int adlib, int smooth)
{
	char path[600];
	FILE *f;
	snprintf(path, sizeof path, "%s/source.txt", dir);
	f = fopen(path, "wb");   /* LF, as the game writes it on the Amiga */
	if (f == NULL) return 0;
	if (adlib) fprintf(f, "AdLib (ADLIB.CAT), 22 kHz\n");
	else fprintf(f, "samples (GM.CAT), 22 kHz %s\n", smooth ? "smooth" : "plain");
	fclose(f);
	return 1;
}

/* ------------------------------------------------------------ AdLib */

/* AdlibMusic::amigaRender (Engine/AdlibMusic.cpp, port patch 6amY), as is */
static int renderAdlib(const unsigned char *data, unsigned long size, const char *path)
{
	const int PER_TICK = 314;                 /* 22050 / 70.2 */
	const unsigned long CAP = 8UL * 60UL * 22050UL;
	INT16 a[314], b[314];
	signed char out[314];
	unsigned long done = 0, pos0 = 0;
	int first = 1, ok = 1;
	char tmp[620];
	FILE *f;

	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	f = fopen(tmp, "wb");
	if (f == NULL) return 0;
	func_setup_music((unsigned char *)data, (int)size);
	func_set_music_volume(127);
	while (func_is_music_playing() && done < CAP) {
		int k;
		func_play_tick();
		YM3812UpdateOne(opl[0], a, PER_TICK, 1, 1.0f);
		YM3812UpdateOne(opl[1], b, PER_TICK, 1, 1.0f);
		for (k = 0; k < PER_TICK; ++k) {
			int v = ((int)a[k] + (int)b[k]) >> 10;
			if (v > 127) v = 127; else if (v < -128) v = -128;
			out[k] = (signed char)v;
		}
		if (fwrite(out, 1, PER_TICK, f) != (size_t)PER_TICK) { ok = 0; break; }
		done += PER_TICK;
		{
			unsigned long pos = func_amiga_position();
			if (first) { pos0 = pos; first = 0; }
			else if (done > 5UL * 22050UL && pos == pos0) break;
		}
	}
	func_mute();
	fclose(f);
	if (!ok || done == 0) { remove(tmp); return 0; }
	remove(path);
	if (rename(tmp, path) != 0) { remove(tmp); return 0; }
	return 1;
}

/* ------------------------------------------------------------ options */

static char o_sound[512], o_bank[512], o_to[512], o_only[64], o_rules[512];
static int o_adlib = -1, o_smooth = -1, o_tftd = -1;

static void setKey(const char *kv)
{
	const char *eq = strchr(kv, '=');
	char key[32];
	size_t n;
	if (eq == NULL) return;
	n = (size_t)(eq - kv);
	if (n >= sizeof key) return;
	for (size_t i = 0; i < n; i++) key[i] = (char)toupper((unsigned char)kv[i]);
	key[n] = 0;
	eq++;
	if (!strcmp(key, "SOUND")) snprintf(o_sound, sizeof o_sound, "%s", eq);
	else if (!strcmp(key, "BANK")) snprintf(o_bank, sizeof o_bank, "%s", eq);
	else if (!strcmp(key, "TO")) snprintf(o_to, sizeof o_to, "%s", eq);
	else if (!strcmp(key, "ONLY")) snprintf(o_only, sizeof o_only, "%s", eq);
	else if (!strcmp(key, "RULES")) snprintf(o_rules, sizeof o_rules, "%s", eq);
	else if (!strcmp(key, "GAME")) o_tftd = (toupper((unsigned char)eq[0]) == 'T') ? 1 : 0;
	else if (!strcmp(key, "SOURCE")) o_adlib = (toupper((unsigned char)eq[0]) == 'A') ? 1 : 0;
	else if (!strcmp(key, "QUALITY")) o_smooth = (toupper((unsigned char)eq[0]) == 'P') ? 0 : 1;
}

/* the game's own setting, from its options.cfg */
static int gameOption(const char *key, int def)
{
	FILE *f = fopen(DEF_OPTS, "r");
	char line[256];
	size_t kl = strlen(key);
	int v = def;
	if (f == NULL) return def;
	while (fgets(line, sizeof line, f) != NULL) {
		const char *p = line;
		while (*p == ' ') p++;
		if (strncmp(p, key, kl) == 0 && p[kl] == ':') { v = atoi(p + kl + 1); break; }
	}
	fclose(f);
	return v;
}

/* TFTD when the game's active master mod is xcom2: in options.cfg
 *     - id: xcom2
 *       active: true */
static int gameIsTftd(void)
{
	FILE *f = fopen(DEF_OPTS, "r");
	char line[256];
	int idTftd = 0, tftd = 0;
	if (f == NULL) return 0;
	while (fgets(line, sizeof line, f) != NULL) {
		if (strstr(line, "id: ") != NULL) idTftd = strstr(line, "id: xcom2") != NULL;
		else if (idTftd && strstr(line, "active: true") != NULL) { tftd = 1; break; }
	}
	fclose(f);
	return tftd;
}

#if defined(__amigaos__)
struct Library *IconBase;
static void readToolTypes(void)
{
	struct DiskObject *dobj;
	struct WBArg *arg;
	BPTR old;
	if (_WBenchMsg == NULL || _WBenchMsg->sm_NumArgs < 1) return;
	IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 36);
	if (IconBase == NULL) return;
	arg = _WBenchMsg->sm_ArgList;
	old = CurrentDir(arg->wa_Lock);
	dobj = GetDiskObject((CONST_STRPTR)arg->wa_Name);
	if (dobj != NULL) {
		char **tt = (char **)dobj->do_ToolTypes;
		for (int i = 0; tt != NULL && tt[i] != NULL; i++) {
			const char *p = tt[i];
			while (*p == ' ') p++;
			if (*p == '(') continue;   /* "(KEY=...)" is only a hint */
			setKey(p);
		}
		FreeDiskObject(dobj);
	}
	CurrentDir(old);
	CloseLibrary(IconBase);
	IconBase = NULL;
}
#endif

/* --------------------------------------------------------------- main */

int main(int argc, char **argv)
{
	Cat gm, adl, aintro;
	int made = 0, failed = 0;
	time_t t0 = time(NULL);
	unsigned long total = 0;

	/* The conversion counts in 32-bit longs and lets them wrap, as on the
	 * Amiga (16.16 sample positions): built with a 64-bit long (Linux,
	 * macOS) 8 of the 14 tunes came out different. make_music.py builds
	 * with -m32 there; Windows (MinGW) has a 32-bit long. */
	if (sizeof(long) != 4) {
		printf("MakeMusic must be built with a 32-bit long (gcc -m32): this build has %d bytes.\n", (int)sizeof(long));
		return 20;
	}
	snprintf(o_bank, sizeof o_bank, "%s", DEF_BANK);
	snprintf(o_to, sizeof o_to, "%s", DEF_TO);
#if defined(__amigaos__)
	if (argc == 0) { s_fromIcon = 1; readToolTypes(); }
#endif
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "?") || !strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			printf("MakeMusic [GAME=UFO|TFTD] [SOUND=dir] [BANK=file] [RULES=music.rul] [TO=dir]\n"
			       "          [SOURCE=SAMPLES|ADLIB] [QUALITY=SMOOTH|PLAIN] [ONLY=NAME]\n");
			return 0;
		}
		setKey(argv[i]);
	}
	if (o_adlib < 0) o_adlib = gameOption("amigaMusicSource", 0) == 1 ? 1 : 0;
	if (o_smooth < 0) o_smooth = gameOption("amigaMusicQuality", 1) ? 1 : 0;
	if (o_tftd < 0) o_tftd = gameIsTftd();
	if (!o_sound[0]) snprintf(o_sound, sizeof o_sound, "%s/%s/SOUND", DEF_DATA, o_tftd ? "TFTD" : "UFO");
	if (!o_rules[0]) snprintf(o_rules, sizeof o_rules, "%s/standard/%s/music.rul", DEF_DATA, o_tftd ? "xcom2" : "xcom1");

	printf("AmiXcom MakeMusic 1.1 - X-COM music for AmiXcom\n");
	if (!loadRules(o_rules)) {
		if (o_tftd) {
			say3("Cannot read the tune list %s.", "Nie da sie odczytac listy utworow %s.",
			     "Die Liste der Stuecke %s ist nicht lesbar.", o_rules);
			return Finish(10);
		}
		memcpy(TUNES, TUNES_UFO, sizeof TUNES_UFO);   /* UFO's, built in */
		NTUNES = (int)(sizeof TUNES_UFO / sizeof TUNES_UFO[0]);
		snprintf(o_rules, sizeof o_rules, "built in");
	}
	printf("  %s: %d tunes (%s)\n", o_tftd ? "Terror from the Deep" : "UFO", NTUNES, o_rules);
	printf("  from %s (%s)\n  to   %s\n\n", o_sound, o_adlib ? "ADLIB.CAT" : "GM.CAT", o_to);

	openCat(o_sound, "GM.CAT", &gm);
	openCat(o_sound, "ADLIB.CAT", &adl);
	openCat(o_sound, "AINTRO.CAT", &aintro);
	/* the game's fallback: the other file when the chosen one is missing */
	if (o_adlib && adl.buf == NULL && gm.buf != NULL) {
		say3("ADLIB.CAT not found - using GM.CAT.", "Brak ADLIB.CAT - uzywam GM.CAT.",
		     "ADLIB.CAT fehlt - nehme GM.CAT.", NULL);
		o_adlib = 0;
	} else if (!o_adlib && gm.buf == NULL && adl.buf != NULL) {
		say3("GM.CAT not found - using ADLIB.CAT.", "Brak GM.CAT - uzywam ADLIB.CAT.",
		     "GM.CAT fehlt - nehme ADLIB.CAT.", NULL);
		o_adlib = 1;
	}
	if ((o_adlib && adl.buf == NULL) || (!o_adlib && gm.buf == NULL)) {
		say3("No GM.CAT or ADLIB.CAT in %s.", "Brak GM.CAT i ADLIB.CAT w %s.",
		     "Kein GM.CAT oder ADLIB.CAT in %s.", o_sound);
		say3("Copy the SOUND directory of your PC X-COM there.",
		     "Skopiuj tam katalog SOUND swojej gry X-COM z PC.",
		     "Kopiere das Verzeichnis SOUND deines PC-X-COM dorthin.", NULL);
		return Finish(10);
	}
	if (!o_adlib && !AmigaMusic_LoadBank(o_bank)) {
		say3("The instrument bank %s is missing.", "Brak banku instrumentow %s.",
		     "Die Instrumentenbank %s fehlt.", o_bank);
		return Finish(10);
	}
	if (o_adlib) {
		opl[0] = OPLCreate(OPL_TYPE_YM3812, 3579545, 22050);   /* AdlibMusic(): rate 22050 */
		opl[1] = OPLCreate(OPL_TYPE_YM3812, 3579545, 22050);
		if (opl[0] == NULL || opl[1] == NULL) {
			say3("Out of memory.", "Za malo pamieci.", "Nicht genug Speicher.", NULL);
			return Finish(20);
		}
	}
#if defined(__amigaos__)
	{ BPTR l = CreateDir((CONST_STRPTR)o_to); if (l) UnLock(l); }
#elif defined(_WIN32)
	_mkdir(o_to);
#else
	mkdir(o_to, 0777);
#endif

	for (int t = 0; t < NTUNES; t++) {
		char path[620];
		const unsigned char *data = NULL;
		unsigned long len = 0;
		time_t t1 = time(NULL);
		int ok;
		long bytes = 0;

		if (o_only[0] && strcmp(o_only, TUNES[t].name) != 0) continue;
		snprintf(path, sizeof path, "%s/%s.raw", o_to, TUNES[t].name);
		printf("%-9s ", TUNES[t].name);
		fflush(stdout);
		if (o_adlib) {
			/* Mod::loadMusic: ADLIB.CAT, else the rest from AINTRO.CAT,
			 * the name kept (load(track, true)); AdlibMusic::load then adds
			 * the name's length once more - kept, the player only uses it
			 * as a bound */
			unsigned long tr = (unsigned long)TUNES[t].catPos;
			if (tr < adl.amount) data = catLoad(adl, tr, 1, &len);
			else if (aintro.buf != NULL && tr - adl.amount < aintro.amount) data = catLoad(aintro, tr - adl.amount, 1, &len);
			if (data != NULL && data[0] <= 56) len += data[0];
			ok = data != NULL && renderAdlib(data, len, path);
		} else {
			if ((unsigned long)TUNES[t].catPos < gm.amount) data = catLoad(gm, (unsigned long)TUNES[t].catPos, 0, &len);
			ok = data != NULL && AmigaMusic_RenderToFile(data, len, AmigaMusic_TuneGain(TUNES[t].catPos), path, NULL);
		}
		if (ok) {
			FILE *f = fopen(path, "rb");
			if (f != NULL) { fseek(f, 0, SEEK_END); bytes = ftell(f); fclose(f); }
			total += (unsigned long)bytes;
			made++;
			/* whole numbers: printf takes the decimal mark from the locale */
			printf("%4ld s of music, %ld s\n", (bytes + 11025) / 22050, (long)(time(NULL) - t1));
		} else {
			failed++;
			printf("FAILED\n");
			if (data != NULL)
				say3("Could not write %s - is the disk full?", "Nie da sie zapisac %s - pelny dysk?",
				     "%s laesst sich nicht schreiben - Diskette/Platte voll?", path);
		}
	}
	if (made > 0 && !o_only[0]) writeMark(o_to, o_adlib, o_smooth);
	if (!o_adlib) AmigaMusic_FreeBank();
	if (o_adlib) { OPLDestroy(opl[0]); OPLDestroy(opl[1]); opl[0] = opl[1] = NULL; }

	printf("\n%d tunes, %lu KB, %ld s.\n", made, (total + 1023) / 1024, (long)(time(NULL) - t0));
	if (failed == 0 && made > 0)
		say3("Done. Start the game: it plays the music from %s.", "Gotowe. Uruchom gre: muzyka gra z %s.",
		     "Fertig. Starte das Spiel: es spielt die Musik aus %s.", o_to);
	return Finish(failed ? 5 : 0);
}
