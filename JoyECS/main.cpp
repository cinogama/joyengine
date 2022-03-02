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
        // If an entity have parent, it will have LocalToParent and without
        // LocalToWorld.
        /*
                                                    Parent's Translation--\
                                                         /(apply)          \
                                                        /                   \
                                 LocalToParent-----LocalToParent             >Translation (2x  4x4 matrix)
                                                 yes  /                     /
        LocalPosition------------>(HAVE_PARENT?)-----/                     /
                       /    /              \ no                           /
        LocalScale----/    /                ---------> LocalToWorld------/
                          /
        LocalRotation----/                                (LocalToXXX) Still using 2x 3 vector + 1x quat

        */

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
            jeecs::typing::uid_t anchor_id = je_uid_generate();
        };

        struct LocalToParent
        {
            float x, y, z;
            float scale_x, scale_y, scale_z;
            float rx, ry, rz, rw;

            jeecs::typing::uid_t parent_uid;
        };

        struct LocalToWorld
        {
            float x, y, z;
            float scale_x, scale_y, scale_z;
            float rx, ry, rz, rw;
        };

        struct Translation
        {
            float object2world[16] = {};
            float rotation[16] = {};
        };
    }

}

using namespace jeecs;
using namespace Transform;

struct TranslationUpdatingSystem :public game_system
{
    struct anchor
    {
        const Translation* m_parent_translation;
    };

    std::unordered_map<typing::uid_t, anchor> m_anchor_list;

    TranslationUpdatingSystem(game_world world) :game_system(world)
    {
        register_system_func(&TranslationUpdatingSystem::PreUpdateChildAnchor,
            {
                system_write(&m_anchor_list),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateChildAnchor,
            {
                system_write(&m_anchor_list),
                after(&TranslationUpdatingSystem::PreUpdateChildAnchor),
            });
        /////////////////////////////////////////////////////////////////////////////////
        register_system_func(&TranslationUpdatingSystem::UpdateLocalPositionToWorld,
            {
                except<LocalToParent>(),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateLocalScaleToWorld,
            {
                except<LocalToParent>(),
                after(&TranslationUpdatingSystem::UpdateLocalPositionToWorld),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateLocalRotationToWorld,
            {
                except<LocalToParent>(),
                after(&TranslationUpdatingSystem::UpdateLocalScaleToWorld),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateWorldToTranslation,
            {
                except<LocalToParent>(),
            });
        /////////////////////////////////////////////////////////////////////////////////
        register_system_func(&TranslationUpdatingSystem::UpdateLocalPositionToParent,
            {
                except<LocalToWorld>(),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateLocalScaleToParent,
            {
                except<LocalToWorld>(),
                after(&TranslationUpdatingSystem::UpdateLocalPositionToParent),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateLocalRotationToParent,
            {
                except<LocalToWorld>(),
                after(&TranslationUpdatingSystem::UpdateLocalScaleToParent),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateParentToTranslation,
            {
                except<LocalToWorld>(),
            });
    }
    void PreUpdateChildAnchor()
    {
        m_anchor_list.clear();
    }
    void UpdateChildAnchor(read<ChildAnchor> anchor, read<Translation> trans)
    {
        m_anchor_list[anchor->anchor_id].m_parent_translation = &trans;
    }

    ///////////////////////////////////////////////////////////////////////////////////

    void UpdateLocalPositionToWorld(const LocalPosition* pos, LocalToWorld* l2w)
    {
        l2w->x = pos->x;
        l2w->y = pos->y;
        l2w->z = pos->z;
    }
    void UpdateLocalScaleToWorld(const LocalScale* scal, LocalToWorld* l2w)
    {
        l2w->scale_x = scal->x;
        l2w->scale_y = scal->y;
        l2w->scale_z = scal->z;
    }
    void UpdateLocalRotationToWorld(const LocalRotation* rot, LocalToWorld* l2w)
    {
        l2w->rx = rot->x;
        l2w->ry = rot->y;
        l2w->rz = rot->z;
        l2w->rw = rot->w;
    }
    void UpdateWorldToTranslation(const LocalToWorld* l2w, Translation* trans)
    {
        // TODO
    }

    ///////////////////////////////////////////////////////////////////////////////////
    void UpdateLocalPositionToParent(const LocalPosition* pos, LocalToParent* l2p)
    {
        l2p->x = pos->x;
        l2p->y = pos->y;
        l2p->z = pos->z;
    }
    void UpdateLocalScaleToParent(const LocalScale* scal, LocalToParent* l2p)
    {
        l2p->scale_x = scal->x;
        l2p->scale_y = scal->y;
        l2p->scale_z = scal->z;
    }
    void UpdateLocalRotationToParent(const LocalRotation* rot, LocalToParent* l2p)
    {
        l2p->rx = rot->x;
        l2p->ry = rot->y;
        l2p->rz = rot->z;
        l2p->rw = rot->w;
    }
    void UpdateParentToTranslation(const LocalToParent* l2p, Translation* trans)
    {
        // TODO
    }

};

#include <iostream>

int main()
{
    using namespace jeecs;
    using namespace std;

    std::unordered_set<typing::uid_t> x;

    jeecs::enrty::module_entry();

    //while (true)
    {
        game_universe universe = game_universe::create_universe();

        game_world world = universe.create_world();
        world.add_system<TranslationUpdatingSystem>();

        auto entity = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalScale,
            Transform::ChildAnchor,
            Transform::Translation>();

        je_clock_sleep_for(0.5);
        game_universe::destroy_universe(universe);
    }
    je_clock_sleep_for(5.);

    jeecs::enrty::module_leave();
}
