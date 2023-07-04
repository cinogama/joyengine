#pragma once

// Important:
// Graphic is a job, not a system.

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <queue>
#include <list>

#define JE_MAX_LIGHT2D_COUNT 16
#define JE_SHADOW2D_0 24

#define JE_LIGHT2D_DEFER_0 16

const char* shader_light2d_path = "je/shader/light2d.wo";
const char* shader_light2d_src = R"(
// JoyEngineECS RScene shader for light2d-system
// This script only used for forward light2d pipeline.

import je.shader;

public let MAX_SHADOW_LIGHT_COUNT = 16;
let SHADOW2D_TEX_0 = 24;
let DEFER_TEX_0 = 16;

// define struct for Light
GRAPHIC_STRUCT! Light2D
{
    color:      float4,  // color->xyz is color, color->w is intensity.
    position:   float4,  // position->xyz used for point-light
                         // position->w is range that calced by local-scale & shape size.
    direction:  float4,  // direction->xyz used for parallel-light
                         // direction->w is reserved.
    factors:    float4,  // factors->x & y used for effect position or parallel
                         // factors->z is decay, normally, parallel light's decay should be 0
                         // factors->w is reserved.
};

UNIFORM_BUFFER! JOYENGINE_LIGHT2D = 1
{
    // Append nothing here, add them later.
};

// Create lights for JOYENGINE_LIGHT2D
public let je_light2ds = func(){
    let lights = []mut: vec<Light2D_t>;

    for (let mut i = 0;i < MAX_SHADOW_LIGHT_COUNT; i += 1)
        lights->add(JOYENGINE_LIGHT2D->append_struct_uniform(F"JE_LIGHT2D_{i}", Light2D): gchandle: Light2D_t);

    return lights->toarray;
}();

public let je_shadow2ds = func(){
    let shadows = []mut: vec<texture2d>;

    for (let mut i = 0;i < MAX_SHADOW_LIGHT_COUNT; i += 1)
        shadows->add(uniform_texture:<texture2d>(F"JOYENGINE_SHADOW2D_{i}", SHADOW2D_TEX_0 + i));

    return shadows->toarray;
}();


public let je_light2d_resolutin = uniform("JOYENGINE_LIGHT2D_RESOLUTION", float2::one);
public let je_light2d_decay = uniform("JOYENGINE_LIGHT2D_DECAY", float::one);

public let je_light2d_defer_albedo = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_Albedo", DEFER_TEX_0 + 0);
public let je_light2d_defer_self_luminescence = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_SelfLuminescence", DEFER_TEX_0 + 1);
public let je_light2d_defer_visual_coord = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_VisualCoordinates", DEFER_TEX_0 + 2);
public let je_light2d_defer_shadow = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_Shadow", DEFER_TEX_0 + 3);
)";

const char* shader_pbr_path = "je/shader/pbr.wo";
const char* shader_pbr_src = R"(
// JoyEngineECS RScene shader tools.
// This script only used for defer light-pbr.

import je.shader;

public let PI = float::new(3.1415926535897932384626);

public func DistributionGGX(N: float3, H: float3, roughness: float)
{
    let a = roughness * roughness;
    let a2 = a * a;
    let NdotH = max(dot(N, H), float::zero);
    let NdotH2 = NdotH * NdotH;
    
    let nom = a2;
    let denom = NdotH2 * (a2 - float::one) + float::one;
    let pidenom2 = PI * denom * denom;

    return nom / pidenom2;
}

public func GeometrySchlickGGX(NdotV: float, roughness: float)
{
    let r = roughness + float::one;
    let k = r * r / float::new(8.);
    
    let nom = NdotV;
    let denom = NdotV * (float::one - k) + k;

    return nom / denom;
}

public func GeometrySmith(N: float3, V: float3, L: float3, roughness: float)
{
    let NdotV = max(dot(N, V), float::zero);
    let NdotL = max(dot(N, L), float::zero);

    let ggx1 = GeometrySchlickGGX(NdotL, roughness);
    let ggx2 = GeometrySchlickGGX(NdotV, roughness);

    return ggx1 * ggx2;
}

public func FresnelSchlick(cosTheta: float, F0: float3)
{
    return F0 + (float3::one - F0) * pow(float::one - cosTheta, float::new(5.));
}

public func multi_sampling_for_bias_shadow(shadow: texture2d, reso: float2, uv: float2)
{
    let mut shadow_factor = float::zero;
    let bias = 2.;

    let bias_weight = [
        (0., 0., 1.)
        //(-2., 2., 0.08),    (-1., 2., 0.08),    (0., 2., 0.08),     (1., 2., 0.08),     (2., 2., 0.08),
        //(-2., 1., 0.08),    (-1., 1., 0.08),    (0., 1., 0.16),     (1., 1., 0.16),     (2., 1., 0.08),
        //(-2., 0., 0.08),    (-1., 0., 0.08),    (0., 0., 0.72),     (1., 0., 0.16),     (2., 0., 0.08),
        //(-2., -1., 0.08),   (-1., -1., 0.16),   (0., -1., 0.16),    (1., -1., 0.16),    (2., -1., 0.08),
        //(-2., -2., 0.08),   (-1., -2., 0.08),   (0., -2., 0.08),    (1., -2., 0.08),    (2., -2., 0.08),
    ];

    let reso_inv = float2::one / reso;

    for (let _, (x, y, weight) : bias_weight)
    {
        shadow_factor = shadow_factor + texture(
            shadow, uv + reso_inv * float2::create(x, y) * bias
        )->x * weight;
        
    }
    return clamp(shadow_factor, 0., 1.);
}

)";

