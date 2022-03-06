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

    std::unordered_set<typing::uid_t> x;

    jeecs::enrty::module_entry();
    {
        game_universe universe = game_universe::create_universe();

        game_world world = universe.create_world();
        world.add_system<TranslationUpdatingSystem>();

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
