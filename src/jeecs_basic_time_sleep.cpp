#define JE_IMPL
#include "jeecs.hpp"

#include <chrono>
#include <thread>

auto _start_time = std::chrono::steady_clock::now();
double _sleep_suppression = 0.010;

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

void je_clock_suppress_sleep(double sup_stax)
{
    _sleep_suppression = sup_stax;
}

void je_clock_sleep_for(double time)
{
    using namespace std;
    auto current_time_point = je_clock_time();

    std::this_thread::sleep_for((time - _sleep_suppression) * 1s);
    while (je_clock_time() < current_time_point + time);
}
