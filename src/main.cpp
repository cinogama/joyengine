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
    rs_load_source(vmm, "_example.rsn", "import rscene.std; std::println(\"Helloworld\");");
    rs_run(vmm);
    rs_close_vm(vmm);


    std::unordered_set<typing::uid_t> x;

    jeecs::enrty::module_entry();

    game_universe universe = game_universe::create_universe();
    universe.add_shared_system(typing::type_info::of("jeecs::DefaultGraphicPipelineSystem"));

    game_world world = universe.create_world();
    world.add_system(typing::type_info::of("jeecs::TranslationUpdatingSystem"));

    universe.attach_shared_system_to(typing::type_info::of("jeecs::DefaultGraphicPipelineSystem"), world);

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

    je_clock_sleep_for(1);

    jeecs::enrty::module_leave();
    rs_finish();
}
