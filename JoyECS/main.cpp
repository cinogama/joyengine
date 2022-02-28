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
    struct Translation
    {
        float local2world[16] = {};
        float rotation[16] = {};
    };
    struct LocalToParent
    {
        size_t parent_id_in_manager;
    };
}

struct TranslationUpdatingSystem :public jeecs::game_system
{
    TranslationUpdatingSystem(jeecs::game_world world)
        : jeecs::game_system(world)
    {
        register_system_func(&TranslationUpdatingSystem::apply_local_position_to_l2w,
            {
                except<jeecs::LocalToParent>()
            });
        register_system_func(&TranslationUpdatingSystem::apply_local_scale_to_l2w,
            {
                except<jeecs::LocalToParent>()
            });
        register_system_func(&TranslationUpdatingSystem::apply_local_rotation_to_l2w,
            {
                except<jeecs::LocalToParent>()
            });
        register_system_func(&TranslationUpdatingSystem::apply_non_rotation_to_l2w,
            {
                except<jeecs::LocalToParent>(),
                except<jeecs::LocalRotation>()
            });
    }

    void apply_local_position_to_l2w(
        read<jeecs::LocalPosition> pos,     // read LocalPosition
        jeecs::Translation* local2world    // write LocalToWorld
    )
    {
        local2world->local2world[3 + 0 * 4] = pos->x;
        local2world->local2world[3 + 1 * 4] = pos->y;
        local2world->local2world[3 + 2 * 4] = pos->z;
    }

    void apply_local_scale_to_l2w(
        read<jeecs::LocalScale> scale,      // read LocalScale
        jeecs::Translation* local2world    // write LocalToWorld
    )
    {
        local2world->local2world[0 + 0 * 4] = scale->x;
        local2world->local2world[1 + 1 * 4] = scale->y;
        local2world->local2world[2 + 2 * 4] = scale->z;
        local2world->local2world[3 + 3 * 4] = 1.0f;
    }

    void apply_local_rotation_to_l2w(
        read<jeecs::LocalRotation> rotation,      // read LocalScale
        jeecs::Translation* local2world    // write LocalToWorld
    )
    {
        //TODO: generate rotation matrix for entity
    }

    void apply_non_rotation_to_l2w(
        jeecs::Translation* local2world    // write LocalToWorld
    )
    {
        //TODO: generate rotation matrix for non-rotation entity
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

    world.add_entity<LocalPosition, LocalRotation, LocalScale, Translation, LocalToParent>();

    je_clock_sleep_for(5.);
    game_universe::destroy_universe(universe);
    je_clock_sleep_for(5.);

    jeecs::enrty::module_leave();
}
