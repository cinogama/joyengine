#define JE_IMPL
#include "jeecs/clock.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_basic_bootup.hpp"

#include <chrono>
#include <thread>

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
#   include <Windows.h>
#endif

namespace jeecs::time {}

using namespace std;
using namespace std::chrono;
using namespace jeecs;
using namespace jeecs::time;

namespace jeecs::time
{
    namespace
    {
        steady_clock::time_point epoch_point;
        double sleep_suppression;
    }

    void bootup(int, char**)
    {
        epoch_point = steady_clock::now();
        sleep_suppression = 0.0005;
    }
    void shutdown()
    {
    }
}

double je_clock_sleep_suppression_get()
{
    return sleep_suppression;
}

void je_clock_sleep_suppression_set(double v)
{
    sleep_suppression = v;
}

double je_clock_time()
{
    using namespace std;
    return (steady_clock::now() - epoch_point) / 1.0s;
}
je_timestamp_ms_t je_clock_time_stamp()
{
    using namespace std;
    return (je_timestamp_ms_t)(
        steady_clock::now().time_since_epoch() / 1ms);
}

void je_clock_sleep_until(double time)
{
    je_clock_sleep_for(time - je_clock_time());
}

void je_clock_sleep_for(double time)
{
    using namespace std;
    auto current_time_point = je_clock_time();

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
    // 这里将时钟精度上调到最大限度，这样可以让画面更加稳定
    // 不过这么做也会导致功耗上升（根据手册）并影响CPU的频率和节能
    // https://learn.microsoft.com/zh-cn/windows/win32/api/timeapi/nf-timeapi-timebeginperiod

    auto _result = timeBeginPeriod(1);
    assert(_result == TIMERR_NOERROR);
#endif
    this_thread::sleep_for((time - sleep_suppression) * 1s);

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
    _result = timeEndPeriod(1);
    assert(_result == TIMERR_NOERROR);
#endif

    while (je_clock_time() < current_time_point + time)
        this_thread::yield();
}
