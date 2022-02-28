#define JE_IMPL
#include "jeecs.hpp"

#include <chrono>
#include <thread>

std::chrono::high_resolution_clock _sys_clock;
auto _start_time = _sys_clock.now();
double _sleep_suppression = 0.0001;

double je_clock_time()
{
    using namespace std;
    return (_sys_clock.now() - _start_time) / 1.0s;
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
