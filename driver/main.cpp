/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

struct AAA : jeecs::game_system
{
    AAA(jeecs::game_world w) : jeecs::game_system(w)
    {
        printf("CREATED\n");
    }
    ~AAA()
    {
        printf("DESTROYED\n");
    }
};
struct CCC
{
    jeecs::game_world w = nullptr;
    CCC()
    {
        printf("C CREATED\n");
    }
    CCC(CCC&&)
    {
        printf("C MOVED\n");
    }
    ~CCC()
    {
        printf("C DESTROYED\n");
        w.add_entity<CCC>();
    }
};
struct BBB : jeecs::game_system
{
    BBB(jeecs::game_world w) : jeecs::game_system(w)
    {
    }
    ~BBB()
    {
        auto e = get_world().add_entity<CCC>();
        e.get_component<CCC>()->w = get_world();
        get_world().add_system<AAA>();
    }
};


int main(int argc, char** argv)
{
    je_init(argc, argv);
    {
        using namespace jeecs;
        using namespace std;

        typing::type_info::of<AAA>("AAA");
        typing::type_info::of<BBB>("BBB");
        typing::type_info::of<CCC>("CCC");
        while (1)
        {
            game_universe u(je_ecs_universe_create());
            u.create_world().add_system<BBB>();
            je_clock_sleep_for(2.0);
            u.stop();
            u.wait();
            game_universe::destroy_universe(u);
        }

        enrty::module_entry();
        {
            jedbg_main_script_entry();
        }
        enrty::module_leave();
    }
    je_finish();
}
