#pragma once

#define JE_IMPL
#include "jeecs.hpp"

namespace jeecs
{
    struct TranslationUpdatingSystem :public game_system
    {
        using ChildAnchor = Transform::ChildAnchor;
        using LocalPosition = Transform::LocalPosition;
        using LocalRotation = Transform::LocalRotation;
        using LocalScale = Transform::LocalScale;

        using LocalToParent = Transform::LocalToParent;
        using LocalToWorld = Transform::LocalToWorld;
        using Translation = Transform::Translation;
        using InverseTranslation = Transform::InverseTranslation;

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
            register_system_func(&TranslationUpdatingSystem::PreUpdateLocalToWorld);
            register_system_func(&TranslationUpdatingSystem::PreUpdateLocalToParent);
            /////////////////////////////////////////////////////////////////////////////////
            register_system_func(&TranslationUpdatingSystem::UpdateLocalPositionToWorld,
                {
                    after(&TranslationUpdatingSystem::PreUpdateLocalToWorld),
                    except<LocalToParent>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateLocalScaleToWorld,
                {
                    after(&TranslationUpdatingSystem::PreUpdateLocalToWorld),
                    except<LocalToParent>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateLocalRotationToWorld,
                {
                    after(&TranslationUpdatingSystem::PreUpdateLocalToWorld),
                    except<LocalToParent>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateWorldToTranslation,
                {
                    except<InverseTranslation>(),
                    except<LocalToParent>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateWorldToTranslationInverse,
                {
                    contain<InverseTranslation>(),
                    except<LocalToParent>(),
                });
            /////////////////////////////////////////////////////////////////////////////////
            register_system_func(&TranslationUpdatingSystem::UpdateLocalPositionToParent,
                {
                    after(&TranslationUpdatingSystem::PreUpdateLocalToParent),
                    except<LocalToWorld>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateLocalScaleToParent,
                {
                    after(&TranslationUpdatingSystem::PreUpdateLocalToParent),
                    except<LocalToWorld>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateLocalRotationToParent,
                {
                    after(&TranslationUpdatingSystem::PreUpdateLocalToParent),
                    except<LocalToWorld>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateParentToTranslation,
                {
                    except<InverseTranslation>(),
                    except<LocalToWorld>(),
                    system_read_updated(&m_anchor_list)
                });
            register_system_func(&TranslationUpdatingSystem::UpdateParentToTranslationInverse,
                {
                    contain<InverseTranslation>(),
                    except<LocalToWorld>(),
                    system_read_updated(&m_anchor_list)
                });
        }
        void PreUpdateChildAnchor()
        {
            m_anchor_list.clear();
        }
        void PreUpdateLocalToWorld(LocalToWorld* l2w)
        {
            l2w->pos = math::vec3();
            l2w->rot = math::quat();
            l2w->scale = math::vec3(1.0f, 1.0f, 1.0f);
        }
        void PreUpdateLocalToParent(LocalToParent* l2p)
        {
            l2p->pos = math::vec3();
            l2p->rot = math::quat();
            l2p->scale = math::vec3(1.0f, 1.0f, 1.0f);
        }
        void UpdateChildAnchorTranslation(read<ChildAnchor> anchor, read<Translation> trans)
        {
            m_anchor_list[anchor->anchor_uid].m_translation = trans;
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
            trans->set_rotation(l2w->rot);
            trans->set_position(l2w->pos);
            trans->set_scale(l2w->scale);
        }
        void UpdateWorldToTranslationInverse(const LocalToWorld* l2w, Translation* trans)
        {
            trans->set_inverse_rotation(l2w->rot);
            trans->set_inverse_position(l2w->pos);
            trans->set_scale(l2w->scale); // no need for inverse scale..
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
                trans->set_rotation(parent_trans->rotation * l2p->rot);
                trans->set_position(parent_trans->rotation * l2p->pos + parent_trans->get_position());
                trans->set_scale(l2p->scale); // TODO: need apply scale?   
            }
            else
            {
                // Parent is not exist, treate it as l2w
                UpdateWorldToTranslation(reinterpret_cast<const LocalToWorld*>(l2p), trans);
            }
        }
        void UpdateParentToTranslationInverse(const LocalToParent* l2p, Translation* trans)
        {
            // Get parent's translation, then apply them.
            auto fnd = m_anchor_list.find(l2p->parent_uid);
            if (fnd != m_anchor_list.end())
            {
                const Translation* parent_trans = fnd->second.m_translation;
                trans->set_inverse_rotation(parent_trans->get_rotation() * l2p->rot);
                trans->set_inverse_position(parent_trans->get_rotation() * l2p->pos + parent_trans->get_position());
                trans->set_scale(l2p->scale); // TODO: need apply scale?   
            }
            else
            {
                // Parent is not exist, treate it as l2w
                UpdateWorldToTranslationInverse(reinterpret_cast<const LocalToWorld*>(l2p), trans);
            }
        }
    };
}