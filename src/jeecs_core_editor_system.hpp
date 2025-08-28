#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <optional>
#include <variant>
#include <set>

namespace jeecs
{
    namespace Editor
    {
        struct Name
        {
            basic::string name;
        };
        struct Invisable
        {
            // Entity with this component will not display in editor, and will not be saved.
        };
        struct EditorWalker
        {
            // Walker entity will have a child camera and controled by user.
        };
        struct EntityMover
        {
            enum mover_mode
            {
                NOSPECIFY,
                SELECTION,
                MOVEMENT,
                ROTATION,
                SCALE,
            };
            mover_mode mode = mover_mode::NOSPECIFY;

            // Editor will create a entity with EntityMoverRoot,
            // and DefaultEditorSystem should handle this entity hand create 3 mover for x,y,z axis
            math::vec3 axis = {};
        };
        struct EntitySelectBox
        {
        };
        struct EntityMoverRoot
        {
            bool init = false;
        };
        struct Prefab
        {
            jeecs::basic::string path;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Prefab::path, "path");
            }
        };
        struct EntityId
        {
            jeecs::typing::debug_eid_t eid;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &EntityId::eid, "eid");
            }
        };

        // Used for store uniform vars of failed-shader in entity. used for 'update' shaders
        struct BadShadersUniform
        {
            using uniform_inform = std::map<std::string, jegl_shader::unifrom_variables>;
            struct bad_shader_data
            {
                std::string m_path;
                uniform_inform m_vars;

                bad_shader_data(const std::string path)
                    : m_path(path)
                {
                }
            };

            struct ok_or_bad_shader
            {
                std::variant<bad_shader_data, jeecs::basic::resource<jeecs::graphic::shader>> m_shad;
                ok_or_bad_shader(const bad_shader_data& badshader) : m_shad(badshader)
                {
                }
                ok_or_bad_shader(const jeecs::basic::resource<jeecs::graphic::shader>& okshader) : m_shad(okshader)
                {
                }
                bool is_ok() const
                {
                    return nullptr == std::get_if<bad_shader_data>(&m_shad);
                }
                bad_shader_data& get_bad()
                {
                    return std::get<bad_shader_data>(m_shad);
                }
                jeecs::basic::resource<jeecs::graphic::shader>& get_ok()
                {
                    return std::get<jeecs::basic::resource<jeecs::graphic::shader>>(m_shad);
                }
            };
            std::vector<ok_or_bad_shader> stored_uniforms;
        };
    }

    struct GizmoResources
    {
        static basic::resource<graphic::vertex> _create_circle_vertex(math::vec3 anx)
        {
            auto rot = 90.0f * anx;
            std::swap(rot.x, rot.y);

            math::quat r(rot.x, rot.y, rot.z);

            const size_t half_step_count = 50;

            std::vector<float> points;
            std::vector<uint32_t> indices;
            for (size_t i = 0; i < half_step_count; ++i)
            {
                // x2 + y2 = r2
                // y = + sqrt(r2 - x2)
                float x = (float)i / (float)half_step_count * 2.0f - 1.0f;
                auto pos = r * math::vec3(x, sqrt(1.f - x * x), 0.f);

                points.push_back(pos.x);
                points.push_back(pos.y);
                points.push_back(pos.z);

                points.push_back(anx.x);
                points.push_back(anx.y);
                points.push_back(anx.z);

                indices.push_back((uint32_t)indices.size());
            }
            for (size_t i = 0; i <= half_step_count; ++i)
            {
                // x2 + y2 = r2
                // y = + sqrt(r2 - x2)
                float x = 1.0f - (float)i / (float)half_step_count * 2.0f;
                auto pos = r * math::vec3(x, -sqrt(1.f - x * x), 0.f);

                points.push_back(pos.x);
                points.push_back(pos.y);
                points.push_back(pos.z);

                points.push_back(anx.x);
                points.push_back(anx.y);
                points.push_back(anx.z);

                indices.push_back((uint32_t)indices.size());
            }
            return graphic::vertex::create(
                jegl_vertex::type::LINESTRIP,
                points.data(),
                points.size() * sizeof(float),
                indices,
                {
                    {jegl_vertex::data_type::FLOAT32, 3},
                    {jegl_vertex::data_type::FLOAT32, 3},
                })
                .value();
        }
        static basic::resource<graphic::texture> _create_missing_default_icon()
        {
            basic::resource<graphic::texture> result =
                graphic::texture::create(8, 8, jegl_texture::format::RGBA);

            static const uint8_t _unexist_gizmo_icon[8][8] = {
                {
                    0,
                    0,
                    1,
                    1,
                    1,
                    1,
                    0,
                    0,
                },
                {
                    0,
                    1,
                    0,
                    0,
                    0,
                    0,
                    1,
                    0,
                },
                {
                    0,
                    1,
                    0,
                    0,
                    0,
                    0,
                    1,
                    0,
                },
                {
                    0,
                    0,
                    0,
                    0,
                    0,
                    1,
                    1,
                    0,
                },
                {
                    0,
                    0,
                    0,
                    0,
                    1,
                    0,
                    0,
                    0,
                },
                {
                    0,
                    0,
                    0,
                    1,
                    1,
                    0,
                    0,
                    0,
                },
                {
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                },
                {
                    0,
                    0,
                    0,
                    1,
                    1,
                    0,
                    0,
                    0,
                },
            };

            for (size_t ix = 0; ix < 8; ++ix)
            {
                for (size_t iy = 0; iy < 8; ++iy)
                {
                    if (_unexist_gizmo_icon[7 - iy][ix] == 0)
                        result->pix(ix, iy).set({ 0.f, 0.f, 0.f, 0.f });
                    else
                        result->pix(ix, iy).set({ 1.f, 1.f, 1.f, 0.8f });
                }
            }
            return result;
        };

        basic::resource<graphic::texture> m_camera_icon;
        basic::resource<graphic::texture> m_point_or_shape_light2d_icon;
        basic::resource<graphic::texture> m_parallel_light2d_icon;

        basic::resource<graphic::texture> m_selecting_default_texture;

        basic::resource<graphic::shader> m_gizmo_shader;
        basic::resource<graphic::shader> m_gizmo_camera_visual_cone_shader;
        basic::resource<graphic::shader> m_gizmo_physics2d_collider_shader;
        basic::resource<graphic::shader> m_gizmo_selecting_item_highlight_shader;

        basic::resource<graphic::vertex> m_gizmo_vertex;
        basic::resource<graphic::vertex> m_gizmo_camera_visual_cone_vertex;
        basic::resource<graphic::vertex> m_gizmo_physics2d_collider_box_vertex;
        basic::resource<graphic::vertex> m_gizmo_physics2d_collider_circle_vertex;
        // basic::resource<graphic::vertex> m_gizmo_physics2d_collider_capsule_vertex;

        inline static const float gizmo_vertex_data[] = {
            -0.5f,
            0.5f,
            0.0f,
            0.0f,
            1.0f,
            -0.5f,
            -0.5f,
            0.0f,
            0.0f,
            0.0f,
            0.5f,
            0.5f,
            0.0f,
            1.0f,
            1.0f,
            0.5f,
            -0.5f,
            0.0f,
            1.0f,
            0.0f,
        };
        inline static const float gizmo_camera_visual_cone_vertex_data[] = {
            -1.0f,
            -1.0f,
            -1.0f,
            1.0f,
            -1.0f,
            -1.0f,
            1.0f,
            1.0f,
            -1.0f,
            -1.0f,
            1.0f,
            -1.0f,
            -1.0f,
            -1.0f,
            1.0f,
            1.0f,
            -1.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            -1.0f,
            1.0f,
            1.0f,
        };

        JECS_DISABLE_MOVE_AND_COPY(GizmoResources);

        GizmoResources(jegl_context* glcontext)
            : m_camera_icon{ _create_missing_default_icon() }
            , m_point_or_shape_light2d_icon{ _create_missing_default_icon() }
            , m_parallel_light2d_icon{ _create_missing_default_icon() }
            , m_selecting_default_texture{ 
                graphic::texture::create(1, 1, jegl_texture::format::RGBA) }
            , m_gizmo_shader{ graphic::shader::create("!/builtin/gizmo.shader",                                                                                                                                                                                                                                                                                                               {R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex : float3,
        uv : float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos : float4,
        uv : float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color : float4,
        self_lum: float4,
    };
    
public let vert =
    \v: vin = v2f {
            pos = JE_MVP * vec4!(v.vertex, 1.0),
            uv = v.uv,
        }
        ;
    ;
    
let NearestSampler  = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main        = texture2d::uniform(0, NearestSampler);
    
public let frag =
    \vf: v2f = fout{
            color = texture_color * vec4!(1., 1., 1., 0.8),
            self_lum = vec4!(texture_color->xyz, 1.0),
        }
        where texture_color = alphatest(JE_COLOR * tex2d(Main, vf.uv))
        ;
    ;
)"})
                                                                                                                                                                                                                                                                                                 .value() },
            m_gizmo_camera_visual_cone_shader{ 
                graphic::shader::create("!/builtin/gizmo_camera_visual_cone.shader",
                {R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos : float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color : float4,
        self_lum: float4,
    };
    
WOSHADER_UNIFORM!
    let InverseCameraProjection = float4x4::unit;

public func vert(v: vin)
{
    return v2f {
        pos = JE_MVP * InverseCameraProjection * vec4!(v.vertex, 1.),
        };
}
public func frag(_: v2f)
{
    return fout{
        color = vec4!(1., 1., 1., 0.8),
        self_lum = vec4!(1., 1., 1., 1.0),
    };
}
)"})
                .value() },
            m_gizmo_physics2d_collider_shader{ 
                graphic::shader::create("!/builtin/gizmo_physics2d_collider.shader",
                {R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos : float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color : float4,
        self_lum: float4,
    };
    
public let vert =
    \v: vin = v2f {
            pos = JE_MVP * vec4!(v.vertex, 1.0),
        }
        ;
    ;
public let frag =
    \_: v2f = fout{
            color = vec4!(0., 1., 1., 0.5),
            self_lum = vec4!(0., 1., 1., 1.0),
        }
        ;
    ;
)"})
            .value() }
        , m_gizmo_selecting_item_highlight_shader{
            graphic::shader::create("!/builtin/gizmo_selecting_item_highlight.shader",
            {R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,
        uv      : float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        uv      : float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color       : float4,
        self_lum    : float4,
    };
    
public func vert(v: vin)
{
    return v2f{
        pos = JE_MVP * vec4!(v.vertex, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}

let NearestSampler  = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main        = texture2d::uniform(0, NearestSampler);
    
public func frag(vf: v2f)
{
    let final_color =
        vec4!(
            0.62,
            0.22,
            0.16,
            abs(sin(JE_TIME->x * 2.)) * 0.5 * alphatest(tex2d(Main, vf.uv))->w);
            
    return fout{
        color = final_color,
        self_lum = vec4!(0., 0., 0., 0.),
    };
}
)"})
        .value() }
        , m_gizmo_vertex{ graphic::vertex::create(
                               jegl_vertex::type::TRIANGLESTRIP,
                               gizmo_vertex_data,
                               sizeof(gizmo_vertex_data),
                               {0, 1, 2, 3},
                               {
                                   {jegl_vertex::data_type::FLOAT32, 3},
                                   {jegl_vertex::data_type::FLOAT32, 2},
                               })
                               .value() },
            m_gizmo_camera_visual_cone_vertex{ graphic::vertex::create(jegl_vertex::type::LINESTRIP,
                                                                      gizmo_camera_visual_cone_vertex_data,
                                                                      sizeof(gizmo_camera_visual_cone_vertex_data),
                                                                      {0, 1, 2, 3, 0, 4, 5, 6, 7, 4, 5, 1, 2, 6, 7, 3},
                                                                      {
                                                                          {jegl_vertex::data_type::FLOAT32, 3},
                                                                      })
                                                  .value() },
            m_gizmo_physics2d_collider_box_vertex{ graphic::vertex::create(
                                                      jegl_vertex::type::LINESTRIP,
                                                      gizmo_vertex_data,
                                                      sizeof(gizmo_vertex_data),
                                                      {0, 1, 3, 2, 0},
                                                      {
                                                          {jegl_vertex::data_type::FLOAT32, 3},
                                                          {jegl_vertex::data_type::FLOAT32, 2},
                                                      })
                                                      .value() },
            m_gizmo_physics2d_collider_circle_vertex{ _create_circle_vertex({0.f, 0.f, 1.f}) }
        {
            auto camera_icon = graphic::texture::load(glcontext, "!/builtin/icon/gizmo_camera.png");
            if (camera_icon.has_value())
                m_camera_icon = camera_icon.value();

            auto point_or_shape_light2d_icon = graphic::texture::load(glcontext, "!/builtin/icon/gizmo_point_light2d.png");
            if (point_or_shape_light2d_icon.has_value())
                m_point_or_shape_light2d_icon = point_or_shape_light2d_icon.value();

            auto parallel_light2d_icon = graphic::texture::load(glcontext, "!/builtin/icon/gizmo_parallel_light2d.png");
            if (parallel_light2d_icon.has_value())
                m_parallel_light2d_icon = parallel_light2d_icon.value();

            m_selecting_default_texture->pix(0, 0).set({ 1.f, 1.f, 1.f, 1.f });
        }
    };

    struct DefaultEditorSystem : public game_system
    {
        inline static bool _editor_enabled = true;
        inline static jeecs::typing::debug_eid_t _allocate_eid = 0;

        enum coord_mode
        {
            GLOBAL,
            LOCAL
        };
        enum gizmo_mode
        {
            NONE = 0,

            CAMERA = 0b0000'0001,
            CAMERA_VISUAL_CONE = 0b0000'0010,
            LIGHT2D = 0b0000'0100,
            PHYSICS2D_COLLIDER = 0b0000'1000,
            SELECTING_HIGHLIGHT = 0b0001'0000,

            ALL = 0x7FFFFFFF,
        };

        graphic_uhost* _graphic_uhost;
        rendchain_branch* _gizmo_draw_branch;

        // _gizmo_resources must defined after _graphic_uhost
        // to make sure init seq.
        GizmoResources _gizmo_resources;

        Editor::EntityMover::mover_mode _mode = Editor::EntityMover::mover_mode::SELECTION;
        coord_mode _coord = coord_mode::GLOBAL;
        int _gizmo_mask = gizmo_mode::ALL;

        basic::resource<graphic::vertex> axis_x;
        basic::resource<graphic::vertex> axis_y;
        basic::resource<graphic::vertex> axis_z;
        basic::resource<graphic::vertex> circ_x;
        basic::resource<graphic::vertex> circ_y;
        basic::resource<graphic::vertex> circ_z;

        math::vec3 _camera_pos;
        math::quat _camera_rot;
        math::ray _camera_ray;

        const Camera::Projection* _camera_porjection = nullptr;
        const Camera::OrthoProjection* _camera_ortho_porjection = nullptr;
        bool _camera_is_in_o2d_mode = false;

        inline static constexpr float MOUSE_MOVEMENT_SCALE = 0.005f;
        inline static constexpr float MOUSE_ROTATION_SCALE = 0.1f;

        struct input_msg
        {
            bool w = false;
            bool s = false;
            bool a = false;
            bool d = false;

            bool l_tab = false;

            bool l_ctrl = false;
            bool l_shift = false;

            bool l_button = false;
            bool r_button = false;

            bool l_button_click = false;
            bool l_button_pushed = false;
            bool r_button_click = false;
            bool r_button_pushed = false;

            float delta_time = 0.0f;

            bool _drag_viewing = false;
            math::vec2 _last_drag_mouse_pos = {};
            jeecs::math::vec2 _next_drag_mouse_pos = {};
            jeecs::math::vec2 current_mouse_pos = {};

            int _wheel_count_record = INT_MAX;
            int wheel_delta_count = 0;

            bool advise_lock_mouse_walking_camera = false;
            const Transform::Translation* _grab_axis_translation = nullptr;

            // Why write an empty constructor function here?
            // It's a bug of clang/gcc, fuck!
            input_msg() noexcept {}

            // selected_entity 用于储存当前被选择的实体，仅用于编辑窗口中，可能为空
            std::optional<jeecs::game_entity> selected_entity;
        };

        inline static input_msg _inputs = {};

        inline static const float axis_x_data[] = {
            -1.f, 0.f, 0.f, 0.25f, 0.f, 0.f,
            1.f, 0.f, 0.f, 1.f, 0.f, 0.f };
        inline static const float axis_y_data[] = {
            0.f,
            -1.f,
            0.f,
            0.f,
            0.25f,
            0.f,
            0.f,
            1.f,
            0.f,
            0.f,
            1.f,
            0.f,
        };
        inline static const float axis_z_data[] = {
            0.f, 0.f, -1.f, 0.f, 0.f, 0.25f,
            0.f, 0.f, 1.f, 0.f, 0.f, 1.f };

        DefaultEditorSystem(game_world w)
            : game_system(w)
            , _graphic_uhost(jegl_uhost_get_or_create_for_universe(w.get_universe().handle(), nullptr))
            , _gizmo_resources(jegl_uhost_get_context(_graphic_uhost))
            , axis_x{ graphic::vertex::create(jegl_vertex::type::LINESTRIP,
                                            axis_x_data,
                                            sizeof(axis_x_data),
                                            {0, 1},
                                            {
                                                {jegl_vertex::data_type::FLOAT32, 3},
                                                {jegl_vertex::data_type::FLOAT32, 3},
                                            })
                        .value() },
            axis_y{ graphic::vertex::create(jegl_vertex::type::LINESTRIP,
                                           axis_y_data,
                                           sizeof(axis_y_data),
                                           {0, 1},
                                           {
                                               {jegl_vertex::data_type::FLOAT32, 3},
                                               {jegl_vertex::data_type::FLOAT32, 3},
                                           })
                       .value() },
            axis_z{ graphic::vertex::create(jegl_vertex::type::LINESTRIP,
                                           axis_z_data,
                                           sizeof(axis_z_data),
                                           {0, 1},
                                           {
                                               {jegl_vertex::data_type::FLOAT32, 3},
                                               {jegl_vertex::data_type::FLOAT32, 3},
                                           })
                       .value() },
            circ_x{ GizmoResources::_create_circle_vertex({1.f, 0.f, 0.f}) }
            , circ_y{ GizmoResources::_create_circle_vertex({0.f, 1.f, 0.f}) }
            , circ_z{ GizmoResources::_create_circle_vertex({0.f, 0.f, 1.f}) }
        {
            _gizmo_draw_branch = jegl_uhost_alloc_branch(_graphic_uhost);

            const float selector_size = 0.1f;

            circ_x->resource()->m_raw_vertex_data->m_y_min += -selector_size;
            circ_x->resource()->m_raw_vertex_data->m_y_max += selector_size;
            circ_x->resource()->m_raw_vertex_data->m_z_min += -selector_size;
            circ_x->resource()->m_raw_vertex_data->m_z_max += selector_size;

            circ_y->resource()->m_raw_vertex_data->m_x_min += -selector_size;
            circ_y->resource()->m_raw_vertex_data->m_x_max += selector_size;
            circ_y->resource()->m_raw_vertex_data->m_z_min += -selector_size;
            circ_y->resource()->m_raw_vertex_data->m_z_max += selector_size;

            circ_z->resource()->m_raw_vertex_data->m_x_min += -selector_size;
            circ_z->resource()->m_raw_vertex_data->m_x_max += selector_size;
            circ_z->resource()->m_raw_vertex_data->m_y_min += -selector_size;
            circ_z->resource()->m_raw_vertex_data->m_y_max += selector_size;

            circ_x->resource()->m_raw_vertex_data->m_x_min = circ_y->resource()->m_raw_vertex_data->m_y_min = circ_z->resource()->m_raw_vertex_data->m_z_min = axis_x->resource()->m_raw_vertex_data->m_y_min = axis_x->resource()->m_raw_vertex_data->m_z_min = axis_y->resource()->m_raw_vertex_data->m_x_min = axis_y->resource()->m_raw_vertex_data->m_z_min = axis_z->resource()->m_raw_vertex_data->m_x_min = axis_z->resource()->m_raw_vertex_data->m_y_min = -selector_size;

            circ_x->resource()->m_raw_vertex_data->m_x_max = circ_y->resource()->m_raw_vertex_data->m_y_max = circ_z->resource()->m_raw_vertex_data->m_z_max = axis_x->resource()->m_raw_vertex_data->m_y_max = axis_x->resource()->m_raw_vertex_data->m_z_max = axis_y->resource()->m_raw_vertex_data->m_x_max = axis_y->resource()->m_raw_vertex_data->m_z_max = axis_z->resource()->m_raw_vertex_data->m_x_max = axis_z->resource()->m_raw_vertex_data->m_y_max = selector_size;
        }
        ~DefaultEditorSystem()
        {
            jegl_uhost_free_branch(_graphic_uhost, _gizmo_draw_branch);
        }

        struct SelectedResult
        {
            float distance;
            jeecs::game_entity entity;

            bool operator<(const SelectedResult& s) const noexcept
            {
                return distance < s.distance;
            }
        };
        std::multiset<SelectedResult> selected_list;

        void MoveWalker(Transform::LocalPosition& position, Transform::LocalRotation& rotation, Transform::Translation& trans)
        {
            if (!_editor_enabled)
                return;

            using namespace input;
            using namespace math;

            if (_inputs.r_button_pushed)
            {
                _inputs._drag_viewing = false;
            }

            if (_inputs.r_button)
            {
                float move_speed = 5.0f;
                if (_inputs.l_ctrl)
                    move_speed = move_speed / 2.0f;
                if (_inputs.l_shift)
                    move_speed = move_speed * 2.0f;

                auto delta_drag = _inputs.current_mouse_pos - _inputs._last_drag_mouse_pos;
                if (_inputs._drag_viewing || delta_drag.length() >= 5.f)
                {
                    _inputs._drag_viewing = true;

                    if (_camera_is_in_o2d_mode)
                    {
                        assert(_camera_ortho_porjection != nullptr);

                        move_speed /= _camera_ortho_porjection->scale * 0.5f;

                        position.pos -= move_speed * vec3(delta_drag.x, -delta_drag.y, 0.0) * MOUSE_MOVEMENT_SCALE;
                        rotation.rot = quat();
                    }
                    else
                    {
                        _inputs.advise_lock_mouse_walking_camera = true;
                        rotation.rot = rotation.rot * quat(0, MOUSE_ROTATION_SCALE * delta_drag.x, 0);
                    }
                }

                if (_inputs.w)
                    position.pos += _camera_rot * vec3(0, 0, move_speed * _inputs.delta_time);
                if (_inputs.s)
                    position.pos += _camera_rot * vec3(0, 0, -move_speed * _inputs.delta_time);
                if (_inputs.a)
                    position.pos += _camera_rot * vec3(-move_speed * _inputs.delta_time, 0, 0);
                if (_inputs.d)
                    position.pos += _camera_rot * vec3(move_speed * _inputs.delta_time, 0, 0);
            }
            else
                _inputs.advise_lock_mouse_walking_camera = false;
        }

        void SelectEntity(game_entity entity, Transform::Translation& trans, Renderer::Shape* shape)
        {
            if (!_editor_enabled)
                return;

            if (_inputs.l_button_pushed)
            {
                auto result = shape == nullptr
                    ? _camera_ray.intersect_box(trans.world_position, math::vec3(1.f, 1.f, 1.f), trans.world_rotation)
                    : _camera_ray.intersect_entity(trans, shape, false);

                if (result.intersected)
                    selected_list.insert(SelectedResult{ result.distance, entity });
            }
        }

        void UpdateAndCreateMover(game_entity mover_entity,
            Transform::Anchor& anchor,
            Transform::LocalPosition& position,
            Transform::LocalRotation& rotation,
            Transform::LocalScale& scale,
            Transform::Translation& trans,
            Editor::EntityMoverRoot& mover)
        {
            if (!mover.init)
            {
                mover.init = true;

                const float select_box_vert_data[] = {
                    -0.5f,
                    -0.5f,
                    -0.5f,
                    0.5f,
                    -0.5f,
                    -0.5f,
                    0.5f,
                    0.5f,
                    -0.5f,
                    -0.5f,
                    0.5f,
                    -0.5f,
                    -0.5f,
                    -0.5f,
                    0.5f,
                    0.5f,
                    -0.5f,
                    0.5f,
                    0.5f,
                    0.5f,
                    0.5f,
                    -0.5f,
                    0.5f,
                    0.5f,
                };
                basic::resource<graphic::vertex> select_box_vert =
                    graphic::vertex::create(jegl_vertex::type::LINESTRIP,
                        select_box_vert_data,
                        sizeof(select_box_vert_data),
                        { 0, 1, 2, 3, 0, 4, 5, 6, 7, 4, 5, 1, 2, 6, 7, 3 },
                                            {
                                                {jegl_vertex::data_type::FLOAT32, 3},
                                            })
                                            .value();

                basic::resource<graphic::shader>
                    axis_shader = graphic::shader::create("!/builtin/mover_axis.shader",
                        { R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex : float3,
        color  : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos : float4,
        color : float3,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color : float4,
        self_lum: float4,
    };
    
public let vert =
    \v: vin = v2f {
            pos = JE_MVP * vec4!(v.vertex, 1.),
            color = v.color
            }
        ;
    ;
public let frag =
    \f: v2f = fout{
            color = vec4!(show_color, 1.),
            self_lum = vec4!(show_color * 100., 1.),
        }
        where show_color = lerp(f.color, vec3!(1., 1., 1.), vec3!(ratio, ratio, ratio))
            , ratio = step(0.5, JE_COLOR->x)
        ;
    ;
        )" })
                    .value();
                basic::resource<graphic::shader>
                    select_box_shader = graphic::shader::create("!/builtin/select_box.shader",
                        { R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos : float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color : float4,
        self_lum: float4,
    };
    
public let vert =
    \v: vin = v2f{
            pos = JE_MVP * vec4!(v.vertex, 1.)
            }
        ;
    ;
public let frag =
    \_: v2f = fout{
            color = vec4!(0.5, 1., 0.5, 1.),
            self_lum = vec4!(1., 100., 1., 1.),
        }
        ;
    ;
        )" })
                    .value();

                game_world current_world = mover_entity.game_world();
                game_entity axis_x_e = current_world.add_entity<
                    Transform::LocalPosition,
                    Transform::LocalScale,
                    Transform::LocalToParent,
                    Transform::Translation,
                    Renderer::Shaders,
                    Renderer::Shape,
                    Renderer::Rendqueue,
                    Renderer::Color,
                    Editor::Invisable,
                    Editor::EntityMover>();
                game_entity axis_y_e = current_world.add_entity<
                    Transform::LocalPosition,
                    Transform::LocalScale,
                    Transform::LocalToParent,
                    Transform::Translation,
                    Renderer::Shaders,
                    Renderer::Shape,
                    Renderer::Rendqueue,
                    Renderer::Color,
                    Editor::Invisable,
                    Editor::EntityMover>();
                game_entity axis_z_e = current_world.add_entity<
                    Transform::LocalPosition,
                    Transform::LocalScale,
                    Transform::LocalToParent,
                    Transform::Translation,
                    Renderer::Shaders,
                    Renderer::Shape,
                    Renderer::Rendqueue,
                    Renderer::Color,
                    Editor::Invisable,
                    Editor::EntityMover>();

                game_entity select_box = current_world.add_entity<
                    Transform::LocalRotation,
                    Transform::LocalPosition,
                    Transform::LocalScale,
                    Transform::LocalToParent,
                    Transform::Translation,
                    Renderer::Shaders,
                    Renderer::Shape,
                    Renderer::Rendqueue,
                    Editor::Invisable,
                    Editor::EntitySelectBox>();

                axis_x_e.get_component<Renderer::Shaders>()->shaders.push_back(axis_shader);
                axis_y_e.get_component<Renderer::Shaders>()->shaders.push_back(axis_shader);
                axis_z_e.get_component<Renderer::Shaders>()->shaders.push_back(axis_shader);
                select_box.get_component<Renderer::Shaders>()->shaders.push_back(select_box_shader);

                axis_x_e.get_component<Editor::EntityMover>()->axis = math::vec3(1.f, 0, 0);
                axis_y_e.get_component<Editor::EntityMover>()->axis = math::vec3(0, 1.f, 0);
                axis_z_e.get_component<Editor::EntityMover>()->axis = math::vec3(0, 0, 1.f);

                axis_x_e.get_component<Renderer::Shape>()->vertex = axis_x;
                axis_y_e.get_component<Renderer::Shape>()->vertex = axis_y;
                axis_z_e.get_component<Renderer::Shape>()->vertex = axis_z;
                select_box.get_component<Renderer::Shape>()->vertex = select_box_vert;

                select_box.get_component<Renderer::Rendqueue>()->rend_queue =
                    axis_x_e.get_component<Renderer::Rendqueue>()->rend_queue =
                    axis_y_e.get_component<Renderer::Rendqueue>()->rend_queue =
                    axis_z_e.get_component<Renderer::Rendqueue>()->rend_queue = 100000;

                axis_x_e.get_component<Transform::LocalPosition>()->pos = math::vec3(1.f, 0, 0);
                axis_y_e.get_component<Transform::LocalPosition>()->pos = math::vec3(0, 1.f, 0);
                axis_z_e.get_component<Transform::LocalPosition>()->pos = math::vec3(0, 0, 1.f);

                select_box.get_component<Transform::LocalToParent>()->parent_uid =
                    axis_x_e.get_component<Transform::LocalToParent>()->parent_uid =
                    axis_y_e.get_component<Transform::LocalToParent>()->parent_uid =
                    axis_z_e.get_component<Transform::LocalToParent>()->parent_uid =
                    anchor.uid;
            }
            if (const game_entity* current = _inputs.selected_entity ? &_inputs.selected_entity.value() : nullptr)
            {
                if (auto* trans = current->get_component<Transform::Translation>())
                {
                    position.pos = trans->world_position;

                    if (_coord == coord_mode::LOCAL || _mode == Editor::EntityMover::mover_mode::SCALE)
                        rotation.rot = trans->world_rotation;
                    else
                        rotation.rot = math::quat();
                }
            }
        }

        void MoveEntity(
            Editor::EntityMover& mover,
            Transform::Translation& trans,
            Transform::LocalPosition& posi,
            Transform::LocalScale& scale,
            Renderer::Shape* shape,
            Renderer::Color& color)
        {
            if (mover.mode != _mode || _inputs.l_tab)
            {
                if (_inputs.l_tab)
                    mover.mode = jeecs::Editor::EntityMover::SELECTION;
                else
                    mover.mode = _mode;
                switch (mover.mode)
                {
                case jeecs::Editor::EntityMover::SELECTION:
                    scale.scale = { 0.f, 0.f, 0.f };
                    break;
                case jeecs::Editor::EntityMover::ROTATION:
                    if (mover.axis.x != 0.f)
                        shape->vertex = circ_x;
                    else if (mover.axis.y != 0.f)
                        shape->vertex = circ_y;
                    else
                        shape->vertex = circ_z;
                    posi.pos = { 0.f, 0.f, 0.f };
                    scale.scale = { 1.f, 1.f, 1.f };
                    break;
                case jeecs::Editor::EntityMover::MOVEMENT:
                case jeecs::Editor::EntityMover::SCALE:
                    if (mover.axis.x != 0.f)
                        shape->vertex = axis_x;
                    else if (mover.axis.y != 0.f)
                        shape->vertex = axis_y;
                    else
                        shape->vertex = axis_z;
                    scale.scale = { 1.f, 1.f, 1.f };
                    break;
                default:
                    jeecs::debug::logerr("Unknown mode.");
                    break;
                }

                if (mover.mode != jeecs::Editor::EntityMover::ROTATION)
                    posi.pos = mover.axis;
            }
            if (mover.mode == Editor::EntityMover::SELECTION)
                return;

            if (!_editor_enabled)
                return;

            const auto* editing_entity = _inputs.selected_entity ? &_inputs.selected_entity.value() : nullptr;
            Transform::LocalPosition* editing_pos_may_null = editing_entity
                ? editing_entity->get_component<Transform::LocalPosition>()
                : nullptr;
            Transform::Translation* editing_trans = editing_entity
                ? editing_entity->get_component<Transform::Translation>()
                : nullptr;
            Transform::LocalRotation* editing_rot_may_null = editing_entity
                ? editing_entity->get_component<Transform::LocalRotation>()
                : nullptr;
            Transform::LocalScale* editing_scale_may_null = editing_entity
                ? editing_entity->get_component<Transform::LocalScale>()
                : nullptr;

            if (_inputs.r_button || !_inputs.l_button || nullptr == editing_trans)
                _inputs._grab_axis_translation = nullptr;

            if (_inputs._grab_axis_translation && _inputs.l_button && editing_trans)
            {
                if (_inputs._grab_axis_translation == &trans && _camera_porjection)
                {
                    math::vec2 diff =
                        (_inputs.current_mouse_pos - _inputs._last_drag_mouse_pos) * math::vec2(1.f, -1.f) * MOUSE_MOVEMENT_SCALE;

                    math::vec4 p0 = trans.world_position;
                    p0.w = 1.0f;
                    p0 = math::mat4trans(
                        _camera_porjection->projection,
                        math::mat4trans(_camera_porjection->view, p0));

                    math::vec4 p1 = trans.world_position + trans.world_rotation * mover.axis;

                    p1.w = 1.0f;
                    p1 = math::mat4trans(
                        _camera_porjection->projection,
                        math::mat4trans(_camera_porjection->view, p1));

                    math::vec2 screen_axis = { p1.x - p0.x, p1.y - p0.y };
                    screen_axis = screen_axis.unit();

                    float factor = 1.0f;
                    if (_inputs.l_ctrl)
                        factor *= 0.5f;
                    if (_inputs.l_shift)
                        factor *= 2.0f;

                    float distance =
                        _camera_ortho_porjection == nullptr
                        ? (_camera_pos - trans.world_position).length()
                        : 5.0f / _camera_ortho_porjection->scale;

                    if (mover.mode == Editor::EntityMover::mover_mode::MOVEMENT && editing_pos_may_null)
                    {
                        editing_trans->set_global_position(
                            editing_trans->world_position + diff.dot(
                                screen_axis) *
                            (trans.world_rotation * (mover.axis * distance * factor)),
                            editing_pos_may_null,
                            editing_rot_may_null);
                    }
                    else if (mover.mode == Editor::EntityMover::mover_mode::SCALE && editing_scale_may_null)
                    {
                        editing_scale_may_null->scale += diff.dot(screen_axis) * (mover.axis * distance * factor);
                    }
                    else if (mover.mode == Editor::EntityMover::mover_mode::ROTATION && editing_rot_may_null)
                    {
                        auto euler = trans.world_rotation * mover.axis * (diff.x + diff.y) * factor * 20.0f;
                        editing_rot_may_null->rot = math::quat(euler.x, euler.y, euler.z) * editing_rot_may_null->rot;
                    }
                }
            }
            else
            {
                auto result = _camera_ray.intersect_entity(trans, shape, false);
                bool select_click = _inputs.l_button_pushed;

                bool intersected = result.intersected;
                if (intersected && mover.mode == Editor::EntityMover::mover_mode::ROTATION)
                {
                    float distance =
                        _camera_ortho_porjection == nullptr
                        ? 0.25f * (_camera_pos - trans.world_position).length()
                        : 1.0f / _camera_ortho_porjection->scale;

                    auto dist = 1.f - ((result.place - trans.world_position) / distance).length();

                    if (abs(dist) > 0.2f)
                        intersected = false;
                }

                if (intersected)
                {
                    if (select_click)
                        _inputs._grab_axis_translation = &trans;

                    if (!_inputs.l_button)
                        color.color.x = 1.0f;
                }
                else
                    color.color.x = 0.0f;
            }

            if (const game_entity* current = _inputs.selected_entity ? &_inputs.selected_entity.value() : nullptr)
            {
                if (auto* etrans = current->get_component<Transform::Translation>())
                {
                    float distance =
                        _camera_ortho_porjection == nullptr
                        ? 0.25f * (_camera_pos - etrans->world_position).length()
                        : 1.0f / _camera_ortho_porjection->scale;
                    scale.scale = math::vec3(distance, distance, distance);

                    if (_mode != Editor::EntityMover::mover_mode::ROTATION)
                        posi.pos = mover.axis * distance;
                }
            }
            else
            {
                scale.scale = math::vec3(0.f, 0.f, 0.f);
            }
        }

        void StateUpdate(jeecs::selector& selector)
        {
            _inputs.w = input::keydown(input::keycode::W);
            _inputs.s = input::keydown(input::keycode::S);
            _inputs.a = input::keydown(input::keycode::A);
            _inputs.d = input::keydown(input::keycode::D);
            _inputs.l_tab = input::keydown(input::keycode::TAB);
            _inputs.l_ctrl = input::keydown(input::keycode::L_CTRL);
            _inputs.l_shift = input::keydown(input::keycode::L_SHIFT);
            _inputs.l_button = input::mousedown(0, input::mousecode::LEFT);
            _inputs.r_button = input::mousedown(0, input::mousecode::RIGHT);
            _inputs.l_button_click = input::is_up(_inputs.l_button);
            _inputs.l_button_pushed = input::first_down(_inputs.l_button);
            _inputs.r_button_click = input::is_up(_inputs.r_button);
            _inputs.r_button_pushed = input::first_down(_inputs.r_button);
            _inputs.selected_entity = std::nullopt;
            _inputs.delta_time = deltatime();

            if (_inputs._wheel_count_record != INT_MAX)
            {
                _inputs.wheel_delta_count = (int)input::wheel(0).y - _inputs._wheel_count_record;
            }
            _inputs._wheel_count_record = (int)input::wheel(0).y;

            // 获取被选中的实体
            selector.exec([this](game_entity e, Editor::EntityId* eid)
                {
                    if (eid == nullptr)
                    {
                        auto* ec = e.add_component<Editor::EntityId>();
                        if (ec != nullptr)
                            ec->eid = ++this->_allocate_eid;
                    }
                    else
                    {
                        if (eid->eid == jedbg_get_editing_entity_uid())
                            _inputs.selected_entity = std::optional(e);
                    }
                });

            // Move walker(root)
            selector.contains<Editor::EditorWalker>();
            selector.except<Camera::Projection>();
            selector.exec(&DefaultEditorSystem::MoveWalker);

            struct EditorGizmoContext
            {
                basic::resource<graphic::framebuffer> m_framebuffer;
                Transform::Translation* m_translation;
                Camera::Projection* m_projection;
            };
            std::optional<EditorGizmoContext> enable_draw_gizmo_at_framebuf = std::nullopt;

            // Move walker(camera)
            selector.contains<Editor::EditorWalker>();
            selector.exec([this, &enable_draw_gizmo_at_framebuf](
                Transform::LocalRotation& rotation,
                Camera::Projection& proj,
                Transform::Translation& trans,
                Camera::RendToFramebuffer& r2b,
                Camera::OrthoProjection* o2d)
                {
                    if (!r2b.framebuffer.has_value())
                        return;

                    enable_draw_gizmo_at_framebuf = EditorGizmoContext{
                           r2b.framebuffer.value(),
                           &trans,
                           &proj,
                    };

                    if (!_editor_enabled)
                        return;

                    using namespace input;
                    using namespace math;

                    auto view_space_width = r2b.framebuffer.value()->width();
                    auto view_space_height = r2b.framebuffer.value()->height();

                    auto uniform_mouse_x = 2.0f * ((float)_inputs.current_mouse_pos.x / (float)view_space_width - 0.5f);
                    auto uniform_mouse_y = -2.0f * ((float)_inputs.current_mouse_pos.y / (float)view_space_height - 0.5f);

                    if ((_camera_is_in_o2d_mode = o2d != nullptr))
                    {
                        o2d->scale = o2d->scale * pow(2.0f, (float)_inputs.wheel_delta_count);
                        rotation.rot = quat();
                    }

                    _camera_ray = math::ray(
                        trans,
                        proj,
                        math::vec2(uniform_mouse_x, uniform_mouse_y),
                        _camera_is_in_o2d_mode);

                    _camera_porjection = &proj;
                    _camera_ortho_porjection = o2d;

                    if (_inputs._drag_viewing && _inputs.r_button)
                    {
                        if (!_camera_is_in_o2d_mode)
                        {
                            auto delta_drag = _inputs.current_mouse_pos - _inputs._last_drag_mouse_pos;
                            rotation.rot = rotation.rot * quat(MOUSE_ROTATION_SCALE * delta_drag.y, 0, 0);
                        }

                        _camera_rot = trans.world_rotation;
                        _camera_pos = trans.world_position;
                    }
                });

            /////////////////////////////////////////////////////////////////////////
            // Prepare for gizmo drawing.
            jegl_rendchain* gizmo_rchain = nullptr;
            if (enable_draw_gizmo_at_framebuf.has_value())
            {
                auto& gizmo_context = enable_draw_gizmo_at_framebuf.value();

                jegl_branch_new_frame(_gizmo_draw_branch, graphic::EDITOR_GIZMO_BRANCH_QUEUE);
                gizmo_rchain = jegl_branch_new_chain(
                    _gizmo_draw_branch,
                    gizmo_context.m_framebuffer->resource(),
                    0,
                    0,
                    0,
                    0);

                jegl_rchain_bind_uniform_buffer(
                    gizmo_rchain,
                    gizmo_context.m_projection->default_uniform_buffer->resource());
            }

#define JE_CHECK_NEED_AND_SET_UNIFORM(ACTION, UNIFORM, ITEM, TYPE, ...)                                      \
    do                                                                                                       \
    {                                                                                                        \
        if (UNIFORM->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)                                     \
            jegl_rchain_set_builtin_uniform_##TYPE(ACTION, &UNIFORM->m_builtin_uniform_##ITEM, __VA_ARGS__); \
    } while (0)

            auto easy_draw_impl = [&](
                const math::vec3& postion,
                const math::quat& rotation,
                const math::vec3& scale,
                jegl_resource* shader,
                jegl_resource* vertex,
                jegl_rchain_texture_group_idx_t group) -> jegl_rendchain_rend_action*
                {
                    if (enable_draw_gizmo_at_framebuf.has_value())
                    {
                        float MAT4_GIZMO_M[4][4] = {};
                        float MAT4_GIZMO_MV[4][4] = {};
                        float MAT4_GIZMO_MVP[4][4] = {};

                        auto& gizmo_context = enable_draw_gizmo_at_framebuf.value();

                        auto draw_action = jegl_rchain_draw(
                            gizmo_rchain,
                            shader,
                            vertex,
                            group);

                        auto* builtin_uniform = &shader->m_raw_shader_data->m_builtin_uniforms;

                        math::transform(MAT4_GIZMO_M, postion, rotation, scale);

                        math::mat4xmat4(MAT4_GIZMO_MVP, gizmo_context.m_projection->view_projection, MAT4_GIZMO_M);
                        math::mat4xmat4(MAT4_GIZMO_MV, gizmo_context.m_projection->view, MAT4_GIZMO_M);

                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, builtin_uniform, m, float4x4, MAT4_GIZMO_M);
                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, builtin_uniform, mvp, float4x4, MAT4_GIZMO_MVP);
                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, builtin_uniform, mv, float4x4, MAT4_GIZMO_MV);

                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, builtin_uniform, local_scale, float3,
                            scale.x,
                            scale.y,
                            scale.z);

                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, builtin_uniform, tiling, float2, 1.0f, 1.0f);
                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, builtin_uniform, offset, float2, 0.0f, 0.0f);

                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, builtin_uniform, color, float4,
                            1.0f, 1.0f, 1.0f, 1.0f);

                        return draw_action;
                    }
                    return nullptr;
                };
            auto draw_easy_gizmo_impl = [&](Transform::Translation& trans, jegl_rchain_texture_group_idx_t group, bool rotation)
                {
                    if (enable_draw_gizmo_at_framebuf.has_value())
                    {
                        auto& gizmo_context = enable_draw_gizmo_at_framebuf.value();

                        auto final_rotation = gizmo_context.m_translation->world_rotation;

                        if (rotation)
                            final_rotation = final_rotation * math::quat::euler(0.f, 0.f, trans.world_rotation.euler_angle().z);

                        easy_draw_impl(
                            trans.world_position,
                            final_rotation,
                            math::vec3(1.f, 1.f, 1.f),
                            _gizmo_resources.m_gizmo_shader->resource(),
                            _gizmo_resources.m_gizmo_vertex->resource(),
                            group);
                    }
                };

            auto camera_gizmo_texture_group =
                enable_draw_gizmo_at_framebuf.has_value() ? jegl_rchain_allocate_texture_group(gizmo_rchain) : 0;
            auto point_light_gizmo_texture_group =
                enable_draw_gizmo_at_framebuf.has_value() ? jegl_rchain_allocate_texture_group(gizmo_rchain) : 0;
            auto parallel_light_gizmo_texture_group =
                enable_draw_gizmo_at_framebuf.has_value() ? jegl_rchain_allocate_texture_group(gizmo_rchain) : 0;

            if (enable_draw_gizmo_at_framebuf.has_value())
            {
                jegl_rchain_bind_texture(
                    gizmo_rchain,
                    camera_gizmo_texture_group,
                    0,
                    _gizmo_resources.m_camera_icon->resource());
                jegl_rchain_bind_texture(
                    gizmo_rchain,
                    point_light_gizmo_texture_group,
                    0,
                    _gizmo_resources.m_point_or_shape_light2d_icon->resource());
                jegl_rchain_bind_texture(
                    gizmo_rchain,
                    parallel_light_gizmo_texture_group,
                    0,
                    _gizmo_resources.m_parallel_light2d_icon->resource());
            }

            selector.except<Editor::Invisable>();
            selector.anyof<Camera::OrthoProjection, Camera::PerspectiveProjection>();
            selector.exec([&](game_entity e, Transform::Translation& trans, Camera::Projection& proj)
                {
                    if (_gizmo_mask & gizmo_mode::CAMERA)
                    {
                        SelectEntity(e, trans, nullptr);
                        draw_easy_gizmo_impl(trans, camera_gizmo_texture_group, false);
                    }

                    if (_gizmo_mask & gizmo_mode::CAMERA_VISUAL_CONE
                        && this->_inputs.selected_entity.has_value()
                        && e == this->_inputs.selected_entity.value())
                    {
                        auto* draw_action = easy_draw_impl(
                            trans.world_position,
                            trans.world_rotation,
                            math::vec3(1.0f, 1.0f, 1.0f),
                            _gizmo_resources.m_gizmo_camera_visual_cone_shader->resource(),
                            _gizmo_resources.m_gizmo_camera_visual_cone_vertex->resource(),
                            SIZE_MAX);

                        if (draw_action != nullptr)
                        {
                            auto* location_addr =
                                _gizmo_resources.m_gizmo_camera_visual_cone_shader->
                                get_uniform_location_as_builtin("InverseCameraProjection");

                            jegl_rchain_set_builtin_uniform_float4x4(
                                draw_action,
                                location_addr,
                                proj.inv_projection);
                        }
                    }
                });

            selector.except<Editor::Invisable>();
            selector.anyof<Light2D::Point, Light2D::Range>();
            selector.exec([&](game_entity e, Transform::Translation& trans)
                {
                    if (_gizmo_mask & gizmo_mode::LIGHT2D)
                    {
                        SelectEntity(e, trans, nullptr);
                        draw_easy_gizmo_impl(trans, point_light_gizmo_texture_group, false);
                    }
                });

            selector.except<Editor::Invisable>();
            selector.contains<Light2D::Parallel>();
            selector.exec([&](game_entity e, Transform::Translation& trans)
                {
                    if (_gizmo_mask & gizmo_mode::LIGHT2D)
                    {
                        SelectEntity(e, trans, nullptr);
                        draw_easy_gizmo_impl(trans, parallel_light_gizmo_texture_group, true);
                    }
                });

            selector.except<Editor::Invisable>();
            selector.anyof<
                Physics2D::Collider::Box,
                Physics2D::Collider::Circle,
                Physics2D::Collider::Capsule>();
            selector.exec([&](
                Transform::Translation& trans,
                Physics2D::Transform::Position* ppos,
                Physics2D::Transform::Rotation* prot,
                Physics2D::Transform::Scale* pscale,
                Physics2D::Collider::Box* box,
                Physics2D::Collider::Capsule* capsule,
                Physics2D::Collider::Circle* circle)
                {
                    if (_gizmo_mask & gizmo_mode::PHYSICS2D_COLLIDER)
                    {
                        auto final_world_position = trans.world_position;
                        if (ppos != nullptr)
                            final_world_position += math::vec3(ppos->offset);

                        auto final_world_rotation = trans.world_rotation;
                        if (prot != nullptr)
                            final_world_rotation = final_world_rotation * math::quat::euler(0.f, 0.f, prot->angle);

                        auto final_local_scale = trans.local_scale;
                        if (pscale != nullptr)
                            // We don't care about z of sacle.
                            final_local_scale = final_local_scale * math::vec3(pscale->scale);

                        if (box != nullptr)
                            easy_draw_impl(
                                final_world_position,
                                final_world_rotation,
                                final_local_scale,
                                _gizmo_resources.m_gizmo_physics2d_collider_shader->resource(),
                                _gizmo_resources.m_gizmo_physics2d_collider_box_vertex->resource(),
                                SIZE_MAX);
                        else if (circle != nullptr)
                        {
                            // m_gizmo_physics2d_collider_circle_vertex's R is 1.0f, so we need to scale it.
                            final_local_scale.x = std::max(final_local_scale.x, final_local_scale.y) / 2.0f;
                            final_local_scale.y = final_local_scale.x;

                            easy_draw_impl(
                                final_world_position,
                                final_world_rotation,
                                final_local_scale,
                                _gizmo_resources.m_gizmo_physics2d_collider_shader->resource(),
                                _gizmo_resources.m_gizmo_physics2d_collider_circle_vertex->resource(),
                                SIZE_MAX);
                        }
                        else if (capsule != nullptr)
                        {
                            // We need to draw 2 circles and 1 box.
                            auto circle_r = abs(final_local_scale.x / 2.0f);
                            auto circle_offset = std::max(abs(final_local_scale.y) / 2.0f - abs(circle_r), 0.f);

                            auto circle_position1 = final_world_position + final_world_rotation * math::vec3(0.f, circle_offset, 0.f);
                            auto circle_position2 = final_world_position + final_world_rotation * math::vec3(0.f, -circle_offset, 0.f);
                            auto circle_scale = math::vec3(circle_r, circle_r, circle_r);

                            auto box_scale = math::vec3(
                                final_local_scale.x,
                                circle_offset * 2.0f,
                                final_local_scale.z);

                            easy_draw_impl(
                                circle_position1,
                                final_world_rotation,
                                circle_scale,
                                _gizmo_resources.m_gizmo_physics2d_collider_shader->resource(),
                                _gizmo_resources.m_gizmo_physics2d_collider_circle_vertex->resource(),
                                SIZE_MAX);

                            easy_draw_impl(
                                circle_position2,
                                final_world_rotation,
                                circle_scale,
                                _gizmo_resources.m_gizmo_physics2d_collider_shader->resource(),
                                _gizmo_resources.m_gizmo_physics2d_collider_circle_vertex->resource(),
                                SIZE_MAX);

                            easy_draw_impl(
                                final_world_position,
                                final_world_rotation,
                                box_scale,
                                _gizmo_resources.m_gizmo_physics2d_collider_shader->resource(),
                                _gizmo_resources.m_gizmo_physics2d_collider_box_vertex->resource(),
                                SIZE_MAX);
                        }
                    }
                });

            // Assure gizmo_rchain exists.
            if (enable_draw_gizmo_at_framebuf.has_value())
            {
                if (_inputs.selected_entity.has_value())
                {
                    if (_gizmo_mask & gizmo_mode::SELECTING_HIGHLIGHT)
                    {
                        auto& selected_entity = _inputs.selected_entity.value();
                        auto* translation = selected_entity.get_component<Transform::Translation>();
                        auto* shape = selected_entity.get_component<Renderer::Shape>();
                        auto* textures = selected_entity.get_component<Renderer::Textures>();

                        if (translation != nullptr && shape != nullptr && selected_entity.get_component<Renderer::Shaders>() != nullptr && selected_entity.get_component<Light2D::Point>() == nullptr && selected_entity.get_component<Light2D::Range>() == nullptr && selected_entity.get_component<Light2D::Parallel>() == nullptr)
                        {
                            jegl_rchain_texture_group_idx_t group =
                                jegl_rchain_allocate_texture_group(gizmo_rchain);

                            bool binded = false;
                            if (textures != nullptr)
                            {
                                auto binded_in_zero = textures->get_texture(0);
                                if (binded_in_zero.has_value())
                                {
                                    binded = true;
                                    jegl_rchain_bind_texture(
                                        gizmo_rchain,
                                        group,
                                        0,
                                        binded_in_zero.value()->resource());
                                }
                            }

                            if (!binded)
                                jegl_rchain_bind_texture(
                                    gizmo_rchain,
                                    group,
                                    0,
                                    _gizmo_resources.m_selecting_default_texture->resource());

                            auto* draw_action = easy_draw_impl(
                                translation->world_position,
                                translation->world_rotation,
                                translation->local_scale,
                                _gizmo_resources.m_gizmo_selecting_item_highlight_shader->resource(),
                                shape->vertex.has_value()
                                ? shape->vertex.value()->resource()
                                : _gizmo_resources.m_gizmo_vertex->resource(),
                                group);

                            if (draw_action != nullptr && textures != nullptr)
                            {
                                auto* builtin_uniform = _gizmo_resources.m_gizmo_selecting_item_highlight_shader->m_builtin;

                                JE_CHECK_NEED_AND_SET_UNIFORM(
                                    draw_action, builtin_uniform, tiling, float2, textures->tiling.x, textures->tiling.y);
                                JE_CHECK_NEED_AND_SET_UNIFORM(
                                    draw_action, builtin_uniform, offset, float2, textures->offset.x, textures->offset.y);
                            }
                        }
                    }
                }
            }

