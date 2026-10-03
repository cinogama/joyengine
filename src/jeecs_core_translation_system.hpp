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
            // UI 布局阶段：每帧把 Element 输入解析为 WorldLayout 的两层派生状态：
            //  1) 参考系（base/offset/unit 以单位一空间 ratio_space 分桶表达、
            //     相对显示区中心，各通道语义见 WorldLayout 注释）。根元素的
            //     anchor 与相对通道（offset_ratio/size_ratio）以显示区为参考；
            //     子元素的以父元素矩形为参考——anchor 锚定父矩形方位（纯几何
            //     量，写入 base 通道），自身相对通道按自身 ratio_unit 以父矩形
            //     的高/宽为单位一（height_unit/width_unit 两轴同用高/宽标量，
            //     per_axis 则 x 轴用宽、y 轴用高）（父矩形有效尺寸 = 父输入
            //     尺寸 + 父 size_ratio ⊙ 父单位一，递归）。父链偏移（offset
            //     通道）把每个祖先自身的 offset/offset_ratio 按**祖先自己的
            //     单位一**折算进对应分桶后逐级累加——因此父 offset_ratio 位移
            //     被子元素按原像素量继承，不受子元素 ratio_unit 影响；
            //  2) 已复合显示矩形（center/size/pivot_offset/anchor，affine2
            //     系数）——父子关系在此完全展开：每个通道只是目标缓冲区宽高
            //     (w,h) 的线性函数，祖先旋转链也在系数空间复合完成（定点旋转
            //     是线性映射，对 k0/kw/kh 各系数向量旋转即可；带语义的分桶
            //     空间对旋转不封闭，from_ratio 的并桶恰好完成转换）。绘制/
            //     命中测试端只需按相机目标尺寸像素化
            //    （Element::resolve_display_rect），不再沿父链遍历。
            // 有效旋转角（rotation）的缓存与 TransfromStageUpdate 递推
            // world_rotation 同理：根元素 = 自身 Rotation::angle，子元素 =
            // 父有效角 + 自身角，沿 Anchor+LocalToParent 父链逐级累加（未挂
            // Rotation 组件按 0 计）。
            // 因此先解父后解子；WorldLayout 是唯一的派生状态，输入组件不会被任何系统回写。

            struct ResolvedParent
            {
                UserInterface::Element* elem;
                UserInterface::WorldLayout* layout;

                // 父链传播量（系数空间）：local_center 为祖先未旋转帧中的
                // 中心，display_center 为含祖先旋转链的中心，pivot_offset 为
                // 枢轴修正，own_angle 为自身角（绕自身枢轴，未计入 display）。
                WorldLayout::affine2 local_center;
                WorldLayout::affine2 display_center;
                WorldLayout::affine2 pivot_offset;
                float own_angle;
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

            // 把当前布局通道 + 元素输入合成为本元素的仿射矩形系数（祖先
            // 未旋转帧中的局部量）。与 Element::resolve_layout 同式，只是
            // 保持 (w,h) 为符号量；center 通道含半屏项（左下原点像素系，
            // 与 resolved_rect 约定一致）。
            const auto compose_local_channels = [](
                const Element& elem, const WorldLayout& layout,
                WorldLayout::affine2& size,
                WorldLayout::affine2& pivot_offset,
                WorldLayout::affine2& local_center,
                WorldLayout::affine2& anchor)
            {
                const auto size_space =
                    WorldLayout::ratio_space{ elem.size, {}, {}, {} }
                    + layout.unit.scaled(elem.size_ratio);

                size = WorldLayout::affine2::from_ratio(size_space);
                pivot_offset = WorldLayout::affine2::from_ratio(
                    Element::pivot_shift(elem.pivot, size_space));

                const WorldLayout::affine2 half_screen{
                    math::vec2(), math::vec2(0.5f, 0.f), math::vec2(0.f, 0.5f) };
                local_center = WorldLayout::affine2::from_ratio(
                    layout.base + layout.offset
                    + WorldLayout::ratio_space{ elem.offset, {}, {}, {} }
                    + layout.unit.scaled(elem.offset_ratio))
                    + pivot_offset + half_screen;
                // 锚点 = 自身偏移通道（offset/offset_ratio）归零时的枢轴落点，
                // 供编辑器偏移拖拽作参考原点。
                anchor = WorldLayout::affine2::from_ratio(
                    layout.base + layout.offset)
                    + pivot_offset + half_screen;
            };

            // 沿父链复合祖先旋转（系数空间）：子局部量先绕父枢轴转父自身角，
            // 再经父祖先累计角提升到显示系。与旧图形端逐相机像素复合逐位
            // 等价（线性映射与线性代入可交换，仅浮点舍入顺序不同）。
            const auto orbit_to_display = [](
                const WorldLayout::affine2& local,
                const ResolvedParent& parent)
            {
                const auto parent_pivot =
                    parent.local_center - parent.pivot_offset;
                const auto rotated_by_parent = parent_pivot
                    + (local - parent_pivot).rotated(parent.own_angle);
                const float parent_angle_acc =
                    parent.layout->rotation - parent.own_angle;
                return parent.display_center
                    + (rotated_by_parent - parent.local_center)
                    .rotated(parent_angle_acc);
            };

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

                    // 根的显示量 = 局部量（无祖先旋转链）。
                    WorldLayout::affine2 size, pivot_off, local_center, anchor_pos;
                    compose_local_channels(
                        elem, layout, size, pivot_off, local_center, anchor_pos);
                    layout.size = size;
                    layout.pivot_offset = pivot_off;
                    layout.center = local_center;
                    layout.anchor = anchor_pos;

                    if (anchor != nullptr)
                    {
                        resolved_parents.emplace(
                            anchor->uid, ResolvedParent{
                                &elem, &layout,
                                local_center, local_center, pivot_off,
                                rotation ? rotation->angle : 0.f });
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
                        const auto& parent = fnd->second;
                        const auto* parent_elem = parent.elem;
                        const auto* parent_layout = parent.layout;

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

                        // 已复合显示矩形：局部系数 + 祖先旋转链（系数空间复合，
                        // 与旧图形端逐相机像素复合逐位等价）。
                        WorldLayout::affine2 size, pivot_off, local_center, anchor_local;
                        compose_local_channels(
                            *current_idx->elem, *current_idx->layout,
                            size, pivot_off, local_center, anchor_local);

                        current_idx->layout->size = size;
                        current_idx->layout->pivot_offset = pivot_off;
                        current_idx->layout->center =
                            orbit_to_display(local_center, parent);
                        current_idx->layout->anchor =
                            orbit_to_display(anchor_local, parent);

                        // 完成应用，将当前布局绑定到binding，然后从pending中删除当前项
                        if (current_idx->anchor_may_null != nullptr)
                            resolved_parents.emplace(
                                current_idx->anchor_may_null->uid,
                                ResolvedParent{
                                    current_idx->elem,
                                    current_idx->layout,
                                    local_center,
                                    current_idx->layout->center,
                                    pivot_off,
                                    current_idx->rotation_may_null != nullptr
                                        ? current_idx->rotation_may_null->angle
                                        : 0.f });

                        pending_anchor_information.erase(current_idx);
                    }
                }
                if (pending_anchor_information.size() == count)
                {
                    // 剩余布局缺失父布局或祖布局，不做处理以确保问题立即被发现；
                    break;
                }
            }

            // 父链缺失的孤儿按根语义复合已显示矩形（与旧图形端“孤儿按根
            // 回退”一致）；不注册 anchor——其子孙同样回退为根。
            for (auto& orphan : pending_anchor_information)
            {
                WorldLayout::affine2 size, pivot_off, local_center, anchor_pos;
                compose_local_channels(
                    *orphan.elem, *orphan.layout,
                    size, pivot_off, local_center, anchor_pos);
                orphan.layout->size = size;
                orphan.layout->pivot_offset = pivot_off;
                orphan.layout->center = local_center;
                orphan.layout->anchor = anchor_pos;
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
