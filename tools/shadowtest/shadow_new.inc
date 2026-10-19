/* AMIGA-PORT: shadowApply(dest, v) as a table. The result depends only on
 * dest and min(v, 31), and not on v at all when v <= 0, so [k][dest] - k 0
 * for v <= 0, min(v, 31) otherwise - holds every possible answer: 8 KB,
 * built from shadowApply itself, again whenever the ruleset's OCEAN_COLOR
 * is another one. The per-pixel work becomes one table read instead of
 * shadowApply's tests and its division by 3 (on the 68060 a trapped
 * 64-bit MULS.L, see the porting notes). */
static Uint8 s_shadeLut[32][256];
static int s_shadeLutOcean = -1;

static void shadeLutBuild()
{
	for (int k = 0; k < 32; ++k)
		for (int d = 0; d < 256; ++d)
			s_shadeLut[k][d] = shadowApply((Uint8)d, k);
	s_shadeLutOcean = Globe::OCEAN_COLOR;
}

/* Where row r of the normals table is inside the disc (z != 0): [*x0, *x1).
 * The disc is convex and centred on the surface (drawShadowHalfFix is only
 * used then), so the inside pixels of a row are one interval around the
 * centre column, and its ends are found by bisection - ~20 table reads per
 * row instead of one per pixel. */
static void discSpan(const CordFix *row, int w2, int *x0, int *x1)
{
	const int c = w2 / 2;
	if (row[c].z == 0) { *x0 = *x1 = c; return; }   /* the row misses the disc */
	int lo = 0, hi = c;                 /* first inside in [lo, hi]: row[hi] is inside */
	while (lo < hi) { const int m = (lo + hi) / 2; if (row[m].z != 0) hi = m; else lo = m + 1; }
	*x0 = lo;
	lo = c; hi = w2;                    /* first outside in (lo, hi]: row[lo] is inside */
	while (hi - lo > 1) { const int m = (lo + hi) / 2; if (row[m].z != 0) lo = m; else hi = m; }
	*x1 = hi;
}

/* Walks whole rows: the block values of a row pair first (one dot product
 * per 2x2 block, from its top-left pixel), then each row pixel by pixel
 * through s_shadeLut. Outside the disc span the pixels are cleared, inside
 * it the normals table is not read at all except for block anchors (even
 * rows) and the few rim pixels whose anchor is outside - the table is 6
 * bytes a pixel, 1.4 MB at 640x400, more than the Warp's L2. Bit for bit
 * the same picture as the 2x2-block walk with shadowApply per pixel it
 * replaces (checked on the host with random globes of both sizes). */
static std::vector<int> s_shadeBlocks;

static void drawShadowHalfFix(SDL_Surface *ss, const CordFix *ef, int w2, int h2, const CordFix &sun, const Sint16 *noise, int nsz)
{
	if (s_shadeLutOcean != Globe::OCEAN_COLOR) shadeLutBuild();
	Uint8 *px = (Uint8 *)ss->pixels;
	const int pitch = ss->pitch;
	const int nb = (w2 + 1) / 2;
	if ((int)s_shadeBlocks.size() < nb) s_shadeBlocks.resize(nb);
	int *vb = &s_shadeBlocks[0];
	for (int j = 0; j < h2; j += 2)
	{
		const int rows = (j + 1 < h2) ? 2 : 1;
		int sx0[2], sx1[2];
		discSpan(ef + (size_t)j * w2, w2, &sx0[0], &sx1[0]);
		if (rows == 2) discSpan(ef + (size_t)(j + 1) * w2, w2, &sx0[1], &sx1[1]);
		else { sx0[1] = sx0[0]; sx1[1] = sx1[0]; }
		/* block values for the blocks either row's span touches; 0x7FFF where
		 * the anchor (row j, even column) is outside (shadowDotV never
		 * returns it): those pixels compute their own */
		{
			const CordFix *e0 = ef + (size_t)j * w2;
			const int b0 = (sx0[0] < sx0[1] ? sx0[0] : sx0[1]) >> 1;
			const int b1 = ((sx1[0] > sx1[1] ? sx1[0] : sx1[1]) + 1) >> 1;
			for (int b = b0; b < b1; ++b)
				vb[b] = (2 * b >= sx0[0] && 2 * b < sx1[0]) ? shadowDotV(e0[2 * b], sun) : 0x7FFF;
		}
		for (int bj = 0; bj < rows; ++bj)
		{
			Uint8 *dp = px + (size_t)(j + bj) * pitch;
			const CordFix *eb = ef + (size_t)(j + bj) * w2;
			const Sint16 *nr = noise + ((j + bj) % nsz) * nsz;
			const int x0 = sx0[bj], x1 = sx1[bj];
			if (x0 > 0) memset(dp, 0, (size_t)x0);            /* outside the disc */
			if (x1 < w2) memset(dp + x1, 0, (size_t)(w2 - x1));
			int n = x0 % nsz;
			for (int i = x0; i < x1; ++i)
			{
				const Uint8 dest = dp[i];
				if (dest != 0)
				{
					int v = vb[i >> 1];
					if (v == 0x7FFF) v = shadowDotV(eb[i], sun);
					v -= nr[n];
					dp[i] = s_shadeLut[v <= 0 ? 0 : (v > 31 ? 31 : v)][dest];
				}
				if (++n == nsz) n = 0;
			}
		}
	}
}
