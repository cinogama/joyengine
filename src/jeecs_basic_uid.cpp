#define JE_IMPL
#include "jeecs.hpp"

#include <atomic>
#include <random>

#include <cstdint>

namespace jeecs::uid {}

using namespace std;
using namespace jeecs;
using namespace jeecs::uid;

namespace jeecs
{
    namespace
    {
        atomic_uint32_t uid_counter = 1;
        thread_local mt19937 mt = mt19937(random_device()());
    }
}

void je_uid_generate(je_uuid_t *uid)
{
    uniform_int_distribution<uint16_t> dist_ushort(0, UINT16_MAX);
    uniform_int_distribution<uint32_t> dist_uint(0, UINT32_MAX);

    // JUID
    // 4-2-2-2-4
    // 4-2 -> TIME STAMP
    // 2 -> RANDOM
    // 2 -> INC LOW 16
    // 2 -> INC HIGH 16
    // 6 -> RANDOM

    uint32_t count = uid_counter++;
    auto tm_st = je_clock_time_stamp();
    uid->x = (decltype(uid->x))(tm_st >> 16);
    uid->y = (decltype(uid->y))tm_st;
    uid->z = dist_ushort(mt);
    uid->w = (decltype(uid->w))(count);
    uid->u = (decltype(uid->u))(count >> 16);
    uid->v = dist_uint(mt);
}