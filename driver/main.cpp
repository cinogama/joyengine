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
    while (true)
    {
        for (int i = 0; i < 10; ++i)
        {
            auto w = u.create_world();
            w.close();
        }

        jeecs::selector s;
        s.at(woo)
            .exec([](jeecs::Transform::Translation& trans) {});

        je_clock_sleep_for(0.5);
        woo.add_entity<jeecs::Transform::Translation>();
    }

    u.wait();


    je_clock_sleep_for(1);
}
