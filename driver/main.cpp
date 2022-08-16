/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <iostream>

#include <future>

struct mysys : jeecs::game_system
{
    mysys(jeecs::game_world w)
        : jeecs::game_system(w)
    {

    }
    void PreUpdate()
    {
        static double last_tm = 0.;
        double cur = je_clock_time();

        jeecs::debug::log_fatal("CUR TIME pre= %f(cur=%f)", cur - last_tm, cur);
        last_tm = cur;
    }
    void Update()
    {
        //jeecs::debug::log_fatal("CUR TIME cur= %f", je_clock_time());
    }
    void LateUpdate()
    {
        //jeecs::debug::log_fatal("CUR TIME lat= %f", je_clock_time());
    }
};

int main(int argc, char** argv)
{
    je_init(argc, argv);
    atexit(je_finish);
    at_quick_exit(je_finish);

    using namespace jeecs;
    using namespace std;

    jeecs::enrty::module_entry();

    atexit(jeecs::enrty::module_leave);
    at_quick_exit(jeecs::enrty::module_leave);

    // jedbg_editor();
    game_universe u(je_ecs_universe_create());
    auto woo = u.create_world();
    woo.add_system<mysys>();
   
    u.wait();

    je_clock_sleep_for(1);
}
