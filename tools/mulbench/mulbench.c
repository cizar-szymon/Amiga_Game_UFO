/* mulbench - what does a 64-bit MULS.L cost on this CPU?
 *
 * gcc -mcpu=68020 compiles x / 3 as a 64-bit MULS.L (multiply by the
 * reciprocal); the 68060 does not implement that form and 68060.library
 * emulates it after a trap. Times N divisions through both code shapes:
 *   div3_mul64: the 64-bit MULS.L sequence gcc emits for 68020
 *   div3_divsl: a 32-bit DIVS.L (what -mtune=68060 emits)
 *   loop_only:  the loop overhead alone
 * Timing: timer.device E-clock. Build: see tools/mulbench/build.sh */
#include <stdio.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>

struct Device *TimerBase;

#define N 1000000L

static long div3_mul64(long n)
{
	long s = 0, i;
	for (i = 1; i <= n; i++) {
		long q;
		/* q = i / 3 the way gcc does it for 68020 */
		__asm__ volatile (
			"move.l %1,%%d1\n\t"
			"muls.l #0x55555556,%%d0:%%d1\n\t"
			"move.l %1,%%d1\n\t"
			"add.l %%d1,%%d1\n\t"
			"subx.l %%d1,%%d1\n\t"
			"sub.l %%d1,%%d0\n\t"
			"move.l %%d0,%0"
			: "=d" (q) : "d" (i) : "d0", "d1", "cc");
		s += q;
	}
	return s;
}

static long div3_divsl(long n)
{
	long s = 0, i;
	for (i = 1; i <= n; i++) {
		long q;
		__asm__ volatile (
			"move.l %1,%%d0\n\t"
			"divs.l #3,%%d0\n\t"
			"move.l %%d0,%0"
			: "=d" (q) : "d" (i) : "d0", "cc");
		s += q;
	}
	return s;
}

static long loop_only(long n)
{
	long s = 0, i;
	for (i = 1; i <= n; i++) {
		long q;
		__asm__ volatile ("move.l %1,%0" : "=d" (q) : "d" (i));
		s += q;
	}
	return s;
}

static ULONG ef;

static double now_us(void)
{
	struct EClockVal ev;
	ef = ReadEClock(&ev);
	return ((double)ev.ev_hi * 4294967296.0 + (double)ev.ev_lo) * 1000000.0 / (double)ef;
}

int main(void)
{
	struct timerequest tr;
	double t0, t1, t2, t3;
	long a, b, c;

	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0) != 0) {
		printf("no timer.device\n");
		return 20;
	}
	TimerBase = tr.tr_node.io_Device;

	Forbid();
	t0 = now_us(); c = loop_only(N);
	t1 = now_us(); a = div3_mul64(N);
	t2 = now_us(); b = div3_divsl(N);
	t3 = now_us();
	Permit();

	printf("N=%ld  checksums %ld %ld %ld (first two must match)\n", N, a, b, c);
	printf("loop only       : %8.0f us  %6.3f us/iter\n", t1 - t0, (t1 - t0) / N);
	printf("64-bit muls.l   : %8.0f us  %6.3f us/iter  (+%.3f over loop)\n", t2 - t1, (t2 - t1) / N, ((t2 - t1) - (t1 - t0)) / N);
	printf("32-bit divs.l   : %8.0f us  %6.3f us/iter  (+%.3f over loop)\n", t3 - t2, (t3 - t2) / N, ((t3 - t2) - (t1 - t0)) / N);
	CloseDevice((struct IORequest *)&tr);
	return 0;
}
