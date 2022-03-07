/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#include "jeecs.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    rs_init(argc, argv);
    using namespace jeecs;
    using namespace std;


    rs_vm vmm = rs_create_vm();
    bool ok = rs_load_source(vmm, "_example.rsn",
        R"(
import rscene.std;
import rscene.co;

using std;

var co_list = []:array<co>;

func example_co_job(var co_number:int)
{
    for(var i = 0; i<1000000; i+=1)
        ;
    println("Hello, i'm co:", co_number, "JOB DONE!");
}

// Create 30000 coroutine to execute.
for(var i = 0; i<30000; i+=1)
    co_list->add(co(example_co_job, i));

for(var coroutine : co_list)
    while(coroutine->completed())
        sleep(1);

)");
    if (!ok)
        debug::log_error(rs_get_compile_error(vmm, RS_NEED_COLOR));

    rs_run(vmm);

    je_clock_sleep_for(10000);

    std::unordered_set<typing::uid_t> x;

    jeecs::enrty::module_entry();
    {
        game_universe universe = game_universe::create_universe();

        game_world world = universe.create_world();
        world.add_system(typing::type_info::of("jeecs::TranslationUpdatingSystem"));

        auto entity = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalScale,
            Transform::ChildAnchor,
            Transform::LocalToWorld,
            Transform::Translation>();

        auto entity2 = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalScale,
            Transform::LocalToParent,
            Transform::Translation>();

        je_clock_sleep_for(0.5);
        game_universe::destroy_universe(universe);
    }
    je_clock_sleep_for(5.);

    jeecs::enrty::module_leave();
    rs_finish();
}
