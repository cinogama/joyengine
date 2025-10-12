#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <list>

namespace jeecs
{
    struct TranslationUpdatingSystem : public game_system
    {
        using Anchor = Transform::Anchor;
        using LocalPosition = Transform::LocalPosition;
        using LocalRotation = Transform::LocalRotation;
        using LocalScale = Transform::LocalScale;

        using LocalToParent = Transform::LocalToParent;
        using LocalToWorld = Transform::LocalToWorld;
        using Translation = Transform::Translation;

        TranslationUpdatingSystem(game_world world) : game_system(world)
        {
        }

        void TransfromStageUpdate(jeecs::selector &selector)
        {
            std::unordered_map<typing::uuid, Translation *> binded_trans;

            // 对于有L2W的组件，在此优先处理
            selector.except<LocalToParent>();
            selector.exec(
                [&binded_trans](
                    Anchor *anchor,
                    Translation &trans,
                    LocalToWorld &l2w,
                    LocalPosition *position,
                    LocalRotation *rotation,
                    LocalScale *scale)
                {
                    l2w.pos = position ? position->pos : math::vec3();
                    l2w.rot = rotation ? rotation->rot : math::quat();
                    l2w.scale = scale ? scale->scale : math::vec3(1.f, 1.f, 1.f);

                    trans.world_rotation = l2w.rot;
                    trans.world_position = l2w.pos;
                    trans.local_scale = l2w.scale;

                    if (anchor != nullptr)
                    {
                        // 对于L2W的变换，其直接作为变换起点集合
                        binded_trans.insert(std::make_pair(anchor->uid, &trans));
                    }
                });

            struct AnchoredTrans
            {
                Anchor *anchor_may_null;
                Translation *trans;
                LocalToParent *l2p;
            };
            std::list<AnchoredTrans> pending_anchor_information;

            // 对于有L2P先进行应用，稍后更新到Translation上
            selector.except<LocalToWorld>();
            selector.exec(
                [&](Anchor *anchor,
                    Translation &trans,
                    LocalToParent &l2p,
                    LocalPosition *position,
                    LocalRotation *rotation,
                    LocalScale *scale)
                {
                    l2p.pos = position ? position->pos : math::vec3();
                    l2p.rot = rotation ? rotation->rot : math::quat();
                    l2p.scale = scale ? scale->scale : math::vec3(1.f, 1.f, 1.f);

                    pending_anchor_information.push_back(
                        AnchoredTrans{
                            anchor,
                            &trans,
                            &l2p});
                });

            size_t count = 0;
            for (;;)
            {
                count = pending_anchor_information.size();
                for (auto idx = pending_anchor_information.begin();
                     idx != pending_anchor_information.end();)
                {
                    auto current_idx = idx++;

                    auto fnd = binded_trans.find(current_idx->l2p->parent_uid);
                    if (fnd != binded_trans.end())
                    {
                        // 父变换已决，应用之
                        const Translation *parent_trans = fnd->second;
                        current_idx->trans->world_rotation = parent_trans->world_rotation * current_idx->l2p->rot;
                        current_idx->trans->world_position = parent_trans->world_rotation * current_idx->l2p->pos + parent_trans->world_position;
                        current_idx->trans->local_scale = current_idx->l2p->scale;

                        // 完成应用，将当前变换绑定到binding，然后从pending中删除当前项
                        if (current_idx->anchor_may_null != nullptr)
                            binded_trans.insert(std::make_pair(current_idx->anchor_may_null->uid, current_idx->trans));

                        pending_anchor_information.erase(current_idx);
                    }
                }
                if (pending_anchor_information.size() == count)
                {
                    // 剩余变换缺失父变换或祖变换，不做处理以确保问题立即被发现；
                    break;
                }
            }
        }

        void UserInterfaceStageUpdate(jeecs::selector &selector)
        {
            std::unordered_map<typing::uuid, UserInterface::Origin *> binded_origins;

            struct AnchoredOrigin
            {
                Anchor *anchor_may_null;
                UserInterface::Origin *origin;
                LocalToParent *l2p;
            };
            std::list<AnchoredOrigin> pending_anchor_information;

            selector.exec(
                [&binded_origins, &pending_anchor_information](
                    Anchor *anchor,
                    Transform::LocalToParent *l2p,
                    UserInterface::Origin &origin,
                    UserInterface::Absolute *absolute,
                    UserInterface::Relatively *relatively)
                {
                    if (absolute != nullptr)
                    {
                        origin.global_offset = absolute->offset;
                        origin.size = absolute->size;
                    }
                    else
                        origin.size = {};

                    if (relatively != nullptr)
                    {
                        origin.global_location = relatively->location;
                        origin.scale = relatively->scale;

                        origin.keep_vertical_ratio = relatively->use_vertical_ratio;
                    }
                    else
                    {
                        origin.scale = {};
                        // NOTE: We dont care `keep_vertical_ratio` if rel is not exist.
                    }

                    if (l2p != nullptr)
                    {
                        pending_anchor_information.push_back(
                            AnchoredOrigin{
                                anchor,
                                &origin,
                                l2p});
                    }
                    else
                    {
                        // 是根UI元素
                        origin.root_center = origin.elem_center;
                        if (anchor != nullptr)
                        {
                            binded_origins.insert(std::make_pair(anchor->uid, &origin));
                        }
                    }
                });

            size_t count = 0;
            for (;;)
            {
                count = pending_anchor_information.size();
                for (auto idx = pending_anchor_information.begin();
                     idx != pending_anchor_information.end();)
                {
                    auto current_idx = idx++;

                    auto fnd = binded_origins.find(current_idx->l2p->parent_uid);
                    if (fnd != binded_origins.end())
                    {
                        // 父变换已决，应用之
                        const UserInterface::Origin *parent_origin = fnd->second;

                        current_idx->origin->root_center = parent_origin->root_center;
                        current_idx->origin->global_location += parent_origin->global_location;
                        current_idx->origin->global_offset += parent_origin->global_offset;

                        // 完成应用，将当前变换绑定到binding，然后从pending中删除当前项
                        if (current_idx->anchor_may_null != nullptr)
                            binded_origins.insert(std::make_pair(current_idx->anchor_may_null->uid, current_idx->origin));

                        pending_anchor_information.erase(current_idx);
                    }
                }
                if (pending_anchor_information.size() == count)
                {
                    // 剩余变换缺失父变换或祖变换，不做处理以确保问题立即被发现；
                    break;
                }
            }
        }

        void TransformUpdate(jeecs::selector &selector)
        {
            TransfromStageUpdate(selector);
            UserInterfaceStageUpdate(selector);
        }
        void CommitUpdate(jeecs::selector &selector)
        {
            // 到此为止，所有的变换均已应用到 Translation 上，现在更新变换矩阵

            jeecs::slice<jeecs::slice_requirement::View<Translation&>> a;

            a.fetch(get_world()).foreach_parallel(
                [](auto& slice) 
                {
                    auto& [trans] = slice;

                    math::transform(
                        trans.object2world,
                        trans.world_position,
                        trans.world_rotation,
                        trans.local_scale);
                });

            /*for (auto& [trans] : a.fetch(get_world()))
            {
                math::transform(
                    trans.object2world,
                    trans.world_position,
                    trans.world_rotation,
                    trans.local_scale);
            }*/

            /*selector.exec(
                [](Translation &trans)
                {
                    math::transform(
                        trans.object2world,
                        trans.world_position,
                        trans.world_rotation,
                        trans.local_scale);
                });*/
        }
    };
}
