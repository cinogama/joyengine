#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif


#if 0

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
            register_system_func(&TranslationUpdatingSystem::UpdateLocalToWorld,
                {
                    except<LocalToParent>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateWorldToTranslation,
                {
                    except<LocalToParent>(),
                });
            /////////////////////////////////////////////////////////////////////////////////
            register_system_func(&TranslationUpdatingSystem::UpdateLocalToParent,
                {
                    except<LocalToWorld>(),
                });
            register_system_func(&TranslationUpdatingSystem::UpdateParentToTranslation,
                {
                    after(&TranslationUpdatingSystem::UpdateWorldToTranslation),
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
            m_anchor_list[anchor->anchor_uid].m_translation = trans;
        }

        ///////////////////////////////////////////////////////////////////////////////////
        template<typename T>
        void _generate_mat_from_local(float(*out_mat)[4], const T* local)
        {
            float temp_mat_trans[4][4] = {};
            temp_mat_trans[0][0]
                = temp_mat_trans[1][1]
                = temp_mat_trans[2][2]
                = temp_mat_trans[3][3] = 1.0f;
            temp_mat_trans[3][0] = local->pos.x;
            temp_mat_trans[3][1] = local->pos.y;
            temp_mat_trans[3][2] = local->pos.z;

            float temp_mat_rotation[4][4];
            local->rot.create_matrix(temp_mat_rotation);

            float temp_mat_trans_rot[4][4];
            math::mat4xmat4(temp_mat_trans_rot, temp_mat_trans, temp_mat_rotation);

            float temp_mat_scale[4][4] = {};
            temp_mat_scale[0][0] = local->scale.x;
            temp_mat_scale[1][1] = local->scale.y;
            temp_mat_scale[2][2] = local->scale.z;
            temp_mat_scale[3][3] = 1.0f;

            math::mat4xmat4(out_mat, temp_mat_trans_rot, temp_mat_scale);
        }

        void UpdateLocalToWorld(
            maynot<const LocalPosition*> position,
            maynot<const LocalRotation*> rotation,
            maynot<const LocalScale*>    scale,
            LocalToWorld* l2w)
        {
            l2w->pos = position ? position->pos : math::vec3();
            l2w->rot = rotation ? rotation->rot : math::quat();
            l2w->scale = scale ? scale->scale : math::vec3(1, 1, 1);
        }
        void UpdateWorldToTranslation(const LocalToWorld* l2w, Translation* trans)
        {
            trans->set_rotation(l2w->rot);
            trans->set_position(l2w->pos);
            trans->set_scale(l2w->scale);

            _generate_mat_from_local(trans->object2world, l2w);
        }
        ///////////////////////////////////////////////////////////////////////////////////
        void UpdateLocalToParent(
            maynot<const LocalPosition*> position,
            maynot<const LocalRotation*> rotation,
            maynot<const LocalScale*>    scale,
            LocalToParent* l2P)
        {
            l2P->pos = position ? position->pos : math::vec3();
            l2P->rot = rotation ? rotation->rot : math::quat();
            l2P->scale = scale ? scale->scale : math::vec3(1, 1, 1);
        }
        void UpdateParentToTranslation(const LocalToParent* l2p, Translation* trans)
        {
            // Get parent's translation, then apply them.
            auto fnd = m_anchor_list.find(l2p->parent_uid);
            if (fnd != m_anchor_list.end())
            {
                const Translation* parent_trans = fnd->second.m_translation;
                trans->set_rotation(parent_trans->world_rotation * l2p->rot);
                trans->set_position(parent_trans->world_rotation * l2p->pos + parent_trans->world_position);
                trans->set_scale(l2p->scale); // TODO: need apply scale? 

                float local_trans[4][4];
                _generate_mat_from_local(local_trans, l2p);
                math::mat4xmat4(trans->object2world, parent_trans->object2world, local_trans);
            }
            else
            {
                // Parent is not exist, treate it as l2w
                UpdateWorldToTranslation(reinterpret_cast<const LocalToWorld*>(l2p), trans);
            }
        }
    };
}

#endif