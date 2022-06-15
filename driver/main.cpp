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

    if (1)
    {
        game_universe universe = game_universe::create_universe();
        jedbg_set_editor_universe(universe.handle());

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

        entity.get_component<Transform::LocalRotation>()->rot = jeecs::math::quat(0, 0, 0);
        entity2.get_component<Transform::LocalPosition>()->pos = jeecs::math::vec3(0.25, 0, 0.5);
        entity2.get_component<Transform::LocalToParent>()->parent_uid =
            entity.get_component<Transform::ChildAnchor>()->anchor_uid;

        auto entity3 = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalToWorld,
            Transform::Translation,
            Camera::Clip,
            Camera::PerspectiveProjection,
            Camera::Projection,
            Camera::Viewport>();

        entity3.get_component<Transform::LocalRotation>()->rot = math::quat(0, 0, 15);
        entity3.get_component<Transform::LocalPosition>()->pos = math::vec3(0, 0, -10);

        universe.wait();
        game_universe::destroy_universe(universe);
    }

    je_clock_sleep_for(1);
}
