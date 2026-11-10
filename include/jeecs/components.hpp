#pragma once

// ============================================================================
// jeecs/components.hpp —— 内置组件（C++）：Transform / UserInterface / Renderer / Camera / Physics2D / Light2D / Animation / Particle / Tilemap / Audio / Input
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/math.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/animation.hpp"
#include "jeecs/audio.hpp"
#include "jeecs/io.hpp"
#include "jeecs/tilemap.hpp"
#include "jeecs/game.hpp"
#include "jeecs/typing.hpp"

#ifdef __cplusplus
namespace jeecs
{
    namespace Transform
    {
        // An entity without childs and parent will contain these components:
        // LocalPosition/LocalRotation/LocalScale and using LocalToWorld to apply
        // local transform to Translation
        // If an entity have childs, it will have Anchor
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
        struct LocalRotation
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalRotation);
            JECS_DEFAULT_CONSTRUCTOR(LocalRotation);

            math::quat rot;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalRotation::rot, "rot");
            }
        };
        struct LocalPosition
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalPosition);
            JECS_DEFAULT_CONSTRUCTOR(LocalPosition);

            math::vec3 pos;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalPosition::pos, "pos");
            }
        };
        struct LocalScale
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalScale);
            JECS_DEFAULT_CONSTRUCTOR(LocalScale);

            math::vec3 scale = { 1.0f, 1.0f, 1.0f };

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalScale::scale, "scale");
            }
        };
        struct Translation
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Translation);
            JECS_DEFAULT_CONSTRUCTOR(Translation);

            float object2world[4][4] = {};

            math::vec3 world_position = { 0.0f, 0.0f, 0.0f };
            math::quat world_rotation;
            math::vec3 local_scale = { 1.0f, 1.0f, 1.0f };

            math::quat get_parent_rotation(const LocalRotation* local_rot) const noexcept
            {
                if (local_rot != nullptr)
                    return world_rotation * local_rot->rot.inverse();
                return world_rotation;
            }
            math::vec3 get_parent_position(const LocalPosition* local_pos, const LocalRotation* rotation) const noexcept
            {
                if (local_pos)
                    return world_position - get_parent_rotation(rotation) * local_pos->pos;
                return world_position;
            }
            void set_global_rotation(const math::quat& _rot, LocalRotation* rot)
            {

                if (rot)
                    rot->rot = _rot * get_parent_rotation(rot).inverse();
                world_rotation = _rot;
            }
            void set_global_position(const math::vec3& _pos, LocalPosition* pos, const LocalRotation* rot)
            {
                if (pos)
                    pos->pos = get_parent_rotation(rot).inverse() * (_pos - get_parent_position(pos, rot));
                world_position = _pos;
            }
        };
        struct Anchor
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Anchor);

            typing::uid_t uid = typing::uid_t::generate();

            Anchor() = default;
            Anchor(Anchor&&) = default;
            Anchor(const Anchor&) {}

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Anchor::uid, "uid");
            }
        };
        struct LocalToParent
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalToParent);
            JECS_DEFAULT_CONSTRUCTOR(LocalToParent);

            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;

            typing::uid_t parent_uid = {};

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalToParent::parent_uid, "parent_uid");
            }
        };
        struct LocalToWorld
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalToWorld);
            JECS_DEFAULT_CONSTRUCTOR(LocalToWorld);

            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;
        };
        static_assert(offsetof(LocalToParent, pos) == offsetof(LocalToWorld, pos));
        static_assert(offsetof(LocalToParent, scale) == offsetof(LocalToWorld, scale));
        static_assert(offsetof(LocalToParent, rot) == offsetof(LocalToWorld, rot));
    }
    namespace UserInterface
    {
        // 布局层级的解析结果：由 UI 布局阶段（TranslationUpdatingSystem）每帧
        // 从 Element 重建后写入，用户只读。分两层：
        //   参考系（base/offset/unit，单位一空间 ratio_space 分桶、相对显示区
        //中心）——层级传播的中间态，也是 Element::resolve_layout（祖先未旋转
        //帧中的局部矩形）的输入：
        //   锚定基准点（base）= 父链基点 + 父枢轴修正 + 自身 anchor 锚定
        //（根元素为相对显示区的锚定），纯几何量；
        //   父链偏移（offset）= 全部祖先自身偏移（offset + offset_ratio 按
        //各自单位一折算）的累计，不含本元素自身偏移——父链位移按原像素
        //量被子元素继承，不受子元素 ratio_unit 影响；
        //   单位一（unit）= 本元素相对量（offset_ratio/size_ratio）的参照
        //尺寸——根元素为显示区、子元素为父元素有效尺寸，已按本元素
        //ratio_unit 折算（height_unit/width_unit 取高/宽标量广播到两轴，
        //per_axis 逐轴取宽/高）；
        //   已复合显示矩形（center/size/pivot_offset/anchor，affine2 系数）
        //——父子关系已完全展开：每个通道只是目标缓冲区宽高 (w,h) 的线性
        //函数（含祖先旋转链复合，不含自身角），绘制/命中测试端按相机目标
        //尺寸像素化即可（Element::resolve_display_rect），无需再沿父链遍历；
        //   有效旋转角（rotation）= 全部祖先与自身 Rotation::angle 之和（度）。
        // 未携带本组件的元素不参与 UI 布局与渲染（参见编辑器的 UI 组件预设）。
        struct WorldLayout
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(WorldLayout);
            JECS_DEFAULT_CONSTRUCTOR(WorldLayout);

            // 单位一空间：布局量按“相对量的参照空间”分桶传播。
            // 绝对通道为像素；三个相对分桶在解析时乘以各自的显示区参照：
            //   per_axis ⊙ (w, h) —— 逐轴：x 轴以显示区宽、y 轴以显示区高为单位；
            //   height   ⊙ (h, h) —— 两轴同以显示区高为单位一（height_unit 语义）；
            //   width    ⊙ (w, w) —— 两轴同以显示区宽为单位一（width_unit 语义）。
            // 布局阶段不知道显示区尺寸，“以高/宽为单位一”的横向量（x 轴上 ×h
            // 的量）无法表达为逐轴分数（需要 h/w 纵横比），必须独立分桶；解析端
            //（ratio_space::resolve）对三桶各乘参照后求和，无需任何 ratio_unit 相关的
            // 绘制端折算。层级传播（父链偏移继承、父矩形锚定/枢轴修正、单位一
            // 折算）全部在桶内逐分量进行，跨元素继承的量不会被另一个元素的
            // ratio_unit 重新解释。
            struct ratio_space
            {
                math::vec2 absolute = {}; // 像素
                math::vec2 per_axis = {}; // ×(显示区宽, 显示区高)
                math::vec2 height = {};   // ×(显示区高, 显示区高)
                math::vec2 width = {};    // ×(显示区宽, 显示区宽)

                friend ratio_space operator+(
                    const ratio_space& a, const ratio_space& b) noexcept
                {
                    return ratio_space{
                        a.absolute + b.absolute,
                        a.per_axis + b.per_axis,
                        a.height + b.height,
                        a.width + b.width };
                }

                // 用无单位比例（如 offset_ratio/size_ratio）同时缩放全部通道。
                // 因为比例无量纲，各桶同乘后像素语义保持：
                // ratio ⊙ s 最终解析为 ratio ∘ resolve(s)。
                ratio_space scaled(const math::vec2& ratio) const noexcept
                {
                    return ratio_space{
                        absolute * ratio,
                        per_axis * ratio,
                        height * ratio,
                        width * ratio };
                }

                // 单位一空间 → 显示像素：绝对 + 逐轴桶×(w,h) + 高桶×h + 宽桶×w。
                math::vec2 resolve(float w, float h) const noexcept
                {
                    return absolute
                        + per_axis * math::vec2(w, h)
                        + height * h
                        + width * w;
                }
            };

            // 已复合显示矩形的系数空间：值 = k0 + kw*w + kh*h（对目标缓冲区
            // 宽高 (w,h) 线性）。ratio_space 的 resolve 对 (w,h) 线性，且定点
            // 旋转是线性映射——R·(k0 + kw·w + kh·h) = R·k0 + R·kw·w + R·kh·h
            //——因此布局阶段可以在系数空间完成“像素化 + 祖先旋转链复合”
            //（子中心绕父枢轴转父自身角、再经父祖先累计角提升，逐级都是
            // 旋转+平移+系数线性组合），绘制端只需代入具体宽高；同一份系数
            // 对任意相机目标尺寸都成立。带语义的分桶空间对旋转不封闭
            //（per_axis 桶会在旋转下分裂进宽/高桶），from_ratio 的有损合并
            // 恰好完成这一步。
            struct affine2
            {
                math::vec2 k0 = {}; // 常数项（像素）
                math::vec2 kw = {}; // ×宽系数
                math::vec2 kh = {}; // ×高系数

                math::vec2 resolve(float w, float h) const noexcept
                {
                    return k0 + kw * w + kh * h;
                }

                // 三系数向量各自旋转（度）：等价于对任意 (w,h) 的 resolve
                // 结果整体绕原点旋转。
                affine2 rotated(float deg) const noexcept
                {
                    return affine2{
                        k0.rotate(deg), kw.rotate(deg), kh.rotate(deg) };
                }

                affine2 operator+(const affine2& another) const noexcept
                {
                    return affine2{
                        k0 + another.k0, kw + another.kw, kh + another.kh };
                }
                affine2 operator-(const affine2& another) const noexcept
                {
                    return affine2{
                        k0 - another.k0, kw - another.kw, kh - another.kh };
                }

                // 分桶 → 系数（有损合并：per_axis 与 width 的 x 系数、
                // per_axis 与 height 的 y 系数并桶；resolve 结果不变）。
                static affine2 from_ratio(const ratio_space& s) noexcept
                {
                    return affine2{
                        s.absolute,
                        math::vec2(s.per_axis.x + s.width.x, s.width.y),
                        math::vec2(s.height.x, s.per_axis.y + s.height.y) };
                }
            };

            // 锚定基准点（单位一空间）：父链基点 + 父枢轴修正 + 自身 anchor
            // 锚定（根元素为相对显示区的锚定）。纯几何量——子元素永远以
            // 父矩形的中心/边角为基准点，即便父矩形由 size_ratio 撑起、且
            // 子元素 ratio_unit 不是 per_axis，锚定位置也不受子元素
            // ratio_unit 影响。
            ratio_space base = {};
            // 父链累计自身偏移（单位一空间）：全部祖先的 offset 与
            // offset_ratio（按祖先各自的单位一折算）之和，不含本元素自身
            // 偏移（自身偏移由 Element::resolve_layout 按本元素单位一现算）。
            // 分桶传播保证父 offset_ratio 位移被子元素按原像素量继承。
            ratio_space offset = {};
            // 本元素相对量的单位一（单位一空间）：参照矩形（根元素=显示区，
            // 子元素=父元素有效尺寸）按本元素 ratio_unit 折算后的通道。
            ratio_space unit = {};

            // ---- 已复合显示矩形系数（布局阶段写入，父子关系已展开）----

            // 元素中心的显示位置系数：含半屏 (w/2,h/2) 项与全部祖先旋转链的
            // 复合（子中心绕父枢轴转父自身角、再经父祖先累计角逐级提升）；
            // 不含自身角——自身旋转由绘制端按 rotation 绕枢轴施加。resolve
            // 后为左下原点像素系（与 Element::resolved_rect 同约定）。
            affine2 center = {};
            // 有效尺寸系数（未旋转局部量 = 绝对输入 + size_ratio ⊙ 单位一）。
            affine2 size = {};
            // 枢轴到元素中心的偏移系数（未旋转局部量，绘制旋转的中心修正量）。
            affine2 pivot_offset = {};
            // 锚点显示位置系数：自身 offset/offset_ratio 归零时枢轴的落点
            //（偏移拖拽的参考原点，含祖先旋转链；供编辑器/命中测试使用）。
            affine2 anchor = {};

            // 有效旋转角（度，绕元素枢轴）= 全部祖先与自身的 Rotation::angle 之和
            //（未挂 Rotation 组件的实体按 0 计）。由布局阶段沿 Anchor+LocalToParent
            // 父链逐级递推缓存（父有效角+自身角）；祖先分量已复合进 center/anchor
            // 系数，本值供绘制端构建自身旋转矩阵与命中测试使用。
            float rotation = 0.f;
        };

        // UI 元素的布局输入（用户编写，系统只读）。
        // 一个 UI 元素 = Element（布局输入）+ WorldLayout（层级解析结果，系统写入），
        // 层级关系复用 Transform 的 Anchor + LocalToParent。
        struct Element
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Element);
            JECS_DEFAULT_CONSTRUCTOR(Element);

            // UI 元素的对齐标记，可按位组合（如 left | top）；center 为 0 即无标记。
            enum alignment : uint8_t
            {
                center = 0,

                left = 1 << 0,
                right = 1 << 1,
                top = 1 << 2,
                bottom = 1 << 3,
            };

            // 相对量（offset_ratio/size_ratio）的单位一基准：
            //   height_unit —— 两轴都以参考矩形的“高”为单位一（标量单位，
            //                  比例不随参考矩形宽高比拉伸，等同旧
            //                  keep_vertical_ratio = true）；
            //   width_unit  —— 两轴都以参考矩形的“宽”为单位一（等同旧
            //                  keep_vertical_ratio = false）；
            //   per_axis    —— x 轴以参考矩形的“宽”、y 轴以“高”为单位一，
            //                  宽高各自按参考矩形对应轴的比例缩放。
            // 参考矩形：根元素为显示区，子元素为父元素矩形。
            enum ratio_unit : uint8_t
            {
                height_unit = 0,
                width_unit = 1,
                per_axis = 2,
            };

            // 参照锚点：偏移起点位于参照矩形的哪个方位（默认中心）。
            // 根元素的参照矩形是整个显示区；子元素（经 Transform::LocalToParent
            // 挂钩）的参照矩形是父 UI 元素的矩形。由布局阶段解析进 WorldLayout。
            alignment anchor = alignment::center;

            // 元素枢轴：元素盒上的哪个点对齐到偏移起点（默认元素中心）。
            alignment pivot = alignment::center;

            // 相对量（offset_ratio/size_ratio）的单位一基准，三选一，见
            // Element::ratio_unit（默认 height_unit，即以参考矩形的
            // 高为单位一）。参考矩形：根元素为显示区，子元素为父元素矩形。
            // 由布局阶段解析进 WorldLayout。
            ratio_unit unit_kind = ratio_unit::height_unit;

            // 偏移与尺寸各含绝对（像素）与相对两个通道，最终取两者之和。
            // 相对通道按 ratio_unit 取参考矩形的高/宽为单位一：
            // 根元素的参考矩形为显示区，子元素（经 Transform::LocalToParent 挂钩）
            // 为父元素矩形。由布局阶段解析进 WorldLayout。
            math::vec2 offset = {};
            math::vec2 offset_ratio = {};
            math::vec2 size = {};
            math::vec2 size_ratio = {};

            // 解析后的 UI 元素矩形：以显示区左下角为原点，单位像素。
            struct resolved_rect
            {
                // 元素中心位置（非枢轴位置）。
                math::vec2 center = {};
                math::vec2 size = {};
                // 枢轴点到元素中心的偏移，绘制旋转时作为旋转中心修正量。
                math::vec2 pivot_offset = {};

                // 鼠标命中测试：mouse_view_pos 为视口空间坐标（-1..1，y 轴向上），
                // rot_angle 为元素旋转角（度，与 Rotation::angle 同单位；通常传
                // WorldLayout::rotation，即全部祖先累计 + 自身的有效角）。
                bool hit_test(
                    float rot_angle,
                    math::vec2 mouse_view_pos,
                    float w, float h) const noexcept
                {
                    const math::vec2 abs_mouse =
                        (mouse_view_pos + math::vec2(1.f, 1.f)) / 2.f * math::vec2(w, h);

                    const math::vec3 corrected_mouse_diff =
                        math::quat::euler(0., 0., -rot_angle) * math::vec3(abs_mouse - center);

                    return abs(corrected_mouse_diff.x) < size.x / 2.f
                        && abs(corrected_mouse_diff.y) < size.y / 2.f;
                }
            };

            // 参照锚点相对参照矩形（单位一空间尺寸）的偏移，按方位标记取半尺寸
            // 符号：left 取 -x/2，right 取 +x/2，top 取 +y/2，bottom 取 -y/2。
            // 根元素的参照矩形是整个显示区（绝对0 + 逐轴相对1），子元素是父元素矩形。
            static WorldLayout::ratio_space anchor_shift(
                alignment anchor, const WorldLayout::ratio_space& rect) noexcept
            {
                math::vec2 sign(0.f, 0.f);
                if (anchor & alignment::left)
                    sign.x -= 1.f;
                if (anchor & alignment::right)
                    sign.x += 1.f;
                if (anchor & alignment::top)
                    sign.y += 1.f;
                if (anchor & alignment::bottom)
                    sign.y -= 1.f;
                return rect.scaled(sign * 0.5f);
            }

            // 枢轴到矩形中心的偏移（单位一空间），按方位标记取半尺寸符号：
            // left 取 +x/2，right 取 -x/2，top 取 -y/2，bottom 取 +y/2。
            static WorldLayout::ratio_space pivot_shift(
                alignment pivot, const WorldLayout::ratio_space& rect) noexcept
            {
                math::vec2 sign(0.f, 0.f);
                if (pivot & alignment::left)
                    sign.x += 1.f;
                if (pivot & alignment::right)
                    sign.x -= 1.f;
                if (pivot & alignment::top)
                    sign.y -= 1.f;
                if (pivot & alignment::bottom)
                    sign.y += 1.f;
                return rect.scaled(sign * 0.5f);
            }

            // 按单位一基准折算参照矩形，得到本元素相对量的单位一：
            //   per_axis    —— 矩形通道原样（x 轴宽、y 轴高逐轴）；
            //   height_unit —— 取矩形 y 轴标量广播到两轴：
            //       Rh = absolute.y + (per_axis.y + height.y)×h + width.y×w，
            //     故 absolute 取 y 广播、height 桶取 (per_axis.y+height.y) 广播、
            //     width 桶取 width.y 广播；
            //   width_unit  —— 取矩形 x 轴标量广播到两轴，对称地：
            //       Rw = absolute.x + (per_axis.x + width.x)×w + height.x×h。
            static WorldLayout::ratio_space fold_unit(
                const WorldLayout::ratio_space& rect, ratio_unit kind) noexcept
            {
                switch (kind)
                {
                case ratio_unit::height_unit:
                    return WorldLayout::ratio_space{
                        math::vec2(rect.absolute.y, rect.absolute.y),
                        math::vec2(0.f, 0.f),
                        math::vec2(
                            rect.per_axis.y + rect.height.y,
                            rect.per_axis.y + rect.height.y),
                        math::vec2(rect.width.y, rect.width.y) };
                case ratio_unit::width_unit:
                    return WorldLayout::ratio_space{
                        math::vec2(rect.absolute.x, rect.absolute.x),
                        math::vec2(0.f, 0.f),
                        math::vec2(rect.height.x, rect.height.x),
                        math::vec2(
                            rect.per_axis.x + rect.width.x,
                            rect.per_axis.x + rect.width.x) };
                case ratio_unit::per_axis:
                default:
                    return rect;
                }
            }

            // 将布局输入解析为显示区（宽 w、高 h）内的最终矩形。
            // world 为布局阶段解析好的参考系（锚定基准点 + 父链偏移 + 单位一）：
            // 元素自身的 anchor 与 offset_ratio/size_ratio 的层级语义（根元素相对
            // 显示区、子元素相对父元素矩形）已由布局阶段折算进 world（分桶传播），
            // 此处只做像素化（ratio_space::resolve）与枢轴修正——无任何 ratio_unit
            // 相关的绘制端折算：单位一语义已在布局阶段折进分桶。
            resolved_rect resolve_layout(
                const WorldLayout& world, float w, float h) const noexcept
            {
                // 有效尺寸 = 绝对输入 + size_ratio ⊙ 本元素单位一（分桶）；
                // 枢轴修正按该尺寸取半。
                const WorldLayout::ratio_space size_space =
                    WorldLayout::ratio_space{ size, {}, {}, {} }
                    + world.unit.scaled(size_ratio);

                resolved_rect rect;
                rect.size = size_space.resolve(w, h);
                rect.pivot_offset =
                    pivot_shift(pivot, size_space).resolve(w, h);

                // 元素中心 = 显示区中心 + 锚定基准点（纯几何量）
                //          + 父链累计偏移 + 自身偏移（绝对 + offset_ratio ⊙ 单位一）
                //          + 枢轴修正。
                rect.center = math::vec2(w / 2.0f, h / 2.0f)
                    + world.base.resolve(w, h)
                    + world.offset.resolve(w, h)
                    + offset
                    + world.unit.scaled(offset_ratio).resolve(w, h)
                    + rect.pivot_offset;
                return rect;
            }

            // 像素化布局阶段已复合的显示矩形（WorldLayout 的仿射系数通道）：
            // 父子层级与祖先旋转链已在系数中展开，本函数只做线性代入，
            // 任意相机目标尺寸各调用一次即可。rect.center 为含祖先旋转链的
            // 显示位置；自身旋转仍由消费方按 world.rotation 绕枢轴施加。
            resolved_rect resolve_display_rect(
                const WorldLayout& world, float w, float h) const noexcept
            {
                resolved_rect rect;
                rect.center = world.center.resolve(w, h);
                rect.size = world.size.resolve(w, h);
                rect.pivot_offset = world.pivot_offset.resolve(w, h);
                return rect;
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Element::anchor, "anchor");
                typing::register_member(guard, &Element::pivot, "pivot");
                typing::register_member(guard, &Element::unit_kind, "unit_kind");
                typing::register_member(guard, &Element::offset, "offset");
                typing::register_member(guard, &Element::offset_ratio, "offset_ratio");
                typing::register_member(guard, &Element::size, "size");
                typing::register_member(guard, &Element::size_ratio, "size_ratio");
            }
        };

        struct Rotation
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Rotation);
            JECS_DEFAULT_CONSTRUCTOR(Rotation);

            // 旋转角（度，绕元素枢轴，与 Transform 欧拉角同单位，
            // 绘制端经 quat::euler 按 DEG2RAD 消费）。
            // 旋转会带动整个 UI 子树：子元素绕本元素枢轴一同旋转，
            // 子元素的有效旋转角为全部祖先角与自身角之和，由布局阶段沿父链
            // 递推缓存进 WorldLayout::rotation。
            float angle = 0.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Rotation::angle, "angle");
            }
        };
        struct Text
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Text);
            JECS_DEFAULT_CONSTRUCTOR(Text);

            // 自动换行策略（显式 '\n' 硬换行始终支持，'\r' 被忽略）：
            //   none —— 不自动换行，长文本溢出元素宽度（无逐绘制裁剪能力，
            //           溢出部分可见）；
            //   word —— 按元素宽度折行：CJK 字符间可断行、拉丁文本按空格
            //           断词、超宽单词逐字断行，行尾空格不计宽。
            enum wrap_mode : uint8_t
            {
                none = 0,
                word = 1,
            };

            // UTF-8 文本内容，支持内联标记：{scale:f} {color:RRGGBBAA}
            // {offset:(x,y)}（三者均为绝对设置，非累加），'\' 转义下一个字符
            //（如 '\{' 输出字面 '{'）。标记是逐字实时属性：修改颜色 / 缩放 /
            // 偏移不触发任何重光栅化。offset 单位为基准字号的倍数，y 向上。
            basic::string content;

            // 字体文件资源；未指定时不渲染文本。
            // 文本由渲染管线逐字直接绘制（字形图集 + 批量四边形），无需
            // Renderer::Shaders/Shape/Textures：
            //   * 若实体持有 Renderer::Shaders，用其第一个着色器替代内置文本
            //     着色器绘制该文本的字形（自定义文本特效入口；顶点输入为
            //     vertex:float3, uv:float2, color:float4，纹理通道0为字形图集，
            //     坐标已烘焙进顶点，MVP 为 UI 视图矩阵）；
            //   * 若实体持有 Renderer::Color，作为整段文本的色调（与逐字
            //     颜色标记相乘）。
            basic::file_resource<graphic::font> font;

            // 文本块在元素矩形内的对齐（位掩码复用 Element::alignment，
            // center=0 即水平垂直都居中）：
            //   水平位（left/right）—— 各行在文本块宽内的对齐；
            //   垂直位（top/bottom）—— 文本块在元素矩形内的放置。
            // 行布局采用固定行网格（行高只取决于字体与基准字号，与内容、
            // {scale:} 标记无关），字形按基线相对放置，基线稳定不跳变；
            // 垂直对齐锚定字体度量行盒（ascent..descent），而非墨水包围盒。
            Element::alignment alignment = Element::alignment::center;

            // 自动换行策略，见 wrap_mode（默认按词换行）。
            wrap_mode wrap = wrap_mode::word;

            // 渲染管线的运行期逐字布局缓存，请勿手动修改（不反射、不序列化）。
            // 脏检查键为（内容, 字体, 量化字号, 折行宽度, 对齐）；布局结果为
            // 文本空间（原点=块左下角，y 向上，单位=基准字号像素）的字形放置。
            struct runtime_layout_t
            {
                // ---- 脏检查键 ----
                bool valid = false;
                const void* key_font = nullptr; // je_font_t*（图集条目持有字体引用，键存活期内稳定）
                int32_t key_font_px = 0;
                int32_t key_wrap_width_px = 0;  // -1 = 不折行
                uint32_t key_alignment = 0;
                basic::string key_content;

                // ---- 布局结果 ----
                struct glyph_t
                {
                    char32_t cp = 0;
                    int32_t raster_px = 0; // 该字形的量化光栅字号（基准字号×scale）
                    float x = 0.f, y = 0.f; // 字形盒左下角（含 offset 标记）
                    math::vec4 color = math::vec4(0.f, 0.f, 0.f, 0.f);
                };
                basic::vector<glyph_t> glyphs; // 仅含有位图的字形（空格等不出席）
                float block_w = 0.f; // 自然尺寸：最长行进距（剔除行尾空格）
                float block_h = 0.f; // 自然尺寸：ascent+descent+(行数-1)×行高
                float win_base_px = 0.f; // 布局时基准轴的未量化像素，供相机目标间缩放
            };
            runtime_layout_t runtime_layout;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Text::content, "content");
                typing::register_member(guard, &Text::font, "font");
                typing::register_member(guard, &Text::alignment, "alignment");
                typing::register_member(guard, &Text::wrap, "wrap");
            }
        };

        // 鼠标与 UI 元素的交互状态（用户挂载，UI 渲染管线逐帧写入，用户只读）。
        // 挂在需要鼠标交互的 UI 实体上（如按钮；通常同时挂 Element + WorldLayout）
        // 后，UI 渲染管线在该实体的每个摄像机绘制阶段做命中测试：
        //   1) 只处理目标为窗口的摄像机——渲染到 Framebuffer 的摄像机是离屏
        //      目标，鼠标不存在于其空间中，不参与命中；
        //   2) 鼠标必须落在摄像机的 Camera::Viewport 子视口内（未挂视口则
        //      整个目标）；视口只裁剪绘制，元素矩形仍按目标完整尺寸解析；
        //   3) 元素矩形按该摄像机目标尺寸像素化（resolve_display_rect），
        //      命中测试含元素自身角与祖先旋转链（WorldLayout::rotation）；
        //   4) 不参与渲染（无 Shape/Text/Shaders）的元素同样参与测试——
        //      可用于不可见按钮热区。
        // 命中状态每帧先清零再逐摄像机累加：同一元素被多个摄像机命中时
        // 保持为 true。
        struct MouseOn
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(MouseOn);
            JECS_DEFAULT_CONSTRUCTOR(MouseOn);

            // 本帧鼠标是否位于该元素上（任一参与测试的摄像机命中）。
            bool is_mouse_on = false;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &MouseOn::is_mouse_on, "is_mouse_on");
            }
        };
    };
    namespace Renderer
    {
        // An entity need to be rendered, must have Transform::Translation and
        // Renderer,
        /*

        (Viewport)
        OrthoProjection/PerspectiveProjection
        Projection ----\
                         >------> Cameras
                        /          / | \
        Translation ---<          /  |  \ (for each..)
                        \        /   |   \
                         >--  The entities need
        Material--------/         be rend..
                       /
        (Shape)-------/

        */
        struct Rendqueue
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Rendqueue);
            JECS_DEFAULT_CONSTRUCTOR(Rendqueue);

            int rend_queue = 0;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Rendqueue::rend_queue, "rend_queue");
            }
        };
        struct Shape
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Shape);
            JECS_DEFAULT_CONSTRUCTOR(Shape);

            basic::optional<basic::resource<graphic::vertex>> vertex;
        };
        struct Shaders
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Shaders);
            JECS_DEFAULT_CONSTRUCTOR(Shaders);

            basic::vector<basic::resource<graphic::shader>> shaders;

            template <typename T>
            void set_uniform(const std::string& name, const T& val) noexcept
            {
                for (auto& shad : shaders)
                    shad->set_uniform(name, val);
            }
        };
        struct Textures2D
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Textures2D);
            JECS_DEFAULT_CONSTRUCTOR(Textures2D);

            // NOTE: textures should not be nullptr!
            struct texture_with_passid
            {
                size_t m_pass_id;
                basic::resource<graphic::texture2d> m_texture;

                texture_with_passid(size_t pass, const basic::resource<graphic::texture2d>& tex)
                    : m_pass_id(pass), m_texture(tex)
                {
                }
            };
            math::vec2 tiling = math::vec2(1.f, 1.f);
            math::vec2 offset = math::vec2(0.f, 0.f);
            basic::vector<texture_with_passid> textures;

            void bind_texture(size_t passid, const basic::resource<graphic::texture2d>& texture)
            {
                for (auto& pair : textures)
                {
                    if (pair.m_pass_id == passid)
                    {
                        pair.m_texture = texture;
                        return;
                    }
                }
                textures.push_back(texture_with_passid(passid, texture));
            }
            void remove_texture(size_t passid)
            {
                for (auto i = textures.begin(); i != textures.end(); ++i)
                {
                    if (i->m_pass_id == passid)
                    {
                        textures.erase(i);
                        return;
                    }
                }
            }
            std::optional<basic::resource<graphic::texture2d>> get_texture(size_t passid) const
            {
                for (auto& [pass, tex] : textures)
                    if (pass == passid)
                        return tex;

                return std::nullopt;
            }
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Textures2D::tiling, "tiling");
                typing::register_member(guard, &Textures2D::offset, "offset");
            }
        };
        // 纹理数组通道绑定（与 Textures2D 平行的强类型组件，只持有
        // texture_array）：瓦片地图分片用它把"每层一个瓦片"的数组纹理
        // 绑到 pass 0，采样侧用 tex2darray（shader DSL）
        struct Textures2DArray
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Textures2DArray);
            JECS_DEFAULT_CONSTRUCTOR(Textures2DArray);

            // NOTE: textures should not be nullptr!
            struct texture_with_passid
            {
                size_t m_pass_id;
                basic::resource<graphic::texture_array> m_texture;

                texture_with_passid(size_t pass, const basic::resource<graphic::texture_array>& tex)
                    : m_pass_id(pass), m_texture(tex)
                {
                }
            };
            basic::vector<texture_with_passid> textures;

            void bind_texture(size_t passid, const basic::resource<graphic::texture_array>& texture)
            {
                for (auto& pair : textures)
                {
                    if (pair.m_pass_id == passid)
                    {
                        pair.m_texture = texture;
                        return;
                    }
                }
                textures.push_back(texture_with_passid(passid, texture));
            }
            void remove_texture(size_t passid)
            {
                for (auto i = textures.begin(); i != textures.end(); ++i)
                {
                    if (i->m_pass_id == passid)
                    {
                        textures.erase(i);
                        return;
                    }
                }
            }
            std::optional<basic::resource<graphic::texture_array>> get_texture(size_t passid) const
            {
                for (auto& [pass, tex] : textures)
                    if (pass == passid)
                        return tex;

                return std::nullopt;
            }
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                (void)guard;
            }
        };
        struct Color
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Color);
            JECS_DEFAULT_CONSTRUCTOR(Color);

            math::color color = math::color(1.0f, 1.0f, 1.0f, 1.0f);

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Color::color, "color");
            }
        };
    }
    namespace Camera
    {
        struct FrustumCulling
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(FrustumCulling);
            JECS_DEFAULT_CONSTRUCTOR(FrustumCulling);

            float frustum_plane_distance[6] = {};
            math::vec3 frustum_plane_normals[6] = {};

            void set_plane_normal_and_distance(
                size_t plane_idx, float nx, float ny, float nz, float d)
            {
                auto& normal = frustum_plane_normals[plane_idx];

                normal.x = nx;
                normal.y = ny;
                normal.z = nz;

                const float len = normal.length();
                normal /= len;

                frustum_plane_distance[plane_idx] = d / len;
            }
            bool test_circle(const math::vec3& origin, float r) const
            {
                for (size_t index = 0; index < 6; ++index)
                {
                    if (frustum_plane_normals[index].dot(origin) + frustum_plane_distance[index] < -r)
                        return false;
                }
                return true;
            }
        };
        struct Projection
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Projection);

            float znear = 0.3f;
            float zfar = 1000.0f;

            jeecs::basic::resource<jeecs::graphic::uniformbuffer>
                default_uniform_buffer = jeecs::graphic::uniformbuffer::create(
                    0, sizeof(graphic::BasePipelineInterface::default_uniform_buffer_data_t)).value();

            float view[4][4] = {};
            float projection[4][4] = {};
            float view_projection[4][4] = {};
            float inv_projection[4][4] = {};

            Projection() = default;
            Projection(const Projection&) { /* Do nothing */ }
            Projection(Projection&&) = default;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Projection::znear, "znear");
                typing::register_member(guard, &Projection::zfar, "zfar");
            }
        };
        struct OrthoProjection
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(OrthoProjection);
            JECS_DEFAULT_CONSTRUCTOR(OrthoProjection);

            float scale = 1.0f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &OrthoProjection::scale, "scale");
            }
        };
        struct PerspectiveProjection
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(PerspectiveProjection);
            JECS_DEFAULT_CONSTRUCTOR(PerspectiveProjection);

            float angle = 75.0f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &PerspectiveProjection::angle, "angle");
            }
        };
        struct Viewport
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Viewport);
            JECS_DEFAULT_CONSTRUCTOR(Viewport);

            math::vec4 viewport = math::vec4(0, 0, 1, 1);
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Viewport::viewport, "viewport");
            }
        };
        struct Clear
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Clear);
            JECS_DEFAULT_CONSTRUCTOR(Clear);

            math::color color = math::color(0.f, 0.f, 0.f, 1.f);
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Clear::color, "color");
            }
        };
        struct RendToFramebuffer
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(RendToFramebuffer);
            JECS_DEFAULT_CONSTRUCTOR(RendToFramebuffer);

            basic::optional<basic::resource<graphic::framebuffer>> framebuffer;
        };
    }
    namespace Physics2D
    {
        // ===================== Scene-level config =====================
        // Exactly one entity per game_world should carry this. The referenced
        // woolang script is the single source of truth for the scene's physics:
        //   1. the number of physics layers (worlds),
        //   2. per-layer settings (gravity / solver substeps / sleeping / CCD),
        //   3. the collision-group rules shared across ALL layers.
        // Rigidbodies bind to a layer via Rigidbody::layerid (0..N-1).
        struct Scene
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Scene);
            JECS_DEFAULT_CONSTRUCTOR(Scene);

            basic::file_resource<> physics_config;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Scene::physics_config, "physics_config");
            }
        };

        // ===================== Rigidbody layer =====================
        // Mandatory component for any 2D-physics-driven entity. `layerid` binds it
        // to a layer declared by the scene's Physics2D::Scene config script.
        struct Rigidbody
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Rigidbody);
            JECS_DEFAULT_CONSTRUCTOR(Rigidbody);

            size_t layerid = 0;

            // Stable cross-frame body identifier, written by Physics2DUpdatingSystem
            // every PhysicsUpdate. Survives archetype migration (value semantics,
            // not a pointer), so it is safe to read in any system phase — including
            // the ones that run before PhysicsUpdate, which observe the previous
            // frame's CollisionResult.
            // 0 = no body has been created yet (before first PhysicsUpdate, or
            //     after the owning layer disappeared from the scene config).
            uint64_t body_id = 0;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Rigidbody::layerid, "layerid");
            }
        };
        // BodyType tags: at most one. None = Static.
        struct DynamicBody
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(DynamicBody);
            JECS_DEFAULT_CONSTRUCTOR(DynamicBody);
        };
        struct KinematicBody
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(KinematicBody);
            JECS_DEFAULT_CONSTRUCTOR(KinematicBody);
        };
        // Tag: enable per-body CCD for fast-moving entities.
        struct Bullet
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Bullet);
            JECS_DEFAULT_CONSTRUCTOR(Bullet);
        };

        // ===================== Axis locks (orthogonal tags) =====================
        struct LockTranslationX
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LockTranslationX);
            JECS_DEFAULT_CONSTRUCTOR(LockTranslationX);
        };
        struct LockTranslationY
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LockTranslationY);
            JECS_DEFAULT_CONSTRUCTOR(LockTranslationY);
        };
        struct LockRotation
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LockRotation);
            JECS_DEFAULT_CONSTRUCTOR(LockRotation);
        };

        // ===================== Kinematic state (bidirectional) =====================
        struct LinearVelocity
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LinearVelocity);
            JECS_DEFAULT_CONSTRUCTOR(LinearVelocity);

            math::vec2 value = {};
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LinearVelocity::value, "value");
            }
        };
        struct AngularVelocity
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(AngularVelocity);
            JECS_DEFAULT_CONSTRUCTOR(AngularVelocity);

            float value = 0.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &AngularVelocity::value, "value");
            }
        };
        struct LinearDamping
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LinearDamping);
            JECS_DEFAULT_CONSTRUCTOR(LinearDamping);

            float value = 0.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LinearDamping::value, "value");
            }
        };
        struct AngularDamping
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(AngularDamping);
            JECS_DEFAULT_CONSTRUCTOR(AngularDamping);

            float value = 0.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &AngularDamping::value, "value");
            }
        };
        struct GravityScale
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(GravityScale);
            JECS_DEFAULT_CONSTRUCTOR(GravityScale);

            float value = 1.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &GravityScale::value, "value");
            }
        };

        // ===================== Colliders (mutually exclusive) =====================
        namespace Collider
        {
            struct Box
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Box);
                JECS_DEFAULT_CONSTRUCTOR(Box);

                math::vec2 size = math::vec2(1.f, 1.f);
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Box::size, "size");
                }
            };
            struct Circle
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Circle);
                JECS_DEFAULT_CONSTRUCTOR(Circle);

                float radius = 0.5f;
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Circle::radius, "radius");
                }
            };
            struct Capsule
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Capsule);
                JECS_DEFAULT_CONSTRUCTOR(Capsule);

                float radius = 0.25f;
                float height = 1.f;
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Capsule::radius, "radius");
                    typing::register_member(guard, &Capsule::height, "height");
                }
            };
            // Arbitrary simple polygon outline (convex or concave; vertices in
            // body-local space, any winding). Box2D has no native concave
            // "mesh" shape, so the physics system decomposes the loop into
            // convex parts (a single polygon when possible, otherwise ear-cut
            // triangles) and attaches them as compound shapes on one body.
            struct Mesh
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Mesh);
                JECS_DEFAULT_CONSTRUCTOR(Mesh);

                // ===================== Script-visible vertex loop =====================
                // A dynamic list of 2D points exposed to woolang as `array<vec2>`
                // (same mapping pattern as Light2D::BlockShadow::block_mesh).
                // Used by Collider::Mesh to store the polygon outline. Default value
                // is the unit square, matching Collider::Box's default extents.
                struct vertex_list
                {
                    basic::vector<math::vec2> points = {
                        math::vec2(-0.5f, -0.5f),
                        math::vec2(0.5f, -0.5f),
                        math::vec2(0.5f,  0.5f),
                        math::vec2(-0.5f,  0.5f),
                    };

                    static const char* JEScriptTypeName()
                    {
                        return "Physics2D::vertex_list";
                    }
                    static const char* JEScriptTypeDeclare()
                    {
                        return
                            "namespace Physics2D\n"
                            "{\n"
                            "    public using vertex_list = array<vec2>;\n"
                            "}";
                    }
                    void JEParseFromScriptType(woort_value v)
                    {
                        woort_value pos;
                        if (!woort_push_reserve(1, &pos))
                            woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                        else
                        {
                            const size_t point_count = woort_vec_len(v);

                            points.clear();

                            for (size_t i = 0; i < point_count; ++i)
                            {
                                (void)woort_vec_get(pos, v, i);

                                math::vec2 position;
                                position.JEParseFromScriptType(pos);

                                points.push_back(position);
                            }

                            woort_pop(1);
                        }
                    }
                    void JEParseToScriptType(woort_value v) const
                    {
                        woort_value pos;
                        if (!woort_push_reserve(1, &pos))
                            woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                        else
                        {
                            woort_set_vec(v);
                            woort_vec_resize(v, points.size());
                            for (size_t i = 0; i < points.size(); ++i)
                            {
                                points.at(i).JEParseToScriptType(pos);
                                (void)woort_vec_set(v, i, pos);
                            }

                            woort_pop(1);
                        }
                    }
                };

                vertex_list vertices;
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Mesh::vertices, "vertices");
                }
            };
        }

        // ===================== Material (orthogonal) =====================
        struct Density
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Density);
            JECS_DEFAULT_CONSTRUCTOR(Density);

            float value = 1.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Density::value, "value");
            }
        };
        struct Friction
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Friction);
            JECS_DEFAULT_CONSTRUCTOR(Friction);

            float value = 0.2f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Friction::value, "value");
            }
        };
        struct Restitution
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Restitution);
            JECS_DEFAULT_CONSTRUCTOR(Restitution);

            float value = 0.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Restitution::value, "value");
            }
        };
        // Tag: sensor shape — generates contacts but no collision response.
        struct IsTrigger
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(IsTrigger);
            JECS_DEFAULT_CONSTRUCTOR(IsTrigger);
        };

        // ===================== Local pose offsets (orthogonal, optional) =====================
        namespace Offset
        {
            struct Position
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Position);
                JECS_DEFAULT_CONSTRUCTOR(Position);

                math::vec2 value = {};
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Position::value, "value");
                }
            };
            struct Rotation
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Rotation);
                JECS_DEFAULT_CONSTRUCTOR(Rotation);

                float degree = 0.f;
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Rotation::degree, "degree");
                }
            };
        }

        // ===================== Output: per-frame contacts =====================
        struct CollisionResult
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(CollisionResult);

            struct Contact
            {
                // Stable identifier matching the other body's Rigidbody::body_id.
                // Safe to compare across frame boundaries (it's a value, not a pointer).
                uint64_t    other_body_id = 0;

                math::vec2  point;             // World-space contact point.
                math::vec2  normal;            // Direction from self to other.
                float       normal_impulse = 0.f;
                float       tangent_impulse = 0.f;

                // True if the other side is a sensor (IsTrigger). Such contacts
                // represent overlap only — no physical response was applied.
                // C++-only; not exposed through the woolang collide_result struct.
                bool        is_trigger = false;
            };
            // Flat array keyed by other_body_id. Linear-scan lookup; expected
            // per-entity contact count is small (<8), so this beats a map.
            basic::vector<Contact> contacts;

            CollisionResult() = default;
            CollisionResult(CollisionResult&&) = default;
            CollisionResult(const CollisionResult&) {}

            const Contact* find(const Rigidbody& body) const
            {
                for (auto& c : contacts)
                    if (c.other_body_id == body.body_id)
                        return &c;
                return nullptr;
            }
        };
    }
    namespace Light2D
    {
        struct TopDown
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(TopDown);
            JECS_DEFAULT_CONSTRUCTOR(TopDown);
        };
        struct Gain
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Gain);
            JECS_DEFAULT_CONSTRUCTOR(Gain);

            float gain = 1.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Gain::gain, "gain");
            }
        };
        struct Range
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Range);
            JECS_DEFAULT_CONSTRUCTOR(Range);

            struct light_shape
            {
                size_t m_point_count;
                basic::vector<float> m_strength;
                basic::vector<math::vec2> m_positions;
                basic::optional<jeecs::basic::resource<jeecs::graphic::vertex>> m_light_mesh;

                light_shape()
                    : m_point_count(0)
                {
                }

                static const char* JEScriptTypeName()
                {
                    return "Light2D::Range::light_shape";
                }
                static const char* JEScriptTypeDeclare()
                {
                    return
                        "namespace Light2D::Range\n"
                        "{\n"
                        "    public using light_shape = struct{\n"
                        "        public m_point_count: int,\n"
                        "        public m_strength: array<float>,\n"
                        "        public m_positions: array<vec2>,\n"
                        "    };\n"
                        "}";
                }
                void JEParseFromScriptType(woort_value v)
                {
                    woort_value s;
                    if (!woort_push_reserve(3, &s))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        const woort_value val = s + 0;
                        const woort_value strengths = s + 1;
                        const woort_value positions = s + 2;

                        woort_struct_get(val, v, 0);
                        woort_struct_get(strengths, v, 1);
                        woort_struct_get(positions, v, 2);
                        size_t position_count = (size_t)woort_int(val);

                        size_t layer_count = woort_vec_len(strengths);

                        m_point_count = position_count;
                        m_positions.clear();
                        m_strength.clear();

                        m_light_mesh.reset();

                        for (size_t ilayer = 0; ilayer < layer_count; ++ilayer)
                        {
                            float strength = 0.0f;
                            if (woort_vec_get(val, strengths, ilayer))
                                strength = woort_unbox_float(val);

                            m_strength.push_back(strength);

                            for (size_t iposition = 0; iposition < position_count; ++iposition)
                            {
                                math::vec2 pos = {};

                                if (woort_vec_get(val, positions, iposition + ilayer * position_count))
                                    pos.JEParseFromScriptType(val);

                                m_positions.push_back(pos);
                            }
                        }

                        woort_pop(3);
                    }
                }
                void JEParseToScriptType(woort_value v) const
                {
                    woort_value s;
                    if (!woort_push_reserve(2, &s))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        const woort_value val = s + 0;
                        const woort_value arr = s + 1;

                        woort_set_struct(v, 3);

                        woort_set_int(val, (woort_Int)m_point_count);
                        woort_struct_set(v, 0, val);

                        size_t layer_count = m_strength.size();

                        woort_set_vec(arr);
                        woort_vec_resize(arr, layer_count);
                        for (size_t i = 0; i < layer_count; ++i)
                        {
                            woort_set_box_float(val, m_strength.at(i));
                            (void)woort_vec_set(arr, i, val);
                        }
                        woort_struct_set(v, 1, arr);

                        woort_set_vec(arr);
                        woort_vec_resize(arr, m_positions.size());
                        for (size_t i = 0; i < m_positions.size(); ++i)
                        {
                            m_positions.at(i).JEParseToScriptType(val);
                            (void)woort_vec_set(arr, i, val);
                        }
                        woort_struct_set(v, 2, arr);

                        woort_pop(2);
                    }
                }
            };

            float decay = 1.0f;
            light_shape shape;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Range::decay, "decay");
                typing::register_member(guard, &Range::shape, "shape");
            }
        };
        struct Point
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Point);
            JECS_DEFAULT_CONSTRUCTOR(Point);

            float decay = 1.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Point::decay, "decay");
            }
        };
        struct Parallel
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Parallel);
            JECS_DEFAULT_CONSTRUCTOR(Parallel);
        };
        // 阴影计算点偏移：offset 为光源实体局部空间的偏移量（随实体
        // 旋转变换到世界空间，不受实体 local_scale 缩放影响）。挂载后
        // 光照本身（着色器内的 light_vpos 与光照范围网格）仍从光源实体
        // 原始的 Translation::world_position 渲染；而阴影投射（BlockShadow
        // / ShapeShadow / SpriteShadow / WallShadow / SelfShadow 及各
        // auto_disable 高度判定、遮挡距离剔除）改为以
        // world_position + world_rotation * offset 为原点计算。
        struct ShadowOffset
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(ShadowOffset);
            JECS_DEFAULT_CONSTRUCTOR(ShadowOffset);

            math::vec3 offset = math::vec3(0.f, 0.f, 0.f);

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &ShadowOffset::offset, "offset");
            }
        };
        // 阴影缓冲：颜色附件 R 存阴影强度（MAX 合并）；内部还带深度附件，
        // 存每个阴影贡献的逐像素 painter key（TopDown 世界 Y / 普通世界 Z），
        // 用于按俯视角前后关系仲裁 SelfShadow 与 BlockShadow。SpriteShadow
        // 只做 MAX 叠加、不写深度（其四边形是接触阴影的包围框，不应当作
        // 遮挡面参与仲裁）。分辨率由 resolution_ratio 控制（深度/颜色同尺寸）。
        struct ShadowBuffer
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(ShadowBuffer);

            float resolution_ratio = 0.5f;
            // BlockShadow 的背光挤出长度（世界单位）：阴影四边形从遮挡
            // 轮廓沿背光方向延伸的距离，默认 2000（与旧硬编码值一致）。
            float block_shadow_length = 2000.0f;
            basic::optional<basic::resource<graphic::framebuffer>> buffer;

            ShadowBuffer() = default;
            ShadowBuffer(const ShadowBuffer& another)
                : resolution_ratio(another.resolution_ratio)
                , block_shadow_length(another.block_shadow_length)
            {
            }
            ShadowBuffer(ShadowBuffer&&) = default;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &ShadowBuffer::resolution_ratio, "resolution_ratio");
                typing::register_member(guard, &ShadowBuffer::block_shadow_length, "block_shadow_length");
            }
        };
        struct CameraPostPass
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(CameraPostPass);

            basic::optional<basic::resource<graphic::framebuffer>> post_rend_target;
            basic::optional<basic::resource<jeecs::graphic::framebuffer>> post_light_target;

            float light_rend_ratio = 0.5f;

            CameraPostPass() = default;
            CameraPostPass(const CameraPostPass& another) : light_rend_ratio(another.light_rend_ratio) {}
            CameraPostPass(CameraPostPass&&) = default;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &CameraPostPass::light_rend_ratio, "light_rend_ratio");
            }
        };
        // 遮挡阴影：auto_disable = true 时按光源高度决定是否投射。
        // 普通 2D 光源按实体原点 Z 与光源 Z 比较；TopDown 光源下按实体
        // 世界包围盒底边 Y（而不是原点中心 Y）与光源 Y 比较。
        struct BlockShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(BlockShadow);
            JECS_DEFAULT_CONSTRUCTOR(BlockShadow);

            struct block_mesh
            {
                basic::vector<math::vec2> m_block_points = {
                    math::vec2(-0.5f, 0.5f),
                    math::vec2(-0.5f, -0.5f),
                    math::vec2(0.5f, -0.5f),
                    math::vec2(0.5f, 0.5f),
                    math::vec2(-0.5f, 0.5f),
                };
                basic::optional<basic::resource<graphic::vertex>> m_block_mesh;

                static const char* JEScriptTypeName()
                {
                    return "Light2D::BlockShadow::block_mesh";
                }
                static const char* JEScriptTypeDeclare()
                {
                    return
                        "namespace Light2D::BlockShadow\n"
                        "{\n"
                        "    public using block_mesh = array<vec2>;\n"
                        "}";
                }
                void JEParseFromScriptType(woort_value v)
                {
                    m_block_mesh.reset();

                    woort_value pos;
                    if (!woort_push_reserve(1, &pos))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        const size_t point_count = woort_vec_len(v);

                        m_block_points.clear();

                        for (size_t i = 0; i < point_count; ++i)
                        {
                            (void)woort_vec_get(pos, v, i);

                            math::vec2 position;
                            position.JEParseFromScriptType(pos);

                            m_block_points.push_back(position);
                        }

                        woort_pop(1);
                    }
                }
                void JEParseToScriptType(woort_value v) const
                {
                    woort_value pos;
                    if (!woort_push_reserve(1, &pos))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        woort_set_vec(v);
                        woort_vec_resize(v, m_block_points.size());
                        for (size_t i = 0; i < m_block_points.size(); ++i)
                        {
                            m_block_points.at(i).JEParseToScriptType(pos);
                            (void)woort_vec_set(v, i, pos);
                        }

                        woort_pop(1);
                    }
                }
            };

            block_mesh shape;
            float factor = 1.0f;
            bool reverse = false;
            bool auto_disable = true;
            // 内部标志：由瓦片地图分片置位。TopDown 光源下 DeferLight2D 用
            // 阴影网格顶点携带的逐段 painter key（世界 Y；瓦片轮廓取本段
            // 最低边并合并 8 邻域内的跨层墙段）参与逐像素深度排序，而不是
            // 整个实体共用一个底边 key；普通光源下仍用实体世界 Z，与同层
            // SelfShadow 的 surface key 只差固定的半层偏移，由后者覆盖掉
            // 本层遮挡落在本层表面上的自遮挡。普通实体保持 false，维持
            // 实体级语义。不注册到脚本/编辑器，由 TilemapSystem 每帧对齐。
            bool vertex_priority = false;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &BlockShadow::shape, "shape");
                typing::register_member(guard, &BlockShadow::factor, "factor");
                typing::register_member(guard, &BlockShadow::reverse, "reverse");
                typing::register_member(guard, &BlockShadow::auto_disable, "auto_disable");
            }
        };
        // 轮廓遮挡阴影：auto_disable 判定规则与 BlockShadow 相同（TopDown
        // 光源下以实体世界包围盒底边 Y 为准）
        struct ShapeShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(ShapeShadow);
            JECS_DEFAULT_CONSTRUCTOR(ShapeShadow);

            float factor = 1.0f;
            float distance = 1.0f;
            math::vec2 tiling_scale = math::vec2(1.f, 1.f);
            bool auto_disable = true;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &ShapeShadow::factor, "factor");
                typing::register_member(guard, &ShapeShadow::distance, "distance");
                typing::register_member(guard, &ShapeShadow::tiling_scale, "tiling_scale");
                typing::register_member(guard, &ShapeShadow::auto_disable, "auto_disable");
            }
        };
        // 角色接触阴影：在实体世界包围盒底边中点（脚下）生成一个椭圆，
        // 并从椭圆沿背离光源的方向拉出尾迹，尾迹随消隐距离渐隐；
        // 点光下尾迹按光源切锥自然张开，平行光下保持等宽。
        // 椭圆中心取世界空间底部（变换后的包围盒底边中点），实体旋转时
        // 仍保持在脚下，不会随局部底边一起转离地面。
        // 椭圆轴固定在视空间（长轴水平、短轴垂直，不随光向旋转），
        // 只有尾迹朝背光方向延伸；各长度均为视图空间单位
        //（正交 2D 相机下等同世界单位），与 distance 同口径。
        // 接触阴影只按 MAX 叠加强度，不参与逐像素 painter key 深度仲裁
        //（其四边形只是椭圆/尾迹的包围框，不应以脚底 key 遮挡其它阴影）。
        // 它推迟到同一光源的其它阴影贡献之后绘制，因此后方 SelfShadow
        // 接收面的覆盖写入不会擦掉它；位于其前方（key 更小）的阴影仍按
        // 深度测试正确遮挡它。
        struct SpriteShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(SpriteShadow);
            JECS_DEFAULT_CONSTRUCTOR(SpriteShadow);

            float factor = 1.0f;         // 阴影强度
            float distance = 1.0f;       // 尾迹延伸长度；<=0 时只画椭圆
            float ellipse_major = 0.5f;  // 椭圆长轴全长（视空间 X，水平）
            float ellipse_minor = 0.3f;  // 椭圆短轴全长（视空间 Y，垂直）
            float fade_distance = 1.0f;  // 尾迹消隐距离；<=0 时取 distance

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &SpriteShadow::factor, "factor");
                typing::register_member(guard, &SpriteShadow::distance, "distance");
                typing::register_member(guard, &SpriteShadow::ellipse_major, "ellipse_major");
                typing::register_member(guard, &SpriteShadow::ellipse_minor, "ellipse_minor");
                typing::register_member(guard, &SpriteShadow::fade_distance, "fade_distance");
            }
        };
        // 墙状遮挡阴影：把精灵四边形的背光锚边沿挤出方向抹出条带
        // （贴图整图压扁观感），适合墙体/箱柜等实心矩形遮挡物
        struct WallShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(WallShadow);
            JECS_DEFAULT_CONSTRUCTOR(WallShadow);

            float factor = 1.0f;
            float distance = 1.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &WallShadow::factor, "factor");
                typing::register_member(guard, &WallShadow::distance, "distance");
            }
        };
        // 自阴影覆盖：把实体自身覆盖为阴影色（factor 为覆盖强度）。
        // auto_disable = true 时按高度自动受光：
        //  * 普通 2D 光源（沿 Z 分层）下，实体原点世界 Z 不低于光源世界 Z
        //    时不追加覆盖；
        //  * TopDown 光源下，改为比较实体包围盒底边的世界 Y（取形状局部
        //    AABB 八角点变换后的最小 Y，兼容旋转），底边不低于光源高度时
        //    受光——高大实体仍以脚下位置判断，而不是以原点中心判断；
        //  * 同挂 PerBlockSelfShadow 时，TopDown 下进一步改为按网格顶点
        //    携带的块底边逐块判定（见下文 PerBlockSelfShadow）。
        // 阴影合成按逐像素 painter key 排序：SelfShadow 的 key 取实体底边
        // 减半层偏移（0.005），因此它只清除位于其后方（key 更大）的遮挡
        // 阴影；位于前方的遮挡阴影仍会覆盖它。这样“有时被阴影盖住、有时
        // 清除阴影”由俯视角前后关系决定，而不是绘制先后决定。
        struct SelfShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(SelfShadow);
            JECS_DEFAULT_CONSTRUCTOR(SelfShadow);

            float factor = 1.0f;
            bool auto_disable = true;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &SelfShadow::factor, "factor");
                typing::register_member(guard, &SelfShadow::auto_disable, "auto_disable");
            }
        };
        // 逐块自阴影修饰标记（与 Light2D::SelfShadow 同挂时生效）：
        // 标记遮挡物为分块式大网格内容（瓦片地图等）。非 TopDown 光源下
        // 与普通精灵的 SelfShadow 完全一致（接收面覆盖语义），本标记只在
        // TopDown 光源下生效：
        //  * 遮挡物不再作为整体参与 AUTO 高度判定，而是逐"块"独立判定——
        //    要求渲染网格在顶点属性位置 3 额外携带 float1 "bottom"（本块
        //    底边的局部 Y，同一块的各顶点须写同值，图元内判定才一致）。
        //    块底边变换到世界后的 Y >= 光源世界 Y 时该块受光（cover=0，
        //    MAX 下等同不写入），否则写 factor。
        //  * TopDown 的深度排序同样逐块进行：以格顶（bottom + 1）作为
        //    painter key；同一 key 的自阴影按 MAX 合并，不清除遮挡。
        struct PerBlockSelfShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(PerBlockSelfShadow);
            JECS_DEFAULT_CONSTRUCTOR(PerBlockSelfShadow);
        };
    }
    namespace Animation
    {
        struct Animator
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Animator);

            // basic::resource 删除了默认构造，Animator 的默认构造以
            // 空动画占位（加载失败时 graphic::animation::load 的同款回退），
            // 保证组件默认可用、资源始终有效；拷贝/移动构造同
            // JECS_DEFAULT_CONSTRUCTOR 显式默认（已声明的删除赋值会抑制隐式生成）。
            Animator()
                : m_animation_resource(graphic::animation::load(""))
            {
            }
            Animator(const Animator&) = default;
            Animator(Animator&&) = default;

            // 动画集绑定：指向一个 .je4animation 文件（内含多个动作），
            // 可选指定资源加载完成后自动播放的动作。
            struct animation_binding
            {
                basic::string m_path;
                basic::optional<basic::string> m_autoplay_action;

                animation_binding() = default;
                explicit animation_binding(const std::string& path)
                    : m_path(path)
                {
                }

                static const char* JEScriptTypeName()
                {
                    return "Animation::Animator::animation_binding";
                }
                static const char* JEScriptTypeDeclare()
                {
                    return
                        "namespace Animation::Animator\n"
                        "{\n"
                        "    public using animation_binding = struct{\n"
                        "        public m_path: string,\n"
                        "        public m_action: option<string>,\n"
                        "    };\n"
                        "}";
                }
                void JEParseFromScriptType(woort_value v)
                {
                    woort_value tmp;
                    if (!woort_push_reserve(1, &tmp))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        woort_struct_get(tmp, v, 0);
                        m_path = woort_string(tmp);

                        woort_struct_get(tmp, v, 1);
                        if (woort_option_get(tmp, tmp))
                            m_autoplay_action.emplace() = woort_string(tmp);
                        else
                            m_autoplay_action.reset();

                        woort_pop(1);
                    }
                }
                void JEParseToScriptType(woort_value v) const
                {
                    woort_value tmp;
                    if (!woort_push_reserve(1, &tmp))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        woort_set_struct(v, 2);

                        woort_set_string(tmp, m_path.c_str());
                        woort_struct_set(v, 0, tmp);

                        if (m_autoplay_action.has_value())
                            woort_set_option_string(tmp, m_autoplay_action.value().c_str());
                        else
                            woort_set_option_none(tmp);
                        woort_struct_set(v, 1, tmp);

                        woort_pop(1);
                    }
                }
            };

            // 运行时播放状态，由 AnimatorSystem 每帧推进，不参与序列化。
            // 状态切换逻辑（play/crossfade/stop）同时被 je_animation_entity_* C API 使用。
            struct playback_state
            {
                basic::string m_action;      // 当前动作名；空串表示未播放
                double m_time = 0.0;         // 当前动作内时间（秒）
                int m_direction = 1;         // PINGPONG 播放方向
                bool m_paused = false;
                bool m_finished = false;     // 非循环动作播放到结尾

                // crossfade 过渡目标；m_fade_duration 为 0 表示无过渡
                basic::string m_fading_action;
                double m_fading_time = 0.0;
                int m_fading_direction = 1;
                float m_fade_duration = 0.0f;
                float m_fade_elapsed = 0.0f;

                bool has_action() const
                {
                    return m_action.size() != 0;
                }
                bool is_playing() const
                {
                    return has_action() && !m_paused && !m_finished;
                }

                void play(const char* action_name)
                {
                    m_action = action_name;
                    m_time = 0.0;
                    m_direction = 1;
                    m_paused = false;
                    m_finished = false;
                    clear_fade();
                }
                void crossfade(const char* action_name, float fade_duration)
                {
                    if (!has_action() || m_finished || fade_duration <= 0.0f
                        || m_action == action_name)
                    {
                        // 没有可淡出的动作时等价于直接切换
                        play(action_name);
                        return;
                    }
                    if (m_fading_action == action_name)
                        return;

                    m_fading_action = action_name;
                    m_fading_time = 0.0;
                    m_fading_direction = 1;
                    m_fade_duration = fade_duration;
                    m_fade_elapsed = 0.0f;
                }
                void stop()
                {
                    m_action = "";
                    m_time = 0.0;
                    m_direction = 1;
                    m_paused = false;
                    m_finished = false;
                    clear_fade();
                }
                void pause()
                {
                    m_paused = true;
                }
                void resume()
                {
                    m_paused = false;
                }
                // 过渡完成：目标动作接替为当前动作。结束标志不继承旧动作，
                // 若新动作（非循环）已到终点，下一次推进时会重新置位
                void finish_fade()
                {
                    m_action = m_fading_action;
                    m_time = m_fading_time;
                    m_direction = m_fading_direction;
                    m_finished = false;
                    clear_fade();
                }
                void clear_fade()
                {
                    m_fading_action = "";
                    m_fading_time = 0.0;
                    m_fading_direction = 1;
                    m_fade_duration = 0.0f;
                    m_fade_elapsed = 0.0f;
                }
            };

            animation_binding m_animation;
            float speed = 1.0f;

            // 以下均为运行时状态，由 AnimatorSystem 维护，不参与序列化。
            // m_animation_resource 始终有效：初始为空动画占位，与空的
            // m_loaded_path 对应；绑定路径变化后由系统重新加载。
            basic::resource<graphic::animation> m_animation_resource;
            basic::string m_loaded_path;                       // 已加载资源对应的路径
            basic::optional<basic::string> m_autoplay_played;  // 已触发过的 autoplay 动作
            playback_state m_playing;
            basic::vector<basic::string> m_frame_events;       // 本帧触发的事件

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Animator::m_animation, "animation");
                typing::register_member(guard, &Animator::speed, "speed");
            }
        };
    }
    namespace Particle
    {
        /*
        Emitter [组件]
        粒子发射器：描述一类粒子的发射与生命周期行为，模拟与网格构建由
        Particle::ParticleSystem 完成（见 jeecs_core_particle_system.hpp）。
        发射器实体同时需要 Renderer::Shape / Shaders / Textures2D 三件套参与
        常规渲染管线（系统会在缺失时自动补齐并挂上内置粒子着色器）；
        粒子纹理绑定在 Renderer::Textures2D 的 pass 0，混合模式由所选着色器决定。
        角度类成员（散布角/初始自转/自转速度）以度为单位。
        m_simulation_space 为 world 时粒子在世界空间模拟（发射器移动不携带
        已发射粒子），为 local 时在发射器局部空间模拟（整体随发射器刚体
        运动）；局部空间下 m_direction 与 m_gravity 按发射器局部坐标系解释。
        m_clear_particles 置为 true 后系统会清空全部已发射粒子并把该标记复位。
        */
        struct Emitter
        {
            // 颜色渐变：随生命周期时间（0~1）插值的一组 RGBA 关键帧。
// m_times 与 m_colors 一一对应且应按时间升序排列；
// 采样时 t 会被钳制到首末关键帧覆盖的范围。
            struct color_gradient
            {
                basic::vector<float> m_times;
                basic::vector<math::color> m_colors;

                color_gradient()
                {
                    m_times.push_back(0.0f);
                    m_times.push_back(1.0f);
                    m_colors.push_back(math::color(1.0f, 1.0f, 1.0f, 1.0f));
                    m_colors.push_back(math::color(1.0f, 1.0f, 1.0f, 0.0f));
                }

                // 关键帧数量（times/colors 以较短者为准）
                size_t size() const noexcept
                {
                    return std::min(m_times.size(), m_colors.size());
                }
                // 采样：空渐变返回 fallback；t 超出范围时钳制到首/末关键帧
                math::color sample(float t, const math::color& fallback) const
                {
                    const size_t key_count = size();
                    if (key_count == 0)
                        return fallback;
                    if (key_count == 1 || t <= m_times.at(0))
                        return m_colors.at(0);
                    if (t >= m_times.at(key_count - 1))
                        return m_colors.at(key_count - 1);

                    for (size_t i = 1; i < key_count; ++i)
                    {
                        if (t <= m_times.at(i))
                        {
                            const float t0 = m_times.at(i - 1), t1 = m_times.at(i);
                            const float span = t1 - t0;
                            const float k = span > 0.0f ? (t - t0) / span : 0.0f;
                            const math::color& c0 = m_colors.at(i - 1);
                            const math::color& c1 = m_colors.at(i);
                            return math::color(
                                c0.r + (c1.r - c0.r) * k,
                                c0.g + (c1.g - c0.g) * k,
                                c0.b + (c1.b - c0.b) * k,
                                c0.a + (c1.a - c0.a) * k);
                        }
                    }
                    return m_colors.at(key_count - 1);
                }

                static const char* JEScriptTypeName()
                {
                    return "Particle::color_gradient";
                }
                static const char* JEScriptTypeDeclare()
                {
                    return
                        "namespace Particle\n"
                        "{\n"
                        "    public using color_gradient = struct{\n"
                        "        public m_times: array<float>,\n"
                        "        public m_colors: array<color>,\n"
                        "    };\n"
                        "}";
                }
                void JEParseFromScriptType(woort_value v)
                {
                    woort_value s;
                    if (!woort_push_reserve(3, &s))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        const woort_value tmp = s + 0;
                        const woort_value times = s + 1;
                        const woort_value colors = s + 2;

                        woort_struct_get(times, v, 0);
                        woort_struct_get(colors, v, 1);

                        const size_t time_count = woort_vec_len(times);
                        const size_t color_count = woort_vec_len(colors);
                        const size_t key_count = std::min(time_count, color_count);

                        m_times.clear();
                        m_colors.clear();
                        for (size_t i = 0; i < key_count; ++i)
                        {
                            if (woort_vec_get(tmp, times, i))
                                m_times.push_back(woort_unbox_float(tmp));
                            if (woort_vec_get(tmp, colors, i))
                            {
                                math::color key_color;
                                key_color.JEParseFromScriptType(tmp);
                                m_colors.push_back(key_color);
                            }
                        }

                        woort_pop(3);
                    }
                }
                void JEParseToScriptType(woort_value v) const
                {
                    woort_value s;
                    if (!woort_push_reserve(2, &s))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        const woort_value tmp = s + 0;
                        const woort_value arr = s + 1;

                        const size_t key_count = size();

                        woort_set_struct(v, 2);

                        woort_set_vec(arr);
                        woort_vec_resize(arr, key_count);
                        for (size_t i = 0; i < key_count; ++i)
                        {
                            woort_set_box_float(tmp, m_times.at(i));
                            (void)woort_vec_set(arr, i, tmp);
                        }
                        woort_struct_set(v, 0, arr);

                        woort_set_vec(arr);
                        woort_vec_resize(arr, key_count);
                        for (size_t i = 0; i < key_count; ++i)
                        {
                            m_colors.at(i).JEParseToScriptType(tmp);
                            (void)woort_vec_set(arr, i, tmp);
                        }
                        woort_struct_set(v, 1, arr);

                        woort_pop(2);
                    }
                }
            };

            // 标量曲线：随生命周期时间（0~1）插值的一组 (时间, 数值) 关键帧，
            // 用于尺寸等标量的生命周期调制；采样规则与 color_gradient 相同。
            struct scalar_curve
            {
                basic::vector<math::vec2> m_keys;

                scalar_curve()
                {
                    m_keys.push_back(math::vec2(0.0f, 1.0f));
                    m_keys.push_back(math::vec2(1.0f, 1.0f));
                }

                size_t size() const noexcept
                {
                    return m_keys.size();
                }
                float sample(float t, float fallback) const
                {
                    const size_t key_count = m_keys.size();
                    if (key_count == 0)
                        return fallback;
                    if (key_count == 1 || t <= m_keys.at(0).x)
                        return m_keys.at(0).y;
                    if (t >= m_keys.at(key_count - 1).x)
                        return m_keys.at(key_count - 1).y;

                    for (size_t i = 1; i < key_count; ++i)
                    {
                        const math::vec2& k1 = m_keys.at(i);
                        if (t <= k1.x)
                        {
                            const math::vec2& k0 = m_keys.at(i - 1);
                            const float span = k1.x - k0.x;
                            const float k = span > 0.0f ? (t - k0.x) / span : 0.0f;
                            return k0.y + (k1.y - k0.y) * k;
                        }
                    }
                    return m_keys.at(key_count - 1).y;
                }

                static const char* JEScriptTypeName()
                {
                    return "Particle::scalar_curve";
                }
                static const char* JEScriptTypeDeclare()
                {
                    return
                        "namespace Particle\n"
                        "{\n"
                        "    public using scalar_curve = struct{\n"
                        "        public m_keys: array<vec2>,\n"
                        "    };\n"
                        "}";
                }
                void JEParseFromScriptType(woort_value v)
                {
                    woort_value s;
                    if (!woort_push_reserve(2, &s))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        const woort_value tmp = s + 0;
                        const woort_value keys = s + 1;

                        woort_struct_get(keys, v, 0);

                        const size_t key_count = woort_vec_len(keys);
                        m_keys.clear();
                        for (size_t i = 0; i < key_count; ++i)
                        {
                            if (woort_vec_get(tmp, keys, i))
                            {
                                math::vec2 key;
                                key.JEParseFromScriptType(tmp);
                                m_keys.push_back(key);
                            }
                        }
                        woort_pop(2);
                    }
                }
                void JEParseToScriptType(woort_value v) const
                {
                    woort_value s;
                    if (!woort_push_reserve(2, &s))
                        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                    else
                    {
                        const woort_value tmp = s + 0;
                        const woort_value arr = s + 1;

                        woort_set_struct(v, 1);

                        woort_set_vec(arr);
                        woort_vec_resize(arr, m_keys.size());
                        for (size_t i = 0; i < m_keys.size(); ++i)
                        {
                            m_keys.at(i).JEParseToScriptType(tmp);
                            (void)woort_vec_set(arr, i, tmp);
                        }
                        woort_struct_set(v, 0, arr);

                        woort_pop(2);
                    }
                }
            };

            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Emitter);
            JECS_DEFAULT_CONSTRUCTOR(Emitter);

            // ===== 发射 =====
            // 发射形状种类：决定发射偏移的采样区域（见 emit_one 的 switch）
            enum shape_type : int32_t
            {
                point = 0,      // 点：无发射偏移
                sphere = 1,     // 球体：体积内均匀
                circle = 2,     // 圆（2D）：局部 XY 平面内
                box = 3,        // 盒：半边长 m_shape_size
            };
            shape_type m_shape = shape_type::point;
            float m_shape_radius = 0.25f;           // 球/圆 的发射半径
            math::vec3 m_shape_size = { 0.5f, 0.5f, 0.5f }; // 盒的半边长
            float m_spawn_rate = 32.0f;             // 每秒发射数量
            int m_burst_count = 0;                  // 周期爆发的数量（0=关闭）
            float m_burst_interval = 1.0f;          // 爆发间隔（秒）

            // ===== 初速与受力 =====
            float m_lifetime_min = 2.0f;            // 初始寿命区间（秒）
            float m_lifetime_max = 3.0f;
            float m_speed_min = 0.8f;               // 初始速度区间
            float m_speed_max = 1.6f;
            math::vec3 m_direction = { 0.0f, 1.0f, 0.0f }; // 主发射方向（内部归一化）
            float m_spread_angle = 25.0f;           // 围绕主方向的散布角（度，180=全球面）
            math::vec3 m_gravity = { 0.0f, -0.8f, 0.0f }; // 加速度（见模拟空间说明）
            float m_drag = 0.0f;                    // 速度阻尼（每秒指数衰减系数）

            // ===== 初始外观（在区间内均匀随机） =====
            float m_size_min = 0.08f;               // 初始尺寸区间（世界单位）
            float m_size_max = 0.16f;
            math::color m_color_min = math::color(1.0f, 1.0f, 1.0f, 1.0f);
            math::color m_color_max = math::color(1.0f, 1.0f, 1.0f, 1.0f);
            float m_rotation_min = 0.0f;            // 初始自转区间（度）
            float m_rotation_max = 0.0f;
            float m_angular_min = 0.0f;             // 自转速度区间（度/秒）
            float m_angular_max = 0.0f;

            // ===== 生命周期调制（乘算到初始值上） =====
            color_gradient m_color_over_lifetime;
            scalar_curve m_size_over_lifetime;

            // ===== 通用 =====
            int m_max_particles = 512;
            // 模拟空间：world=世界空间（发射器移动不携带已发射粒子），
            // local=发射器局部空间（整体随发射器刚体运动）
            enum simulation_space_type : int32_t
            {
                world = 0,
                local = 1,
            };
            simulation_space_type m_simulation_space = simulation_space_type::world;
            bool m_playing = true;
            bool m_clear_particles = false;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Emitter::m_shape, "shape");
                typing::register_member(guard, &Emitter::m_shape_radius, "shape_radius");
                typing::register_member(guard, &Emitter::m_shape_size, "shape_size");
                typing::register_member(guard, &Emitter::m_spawn_rate, "spawn_rate");
                typing::register_member(guard, &Emitter::m_burst_count, "burst_count");
                typing::register_member(guard, &Emitter::m_burst_interval, "burst_interval");
                typing::register_member(guard, &Emitter::m_lifetime_min, "lifetime_min");
                typing::register_member(guard, &Emitter::m_lifetime_max, "lifetime_max");
                typing::register_member(guard, &Emitter::m_speed_min, "speed_min");
                typing::register_member(guard, &Emitter::m_speed_max, "speed_max");
                typing::register_member(guard, &Emitter::m_direction, "direction");
                typing::register_member(guard, &Emitter::m_spread_angle, "spread_angle");
                typing::register_member(guard, &Emitter::m_gravity, "gravity");
                typing::register_member(guard, &Emitter::m_drag, "drag");
                typing::register_member(guard, &Emitter::m_size_min, "size_min");
                typing::register_member(guard, &Emitter::m_size_max, "size_max");
                typing::register_member(guard, &Emitter::m_color_min, "color_min");
                typing::register_member(guard, &Emitter::m_color_max, "color_max");
                typing::register_member(guard, &Emitter::m_rotation_min, "rotation_min");
                typing::register_member(guard, &Emitter::m_rotation_max, "rotation_max");
                typing::register_member(guard, &Emitter::m_angular_min, "angular_min");
                typing::register_member(guard, &Emitter::m_angular_max, "angular_max");
                typing::register_member(guard, &Emitter::m_color_over_lifetime, "color_over_lifetime");
                typing::register_member(guard, &Emitter::m_size_over_lifetime, "size_over_lifetime");
                typing::register_member(guard, &Emitter::m_max_particles, "max_particles");
                typing::register_member(guard, &Emitter::m_simulation_space, "simulation_space");
                typing::register_member(guard, &Emitter::m_playing, "playing");
                typing::register_member(guard, &Emitter::m_clear_particles, "clear_particles");
            }
        };
    }
    namespace Tilemap
    {
        /*
        Tilemap::Map [组件]
        瓦片地图实例组件：挂在地图根实体上，map_path 指向 .je4tilemap 资源
        （运行时路径，@/ 或 !/ 前缀）。地图的分块渲染子实体由
        Tilemap::TilemapSystem 自动创建与回收，根实体本身的移动/缩放
        会携带整张地图。文档读写与运行时查询见 je_tilemap_* C API，
        只读查询亦可使用 jeecs::tilemap C++ 封装。
        */
        struct Map
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Map);
            JECS_DEFAULT_CONSTRUCTOR(Map);

            basic::string map_path;

            static void JERefRegsiter(typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Map::map_path, "map_path");
            }
        };

        /*
        Tilemap::Wall [组件]
        瓦片地图碰撞体开关：挂在地图根实体上（与 Tilemap::Map 同实体）时，
        TilemapSystem 会把整张地图的不可通行格（任一图层不可通行即墙，
        与 je_tilemap_is_walkable 同一语义）贪心合并为矩形，并为每个矩形
        创建一个静态 Physics2D::Collider::Box 子实体（见 Tilemap::WallPart；
        地图版本变化时自动重建）。layerid 指定生成的碰撞体所属的物理世界
        层级，直接写入各 Rigidbody::layerid（见 Physics2D::Rigidbody）。
        block_empty 开启严格模式：没有任何图块的空格与地图外缘（四周边界
        向外一圈）同样视为不可通行，只有可通行瓦片覆盖的格子可通过。
        restitution / friction 为墙体物理材质（弹性/摩擦力系数），写入各
        碰撞体实体的 Physics2D::Restitution / Physics2D::Friction，每帧
        跟随组件同步，运行时修改即时生效。
        */
        struct Wall
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Wall);
            JECS_DEFAULT_CONSTRUCTOR(Wall);

            size_t layerid = 0;
            bool block_empty = false;
            float restitution = 0.f;
            float friction = 0.2f;

            static void JERefRegsiter(typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Wall::layerid, "layerid");
                typing::register_member(guard, &Wall::block_empty, "block_empty");
                typing::register_member(guard, &Wall::restitution, "restitution");
                typing::register_member(guard, &Wall::friction, "friction");
            }
        };

        /*
        Tilemap::WallPart [组件]
        瓦片地图碰撞体分片：由 TilemapSystem 按 Tilemap::Wall 生成的静态
        Box 碰撞体子实体持有，配对 Editor::Invisible（不显示在编辑器实体
        列表、不参与世界保存，加载后由系统重建）。字段仅作系统内部簿记
        （含按根实体比例尺同步碰撞体所需的地图局部几何），不注册反射成员。
        */
        struct WallPart
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(WallPart);
            JECS_DEFAULT_CONSTRUCTOR(WallPart);

            uint32_t owner_id = 0;   // 所属地图根实体 id（je_game_entity_t::_m_id）
            float center_x = 0.f;    // 矩形中心（地图局部空间，1 格 = 1 单位）
            float center_y = 0.f;
            float size_x = 0.f;      // 矩形尺寸（格数）
            float size_y = 0.f;
        };

        /*
        Tilemap::RenderPart [组件]
        瓦片地图渲染分片：由 TilemapSystem 创建的“图层 × 分块 × 图集”
        渲染子实体持有，配对 Editor::Invisible（不显示在编辑器实体列表、
        不参与世界保存，加载后由系统重建）。字段仅作系统内部簿记，
        不注册反射成员。
        */
        struct RenderPart
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(RenderPart);
            JECS_DEFAULT_CONSTRUCTOR(RenderPart);

            uint32_t owner_id = 0;   // 所属地图根实体 id（je_game_entity_t::_m_id）
            int32_t layer = 0;       // 图层下标
            int32_t chunk_x = 0;     // 分块列
            int32_t chunk_y = 0;     // 分块行
            int32_t source = 0;      // 图集纹理下标（Tileset 源）
        };
    }
    namespace Audio
    {
        struct Source
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Source);

            basic::resource<audio::source> source;

            float pitch;
            float volume;

            math::vec3 last_position;

            Source() noexcept
                : source(audio::source::create()), pitch(1.0f), volume(1.0f)

            {
            }
            Source(const Source& another) noexcept
                : Source()
            {
                pitch = another.pitch;
                volume = another.volume;
                last_position = another.last_position;
            }
            Source(Source&& another) noexcept
                : source(std::move(another.source))
                , pitch(another.pitch)
                , volume(another.volume)
                , last_position(another.last_position)
            {
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Source::pitch, "pitch");
                typing::register_member(guard, &Source::volume, "volume");
            }
        };
        struct Listener
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Listener);
            JECS_DEFAULT_CONSTRUCTOR(Listener);

            float volume = 1.0f;
            math::vec3 face = { 0, 0, 1 };
            math::vec3 up = { 0, 1, 0 };

            math::vec3 last_position = {};

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Listener::volume, "volume");
                typing::register_member(guard, &Listener::face, "face");
                typing::register_member(guard, &Listener::up, "up");
            }
        };
        struct Playing
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Playing);

            enum class play_state
            {
                STOPPED,
                PLAYING,
                LOOPED_PLAYING,
            };
            play_state state = play_state::STOPPED;
            basic::file_resource<audio::buffer> buffer;

            bool play = true;
            bool loop = true;

            Playing() = default;
            Playing(const Playing& another)
                : state(play_state::STOPPED)
                , buffer(another.buffer)
                , play(another.play)
                , loop(another.loop)
            {
            }
            Playing(Playing&&) = default;

            void set_buffer(const basic::resource<audio::buffer>& buf)
            {
                buffer.set_resource(buf);
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Playing::buffer, "buffer");
                typing::register_member(guard, &Playing::play, "play");
                typing::register_member(guard, &Playing::loop, "loop");
            }
        };
    }
    namespace Input
    {
        struct VirtualGamepad
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(VirtualGamepad);

            je_io_gamepad_handle_t gamepad;

            basic::map<je_keycode_t, je_gamepadcode_t> keymap;
            je_keycode_t left_stick_up_left_down_right[4];

            VirtualGamepad()
                : gamepad(je_io_gamepad_create(nullptr, nullptr))
                , left_stick_up_left_down_right{
                    JE_KEY_W,
                    JE_KEY_A,
                    JE_KEY_S,
                    JE_KEY_D }
            {
                keymap[JE_KEY_UP] = JE_GAMEPAD_UP;
                keymap[JE_KEY_DOWN] = JE_GAMEPAD_DOWN;
                keymap[JE_KEY_LEFT] = JE_GAMEPAD_LEFT;
                keymap[JE_KEY_RIGHT] = JE_GAMEPAD_RIGHT;

                keymap[JE_KEY_ENTER] = JE_GAMEPAD_START;
                keymap[JE_KEY_ESC] = JE_GAMEPAD_SELECT;
            }
            VirtualGamepad(const VirtualGamepad& another)
                : gamepad(je_io_gamepad_create(nullptr, nullptr))
                , keymap(another.keymap)
                , left_stick_up_left_down_right{
                    another.left_stick_up_left_down_right[0],
                    another.left_stick_up_left_down_right[1],
                    another.left_stick_up_left_down_right[2],
                    another.left_stick_up_left_down_right[3] }
            {
            }
            VirtualGamepad(VirtualGamepad&& another)
                : gamepad(another.gamepad)
                , keymap(std::move(another.keymap))
                , left_stick_up_left_down_right{
                    another.left_stick_up_left_down_right[0],
                    another.left_stick_up_left_down_right[1],
                    another.left_stick_up_left_down_right[2],
                    another.left_stick_up_left_down_right[3] }
            {
                another.gamepad = nullptr;
            }
            ~VirtualGamepad()
            {
                if (gamepad != nullptr)
                    je_io_gamepad_close(gamepad);
            }
        };
    }
}
#endif // __cplusplus
