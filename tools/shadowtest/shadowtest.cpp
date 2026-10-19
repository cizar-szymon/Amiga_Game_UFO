/* shadowtest - is the table-driven drawShadowHalfFix bit-exact with the one it
 * replaces? Host build (g++), random globe content, many sun directions, both
 * globe sizes (256x200 for 320x200, 576x400 for 640x400) and odd sizes.
 * OLD: copied verbatim from build/apply-amiga-patches.py (5u, "CreateShadowFix shader").
 * NEW: the replacement. Exit code 0 = identical everywhere. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

typedef unsigned char Uint8;
typedef signed short Sint16;
typedef unsigned short Uint16;
typedef signed int Sint32;
typedef unsigned int Uint32;

struct SDL_Surface { void *pixels; int pitch; };
namespace helper { const Uint8 ColorGroup = 15 << 4; const Uint8 ColorShade = 15; }
struct Globe { static Uint8 OCEAN_COLOR; };
Uint8 Globe::OCEAN_COLOR = 192;
struct StaticData { Sint16 shade_gradient[240]; StaticData() {
	for (int i = 0; i < 240; ++i) { int j = i - 120;
		if (j<-66) j=-16; else if (j<-48) j=-15; else if (j<-33) j=-14; else if (j<-22) j=-13;
		else if (j<-15) j=-12; else if (j<-11) j=-11; else if (j<-9) j=-10;
		if (j>120) j=19; else if (j>98) j=18; else if (j>86) j=17; else if (j>74) j=16;
		else if (j>54) j=15; else if (j>38) j=14; else if (j>26) j=13; else if (j>18) j=12;
		else if (j>13) j=11; else if (j>10) j=10; else if (j>8) j=9;
		shade_gradient[i] = j + 16; } } } static_data;
struct CordFix { Sint16 x, y, z; };

/* ------------------------------------------------------------------ OLD -- */
static inline int shadowDotV(const CordFix &e, const CordFix &sun)
{
	const Sint32 dx = (Sint32)e.x - sun.x;
	const Sint32 dy = (Sint32)e.y - sun.y;
	const Sint32 dz = (Sint32)e.z - sun.z;
	const Sint32 n = ((dx * dx) >> 4) + ((dy * dy) >> 4) + ((dz * dz) >> 4);
	const Sint32 x = ((n - (2L << 24)) >> 8) * 125;
	if (x < -(110L << 16)) return -31;
	if (x > (120L << 16)) return 50;
	return static_data.shade_gradient[(int)(x / 65536) + 120];
}

static inline Uint8 shadowApply(Uint8 dest, int v)
{
	if (v > 0)
	{
		const int val = (v > 31) ? 31 : v;
		const int d = dest & helper::ColorGroup;
		if (d == Globe::OCEAN_COLOR || d == Globe::OCEAN_COLOR + 16)
			return (Uint8)(Globe::OCEAN_COLOR + val);
		const int s = val / 3;
		const int e = dest + s;
		if (e > d + helper::ColorShade)
			return (Uint8)(d + helper::ColorShade);
		return (Uint8)e;
	}
	const int d = dest & helper::ColorGroup;
	if (d == Globe::OCEAN_COLOR || d == Globe::OCEAN_COLOR + 16)
		return (Uint8)Globe::OCEAN_COLOR;
	return dest;
}

