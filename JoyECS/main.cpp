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
            math::vec3 pos;
        };
        struct LocalRotation
        {
            math::quat rot;
        };
        struct LocalScale
        {
            math::vec3 scale;
        };

        struct ChildAnchor
        {
            typing::uid_t anchor_id = je_uid_generate();
        };

        struct LocalToParent
        {
            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;

            typing::uid_t parent_uid;
        };

        struct LocalToWorld
        {
            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;
        };
        static_assert(offsetof(LocalToParent, pos) == offsetof(LocalToWorld, pos));
        static_assert(offsetof(LocalToParent, scale) == offsetof(LocalToWorld, scale));
        static_assert(offsetof(LocalToParent, rot) == offsetof(LocalToWorld, rot));

        struct Translation
        {
            float object2world[16] =
            {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1, };
            float objectrotation[16] = {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1, };
            math::quat rotation;

            inline void set_position(const math::vec3& _v3) noexcept
            {
                object2world[0 + 3 * 4] = _v3.x;
                object2world[1 + 3 * 4] = _v3.y;
                object2world[2 + 3 * 4] = _v3.z;
            }

            inline void set_scale(const math::vec3& _v3) noexcept
            {
                object2world[0 + 0 * 4] = _v3.x;
                object2world[1 + 1 * 4] = _v3.y;
                object2world[2 + 2 * 4] = _v3.z;
                object2world[3 + 3 * 4] = 1.f;
            }

            inline constexpr math::vec3 get_position() const noexcept
            {
                return math::vec3(
                    object2world[0 + 3 * 4],
                    object2world[1 + 3 * 4],
                    object2world[2 + 3 * 4]);
            }
            inline constexpr math::vec3 get_scale() const noexcept
            {
                return math::vec3(
                    object2world[0 + 0 * 4],
                    object2world[1 + 1 * 4],
                    object2world[2 + 2 * 4]);
            }
        };
    }
    namespace Physics
    {
        // Entity with physics effect will 
    }
}

using namespace jeecs;
using namespace Transform;

struct TranslationUpdatingSystem :public game_system
{
    struct anchor
    {
        const Translation* m_translation;
    };

    std::unordered_map<typing::uid_t, anchor> m_anchor_list;

    TranslationUpdatingSystem(game_world world) :game_system(world)
    {
        register_system_func(&TranslationUpdatingSystem::PreUpdateChildAnchor,
            {
                system_write(&m_anchor_list),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateChildAnchorTranslation,
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
            });
        register_system_func(&TranslationUpdatingSystem::UpdateLocalRotationToWorld,
            {
                except<LocalToParent>(),
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
            });
        register_system_func(&TranslationUpdatingSystem::UpdateLocalRotationToParent,
            {
                except<LocalToWorld>(),
            });
        register_system_func(&TranslationUpdatingSystem::UpdateParentToTranslation,
            {
                except<LocalToWorld>(),
                system_read_updated(&m_anchor_list)
            });
    }
    void PreUpdateChildAnchor()
    {
        m_anchor_list.clear();
    }
    void UpdateChildAnchorTranslation(read<ChildAnchor> anchor, read<Translation> trans)
    {
        m_anchor_list[anchor->anchor_id].m_translation = trans;
    }

    ///////////////////////////////////////////////////////////////////////////////////

    void UpdateLocalPositionToWorld(const LocalPosition* pos, LocalToWorld* l2w)
    {
        l2w->pos = pos->pos;
    }
    void UpdateLocalScaleToWorld(const LocalScale* scal, LocalToWorld* l2w)
    {
        l2w->scale = scal->scale;
    }
    void UpdateLocalRotationToWorld(const LocalRotation* rot, LocalToWorld* l2w)
    {
        l2w->rot = rot->rot;
    }
    void UpdateWorldToTranslation(const LocalToWorld* l2w, Translation* trans)
    {
        trans->rotation = l2w->rot;
        trans->rotation.create_matrix(trans->objectrotation);
        trans->set_position(l2w->pos);
        trans->set_scale(l2w->scale);

    }

    ///////////////////////////////////////////////////////////////////////////////////
    void UpdateLocalPositionToParent(const LocalPosition* pos, LocalToParent* l2p)
    {
        l2p->pos = pos->pos;
    }
    void UpdateLocalScaleToParent(const LocalScale* scal, LocalToParent* l2p)
    {
        l2p->scale = scal->scale;
    }
    void UpdateLocalRotationToParent(const LocalRotation* rot, LocalToParent* l2p)
    {
        l2p->rot = rot->rot;
    }
    void UpdateParentToTranslation(const LocalToParent* l2p, Translation* trans)
    {
        // Get parent's translation, then apply them.
        auto fnd = m_anchor_list.find(l2p->parent_uid);
        if (fnd != m_anchor_list.end())
        {
            const Translation* parent_trans = fnd->second.m_translation;
            trans->rotation = parent_trans->rotation * l2p->rot;
            trans->rotation.create_matrix(trans->objectrotation);
            trans->set_position(parent_trans->rotation * l2p->pos + parent_trans->get_position());
            trans->set_scale(l2p->scale); // TODO: need apply scale?   
        }
        else
        {
            // Parent is not exist
        }
    }

};

#include <iostream>

int main(int argc, char** argv)
{
    rs_init(argc, argv);

    rs_vm m = rs_create_vm();
    if (rs_load_source(m, "_example.rsn", R"(
import rscene.std;

extern("rslib_std_print") func print<T>(var x:T):T;

extern func main()
{
    var x = func(){};
    var y = func(){};
    var xt = 0;
    xt = print:<typeof(x)>(y);

    if((x && y) || (x && y))
        std::panic("That should not happend..");

    var a=["Hello", "world"];
    a->add(666:string);

    std::println(a);
}
)"))
    {
        rs_invoke_rsfunc(m, rs_extern_symb(m, "::main"), 0);
    }
    else
    {
        jeecs::debug::log_error(rs_get_compile_error(m, RS_NEED_COLOR));
    }

    rs_close_vm(m);

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
