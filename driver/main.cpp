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

    // When abort try clear rs-state and module
    at_quick_exit(rs_finish);
    at_quick_exit(jeecs::enrty::module_leave);

    std::unordered_set<typing::uid_t> x;

    jeecs::enrty::module_entry();

    rs_vm v = rs_create_vm();
    if (!rs_load_source(v, "xx.rsn", R"(
import rscene.std;

func operator  (var a:string, var b:int)
{
    var result = "";
    for (var i=0; i<b; i+=1)
        result += a;
    return result;
}

std::println("Helloworld" > 5);

while(true);
)"))
    {
        printf(rs_get_compile_error(v, RS_NEED_COLOR));
    }
    
    rs_run(v);
    rs_close_vm(v);

    jeecs::enrty::module_leave();
    rs_finish();
    return 0;

    // goto debug_endl;
    // while (true)
    {
        game_universe universe = game_universe::create_universe();
        universe.add_shared_system(typing::type_info::of("jeecs::DefaultGraphicPipelineSystem"));

        game_world world = universe.create_world();
        world.add_system(typing::type_info::of("jeecs::TranslationUpdatingSystem"));

        world.attach_shared_system(typing::type_info::of("jeecs::DefaultGraphicPipelineSystem"));

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
            Transform::Translation,
            Renderer::Material,
            Renderer::Shape>();

        universe.wait();
        game_universe::destroy_universe(universe);
    }

    je_clock_sleep_for(1);

debug_endl:

    jeecs::enrty::module_leave();
    rs_finish();
}
