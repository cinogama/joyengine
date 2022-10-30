#define JE_IMPL
#include "jeecs.hpp"

#include <cstdlib>

void* je_mem_alloc(size_t sz)
{
    assert(sz != 0);
    return malloc(sz);
}
void* je_mem_realloc(void* mem, size_t sz)
{
    return realloc(mem, sz);
}
void je_mem_free(void* ptr)
{
    return free(ptr);
}