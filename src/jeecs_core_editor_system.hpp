#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"
#include "jeecs_core_rendchain_helpers.hpp"

#include <optional>
#include <variant>
#include <set>
#include <vector>
#include <algorithm>

namespace jeecs
{
    using namespace slice_requirement;

    using namespace Transform;
    using namespace Camera;
    using namespace Light2D;
    using namespace Renderer;

    // Load an editor-only graphic resource (shader/vertex/texture). Forwards
    // the std::optional unchanged but logs a clear message first when loading
    // failed. Callers should treat the returned optional as potentially empty
    // (check via has_value() or pass to detail::raw_or_null at draw sites) so
    // the editor still boots with missing gizmos rather than aborting.
    namespace detail
    {
        template <typename Resource>
        inline std::optional<Resource> load_editor_resource_or_log(
            std::optional<Resource>&& opt, const char* desc)
        {
            if (!opt.has_value())
                jeecs::debug::logerr("Failed to create editor resource: %s", desc);
            return std::move(opt);
        }

        // Dereference an optional<basic::resource<T>> to its raw handle, or
        // return nullptr when the resource failed to load. Lets draw code skip
        // failed resources without crashing on a null pointer dereference.
        template <typename T>
        inline auto raw_or_null(std::optional<basic::resource<T>>& r)
        {
            return r.has_value() ? r.value()->resource() : nullptr;
        }
    }

    namespace Editor
    {
        struct Name
        {
            basic::string name;
        };
        struct Invisible
        {
            // Entity with this component will not display in editor, and will not be saved.
        };
        struct EditorWalker
        {
            // Walker entity will have a child camera and controlled by user.
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

            // Editor will create an entity with EntityMoverRoot,
            // and DefaultEditorSystem should handle this entity and create 3 movers for x,y,z axis
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

        // Used to store uniform vars of failed-shader in entity. used for 'update' shaders
        struct BadShadersUniform
        {
            using uniform_info = std::map<std::string, jegl_shader::unifrom_variables>;
            struct bad_shader_data
            {
                std::string m_path;
                uniform_info m_vars;

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
                const bad_shader_data& get_bad() const
                {
                    return std::get<bad_shader_data>(m_shad);
                }
                jeecs::basic::resource<jeecs::graphic::shader>& get_ok()
                {
                    return std::get<jeecs::basic::resource<jeecs::graphic::shader>>(m_shad);
                }
                const jeecs::basic::resource<jeecs::graphic::shader>& get_ok() const
                {
                    return std::get<jeecs::basic::resource<jeecs::graphic::shader>>(m_shad);
                }
            };
            std::vector<ok_or_bad_shader> stored_uniforms;
        };
    }

    // Woolang shader sources for the editor gizmos. Kept here as inline string
    // literals so the GizmoResources constructor stays readable and so a
    // formatter can't produce multi-hundred-char whitespace blobs.
    inline constexpr const char* GIZMO_SHADER_SRC = R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
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
)";

