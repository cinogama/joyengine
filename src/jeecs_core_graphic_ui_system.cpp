#define JE_IMPL
#include "jeecs/basic.hpp"
#include "jeecs/components.hpp"
#include "jeecs/game.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/input.hpp"
#include "jeecs/io.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_render_helpers.hpp"

namespace jeecs::graphic
{
    using namespace slice_requirement;

    namespace
    {
        // ---- 文本逐字渲染常量 ----
        // 顶点格式（与内置文本着色器的 vin 声明顺序一致）：
        // float3 位置（相机像素空间，元素矩阵已烘焙）+ float2 UV（字形图集）
        // + float4 颜色（逐字颜色 × 节点色调）。
        constexpr size_t TEXT_VERTEX_FLOATS = 9;
        // 单个动态顶点缓冲的字形容量（8192 四边形 ≈ 1.15MB 顶点 + 192KB 索引）。
        constexpr size_t MAX_TEXT_QUADS_PER_VB = 8192;
        // 顶点缓冲池上限：每个“连续同键（图集页, 着色器）文本运行段”占用一块，
        // 池跨帧复用；超限丢弃溢出文本并告警（正常 UI 场景为个位数）。
        constexpr size_t MAX_TEXT_VB_COUNT = 32;
    }

    UserInterfaceGraphicPipelineSystem::UserInterfaceGraphicPipelineSystem(game_world w)
        : GraphicPipelineBase(w)
        , m_text_vb_used(0)
        , m_warned_text_vb_exhausted(false)
        , m_text_batch_page(nullptr)
        , m_text_batch_shader(nullptr)
        , m_text_batch_vb(nullptr)
        , m_text_batch_quads(0)
    {
    }

    UserInterfaceGraphicPipelineSystem::~UserInterfaceGraphicPipelineSystem()
    {
    }

    void UserInterfaceGraphicPipelineSystem::GraphicUpdate()
    {
        begin_frame();

        // MouseOn 是逐帧派生态：帧首统一清零，随后各摄像机的 UI 阶段
        // 命中时置位（只累加不清理，见 UpdateMouseOn）。
        for (auto&& [mouse_on] : query<view typesof(MouseOn&)>())
            mouse_on.is_mouse_on = false;

        collect_cameras(m_cameras);

        // 帧首推进字形图集 LRU，并按窗口分辨率重建全部文本的逐字布局
        //（布局与相机无关，相机目标分辨率差异由发射端统一缩放）。
        m_text_renderer.begin_frame();
        PrepareTextLayouts();
        m_text_vb_used = 0;

        // 收集可渲染 UI 实体：带 Shaders 且带 Shape（常规四边形）或
        // Text（逐字文本）。父子层级已由布局阶段复合进 WorldLayout 的
        // 仿射系数通道，此处不再构建节点树、不读 Anchor/LocalToParent。
        // 2D 光源由专用查询收集，此处不再单独排除。
        m_renderers.clear();
        for (auto&& [
            elem, layout, rotation, text, texs,
            shads, shape, rendqueue, color] : query<
            view typesof(
                Element&,
                WorldLayout&,
                Rotation*,
                Text*,
                Textures2D*,
                Shaders&,
                Shape*,
                Rendqueue*,
                Color*),
            anyof typesof(Shape, Text)
            >())
        {
            m_renderers.push_back(
                ui_renderer_arch{
                    color, rendqueue, shape, &shads, texs,
                    &elem, &layout, rotation, text });
        }
        sort_by_rend_queue(m_renderers);

        end_frame();
        DrawFrame();
    }

    void UserInterfaceGraphicPipelineSystem::PrepareTextLayouts()
    {
        for (auto&& [text, elem, layout] : query<view typesof(
            Text&, Element&, WorldLayout&)>())
        {
            m_text_renderer.layout_text(
                text, elem, layout,
                (float)m_window_width, (float)m_window_height);
        }
    }

