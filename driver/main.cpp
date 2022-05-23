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
    je_init(argc, argv);
    using namespace jeecs;
    using namespace std;

    rs_vm v = rs_create_vm();
    rs_load_source(v, "x.rsn", R"(
import rscene.std;
import je.shader;

func main()
{
    while (true)
    {
        var a = float(1);
        var b = a + float(3.14);
    }
}
main();
)");
    std::cout << rs_get_compile_error(v, RS_NEED_COLOR) << std::endl;
    rs_run(v);

    jeecs::enrty::module_entry();

    at_quick_exit(jeecs::enrty::module_leave);
    atexit(jeecs::enrty::module_leave);

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
}
