/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#include "jeecs.hpp"

namespace jeecs // Transform
{
    namespace Transform
    {
        // An entity without childs and parent will contain these components:
        // LocalPosition/LocalRotation/LocalScale and using LocalToWorld to apply
        // local transform to Translation
        // If an entity have childs, it will have ChildAnchor 
        // If an entity have parent, it will have Parent LocalToParent and without
        // LocalToWorld.

        struct LocalPosition
        {
            float x = 0, y = 0, z = 0;
        };
        struct LocalRotation
        {
            float x = 0, y = 0, z = 0, w = 1.f;
        };
        struct LocalScale
        {
            float x = 1.f, y = 1.f, z = 1.f;
        };

        struct ChildAnchor
        {
            static constexpr size_t INVALID_ANCHOR_ID = (size_t)(-1);
            size_t anchor_id = INVALID_ANCHOR_ID;
        };

        struct LocalToParent
        {
            size_t parent_anchor_id;
        };

        struct LocalToWorld
        {
        };

        struct Translation
        {
            float object2world[16] = {};
            float rotation[16] = {};
        };
    }

}

struct TranslationUpdatingSystem :public jeecs::game_system
{
    TranslationUpdatingSystem(jeecs::game_world world) :jeecs::game_system(world)
    {
        register_system_func(&TranslationUpdatingSystem::example,
            {
                system_read(&ababa)
            });
        
    }

    void example(read<jeecs::Transform::LocalPosition> pos)
    {
        printf("x");
    }
};

int main()
{
    using namespace jeecs;
    using namespace std;

    jeecs::enrty::module_entry();

    game_universe universe = game_universe::create_universe();

    game_world world = universe.create_world();
    world.add_system<TranslationUpdatingSystem>();

    world.add_entity<
        Transform::LocalPosition, 
        Transform::LocalRotation, 
        Transform::LocalScale>();

    je_clock_sleep_for(5.);
    game_universe::destroy_universe(universe);
    je_clock_sleep_for(5.);

    jeecs::enrty::module_leave();
}
