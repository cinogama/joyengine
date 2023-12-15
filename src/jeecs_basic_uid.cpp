#define JE_IMPL
#include "jeecs.hpp"

#include <atomic>
#include <random>

#include <cstdint>

#ifdef JE_OS_WINDOWS
#   include <Windows.h>
#   include <iphlpapi.h>
#   pragma comment(lib, "IPHLPAPI.lib")
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <sys/ioctl.h>
#endif

std::atomic_uint32_t _je_uid_counter = 1;

uint16_t _je_uid_mac_low16_or_rand16;
uint32_t _je_uid_mac_high32_or_rand32;

void je_uid_init()
{
    // Get MAC Address
#ifdef JE_OS_WINDOWS
    IP_ADAPTER_INFO AdapterInfo[16]; // Allocate information
    DWORD           dwBufLen = sizeof(AdapterInfo); // Save memory size of buffer
    DWORD           dwStatus = GetAdaptersInfo( // Call GetAdapterInfo
        AdapterInfo, // [out] buffer to receive data
        &dwBufLen); // [in] size of receive data buffer
    assert(dwStatus == ERROR_SUCCESS); // Verify return value is
    PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; // Contains pointer to

    while (pAdapterInfo != nullptr && pAdapterInfo->Next != nullptr)
        pAdapterInfo = pAdapterInfo->Next;

    // current adapter info
    if (pAdapterInfo)
    {
        _je_uid_mac_low16_or_rand16 = 
            (uint16_t)pAdapterInfo->Address[0] << 8 
            | (uint16_t)pAdapterInfo->Address[1];
        _je_uid_mac_high32_or_rand32 = 
            (uint32_t)pAdapterInfo->Address[2] << 24 
            | (uint32_t)pAdapterInfo->Address[3] << 16 
            | (uint32_t)pAdapterInfo->Address[4] << 8 
            | (uint32_t)pAdapterInfo->Address[5];
        return;
    }
#elif defined(JE_OS_LINUX) || defined(JE_OS_ANDROID)
    struct ifreq ifr;
    int          sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock != -1)
    {
        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
        if (0 == ioctl(sock, SIOCGIFHWADDR, &ifr))
        {
            close(sock);
            _je_uid_mac_low16_or_rand16 = 
                (uint16_t)ifr.ifr_hwaddr.sa_data[0] << 8 
                | (uint16_t)ifr.ifr_hwaddr.sa_data[1];
            _je_uid_mac_high32_or_rand32 = 
                (uint32_t)ifr.ifr_hwaddr.sa_data[2] << 24 
                | (uint32_t)ifr.ifr_hwaddr.sa_data[3] << 16 
                | (uint32_t)ifr.ifr_hwaddr.sa_data[4] << 8 
                | (uint32_t)ifr.ifr_hwaddr.sa_data[5];
            return;
        }
        close(sock);
    }
#endif

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
