// amiga_heapguard_new.cpp - operator new/delete for the checking allocator
// (amiga_heapguard.c): the allocation site is the game code that called new,
// not the inside of libstdc++'s operator new.
#include <cstddef>
#include <new>

extern "C" void *hg_alloc(std::size_t n, void *site);
extern "C" void hg_free(void *p, void *site);

void *operator new(std::size_t n)
{
	void *p = hg_alloc(n ? n : 1, __builtin_return_address(0));
	if (!p) throw std::bad_alloc();
	return p;
}
void *operator new[](std::size_t n)
{
	void *p = hg_alloc(n ? n : 1, __builtin_return_address(0));
	if (!p) throw std::bad_alloc();
	return p;
}
void *operator new(std::size_t n, const std::nothrow_t &) throw() { return hg_alloc(n ? n : 1, __builtin_return_address(0)); }
void *operator new[](std::size_t n, const std::nothrow_t &) throw() { return hg_alloc(n ? n : 1, __builtin_return_address(0)); }
void operator delete(void *p) throw() { hg_free(p, __builtin_return_address(0)); }
void operator delete[](void *p) throw() { hg_free(p, __builtin_return_address(0)); }
void operator delete(void *p, std::size_t) throw() { hg_free(p, __builtin_return_address(0)); }
void operator delete[](void *p, std::size_t) throw() { hg_free(p, __builtin_return_address(0)); }
