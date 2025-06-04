#define JE_IMPL
#include "jeecs.hpp"

#include <atomic>
#include <random>

#include <cstdint>

std::atomic_uint32_t _uid_counter = 1;

void je_uid_generate(jeecs::typing::uid_t *uid)
{
    static std::random_device rd;
    static std::mt19937 mt(rd());
    static std::uniform_int_distribution<uint16_t> dist_ushort(0, UINT16_MAX);
    static std::uniform_int_distribution<uint32_t> dist_uint(0, UINT32_MAX);

    // JUID
    // 4-2-2-2-4
    // 4-2 -> TIME STAMP
    // 2 -> RANDOM
    // 2 -> INC LOW 16
    // 2 -> INC HIGH 16
    // 6 -> RANDOM

    uint32_t count = _uid_counter++;
    auto tm_st = je_clock_time_stamp();
    uid->x = (decltype(uid->x))(tm_st >> 16);
    uid->y = (decltype(uid->y))tm_st;
    uid->z = dist_ushort(mt);
    uid->w = (decltype(uid->w))(count);
    uid->u = (decltype(uid->u))(count >> 16);
    uid->v = dist_uint(mt);
}