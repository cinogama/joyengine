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
    using namespace Transform;
    using namespace UserInterface;

    using namespace slice_requirement;

    struct TranslationUpdatingSystem : public game_system
    {
        TranslationUpdatingSystem(game_world world) : game_system(world)
        {
        }

        void TransfromStageUpdate()
        {
            struct AnchoredTrans
            {
                Anchor* anchor_may_null;
                Translation* trans;
                LocalToParent* l2p;
            };
            std::list<AnchoredTrans> pending_anchor_information;
            std::unordered_map<typing::uuid, Translation*> binded_trans;

            // 对于有L2W的组件，在此优先处理
            for (auto&& [anchor, trans, l2w, position, rotation, scale] : query<
                view typesof(
                    Anchor*,
                    Translation&,
                    LocalToWorld&,
                    LocalPosition*,
                    LocalRotation*,
                    LocalScale*),
                except typesof(LocalToParent)
            >())
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
                    binded_trans.emplace(anchor->uid, &trans);
                }
            }

            // 对于有L2P先进行应用，稍后更新到Translation上
            for (auto&& [anchor, trans, l2p, position, rotation, scale] : query<
                view typesof(
                    Anchor*,
                    Translation&,
                    LocalToParent&,
                    LocalPosition*,
                    LocalRotation*,
                    LocalScale*
                ),
                except typesof(LocalToWorld)
            >())
            {
                l2p.pos = position ? position->pos : math::vec3();
                l2p.rot = rotation ? rotation->rot : math::quat();
                l2p.scale = scale ? scale->scale : math::vec3(1.f, 1.f, 1.f);

                pending_anchor_information.push_back(
                    AnchoredTrans{
                        anchor,
                        &trans,
                        &l2p });
            }

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
                        const Translation* parent_trans = fnd->second;
                        current_idx->trans->world_rotation = parent_trans->world_rotation * current_idx->l2p->rot;
                        current_idx->trans->world_position = parent_trans->world_rotation * current_idx->l2p->pos + parent_trans->world_position;
                        current_idx->trans->local_scale = current_idx->l2p->scale;

                        // 完成应用，将当前变换绑定到binding，然后从pending中删除当前项
                        if (current_idx->anchor_may_null != nullptr)
                            binded_trans.emplace(current_idx->anchor_may_null->uid, current_idx->trans);

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
        void UserInterfaceStageUpdate()
        {
            // UI 布局阶段：每帧把 Element 的输入重建到 WorldLayout，并沿
            // Transform 层级（Anchor + LocalToParent 的 parent_uid）累加祖先偏移。
            // WorldLayout 是唯一的派生状态，Element 等输入组件不会被任何系统回写。
            std::unordered_map<typing::uuid, UserInterface::WorldLayout*> resolved_layouts;

            struct AnchoredLayout
            {
                Anchor* anchor_may_null;
                UserInterface::WorldLayout* layout;
                LocalToParent* l2p;
            };
            std::list<AnchoredLayout> pending_anchor_information;

            for (auto&& [anchor, l2p, elem, layout] : query<
                view typesof(
                    Anchor*,
                    LocalToParent*,
                    Element&,
                    WorldLayout&
                )
            >())
            {
                // 每帧从输入重建（用户可随意修改 Element，无持久派生数据需要维护）。
                layout.offset = elem.offset;
                layout.offset_ratio = elem.offset_ratio;

                if (l2p != nullptr)
                {
                    pending_anchor_information.push_back(
                        AnchoredLayout{
                            anchor,
                            &layout,
                            l2p });
                }
                else if (anchor != nullptr)
                {
                    // 是根UI元素，注册为父级查找目标
                    resolved_layouts.emplace(anchor->uid, &layout);
                }
            }

            size_t count = 0;
            for (;;)
            {
                count = pending_anchor_information.size();
                for (auto idx = pending_anchor_information.begin();
                    idx != pending_anchor_information.end();)
                {
                    auto current_idx = idx++;

                    auto fnd = resolved_layouts.find(current_idx->l2p->parent_uid);
                    if (fnd != resolved_layouts.end())
                    {
                        // 父布局已决，累加之
                        const UserInterface::WorldLayout* parent_layout = fnd->second;

                        current_idx->layout->offset += parent_layout->offset;
                        current_idx->layout->offset_ratio += parent_layout->offset_ratio;

                        // 完成应用，将当前布局绑定到binding，然后从pending中删除当前项
                        if (current_idx->anchor_may_null != nullptr)
                            resolved_layouts.emplace(current_idx->anchor_may_null->uid, current_idx->layout);

                        pending_anchor_information.erase(current_idx);
                    }
                }
                if (pending_anchor_information.size() == count)
                {
                    // 剩余布局缺失父布局或祖布局，不做处理以确保问题立即被发现；
                    break;
                }
            }
        }

        void TransformUpdate()
        {
            TransfromStageUpdate();
            UserInterfaceStageUpdate();
        }
        void CommitUpdate()
        {
            // 到此为止，所有的变换均已应用到 Translation 上，现在更新变换矩阵

            for (auto&& [trans] :
                query typesof(
                    view typesof(Translation&)
                )())
            {
                math::transform(
                    trans.object2world,
                    trans.world_position,
                    trans.world_rotation,
                    trans.local_scale);
            }
        }
    };
}
