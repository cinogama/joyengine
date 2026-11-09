#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs.hpp"
#include "jeecs_ui_text_renderer.hpp"

#include <map>
#include <utility>

namespace jeecs::graphic
{
    using namespace Transform;
    using namespace Camera;
    using namespace Renderer;
    using namespace UserInterface;
    using namespace Light2D;

    struct DefaultResources
    {
        basic::resource<graphic::vertex> default_shape_quad;
        basic::resource<graphic::shader> default_shader;
        basic::resource<graphic::texture2d> default_texture;
        basic::vector<basic::resource<graphic::shader>> default_shaders_list;

        JECS_DISABLE_MOVE_AND_COPY(DefaultResources);

        DefaultResources();
    };

    struct BaseImpledGraphicPipeline : public graphic::BasePipelineInterface
    {
        BaseImpledGraphicPipeline(game_world w);

        struct camera_arch
        {
            je_render_branch_t* branchPipeline;

            const Rendqueue* rendqueue;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;
            const FrustumCulling* frustumCulling;
            const Camera::Clear* clear;

            bool operator<(const camera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };
        struct renderer_arch
        {
            const Renderer::Color* color;
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Shape* shape;
            const Shaders* shaders;
            const Textures2D* textures;

            // UI 布局量：父子层级（含祖先旋转链）已由布局阶段复合进
            // WorldLayout 的仿射系数通道，绘制端只按相机目标尺寸像素化。
            const UserInterface::Element* ui_elem = nullptr;
            const UserInterface::WorldLayout* ui_layout = nullptr;
            const UserInterface::Rotation* ui_rotation = nullptr; // 可空

            // UI 文本（逐字渲染路径）；仅 UI 渲染路径使用，为空时该实体走
            // 常规四边形路径（两者可并存：先四边形后文本，文字在背景板之上）。
            const UserInterface::Text* text = nullptr;

            // 纹理数组通道绑定（瓦片地图分片等使用）；可空
            const Textures2DArray* textures_array = nullptr;

            bool operator<(const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        DefaultResources m_default_resources;

        std::multiset<camera_arch> m_camera_list;
        std::multiset<renderer_arch> m_renderer_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        void PrepareCameras();

        math::vec3 get_entity_size(
            const Transform::Translation& trans,
            const basic::optional<basic::resource<graphic::vertex>>& mesh);

        /* ---------------- Shared render-pipeline helpers ---------------- */

        // Write the per-camera view/projection/VP/time quartet into the default uniform buffer.
        void update_default_uniform_buffer(
            const Projection& projection,
            const float(&view)[4][4],
            const float(&proj)[4][4],
            const float(&vp)[4][4],
            const math::vec4& shader_time);

        // Pipeline cache keyed by (shader, topology). The engine shaders carry
        // their fixed state; topology comes from the vertex resource.
        std::map<std::pair<je_gl_shader_t*, uint8_t>, je_gl_pipeline_t*> m_pipeline_cache;

        ~BaseImpledGraphicPipeline();

        je_gl_context_t* graphic_context() const;

        static je_gl_topology_t topology_of(const je_gl_vertex_t* vertex);

        je_gl_pipeline_t* pipeline_for(
            graphic::shader* shader, je_gl_vertex_t* vertex);

        // Allocate a command buffer/pass for a camera; honors optional
        // sub-viewport (normalized rect).
        je_gl_command_buffer_t* new_rend_chain_for_camera(
            je_render_branch_t* branch,
            const Viewport* viewport,
            graphic::framebuffer* rend_aim_buffer,
            size_t buffer_w, size_t buffer_h);

        // Clear attachment 0 if the camera requested a clear color.
        void clear_color_if_any(
            je_gl_command_buffer_t* cmd, const Camera::Clear* clear);

        // Bind default texture at slot 0, then any user-supplied textures.
        // Returns the resolved tiling/offset.
        struct bound_textures_t
        {
            math::vec2 tiling;
            math::vec2 offset;
        };
        bound_textures_t bind_entity_textures(je_gl_command_buffer_t* cmd, const Textures2D* textures,
            const Textures2DArray* textures_array);

        // Optional uniforms grouped together; nullptr / unset fields are skipped.
        // Used by both world-space (Unlit / DeferLight2D main) and screen-space (UI) passes.
        struct pass_uniforms_t
        {
            const float (*m)[4] = nullptr;
            const float (*mv)[4] = nullptr;
            const float (*mvp)[4] = nullptr;
            math::vec3 local_scale = math::vec3(1.f, 1.f, 1.f);
            math::vec2 tiling = math::vec2(1.f, 1.f);
            math::vec2 offset = math::vec2(0.f, 0.f);
            math::vec4 color = math::vec4(1.f, 1.f, 1.f, 1.f);
            // Light2D-only extras; set when non-null.
            const math::vec2* light2d_resolution = nullptr;
            const float* light2d_decay = nullptr;
        };

        // Iterate shader passes of `shaders`, falling back to default shader if a pass is not builtin,
        // and upload the standard set of builtin uniforms.
        void draw_shader_passes(
            je_gl_command_buffer_t* cmd,
            const basic::resource<graphic::vertex>& shape,
            const basic::vector<basic::resource<graphic::shader>>& shaders,
            const pass_uniforms_t& u);

        // Draw every entity in m_renderer_list using the standard world-space pipeline
        // (frustum culling -> MV/MVP -> bind_entity_textures -> draw_shader_passes).
        // Shared between UnlitGraphicPipelineSystem and DeferLight2DGraphicPipelineSystem.
        // Takes the frustum-culling handle directly so it works with any camera archetype
        // (camera_arch / l2dcamera_arch / ...) that exposes a `frustumCulling` field.
        void draw_world_renderers(
            je_gl_command_buffer_t* cmd,
            const FrustumCulling* frustum_culling,
            const float(&view)[4][4],
            const float(&vp)[4][4]);
    };

