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
    at_quick_exit(je_finish);

    using namespace jeecs;
    using namespace std;

    jeecs::enrty::module_entry();
    at_quick_exit(jeecs::enrty::module_leave);

    if (1)
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

        entity2.get_component<Transform::LocalToParent>()->parent_uid =
            entity.get_component<Transform::ChildAnchor>()->anchor_uid;

        auto entity3 = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalToWorld,
            Transform::Translation,
            Transform::InverseTranslation,
            Renderer::OrthoCamera>();

        universe.wait();
        game_universe::destroy_universe(universe);
    }

    je_clock_sleep_for(1);
    je_finish();
}