#undef JE_CHECK_NEED_AND_SET_UNIFORM
            // Draw gizmo end.
            /////////////////////////////////////////////////////////////////////////

            // Select entity
            selector.except<Editor::Invisable, Light2D::Point, Light2D::Parallel, Light2D::Range>();
            selector.contains<Renderer::Shaders, Renderer::Shape>();
            selector.exec(&DefaultEditorSystem::SelectEntity);

            // Create & create mover!
            selector.exec(&DefaultEditorSystem::UpdateAndCreateMover);

            selector.contains<Editor::EntitySelectBox>();
            selector.exec([this](
                Transform::Translation& trans,
                Transform::LocalScale& localScale,
                Transform::LocalRotation& localRotation)
                {
                    if (const game_entity* current =
                        _inputs.selected_entity ? &_inputs.selected_entity.value() : nullptr)
                    {
                        localRotation.rot = math::quat();
                        auto* etrans = current->get_component<Transform::Translation>();
                        if (etrans != nullptr)
                        {
                            localScale.scale = etrans->local_scale;
                            if (_coord != coord_mode::LOCAL && _mode != Editor::EntityMover::mover_mode::SCALE)
                                localRotation.rot = etrans->world_rotation;
                        }

                        if (auto* eshape = current->get_component<Renderer::Shape>())
                        {
                            if (current->get_component<Light2D::Point>() == nullptr
                                && current->get_component<Light2D::Parallel>() == nullptr
                                && current->get_component<Light2D::Range>() == nullptr)
                            {
                                localScale.scale = localScale.scale * (
                                    eshape->vertex.has_value()
                                    ? jeecs::math::vec3(
                                        eshape->vertex.value()->resource()->m_raw_vertex_data->m_x_max
                                        - eshape->vertex.value()->resource()->m_raw_vertex_data->m_x_min,
                                        eshape->vertex.value()->resource()->m_raw_vertex_data->m_y_max
                                        - eshape->vertex.value()->resource()->m_raw_vertex_data->m_y_min,
                                        eshape->vertex.value()->resource()->m_raw_vertex_data->m_z_max
                                        - eshape->vertex.value()->resource()->m_raw_vertex_data->m_z_min
                                    )
                                    : jeecs::math::vec3(1.0f, 1.0f, 0.0f));
                            }
                            else
                            {
                                localScale.scale = jeecs::math::vec3(1.0f, 1.0f, 1.0f);
                            }
                        }
                    }
                    else
                    {
                        // Hide the mover
                        localScale.scale = math::vec3(0, 0, 0);
                    } });

                    // Mover mgr
                    selector.exec(&DefaultEditorSystem::MoveEntity);

                    if (_editor_enabled)
                    {
                        if (nullptr == _inputs._grab_axis_translation)
                        {
                            auto _set_editing_entity = [](const jeecs::game_entity& e)
                                {
                                    auto* eid = e.get_component<Editor::EntityId>();
                                    if (eid != nullptr)
                                        jedbg_set_editing_entity_uid(eid->eid);
                                };

                            if (!selected_list.empty())
                            {
                                const game_entity* e = _inputs.selected_entity ? &_inputs.selected_entity.value() : nullptr;
                                if (auto fnd = std::find_if(selected_list.begin(), selected_list.end(),
                                    [e](const SelectedResult& s) -> bool
                                    { return e ? s.entity == *e : false; });
                                    fnd != selected_list.end())
                                {
                                    if (_inputs.l_shift)
                                    {
                                        if (fnd == selected_list.begin())
                                            fnd = selected_list.end();

                                        _set_editing_entity((--fnd)->entity);
                                    }
                                    else
                                    {
                                        if (++fnd == selected_list.end())
                                            _set_editing_entity(selected_list.begin()->entity);
                                        else
                                            _set_editing_entity(fnd->entity);
                                    }
                                }
                                else
                                    _set_editing_entity(selected_list.begin()->entity);
                            }
                            else if (_inputs.l_button_pushed)
                                jedbg_set_editing_entity_uid(0);
                        }
                        selected_list.clear();
                    }
                    je_io_set_lock_mouse(
                        _inputs.advise_lock_mouse_walking_camera);

                    _inputs._last_drag_mouse_pos = _inputs.current_mouse_pos;
                    _inputs.current_mouse_pos = _inputs._next_drag_mouse_pos;
        }
    };
}
WO_API wo_api wojeapi_store_bad_shader_name(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_string_t shader_path = wo_string(args + 1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (nullptr == badShadersUniform)
        return wo_ret_panic(vm, "Failed to store uniforms for bad shader, entity has not 'Editor::BadShadersUniform'.");

    return wo_ret_pointer(vm, &badShadersUniform->stored_uniforms.emplace_back(jeecs::Editor::BadShadersUniform::bad_shader_data(shader_path)));
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_int(wo_vm vm, wo_value args)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::INT;
    bad_uniform_var.m_value.ix = (int)wo_int(args + 2);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_int2(wo_vm vm, wo_value args)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::INT2;
    bad_uniform_var.m_value.ix = (int)wo_int(args + 2);
    bad_uniform_var.m_value.iy = (int)wo_int(args + 3);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_int3(wo_vm vm, wo_value args)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::INT3;
    bad_uniform_var.m_value.ix = (int)wo_int(args + 2);
    bad_uniform_var.m_value.iy = (int)wo_int(args + 3);
    bad_uniform_var.m_value.iz = (int)wo_int(args + 4);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_int4(wo_vm vm, wo_value args)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::INT4;
    bad_uniform_var.m_value.ix = (int)wo_int(args + 2);
    bad_uniform_var.m_value.iy = (int)wo_int(args + 3);
    bad_uniform_var.m_value.iz = (int)wo_int(args + 4);
    bad_uniform_var.m_value.iw = (int)wo_int(args + 5);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float(wo_vm vm, wo_value args)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT;
    bad_uniform_var.m_value.x = wo_float(args + 2);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float2(wo_vm vm, wo_value args)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT2;
    bad_uniform_var.m_value.x = wo_float(args + 2);
    bad_uniform_var.m_value.y = wo_float(args + 3);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float3(wo_vm vm, wo_value args)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT3;
    bad_uniform_var.m_value.x = wo_float(args + 2);
    bad_uniform_var.m_value.y = wo_float(args + 3);
    bad_uniform_var.m_value.z = wo_float(args + 4);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float4(wo_vm vm, wo_value args)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT4;
    bad_uniform_var.m_value.x = wo_float(args + 2);
    bad_uniform_var.m_value.y = wo_float(args + 3);
    bad_uniform_var.m_value.z = wo_float(args + 4);
    bad_uniform_var.m_value.w = wo_float(args + 5);

    return wo_ret_void(vm);
}

inline void update_shader(
    jegl_shader::unifrom_variables* uni_var,
    const std::string& uname,
    jeecs::graphic::shader* new_shad)
{
    switch (uni_var->m_uniform_type)
    {
    case jegl_shader::uniform_type::INT:
        new_shad->set_uniform(uname, uni_var->m_value.ix);
        break;
    case jegl_shader::uniform_type::INT2:
        new_shad->set_uniform(uname, uni_var->m_value.ix, uni_var->m_value.iy);
        break;
    case jegl_shader::uniform_type::INT3:
        new_shad->set_uniform(uname, uni_var->m_value.ix, uni_var->m_value.iy, uni_var->m_value.iz);
        break;
    case jegl_shader::uniform_type::INT4:
        new_shad->set_uniform(uname, uni_var->m_value.ix, uni_var->m_value.iy, uni_var->m_value.iz, uni_var->m_value.iw);
        break;
    case jegl_shader::uniform_type::FLOAT:
        new_shad->set_uniform(uname, uni_var->m_value.x);
        break;
    case jegl_shader::uniform_type::FLOAT2:
        new_shad->set_uniform(uname,
            jeecs::math::vec2(uni_var->m_value.x, uni_var->m_value.y));
        break;
    case jegl_shader::uniform_type::FLOAT3:
        new_shad->set_uniform(uname,
            jeecs::math::vec3(uni_var->m_value.x, uni_var->m_value.y, uni_var->m_value.z));
        break;
    case jegl_shader::uniform_type::FLOAT4:
        new_shad->set_uniform(uname,
            jeecs::math::vec4(uni_var->m_value.x, uni_var->m_value.y, uni_var->m_value.z, uni_var->m_value.w));
        break;
    default:
        break; // donothing
    }
}
bool _update_bad_shader_to_new_shader(
    jeecs::Renderer::Shaders* shaders,
    jeecs::Editor::BadShadersUniform* bad_uniforms)
{
    assert(bad_uniforms != nullptr && shaders != nullptr);
    for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
        if (!ok_or_bad_shader.is_ok())
            return false;

    for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
        shaders->shaders.push_back(ok_or_bad_shader.get_ok());
    return true;
}

WO_API wo_api wojeapi_remove_bad_shader_name(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_string_t shader_path = wo_string(args + 1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (badShadersUniform != nullptr)
    {
        for (size_t i = 0; i < badShadersUniform->stored_uniforms.size(); i++)
        {
            auto& ok_or_bad_shader = badShadersUniform->stored_uniforms[i];
            if (!ok_or_bad_shader.is_ok())
            {
                if (ok_or_bad_shader.get_bad().m_path == shader_path)
                    badShadersUniform->stored_uniforms.erase(badShadersUniform->stored_uniforms.begin() + i);
            }
        }

        jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>();
        if (_update_bad_shader_to_new_shader(shaders, badShadersUniform))
            entity->remove_component<jeecs::Editor::BadShadersUniform>();
    }
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_reload_texture_of_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    auto* gcontext = jegl_uhost_get_context(jegl_uhost_get_or_create_for_universe(
        entity->game_world().get_universe().handle(), nullptr));

    std::string old_texture_path = wo_string(args + 1);
    std::string new_texture_path = wo_string(args + 2);

    auto newtexture = jeecs::graphic::texture::load(gcontext, new_texture_path);
    if (newtexture == nullptr)
    {
        return wo_ret_bool(vm, false);
    }

    jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>();
    if (textures != nullptr)
    {
        for (auto& texture_res : textures->textures)
        {
            assert(texture_res.m_texture != nullptr && texture_res.m_texture->resource() != nullptr);

            const char* existed_texture_path = texture_res.m_texture->resource()->m_path;

            if (existed_texture_path != nullptr && old_texture_path == existed_texture_path)
                texture_res.m_texture = newtexture.value();
        }
    }
    return wo_ret_bool(vm, true);
}
WO_API wo_api wojeapi_reload_shader_of_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    auto* gcontext = jegl_uhost_get_context(jegl_uhost_get_or_create_for_universe(
        entity->game_world().get_universe().handle(), nullptr));

    std::string old_shader_path = wo_string(args + 1);
    std::string new_shader_path = wo_string(args + 2);

    jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>();
    jeecs::Editor::BadShadersUniform* bad_uniforms = entity->get_component<jeecs::Editor::BadShadersUniform>();

    auto bad_shader_generator = [](const std::string& path, const jeecs::basic::resource<jeecs::graphic::shader>& shader)
        {
            jeecs::Editor::BadShadersUniform::bad_shader_data bad_shader(path);

            auto* uniform_var = shader->resource()->m_raw_shader_data->m_custom_uniforms;
            while (uniform_var != nullptr)
            {
                bad_shader.m_vars[uniform_var->m_name] = *uniform_var;
                uniform_var = uniform_var->m_next;
            }
            return bad_shader;
        };
    auto copy_shader_generator = [gcontext](
        jeecs::basic::resource<jeecs::graphic::shader>* newshader, auto oldshader)
        {
            assert(newshader != nullptr && (*newshader)->resource()->m_path != nullptr);

            jeecs::basic::resource<jeecs::graphic::shader> new_shader_instance = *newshader;

            // Load and create new shader instance, must be successful.
            *newshader = jeecs::graphic::shader::load(gcontext, new_shader_instance->resource()->m_path).value();

            const char builtin_uniform_varname[] = "JE_";

            if constexpr (std::is_same<decltype(oldshader), jeecs::basic::resource<jeecs::graphic::shader>>::value)
            {
                auto* uniform_var = oldshader->resource()->m_raw_shader_data->m_custom_uniforms;
                while (uniform_var != nullptr)
                {
                    if (strncmp(uniform_var->m_name, builtin_uniform_varname, sizeof(builtin_uniform_varname) - 1) != 0)
                    {
                        switch (uniform_var->m_uniform_type)
                        {
                        case jegl_shader::uniform_type::INT:
                            new_shader_instance->set_uniform(
                                uniform_var->m_name, 
                                uniform_var->m_value.ix);
                            break;
                        case jegl_shader::uniform_type::INT2:
                            new_shader_instance->set_uniform(
                                uniform_var->m_name, 
                                uniform_var->m_value.ix, 
                                uniform_var->m_value.iy);
                            break;
                        case jegl_shader::uniform_type::INT3:
                            new_shader_instance->set_uniform(
                                uniform_var->m_name, 
                                uniform_var->m_value.ix, 
                                uniform_var->m_value.iy, 
                                uniform_var->m_value.iz);
                            break;
                        case jegl_shader::uniform_type::INT4:
                            new_shader_instance->set_uniform(
                                uniform_var->m_name, 
                                uniform_var->m_value.ix,
                                uniform_var->m_value.iy,
                                uniform_var->m_value.iz, 
                                uniform_var->m_value.iw);
                            break;
                        case jegl_shader::uniform_type::FLOAT:
                            new_shader_instance->set_uniform(
                                uniform_var->m_name, 
                                uniform_var->m_value.x);
                            break;
                        case jegl_shader::uniform_type::FLOAT2:
                            new_shader_instance->set_uniform(
                                uniform_var->m_name,
                                jeecs::math::vec2(
                                    uniform_var->m_value.x,
                                    uniform_var->m_value.y));
                            break;
                        case jegl_shader::uniform_type::FLOAT3:
                            new_shader_instance->set_uniform(
                                uniform_var->m_name,
                                jeecs::math::vec3(
                                    uniform_var->m_value.x, 
                                    uniform_var->m_value.y, 
                                    uniform_var->m_value.z));
                            break;
                        case jegl_shader::uniform_type::FLOAT4:
                            new_shader_instance->set_uniform(
                                uniform_var->m_name,
                                jeecs::math::vec4(
                                    uniform_var->m_value.x, 
                                    uniform_var->m_value.y, 
                                    uniform_var->m_value.z, 
                                    uniform_var->m_value.w));
                            break;
                        default:
                            // Just skip it.
                            break;
                        }
                    }
                    uniform_var = uniform_var->m_next;
                }
            }
            else
            {
                for (auto& [name, var] : oldshader.m_vars)
                {
                    if (strncmp(name.c_str(), builtin_uniform_varname, sizeof(builtin_uniform_varname) - 1) != 0)
                    {
                        switch (var.m_uniform_type)
                        {
                        case jegl_shader::uniform_type::INT:
                            new_shader_instance->set_uniform(name, var.m_value.ix);
                            break;
                        case jegl_shader::uniform_type::INT2:
                            new_shader_instance->set_uniform(name, var.m_value.ix, var.m_value.iy);
                            break;
                        case jegl_shader::uniform_type::INT3:
                            new_shader_instance->set_uniform(name, var.m_value.ix, var.m_value.iy, var.m_value.iz);
                            break;
                        case jegl_shader::uniform_type::INT4:
                            new_shader_instance->set_uniform(name, var.m_value.ix, var.m_value.iy, var.m_value.iz, var.m_value.iw);
                            break;
                        case jegl_shader::uniform_type::FLOAT:
                            new_shader_instance->set_uniform(name, var.m_value.x);
                            break;
                        case jegl_shader::uniform_type::FLOAT2:
                            new_shader_instance->set_uniform(name, jeecs::math::vec2(var.m_value.x, var.m_value.y));
                            break;
                        case jegl_shader::uniform_type::FLOAT3:
                            new_shader_instance->set_uniform(name, jeecs::math::vec3(var.m_value.x, var.m_value.y, var.m_value.z));
                            break;
                        case jegl_shader::uniform_type::FLOAT4:
                            new_shader_instance->set_uniform(name, jeecs::math::vec4(var.m_value.x, var.m_value.y, var.m_value.z, var.m_value.w));
                            break;
                        default:
                            // Just skip it.
                            break;
                        }
                    }
                }
            }
            return new_shader_instance;
        };

    if (shaders != nullptr)
    {
        bool need_update = false;
        if (bad_uniforms == nullptr)
        {
            for (auto& shader : shaders->shaders)
            {
                assert(shader != nullptr);
                if (shader->resource()->m_path != nullptr && old_shader_path == shader->resource()->m_path)
                {
                    need_update = true;
                    break;
                }
            }
        }
        else
        {
            for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
            {
                if (ok_or_bad_shader.is_ok())
                {
                    auto& ok_shader = ok_or_bad_shader.get_ok();
                    if (ok_shader->resource()->m_path != nullptr && old_shader_path == ok_shader->resource()->m_path)
                    {
                        need_update = true;
                        break;
                    }
                }
                else if (ok_or_bad_shader.get_bad().m_path == old_shader_path)
                {
                    need_update = true;
                    break;
                }
            }
        }

        if (!need_update)
            return wo_ret_bool(vm, true);

        // 1. Load shader for checking bad shaders
        auto new_shader =
            jeecs::graphic::shader::load(gcontext, new_shader_path);

        if (new_shader.has_value())
        {
            // 1.2 OK! replace old shader with new shader.
            if (bad_uniforms == nullptr)
            {
                for (auto& shader : shaders->shaders)
                {
                    assert(shader != nullptr);
                    if (shader->resource()->m_path != nullptr && old_shader_path == shader->resource()->m_path)
                        shader = copy_shader_generator(&new_shader.value(), shader);
                }
            }
            else
            {
                for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
                {
                    if (ok_or_bad_shader.is_ok())
                    {
                        auto& ok_shader = ok_or_bad_shader.get_ok();
                        if (ok_shader->resource()->m_path != nullptr && old_shader_path == ok_shader->resource()->m_path)
                            ok_or_bad_shader = copy_shader_generator(&new_shader.value(), ok_shader);
                    }
                    else if (ok_or_bad_shader.get_bad().m_path == old_shader_path)
                        ok_or_bad_shader = copy_shader_generator(&new_shader.value(), ok_or_bad_shader.get_bad());
                }

                // Ok, check for update!
                if (_update_bad_shader_to_new_shader(shaders, bad_uniforms))
                    entity->remove_component<jeecs::Editor::BadShadersUniform>();
            }
            return wo_ret_bool(vm, true);
        }
        else
        {
            // 1.1 Shader is failed, if current entity still have BadShadersUniform, do nothing.
            //     or move all shader to BadShadersUniform.
            if (bad_uniforms == nullptr)
            {
                // 1.1.1 Move all shader to bad_uniforms
                bad_uniforms = entity->add_component<jeecs::Editor::BadShadersUniform>();
                assert(bad_uniforms != nullptr);

                for (auto& shader : shaders->shaders)
                {
                    assert(shader != nullptr);

                    // 1.1.1.1 If shader is old one, move the data to BadShadersUniform, or move shader directly
                    if (shader->resource()->m_path != nullptr && old_shader_path == shader->resource()->m_path)
                        bad_uniforms->stored_uniforms.emplace_back(bad_shader_generator(new_shader_path, shader));
                    else
                        bad_uniforms->stored_uniforms.emplace_back(shader);
                }
            }
            // 1.1.2 Current entity already failed, if failed uniform includes ok shader. replace it with bad shader
            else // if (bad_uniforms != nullptr)
            {
                for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
                {
                    if (ok_or_bad_shader.is_ok())
                    {
                        auto& ok_shader = ok_or_bad_shader.get_ok();
                        if (ok_shader->resource()->m_path != nullptr && old_shader_path == ok_shader->resource()->m_path)
                            ok_or_bad_shader = bad_shader_generator(new_shader_path, ok_shader);
                    }
                }
            }
            shaders->shaders.clear();

            return wo_ret_bool(vm, false);
        }
    }
    return wo_ret_bool(vm, true);
}
WO_API wo_api wojeapi_get_bad_shader_list_of_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::Editor::BadShadersUniform* bad_uniform = entity->get_component<jeecs::Editor::BadShadersUniform>();

    assert(bad_uniform != nullptr);

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(result, vm, 0);

    for (auto& ok_or_bad_shader : bad_uniform->stored_uniforms)
    {
        if (ok_or_bad_shader.is_ok() == false)
        {
            wo_set_string(elem, vm, ok_or_bad_shader.get_bad().m_path.c_str());
            wo_arr_add(result, elem);
        }
    }
    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_setable_editor_system(wo_vm vm, wo_value args)
{
    jeecs::DefaultEditorSystem::_editor_enabled = wo_bool(args + 0);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_update_editor_mouse_pos(wo_vm vm, wo_value args)
{
    jeecs::DefaultEditorSystem::_inputs._next_drag_mouse_pos =
        jeecs::math::vec2{ wo_float(args + 0), wo_float(args + 1) };

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_get_editing_mover_mode(wo_vm vm, wo_value args)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys == nullptr)
        return wo_ret_int(vm, (wo_integer_t)jeecs::Editor::EntityMover::mover_mode::NOSPECIFY);
    return wo_ret_int(vm, (wo_integer_t)sys->_mode);
}
WO_API wo_api wojeapi_set_editing_mover_mode(wo_vm vm, wo_value args)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys != nullptr)
        sys->_mode = (jeecs::Editor::EntityMover::mover_mode)wo_int(args + 1);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_get_editing_coord_mode(wo_vm vm, wo_value args)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys == nullptr)
        return wo_ret_int(vm, (wo_integer_t)jeecs::DefaultEditorSystem::coord_mode::GLOBAL);
    return wo_ret_int(vm, (wo_integer_t)sys->_coord);
}
WO_API wo_api wojeapi_set_editing_coord_mode(wo_vm vm, wo_value args)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys != nullptr)
        sys->_coord = (jeecs::DefaultEditorSystem::coord_mode)wo_int(args + 1);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_get_editing_gizmo_mode(wo_vm vm, wo_value args)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys == nullptr)
        return wo_ret_int(vm, (wo_integer_t)jeecs::DefaultEditorSystem::gizmo_mode::NONE);
    return wo_ret_int(vm, (wo_integer_t)sys->_gizmo_mask);
}
WO_API wo_api wojeapi_set_editing_gizmo_mode(wo_vm vm, wo_value args)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys != nullptr)
        sys->_gizmo_mask = (int)wo_int(args + 1);

    return wo_ret_void(vm);
}

static jeecs::typing::debug_eid_t _editor_entity_uid;

void jedbg_set_editing_entity_uid(const jeecs::typing::debug_eid_t uid)
{
    _editor_entity_uid = uid;
}
jeecs::typing::debug_eid_t jedbg_get_editing_entity_uid()
{
    return _editor_entity_uid;
}
jeecs::typing::debug_eid_t jedbg_get_entity_uid(const jeecs::game_entity* e)
{
    auto* eid = e->get_component<jeecs::Editor::EntityId>();
    if (eid == nullptr)
        return 0;
    return eid->eid;
}