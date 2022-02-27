#define JE_IMPL
#include "jeecs.hpp"

#include <chrono>
#include <thread>

std::chrono::system_clock _sys_clock;
auto _start_time = _sys_clock.now();

double je_clock_time()
{
    return ((_sys_clock.now() - _start_time).count() *
        std::chrono::system_clock::duration::period::num) /
        (double)std::chrono::system_clock::duration::period::den;
}

void je_clock_sleep_until(double time)
{
    using namespace std;
    std::this_thread::sleep_until(_start_time + time * 1s - 0.001s);

    while (je_clock_time() < time);
        std::this_thread::yield();
}

void je_clock_sleep_for(double time)
{
    double current_time = je_clock_time();
    je_clock_sleep_until(current_time + time);
}