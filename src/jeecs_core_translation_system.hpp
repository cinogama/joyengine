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
            // UI 布局阶段：每帧把 Element 输入解析为 WorldLayout（参考系 =
            // 锚定基准点 + 自身偏移 + 单位一，双通道，相对显示区中心表达）。
            // 根元素的 anchor 与相对通道（offset_ratio/size_ratio）以显示区为参考；
            // 子元素的以父元素矩形为参考——anchor 锚定父矩形方位（纯几何量，
            // 写入 base 通道，逐轴解析、不受 ratio_unit 影响），自身相对通道
            // 按自身 ratio_unit 以父矩形的高/宽为单位一（height_unit/width_unit
            // 两轴同用高/宽标量，per_axis 则 x 轴用宽、y 轴用高）
            //（父矩形有效尺寸 = 父输入尺寸 + 父 size_ratio ⊙ 祖传单位一，递归）。
            // 因此先解父后解子；WorldLayout 是唯一的派生状态，输入组件不会被任何系统回写。
            //
            // 有效旋转角（rotation）的缓存与 TransfromStageUpdate 递推 world_rotation
            // 同理：根元素 = 自身 Rotation::angle，子元素 = 父有效角 + 自身角，
            // 沿 Anchor+LocalToParent 父链逐级累加（未挂 Rotation 组件按 0 计）。
            // 旋转是像素空间量，无法在双通道中传播，故只缓存角度本身；
            // 旋转后的位置仍由绘制阶段按相机目标尺寸在像素空间复合。

            struct ResolvedParent
            {
                UserInterface::Element* elem;
                UserInterface::WorldLayout* layout;
            };
            std::unordered_map<typing::uuid, ResolvedParent> resolved_parents;

            struct AnchoredLayout
            {
                Anchor* anchor_may_null;
                Element* elem;
                WorldLayout* layout;
                LocalToParent* l2p;
                Rotation* rotation_may_null;
            };
            std::list<AnchoredLayout> pending_anchor_information;

            // 显示区作为根元素的参照矩形与单位一（双通道：绝对0 + 相对1）。
            const UserInterface::layout_value display_rect{
                math::vec2(0.f, 0.f), math::vec2(1.f, 1.f) };

            for (auto&& [anchor, l2p, elem, layout, rotation] : query<
                view typesof(
                    Anchor*,
                    LocalToParent*,
                    Element&,
                    WorldLayout&,
                    Rotation*
                )
            >())
            {
                if (l2p != nullptr)
                {
                    // 子元素：先按根语义暂存（父元素解析后覆盖为完整参考系；
                    // 若父链缺失则保留此退化结果，问题可立即被发现）。
                    layout.offset = elem.offset;
                    layout.offset_ratio = elem.offset_ratio;
                    layout.base_offset = math::vec2(0.f, 0.f);
                    layout.base_offset_ratio = math::vec2(0.f, 0.f);
                    layout.unit = display_rect.absolute;
                    layout.unit_ratio = display_rect.relative;
                    layout.rotation = rotation ? rotation->angle : 0.f;

                    pending_anchor_information.push_back(
                        AnchoredLayout{
                            anchor,
                            &elem,
                            &layout,
                            l2p,
                            rotation });
                }
                else
                {
                    // 根元素：anchor 相对显示区解析；锚定基准点走 base 通道
                    //（逐轴，不受 ratio_unit 影响），offset 通道只留自身偏移。
                    const auto anchored = anchor_shift(elem.anchor, display_rect);

                    layout.offset = elem.offset;
                    layout.offset_ratio = elem.offset_ratio;
                    layout.base_offset = anchored.absolute;
                    layout.base_offset_ratio = anchored.relative;
                    layout.unit = display_rect.absolute;
                    layout.unit_ratio = display_rect.relative;
                    layout.rotation = rotation ? rotation->angle : 0.f;

                    if (anchor != nullptr)
                    {
                        resolved_parents.emplace(
                            anchor->uid, ResolvedParent{ &elem, &layout });
                    }
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

                    auto fnd = resolved_parents.find(current_idx->l2p->parent_uid);
                    if (fnd != resolved_parents.end())
                    {
                        const auto& [parent_elem, parent_layout] = fnd->second;

                        // 父矩形的有效尺寸通道（递归），用作：
                        // 1) 父枢轴修正与子 anchor 锚定的参照尺寸（几何量，按各自轴取半）；
                        // 2) 子元素相对通道的单位一——按子元素的 ratio_unit 折算：
                        //    height_unit 取父高、width_unit 取父宽作为标量广播到两轴
                        //    （与根元素“以显示区高/宽为单位”的语义一致），
                        //    per_axis 则 x 轴取父宽、y 轴取父高。
                        const UserInterface::layout_value parent_unit{
                            parent_layout->unit, parent_layout->unit_ratio };
                        const auto parent_scaled_ratio =
                            scale_channels(parent_elem->size_ratio, parent_unit);
                        const UserInterface::layout_value parent_rect{
                            parent_elem->size + parent_scaled_ratio.absolute,
                            parent_scaled_ratio.relative };

                        math::vec2 unit_absolute = {};
                        math::vec2 unit_relative = {};
                        switch (current_idx->elem->ratio_unit)
                        {
                        case ratio_unit::height_unit:
                            // 子单位一 = 父高标量，广播到两轴。
                            unit_absolute = math::vec2(
                                parent_rect.absolute.y, parent_rect.absolute.y);
                            unit_relative = math::vec2(
                                parent_rect.relative.y, parent_rect.relative.y);
                            break;
                        case ratio_unit::width_unit:
                            // 子单位一 = 父宽标量，广播到两轴。
                            unit_absolute = math::vec2(
                                parent_rect.absolute.x, parent_rect.absolute.x);
                            unit_relative = math::vec2(
                                parent_rect.relative.x, parent_rect.relative.x);
                            break;
                        case ratio_unit::per_axis:
                        default:
                            // 子单位一 = 父宽（x 轴）与父高（y 轴）分轴取值。
                            unit_absolute = parent_rect.absolute;
                            unit_relative = parent_rect.relative;
                            break;
                        }

                        current_idx->layout->unit = unit_absolute;
                        current_idx->layout->unit_ratio = unit_relative;

                        // 子有效旋转角 = 父有效角 + 自身角（度）。
                        // 父自身的角已含于父的 WorldLayout::rotation，与
                        // TransfromStageUpdate 中“父 world_rotation 已含父局部旋转”
                        // 的递推结构一致，因此这里无需再读父的 Rotation 组件。
                        current_idx->layout->rotation = parent_layout->rotation
                            + (current_idx->rotation_may_null != nullptr
                                ? current_idx->rotation_may_null->angle
                                : 0.f);

                        // 锚定基准点（base 通道，逐轴，不受 ratio_unit 影响）：
                        // 父链基准点 + 父枢轴修正 + 自身 anchor 相对父矩形的锚定。
                        // 子元素永远以父矩形的中心/边角为基准点——即便父矩形
                        // 由 size_ratio 撑起、且子元素 ratio_unit 不是 per_axis，
                        // 锚定位置也不产生额外偏移。
                        const auto parent_pivot = pivot_shift(parent_elem->pivot, parent_rect);
                        const auto anchored = anchor_shift(current_idx->elem->anchor, parent_rect);

                        current_idx->layout->base_offset =
                            parent_layout->base_offset + parent_layout->offset
                            + parent_pivot.absolute
                            + anchored.absolute;
                        current_idx->layout->base_offset_ratio =
                            parent_layout->base_offset_ratio + parent_pivot.relative
                            + anchored.relative;

                        // 自身偏移通道（按自身 ratio_unit 折算）：
                        // 绝对部分在布局期按单位一折算；相对部分保持自身
                        // offset_ratio（绘制端按 ratio_unit 折算），并继承
                        // 父链自身的 offset_ratio。
                        const auto own_offset = scale_channels(
                            current_idx->elem->offset_ratio,
                            UserInterface::layout_value{
                                current_idx->layout->unit,
                                current_idx->layout->unit_ratio });

                        current_idx->layout->offset =
                            current_idx->elem->offset + own_offset.absolute;
                        current_idx->layout->offset_ratio =
                            own_offset.relative + parent_layout->offset_ratio;

                        // 完成应用，将当前布局绑定到binding，然后从pending中删除当前项
                        if (current_idx->anchor_may_null != nullptr)
                            resolved_parents.emplace(
                                current_idx->anchor_may_null->uid,
                                ResolvedParent{
                                    current_idx->elem,
                                    current_idx->layout });

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
