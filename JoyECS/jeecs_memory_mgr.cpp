#define JE_IMPL
#include "jeecs.hpp"

#include <cstdlib>

void* je_mem_alloc(size_t sz)
{
    return malloc(sz);
}
void je_mem_free(void* ptr)
{
    return free(ptr);
}