    inline constexpr const char* GIZMO_CAMERA_VISUAL_CONE_SHADER_SRC = R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE);
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
)";

    inline constexpr const char* GIZMO_PHYSICS2D_COLLIDER_SHADER_SRC = R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE);
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
)";

    inline constexpr const char* GIZMO_SELECTING_ITEM_HIGHLIGHT_SHADER_SRC = R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE);
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
)";

    inline constexpr const char* MOVER_AXIS_SHADER_SRC = R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
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
            , ratio = step(JE_COLOR->x, 0.5)
        ;
    ;
)";

    inline constexpr const char* SELECT_BOX_SHADER_SRC = R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
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
)";

    struct GizmoResources
    {
        static std::optional<basic::resource<graphic::vertex>> _create_circle_vertex(math::vec3 anx)
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
            return detail::load_editor_resource_or_log(
                graphic::vertex::create(
                    jegl_vertex::type::LINESTRIP,
                    points.data(),
                    points.size() * sizeof(float),
                    indices,
                    {
                        {jegl_vertex::data_type::FLOAT32, 3},
                        {jegl_vertex::data_type::FLOAT32, 3},
                    }),
                "gizmo circle vertex");
        }
        static std::optional<basic::resource<graphic::texture>> _create_missing_default_icon()
        {
            // texture::create returns basic::resource<texture> directly (it
            // asserts success internally), so this never fails; we still wrap
            // it in optional for type-consistency with the other gizmo fields.
            std::optional<basic::resource<graphic::texture>> result =
                graphic::texture::create(8, 8, jegl_texture::format::RGBA);

            static const uint8_t _unexist_gizmo_icon[8][8] = {
                { 0, 0, 1, 1, 1, 1, 0, 0, },
                { 0, 1, 0, 0, 0, 0, 1, 0, },
                { 0, 1, 0, 0, 0, 0, 1, 0, },
                { 0, 0, 0, 0, 0, 1, 1, 0, },
                { 0, 0, 0, 0, 1, 0, 0, 0, },
                { 0, 0, 0, 1, 1, 0, 0, 0, },
                { 0, 0, 0, 0, 0, 0, 0, 0, },
                { 0, 0, 0, 1, 1, 0, 0, 0, },
            };

            for (size_t ix = 0; ix < 8; ++ix)
            {
                for (size_t iy = 0; iy < 8; ++iy)
                {
                    if (_unexist_gizmo_icon[7 - iy][ix] == 0)
                        result.value()->pix(ix, iy).set({ 0.f, 0.f, 0.f, 0.f });
                    else
                        result.value()->pix(ix, iy).set({ 1.f, 1.f, 1.f, 0.8f });
                }
            }
            return result;
        }

        std::optional<basic::resource<graphic::texture>> m_camera_icon;
        std::optional<basic::resource<graphic::texture>> m_point_or_shape_light2d_icon;
        std::optional<basic::resource<graphic::texture>> m_parallel_light2d_icon;

        std::optional<basic::resource<graphic::texture>> m_selecting_default_texture;

        std::optional<basic::resource<graphic::shader>> m_gizmo_shader;
        std::optional<basic::resource<graphic::shader>> m_gizmo_camera_visual_cone_shader;
        std::optional<basic::resource<graphic::shader>> m_gizmo_physics2d_collider_shader;
        std::optional<basic::resource<graphic::shader>> m_gizmo_selecting_item_highlight_shader;

        std::optional<basic::resource<graphic::vertex>> m_gizmo_vertex;
        std::optional<basic::resource<graphic::vertex>> m_gizmo_camera_visual_cone_vertex;
        std::optional<basic::resource<graphic::vertex>> m_gizmo_physics2d_collider_box_vertex;
        std::optional<basic::resource<graphic::vertex>> m_gizmo_physics2d_collider_circle_vertex;
        // std::optional<basic::resource<graphic::vertex>> m_gizmo_physics2d_collider_capsule_vertex;

        inline static const float gizmo_vertex_data[] = {
            -0.5f, 0.5f, 0.0f,      0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,
            0.5f, 0.5f, 0.0f,       1.0f, 1.0f,
            0.5f, -0.5f, 0.0f,      1.0f, 0.0f,
        };
        inline static const float gizmo_camera_visual_cone_vertex_data[] = {
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f,
        };

        JECS_DISABLE_MOVE_AND_COPY(GizmoResources);

        GizmoResources(jegl_context* glcontext)
            : m_camera_icon{ _create_missing_default_icon() }
            , m_point_or_shape_light2d_icon{ _create_missing_default_icon() }
            , m_parallel_light2d_icon{ _create_missing_default_icon() }
            , m_selecting_default_texture{
                graphic::texture::create(1, 1, jegl_texture::format::RGBA) }
            , m_gizmo_shader{
                detail::load_editor_resource_or_log(
                    graphic::shader::create(nullptr, "!/builtin/gizmo.shader", GIZMO_SHADER_SRC),
                    "gizmo shader") }
            , m_gizmo_camera_visual_cone_shader{
                detail::load_editor_resource_or_log(
                    graphic::shader::create(nullptr, "!/builtin/gizmo_camera_visual_cone.shader", GIZMO_CAMERA_VISUAL_CONE_SHADER_SRC),
                    "gizmo camera visual cone shader") }
            , m_gizmo_physics2d_collider_shader{
                detail::load_editor_resource_or_log(
                    graphic::shader::create(nullptr, "!/builtin/gizmo_physics2d_collider.shader", GIZMO_PHYSICS2D_COLLIDER_SHADER_SRC),
                    "gizmo physics2d collider shader") }
            , m_gizmo_selecting_item_highlight_shader{
                detail::load_editor_resource_or_log(
                    graphic::shader::create(nullptr, "!/builtin/gizmo_selecting_item_highlight.shader", GIZMO_SELECTING_ITEM_HIGHLIGHT_SHADER_SRC),
                    "gizmo selecting item highlight shader") }
            , m_gizmo_vertex{
                detail::load_editor_resource_or_log(
                    graphic::vertex::create(
                        jegl_vertex::type::TRIANGLESTRIP,
                        gizmo_vertex_data,
                        sizeof(gizmo_vertex_data),
                        {0, 1, 2, 3},
                        {
                            {jegl_vertex::data_type::FLOAT32, 3},
                            {jegl_vertex::data_type::FLOAT32, 2},
                        }),
                    "gizmo vertex") }
            , m_gizmo_camera_visual_cone_vertex{
                detail::load_editor_resource_or_log(
                    graphic::vertex::create(jegl_vertex::type::LINESTRIP,
                        gizmo_camera_visual_cone_vertex_data,
                        sizeof(gizmo_camera_visual_cone_vertex_data),
                        {0, 1, 2, 3, 0, 4, 5, 6, 7, 4, 5, 1, 2, 6, 7, 3},
                        {
                            {jegl_vertex::data_type::FLOAT32, 3},
                        }),
                    "gizmo camera visual cone vertex") }
            , m_gizmo_physics2d_collider_box_vertex{
                detail::load_editor_resource_or_log(
                    graphic::vertex::create(
                        jegl_vertex::type::LINESTRIP,
                        gizmo_vertex_data,
                        sizeof(gizmo_vertex_data),
                        {0, 1, 3, 2, 0},
                        {
                            {jegl_vertex::data_type::FLOAT32, 3},
                            {jegl_vertex::data_type::FLOAT32, 2},
                        }),
                    "gizmo physics2d collider box vertex") }
            , m_gizmo_physics2d_collider_circle_vertex{ _create_circle_vertex({0.f, 0.f, 1.f}) }
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

            m_selecting_default_texture.value()->pix(0, 0).set({ 1.f, 1.f, 1.f, 1.f });
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

        std::optional<basic::resource<graphic::vertex>> axis_x;
        std::optional<basic::resource<graphic::vertex>> axis_y;
        std::optional<basic::resource<graphic::vertex>> axis_z;
        std::optional<basic::resource<graphic::vertex>> circ_x;
        std::optional<basic::resource<graphic::vertex>> circ_y;
        std::optional<basic::resource<graphic::vertex>> circ_z;

        math::vec3 _camera_pos;
        math::quat _camera_rot;
        math::ray _camera_ray;

        const Camera::Projection* _camera_projection = nullptr;
        const Camera::OrthoProjection* _camera_ortho_projection = nullptr;
        bool _camera_is_in_o2d_mode = false;

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

            std::optional<int> _last_wheel_y;
            int wheel_delta_count = 0;

            bool advise_lock_mouse_walking_camera = false;
            const Transform::Translation* _grab_axis_translation = nullptr;

            // Empty user-defined default ctor works around a clang/gcc bug
            // where the implicit aggregate init (= {}) leaves std::optional
            // members (e.g. selected_entity below) containing indeterminate
            // values. The empty body forces value-initialization.
            input_msg() noexcept {}

            // selected_entity 用于储存当前被选择的实体，仅用于编辑窗口中，可能为空
            std::optional<jeecs::game_entity> selected_entity;
        };

        inline static input_msg _inputs = {};

        inline static constexpr float MOUSE_MOVEMENT_SCALE = 0.005f;
        inline static constexpr float MOUSE_ROTATION_SCALE = 0.1f;
        inline static constexpr float WALK_SPEED              = 5.0f;
        inline static constexpr float DRAG_THRESHOLD_PX       = 5.f;
        inline static constexpr int   GIZMO_REND_QUEUE        = 100000;
        inline static constexpr float SELECTOR_PADDING        = 0.1f;
        inline static constexpr float ROTATION_RING_TOLERANCE = 0.25f;

        inline static const float axis_x_data[] = {
            -1.f, 0.f, 0.f,     0.25f, 0.f, 0.f,
            1.f, 0.f, 0.f,      1.f, 0.f, 0.f,
        };
        inline static const float axis_y_data[] = {
            0.f, -1.f, 0.f,     0.f, 0.25f, 0.f,
            0.f, 1.f, 0.f,      0.f, 1.f, 0.f,
        };
        inline static const float axis_z_data[] = {
            0.f, 0.f, -1.f,     0.f, 0.f, 0.25f,
            0.f, 0.f, 1.f,      0.f, 0.f, 1.f,
        };

        // Build one of the three axis line vertices (pos + color streams).
        static std::optional<basic::resource<graphic::vertex>> _create_axis_vertex(const float* data, size_t byte_size, const char* desc)
        {
            return detail::load_editor_resource_or_log(
                graphic::vertex::create(
                    jegl_vertex::type::LINESTRIP,
                    data,
                    byte_size,
                    {0, 1},
                    {
                        {jegl_vertex::data_type::FLOAT32, 3},
                        {jegl_vertex::data_type::FLOAT32, 3},
                    }),
                desc);
        }

        // Force the resource's bounds to a cubic [-size, +size] region centered
        // at the origin. Used so axis/circle gizmos remain pickable even when
        // their geometry is a thin line. No-op if the resource failed to load.
        static void _reset_bounds_to_selector(std::optional<basic::resource<graphic::vertex>>& v, float size)
        {
            if (!v.has_value()) return;
            auto* r = v.value()->resource();
            r->m_x_min = r->m_y_min = r->m_z_min = -size;
            r->m_x_max = r->m_y_max = r->m_z_max =  size;
        }

        // Expand the two axes orthogonal to `axis_index` (0=x, 1=y, 2=z) by
        // +/- pad, leaving the along-axis bounds untouched so the line still
        // spans its full length. No-op if the resource failed to load.
        static void _pad_orthogonal(std::optional<basic::resource<graphic::vertex>>& v, int axis_index, float pad)
        {
            if (!v.has_value()) return;
            auto* r = v.value()->resource();
            if (axis_index == 0)
            {
                r->m_y_min -= pad; r->m_y_max += pad;
                r->m_z_min -= pad; r->m_z_max += pad;
            }
            else if (axis_index == 1)
            {
                r->m_x_min -= pad; r->m_x_max += pad;
                r->m_z_min -= pad; r->m_z_max += pad;
            }
            else
            {
                r->m_x_min -= pad; r->m_x_max += pad;
                r->m_y_min -= pad; r->m_y_max += pad;
            }
        }

        DefaultEditorSystem(game_world w)
            : game_system(w)
            , _graphic_uhost(
                jegl_uhost_get_or_create_for_universe(
                    w.get_universe().handle(),
                    nullptr))
            , _gizmo_resources(
                jegl_uhost_get_context(
                    _graphic_uhost))
            , axis_x{ _create_axis_vertex(axis_x_data, sizeof(axis_x_data), "editor axis_x vertex") }
            , axis_y{ _create_axis_vertex(axis_y_data, sizeof(axis_y_data), "editor axis_y vertex") }
            , axis_z{ _create_axis_vertex(axis_z_data, sizeof(axis_z_data), "editor axis_z vertex") }
            , circ_x{ GizmoResources::_create_circle_vertex({1.f, 0.f, 0.f}) }
            , circ_y{ GizmoResources::_create_circle_vertex({0.f, 1.f, 0.f}) }
            , circ_z{ GizmoResources::_create_circle_vertex({0.f, 0.f, 1.f}) }
        {
            _gizmo_draw_branch = jegl_uhost_alloc_branch(_graphic_uhost);

            // Expand the pickable area of every gizmo so thin lines/circles can
            // still be grabbed with the mouse. Lines keep their along-axis span;
            // circles get padded in all three axes instead.
            _pad_orthogonal(circ_x, 0, SELECTOR_PADDING);
            _pad_orthogonal(circ_y, 1, SELECTOR_PADDING);
            _pad_orthogonal(circ_z, 2, SELECTOR_PADDING);

            _reset_bounds_to_selector(axis_x, SELECTOR_PADDING);
            _reset_bounds_to_selector(axis_y, SELECTOR_PADDING);
            _reset_bounds_to_selector(axis_z, SELECTOR_PADDING);

            // Along-axis bounds for the circles (the lines were already reset above).
            if (circ_x.has_value())
            {
                circ_x.value()->resource()->m_x_min = -SELECTOR_PADDING;
                circ_x.value()->resource()->m_x_max =  SELECTOR_PADDING;
            }
            if (circ_y.has_value())
            {
                circ_y.value()->resource()->m_y_min = -SELECTOR_PADDING;
                circ_y.value()->resource()->m_y_max =  SELECTOR_PADDING;
            }
            if (circ_z.has_value())
            {
                circ_z.value()->resource()->m_z_min = -SELECTOR_PADDING;
                circ_z.value()->resource()->m_z_max =  SELECTOR_PADDING;
            }
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

        // Receives a hit during the current frame's pick pass. selected_list
        // is owned by StateUpdate() and passed in by reference.
        void SelectEntity(
            std::multiset<SelectedResult>& selected_list,
            game_entity entity,
            Transform::Translation& trans,
            Renderer::Shape* shape)
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

        void MoveWalker()
        {
            using namespace input;
            using namespace math;

            if (!_editor_enabled)
                return;

            for (auto&& [position, rotation, trans] : query<
                view typesof(LocalPosition&, LocalRotation&, Translation&),
                contains typesof(Editor::EditorWalker),
                except typesof(Projection)
            >())
            {
                if (_inputs.r_button_pushed)
                {
                    _inputs._drag_viewing = false;
                }

                if (_inputs.r_button)
                {
                    float move_speed = WALK_SPEED;
                    if (_inputs.l_ctrl)
                        move_speed = move_speed / 2.0f;
                    if (_inputs.l_shift)
                        move_speed = move_speed * 2.0f;

                    auto delta_drag = _inputs.current_mouse_pos - _inputs._last_drag_mouse_pos;
                    if (_inputs._drag_viewing || delta_drag.length() >= DRAG_THRESHOLD_PX)
                    {
                        _inputs._drag_viewing = true;

                        if (_camera_is_in_o2d_mode)
                        {
                            assert(_camera_ortho_projection != nullptr);

                            move_speed /= _camera_ortho_projection->scale * 0.5f;

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
        }

        void UpdateAndCreateMover()
        {
            for (auto&& [
                mover_entity,
                anchor,
                position,
                rotation,
                scale,
                trans,
                mover] :
            query_entity<
                view typesof(
                    Anchor&,
                    LocalPosition&,
                    LocalRotation&,
                    LocalScale&,
                    Translation&,
                    Editor::EntityMoverRoot&
                )
            >())
            {
                if (!mover.init)
                {
                    mover.init = true;

                    const float select_box_vert_data[] = {
                        -0.5f, -0.5f, -0.5f,
                        0.5f, -0.5f, -0.5f,
                        0.5f, 0.5f, -0.5f,
                        -0.5f, 0.5f, -0.5f,
                        -0.5f, -0.5f, 0.5f,
                        0.5f, -0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f,
                        -0.5f, 0.5f, 0.5f,
                    };
                    std::optional<basic::resource<graphic::vertex>> select_box_vert =
                        detail::load_editor_resource_or_log(
                            graphic::vertex::create(jegl_vertex::type::LINESTRIP,
                                select_box_vert_data,
                                sizeof(select_box_vert_data),
                                { 0, 1, 2, 3, 0, 4, 5, 6, 7, 4, 5, 1, 2, 6, 7, 3 },
                            {
                                {jegl_vertex::data_type::FLOAT32, 3},
                            }),
                            "editor select box vertex");

                    std::optional<basic::resource<graphic::shader>>
                        axis_shader = detail::load_editor_resource_or_log(
                            graphic::shader::create(
                                nullptr,
                                "!/builtin/mover_axis.shader",
                                MOVER_AXIS_SHADER_SRC),
                            "mover axis shader");
                    std::optional<basic::resource<graphic::shader>>
                        select_box_shader = detail::load_editor_resource_or_log(
                            graphic::shader::create(
                                nullptr,
                                "!/builtin/select_box.shader",
                                SELECT_BOX_SHADER_SRC),
                            "select box shader");

                    game_world current_world = mover_entity.game_world();

                    // Helper that creates one of the three axis gizmos (x/y/z).
                    auto make_axis_entity = [&](
                        const math::vec3& axis_vec,
                        const std::optional<basic::resource<graphic::vertex>>& axis_vertex)
                    {
                        game_entity e = current_world.add_entity<
                            Transform::LocalPosition,
                            Transform::LocalScale,
                            Transform::LocalToParent,
                            Transform::Translation,
                            Renderer::Shaders,
                            Renderer::Shape,
                            Renderer::Rendqueue,
                            Renderer::Color,
                            Editor::Invisible,
                            Editor::EntityMover>();

                        if (axis_shader.has_value())
                            e.get_component<Renderer::Shaders>()->shaders.push_back(axis_shader.value());
                        e.get_component<Editor::EntityMover>()->axis = axis_vec;
                        e.get_component<Renderer::Shape>()->vertex = axis_vertex;
                        e.get_component<Renderer::Rendqueue>()->rend_queue = GIZMO_REND_QUEUE;
                        e.get_component<Transform::LocalPosition>()->pos = axis_vec;
                        e.get_component<Transform::LocalToParent>()->parent_uid = anchor.uid;
                        return e;
                    };

                    game_entity axis_x_e = make_axis_entity(math::vec3(1.f, 0, 0), axis_x);
                    game_entity axis_y_e = make_axis_entity(math::vec3(0, 1.f, 0), axis_y);
                    game_entity axis_z_e = make_axis_entity(math::vec3(0, 0, 1.f), axis_z);

                    game_entity select_box = current_world.add_entity<
                        Transform::LocalRotation,
                        Transform::LocalPosition,
                        Transform::LocalScale,
                        Transform::LocalToParent,
                        Transform::Translation,
                        Renderer::Shaders,
                        Renderer::Shape,
                        Renderer::Rendqueue,
                        Editor::Invisible,
                        Editor::EntitySelectBox>();

                    if (select_box_shader.has_value())
                        select_box.get_component<Renderer::Shaders>()->shaders.push_back(select_box_shader.value());
                    select_box.get_component<Renderer::Shape>()->vertex = select_box_vert;
                    select_box.get_component<Renderer::Rendqueue>()->rend_queue = GIZMO_REND_QUEUE;
                    select_box.get_component<Transform::LocalToParent>()->parent_uid = anchor.uid;
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
        }

        void MoveEntity()
        {
            for (auto&& [mover, trans, posi, scale, shape, color] : query<
                view typesof(
                    Editor::EntityMover&,
                    Translation&,
                    LocalPosition&,
                    LocalScale&,
                    Shape*,
                    Color&
                )
            >())
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
                    continue;

                if (!_editor_enabled)
                    continue;

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
                    if (_inputs._grab_axis_translation == &trans && _camera_projection)
                    {
                        math::vec2 diff =
                            (_inputs.current_mouse_pos - _inputs._last_drag_mouse_pos) * math::vec2(1.f, -1.f) * MOUSE_MOVEMENT_SCALE;

                        math::vec4 p0 = trans.world_position;
                        p0.w = 1.0f;
                        p0 = math::mat4trans(
                            _camera_projection->projection,
                            math::mat4trans(_camera_projection->view, p0));

                        math::vec4 p1 = trans.world_position + trans.world_rotation * mover.axis;

                        p1.w = 1.0f;
                        p1 = math::mat4trans(
                            _camera_projection->projection,
                            math::mat4trans(_camera_projection->view, p1));

                        math::vec2 screen_axis = { p1.x - p0.x, p1.y - p0.y };
                        screen_axis = screen_axis.unit();

                        float factor = 1.0f;
                        if (_inputs.l_ctrl)
                            factor *= 0.5f;
                        if (_inputs.l_shift)
                            factor *= 2.0f;

                        float distance =
                            _camera_ortho_projection == nullptr
                            ? (_camera_pos - trans.world_position).length()
                            : 5.0f / _camera_ortho_projection->scale;

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
                            _camera_ortho_projection == nullptr
                            ? ROTATION_RING_TOLERANCE * (_camera_pos - trans.world_position).length()
                            : 1.0f / _camera_ortho_projection->scale;

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
                            _camera_ortho_projection == nullptr
                            ? ROTATION_RING_TOLERANCE * (_camera_pos - etrans->world_position).length()
                            : 1.0f / _camera_ortho_projection->scale;
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
        }

        void StateUpdate()
        {
            // Hits collected during this frame's pick pass. Owned locally so it
            // can never leak state across frames even if StateUpdate returns early.
            std::multiset<SelectedResult> selected_list;

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

            const int current_wheel_y = (int)input::wheel(0).y;
            if (_inputs._last_wheel_y.has_value())
                _inputs.wheel_delta_count = current_wheel_y - *_inputs._last_wheel_y;
            _inputs._last_wheel_y = current_wheel_y;

            // 获取被选中的实体
            // Buffer entities that need an EntityId allocated, and add the component
            // *after* the query loop to avoid structural changes during iteration.
            std::vector<jeecs::game_entity> pending_eid_alloc;
            for (auto&& [e, eid] : query_entity<
                view typesof(Editor::EntityId*)
            >())
            {
                if (eid == nullptr)
                    pending_eid_alloc.push_back(e);
                else
                {
                    if (eid->eid == jedbg_get_editing_entity_uid())
                        _inputs.selected_entity = std::optional(e);
                }
            }
            for (auto& e : pending_eid_alloc)
            {
                auto* ec = e.add_component<Editor::EntityId>();
                if (ec != nullptr)
                    ec->eid = ++this->_allocate_eid;
            }

            // Move walker(root)
            MoveWalker();

            struct EditorGizmoContext
            {
                basic::resource<graphic::framebuffer> m_framebuffer;
                Transform::Translation* m_translation;
                Camera::Projection* m_projection;
            };
            std::optional<EditorGizmoContext> enable_draw_gizmo_at_framebuf = std::nullopt;

            // Move walker(camera)
            for (auto&& [rotation, proj, trans, r2b, o2d] : query<
                view<LocalRotation&, Projection&, Translation&, RendToFramebuffer&, OrthoProjection*>,
                contains<Editor::EditorWalker>>())
            {
                if (!r2b.framebuffer.has_value())
                    continue;

                enable_draw_gizmo_at_framebuf = EditorGizmoContext{
                       r2b.framebuffer.value(),
                       &trans,
                       &proj,
                };

                if (!_editor_enabled)
                    continue;

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

                _camera_projection = &proj;
                _camera_ortho_projection = o2d;

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
            }

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

            auto easy_draw_impl = [&](
                const math::vec3& postion,
                const math::quat& rotation,
                const math::vec3& scale,
                jegl_shader* shader,
                jegl_vertex* vertex,
                jegl_rchain_texture_group* group) -> jegl_rendchain_rend_action*
                {
                    // Skip draws whose shader or vertex failed to load at boot.
                    // The resource loader already logged the failure; rendering is
                    // silently dropped so the editor stays usable.
                    if (shader == nullptr || vertex == nullptr)
                        return nullptr;

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

                        auto* builtin_uniform = &shader->m_builtin_uniforms;

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
            auto draw_easy_gizmo_impl =
                [&](Transform::Translation& trans, jegl_rchain_texture_group* group, bool rotation)
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
                            detail::raw_or_null(_gizmo_resources.m_gizmo_shader),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_vertex),
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
                    detail::raw_or_null(_gizmo_resources.m_camera_icon));
                jegl_rchain_bind_texture(
                    gizmo_rchain,
                    point_light_gizmo_texture_group,
                    0,
                    detail::raw_or_null(_gizmo_resources.m_point_or_shape_light2d_icon));
                jegl_rchain_bind_texture(
                    gizmo_rchain,
                    parallel_light_gizmo_texture_group,
                    0,
                    detail::raw_or_null(_gizmo_resources.m_parallel_light2d_icon));
            }

            if (_gizmo_mask & (gizmo_mode::CAMERA | gizmo_mode::CAMERA_VISUAL_CONE))
            {
                for (auto&& [e, trans, proj] : query_entity<
                    view typesof(Translation&, Projection&),
                    anyof typesof(OrthoProjection, PerspectiveProjection),
                    except typesof(Editor::Invisible)
                >())
                {
                    if (_gizmo_mask & gizmo_mode::CAMERA)
                    {
                        SelectEntity(selected_list, e, trans, nullptr);
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
                            detail::raw_or_null(_gizmo_resources.m_gizmo_camera_visual_cone_shader),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_camera_visual_cone_vertex),
                            nullptr);

                        if (draw_action != nullptr)
                        {
                            const auto* location_addr =
                                _gizmo_resources.m_gizmo_camera_visual_cone_shader.value()->
                                get_uniform_location("InverseCameraProjection");

                            jegl_rchain_set_uniform_float4x4(
                                draw_action,
                                location_addr,
                                proj.inv_projection);
                        }
                    }
                }
            }

            if (_gizmo_mask & gizmo_mode::LIGHT2D)
            {
                for (auto&& [e, trans] : query_entity<
                    view typesof(Translation&),
                    anyof typesof(Point, Range),
                    except typesof(Editor::Invisible)
                >())
                {
                    SelectEntity(selected_list, e, trans, nullptr);
                    draw_easy_gizmo_impl(trans, point_light_gizmo_texture_group, false);
                }
            }

            if (_gizmo_mask & gizmo_mode::LIGHT2D)
            {
                for (auto&& [e, trans] : query_entity<
                    view typesof(Translation&),
                    contains typesof(Parallel),
                    except typesof(Editor::Invisible)
                >())
                {
                    SelectEntity(selected_list, e, trans, nullptr);
                    draw_easy_gizmo_impl(trans, parallel_light_gizmo_texture_group, true);
                }
            }

            if (_gizmo_mask & gizmo_mode::PHYSICS2D_COLLIDER)
            {
                for (auto&& [trans, opos, orot, oscale, box, capsule, circle] : query<
                    view typesof(
                        Translation&,
                        Physics2D::Offset::Position*,
                        Physics2D::Offset::Rotation*,
                        Physics2D::Offset::Scale*,
                        Physics2D::Collider::Box*,
                        Physics2D::Collider::Capsule*,
                        Physics2D::Collider::Circle*
                    ),
                    anyof typesof(
                        Physics2D::Collider::Box,
                        Physics2D::Collider::Capsule,
                        Physics2D::Collider::Circle
                    ),
                    except typesof(
                        Editor::Invisible
                    )
                >())
                {
                    auto final_world_rotation = trans.world_rotation;
                    if (orot != nullptr)
                        final_world_rotation = final_world_rotation * math::quat::euler(0.f, 0.f, orot->degree);

                    auto final_world_position = trans.world_position;
                    if (opos != nullptr)
                    {
                        const math::vec3 rotated = final_world_rotation * math::vec3(opos->value.x, opos->value.y, 0.f);
                        final_world_position += rotated;
                    }

                    // Collider geometry comes from the component itself; Offset::Scale
                    // (if present) further scales it. Transform.local_scale is ignored
                    // for collider preview to match the runtime behavior.
                    auto extra_scale = math::vec2(1.f, 1.f);
                    if (oscale != nullptr)
                        extra_scale = oscale->value;

                    if (box != nullptr)
                    {
                        const auto box_size = math::vec2(
                            std::abs(box->size.x) * std::abs(extra_scale.x),
                            std::abs(box->size.y) * std::abs(extra_scale.y));
                        easy_draw_impl(
                            final_world_position,
                            final_world_rotation,
                            math::vec3(box_size.x, box_size.y, 1.f),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_shader),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_box_vertex),
                            nullptr);
                    }
                    else if (circle != nullptr)
                    {
                        // m_gizmo_physics2d_collider_circle_vertex's R is 1.0f (diameter 2).
                        const float r = std::abs(circle->radius)
                            * std::max(std::abs(extra_scale.x), std::abs(extra_scale.y));
                        easy_draw_impl(
                            final_world_position,
                            final_world_rotation,
                            math::vec3(r, r, r),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_shader),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_circle_vertex),
                            nullptr);
                    }
                    else if (capsule != nullptr)
                    {
                        // Capsule = two end circles + one middle box.
                        const float r = std::abs(capsule->radius) * std::abs(extra_scale.x);
                        const float h = std::abs(capsule->height) * std::abs(extra_scale.y);
                        const float half_h = h * 0.5f;
                        const float offset = std::max(half_h - r, 0.f);

                        const auto circle_position1 = final_world_position + final_world_rotation * math::vec3(0.f,  offset, 0.f);
                        const auto circle_position2 = final_world_position + final_world_rotation * math::vec3(0.f, -offset, 0.f);
                        const auto circle_scale     = math::vec3(r, r, r);
                        const auto box_scale        = math::vec3(r * 2.f, offset * 2.f, 1.f);

                        easy_draw_impl(
                            circle_position1, final_world_rotation, circle_scale,
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_shader),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_circle_vertex),
                            nullptr);
                        easy_draw_impl(
                            circle_position2, final_world_rotation, circle_scale,
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_shader),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_circle_vertex),
                            nullptr);
                        easy_draw_impl(
                            final_world_position, final_world_rotation, box_scale,
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_shader),
                            detail::raw_or_null(_gizmo_resources.m_gizmo_physics2d_collider_box_vertex),
                            nullptr);
                    }
                }
            }

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

                        if (translation != nullptr
                            && shape != nullptr
                            && selected_entity.get_component<Renderer::Shaders>() != nullptr
                            && selected_entity.get_component<Light2D::Point>() == nullptr
                            && selected_entity.get_component<Light2D::Range>() == nullptr
                            && selected_entity.get_component<Light2D::Parallel>() == nullptr)
                        {
                            jegl_rchain_texture_group* group =
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
                                    detail::raw_or_null(_gizmo_resources.m_selecting_default_texture));

                            auto* draw_action = easy_draw_impl(
                                translation->world_position,
                                translation->world_rotation,
                                translation->local_scale,
                                detail::raw_or_null(_gizmo_resources.m_gizmo_selecting_item_highlight_shader),
                                shape->vertex.has_value()
                                ? shape->vertex.value()->resource()
                                : detail::raw_or_null(_gizmo_resources.m_gizmo_vertex),
                                group);

                            if (draw_action != nullptr && textures != nullptr)
                            {
                                auto* builtin_uniform = _gizmo_resources.m_gizmo_selecting_item_highlight_shader.value()->m_builtin;

                                JE_CHECK_NEED_AND_SET_UNIFORM(
                                    draw_action, builtin_uniform, tiling, float2, textures->tiling.x, textures->tiling.y);
                                JE_CHECK_NEED_AND_SET_UNIFORM(
                                    draw_action, builtin_uniform, offset, float2, textures->offset.x, textures->offset.y);
                            }
                        }
                    }
                }
            }

            // Draw gizmo end.
            /////////////////////////////////////////////////////////////////////////

            // Select entity
            for (auto&& [e, trans, shape] : query_entity<
                view typesof(Translation&, Shape&),
                contains typesof(Shaders),
                except typesof(Editor::Invisible, Point, Parallel, Range)
            >())
            {
                SelectEntity(selected_list, e, trans, &shape);
            }

            // Create & update mover!
            UpdateAndCreateMover();

            for (auto&& [trans, localScale, localRotation] : query<
                view typesof(Translation&, LocalScale&, LocalRotation&),
                contains typesof(Editor::EntitySelectBox)
            >())
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
                                    eshape->vertex.value()->resource()->m_x_max
                                    - eshape->vertex.value()->resource()->m_x_min,
                                    eshape->vertex.value()->resource()->m_y_max
                                    - eshape->vertex.value()->resource()->m_y_min,
                                    eshape->vertex.value()->resource()->m_z_max
                                    - eshape->vertex.value()->resource()->m_z_min
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
                }
            }

            // Mover mgr
            MoveEntity();

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
                            {
                                return e ? s.entity == *e : false;
                            });
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
                // selected_list is a local; it goes out of scope at function exit.
            }
            je_io_set_lock_mouse(
                _inputs.advise_lock_mouse_walking_camera);

            _inputs._last_drag_mouse_pos = _inputs.current_mouse_pos;
            _inputs.current_mouse_pos = _inputs._next_drag_mouse_pos;
        }
    };
}
WOORT_API woort_api wojeapi_store_bad_shader_name(void)
{
    jeecs::game_entity* const entity = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    const woort_U8CString shader_path = woort_string(1);

    jeecs::Editor::BadShadersUniform* const badShadersUniform =
        entity->get_component<jeecs::Editor::BadShadersUniform>();

    if (nullptr == badShadersUniform)
        return woort_ret_panic(
            "Failed to store uniforms for bad shader, entity has not 'Editor::BadShadersUniform'.");

    return woort_ret_pointer(
        &badShadersUniform->stored_uniforms.emplace_back(
            jeecs::Editor::BadShadersUniform::bad_shader_data(shader_path)));
}

