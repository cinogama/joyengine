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
            // 锚定基准点 + 父链偏移 + 单位一，均以单位一空间 ratio_space 分桶
            // 表达、相对显示区中心）。根元素的 anchor 与相对通道
            //（offset_ratio/size_ratio）以显示区为参考；子元素的以父元素
            // 矩形为参考——anchor 锚定父矩形方位（纯几何量，写入 base 通道），
            // 自身相对通道按自身 ratio_unit 以父矩形的高/宽为单位一
            //（height_unit/width_unit 两轴同用高/宽标量，per_axis 则 x 轴用宽、
            // y 轴用高）（父矩形有效尺寸 = 父输入尺寸 + 父 size_ratio ⊙ 父
            // 单位一，递归）。父链偏移（offset 通道）把每个祖先自身的
            // offset/offset_ratio 按**祖先自己的单位一**折算进对应分桶后逐级
            // 累加，绘制端按各桶参照像素化——因此父 offset_ratio 位移被子
            // 元素按原像素量继承，不受子元素 ratio_unit 影响。
            // 因此先解父后解子；WorldLayout 是唯一的派生状态，输入组件不会被任何系统回写。
            //
            // 有效旋转角（rotation）的缓存与 TransfromStageUpdate 递推 world_rotation
            // 同理：根元素 = 自身 Rotation::angle，子元素 = 父有效角 + 自身角，
            // 沿 Anchor+LocalToParent 父链逐级累加（未挂 Rotation 组件按 0 计）。
            // 旋转是像素空间量，无法在单位一空间中传播，故只缓存角度本身；
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

            // 显示区作为根元素的参照矩形与单位一来源
            //（单位一空间：绝对0 + 逐轴相对1，其余分桶为 0）。
            const WorldLayout::ratio_space display_rect{
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
                    layout.base = WorldLayout::ratio_space{};
                    layout.offset = WorldLayout::ratio_space{};
                    layout.unit = Element::fold_unit(display_rect, elem.unit_kind);
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
                    // 根元素：anchor 相对显示区解析进 base 通道（纯几何量），
                    // 无父链偏移；单位一 = 显示区按自身 ratio_unit 折算。
                    layout.base = Element::anchor_shift(elem.anchor, display_rect);
                    layout.offset = WorldLayout::ratio_space{};
                    layout.unit = Element::fold_unit(display_rect, elem.unit_kind);
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

                        // 父矩形的有效尺寸（递归，单位一空间），用作：
                        // 1) 父枢轴修正与子 anchor 锚定的参照（几何量，按各自轴取半）；
                        // 2) 子元素相对量的单位一来源——按子元素的 ratio_unit
                        //    折算（height_unit 取父高、width_unit 取父宽标量
                        //    广播到两轴，per_axis 逐轴取父宽/父高）。
                        const auto parent_rect =
                            WorldLayout::ratio_space{ parent_elem->size, {}, {}, {} }
                            + parent_layout->unit.scaled(parent_elem->size_ratio);

                        // 子单位一 = 父矩形按子元素 ratio_unit 折算后的通道。
                        current_idx->layout->unit = Element::fold_unit(
                            parent_rect, current_idx->elem->unit_kind);

                        // 子有效旋转角 = 父有效角 + 自身角（度）。
                        // 父自身的角已含于父的 WorldLayout::rotation，与
                        // TransfromStageUpdate 中“父 world_rotation 已含父局部旋转”
                        // 的递推结构一致，因此这里无需再读父的 Rotation 组件。
                        current_idx->layout->rotation = parent_layout->rotation
                            + (current_idx->rotation_may_null != nullptr
                                ? current_idx->rotation_may_null->angle
                                : 0.f);

                        // 锚定基准点（base 通道，纯几何量）：父链基准点 +
                        // 父枢轴修正 + 自身 anchor 相对父矩形的锚定。
                        // 子元素永远以父矩形的中心/边角为基准点——即便父矩形
                        // 由 size_ratio 撑起、且子元素 ratio_unit 不是 per_axis，
                        // 锚定位置也不受子元素 ratio_unit 影响。
                        const auto parent_pivot =
                            Element::pivot_shift(parent_elem->pivot, parent_rect);
                        const auto anchored =
                            Element::anchor_shift(current_idx->elem->anchor, parent_rect);

                        current_idx->layout->base =
                            parent_layout->base + parent_pivot + anchored;

                        // 父链累计偏移（offset 通道）= 父累计 + 父自身偏移
                        //（绝对 + offset_ratio 按**父单位一**折算进对应分桶）。
                        // 分桶传播保证父 offset_ratio 位移被子元素按原像素量
                        // 继承，不再被子元素的 ratio_unit 重新解释——修复
                        // height_unit/width_unit 子元素跟随父 offset_ratio
                        // 位移时单位不一致的问题。本元素自身偏移不在此累加，
                        // 由 Element::resolve_layout 按本元素单位一现算。
                        current_idx->layout->offset =
                            parent_layout->offset
                            + WorldLayout::ratio_space{ parent_elem->offset, {}, {}, {} }
                            + parent_layout->unit.scaled(parent_elem->offset_ratio);

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