    void UserInterfaceGraphicPipelineSystem::UpdateMouseOn(
        const camera_arch& camera,
        bool render_to_window,
        size_t buffer_w, size_t buffer_h)
    {
        if (!render_to_window
            || m_window_width == 0 || m_window_height == 0
            || buffer_w == 0 || buffer_h == 0)
            return;

        const math::ivec2 mouse = jeecs::input::mousepos(0);

        // 窗口鼠标 -> 渲染目标空间 NDC（-1..1，y 向上），与
        // resolved_rect::hit_test 的 mouse_view_pos 约定一致。
        const math::vec2 mouse_view(
            ((float)mouse.x / (float)m_window_width - 0.5f) * 2.0f,
            ((float)mouse.y / (float)m_window_height - 0.5f) * -2.0f);

        if (camera.viewport != nullptr)
        {
            const math::vec4& vp = camera.viewport->viewport;
            if (vp.z <= 0.f || vp.w <= 0.f)
                return;

            // 视口以渲染目标左下角为原点；鼠标也折算到左下原点后
            // 做包含判定。
            const float x = (float)mouse.x / (float)m_window_width;
            const float y = 1.0f - (float)mouse.y / (float)m_window_height;
            if (x < vp.x || x > vp.x + vp.z
                || y < vp.y || y > vp.y + vp.w)
                return;
        }

        for (auto&& [elem, layout, mouse_on] : query<view typesof(
            Element&, WorldLayout&, MouseOn&)>())
        {
            const auto rect = elem.resolve_display_rect(
                layout, (float)buffer_w, (float)buffer_h);

            if (!rect.hit_test(
                    layout.rotation, mouse_view,
                    (float)buffer_w, (float)buffer_h))
                continue;

            mouse_on.is_mouse_on = true;
        }
    }