namespace jeecs
{
#define JE_CHECK_NEED_AND_SET_UNIFORM(ACTION, UNIFORM, ITEM, TYPE, ...) \
do{if (UNIFORM->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
    jegl_rchain_set_builtin_uniform_##TYPE(ACTION, &UNIFORM->m_builtin_uniform_##ITEM, __VA_ARGS__);}while(0)

    struct rendchain_branch_pipeline
    {
        JECS_DISABLE_MOVE_AND_COPY(rendchain_branch_pipeline);

        std::vector<jegl_rendchain*> m_allocated_chains;
        size_t m_allocated_chains_count;
        int    m_priority;

        rendchain_branch_pipeline()
            : m_allocated_chains_count(0)
            , m_priority(0)
        {
        }
        ~rendchain_branch_pipeline()
        {
            for (auto* chain : m_allocated_chains)
                jegl_rchain_close(chain);
        }

        void new_frame(int priority)
        {
            m_priority = priority;
            m_allocated_chains_count = 0;
        }
        jegl_rendchain* allocate_new_chain(jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
        {
            if (m_allocated_chains_count >= m_allocated_chains.size())
            {
                assert(m_allocated_chains_count == m_allocated_chains.size());
                m_allocated_chains.push_back(jegl_rchain_create());
            }
            auto* rchain = m_allocated_chains[m_allocated_chains_count];
            jegl_rchain_begin(rchain, framebuffer, x, y, w, h);
            ++m_allocated_chains_count;
            return rchain;
        }
        void commit_frame(jegl_thread* thread)
        {
            for (size_t i = 0; i < m_allocated_chains_count; ++i)
                jegl_rchain_commit(m_allocated_chains[i], thread);
        }
    };

    struct GraphicThreadHost
    {
        JECS_DISABLE_MOVE_AND_COPY(GraphicThreadHost);

        jegl_thread* glthread = nullptr;
        jeecs::game_universe universe;
        basic::resource<graphic::vertex> default_shape_quad;
        basic::resource<graphic::shader> default_shader;
        basic::resource<graphic::texture> default_texture;
        jeecs::vector<basic::resource<graphic::shader>> default_shaders_list;

        static double _update_frame_universe_job(void* host)
        {
            auto* graphic_host = (GraphicThreadHost*)host;
            if (!jegl_update(graphic_host->glthread))
            {
                graphic_host->universe.stop();
            }

            return 1.0 / (double)graphic_host->glthread->m_config.m_fps;
        }

        std::mutex m_rendchain_branchs_mx;
        std::vector<rendchain_branch_pipeline*> m_rendchain_branchs;

        rendchain_branch_pipeline* alloc_pipeline()
        {
            rendchain_branch_pipeline* pipe = new rendchain_branch_pipeline();

            std::lock_guard g1(m_rendchain_branchs_mx);
            m_rendchain_branchs.push_back(pipe);

            return pipe;
        }

        void free_pipeline(rendchain_branch_pipeline* pipe)
        {
            std::lock_guard g1(m_rendchain_branchs_mx);
            auto fnd = std::find(m_rendchain_branchs.begin(), m_rendchain_branchs.end(), pipe);
            assert(fnd != m_rendchain_branchs.end());
            m_rendchain_branchs.erase(fnd);

            delete pipe;
        }

        void _frame_rend_impl()
        {
            // Clear frame buffer
            jegl_clear_framebuffer(nullptr);

            std::lock_guard g1(m_rendchain_branchs_mx);

            std::stable_sort(m_rendchain_branchs.begin(), m_rendchain_branchs.end(),
                [](rendchain_branch_pipeline* a, rendchain_branch_pipeline* b)
                {
                    return a->m_priority < b->m_priority;
                });

            for (auto* gpipe : m_rendchain_branchs)
                gpipe->commit_frame(glthread);

            size_t WINDOWS_WIDTH, WINDOWS_HEIGHT;
            jegl_get_windows_size(&WINDOWS_WIDTH, &WINDOWS_HEIGHT);

            jegl_rend_to_framebuffer(nullptr, 0, 0, WINDOWS_WIDTH, WINDOWS_HEIGHT);
        }

        GraphicThreadHost(jeecs::game_universe _universe)
            : universe(_universe)
        {
            default_shape_quad =
                new graphic::vertex(jegl_vertex::TRIANGLESTRIP,
                    {
                        -0.5f, 0.5f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                        -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                        0.5f, 0.5f, 0.0f,       1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                        0.5f, -0.5f, 0.0f,      1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                    },
                    { 3, 2, 3 });

            default_texture = graphic::texture::create(2, 2, jegl_texture::texture_format::RGBA);
            default_texture->pix(0, 0).set({ 1.f, 0.25f, 1.f, 1.f });
            default_texture->pix(1, 1).set({ 1.f, 0.25f, 1.f, 1.f });
            default_texture->pix(0, 1).set({ 0.25f, 0.25f, 0.25f, 1.f });
            default_texture->pix(1, 0).set({ 0.25f, 0.25f, 0.25f, 1.f });
            default_texture->resouce()->m_raw_texture_data->m_sampling = jegl_texture::texture_sampling::NEAREST;
            
            default_shader = graphic::shader::create("!/builtin/builtin_default.shader", R"(
// Default shader
import je.shader;

VAO_STRUCT! vin {
    vertex : float3,
};
using v2f = struct {
    pos : float4,
};
using fout = struct {
    color : float4
};

public let vert = 
\v: vin = v2f{ pos = je_mvp * vertex_pos }
    where vertex_pos = float4::create(v.vertex, 1.);;

public let frag = 
\f: v2f = fout{ color = float4::create(t, 0., t, 1.) }
    where t = je_time->y();;

)");
            default_shaders_list.push_back(default_shader);

            jegl_interface_config config = {};
            config.m_fps = 60;
            config.m_windows_width = 640;
            config.m_windows_height = 480;
            config.m_resolution_x = 640;
            config.m_resolution_y = 480;
            config.m_title = "JoyEngineECS(JoyEngine 4.0)";
            config.m_fullscreen = false;
            config.m_enable_resize = true;
            
            glthread = jegl_start_graphic_thread(
                config,
                jegl_using_opengl3_apis,
                [](void* ptr, jegl_thread* glthread)
                {
                    ((GraphicThreadHost*)ptr)->_frame_rend_impl();
                }, this);

            je_ecs_universe_register_pre_call_once_job(universe.handle(), _update_frame_universe_job, this, nullptr);
        }

        ~GraphicThreadHost()
        {
            assert(this == _m_instance);
            _m_instance = nullptr;
            if (glthread)
                jegl_terminate_graphic_thread(glthread);

            je_ecs_universe_unregister_pre_call_once_job(universe.handle(), _update_frame_universe_job);
        }

        inline static std::atomic<GraphicThreadHost*> _m_instance = nullptr;

        static GraphicThreadHost* get_default_graphic_pipeline_instance(game_universe universe)
        {
            if (nullptr == _m_instance)
            {
                _m_instance = new GraphicThreadHost(universe);
                je_ecs_universe_register_exit_callback(universe.handle(),
                    [](void* instance)
                    {
                        delete (GraphicThreadHost*)instance;
                    },
                    _m_instance);
            }

            assert(_m_instance);
            return _m_instance;
        }
    };

    struct GraphicPipelineBaseSystem : game_system
    {
        GraphicThreadHost* _m_pipeline;

        std::vector<rendchain_branch_pipeline*> _m_rchain_pipeline;
        size_t _m_this_frame_allocate_rchain_pipeline_count;


        GraphicPipelineBaseSystem(game_world w)
            : game_system(w)
            , _m_pipeline(GraphicThreadHost::get_default_graphic_pipeline_instance(w.get_universe()))
            , _m_this_frame_allocate_rchain_pipeline_count(0)
        {
        }
        ~GraphicPipelineBaseSystem()
        {
            for (auto* branch : _m_rchain_pipeline)
                _m_pipeline->free_pipeline(branch);

            _m_rchain_pipeline.clear();
            _m_this_frame_allocate_rchain_pipeline_count = 0;
        }

        void pipeline_update_begin()
        {
            _m_this_frame_allocate_rchain_pipeline_count = 0;
        }
        rendchain_branch_pipeline* pipeline_allocate()
        {
            if (_m_this_frame_allocate_rchain_pipeline_count >= _m_rchain_pipeline.size())
            {
                assert(_m_this_frame_allocate_rchain_pipeline_count == _m_rchain_pipeline.size());
                _m_rchain_pipeline.push_back(_m_pipeline->alloc_pipeline());
            }
            return _m_rchain_pipeline[_m_this_frame_allocate_rchain_pipeline_count++];
        }
        void pipeline_update_end()
        {
            for (size_t i = _m_this_frame_allocate_rchain_pipeline_count; i < _m_rchain_pipeline.size(); ++i)
                _m_pipeline->free_pipeline(_m_rchain_pipeline[i]);

            _m_rchain_pipeline.resize(_m_this_frame_allocate_rchain_pipeline_count);
        }

        inline GraphicThreadHost* host()const noexcept
        {
            return _m_pipeline;
        }
    };

    struct UserInterfaceGraphicPipelineSystem : public GraphicPipelineBaseSystem
    {
        using Translation = Transform::Translation;

        using Rendqueue = Renderer::Rendqueue;

        using Clip = Camera::Clip;
        using PerspectiveProjection = Camera::PerspectiveProjection;
        using OrthoProjection = Camera::OrthoProjection;
        using Projection = Camera::Projection;
        using Viewport = Camera::Viewport;
        using RendToFramebuffer = Camera::RendToFramebuffer;

        using Shape = Renderer::Shape;
        using Shaders = Renderer::Shaders;
        using Textures = Renderer::Textures;

        struct camera_arch
        {
            rendchain_branch_pipeline* branchPipeline;

            const Rendqueue* rendqueue;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;

            bool operator < (const camera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };
        struct renderer_arch
        {
            const Rendqueue* rendqueue;
            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;
            const UserInterface::Origin* ui_origin;
            const UserInterface::Rotation* ui_rotation;
  
            bool operator < (const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        std::vector<camera_arch> m_camera_list;
        std::vector<renderer_arch> m_renderer_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_default_uniform_buffer;
        struct default_uniform_buffer_data_t
        {
            jeecs::math::vec4 time;
        };

        UserInterfaceGraphicPipelineSystem(game_world w)
            : GraphicPipelineBaseSystem(w)
        {
            m_default_uniform_buffer = new jeecs::graphic::uniformbuffer(0, sizeof(default_uniform_buffer_data_t));
        }

        ~UserInterfaceGraphicPipelineSystem()
        {

        }

        void PrepareCameras(Projection& projection,
            Translation& translation,
            OrthoProjection* ortho,
            PerspectiveProjection* perspec,
            Clip* clip,
            Viewport* viewport,
            RendToFramebuffer* rendbuf)
        {
            float mat_inv_rotation[4][4];
            translation.world_rotation.create_inv_matrix(mat_inv_rotation);
            float mat_inv_position[4][4] = {};
            mat_inv_position[0][0] = mat_inv_position[1][1] = mat_inv_position[2][2] = mat_inv_position[3][3] = 1.0f;
            mat_inv_position[3][0] = -translation.world_position.x;
            mat_inv_position[3][1] = -translation.world_position.y;
            mat_inv_position[3][2] = -translation.world_position.z;

            // TODO: Optmize
            math::mat4xmat4(projection.view, mat_inv_rotation, mat_inv_position);

            assert(ortho || perspec);
            float znear = clip ? clip->znear : 0.3f;
            float zfar = clip ? clip->zfar : 1000.0f;

            jegl_resource* rend_aim_buffer = rendbuf && rendbuf->framebuffer ? rendbuf->framebuffer->resouce() : nullptr;

            size_t
                RENDAIMBUFFER_WIDTH =
                (size_t)llround(
                    (viewport ? viewport->viewport.z : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH)),
                RENDAIMBUFFER_HEIGHT =
                (size_t)llround(
                    (viewport ? viewport->viewport.w : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT));

            if (ortho)
            {
                graphic::ortho_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, znear, zfar);
                graphic::ortho_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, znear, zfar);
            }
            else
            {
                graphic::perspective_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, znear, zfar);
                graphic::perspective_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, znear, zfar);
            }
        }

        void PreUpdate()
        {
            m_camera_list.clear();
            m_renderer_list.clear();

            this->pipeline_update_begin();

            std::unordered_map<typing::uid_t, UserInterface::Origin*> parent_origin_list;

            select_from(get_world())
                .exec(&UserInterfaceGraphicPipelineSystem::PrepareCameras).anyof<OrthoProjection, PerspectiveProjection>()
                .exec([&](Transform::Anchor& anchor, UserInterface::Origin& origin)
                    {
                        parent_origin_list[anchor.uid] = &origin;
                    })
                .exec(
                    [this](Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf)
                    {
                        auto* branch = this->pipeline_allocate();
                        branch->new_frame(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                        m_camera_list.emplace_back(
                            camera_arch{
                                branch, rendqueue, &projection, cameraviewport, rendbuf
                            }
                        );
                    })
                .exec(
                    [this, &parent_origin_list](
                        Shaders* shads, 
                        Textures* texs, 
                        Shape* shape, 
                        Rendqueue* rendqueue,
                        Transform::LocalToParent* l2p,
                        UserInterface::Origin& origin,
                        UserInterface::Rotation* rotation,
                        UserInterface::Absolute* absolute, 
                        UserInterface::Relatively* relatively)
                    {
                        UserInterface::Origin* parent_origin = nullptr;
                        if (l2p != nullptr)
                        {
                            auto fnd = parent_origin_list.find(l2p->parent_uid);
                            if (fnd != parent_origin_list.end())
                                parent_origin = fnd->second;
                        }

                        if (parent_origin != nullptr)
                        {
                            origin.global_offset = parent_origin->global_offset;
                            origin.global_location = parent_origin->global_location;
                            origin.use_vertical_ratio = parent_origin->use_vertical_ratio;
                        }
                        else
                        {
                            origin.global_offset = {};
                            origin.global_location = {};
                        }

                        if (absolute != nullptr)
                        {
                            origin.global_offset += absolute->offset;
                            origin.size = absolute->size;
                        }
                        else
                            origin.size = {};

                        if (relatively != nullptr)
                        {
                            origin.global_location += relatively->location;
                            origin.scale = relatively->scale;
                            origin.use_vertical_ratio = relatively->use_vertical_ratio;
                        }
                        else
                            origin.scale = {};

                        m_renderer_list.emplace_back(
                            renderer_arch{
                                rendqueue, shape, shads, texs, &origin, rotation
                            });
                    })
                        .anyof<Shaders, Textures, Shape>()
                        .anyof<UserInterface::Absolute, UserInterface::Relatively>()
                        .except<Light2D::Color>()
                        ;
                    std::sort(m_camera_list.begin(), m_camera_list.end());
                    std::sort(m_renderer_list.begin(), m_renderer_list.end());

                    this->pipeline_update_end();

                    DrawFrame();
        }

        void DrawFrame()
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            // TODO: Update shared uniform.
            double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time ,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5))
            };

            m_default_uniform_buffer->update_buffer(
                offsetof(default_uniform_buffer_data_t, time),
                sizeof(math::vec4),
                &shader_time);

            const float MAT4_UI_UNIT[4][4] = {
                { 1.0f, 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f, 0.0f },
                { 0.0f, 0.0f, 0.0f, 1.0f },
            };

            float MAT4_UI_MODULE[4][4];

            for (auto& current_camera : m_camera_list)
            {
                jegl_resource* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer == nullptr)
                        continue;
                    else
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->resouce();
                }

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT;