static void drawShadowHalfFixOld(SDL_Surface *ss, const CordFix *ef, int w2, int h2, const CordFix &sun, const Sint16 *noise, int nsz)
{
	Uint8 *px = (Uint8 *)ss->pixels;
	const int pitch = ss->pitch;
	for (int j = 0; j < h2; j += 2)
	{
		const int rows = (j + 1 < h2) ? 2 : 1;
		const Sint16 *nrow0 = noise + (j % nsz) * nsz;
		const Sint16 *nrow1 = noise + ((j + 1) % nsz) * nsz;
		int n0 = 0;
		for (int i = 0; i < w2; i += 2)
		{
			const int cols = (i + 1 < w2) ? 2 : 1;
			int n1 = n0 + 1; if (n1 >= nsz) n1 = 0;
			const CordFix *e0 = &ef[(size_t)j * w2 + i];
			const int vb = (e0->z != 0) ? shadowDotV(*e0, sun) : 0x7FFF;
			for (int bj = 0; bj < rows; ++bj)
			{
				Uint8 *dp = px + (size_t)(j + bj) * pitch + i;
				const CordFix *eb = &ef[(size_t)(j + bj) * w2 + i];
				const Sint16 *nr = bj ? nrow1 : nrow0;
				const int ni[2] = { n0, n1 };
				for (int bi = 0; bi < cols; ++bi)
				{
					const Uint8 dest = dp[bi];
					if (!dest || eb[bi].z == 0) { dp[bi] = 0; continue; }
					int v = (vb != 0x7FFF) ? vb : shadowDotV(eb[bi], sun);
					v -= nr[ni[bi]];
					dp[bi] = shadowApply(dest, v);
				}
			}
			n0 += 2; if (n0 >= nsz) n0 -= nsz;
		}
	}
}

/* ------------------------------------------------------------------ NEW -- */
#include "shadow_new.inc"

/* ----------------------------------------------------------------- test -- */
static Sint16 q14(double v) { return (Sint16)floor(v * 16384.0 + 0.5); }

int main()
{
	const int sizes[][2] = { { 256, 200 }, { 576, 400 }, { 575, 399 }, { 33, 17 }, { 1, 1 }, { 2, 3 } };
	const Uint8 oceans[] = { 192, 128, 0, 240 };
	std::vector<Sint16> noise(60 * 60);
	unsigned long px = 0, bad = 0;
	srand(12345);
	for (size_t i = 0; i < noise.size(); ++i) noise[i] = rand() % 4;

	for (unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; ++s)
	for (unsigned o = 0; o < sizeof oceans; ++o)
	for (int rep = 0; rep < 12; ++rep)
	{
		const int w = sizes[s][0], h = sizes[s][1], pitch = w + (rep & 3) * 4;
		const double r = (rep % 3 == 0) ? 0.45 * h : (rep % 3 == 1) ? 0.9 * h : 2.5 * h;   /* small / big / zoomed past the edges */
		std::vector<CordFix> ef((size_t)w * h);
		for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
		{
			const double dx = (x + .5 - w / 2) / r, dy = (y + .5 - h / 2) / r, t = dx * dx + dy * dy;
			CordFix f = { 0, 0, 0 };
			if (t < 1.0) { f.x = q14(dx); f.y = q14(dy); f.z = q14(sqrt(1.0 - t)); if (f.z == 0) f.z = 1; }
			ef[(size_t)y * w + x] = f;
		}
		const double a = rep * 0.7 + s, b = rep * 0.37 - 1.0;
		CordFix sun = { q14(cos(a) * cos(b)), q14(sin(b)), q14(sin(a) * cos(b)) };
		std::vector<Uint8> buf((size_t)pitch * h), ref;
		for (size_t i = 0; i < buf.size(); ++i)
		{
			int k = rand() % 10;
			buf[i] = (k == 0) ? 0 : (k < 4) ? (Uint8)(oceans[o] + rand() % 32) : (Uint8)(rand() % 256);
		}
		ref = buf;
		Globe::OCEAN_COLOR = oceans[o];
		SDL_Surface so = { &ref[0], pitch }, sn = { &buf[0], pitch };
		drawShadowHalfFixOld(&so, &ef[0], w, h, sun, &noise[0], 60);
		drawShadowHalfFix(&sn, &ef[0], w, h, sun, &noise[0], 60);
		for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x, ++px)
			if (buf[(size_t)y * pitch + x] != ref[(size_t)y * pitch + x])
			{
				if (bad < 10) printf("DIFF %dx%d ocean %d rep %d at %d,%d: old %d new %d\n", w, h, oceans[o], rep, x, y,
				                     ref[(size_t)y * pitch + x], buf[(size_t)y * pitch + x]);
				++bad;
			}
		/* bytes past the row end must be untouched */
		for (int y = 0; y < h; ++y) for (int x = w; x < pitch; ++x)
			if (buf[(size_t)y * pitch + x] != ref[(size_t)y * pitch + x]) ++bad;
	}
	printf("%lu pixels compared, %lu different\n", px, bad);
	return bad != 0;
}