// Helper that fetches the bad-shader slot addressed by woort_pointer(0) and
// returns a reference to the named uniform variable inside it (creating one if
// it doesn't exist yet). All wojeapi_store_bad_shader_uniforms_* share this
// prologue; only the type tag and value assignment differ.
inline auto& _bad_shader_uniform_slot()
{
    auto* const bad_shader =
        &(static_cast<jeecs::Editor::BadShadersUniform::ok_or_bad_shader*>(woort_pointer(0)))->get_bad();
    return bad_shader->m_vars[woort_string(1)];
}

WOORT_API woort_api wojeapi_store_bad_shader_uniforms_int(void)
{
    auto& v = _bad_shader_uniform_slot();
    v.m_uniform_type = jegl_shader::uniform_type::INT;
    v.m_value.m_int = (int)woort_int(2);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_int2(void)
{
    auto& v = _bad_shader_uniform_slot();
    v.m_uniform_type = jegl_shader::uniform_type::INT2;
    v.m_value.m_int2[0] = (int)woort_int(2);
    v.m_value.m_int2[1] = (int)woort_int(3);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_int3(void)
{
    auto& v = _bad_shader_uniform_slot();
    v.m_uniform_type = jegl_shader::uniform_type::INT3;
    v.m_value.m_int3[0] = (int)woort_int(2);
    v.m_value.m_int3[1] = (int)woort_int(3);
    v.m_value.m_int3[2] = (int)woort_int(4);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_int4(void)
{
    auto& v = _bad_shader_uniform_slot();
    v.m_uniform_type = jegl_shader::uniform_type::INT4;
    v.m_value.m_int4[0] = (int)woort_int(2);
    v.m_value.m_int4[1] = (int)woort_int(3);
    v.m_value.m_int4[2] = (int)woort_int(4);
    v.m_value.m_int4[3] = (int)woort_int(5);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_float(void)
{
    auto& v = _bad_shader_uniform_slot();
    v.m_uniform_type = jegl_shader::uniform_type::FLOAT;
    v.m_value.m_float = woort_float(2);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_float2(void)
{
    auto& v = _bad_shader_uniform_slot();
    v.m_uniform_type = jegl_shader::uniform_type::FLOAT2;
    v.m_value.m_float2[0] = woort_float(2);
    v.m_value.m_float2[1] = woort_float(3);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_float3(void)
{
    auto& v = _bad_shader_uniform_slot();
    v.m_uniform_type = jegl_shader::uniform_type::FLOAT3;
    v.m_value.m_float3[0] = woort_float(2);
    v.m_value.m_float3[1] = woort_float(3);
    v.m_value.m_float3[2] = woort_float(4);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_float4(void)
{
    auto& v = _bad_shader_uniform_slot();
    v.m_uniform_type = jegl_shader::uniform_type::FLOAT4;
    v.m_value.m_float4[0] = woort_float(2);
    v.m_value.m_float4[1] = woort_float(3);
    v.m_value.m_float4[2] = woort_float(4);
    v.m_value.m_float4[3] = woort_float(5);
    return woort_ret_void();
}

inline void update_shader(
    jegl_shader::unifrom_variables* uni_var,
    const std::string& uname,
    jeecs::graphic::shader* new_shad)
{
    using UT = jegl_shader::uniform_type;
    const auto& v = uni_var->m_value;
    switch (uni_var->m_uniform_type)
    {
    case UT::INT:    new_shad->set_uniform(uname, v.m_int); break;
    case UT::INT2:   new_shad->set_uniform(uname, v.m_int2[0], v.m_int2[1]); break;
    case UT::INT3:   new_shad->set_uniform(uname, v.m_int3[0], v.m_int3[1], v.m_int3[2]); break;
    case UT::INT4:   new_shad->set_uniform(uname, v.m_int4[0], v.m_int4[1], v.m_int4[2], v.m_int4[3]); break;
    case UT::FLOAT:  new_shad->set_uniform(uname, v.m_float); break;
    case UT::FLOAT2: new_shad->set_uniform(uname, jeecs::math::vec2(v.m_float2[0], v.m_float2[1])); break;
    case UT::FLOAT3: new_shad->set_uniform(uname, jeecs::math::vec3(v.m_float3[0], v.m_float3[1], v.m_float3[2])); break;
    case UT::FLOAT4: new_shad->set_uniform(uname, jeecs::math::vec4(v.m_float4[0], v.m_float4[1], v.m_float4[2], v.m_float4[3])); break;
    default: break; // donothing
    }
}
bool _update_bad_shader_to_new_shader(
    jeecs::Renderer::Shaders* shaders,
    jeecs::Editor::BadShadersUniform* bad_uniforms)
{
    if (bad_uniforms == nullptr || shaders == nullptr)
    {
        jeecs::debug::logerr("_update_bad_shader_to_new_shader: null input (shaders=%p, bad_uniforms=%p).",
            (const void*)shaders, (const void*)bad_uniforms);
        return false;
    }
    for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
        if (!ok_or_bad_shader.is_ok())
            return false;

    for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
        shaders->shaders.push_back(ok_or_bad_shader.get_ok());
    return true;
}

WOORT_API woort_api wojeapi_remove_bad_shader_name(void)
{
    jeecs::game_entity* const entity = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    const woort_U8CString shader_path = woort_string(1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (badShadersUniform != nullptr)
    {
        // Use std::erase_if instead of an index loop: the previous index-based
        // erase skipped the element immediately following each removed one.
        std::erase_if(badShadersUniform->stored_uniforms,
            [&shader_path](const jeecs::Editor::BadShadersUniform::ok_or_bad_shader& s) {
                return !s.is_ok() && s.get_bad().m_path == shader_path;
            });

        jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>();
        if (_update_bad_shader_to_new_shader(shaders, badShadersUniform))
            entity->remove_component<jeecs::Editor::BadShadersUniform>();
    }
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_reload_texture_of_entity(void)
{
    jeecs::game_entity* const entity = static_cast<jeecs::game_entity*>(woort_gcpointer(0));

    auto* gcontext = jegl_uhost_get_context(jegl_uhost_get_or_create_for_universe(
        entity->game_world().get_universe().handle(), nullptr));

    std::string old_texture_path = woort_string(1);
    std::string new_texture_path = woort_string(2);

    std::optional<jeecs::basic::resource<jeecs::graphic::texture>> newtexture;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        newtexture = jeecs::graphic::texture::load(gcontext, new_texture_path);
    }
    (void)woort_vm_swap(last);

    if (!newtexture.has_value())
        return woort_ret_bool(false);

    jeecs::Renderer::Textures* textures =
        entity->get_component<jeecs::Renderer::Textures>();

    if (textures != nullptr)
    {
        for (auto& texture_res : textures->textures)
        {
            const char* existed_texture_path =
                texture_res.m_texture->resource()->m_handle.m_path_may_null_if_builtin;

            if (existed_texture_path != nullptr
                && old_texture_path == existed_texture_path)
                texture_res.m_texture = newtexture.value();
        }
    }
    return woort_ret_bool(true);
}
WOORT_API woort_api wojeapi_reload_shader_of_entity(void)
{
    jeecs::game_entity* const entity = static_cast<jeecs::game_entity*>(woort_gcpointer(0));

    auto* gcontext = jegl_uhost_get_context(jegl_uhost_get_or_create_for_universe(
        entity->game_world().get_universe().handle(), nullptr));

    std::string old_shader_path = woort_string(1);
    std::string new_shader_path = woort_string(2);

    jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>();
    jeecs::Editor::BadShadersUniform* bad_uniforms = entity->get_component<jeecs::Editor::BadShadersUniform>();

    bool success = true;
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        auto bad_shader_generator =
            [](const std::string& path, const jeecs::basic::resource<jeecs::graphic::shader>& shader)
            {
                jeecs::Editor::BadShadersUniform::bad_shader_data bad_shader(path);

                auto* uniform_var = shader->resource()->m_custom_uniforms;
                while (uniform_var != nullptr)
                {
                    bad_shader.m_vars[uniform_var->m_name] = *uniform_var;
                    uniform_var = uniform_var->m_next;
                }
                return bad_shader;
            };
        auto copy_shader_generator =
            [gcontext](jeecs::basic::resource<jeecs::graphic::shader>& newshader, auto oldshader)
            {
                if (newshader->resource()->m_handle.m_path_may_null_if_builtin == nullptr)
                {
                    jeecs::debug::logfatal("copy_shader_generator: invalid newshader; aborting.");
                    std::abort();
                }

                jeecs::basic::resource<jeecs::graphic::shader> new_shader_instance = newshader;

                // Re-load the shader from disk. On failure, log and leave
                // newshader unchanged (its previous value is still valid).
                auto reloaded = jeecs::detail::load_editor_resource_or_log(
                    jeecs::graphic::shader::load(
                        gcontext, new_shader_instance->resource()->m_handle.m_path_may_null_if_builtin),
                    "reloaded shader instance");
                if (reloaded.has_value())
                    newshader = reloaded.value();

                const char builtin_uniform_varname[] = "JE_";

                if constexpr (std::is_same<decltype(oldshader), jeecs::basic::resource<jeecs::graphic::shader>>::value)
                {
                    auto* uniform_var = oldshader->resource()->m_custom_uniforms;
                    while (uniform_var != nullptr)
                    {
                        if (strncmp(uniform_var->m_name, builtin_uniform_varname, sizeof(builtin_uniform_varname) - 1) != 0)
                        {
                            update_shader(uniform_var, uniform_var->m_name, new_shader_instance.get());
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
                            update_shader(&var, name, new_shader_instance.get());
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
                    if (shader->resource()->m_handle.m_path_may_null_if_builtin != nullptr
                        && old_shader_path == shader->resource()->m_handle.m_path_may_null_if_builtin)
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
                        if (ok_shader->resource()->m_handle.m_path_may_null_if_builtin != nullptr
                            && old_shader_path == ok_shader->resource()->m_handle.m_path_may_null_if_builtin)
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

            if (need_update)
            {
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
                            if (shader->resource()->m_handle.m_path_may_null_if_builtin != nullptr
                                && old_shader_path == shader->resource()->m_handle.m_path_may_null_if_builtin)
                                shader = copy_shader_generator(new_shader.value(), shader);
                        }
                    }
                    else
                    {
                        for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
                        {
                            if (ok_or_bad_shader.is_ok())
                            {
                                auto& ok_shader = ok_or_bad_shader.get_ok();
                                if (ok_shader->resource()->m_handle.m_path_may_null_if_builtin != nullptr
                                    && old_shader_path == ok_shader->resource()->m_handle.m_path_may_null_if_builtin)
                                    ok_or_bad_shader = copy_shader_generator(new_shader.value(), ok_shader);
                            }
                            else if (ok_or_bad_shader.get_bad().m_path == old_shader_path)
                                ok_or_bad_shader = copy_shader_generator(new_shader.value(), ok_or_bad_shader.get_bad());
                        }

                        // Ok, check for update!
                        if (_update_bad_shader_to_new_shader(shaders, bad_uniforms))
                            entity->remove_component<jeecs::Editor::BadShadersUniform>();
                    }
                }
                else
                {
                    // 1.1 Shader is failed, if current entity still have BadShadersUniform, do nothing.
                    //     or move all shader to BadShadersUniform.
                    if (bad_uniforms == nullptr)
                    {
                        // 1.1.1 Move all shader to bad_uniforms
                        bad_uniforms = entity->add_component<jeecs::Editor::BadShadersUniform>();
                        if (bad_uniforms == nullptr)
                        {
                            jeecs::debug::logerr("wojeapi_reload_shader_of_entity: failed to allocate BadShadersUniform.");
                            return woort_ret_bool(false);
                        }

                        for (auto& shader : shaders->shaders)
                        {
                            // 1.1.1.1 If shader is old one, move the data to BadShadersUniform, or move shader directly
                            if (shader->resource()->m_handle.m_path_may_null_if_builtin != nullptr
                                && old_shader_path == shader->resource()->m_handle.m_path_may_null_if_builtin)
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
                                if (ok_shader->resource()->m_handle.m_path_may_null_if_builtin != nullptr
                                    && old_shader_path == ok_shader->resource()->m_handle.m_path_may_null_if_builtin)
                                    ok_or_bad_shader = bad_shader_generator(new_shader_path, ok_shader);
                            }
                        }
                    }
                    shaders->shaders.clear();

                    success = false;
                }
            }
        }
    }
    (void)woort_vm_swap(last);

    return woort_ret_bool(success);
}
WOORT_API woort_api wojeapi_get_bad_shader_list_of_entity(void)
{
    jeecs::game_entity* const entity =
        static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    jeecs::Editor::BadShadersUniform* const bad_uniform =
        entity->get_component<jeecs::Editor::BadShadersUniform>();

    if (bad_uniform == nullptr)
        return woort_ret_panic("Entity has no 'Editor::BadShadersUniform' component.");

    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(result);

    for (auto& ok_or_bad_shader : bad_uniform->stored_uniforms)
    {
        if (ok_or_bad_shader.is_ok() == false)
        {
            woort_set_string(elem, ok_or_bad_shader.get_bad().m_path.c_str());
            woort_vec_push(result, elem);
        }
    }
    return woort_ret_value(result);
}
WOORT_API woort_api wojeapi_setable_editor_system(void)
{
    jeecs::DefaultEditorSystem::_editor_enabled = woort_bool(0);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_update_editor_mouse_pos(void)
{
    jeecs::DefaultEditorSystem::_inputs._next_drag_mouse_pos =
        jeecs::math::vec2{ woort_float(0), woort_float(1) };

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_get_editing_mover_mode(void)
{
    jeecs::game_world world(woort_pointer(0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys == nullptr)
        return woort_ret_int(
            static_cast<woort_Int>(
                jeecs::Editor::EntityMover::mover_mode::NOSPECIFY));
    return woort_ret_int(static_cast<woort_Int>(sys->_mode));
}
WOORT_API woort_api wojeapi_set_editing_mover_mode(void)
{
    jeecs::game_world world(woort_pointer(0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys != nullptr)
        sys->_mode = static_cast<jeecs::Editor::EntityMover::mover_mode>(woort_int(1));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_get_editing_coord_mode(void)
{
    jeecs::game_world world(woort_pointer(0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys == nullptr)
        return woort_ret_int(
            static_cast<woort_Int>(
                jeecs::DefaultEditorSystem::coord_mode::GLOBAL));
    return woort_ret_int(static_cast<woort_Int>(sys->_coord));
}
WOORT_API woort_api wojeapi_set_editing_coord_mode(void)
{
    jeecs::game_world world(woort_pointer(0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys != nullptr)
        sys->_coord = static_cast<jeecs::DefaultEditorSystem::coord_mode>(woort_int(1));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_get_editing_gizmo_mode(void)
{
    jeecs::game_world world(woort_pointer(0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys == nullptr)
        return woort_ret_int(
            static_cast<woort_Int>(jeecs::DefaultEditorSystem::gizmo_mode::NONE));
    return woort_ret_int(
        static_cast<woort_Int>(sys->_gizmo_mask));
}
WOORT_API woort_api wojeapi_set_editing_gizmo_mode(void)
{
    jeecs::game_world world(woort_pointer(0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys != nullptr)
        sys->_gizmo_mask = static_cast<int>(woort_int(1));

    return woort_ret_void();
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