                jegl_rendchain* rend_chain = nullptr;

                if (current_camera.viewport)
                    rend_chain = current_camera.branchPipeline->allocate_new_chain(rend_aim_buffer,
                        (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                        (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                else
                    rend_chain = current_camera.branchPipeline->allocate_new_chain(rend_aim_buffer,
                        0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                // If camera rend to texture, clear the frame buffer (if need)
                if (rend_aim_buffer)
                    jegl_rchain_clear_color_buffer(rend_chain);
                // Clear depth buffer to overwrite pixels.
                jegl_rchain_clear_depth_buffer(rend_chain);

                jegl_rchain_bind_uniform_buffer(rend_chain, m_default_uniform_buffer->resouce());

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    auto& drawing_shape =
                        (rendentity.shape && rendentity.shape->vertex)
                        ? rendentity.shape->vertex
                        : host()->default_shape_quad;

                    auto& drawing_shaders =
                        (rendentity.shaders && rendentity.shaders->shaders.size())
                        ? rendentity.shaders->shaders
                        : host()->default_shaders_list;

                    // Bind texture here
                    constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;

                    assert(rendentity.ui_origin != nullptr);

                    math::vec2 uisize, uioffset;
                    rendentity.ui_origin->calc_absolute_ui_layout(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT, &uioffset, &uisize);
                    uioffset.x -= (float)RENDAIMBUFFER_WIDTH / 2.0f;
                    uioffset.y -= (float)RENDAIMBUFFER_HEIGHT / 2.0f;

                    // TODO: 这里俩矩阵实际上可以优化，但是UI实际上也没有多少，暂时直接矩阵乘法也无所谓
                    // NOTE: 这里的大小和偏移大小乘二是因为一致空间是 -1 到 1，天然有一个1/2的压缩，为了保证单位正确，这里乘二
                    const float MAT4_UI_SIZE_OFFSET[4][4] = {
                        { uisize.x * 2.0f / (float)RENDAIMBUFFER_WIDTH , 0.0f, 0.0f, 0.0f },
                        { 0.0f, uisize.y * 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 0.0f },
                        { 0.0f, 0.0f, 1.0f, 0.0f },
                        { uioffset.x * 2.0f / (float)RENDAIMBUFFER_WIDTH,
                          uioffset.y * 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 1.0f },
                    };

                    float MAT4_UI_ROTATION[4][4] = {
                      { 1.0f, 0.0f, 0.0f, 0.0f },
                      { 0.0f, 1.0f, 0.0f, 0.0f },
                      { 0.0f, 0.0f, 1.0f, 0.0f },
                      { 0.0f, 0.0f, 0.0f, 1.0f },
                    };
                    if (rendentity.ui_rotation != nullptr)
                    {
                        math::quat q(0.0f, 0.0f, rendentity.ui_rotation->angle);
                        q.create_matrix(MAT4_UI_ROTATION);
                    }

                    math::mat4xmat4(MAT4_UI_MODULE, MAT4_UI_ROTATION, MAT4_UI_SIZE_OFFSET);

                    auto rchain_texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                    jegl_rchain_bind_texture(rend_chain, rchain_texture_group, 0, host()->default_texture->resouce());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, rchain_texture_group, texture.m_pass_id, texture.m_texture->resouce());
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &host()->default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(rend_chain, (*using_shader)->resouce(), drawing_shape->resouce(), rchain_texture_group);
                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_UI_MODULE);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_UI_MODULE);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_UI_MODULE);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, v, float4x4, MAT4_UI_UNIT);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, p, float4x4, MAT4_UI_UNIT);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, vp, float4x4, MAT4_UI_UNIT);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3, 1.0f, 1.0f, 1.0f);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, _using_tiling->x, _using_tiling->y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, _using_offset->x, _using_offset->y);
                    }

                }

            }
        }

    };

    struct UnlitGraphicPipelineSystem : public GraphicPipelineBaseSystem
    {
        using Translation = Transform::Translation;
        using Rendqueue = Renderer::Rendqueue;

        using Clip = Camera::Clip;
        using PerspectiveProjection = Camera::PerspectiveProjection;
        using OrthoProjection = Camera::OrthoProjection;
        using Projection = Camera::Projection;
        using Viewport = Camera::Viewport;
        using RendToFramebuffer = Camera::RendToFramebuffer;

        using Shape = Renderer::Shape;
        using Shaders = Renderer::Shaders;
        using Textures = Renderer::Textures;

        struct camera_arch
        {
            rendchain_branch_pipeline* branchPipeline;

            const Rendqueue* rendqueue;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;

            bool operator < (const camera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };
        struct renderer_arch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;

            bool operator < (const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        std::vector<camera_arch> m_camera_list;
        std::vector<renderer_arch> m_renderer_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_default_uniform_buffer;
        struct default_uniform_buffer_data_t
        {
            jeecs::math::vec4 time;
        };

        UnlitGraphicPipelineSystem(game_world w)
            : GraphicPipelineBaseSystem(w)
        {
            m_default_uniform_buffer = new jeecs::graphic::uniformbuffer(0, sizeof(default_uniform_buffer_data_t));
        }

        ~UnlitGraphicPipelineSystem()
        {

        }

        void PrepareCameras(Projection& projection,
            Translation& translation,
            OrthoProjection* ortho,
            PerspectiveProjection* perspec,
            Clip* clip,
            Viewport* viewport,
            RendToFramebuffer* rendbuf)
        {
            float mat_inv_rotation[4][4];
            translation.world_rotation.create_inv_matrix(mat_inv_rotation);
            float mat_inv_position[4][4] = {};
            mat_inv_position[0][0] = mat_inv_position[1][1] = mat_inv_position[2][2] = mat_inv_position[3][3] = 1.0f;
            mat_inv_position[3][0] = -translation.world_position.x;
            mat_inv_position[3][1] = -translation.world_position.y;
            mat_inv_position[3][2] = -translation.world_position.z;

            // TODO: Optmize
            math::mat4xmat4(projection.view, mat_inv_rotation, mat_inv_position);

            assert(ortho || perspec);
            float znear = clip ? clip->znear : 0.3f;
            float zfar = clip ? clip->zfar : 1000.0f;

            jegl_resource* rend_aim_buffer = rendbuf && rendbuf->framebuffer ? rendbuf->framebuffer->resouce() : nullptr;

            size_t
                RENDAIMBUFFER_WIDTH =
                (size_t)llround(
                    (viewport ? viewport->viewport.z : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH)),
                RENDAIMBUFFER_HEIGHT =
                (size_t)llround(
                    (viewport ? viewport->viewport.w : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT));

            if (ortho)
            {
                graphic::ortho_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, znear, zfar);
                graphic::ortho_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, znear, zfar);
            }
            else
            {
                graphic::perspective_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, znear, zfar);
                graphic::perspective_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, znear, zfar);
            }
        }

        void PreUpdate()
        {
            m_camera_list.clear();
            m_renderer_list.clear();

            this->pipeline_update_begin();

            select_from(get_world())
                .exec(&UnlitGraphicPipelineSystem::PrepareCameras).anyof<OrthoProjection, PerspectiveProjection>()
                .exec(
                    [this](Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf)
                    {
                        auto* branch = this->pipeline_allocate();
                        branch->new_frame(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                        m_camera_list.emplace_back(
                            camera_arch{
                                branch, rendqueue, &projection, cameraviewport, rendbuf
                            }
                        );
                    })
                .exec(
                    [this](Translation& trans, Shaders* shads, Textures* texs, Shape* shape, Rendqueue* rendqueue)
                    {
                        // TODO: Need Impl AnyOf
                            // RendOb will be input to a chain and used for swap
                        m_renderer_list.emplace_back(
                            renderer_arch{
                                rendqueue, &trans, shape, shads, texs
                            });
                    })
                        .anyof<Shaders, Textures, Shape>()
                        .except<Light2D::Color, UserInterface::Origin>()
                        ;
                    std::sort(m_camera_list.begin(), m_camera_list.end());
                    std::sort(m_renderer_list.begin(), m_renderer_list.end());

                    this->pipeline_update_end();

                    DrawFrame();
        }

        void DrawFrame()
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            // TODO: Update shared uniform.
            double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time ,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5))
            };

            m_default_uniform_buffer->update_buffer(
                offsetof(default_uniform_buffer_data_t, time),
                sizeof(math::vec4),
                &shader_time);

            for (auto& current_camera : m_camera_list)
            {
                jegl_resource* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer == nullptr)
                        continue;
                    else
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->resouce();
                }

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;

                float MAT4_MV[4][4], MAT4_VP[4][4];
                math::mat4xmat4(MAT4_VP, MAT4_PROJECTION, MAT4_VIEW);

                jegl_rendchain* rend_chain = nullptr;

                if (current_camera.viewport)
                    rend_chain = current_camera.branchPipeline->allocate_new_chain(rend_aim_buffer,
                        (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                        (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                else
                    rend_chain = current_camera.branchPipeline->allocate_new_chain(rend_aim_buffer,
                        0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                // If camera rend to texture, clear the frame buffer (if need)
                if (rend_aim_buffer)
                    jegl_rchain_clear_color_buffer(rend_chain);
                // Clear depth buffer to overwrite pixels.
                jegl_rchain_clear_depth_buffer(rend_chain);

                jegl_rchain_bind_uniform_buffer(rend_chain, m_default_uniform_buffer->resouce());

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    auto& drawing_shape =
                        (rendentity.shape && rendentity.shape->vertex)
                        ? rendentity.shape->vertex
                        : host()->default_shape_quad;

                    auto& drawing_shaders =
                        (rendentity.shaders && rendentity.shaders->shaders.size())
                        ? rendentity.shaders->shaders
                        : host()->default_shaders_list;

                    // Bind texture here
                    constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;

                    assert(rendentity.translation);

                    float MAT4_MVP[4][4];
                    const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;

                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                    auto rchain_texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                    jegl_rchain_bind_texture(rend_chain, rchain_texture_group, 0, host()->default_texture->resouce());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, rchain_texture_group, texture.m_pass_id, texture.m_texture->resouce());
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &host()->default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(rend_chain, (*using_shader)->resouce(), drawing_shape->resouce(), rchain_texture_group);
                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, v, float4x4, MAT4_VIEW);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, p, float4x4, MAT4_PROJECTION);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, vp, float4x4, MAT4_VP);
                      
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                            rendentity.translation->local_scale.x,
                            rendentity.translation->local_scale.y,
                            rendentity.translation->local_scale.z);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, _using_tiling->x, _using_tiling->y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, _using_offset->x, _using_offset->y);
                    }

                }

            }
        }

    };

    struct DeferLight2DGraphicPipelineSystem : public GraphicPipelineBaseSystem
    {
        struct DeferLight2DHost
        {
            jegl_thread* _m_belong_context;

            // Used for move rend result to camera's render aim buffer.
            jeecs::basic::resource<jeecs::graphic::texture> _no_shadow;
            jeecs::basic::resource<jeecs::graphic::vertex> _screen_vertex;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_parallel_pass;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_pass;

            DeferLight2DHost(jegl_thread* _ctx)
                : _m_belong_context(_ctx)
            {
                using namespace jeecs::graphic;
                _no_shadow = texture::create(1, 1, jegl_texture::texture_format::RGBA);
                _no_shadow->pix(0, 0).set(math::vec4(0.f, 0.f, 0.f, 0.f));

                _screen_vertex = new vertex(jegl_vertex::vertex_type::TRIANGLESTRIP,
                    {
                        -1.f, 1.f, 0.f,     0.f, 1.f,
                        -1.f, -1.f, 0.f,    0.f, 0.f,
                        1.f, 1.f, 0.f,      1.f, 1.f,
                        1.f, -1.f, 0.f,     1.f, 0.f,
                    },
                    { 3, 2 });

                // 用于消除阴影对象本身的阴影
                _defer_light2d_shadow_sub_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_sub.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    return v2f{
        pos = je_mvp * float4::create(v.vertex, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let main_texture = uniform_texture:<texture2d>("MainTexture", 0);
    let final_shadow = alphatest(float4::create(je_color->xyz, texture(main_texture, vf.uv)->w));

    return fout{
        shadow_factor = float4::create(final_shadow->x, final_shadow->x, final_shadow->x, float::one)
    };
}
)") };

                // 用于产生点光源的形状阴影（光在物体前）
                _defer_light2d_shadow_shape_point_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_point_shape.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    let light2d_vpos = je_v * float4::create(je_color->xyz, 1.);
    let shadow_scale_factor = je_color->w;

    let vpos = je_mv * float4::create(v.vertex, 1.);
    let shadow_vpos = normalize((vpos->xyz / vpos->w) - (light2d_vpos->xyz / light2d_vpos->w)) * shadow_scale_factor;
    
    return v2f{
        pos = je_p * float4::create((vpos->xyz / vpos->w) + shadow_vpos, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let main_texture = uniform_texture:<texture2d>("MainTexture", 0);
    let final_shadow = alphatest(
        float4::create(
            je_local_scale,     // NOTE: je_local_scale->x is shadow factor here.
            texture(main_texture, vf.uv)->w));

    return fout{
        shadow_factor = float4::create(final_shadow->x, final_shadow->x, final_shadow->x, float::one)
    };
}
)") };

                // 用于产生点光源的范围阴影（光在物体后）
                _defer_light2d_shadow_point_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_point.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin
{
    vertex: float3,
    factor: float,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    // ATTENTION: We will using je_color: float4 to pass lwpos.
    let light_vpos = je_v * je_color;
    let vpos = je_mv * float4::create(v.vertex, 1.);

    let shadow_vdir = float3::create(normalize(vpos->xy - light_vpos->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(vf: v2f)
{
    // NOTE: je_local_scale->x is shadow factor here.
    return fout{
        shadow_factor = float4::create(je_local_scale->x, je_local_scale->x, je_local_scale->x, float::one)
    };
}
)") };

                // 用于产生平行光源的形状阴影（光在物体前）
                _defer_light2d_shadow_shape_parallel_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_parallel_shape.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    let light2d_vdir = (je_v * float4::create(je_color->xyz, 1.))->xyz - movement(je_v);
    let shadow_scale_factor = je_color->w;

    let vpos = je_mv * float4::create(v.vertex, 1.);
    let shadow_vpos = normalize(light2d_vdir) * shadow_scale_factor;
    
    return v2f{
        pos = je_p * float4::create((vpos->xyz / vpos->w) + shadow_vpos, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let main_texture = uniform_texture:<texture2d>("MainTexture", 0);
    let final_shadow = alphatest(
        float4::create(
            je_local_scale,     // NOTE: je_local_scale->x is shadow factor here.
            texture(main_texture, vf.uv)->w));

    return fout{
        shadow_factor = float4::create(final_shadow->x, final_shadow->x, final_shadow->x, float::one)
    };
}
)") };

                // 用于产生平行光源的范围阴影（光在物体后）
                _defer_light2d_shadow_parallel_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_parallel.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin
{
    vertex: float3,
    factor: float,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    // ATTENTION: We will using je_color: float4 to pass lwpos.
    let light_vdir = (je_v * je_color)->xyz - movement(je_v);
    let vpos = je_mv * float4::create(v.vertex, 1.);

    let shadow_vdir = float3::create(normalize(light_vdir->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(vf: v2f)
{
    // NOTE: je_local_scale->x is shadow factor here.
    return fout{
        shadow_factor = float4::create(je_local_scale->x, je_local_scale->x, je_local_scale->x, float::one)
    };
}
)") };
            }

            static DeferLight2DHost* instance(jegl_thread* glcontext)
            {
                static DeferLight2DHost* _instance = nullptr;
                if (_instance == nullptr || _instance->_m_belong_context != glcontext)
                {
                    if (_instance != nullptr)
                        delete _instance;
                    _instance = new DeferLight2DHost(glcontext);
                }
                return _instance;
            }
        };

        using Translation = Transform::Translation;

        using Rendqueue = Renderer::Rendqueue;

        using Clip = Camera::Clip;
        using PerspectiveProjection = Camera::PerspectiveProjection;
        using OrthoProjection = Camera::OrthoProjection;
        using Projection = Camera::Projection;
        using Viewport = Camera::Viewport;
        using RendToFramebuffer = Camera::RendToFramebuffer;

        using Shape = Renderer::Shape;
        using Shaders = Renderer::Shaders;
        using Textures = Renderer::Textures;

        struct camera_arch
        {
            rendchain_branch_pipeline* branchPipeline;

            const Rendqueue* rendqueue;
            const Translation* translation;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;
            const Light2D::CameraPostPass* light2DPostPass;

            bool operator < (const camera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        struct light2d_arch
        {
            const Translation* translation;
            const Light2D::Color* color;
            const Light2D::Shadow* shadow;

            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;
        };

        struct renderer_arch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;

            bool operator < (const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        struct block2d_arch
        {
            const Translation* translation;
            const Light2D::Block* block;
            const Textures* textures;
            const Shape* shape;
        };

        std::vector<camera_arch> m_camera_list;
        std::vector<renderer_arch> m_renderer_list;
        std::list<light2d_arch> m_2dlight_list;
        std::vector<block2d_arch> m_2dblock_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_default_uniform_buffer;
        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_light2d_uniform_buffer;
        struct default_uniform_buffer_data_t
        {
            jeecs::math::vec4 time;
        };
        struct light2d_uniform_buffer_data_t
        {
            struct light2d_info
            {
                jeecs::math::vec4 color;
                jeecs::math::vec4 position;
                jeecs::math::vec4 direction;
                jeecs::math::vec4 factors;
            };

            light2d_info l2ds[JE_MAX_LIGHT2D_COUNT];
        };

        DeferLight2DGraphicPipelineSystem(game_world w)
            : GraphicPipelineBaseSystem(w)
        {
            m_default_uniform_buffer = new jeecs::graphic::uniformbuffer(0, sizeof(default_uniform_buffer_data_t));
            m_light2d_uniform_buffer = new jeecs::graphic::uniformbuffer(1, sizeof(light2d_uniform_buffer_data_t));
        }

        ~DeferLight2DGraphicPipelineSystem()
        {

        }

        void PrepareCameras(Projection& projection,
            Translation& translation,
            OrthoProjection* ortho,
            PerspectiveProjection* perspec,
            Clip* clip,
            Viewport* viewport,
            RendToFramebuffer* rendbuf)
        {
            float mat_inv_rotation[4][4];
            translation.world_rotation.create_inv_matrix(mat_inv_rotation);
            float mat_inv_position[4][4] = {};
            mat_inv_position[0][0] = mat_inv_position[1][1] = mat_inv_position[2][2] = mat_inv_position[3][3] = 1.0f;
            mat_inv_position[3][0] = -translation.world_position.x;
            mat_inv_position[3][1] = -translation.world_position.y;
            mat_inv_position[3][2] = -translation.world_position.z;

            // TODO: Optmize
            math::mat4xmat4(projection.view, mat_inv_rotation, mat_inv_position);

            assert(ortho || perspec);
            float znear = clip ? clip->znear : 0.3f;
            float zfar = clip ? clip->zfar : 1000.0f;

            jegl_resource* rend_aim_buffer = rendbuf && rendbuf->framebuffer ? rendbuf->framebuffer->resouce() : nullptr;

            size_t
                RENDAIMBUFFER_WIDTH =
                (size_t)llround(
                    (viewport ? viewport->viewport.z : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH)),
                RENDAIMBUFFER_HEIGHT =
                (size_t)llround(
                    (viewport ? viewport->viewport.w : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT));

            if (ortho)
            {
                graphic::ortho_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, znear, zfar);
                graphic::ortho_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, znear, zfar);
            }
            else
            {
                graphic::perspective_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, znear, zfar);
                graphic::perspective_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, znear, zfar);
            }
        }

        void PreUpdate()
        {
            m_2dlight_list.clear();
            m_2dblock_list.clear();
            m_camera_list.clear();
            m_renderer_list.clear();

            this->pipeline_update_begin();

            select_from(get_world())
                .exec(&DeferLight2DGraphicPipelineSystem::PrepareCameras).anyof<OrthoProjection, PerspectiveProjection>()
                .exec(
                    [this](Translation& tarns, Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf, Light2D::CameraPostPass* light2dpostpass)
                    {
                        auto* branch = this->pipeline_allocate();
                        branch->new_frame(rendqueue == nullptr ? 0 : rendqueue->rend_queue);

                        m_camera_list.emplace_back(
                            camera_arch{
                                branch, rendqueue, &tarns, &projection, cameraviewport, rendbuf, light2dpostpass
                            }
                        );

                        if (light2dpostpass != nullptr)
                        {
                            auto* rend_aim_buffer = (rendbuf != nullptr && rendbuf->framebuffer != nullptr)
                                ? rendbuf->framebuffer->resouce()
                                : nullptr;

                            size_t
                                RENDAIMBUFFER_WIDTH =
                                (size_t)llround(
                                    (cameraviewport ? cameraviewport->viewport.z : 1.0f) *
                                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH)),
                                RENDAIMBUFFER_HEIGHT =
                                (size_t)llround(
                                    (cameraviewport ? cameraviewport->viewport.w : 1.0f) *
                                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT));

                            bool need_update = light2dpostpass->post_rend_target == nullptr
                                || light2dpostpass->post_rend_target->resouce()->m_raw_framebuf_data->m_width != RENDAIMBUFFER_WIDTH
                                || light2dpostpass->post_rend_target->resouce()->m_raw_framebuf_data->m_height != RENDAIMBUFFER_HEIGHT;
                            if (need_update)
                            {
                                light2dpostpass->post_rend_target
                                    = new jeecs::graphic::framebuffer(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT,
                                        {
                                            jegl_texture::texture_format::RGBA, // 漫反射颜色
                                            jegl_texture::texture_format(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // 自发光颜色，用于法线反射或者发光物体的颜色参数，最终混合shader会将此参数用于光照计算
                                            jegl_texture::texture_format(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // 视空间坐标(RGB) Alpha通道暂时留空
                                            jegl_texture::texture_format::DEPTH, // 深度缓冲区
                                        });
                                light2dpostpass->post_light_target
                                    = new jeecs::graphic::framebuffer(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT,
                                        {
                                            (jegl_texture::texture_format)(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // 光渲染结果
                                        });
                            }
                        }
                    })
                .exec(
                    [this](Translation& trans, Shaders* shads, Textures* texs, Shape* shape, Rendqueue* rendqueue)
                    {
                        // RendOb will be input to a chain and used for swap
                        m_renderer_list.emplace_back(
                            renderer_arch{
                                rendqueue, &trans, shape, shads, texs
                            });
                    }).anyof<Shaders, Textures, Shape>()
                        .except<Light2D::Color, UserInterface::Origin>()
                        .exec(
                            [this](Translation& trans,
                                Light2D::Color& color,
                                Light2D::Shadow* shadow,
                                Shape* shape,
                                Shaders* shads,
                                Textures* texs)
                            {
                                m_2dlight_list.emplace_back(
                                    light2d_arch{
                                        &trans, &color, shadow,
                                        shape,
                                        shads,
                                        texs,
                                    }
                                );
                                if (shadow != nullptr)
                                {
                                    bool generate_new_framebuffer =
                                        shadow->shadow_buffer == nullptr
                                        || shadow->shadow_buffer->resouce()->m_raw_framebuf_data->m_width != shadow->resolution_width
                                        || shadow->shadow_buffer->resouce()->m_raw_framebuf_data->m_height != shadow->resolution_height;

                                    if (generate_new_framebuffer)
                                    {
                                        shadow->shadow_buffer = new graphic::framebuffer(
                                            shadow->resolution_width, shadow->resolution_height,
                                            {
                                                jegl_texture::texture_format::RGBA,
                                                // Only store shadow value to R, FBO in opengl not support rend to MONO
                                            }
                                        );
                                        shadow->shadow_buffer->get_attachment(0)->resouce()->m_raw_texture_data->m_sampling
                                            = (jegl_texture::texture_sampling)(
                                                jegl_texture::texture_sampling::LINEAR
                                                | jegl_texture::texture_sampling::CLAMP_EDGE);
                                    }
                                }
                            }
                        ).anyof<Shaders, Textures, Shape>()
                                .exec(
                                    [this](Translation& trans, Light2D::Block& block, Textures* texture, Shape* shape)
                                    {
                                        if (block.mesh.m_block_mesh == nullptr)
                                        {
                                            std::vector<float> _vertex_buffer;
                                            if (!block.mesh.m_block_points.empty())
                                            {
                                                for (auto& point : block.mesh.m_block_points)
                                                {
                                                    _vertex_buffer.insert(_vertex_buffer.end(),
                                                        {
                                                            point.x, point.y, 0.f, 0.f,
                                                            point.x, point.y, 0.f, 1.f,
                                                        });
                                                }
                                                _vertex_buffer.insert(_vertex_buffer.end(),
                                                    {
                                                        block.mesh.m_block_points[0].x, block.mesh.m_block_points[0].y, 0.f, 0.f,
                                                        block.mesh.m_block_points[0].x, block.mesh.m_block_points[0].y, 0.f, 1.f,
                                                    });
                                                block.mesh.m_block_mesh = new jeecs::graphic::vertex(
                                                    jeecs::graphic::vertex::type::TRIANGLESTRIP,
                                                    _vertex_buffer, { 3,1 });
                                            }
                                        }
                                        // Cannot create vertex with 0 point.

                                        if (block.mesh.m_block_mesh != nullptr)
                                        {
                                            m_2dblock_list.push_back(
                                                block2d_arch{
                                                        &trans, &block, texture, shape
                                                }
                                            );
                                        }
                                    }
                            );
                            std::sort(m_2dblock_list.begin(), m_2dblock_list.end(),
                                [](const block2d_arch& a, const block2d_arch& b) {
                                    return a.translation->world_position.z > b.translation->world_position.z;
                                });
                            std::sort(m_camera_list.begin(), m_camera_list.end());
                            std::sort(m_renderer_list.begin(), m_renderer_list.end());

                            this->pipeline_update_end();

                            DrawFrame();
        }

        void DrawFrame()
        {
            auto* glthread = _m_pipeline->glthread;
            auto* light2d_host = DeferLight2DHost::instance(glthread);

            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            // TODO: Update shared uniform.
            double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time ,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5))
            };

            m_default_uniform_buffer->update_buffer(
                offsetof(default_uniform_buffer_data_t, time),
                sizeof(math::vec4),
                &shader_time);

            std::vector<jegl_resource*> LIGHT2D_SHADOW;

            light2d_uniform_buffer_data_t l2dbuf = {};
            // Update l2d buffer here.
            size_t light_count = 0;
            for (auto& lightarch : m_2dlight_list)
            {
                if (light_count >= JE_MAX_LIGHT2D_COUNT)
                    break;

                l2dbuf.l2ds[light_count].color = lightarch.color->color;
                l2dbuf.l2ds[light_count].position = lightarch.translation->world_position;
                l2dbuf.l2ds[light_count].direction =
                    lightarch.translation->world_rotation * math::vec3(0.f, -1.f, 1.f).unit();

                l2dbuf.l2ds[light_count].position.w =
                    ((lightarch.shape == nullptr || lightarch.shape->vertex == nullptr
                        ? math::vec3(
                            host()->default_shape_quad->resouce()->m_raw_vertex_data->m_size_x,
                            host()->default_shape_quad->resouce()->m_raw_vertex_data->m_size_y,
                            host()->default_shape_quad->resouce()->m_raw_vertex_data->m_size_z)
                        : math::vec3(
                            lightarch.shape->vertex->resouce()->m_raw_vertex_data->m_size_x,
                            lightarch.shape->vertex->resouce()->m_raw_vertex_data->m_size_y,
                            lightarch.shape->vertex->resouce()->m_raw_vertex_data->m_size_z)
                        )
                        * lightarch.translation->local_scale).length()
                    / 2.0f;

                l2dbuf.l2ds[light_count].factors = math::vec4(
                    lightarch.color->parallel ? 0.f : 1.f,
                    lightarch.color->parallel ? 1.f : 0.f,
                    lightarch.color->decay,
                    0.f);

                if (lightarch.shadow != nullptr)
                {
                    assert(lightarch.shadow->shadow_buffer != nullptr);
                    LIGHT2D_SHADOW.push_back(lightarch.shadow->shadow_buffer->get_attachment(0)->resouce());
                }
                else
                    LIGHT2D_SHADOW.push_back(light2d_host->_no_shadow->resouce());
                    /*jegl_using_texture(light2d_host->_no_shadow->resouce(),
                        JE_SHADOW2D_0 + light_count);*/

                ++light_count;
            }
            m_light2d_uniform_buffer->update_buffer(
                0,
                sizeof(light2d_uniform_buffer_data_t),
                &l2dbuf
            );

            for (auto& current_camera : m_camera_list)
            {
                jegl_resource* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer == nullptr)
                        continue;
                    else
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->resouce();
                }

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;

                float MAT4_VP[4][4];
                float MAT4_MV[4][4], MAT4_MVP[4][4];

                math::mat4xmat4(MAT4_VP, MAT4_PROJECTION, MAT4_VIEW);

                jegl_rendchain* rend_chain = nullptr;

                // If current camera contain light2d-pass, prepare light shadow here.
                if (current_camera.light2DPostPass != nullptr && current_camera.light2DPostPass->post_shader.has_resource())
                {
                    assert(current_camera.light2DPostPass->defer_rend_aim != nullptr);

                    // Walk throw all light, rend shadows to light's ShadowBuffer.
                    for (auto& lightarch : m_2dlight_list)
                    {
                        if (lightarch.shadow != nullptr)
                        {
                            auto light2d_shadow_aim_buffer = lightarch.shadow->shadow_buffer->resouce();
                            jegl_rendchain* light2d_shadow_rend_chain = current_camera.branchPipeline->allocate_new_chain(
                                light2d_shadow_aim_buffer, 0, 0,
                                light2d_shadow_aim_buffer->m_raw_framebuf_data->m_width,
                                light2d_shadow_aim_buffer->m_raw_framebuf_data->m_height);

                            jegl_rchain_clear_color_buffer(light2d_shadow_rend_chain);
                            jegl_rchain_clear_depth_buffer(light2d_shadow_rend_chain);

                            const auto& normal_shadow_pass =
                                lightarch.color->parallel ?
                                light2d_host->_defer_light2d_shadow_parallel_pass :
                                light2d_host->_defer_light2d_shadow_point_pass;
                            const auto& shape_shadow_pass =
                                lightarch.color->parallel ?
                                light2d_host->_defer_light2d_shadow_shape_parallel_pass :
                                light2d_host->_defer_light2d_shadow_shape_point_pass;

                            const auto& sub_shadow_pass = light2d_host->_defer_light2d_shadow_sub_pass;
                            const size_t block_entity_count = m_2dblock_list.size();

                            std::list<block2d_arch*> block_in_current_layer;

                            auto block2d_iter = m_2dblock_list.begin();
                            auto block2d_end = m_2dblock_list.end();

                            // 看了一圈又一圈，之前这里的代码实在是太晦气了，重写！
                            for (; block2d_iter != block2d_end; ++block2d_iter)
                            {
                                auto& blockarch = *block2d_iter;
                                int64_t current_layer = (int64_t)(blockarch.translation->world_position.z * 100.f);

                                block_in_current_layer.push_back(&blockarch);

                                if (blockarch.block->shadow > 0.f)
                                {
                                    if (lightarch.shadow->shape_offset > 0.f
                                        && current_layer > lightarch.translation->world_position.z * 100.f)
                                    {
                                        // 物体比光源更靠近摄像机，且形状阴影工作被允许

                                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_shadow_rend_chain);
                                        if (blockarch.textures != nullptr)
                                        {
                                            jeecs::graphic::texture* main_texture = blockarch.textures->get_texture(0);
                                            if (main_texture != nullptr)
                                                jegl_rchain_bind_texture(light2d_shadow_rend_chain, texture_group, 0, main_texture->resouce());
                                            else
                                                jegl_rchain_bind_texture(light2d_shadow_rend_chain, texture_group, 0, host()->default_texture->resouce());
                                        }

                                        jeecs::graphic::vertex* using_shape = (blockarch.shape == nullptr
                                            || blockarch.shape->vertex == nullptr)
                                            ? host()->default_shape_quad
                                            : blockarch.shape->vertex;

                                        auto* rchain_draw_action = jegl_rchain_draw(
                                            light2d_shadow_rend_chain, 
                                            shape_shadow_pass->resouce(),
                                            using_shape->resouce(), 
                                            texture_group);
                                        auto* builtin_uniform = shape_shadow_pass->m_builtin;

                                        const float(&MAT4_MODEL)[4][4] = blockarch.translation->object2world;

                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, v, float4x4, MAT4_VIEW);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, p, float4x4, MAT4_PROJECTION);

                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, vp, float4x4, MAT4_VP);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                                        // 通过 local_scale.x 传递阴影权重，.y .z 通道预留
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, 
                                            local_scale, float3,
                                            blockarch.block->shadow,
                                            0.f,
                                            0.f);

                                        if (blockarch.textures != nullptr)
                                        {
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, 
                                                blockarch.textures->tiling.x, blockarch.textures->tiling.y);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, 
                                                blockarch.textures->offset.x, blockarch.textures->offset.y);
                                        }

                                        // 通过 je_color 变量传递着色器的位置或方向
                                        if (lightarch.color->parallel)
                                        {
                                            jeecs::math::vec3 rotated_light_dir =
                                                lightarch.translation->world_rotation * jeecs::math::vec3(0.f, -1.f, 0.f);

                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                rotated_light_dir.x,
                                                rotated_light_dir.y,
                                                rotated_light_dir.z,
                                                lightarch.shadow->shape_offset);
                                        }
                                        else
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                lightarch.translation->world_position.x,
                                                lightarch.translation->world_position.y,
                                                lightarch.translation->world_position.z,
                                                lightarch.shadow->shape_offset);
                                    }
                                    else
                                    {
                                        // 物体比光源更远离摄像机，或者形状阴影被禁用

                                        auto* rchain_draw_action = jegl_rchain_draw(
                                            light2d_shadow_rend_chain,
                                            normal_shadow_pass->resouce(),
                                            blockarch.block->mesh.m_block_mesh->resouce(),
                                            SIZE_MAX);
                                        auto* builtin_uniform = normal_shadow_pass->m_builtin;

                                        const float(&MAT4_MODEL)[4][4] = blockarch.translation->object2world;

                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, v, float4x4, MAT4_VIEW);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, p, float4x4, MAT4_PROJECTION);

                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, vp, float4x4, MAT4_VP);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                                        // 通过 local_scale.x 传递阴影权重，.y .z 通道预留
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                                            blockarch.block->shadow,
                                            0.f,
                                            0.f);

                                        if (lightarch.color->parallel)
                                        {
                                            jeecs::math::vec3 rotated_light_dir =
                                                lightarch.translation->world_rotation * jeecs::math::vec3(0.f, -1.f, 1.f).unit();
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                rotated_light_dir.x,
                                                rotated_light_dir.y,
                                                rotated_light_dir.z,
                                                1.f);
                                        }
                                        else
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                lightarch.translation->world_position.x,
                                                lightarch.translation->world_position.y,
                                                lightarch.translation->world_position.z,
                                                1.f);
                                    }
                                }

                                // 如果下一个阴影将会在不同层级，或者当前阴影是最后一个阴影，则更新阴影覆盖
                                auto next_block2d_iter = block2d_iter + 1;
                                if (next_block2d_iter == block2d_end
                                    || current_layer != (int64_t)(next_block2d_iter->translation->world_position.z * 100.f))
                                {
                                    for (auto* block_in_layer : block_in_current_layer)
                                    {
                                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_shadow_rend_chain);
                                        if (block_in_layer->textures != nullptr)
                                        {
                                            jeecs::graphic::texture* main_texture = block_in_layer->textures->get_texture(0);
                                            if (main_texture != nullptr)
                                                jegl_rchain_bind_texture(light2d_shadow_rend_chain, texture_group, 0, main_texture->resouce());
                                            else
                                                jegl_rchain_bind_texture(light2d_shadow_rend_chain, texture_group, 0, host()->default_texture->resouce());
                                        }

                                        jeecs::graphic::vertex* using_shape = (block_in_layer->shape == nullptr
                                            || block_in_layer->shape->vertex == nullptr)
                                            ? host()->default_shape_quad
                                            : block_in_layer->shape->vertex;

                                        auto* rchain_draw_action = jegl_rchain_draw(
                                            light2d_shadow_rend_chain, 
                                            sub_shadow_pass->resouce(), 
                                            using_shape->resouce(),
                                            texture_group);
                                        auto* builtin_uniform = sub_shadow_pass->m_builtin;

                                        const float(&MAT4_MODEL)[4][4] = block_in_layer->translation->object2world;

                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, v, float4x4, MAT4_VIEW);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, p, float4x4, MAT4_PROJECTION);

                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, vp, float4x4, MAT4_VP);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                                        // local_scale.x .y .z 通道预留
                                        /*NEED_AND_SET_UNIFORM(local_scale, float3,
                                            0.f,
                                            0.f,
                                            0.f);*/

                                        if (block_in_layer->translation->world_position.z < lightarch.translation->world_position.z)
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4, 1.f, 1.f, 1.f, 1.f);
                                        else
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4, 0.f, 0.f, 0.f, 0.f);

                                        if (block_in_layer->textures != nullptr)
                                        {
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, block_in_layer->textures->tiling.x, block_in_layer->textures->tiling.y);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, block_in_layer->textures->offset.x, block_in_layer->textures->offset.y);
                                        }

                                    }
                                    block_in_current_layer.clear();
                                }

                            }
                        }
                    }

                    auto light2d_rend_aim_buffer = current_camera.light2DPostPass->post_rend_target->resouce();

                    rend_chain = current_camera.branchPipeline->allocate_new_chain(light2d_rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);
                    jegl_rchain_clear_color_buffer(rend_chain);
                    jegl_rchain_clear_depth_buffer(rend_chain);
                }
                else
                {
                    if (current_camera.viewport)
                        rend_chain = current_camera.branchPipeline->allocate_new_chain(rend_aim_buffer,
                            (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                            (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                    else
                        rend_chain = current_camera.branchPipeline->allocate_new_chain(rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    // If camera rend to texture, clear the frame buffer (if need)
                    if (rend_aim_buffer)
                        jegl_rchain_clear_color_buffer(rend_chain);
                    // Clear depth buffer to overwrite pixels.
                    jegl_rchain_clear_depth_buffer(rend_chain);
                }

                jegl_rchain_bind_uniform_buffer(rend_chain, m_default_uniform_buffer->resouce());
                jegl_rchain_bind_uniform_buffer(rend_chain, m_light2d_uniform_buffer->resouce());

                auto shadow_pre_bind_texture_group = jegl_rchain_allocate_texture_group(rend_chain);
                for (size_t i = 0; i < LIGHT2D_SHADOW.size(); ++i)
                    jegl_rchain_bind_texture(rend_chain, shadow_pre_bind_texture_group, JE_SHADOW2D_0 + i, LIGHT2D_SHADOW[i]);

                jegl_rchain_bind_pre_texture_group(rend_chain, shadow_pre_bind_texture_group);

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.translation);

                    const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;

                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                    auto& drawing_shape =
                        (rendentity.shape && rendentity.shape->vertex)
                        ? rendentity.shape->vertex
                        : host()->default_shape_quad;
                    auto& drawing_shaders =
                        (rendentity.shaders && rendentity.shaders->shaders.size())
                        ? rendentity.shaders->shaders
                        : host()->default_shaders_list;

                    // Bind texture here
                    constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;

                    auto texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                    jegl_rchain_bind_texture(rend_chain, texture_group, 0, host()->default_texture->resouce());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, texture_group, texture.m_pass_id, texture.m_texture->resouce());

                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &host()->default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(rend_chain, (*using_shader)->resouce(), drawing_shape->resouce(), texture_group);
                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, v, float4x4, MAT4_VIEW);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, p, float4x4, MAT4_PROJECTION);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, vp, float4x4, MAT4_VP);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                            rendentity.translation->local_scale.x,
                            rendentity.translation->local_scale.y,
                            rendentity.translation->local_scale.z);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, _using_tiling->x, _using_tiling->y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, _using_offset->x, _using_offset->y);
                    }

                }

                if (current_camera.light2DPostPass != nullptr && current_camera.light2DPostPass->post_shader.has_resource())
                {
                    // Rend light buffer to target buffer.
                    assert(current_camera.light2DPostPass->post_rend_target != nullptr
                        && current_camera.light2DPostPass->post_light_target != nullptr);

                    auto* light2d_host = DeferLight2DHost::instance(glthread);

                    // Rend Light result to target buffer.
                    jegl_rendchain* light2d_light_effect_rend_chain = current_camera.branchPipeline->allocate_new_chain(
                        current_camera.light2DPostPass->post_light_target->resouce(), 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    jegl_rchain_clear_color_buffer(light2d_light_effect_rend_chain);
                    auto lightpass_pre_bind_texture_group = jegl_rchain_allocate_texture_group(light2d_light_effect_rend_chain);
                    
                    // Bind attachment
                    // 绑定漫反射颜色通道
                    jegl_rchain_bind_texture(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group, JE_LIGHT2D_DEFER_0 + 0,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(0)->resouce());
                    // 绑定自发光通道
                    jegl_rchain_bind_texture(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group, JE_LIGHT2D_DEFER_0 + 1,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(1)->resouce());
                    // 绑定视空间坐标通道
                    jegl_rchain_bind_texture(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group, JE_LIGHT2D_DEFER_0 + 2,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(2)->resouce());
                    
                    jegl_rchain_bind_pre_texture_group(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group);

                    for (auto& light2d : m_2dlight_list)
                    {
                        // 绑定阴影
                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_light_effect_rend_chain);

                        jegl_rchain_bind_texture(light2d_light_effect_rend_chain, texture_group,
                            JE_LIGHT2D_DEFER_0 + 3,
                            light2d.shadow != nullptr
                            ? light2d.shadow->shadow_buffer->get_attachment(0)->resouce()
                            : light2d_host->_no_shadow->resouce());

                        // 开始渲染光照！
                        const float(&MAT4_MODEL)[4][4] = light2d.translation->object2world;

                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                        auto& drawing_shape =
                            (light2d.shape && light2d.shape->vertex)
                            ? light2d.shape->vertex
                            : host()->default_shape_quad;
                        auto& drawing_shaders =
                            (light2d.shaders && light2d.shaders->shaders.size())
                            ? light2d.shaders->shaders
                            : host()->default_shaders_list;

                        // Bind texture here
                        constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                        const jeecs::math::vec2
                            * _using_tiling = &default_tiling,
                            * _using_offset = &default_offset;
                        jegl_rchain_bind_texture(light2d_light_effect_rend_chain, texture_group, 0, host()->default_texture->resouce());
                        if (light2d.textures)
                        {
                            _using_tiling = &light2d.textures->tiling;
                            _using_offset = &light2d.textures->offset;

                            for (auto& texture : light2d.textures->textures)
                                jegl_rchain_bind_texture(light2d_light_effect_rend_chain, texture_group, texture.m_pass_id, texture.m_texture->resouce());
                        }
                        for (auto& shader_pass : drawing_shaders)
                        {
                            auto* using_shader = &shader_pass;
                            if (!shader_pass->m_builtin)
                                using_shader = &host()->default_shader;

                            auto* rchain_draw_action = jegl_rchain_draw(light2d_light_effect_rend_chain, (*using_shader)->resouce(), drawing_shape->resouce(), texture_group);
                            auto* builtin_uniform = (*using_shader)->m_builtin;

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, v, float4x4, MAT4_VIEW);
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, p, float4x4, MAT4_PROJECTION);

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, vp, float4x4, MAT4_VP);
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                                light2d.translation->local_scale.x,
                                light2d.translation->local_scale.y,
                                light2d.translation->local_scale.z);

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, _using_tiling->x, _using_tiling->y);
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, _using_offset->x, _using_offset->y);

                            // 传入Light2D所需的颜色、衰减信息
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                light2d.color->color.x,
                                light2d.color->color.y,
                                light2d.color->color.z,
                                light2d.color->color.w);

                            if (light2d.shadow != nullptr)
                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                                    (float)light2d.shadow->resolution_width,
                                    (float)light2d.shadow->resolution_height);
                            else
                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                                    (float)1.f,
                                    (float)1.f);

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_decay, float, light2d.color->decay);                           
                        }
                    }

                    // Rend final result color to screen.
                    // Set target buffer.
                    jegl_rendchain* final_target_rend_chain = nullptr;
                    if (current_camera.viewport)
                        final_target_rend_chain = current_camera.branchPipeline->allocate_new_chain(rend_aim_buffer,
                            (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                            (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                    else
                        final_target_rend_chain = current_camera.branchPipeline->allocate_new_chain(
                            rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    // If camera rend to texture, clear the frame buffer (if need)
                    if (rend_aim_buffer)
                        jegl_rchain_clear_color_buffer(rend_chain);
                    // Clear depth buffer to overwrite pixels.
                    jegl_rchain_clear_depth_buffer(rend_chain);

                    auto texture_group = jegl_rchain_allocate_texture_group(final_target_rend_chain);
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, 0,
                        current_camera.light2DPostPass->post_light_target->get_attachment(0)->resouce());

                    // 将光照信息储存到通道0，进行最终混合和gamma矫正等操作，完成输出
                    auto* rchain_draw_action = jegl_rchain_draw(final_target_rend_chain,
                        current_camera.light2DPostPass->post_shader.get_resource()->resouce(),
                        light2d_host->_screen_vertex->resouce(), texture_group);

                    auto* builtin_uniform = current_camera.light2DPostPass->post_shader.get_resource()->m_builtin;

                    JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                        (float)current_camera.light2DPostPass->post_light_target->resouce()->m_raw_framebuf_data->m_width,
                        (float)current_camera.light2DPostPass->post_light_target->resouce()->m_raw_framebuf_data->m_height);
                } // Finish for Light2d effect.
            }
        }

    };

    struct FrameAnimation2DSystem : public game_system
    {
        double _fixed_time = 0.;

        FrameAnimation2DSystem(game_world w)
            : game_system(w)
        {

        }

        void Update()
        {
            _fixed_time += delta_dtime();
            select_from(get_world())
                .exec([this](game_entity e, Animation2D::FrameAnimation& frame_animation, Renderer::Shaders* shaders)
                    {
                        for (auto& animation : frame_animation.animations.m_animations)
                        {
                            auto* active_animation_frames =
                                animation.m_animations.find(animation.m_current_action);

                            if (active_animation_frames != animation.m_animations.end()
                                && active_animation_frames->v.frames.empty() == false)
                            {
                                // 当前动画数据找到，如果当前帧是 SIZEMAX，或者已经到了要更新帧的时候，
                                if (animation.m_current_frame_index == SIZE_MAX
                                    || animation.m_next_update_time <= _fixed_time)
                                {
                                    bool finish_animation = false;

                                    auto update_and_apply_component_frame_data =
                                        [](const game_entity& e, jeecs::Animation2D::FrameAnimation::animation_data_set_list::frame_data& frame)
                                    {
                                        for (auto& cdata : frame.m_component_data)
                                        {
                                            if (cdata.m_entity_cache == e)
                                                continue;

                                            cdata.m_entity_cache = e;

                                            assert(cdata.m_component_type != nullptr && cdata.m_member_info != nullptr);

                                            auto* component_addr = je_ecs_world_entity_get_component(&e, cdata.m_component_type);
                                            if (component_addr == nullptr)
                                                // 没有这个组件，忽略之
                                                continue;

                                            auto* member_addr = (void*)(cdata.m_member_info->m_member_offset + (intptr_t)component_addr);

                                            // 在这里做好缓存和检查，不要每次都重新获取组件地址和检查类型
                                            cdata.m_member_addr_cache = member_addr;

                                            switch (cdata.m_member_value.m_type)
                                            {
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::INT:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<int>(nullptr))
                                                {
                                                    jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'int', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::FLOAT:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<float>(nullptr))
                                                {
                                                    jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'float', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC2:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec2>(nullptr))
                                                {
                                                    jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec2', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC3:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec3>(nullptr))
                                                {
                                                    jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec3', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC4:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec4>(nullptr))
                                                {
                                                    jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec4', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::QUAT4:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::quat>(nullptr))
                                                {
                                                    jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'quat', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            default:
                                                jeecs::debug::logerr("Bad animation data type(%d) when trying set data of component '%s''s member '%s', please check.",
                                                    (int)cdata.m_member_value.m_type,
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name);
                                                cdata.m_member_addr_cache = nullptr;
                                                break;
                                            }
                                        }
                                        for (auto& cdata : frame.m_component_data)
                                        {
                                            if (cdata.m_member_addr_cache == nullptr)
                                                continue; // Invalid! skip this component.

                                            switch (cdata.m_member_value.m_type)
                                            {
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::INT:
                                                if (cdata.m_offset_mode)
                                                    *(int*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.i32;
                                                else
                                                    *(int*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.i32;
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::FLOAT:
                                                if (cdata.m_offset_mode)
                                                    *(float*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.f32;
                                                else
                                                    *(float*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.f32;
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC2:
                                                if (cdata.m_offset_mode)
                                                    *(math::vec2*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v2;
                                                else
                                                    *(math::vec2*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v2;
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC3:
                                                if (cdata.m_offset_mode)
                                                    *(math::vec3*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v3;
                                                else
                                                    *(math::vec3*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v3;
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC4:
                                                if (cdata.m_offset_mode)
                                                    *(math::vec4*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v4;
                                                else
                                                    *(math::vec4*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v4;
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::QUAT4:
                                                if (cdata.m_offset_mode)
                                                    *(math::quat*)cdata.m_member_addr_cache = *(math::quat*)cdata.m_member_addr_cache * cdata.m_member_value.m_value.q4;
                                                else
                                                    *(math::quat*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.q4;
                                                break;
                                            default:
                                                jeecs::debug::logerr("Bad animation data type(%d) when trying set data of component '%s''s member '%s', please check.",
                                                    (int)cdata.m_member_value.m_type,
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name);
                                                break;
                                            }
                                        }
                                    };

                                    if (animation.m_current_frame_index == SIZE_MAX)
                                    {
                                        animation.m_current_frame_index = 0;
                                        animation.m_next_update_time =
                                            _fixed_time + active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time;
                                    }
                                    else
                                    {
                                        // 到达下一次更新时间！检查间隔时间，并跳转到对应的帧
                                        auto current_animation_frame_count = active_animation_frames->v.frames.size();

                                        auto delta_time_between_frams = _fixed_time - animation.m_next_update_time;
                                        auto next_frame_index = (animation.m_current_frame_index + 1) % current_animation_frame_count;

                                        while (delta_time_between_frams > active_animation_frames->v.frames[next_frame_index].m_frame_time)
                                        {
                                            if (animation.m_loop == false && next_frame_index == 0)
                                                break;

                                            // 在此应用跳过帧的deltaframe数据
                                            update_and_apply_component_frame_data(e, active_animation_frames->v.frames[next_frame_index]);

                                            delta_time_between_frams -= active_animation_frames->v.frames[next_frame_index].m_frame_time;
                                            next_frame_index = (next_frame_index + 1) % current_animation_frame_count;
                                        }

                                        if (animation.m_loop == false && next_frame_index == 0)
                                        {
                                            // 帧动画播放完毕，最后更新到最后一帧，然后终止动画
                                            finish_animation = true;
                                            next_frame_index = current_animation_frame_count - 1;
                                        }

                                        animation.m_current_frame_index = next_frame_index;
                                        animation.m_next_update_time =
                                            _fixed_time + active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time - delta_time_between_frams;
                                    }

                                    auto& updating_frame = active_animation_frames->v.frames[animation.m_current_frame_index];
                                    update_and_apply_component_frame_data(e, updating_frame);

                                    if (shaders != nullptr)
                                    {
                                        for (auto& udata : updating_frame.m_uniform_data)
                                        {
                                            switch (udata.m_uniform_value.m_type)
                                            {
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::INT:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.i32);
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::FLOAT:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.f32);
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC2:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v2);
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC3:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v3);
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC4:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v4);
                                                break;
                                            default:
                                                jeecs::debug::logerr("Bad animation data type(%d) when trying set data of uniform variable '%s', please check.",
                                                    (int)udata.m_uniform_value.m_type,
                                                    udata.m_uniform_name.c_str());
                                                break;
                                            }
                                        }
                                    }

                                    if (finish_animation)
                                    {
                                        // 终止动画
                                        animation.set_action("");
                                    }
                                }
                            }
                            // 如果没有找到对应的动画，那么不做任何事。
                            // 这个注释写在这里单纯是因为花括号写得太难看，稍微避免出现一个大于号
                        }

                        // End
                    }
            );
        }
    };
}

jegl_thread* jedbg_get_editing_graphic_thread(void* universe)
{
    return jeecs::GraphicThreadHost::get_default_graphic_pipeline_instance(universe)->glthread;
}
