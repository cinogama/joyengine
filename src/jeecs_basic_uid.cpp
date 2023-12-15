#define JE_IMPL
#include "jeecs.hpp"

#include <atomic>
#include <random>

#include <cstdint>

std::atomic_uint32_t _je_uid_counter = 1;

uint16_t _je_uid_mac_low16_or_rand16;
uint32_t _je_uid_mac_high32_or_rand32;

void je_uid_init()
{
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<uint16_t> dist_ushort(1, UINT16_MAX);
    std::uniform_int_distribution<uint32_t> dist_uint(1, UINT32_MAX);

    _je_uid_mac_low16_or_rand16 = dist_ushort(mt);
    _je_uid_mac_high32_or_rand32 = dist_uint(mt);
}

jeecs::typing::uid_t je_uid_generate()
{
    // JUID
    // 4-2-2-6(2-4)
    // 4 -> TIME STAMP(HIGH 32)
    // 2 -> TIME STAMP(LOW 16)
    // 2 -> INC LOW 16
    // 2 -> INC HIGH 16
    // 2 -> MAC/RANDOM
    // 4 -> MAC/RANDOM

    jeecs::typing::uid_t uid;

    uint32_t    count = _je_uid_counter++;
    auto        tm_st = je_clock_time_stamp();

    uid.x32 = (decltype(uid.x32))(tm_st >> 16);
    uid.y16 = (decltype(uid.y16))tm_st;
    uid.z16 = (decltype(uid.z16))(count);
    uid.w16 = (decltype(uid.w16))(count >> 16);
    uid.u16 = _je_uid_mac_low16_or_rand16;
    uid.v32 = _je_uid_mac_high32_or_rand32;

    return uid;
}