    struct UserInterfaceGraphicPipelineSystem : public BaseImpledGraphicPipeline
    {
        UserInterfaceGraphicPipelineSystem(game_world w);
        ~UserInterfaceGraphicPipelineSystem();

        // ---- 文本逐字渲染运行时 ----
        // 字形图集 + 行网格布局引擎（LRU、脏检查细节见 jeecs_ui_text_renderer.hpp）。
        ui_text_renderer m_text_renderer;

        // 动态顶点缓冲池：跨帧复用；同一帧内每个运行段独占一块（command buffer
        // 记录绘制动作后图形线程滞后消费，同帧内重复更新同一缓冲会互相覆盖）。
        std::vector<basic::resource<graphic::vertex>> m_text_vb_pool;
        size_t m_text_vb_used = 0;
        bool m_warned_text_vb_exhausted = false;
        // 当前批（连续同键运行段）状态；发射端逐字追加，键变化/缓冲满/相机
        // 切换时 flush 成一次 command-buffer draw。
        std::vector<float> m_text_quad_data;
        je_gl_texture_2d_t* m_text_batch_page = nullptr;
        graphic::shader* m_text_batch_shader = nullptr;
        basic::resource<graphic::vertex>* m_text_batch_vb = nullptr;
        size_t m_text_batch_quads = 0;

        void GraphicUpdate();

        // 按窗口分辨率重建全部 UI 文本的逐字布局（标记解析 → 字形度量 →
        // 折行 → 对齐 → 放置），结果缓存于 Text::runtime_layout，仅脏文本
        // 重建。实际像素字号 = 元素按窗口尺寸解析后的最终布局尺寸（基准轴：
        // width_unit 取 x 分量，否则取 y 分量——per_axis 亦取 y，文字以字高
        // 为基准），并量化为整像素以获得稳定的字形缓存命中；相机目标分辨率
        // 的差异由发射端按基准轴比例统一缩放。字号取局部（未旋转）矩形，
        // 不回写任何布局输入组件。
        void PrepareTextLayouts();

        // 逐摄像机更新 MouseOn（UI 命中测试）。UI 元素矩形按本摄像机目标
        // 尺寸像素化后做含旋转的命中测试，因此必须逐摄像机执行：
        //   * render_to_window 为 false 时不处理——渲染到 Framebuffer 的
        //     摄像机是离屏目标，窗口鼠标不在其空间中；
        //   * 鼠标须落在摄像机视口（Camera::Viewport，归一化、左下原点）
        //     内；视口只裁剪绘制，不改变元素矩形（仍按目标完整尺寸解析）；
        //   * 命中只置位，不清理其他摄像机已写入的结果：帧首已统一清零，
        //     多个摄像机命中同一元素时保持 true。
        // 不参与渲染的元素（无 Shape/Text/Shaders）同样参与测试，可作为
        // 不可见按钮热区。
        void UpdateMouseOn(
            const camera_arch& camera,
            bool render_to_window,
            size_t buffer_w, size_t buffer_h);

        void DrawFrame();

        // ---- 文本逐字批处理 ----

