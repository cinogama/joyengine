#define JE_IMPL
#include "jeecs.hpp"

#include <chrono>
#include <thread>

#ifdef JE_OS_WINDOWS
#include <Windows.h>
#endif

auto _start_time = std::chrono::steady_clock::now();

// 此值应该是是操作系统最小分片时间的二分之一，单位是秒
// Linux平台下尚未考虑，Windows下设置了一毫秒的间隔
// 因此此处是0.0005
double _sleep_suppression = 0.0005; 

double je_clock_get_sleep_suppression()
{
    return _sleep_suppression;
}

void je_clock_set_sleep_suppression(double v)
{
    _sleep_suppression = v;
}

double je_clock_time()
{
    using namespace std;
    return (std::chrono::steady_clock::now() - _start_time) / 1.0s;
}
jeecs::typing::ms_stamp_t je_clock_time_stamp()
{
    using namespace std;
    return (jeecs::typing::ms_stamp_t)(std::chrono::steady_clock::now().time_since_epoch() / 1ms);
}

void je_clock_sleep_until(double time)
{
    je_clock_sleep_for(time - je_clock_time());
}

void je_clock_sleep_for(double time)
{
    using namespace std;
    auto current_time_point = je_clock_time();

#ifdef JE_OS_WINDOWS
    // 这里将时钟精度上调到最大限度，这样可以让画面更加稳定
    // 不过这么做也会导致功耗上升（根据手册）并影响CPU的频率和节能
    // https://learn.microsoft.com/zh-cn/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
    auto _result = timeBeginPeriod(1);
    assert(_result == TIMERR_NOERROR);
#endif
    std::this_thread::sleep_for((time - _sleep_suppression) * 1s);

    while (je_clock_time() < current_time_point + time)
        std::this_thread::yield();

#ifdef JE_OS_WINDOWS
    _result = timeEndPeriod(1);
    assert(_result == TIMERR_NOERROR);
#endif
}
