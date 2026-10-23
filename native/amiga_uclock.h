#ifndef AMIGA_UCLOCK_H
#define AMIGA_UCLOCK_H
#ifdef __cplusplus
extern "C" {
#endif
/* microseconds since first call (timer.device E-clock); 0 if unavailable */
unsigned long amiga_uclock_us(void);
unsigned long amiga_uclock_freq(void);
/* the raw E-clock count (wraps at 2^32): cheap, for hot probes */
unsigned long amiga_uclock_raw(void);
#ifdef __cplusplus
}
#endif
#endif