        // 发射一个文本节点的全部字形四边形。字形放置来自布局缓存（文本
        // 空间，窗口分辨率），经 s（相机基准轴/量化字号）统一缩放、按
        // alignment 九宫格放置进元素矩形（矩形局部像素 → 单位矩形坐标），
        // 再经与四边形路径完全相同的元素矩阵烘焙成相机像素空间顶点；
        // 颜色 = 逐字标记颜色 × 节点 Renderer::Color 色调。
        void EmitTextGlyphs(
            je_gl_command_buffer_t* chain,
            const float(&view)[4][4],
            const float(&model)[4][4],
            const UserInterface::Text& text,
            const UserInterface::Element& elem,
            const UserInterface::Element::resolved_rect& rect,
            const renderer_arch& rendentity);

        // 批维护：键（图集页, 着色器）变化或当前缓冲写满时冲刷旧批、
        // 从池中取下一块顶点缓冲。
        void PrepareTextBatch(
            je_gl_command_buffer_t* chain,
            const float(&view)[4][4],
            je_gl_texture_2d_t* page,
            graphic::shader* shader_res);

        // 冲刷当前批：一次 je_graphic_vertex_buffer_update + 一次 command-buffer draw。
        // 顶点已烘焙至相机像素空间，M=I、MVP/MV=UI 视图矩阵。
        void FlushTextBatch(je_gl_command_buffer_t* chain, const float(&view)[4][4]);

        // 创建一块文本动态顶点缓冲：静态角点索引一次分配，顶点数据逐帧
        // 整体重写并收缩激活索引数量（与粒子系统同款机制，见
        // je_graphic_vertex_buffer_update）。
        basic::resource<graphic::vertex> CreateTextVertexBuffer();
    };

    struct UnlitGraphicPipelineSystem : public BaseImpledGraphicPipeline
    {
        UnlitGraphicPipelineSystem(game_world w);
        ~UnlitGraphicPipelineSystem();

        void GraphicUpdate();
        void DrawFrame();
    };

    struct DeferLight2DGraphicPipelineSystem : public BaseImpledGraphicPipeline
    {
        struct DeferLight2DResource
        {
            JECS_DISABLE_MOVE_AND_COPY(DeferLight2DResource);

            // Used for move rend result to camera's render aim buffer.
            jeecs::basic::resource<jeecs::graphic::texture2d> _no_shadow;
            jeecs::basic::resource<jeecs::graphic::vertex> _screen_vertex;

            jeecs::basic::resource<jeecs::graphic::vertex> _wall_shadow_vertex;

            // SpriteShadow（角色接触阴影）用单位四边形：顶点 ±1，
            // 顶点着色器把它映射到脚底椭圆 + 背光尾迹的包围框
            jeecs::basic::resource<jeecs::graphic::vertex> _sprite_shadow_quad;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_point_reverse_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_parallel_reverse_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sprite_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sprite_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_wall_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_wall_parallel_pass;

            // 遮挡/自阴影 pass 写逐像素 painter key 深度，并以
            // ZTEST(LESS_EQUAL) / ZWRITE(ENABLE) 让更靠前的贡献胜出；
            // BLEND(MAX) 只负责同一 key 内的阴影叠加（不再用于跨层仲裁）。
            // 例外：SpriteShadow 的四边形只是椭圆/尾迹的包围框，片元里才
            // 裁出实际覆盖，因此它不写深度（ZWRITE DISABLE），只 MAX 叠加
            // 强度，避免包围框内无阴影的空洞以脚底 key 擦除其它阴影。
            // 它在同一光源的其余贡献之后统一绘制，否则自阴影接收面的
            // 覆盖写入会把位于其后方（key 更大）的接触阴影一并擦掉。

            // 自阴影接收面 pass ×2（普通精灵与非 TopDown 瓦片分片共用）：
            // 以半层偏移的 surface key + 覆盖写入参与 painter key 仲裁，
            // 覆盖位于其后方（key 更大）的阴影、保留前方阴影；同一图层的
            // BlockShadow 也因 key 稍后而被本 pass 清掉（自遮挡消除）。
            // 数组纹理采样版：瓦片地图分片把图集绑在 Textures2DArray
            // 槽 0（顶点 uv 为 (u, v, 数组层号) float3），按层采样图集
            // alpha 做自阴影覆盖
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_array_pass;

            // 逐块判定版 sub pass ×2（TopDown 光源 + Light2D::PerBlockSelfShadow
            // 遮挡物专用）：AUTO 高度判定不在 CPU 按实体原点一刀切，而是用
            // 网格顶点属性 3 携带的块底边（局部 Y）在顶点着色器逐块比较，
            // 整块受光或整块自阴影。JE_COLOR 约定：x=factor，
            // y=高度线（AUTO 为光源世界 Y；ON 恒暗时为 +1e30），z/w 备用。
            // 仅分块大网格使用，按 MAX 合并（逐块 key 与墙段 key 不可直接
            // 比较，只加深、不清除遮挡阴影）
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_topdown_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_topdown_array_pass;

