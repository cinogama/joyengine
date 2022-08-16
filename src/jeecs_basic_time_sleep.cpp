#define JE_IMPL
#include "jeecs.hpp"

#include <chrono>
#include <thread>

std::chrono::high_resolution_clock _hs_clock;
auto _start_time = _hs_clock.now();
double _sleep_suppression = 0.001;

double je_clock_time()
{
    using namespace std;
    return (_hs_clock.now() - _start_time) / 1.0s;
}

std::chrono::system_clock _sys_clock;

jeecs::typing::ms_stamp_t je_clock_time_stamp()
{
    using namespace std;
    return (jeecs::typing::ms_stamp_t)(_sys_clock.now().time_since_epoch() / 1ms);
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

    std::this_thread::sleep_for((time - _sleep_suppression) * 1.0s);

    while (je_clock_time() < current_time_point + time);
}