    void UserInterfaceGraphicPipelineSystem::DrawFrame()
    {
        const math::vec4 shader_time = current_shader_time();

        // UI uses an identity projection; the view matrix maps pixels to NDC (-1..1).
        const float MAT4_UI_UNIT[4][4] = {
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
        float MAT4_UI_MODEL[4][4];
        float MAT4_UI_MV[4][4];

        for (auto& current_camera : m_cameras)
        {
            const auto target = resolve_camera_target(current_camera);
            if (!target.has_value())
                continue;

            const size_t RENDAIMBUFFER_WIDTH = target->width,
                RENDAIMBUFFER_HEIGHT = target->height;

            // 逐摄像机更新鼠标命中状态；离屏 Framebuffer 不参与。
            UpdateMouseOn(current_camera, target->framebuffer == nullptr,
                RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

            const float MAT4_UI_VIEW[4][4] = {
                {2.0f / (float)RENDAIMBUFFER_WIDTH, 0.0f, 0.0f, 0.0f},
                {0.0f, 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 0.0f},
                {0.0f, 0.0f, 1.0f, 0.0f},
                {0.0f, 0.0f, 0.0f, 1.0f},
            };
            // For UI, vp == view (projection is identity), so view-projection == view.
            update_default_uniform_buffer(
                *current_camera.projection, MAT4_UI_VIEW, MAT4_UI_UNIT, MAT4_UI_VIEW, shader_time);

            je_gl_command_buffer_t* rend_chain = begin_camera_pass(
                current_camera, current_camera.viewport,
                target->framebuffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

            for (auto& rendentity : m_renderers)
            {
                assert(rendentity.elem != nullptr
                    && rendentity.layout != nullptr);

                // 布局阶段已把父子层级与祖先旋转链复合进 WorldLayout 的
                // 仿射系数，此处只按本相机目标尺寸像素化。
                const auto rect = rendentity.elem->resolve_display_rect(
                    *rendentity.layout,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT);

                // 总旋转角 = 布局阶段缓存的 WorldLayout::rotation（祖先累计
                // + 自身，度，quat::euler 按度消费）；祖先累计角 = 总角 −
                // 自身角。矩阵结构与未旋转时一致
                //（T(枢轴)·R·T(pivot_offset)·S 的展开形式）：uioffset 保证
                // 枢轴落在其经祖先旋转链后的显示位置 A(Q)，即
                // uioffset = D_C + (I − R(Θ_acc))·pivot_offset（D_C 为已
                // 复合显示中心 = rect.center，R 由 vec2::rotate 按度计算）。
                // 该矩阵把“单位矩形坐标”（±0.5 张成元素矩形）映射到相机
                // 像素空间，常规四边形路径（着色器侧 MVP）与逐字文本路径
                //（CPU 侧烘焙进顶点）共用同一映射。
                const float total_angle = rendentity.layout->rotation;
                const float angle_acc = total_angle
                    - (rendentity.rotation != nullptr
                        ? rendentity.rotation->angle : 0.f);

                const math::vec2 uioffset = rect.center
                    + (rect.pivot_offset
                        - rect.pivot_offset.rotate(angle_acc))
                    - math::vec2(
                        (float)RENDAIMBUFFER_WIDTH / 2.0f,
                        (float)RENDAIMBUFFER_HEIGHT / 2.0f);
                const math::vec2& uisize = rect.size;
                const math::vec2& uicenteroffset = rect.pivot_offset;

                // TODO: 这里俩矩阵实际上可以优化，但是UI实际上也没有多少，暂时直接矩阵乘法也无所谓
                // NOTE: 这里的大小和偏移大小乘二是因为一致空间是 -1 到 1，天然有一个1/2的压缩，为了保证单位正确，这里乘二
                const float MAT4_UI_OFFSET[4][4] = {
                    {1.0f, 0.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f},
                    {uioffset.x, uioffset.y, 0.0f, 1.0f},
                };
                const float MAT4_UI_SIZE[4][4] = {
                    {uisize.x, 0.0f, 0.0f, 0.0f},
                    {0.0f, uisize.y, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 0.0f, 1.0f}
                };
                float MAT4_UI_ROTATION[4][4] = {
                    {1.0f, 0.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 0.0f, 1.0f},
                };

                {
                    // 旋转绕枢轴进行（角度为 0 时 quat 即单位矩阵）。
                    const float MAT4_UI_CENTER_OFFSET[4][4] = {
                        {1.0f, 0.0f, 0.0f, 0.0f},
                        {0.0f, 1.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f, 0.0f},
                        {uicenteroffset.x, uicenteroffset.y, 0.0f, 1.0f} };
                    const float MAT4_UI_INV_CENTER_OFFSET[4][4] = {
                        {1.0f, 0.0f, 0.0f, 0.0f},
                        {0.0f, 1.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f, 0.0f},
                        {-uicenteroffset.x, -uicenteroffset.y, 0.0f, 1.0f} };
                    math::quat q(0.0f, 0.0f, total_angle);
                    q.create_matrix(MAT4_UI_ROTATION);

                    math::mat4xmat4(MAT4_UI_MV /* tmp */, MAT4_UI_ROTATION, MAT4_UI_CENTER_OFFSET);
                    math::mat4xmat4(MAT4_UI_ROTATION, MAT4_UI_INV_CENTER_OFFSET, MAT4_UI_MV /* tmp */);
                }
                math::mat4xmat4(MAT4_UI_MV /* tmp */, MAT4_UI_OFFSET, MAT4_UI_ROTATION);
                math::mat4xmat4(MAT4_UI_MODEL, MAT4_UI_MV /* tmp */, MAT4_UI_SIZE);
                math::mat4xmat4(MAT4_UI_MV, MAT4_UI_VIEW, MAT4_UI_MODEL);

                // 常规四边形路径：Shaders+Shape 齐备的实体（文本实体若
                // 同时挂有两者则先画背景板，文字叠于其上）。
                if (rendentity.shape != nullptr)
                {
                    auto bound = bind_entity_textures(rend_chain, rendentity.textures, nullptr);

                    pass_uniforms_t u;
                    u.m = MAT4_UI_MODEL;
                    // UI uses MV for both mvp and mv slots.
                    u.mv = MAT4_UI_MV;
                    u.mvp = MAT4_UI_MV;
                    u.tiling = bound.tiling;
                    u.offset = bound.offset;
                    if (rendentity.color != nullptr)
                        u.color = rendentity.color->color.to_linear();

                    draw_shader_passes(rend_chain,
                        m_defaults.shape_of(rendentity.shape->vertex),
                        m_defaults.shaders_of(*rendentity.shaders),
                        u);
                }

                // 逐字文本路径：字形四边形按（图集页, 着色器）合并成连续
                // 运行段，段满/键变化/相机切换时冲刷为一次绘制调用。
                if (rendentity.text != nullptr)
                {
                    EmitTextGlyphs(
                        rend_chain, MAT4_UI_VIEW, MAT4_UI_MODEL,
                        *rendentity.text, *rendentity.elem, rect, rendentity);
                }
            }

            // 冲刷本相机最后一个文本运行段（批状态跨相机持有视图矩阵，
            // 必须在视图离开作用域前落盘），并放弃对当前缓冲的占用——
            // 已记录的绘制动作仍引用它，下一相机必须取新缓冲，否则
            // 同帧内重复更新同一缓冲会与图形线程的滞后消费竞争。
            FlushTextBatch(rend_chain, MAT4_UI_VIEW);
            m_text_batch_vb = nullptr;
        }
    }

    void UserInterfaceGraphicPipelineSystem::EmitTextGlyphs(
        je_gl_command_buffer_t* chain,
        const float(&view)[4][4],
        const float(&model)[4][4],
        const UserInterface::Text& text,
        const UserInterface::Element& elem,
        const UserInterface::Element::resolved_rect& rect,
        const ui_renderer_arch& rendentity)
    {
        const auto& layout = text.runtime_layout;
        if (!layout.valid || layout.glyphs.size() == 0)
            return;

        if (rect.size.x <= 0.f || rect.size.y <= 0.f)
            return;

        // 字号基准轴与布局一致：width_unit 取 x，否则取 y。
        const float base_axis =
            elem.unit_kind == Element::ratio_unit::width_unit
            ? rect.size.x : rect.size.y;
        if (base_axis <= 0.f || layout.key_font_px <= 0)
            return;

        auto font_holder = text.font.get_resource();
        if (!font_holder.has_value())
            return;
        auto& font_res = font_holder.value();

        // 着色器：实体 Shaders 的第一个 pass（文本着色器，通常是编辑器指派的
        // builtin/shader/UIText.shader，也可以是自定义文本特效）；未配置或
        // 该 pass 未就绪时回退默认着色器。
        graphic::shader* shader_res = rendentity.shaders->shaders.empty()
            ? m_defaults.default_shader.get()
            : m_defaults.pass_shader_of(rendentity.shaders->shaders.front());

        // 节点色调烘焙进顶点色（批内共享一次绘制，无法逐节点传 uniform）。
        math::vec4 tint(1.f, 1.f, 1.f, 1.f);
        if (rendentity.color != nullptr)
            tint = rendentity.color->color.to_linear();

        // 文本像素 → 相机像素：布局在窗口分辨率下量化，相机目标分辨率
        // 不同时按基准轴比例统一缩放（行为与旧纹理路径等价）。
        const float s = base_axis / (float)layout.key_font_px;

        // 文本块左下角（矩形局部像素系，原点=矩形中心）与单位矩形换算。
        const math::vec2 block_origin = ui::TextRenderer::block_origin_local(
            rect, layout, text.alignment, s);
        const float inv_size_x = 1.f / rect.size.x;
        const float inv_size_y = 1.f / rect.size.y;

        for (const auto& glyph : layout.glyphs)
        {
            const auto* slot = m_text_renderer.resolve(
                font_res, glyph.raster_px, glyph.cp);
            if (slot->page == nullptr)
                continue; // 无位图字形（布局阶段已过滤，防御）

            PrepareTextBatch(
                chain, view,
                slot->page->m_texture->resource(), shader_res);
            if (m_text_batch_vb == nullptr)
                return; // 顶点缓冲池耗尽，丢弃剩余文本（已告警）

            // 字形盒（文本空间，y 向上）→ 矩形局部像素 → 单位矩形坐标
            //（MODEL 期望 ±0.5 张成元素矩形）→ 相机像素（行向量约定：
            // p' = p·M，平移位于 m[3][*]）。
            const float lx0 = block_origin.x + s * glyph.x;
            const float ly0 = block_origin.y + s * glyph.y;
            const float lx1 = lx0 + s * (float)slot->w;
            const float ly1 = ly0 + s * (float)slot->h;

            const float quad[4][2] = {
                { lx0 * inv_size_x, ly0 * inv_size_y }, // 左下
                { lx0 * inv_size_x, ly1 * inv_size_y }, // 左上
                { lx1 * inv_size_x, ly0 * inv_size_y }, // 右下
                { lx1 * inv_size_x, ly1 * inv_size_y }, // 右上
            };

            const float r = tint.x * glyph.color.x;
            const float g = tint.y * glyph.color.y;
            const float b = tint.z * glyph.color.z;
            const float a = tint.w * glyph.color.w;

            // 角点索引 0,1,2,2,1,3（与粒子系统同款四边形剖分）；
            // UV 与位置同角序（图集行序自底向上，v0 对应字形底边）。
            static constexpr size_t CORNER_UV[4][2] = {
                { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };

            float* v = &m_text_quad_data[
                m_text_batch_quads * 4 * TEXT_VERTEX_FLOATS];
            ++m_text_batch_quads;

            for (size_t corner = 0; corner < 4; ++corner, v += TEXT_VERTEX_FLOATS)
            {
                const float vx = quad[corner][0];
                const float vy = quad[corner][1];

                v[0] = vx * model[0][0] + vy * model[1][0] + model[3][0];
                v[1] = vx * model[0][1] + vy * model[1][1] + model[3][1];
                v[2] = 0.f;
                v[3] = CORNER_UV[corner][0]
                    ? slot->u1 : slot->u0;
                v[4] = CORNER_UV[corner][1]
                    ? slot->v1 : slot->v0;
                v[5] = r;
                v[6] = g;
                v[7] = b;
                v[8] = a;
            }
        }
    }

    void UserInterfaceGraphicPipelineSystem::PrepareTextBatch(
        je_gl_command_buffer_t* chain,
        const float(&view)[4][4],
        je_gl_texture_2d_t* page,
        graphic::shader* shader_res)
    {
        if (m_text_batch_page == page && m_text_batch_shader == shader_res
            && m_text_batch_vb != nullptr
            && m_text_batch_quads < MAX_TEXT_QUADS_PER_VB)
            return;

        FlushTextBatch(chain, view);

        m_text_batch_page = page;
        m_text_batch_shader = shader_res;
        m_text_batch_vb = nullptr;

        if (m_text_vb_used >= MAX_TEXT_VB_COUNT)
        {
            if (!m_warned_text_vb_exhausted)
            {
                m_warned_text_vb_exhausted = true;
                jeecs::debug::logwarn(
                    "UserInterfaceGraphicPipelineSystem: text vertex buffer "
                    "pool (%zu buffers x %zu quads) exhausted, dropping "
                    "overflowing text runs this frame.",
                    MAX_TEXT_VB_COUNT, MAX_TEXT_QUADS_PER_VB);
            }
            return;
        }

        if (m_text_vb_pool.size() <= m_text_vb_used)
            m_text_vb_pool.push_back(CreateTextVertexBuffer());
        m_text_batch_vb = &m_text_vb_pool[m_text_vb_used++];
    }

    void UserInterfaceGraphicPipelineSystem::FlushTextBatch(je_gl_command_buffer_t* chain, const float(&view)[4][4])
    {
        if (m_text_batch_quads == 0 || m_text_batch_vb == nullptr)
        {
            m_text_batch_quads = 0;
            return;
        }

        auto& vb = *m_text_batch_vb;
        vb->update_buffer(
            m_text_quad_data.data(),
            m_text_batch_quads * 4 * TEXT_VERTEX_FLOATS * sizeof(float),
            m_text_batch_quads * 6);

        je_graphic_cmd_bind_texture_2d(chain, 0, m_text_batch_page);

        auto* pipeline = pipeline_for(
            m_text_batch_shader, vb->resource());
        je_graphic_cmd_bind_pipeline(chain, pipeline);
        je_graphic_cmd_bind_vertex(chain, vb->resource());
        render_builtin_uniforms uniforms(chain, pipeline);

        static const float MAT4_IDENTITY[4][4] = {
            {1.f, 0.f, 0.f, 0.f},
            {0.f, 1.f, 0.f, 0.f},
            {0.f, 0.f, 1.f, 0.f},
            {0.f, 0.f, 0.f, 1.f},
        };

        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model, float4x4, MAT4_IDENTITY);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view, float4x4, view);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view_projection, float4x4, view);

        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_local_scale, float3, 1.f, 1.f, 1.f);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_tiling, float2, 1.f, 1.f);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_offset, float2, 0.f, 0.f);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4, 1.f, 1.f, 1.f, 1.f);

        je_graphic_cmd_draw_indexed(
            chain, (uint32_t)(m_text_batch_quads * 6), 0, 1);

        m_text_batch_quads = 0;
    }

    basic::resource<graphic::vertex> UserInterfaceGraphicPipelineSystem::CreateTextVertexBuffer()
    {
        const size_t floats_per_vb =
            MAX_TEXT_QUADS_PER_VB * 4 * TEXT_VERTEX_FLOATS;

        // 首块缓冲顺带把共享发射暂存区扩到容量（零初始化同时作为
        // 缓冲创建期的初始数据）。
        if (m_text_quad_data.size() < floats_per_vb)
            m_text_quad_data.resize(floats_per_vb, 0.f);

        std::vector<uint32_t> indices;
        indices.reserve(MAX_TEXT_QUADS_PER_VB * 6);
        for (size_t q = 0; q < MAX_TEXT_QUADS_PER_VB; ++q)
        {
            const uint32_t base = (uint32_t)(q * 4);
            for (uint32_t idx : { base + 0, base + 1, base + 2,
                base + 2, base + 1, base + 3 })
                indices.push_back(idx);
        }

        auto created = graphic::vertex::create(
            JE_GL_VERTEX_TYPE_TRIANGLES,
            m_text_quad_data.data(),
            m_text_quad_data.size() * sizeof(float),
            indices,
            {
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3 }, // vertex
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2 }, // uv
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 4 }, // color
            });

        // 创建失败即显式失败（引擎资源创建约定，见 DefaultResources）。
        assert(created.has_value());
        return std::move(created.value());
    }
}