            DeferLight2DResource();
        };

        DeferLight2DResource m_defer_light2d_host;

        struct l2dcamera_arch
        {
            je_render_branch_t* branchPipeline;

            const Rendqueue* rendqueue;
            const Translation* translation;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;
            const Light2D::CameraPostPass* light2DPostPass;
            const Shaders* shaders;
            const Textures2D* textures;
            const FrustumCulling* frustumCulling;
            const Camera::Clear* clear;
            const Renderer::Color* color;

            bool operator<(const l2dcamera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        struct light2d_arch
        {
            const Translation* translation;
            const Light2D::TopDown* topdown;
            const Light2D::Point* point;
            const Light2D::Parallel* parallel;
            const Light2D::Range* range;
            // 阴影计算点偏移（可选）：阴影投射与高度判定改用
            // world_position + world_rotation * offset 为原点；
            // 光照本身仍从 translation->world_position 渲染
            const Light2D::ShadowOffset* shadowoffset;
            const Light2D::Gain* gain;
            const Light2D::ShadowBuffer* shadowbuffer;
            const Renderer::Color* color;
            const Shape* shape;
            const Shaders* shaders;
            const Textures2D* textures;
        };

        struct block2d_arch
        {
            const Translation* translation;
            // 实体世界包围盒底边中点（.y 即底部 Y）：构建列表时按形状局部
            // AABB 八角点变换计算一次，供 TopDown painter key、各阴影的
            // auto_disable 判定与 SpriteShadow 椭圆中心复用
            math::vec3 bottom_world_center;
            // 同一包围盒的顶边中点（.y 即顶部 Y）：仅用于 TopDown 阴影
            // 逐像素深度映射的 key 范围，保证逐块/逐边 key 都落在范围内
            math::vec3 top_world_center;

            const Light2D::BlockShadow* blockshadow;
            const Light2D::ShapeShadow* shapeshadow;
            const Light2D::SpriteShadow* spriteshadow;
            const Light2D::WallShadow* wallshadow;
            const Light2D::SelfShadow* selfshadow;
            // 逐块自阴影修饰（与 SelfShadow 同挂）：TopDown 光源下 AUTO
            // 高度判定用网格顶点携带的块底边逐块比较，见 sub_topdown pass
            const Light2D::PerBlockSelfShadow* perblock;

            const Textures2D* textures;
            // 数组图集（瓦片地图分片）：槽 0 为图集，sub pass 用它按层采样
            const Textures2DArray* textures_array;
            const Shape* shape;
        };

        std::multiset<l2dcamera_arch> m_2dcamera_list;
        std::vector<light2d_arch> m_2dlight_list;

        // 阴影遮挡物列表：构建/遍历顺序不再参与遮挡仲裁，逐像素前后
        // 关系由 shadow buffer 的 painter key 深度测试决定。
        std::vector<block2d_arch> m_2dblock_list;

        DeferLight2DGraphicPipelineSystem(game_world w);
        ~DeferLight2DGraphicPipelineSystem();

        void GraphicUpdate();

        /* ------- DeferLight2D-specific helpers (use the base helpers too) ------- */

        // Pick the shadow shader set based on light type (point/parallel).
        struct defer_light2d_shadow_passes_t
        {
            const basic::resource<graphic::shader>& normal;
            const basic::resource<graphic::shader>& reverse_normal;
            const basic::resource<graphic::shader>& shape;
            const basic::resource<graphic::shader>& sprite;
            const basic::resource<graphic::shader>& wall;
            // 自阴影接收面 pass：普通精灵与非 TopDown 瓦片分片共用
            const basic::resource<graphic::shader>& sub;
            const basic::resource<graphic::shader>& sub_texarray;
        };
        defer_light2d_shadow_passes_t select_shadow_passes(bool is_parallel) const;

        void bind_shadow_texture(
            je_gl_command_buffer_t* chain, /* OPTIONAL */ const Textures2D* textures);

        // 实体世界包围盒底边中点：取形状局部 AABB 的 8 个角点经
        // object2world 变换后，X/Z 取中值、Y 取最小值。兼容旋转与非居中
        // 轴心——实体旋转后该点仍在世界空间底部（脚下）。
        // 结果缓存进 block2d_arch::bottom_world_center；TopDown painter key、
        // 各阴影的 auto_disable 与 SpriteShadow 椭圆中心均复用它。
        // top_out 返回同一 AABB 的顶边中点（.y 为最大值），用于 TopDown
        // 阴影深度映射的 key 上界（逐块/逐边 key 可能高于实体底边）。
        // 挂 PerBlockSelfShadow 的分块大网格走顶点属性逐块判定
        //（见 sub_topdown pass），不经过这里。
        math::vec3 get_block_bottom_world_center(
            const Translation& translation, const Shape& shape,
            /* OPTIONAL */ math::vec3* top_out = nullptr) const;

        // 阴影逐像素仲裁的 key→NDC 深度线性映射：TopDown 用世界 Y，
        // 普通 2D 用世界 Z；key 越小越靠前，映射到 [0.05, 0.95] 后由
        // ZTEST(LESS_EQUAL) 保留最靠前的阴影贡献。
        struct shadow_depth_mapping
        {
            float scale = 1.f;
            float bias = 0.f;
        };
        shadow_depth_mapping make_shadow_depth_mapping(
            const std::vector<block2d_arch>& blocks, bool topdown) const;

        // Unified draw call for the 3 Light2D shadow occluder types (block / shape / sprite).
        // They all compute MV/MVP from the occluder's object2world and pack shadow params
        // into local_scale + color. All occluders use the same parallel light dir ref
        // (0,-1,0). shadow_cast_pos is the point-light shadow origin
        //（world_position + world_rotation * ShadowOffset，见 DrawFrame）.
        void draw_shadow_occluder(
            je_gl_command_buffer_t* shadow_chain,
            const basic::resource<graphic::shader>& shadow_shader,
            const basic::resource<graphic::vertex>& shape_vertex,
            const Translation* occluder_translation,
            const light2d_arch& light,
            const math::vec3& shadow_cast_pos,
            const float(&view)[4][4],
            const float(&vp)[4][4],
            math::vec3 local_scale,
            float color_w,
            const math::vec3& parallel_dir_ref,
            const Textures2D* textures,
            const math::vec4& shadow_key,
            /* OPTIONAL */ const math::vec4* color_override = nullptr,
            /* OPTIONAL */ const math::vec4* ellipse_params = nullptr);

        // 自阴影接收面绘制（普通精灵与非 TopDown 瓦片分片共用）：
        //  * 普通光源：key = 实体/图层世界 Z - 半层偏移，ZTEST(LESS_EQUAL)
        //    + 覆盖写入。自阴影就是“接收面”本身——位于其后的遮挡阴影
        //    （key 更大）被覆盖，位于其前的遮挡阴影（key 更小）保留；
        //    同一图层的 BlockShadow 也因此被清除（自遮挡）。
        //  * TopDown + PerBlockSelfShadow：按块底边逐块判高，key 取块顶
        //    （顶点着色器计算），MAX 合并；逐块 key 与墙段 key 不可直接
        //    比较，故只加深、不清除。
        void draw_self_shadow(
            je_gl_command_buffer_t* shadow_chain,
            const defer_light2d_shadow_passes_t& passes,
            const block2d_arch& block,
            bool topdown_light,
            const shadow_depth_mapping& depth_mapping,
            const math::vec3& shadow_cast_pos,
            const float(&view)[4][4],
            const float(&vp)[4][4],
            float shadow_key_mode);

        // SpriteShadow（角色接触阴影）绘制：在实体世界包围盒底边中点
        //（脚下）画椭圆，并沿背光方向拉出尾迹。中心取世界空间底部，
        // 旋转实体不会带着椭圆一起转离地面。
        // local_scale.x 传强度，.yz 传 CPU 预计算的世界空间背光方向
        //（着色器用 JE_V 转到视空间，避免实体非等比缩放扭曲方向）；
        // JE_COLOR.xy 传椭圆中心（世界空间），.z 传阴影计算点到实体的
        // 平面距离（点光用于切线张开率，平行光不用），.w 传中心世界 Z；
        // JE_SHADOW_ELLIPSE 传轴长/延伸/消隐距离。点光方向取"阴影计算点
        // →脚底"（而非→实体中心），尾迹从接触点向外延伸；平行光方向由
        // 光源自身朝向决定，与实体位置无关。
        void draw_sprite_shadow(
            je_gl_command_buffer_t* shadow_chain,
            const defer_light2d_shadow_passes_t& passes,
            const block2d_arch& block,
            const light2d_arch& light,
            const math::vec3& shadow_cast_pos,
            const float(&view)[4][4],
            const float(&vp)[4][4],
            const math::vec4& shadow_key);

        void DrawFrame();
    };
}
