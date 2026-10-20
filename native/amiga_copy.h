#ifndef AMIGA_COPY_H
#define AMIGA_COPY_H

/* Rectangle copy, row by row - see amiga_copy.c. */

#ifdef __cplusplus
extern "C" {
#endif

/* Copy h rows of w bytes; rows dpitch / spitch bytes apart. */
void amiga_copy_rows(void *dst, long dpitch, const void *src, long spitch, long w, long h);

/* The same without the cache push, for a copy that must not make OS calls
 * (inside a P96 bitmap lock): the caller has pushed the source with
 * amiga_copy_push() and the destination is not cached (VRAM). */
void amiga_copy_rows_pushed(void *dst, long dpitch, const void *src, long spitch, long w, long h);

/* Push h rows of w bytes (pitch apart) out of the data cache. */
void amiga_copy_push(const void *p, long pitch, long w, long h);

/* While on, amiga_copy_rows pushes nothing: for copies inside a display lock
 * whose sources were pushed before it (amiga_copy_push), into uncached VRAM. */
void amiga_copy_nopush(int on);

/* memmove for big, overlapping blocks (any distance, MOVE16 when it is a
 * multiple of 16 on a 68040/68060). */
void amiga_move(void *dst, const void *src, unsigned long n);

/* 1 when amiga_copy_rows may use MOVE16 (68040/68060), else 0. */
int amiga_copy_move16(void);

#ifdef __cplusplus
}
#endif

#endif
