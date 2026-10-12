#define JE_IMPL
#include "jeecs.hpp"

#include <cstdlib>

/* OPTIONAL: If allocation fails */ void *je_mem_alloc(size_t sz)
{
    return malloc(sz);
}
/* OPTIONAL: If allocation fails */ void *je_mem_realloc(/* OPTIONAL */ void *mem, size_t sz)
{
    return realloc(mem, sz);
}
void je_mem_free(/* OPTIONAL */ void *ptr)
{
    return free(ptr);
}