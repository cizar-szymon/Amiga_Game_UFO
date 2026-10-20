/* The per-frame heap check is a no-op unless the build links the checking
 * allocator instead (AMIGA_HEAPGUARD=1, native/amiga_heapguard.c). */
void amiga_heapguard_check(const char *where)
{
	(void)where;
}
