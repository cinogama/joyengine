#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
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
                nospecify,
                selection,
                movement,
                rotation,
                scale,
            };
            mover_mode mode = mover_mode::nospecify;

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
                ok_or_bad_shader(const bad_shader_data& badshader) :
                    m_shad(badshader)
                {

                }
                ok_or_bad_shader(const jeecs::basic::resource<jeecs::graphic::shader>& okshader) :
                    m_shad(okshader)
                {

                }
                bool is_ok()const
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

    struct DefaultEditorSystem : public game_system
    {
        inline static bool _editor_enabled = true;

        enum coord_mode
        {
            global,
            local
        };

        Editor::EntityMover::mover_mode _mode = Editor::EntityMover::mover_mode::selection;
        coord_mode _coord = coord_mode::global;

        basic::resource<graphic::vertex> axis_x;
        basic::resource<graphic::vertex> axis_y;
        basic::resource<graphic::vertex> axis_z;
        basic::resource<graphic::vertex> circ_x;
        basic::resource<graphic::vertex> circ_y;
        basic::resource<graphic::vertex> circ_z;

        math::vec3 _begin_drag;
        bool _drag_viewing = false;

        math::vec3 _camera_pos;
        math::quat _camera_rot;
        math::ray _camera_ray;

        const Camera::Projection* _camera_porjection = nullptr;
        const Camera::OrthoProjection* _camera_ortho_porjection = nullptr;
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

            bool l_buttom = false;
            bool r_buttom = false;

            bool l_buttom_click = false;
            bool l_buttom_pushed = false;
            bool r_buttom_click = false;
            bool r_buttom_pushed = false;

            float delta_time = 0.0f;

            jeecs::math::vec2 uniform_mouse_pos = {};
            jeecs::math::ivec2 advise_lock_mouse_pos = {};

            int _wheel_count_record = INT_MAX;
            int wheel_delta_count = 0;

            // Why write an empty constructor function here?
            // It's a bug of clang/gcc, fuck!
            input_msg()noexcept {}

            // selected_entity 用于储存当前被选择的实体，仅用于编辑窗口中，可能为空
            std::optional<jeecs::game_entity> selected_entity;
        };

        inline static input_msg _inputs = {};

        DefaultEditorSystem(game_world w)
            : game_system(w)
        {
            axis_x = graphic::vertex::create(jegl_vertex::type::LINES,
                { -1.f, 0.f, 0.f,       0.25f, 0.f, 0.f,
                   1.f, 0.f, 0.f,       1.f,   0.f, 0.f },
                { 3, 3 });
            axis_y = graphic::vertex::create(jegl_vertex::type::LINES,
                { 0.f, -1.f, 0.f,       0.f, 0.25f, 0.f,
                  0.f, 1.f, 0.f,        0.f, 1.f, 0.f },
                { 3, 3 });
            axis_z = graphic::vertex::create(jegl_vertex::type::LINES,
                { 0.f, 0.f, -1.f,       0.f, 0.f, 0.25f,
                  0.f, 0.f,  1.f,       0.f, 0.f, 1.f },
                { 3, 3 });
            circ_x = _create_circle_vertex({ 1.f, 0.f, 0.f });
            circ_y = _create_circle_vertex({ 0.f, 1.f, 0.f });
            circ_z = _create_circle_vertex({ 0.f, 0.f, 1.f });

            circ_x->resouce()->m_raw_vertex_data->m_size_y += 0.2f;
            circ_x->resouce()->m_raw_vertex_data->m_size_z += 0.2f;
            circ_y->resouce()->m_raw_vertex_data->m_size_x += 0.2f;
            circ_y->resouce()->m_raw_vertex_data->m_size_z += 0.2f;
            circ_z->resouce()->m_raw_vertex_data->m_size_x += 0.2f;
            circ_z->resouce()->m_raw_vertex_data->m_size_y += 0.2f;

            circ_x->resouce()->m_raw_vertex_data->m_size_x =
                circ_y->resouce()->m_raw_vertex_data->m_size_y =
                circ_z->resouce()->m_raw_vertex_data->m_size_z =
                axis_x->resouce()->m_raw_vertex_data->m_size_y
                = axis_x->resouce()->m_raw_vertex_data->m_size_z
                = axis_y->resouce()->m_raw_vertex_data->m_size_x
                = axis_y->resouce()->m_raw_vertex_data->m_size_z
                = axis_z->resouce()->m_raw_vertex_data->m_size_x
                = axis_z->resouce()->m_raw_vertex_data->m_size_y
                = 0.2f;
        }

        struct SelectedResult
        {
            float distance;
            jeecs::game_entity entity;

            bool operator < (const SelectedResult& s) const noexcept
            {
                return distance < s.distance;
            }
        };
        std::multiset<SelectedResult> selected_list;

        const Transform::Translation* _grab_axis_translation = nullptr;
        math::vec2 _grab_last_pos;

        bool advise_lock_mouse = false;

        void MoveWalker(Transform::LocalPosition& position, Transform::LocalRotation& rotation, Transform::Translation& trans)
        {
            if (!_editor_enabled)
                return;

            using namespace input;
            using namespace math;

            if (_inputs.r_buttom_pushed)
            {
                _begin_drag = _inputs.uniform_mouse_pos;
                _drag_viewing = false;
            }

            if (_inputs.r_buttom)
            {
                float move_speed = 5.0f;
                if (_inputs.l_ctrl)
                    move_speed = move_speed / 2.0f;
                if (_inputs.l_shift)
                    move_speed = move_speed * 2.0f;

                auto delta_drag = _inputs.uniform_mouse_pos - _begin_drag;
                if (_drag_viewing || delta_drag.length() >= 0.01f)
                {
                    _drag_viewing = true;

                    if (_camera_is_in_o2d_mode)
                    {
                        assert(_camera_ortho_porjection != nullptr);

                        move_speed /= _camera_ortho_porjection->scale * 0.5f;

                        _begin_drag = _inputs.uniform_mouse_pos;
                        position.pos -= move_speed * vec3(delta_drag.x, delta_drag.y, 0.0);
                        rotation.rot = quat();
                    }
                    else
                    {
                        advise_lock_mouse = true;
                        rotation.rot = rotation.rot * quat(0, 30.f * _inputs.uniform_mouse_pos.x, 0);
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
                advise_lock_mouse = false;
        }

        void CameraWalker(
            Transform::LocalRotation& rotation,
            Camera::Projection& proj,
            Transform::Translation& trans,
            Camera::OrthoProjection* o2d)
        {
            if (!_editor_enabled)
                return;

            using namespace input;
            using namespace math;

            auto mouse_position = _inputs.uniform_mouse_pos;

            if ((_camera_is_in_o2d_mode = o2d != nullptr))
            {
                o2d->scale = o2d->scale * pow(2.0f, (float)_inputs.wheel_delta_count);
                rotation.rot = quat();
            }

            _camera_ray = math::ray(trans, proj, mouse_position, _camera_is_in_o2d_mode);
            _camera_porjection = &proj;
            _camera_ortho_porjection = o2d;

            if (_drag_viewing && _inputs.r_buttom)
            {
                if (!_camera_is_in_o2d_mode)
                    rotation.rot = rotation.rot * quat(30.f * -mouse_position.y, 0, 0);

                _camera_rot = trans.world_rotation;
                _camera_pos = trans.world_position;
            }
        }

        void SelectEntity(game_entity entity, Transform::Translation& trans, Renderer::Shape* shape)
        {
            if (!_editor_enabled)
                return;

            if (_inputs.l_buttom_pushed)
            {
                auto result = _camera_ray.intersect_entity(trans, shape);

                if (result.intersected)
                    selected_list.insert(SelectedResult{ result.distance, entity });
            }
        }

        static basic::resource<graphic::vertex> _create_circle_vertex(math::vec3 anx)
        {
            auto rot = 90.0f * anx;
            std::swap(rot.x, rot.y);

            math::quat r(rot.x, rot.y, rot.z);

            const size_t half_step_count = 50;

            std::vector<float> points;
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
            }
            return graphic::vertex::create(jegl_vertex::type::LINESTRIP, points, { 3, 3 });
        }

        void UpdateAndCreateMover(game_entity mover_entity,
            Transform::Anchor& anchor,
            Transform::LocalPosition& position,
            Transform::LocalRotation& rotation,
            Transform::LocalScale& scale,
            Transform::Translation& trans,
            Editor::EntityMoverRoot& mover)
        {
            basic::resource<graphic::vertex> select_box_vert =
                graphic::vertex::create(jegl_vertex::type::LINESTRIP,
                    {
                           -0.5f,-0.5f,-0.5f,    0.5f,-0.5f,-0.5f,    0.5f,0.5f,-0.5f,      -0.5f,0.5f,-0.5f,     -0.5f,-0.5f,-0.5f,
                           -0.5f,-0.5f,0.5f,    0.5f,-0.5f,0.5f,    0.5f,0.5f,0.5f,      -0.5f,0.5f,0.5f,     -0.5f,-0.5f,0.5f,
                           -0.5f,0.5f,0.5f,       -0.5f,0.5f,-0.5f,    0.5f,0.5f,-0.5f,  0.5f,0.5f,0.5f,0.5f,-0.5f,0.5f,0.5f,-0.5f,-0.5f
                    },
                    { 3 });

            static basic::resource<graphic::shader>
                axis_shader = graphic::shader::create("!/builtin/mover_axis.shader",
                    R"(
import je::shader;
        
ZTEST (ALWAYS);
        
VAO_STRUCT! vin {
    vertex : float3,
    color  : float3
};
using v2f = struct {
    pos : float4,
    color : float3
};
using fout = struct {
    color : float4,
    self_lum: float4,
};
        
public let vert = 
    \v: vin = v2f { pos = je_mvp * vertex_pos, 
                    color = v.color } 
        where vertex_pos = float4::create(v.vertex, 1.)
    ;
;
        
public let frag = 
    \f: v2f = fout{ 
        color = float4::create(show_color, 1.),
        self_lum = float4::create(show_color * 100., 1.),
        }
        where show_color = lerp(f.color, float3::new(1., 1., 1.), ratio)
            , ratio = step(float::new(0.5), je_color->x)
    ;
;
        )");
            static basic::resource<graphic::shader>
                select_box_shader = graphic::shader::create("!/builtin/select_box.shader",
                    R"(
import je::shader;
        
ZTEST (LESS);
        
VAO_STRUCT! vin {
    vertex : float3,
};
using v2f = struct {
    pos : float4,
};
using fout = struct {
    color : float4,
    self_lum: float4,
};
        
public let vert = \v: vin = v2f{ pos = je_mvp * float4::create(v.vertex, 1.) };;
public let frag = 
    \_: v2f = fout{ 
        color = float4::create(0.5, 1., 0.5, 1.),
        self_lum = float4::create(1., 100., 1., 1.),
    }
    ;
;
        )");

            if (!mover.init)
            {
                mover.init = true;

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
                    Editor::EntityMover
                >();
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
                    Editor::EntityMover
                >();
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
                    Editor::EntityMover
                >();

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
                    Editor::EntitySelectBox
                >();

                axis_x_e.get_component<Renderer::Shaders>()->shaders.push_back(axis_shader);
                axis_y_e.get_component<Renderer::Shaders>()->shaders.push_back(axis_shader);
                axis_z_e.get_component<Renderer::Shaders>()->shaders.push_back(axis_shader);
                select_box.get_component<Renderer::Shaders>()->shaders.push_back(select_box_shader);

                axis_x_e.get_component<Editor::EntityMover>()->axis = math::vec3(1.f, 0, 0);
                axis_y_e.get_component<Editor::EntityMover>()->axis = math::vec3(0, 1.f, 0);
                axis_z_e.get_component<Editor::EntityMover>()->axis = math::vec3(0, 0, 1.f);

                axis_x_e.get_component<Renderer::Shape>()->vertex.set_resource(axis_x);
                axis_y_e.get_component<Renderer::Shape>()->vertex.set_resource(axis_y);
                axis_z_e.get_component<Renderer::Shape>()->vertex.set_resource(axis_z);
                select_box.get_component<Renderer::Shape>()->vertex.set_resource(select_box_vert);

                select_box.get_component<Renderer::Rendqueue>()->rend_queue = 0;
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

                    if (_coord == coord_mode::local || _mode == Editor::EntityMover::mover_mode::scale)
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
                    mover.mode = jeecs::Editor::EntityMover::selection;
                else
                    mover.mode = _mode;
                switch (mover.mode)
                {
                case jeecs::Editor::EntityMover::selection:
                    scale.scale = { 0.f, 0.f, 0.f };
                    break;
                case jeecs::Editor::EntityMover::rotation:
                    if (mover.axis.x != 0.f) shape->vertex.set_resource(circ_x);
                    else if (mover.axis.y != 0.f) shape->vertex.set_resource(circ_y);
                    else shape->vertex.set_resource(circ_z);
                    posi.pos = { 0.f, 0.f, 0.f };
                    scale.scale = { 1.f, 1.f, 1.f };
                    break;
                case jeecs::Editor::EntityMover::movement:
                case jeecs::Editor::EntityMover::scale:
                    if (mover.axis.x != 0.f) shape->vertex.set_resource(axis_x);
                    else if (mover.axis.y != 0.f) shape->vertex.set_resource(axis_y);
                    else shape->vertex.set_resource(axis_z);
                    scale.scale = { 1.f, 1.f, 1.f };
                    break;
                default:
                    jeecs::debug::logerr("Unknown mode.");
                    break;
                }

                if (mover.mode != jeecs::Editor::EntityMover::rotation)
                    posi.pos = mover.axis;

            }
            if (mover.mode == Editor::EntityMover::selection)
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

            if (_inputs.r_buttom || !_inputs.l_buttom || nullptr == editing_trans)
                _grab_axis_translation = nullptr;

            if (_grab_axis_translation && _inputs.l_buttom && editing_trans)
            {
                if (_grab_axis_translation == &trans && _camera_porjection)
                {
                    // advise_lock_mouse = true;
                    math::vec2 cur_mouse_pos = _inputs.uniform_mouse_pos;
                    math::vec2 diff = cur_mouse_pos - _grab_last_pos;

                    math::vec4 p0 = trans.world_position;
                    p0.w = 1.0f;
                    p0 = math::mat4trans(_camera_porjection->projection, math::mat4trans(_camera_porjection->view, p0));
                    math::vec4 p1 = trans.world_position + trans.world_rotation * mover.axis;
                    p1.w = 1.0f;
                    p1 = math::mat4trans(_camera_porjection->projection, math::mat4trans(_camera_porjection->view, p1));

                    math::vec2 screen_axis = { p1.x - p0.x,p1.y - p0.y };
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

                    if (mover.mode == Editor::EntityMover::mover_mode::movement && editing_pos_may_null)
                    {
                        editing_pos_may_null->set_global_position(
                            editing_trans->world_position + diff.dot(screen_axis) * (trans.world_rotation * (mover.axis * distance * factor)),
                            *editing_trans,
                            editing_rot_may_null
                        );
                    }
                    else if (mover.mode == Editor::EntityMover::mover_mode::scale && editing_scale_may_null)
                    {
                        editing_scale_may_null->scale += diff.dot(screen_axis) * (mover.axis * distance * factor);
                    }
                    else if (mover.mode == Editor::EntityMover::mover_mode::rotation && editing_rot_may_null)
                    {
                        auto euler = trans.world_rotation * mover.axis * (diff.x + diff.y) * factor * 20.0f;
                        editing_rot_may_null->rot = math::quat(euler.x, euler.y, euler.z) * editing_rot_may_null->rot;
                    }

                    _grab_last_pos = cur_mouse_pos;
                }
            }
            else
            {
                auto result = _camera_ray.intersect_entity(trans, shape);
                bool select_click = _inputs.l_buttom_pushed;

                bool intersected = result.intersected;
                if (intersected && mover.mode == Editor::EntityMover::mover_mode::rotation)
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
                    {
                        _grab_axis_translation = &trans;
                        _grab_last_pos = _inputs.uniform_mouse_pos;
                    }
                    if (!_inputs.l_buttom)
                        color.color.x = 1.0f;
                }
                else
                    color.color.x = 0.0f;
            }

            if (const game_entity* current = _inputs.selected_entity ? &_inputs.selected_entity.value() : nullptr)
            {
                if (auto* etrans = _inputs.selected_entity.value().get_component<Transform::Translation>())
                {
                    float distance =
                        _camera_ortho_porjection == nullptr
                        ? 0.25f * (_camera_pos - etrans->world_position).length()
                        : 1.0f / _camera_ortho_porjection->scale;
                    scale.scale = math::vec3(distance, distance, distance);

                    if (_mode != Editor::EntityMover::mover_mode::rotation)
                        posi.pos = mover.axis * distance;
                }
            }
            else
            {
                scale.scale = math::vec3(0.f, 0.f, 0.f);
            }
        }

        void CommitUpdate()
        {
            _inputs.w = input::keydown(input::keycode::W);
            _inputs.s = input::keydown(input::keycode::S);
            _inputs.a = input::keydown(input::keycode::A);
            _inputs.d = input::keydown(input::keycode::D);
            _inputs.l_tab = input::keydown(input::keycode::TAB);
            _inputs.l_ctrl = input::keydown(input::keycode::L_CTRL);
            _inputs.l_shift = input::keydown(input::keycode::L_SHIFT);
            _inputs.l_buttom = input::mousedown(0, input::mousecode::LEFT);
            _inputs.r_buttom = input::mousedown(0, input::mousecode::RIGHT);
            _inputs.l_buttom_click = input::is_up(_inputs.l_buttom);
            _inputs.l_buttom_pushed = input::first_down(_inputs.l_buttom);
            _inputs.r_buttom_click = input::is_up(_inputs.r_buttom);
            _inputs.r_buttom_pushed = input::first_down(_inputs.r_buttom);
            _inputs.selected_entity = std::nullopt;
            _inputs.delta_time = deltatime();

            if (_inputs._wheel_count_record != INT_MAX)
            {
                _inputs.wheel_delta_count = (int)input::wheel(0).y - _inputs._wheel_count_record;
            }
            _inputs._wheel_count_record = (int)input::wheel(0).y;

            select_from(get_world())
                // 获取被选中的实体
                .exec([this](game_entity e)
                    {
                        if (e.get_euid() == jedbg_get_editing_entity_uid())
                            _inputs.selected_entity = std::optional(e);
                    }
                )
                // Move walker(root)
                .contain<Editor::EditorWalker>()
                .except<Camera::Projection>()
                .exec(&DefaultEditorSystem::MoveWalker)
                // Move walker(camera)
                .contain<Editor::EditorWalker>()
                .exec(&DefaultEditorSystem::CameraWalker)
                // Select entity
                .except<Editor::Invisable>()
                .anyof<Renderer::Shape, Renderer::Shaders, Renderer::Textures>()
                .exec(&DefaultEditorSystem::SelectEntity)
                // Create & create mover!
                .exec(&DefaultEditorSystem::UpdateAndCreateMover)
                .exec([this](
                    Editor::EntitySelectBox&, 
                    Transform::Translation& trans, 
                    Transform::LocalScale& localScale, 
                    Transform::LocalRotation& localRotation)
                    {
                        if (const game_entity* current = _inputs.selected_entity ? &_inputs.selected_entity.value() : nullptr)
                        {
                            float distance =
                                _camera_ortho_porjection == nullptr
                                ? 0.25f * (_camera_pos - trans.world_position).length()
                                : 1.0f / _camera_ortho_porjection->scale;

                            localRotation.rot = math::quat();
                            auto* etrans = _inputs.selected_entity.value().get_component<Transform::Translation>();
                            if (etrans != nullptr)
                            {
                                localScale.scale = etrans->local_scale;
                                if (_coord != coord_mode::local && _mode != Editor::EntityMover::mover_mode::scale)
                                    localRotation.rot = etrans->world_rotation;
                            }

                            if (auto* eshape = _inputs.selected_entity.value().get_component<Renderer::Shape>())
                                localScale.scale = localScale.scale * (
                                    eshape->vertex.has_resource() == false
                                    ? jeecs::math::vec3(1.0f, 1.0f, 0.0f)
                                    : jeecs::math::vec3(
                                        eshape->vertex.get_resource()->resouce()->m_raw_vertex_data->m_size_x,
                                        eshape->vertex.get_resource()->resouce()->m_raw_vertex_data->m_size_y,
                                        eshape->vertex.get_resource()->resouce()->m_raw_vertex_data->m_size_z
                                    ));

                            localScale.scale = 1.05f * localScale.scale;
                        }
                        else
                        {
                            // Hide the mover
                            localScale.scale = math::vec3(0, 0, 0);
                        }
                    }
                )
                // Mover mgr          
                .exec(&DefaultEditorSystem::MoveEntity)
                ;

            if (!_editor_enabled)
                return;

            if (nullptr == _grab_axis_translation)
            {
                auto _set_editing_entity = [](const jeecs::game_entity& e)
                {
                    jedbg_set_editing_entity_uid(e.get_euid());
                };

                if (!selected_list.empty())
                {
                    const game_entity* e = _inputs.selected_entity ? &_inputs.selected_entity.value() : nullptr;
                    if (auto fnd = std::find_if(selected_list.begin(), selected_list.end(),
                        [e](const SelectedResult& s)->bool {return e ? s.entity == *e : false; });
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
                else if (_inputs.l_buttom_pushed)
                    jedbg_set_editing_entity_uid(0);
            }

            je_io_set_lock_mouse(advise_lock_mouse, _inputs.advise_lock_mouse_pos.x, _inputs.advise_lock_mouse_pos.y);

            selected_list.clear();
        }
    };
}
WO_API wo_api wojeapi_store_bad_shader_name(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_string_t shader_path = wo_string(args + 1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (nullptr == badShadersUniform)
        return wo_ret_panic(vm, "Failed to store uniforms for bad shader, entity has not 'Editor::BadShadersUniform'.");

    return wo_ret_pointer(vm, &badShadersUniform->stored_uniforms.emplace_back(jeecs::Editor::BadShadersUniform::bad_shader_data(shader_path)));
}

WO_API wo_api wojeapi_store_bad_shader_uniforms_int(wo_vm vm, wo_value args, size_t argc)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::INT;
    bad_uniform_var.n = (int)wo_int(args + 2);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float(wo_vm vm, wo_value args, size_t argc)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT;
    bad_uniform_var.x = wo_float(args + 2);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float2(wo_vm vm, wo_value args, size_t argc)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT2;
    bad_uniform_var.x = wo_float(args + 2);
    bad_uniform_var.y = wo_float(args + 3);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float3(wo_vm vm, wo_value args, size_t argc)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT3;
    bad_uniform_var.x = wo_float(args + 2);
    bad_uniform_var.y = wo_float(args + 3);
    bad_uniform_var.z = wo_float(args + 4);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float4(wo_vm vm, wo_value args, size_t argc)
{
    auto* bad_shader = &((jeecs::Editor::BadShadersUniform::ok_or_bad_shader*)wo_pointer(args + 0))->get_bad();
    auto& bad_uniform_var = bad_shader->m_vars[wo_string(args + 1)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT4;
    bad_uniform_var.x = wo_float(args + 2);
    bad_uniform_var.y = wo_float(args + 3);
    bad_uniform_var.z = wo_float(args + 4);
    bad_uniform_var.w = wo_float(args + 5);

    return wo_ret_void(vm);
}

inline void update_shader(jegl_shader::unifrom_variables* uni_var, const std::string& uname, jeecs::graphic::shader* new_shad)
{
    switch (uni_var->m_uniform_type)
    {
    case jegl_shader::uniform_type::INT:
        new_shad->set_uniform(uname, uni_var->n);
        break;
    case jegl_shader::uniform_type::FLOAT:
        new_shad->set_uniform(uname, uni_var->x);
        break;
    case jegl_shader::uniform_type::FLOAT2:
        new_shad->set_uniform(uname,
            jeecs::math::vec2(uni_var->x, uni_var->y));
        break;
    case jegl_shader::uniform_type::FLOAT3:
        new_shad->set_uniform(uname,
            jeecs::math::vec3(uni_var->x, uni_var->y, uni_var->z));
        break;
    case jegl_shader::uniform_type::FLOAT4:
        new_shad->set_uniform(uname,
            jeecs::math::vec4(uni_var->x, uni_var->y, uni_var->z, uni_var->w));
        break;
    default: break; // donothing
    }
}

bool _update_bad_shader_to_new_shader(jeecs::Renderer::Shaders* shaders, jeecs::Editor::BadShadersUniform* bad_uniforms)
{
    assert(bad_uniforms != nullptr && shaders != nullptr);
    for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
        if (!ok_or_bad_shader.is_ok())
            return false;

    for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
        shaders->shaders.push_back(ok_or_bad_shader.get_ok());
    return true;
}


WO_API wo_api wojeapi_remove_bad_shader_name(wo_vm vm, wo_value args, size_t argc)
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

WO_API wo_api wojeapi_reload_texture_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    std::string old_texture_path = wo_string(args + 1);
    std::string new_texture_path = wo_string(args + 2);

    auto newtexture = jeecs::graphic::texture::load(new_texture_path);
    if (newtexture == nullptr)
    {
        return wo_ret_bool(vm, false);
    }

    jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>();
    if (textures != nullptr)
    {
        for (auto& texture_res : textures->textures)
        {
            assert(texture_res.m_texture != nullptr && texture_res.m_texture->resouce() != nullptr);
            if (old_texture_path == texture_res.m_texture->resouce()->m_path)
                texture_res.m_texture = newtexture;
        }
    }
    return wo_ret_bool(vm, true);
}
WO_API wo_api wojeapi_reload_shader_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    std::string old_shader_path = wo_string(args + 1);
    std::string new_shader_path = wo_string(args + 2);

    jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>();
    jeecs::Editor::BadShadersUniform* bad_uniforms = entity->get_component<jeecs::Editor::BadShadersUniform>();

    auto bad_shader_generator = [](const std::string& path, const jeecs::basic::resource<jeecs::graphic::shader>& shader) {
        jeecs::Editor::BadShadersUniform::bad_shader_data bad_shader(path);

        auto* uniform_var = shader->resouce()->m_raw_shader_data->m_custom_uniforms;
        while (uniform_var != nullptr)
        {
            bad_shader.m_vars[uniform_var->m_name] = *uniform_var;
            uniform_var = uniform_var->m_next;
        }
        return bad_shader;
    };
    auto copy_shader_generator = [](
        jeecs::basic::resource<jeecs::graphic::shader>* newshader, auto oldshader)
    {
        assert(newshader != nullptr && (*newshader)->resouce()->m_path != nullptr);

        jeecs::basic::resource<jeecs::graphic::shader> new_shader_instance = *newshader;
        *newshader = jeecs::graphic::shader::load(new_shader_instance->resouce()->m_path);

        if constexpr (std::is_same<decltype(oldshader), jeecs::basic::resource<jeecs::graphic::shader>>::value)
        {
            auto* uniform_var = oldshader->resouce()->m_raw_shader_data->m_custom_uniforms;
            while (uniform_var != nullptr)
            {
                switch (uniform_var->m_uniform_type)
                {
                case jegl_shader::uniform_type::INT:
                    new_shader_instance->set_uniform(uniform_var->m_name, uniform_var->n); break;
                case jegl_shader::uniform_type::FLOAT:
                    new_shader_instance->set_uniform(uniform_var->m_name, uniform_var->x); break;
                case jegl_shader::uniform_type::FLOAT2:
                    new_shader_instance->set_uniform(uniform_var->m_name, jeecs::math::vec2(uniform_var->x, uniform_var->y)); break;
                case jegl_shader::uniform_type::FLOAT3:
                    new_shader_instance->set_uniform(uniform_var->m_name, jeecs::math::vec3(uniform_var->x, uniform_var->y, uniform_var->z)); break;
                case jegl_shader::uniform_type::FLOAT4:
                    new_shader_instance->set_uniform(uniform_var->m_name, jeecs::math::vec4(uniform_var->x, uniform_var->y, uniform_var->z, uniform_var->w)); break;
                default:
                    break;
                }
                uniform_var = uniform_var->m_next;
            }
        }
        else
        {
            for (auto& [name, var] : oldshader.m_vars)
            {
                switch (var.m_uniform_type)
                {
                case jegl_shader::uniform_type::INT:
                    new_shader_instance->set_uniform(name, var.n); break;
                case jegl_shader::uniform_type::FLOAT:
                    new_shader_instance->set_uniform(name, var.x); break;
                case jegl_shader::uniform_type::FLOAT2:
                    new_shader_instance->set_uniform(name, jeecs::math::vec2(var.x, var.y)); break;
                case jegl_shader::uniform_type::FLOAT3:
                    new_shader_instance->set_uniform(name, jeecs::math::vec3(var.x, var.y, var.z)); break;
                case jegl_shader::uniform_type::FLOAT4:
                    new_shader_instance->set_uniform(name, jeecs::math::vec4(var.x, var.y, var.z, var.w)); break;
                default:
                    jeecs::debug::logerr("Unsupported type when restore shader(%s)'s uniform named: '%s', skip.", new_shader_instance->resouce()->m_path, name.c_str()); break;
                    break;
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
                if (shader->resouce()->m_path != nullptr && old_shader_path == shader->resouce()->m_path)
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
                    if (ok_shader->resouce()->m_path != nullptr && old_shader_path == ok_shader->resouce()->m_path)
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
        jeecs::basic::resource<jeecs::graphic::shader> new_shader = jeecs::graphic::shader::load(new_shader_path);
        if (new_shader == nullptr)
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
                    if (shader->resouce()->m_path != nullptr && old_shader_path == shader->resouce()->m_path)
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
                        if (ok_shader->resouce()->m_path != nullptr && old_shader_path == ok_shader->resouce()->m_path)
                            ok_or_bad_shader = bad_shader_generator(new_shader_path, ok_shader);
                    }
                }
            }
            shaders->shaders.clear();

            return wo_ret_bool(vm, false);
        }
        else
        {
            // 1.2 OK! replace old shader with new shader.
            if (bad_uniforms == nullptr)
            {
                for (auto& shader : shaders->shaders)
                {
                    assert(shader != nullptr);
                    if (shader->resouce()->m_path != nullptr && old_shader_path == shader->resouce()->m_path)
                        shader = copy_shader_generator(&new_shader, shader);
                }
            }
            else
            {
                for (auto& ok_or_bad_shader : bad_uniforms->stored_uniforms)
                {
                    if (ok_or_bad_shader.is_ok())
                    {
                        auto& ok_shader = ok_or_bad_shader.get_ok();
                        if (ok_shader->resouce()->m_path != nullptr && old_shader_path == ok_shader->resouce()->m_path)
                            ok_or_bad_shader = copy_shader_generator(&new_shader, ok_shader);
                    }
                    else if (ok_or_bad_shader.get_bad().m_path == old_shader_path)
                        ok_or_bad_shader = copy_shader_generator(&new_shader, ok_or_bad_shader.get_bad());
                }

                // Ok, check for update!
                if (_update_bad_shader_to_new_shader(shaders, bad_uniforms))
                    entity->remove_component<jeecs::Editor::BadShadersUniform>();
            }
            return wo_ret_bool(vm, true);
        }
    }
    return wo_ret_bool(vm, true);
}
WO_API wo_api wojeapi_get_bad_shader_list_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::Editor::BadShadersUniform* bad_uniform = entity->get_component<jeecs::Editor::BadShadersUniform>();

    assert(bad_uniform != nullptr);

    wo_value result = wo_push_arr(vm, 0);
    wo_value elem = wo_push_empty(vm);
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
WO_API wo_api wojeapi_setable_editor_system(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::DefaultEditorSystem::_editor_enabled = wo_bool(args + 0);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_update_editor_mouse_pos(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::DefaultEditorSystem::_inputs.uniform_mouse_pos =
        jeecs::math::vec2{ wo_float(args + 0), wo_float(args + 1) };

    jeecs::DefaultEditorSystem::_inputs.advise_lock_mouse_pos =
        jeecs::math::ivec2{ (int)wo_int(args + 2), (int)wo_int(args + 3) };

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_get_editing_mover_mode(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys == nullptr)
        return wo_ret_int(vm, (wo_integer_t)jeecs::Editor::EntityMover::mover_mode::nospecify);
    return wo_ret_int(vm, (wo_integer_t)sys->_mode);
}
WO_API wo_api wojeapi_set_editing_mover_mode(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys != nullptr)
        sys->_mode = (jeecs::Editor::EntityMover::mover_mode)wo_int(args + 1);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_get_editing_coord_mode(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys == nullptr)
        return wo_ret_int(vm, (wo_integer_t)jeecs::DefaultEditorSystem::coord_mode::global);
    return wo_ret_int(vm, (wo_integer_t)sys->_coord);
}
WO_API wo_api wojeapi_set_editing_coord_mode(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world world(wo_pointer(args + 0));
    jeecs::DefaultEditorSystem* sys = world.get_system<jeecs::DefaultEditorSystem>();
    if (sys != nullptr)
        sys->_coord = (jeecs::DefaultEditorSystem::coord_mode)wo_int(args + 1);

    return wo_ret_void(vm);
}