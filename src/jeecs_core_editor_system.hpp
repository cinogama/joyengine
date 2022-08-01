#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

namespace jeecs
{
    namespace Editor
    {
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
            // Editor will create a entity with EntityMoverRoot,
            // and DefaultEditorSystem should handle this entity hand create 3 mover for x,y,z axis
            math::vec3 axis = {};
        };
        struct EntityMoverRoot
        {
            bool init = false;
        };
        struct EditorLife
        {
            int life;
        };
    }

    struct DefaultEditorSystem : public game_shared_system
    {
        math::vec3 _begin_drag;
        bool _drag_viewing = false;

        math::quat _camera_rot;
        math::ray _camera_ray;
        math::ray::intersect_result intersect_result;
        const Camera::Projection* _camera_porjection;
        game_entity intersect_entity;

        void EditorWalkerWork(
            Transform::LocalPosition* position,
            Transform::LocalRotation* rotation,
            read<Transform::Translation> trans)
        {
            using namespace input;
            using namespace math;

            if (first_down(keydown(keycode::MOUSE_R_BUTTION)))
            {
                _begin_drag = mousepos(0);
                _drag_viewing = false;
            }

            if (keydown(keycode::MOUSE_R_BUTTION))
            {
                if (_drag_viewing || (mousepos(0) - _begin_drag).length() >= 0.01)
                {
                    _drag_viewing = true;
                    je_io_lock_mouse(true);

                    rotation->rot = rotation->rot * quat(0, 30.f * mousepos(0).x, 0);
                }
                if (keydown(keycode::W))
                    position->pos += _camera_rot * vec3(0, 0, 0.5);
                if (keydown(keycode::S))
                    position->pos += _camera_rot * vec3(0, 0, -0.5);
                if (keydown(keycode::A))
                    position->pos += _camera_rot * vec3(-0.5, 0, 0);
                if (keydown(keycode::D))
                    position->pos += _camera_rot * vec3(0.5, 0, 0);
            }
            else
                je_io_lock_mouse(false);
        }

        void EditorCameraWork(
            Transform::LocalPosition* position,
            Transform::LocalRotation* rotation,
            read<Camera::Projection> proj,
            read<Transform::Translation> trans)
        {
            using namespace input;
            using namespace math;

            auto mouse_position = mousepos(0);

            _camera_ray = math::ray(trans, proj, mouse_position, false);
            _camera_porjection = proj;

            if (input::keydown(input::keycode::L_CTRL)
                && input::first_down(input::keydown(input::keycode::MOUSE_L_BUTTION)))
            {
                // 创建一条射线
                static basic::resource<graphic::vertex> line = new graphic::vertex(
                    graphic::vertex::type::LINES,
                    { 0,0,0,
                      0,0,1000 }, { 3 });
                static basic::resource<graphic::shader> shad = new graphic::shader("je/debug/drawline.shader", R"(
import je.shader;

using VAO_STRUCT vin = struct {
    vertex : float3,
};
using v2f = struct {
    pos : float4,
};
using fout = struct {
    color : float4
};

let vert = \v: vin = v2f{ pos = je_mvp * vertex_pos }
                where vertex_pos = float4(v.vertex, 1.);;
let frag = \f: v2f = fout{ color = float4(1, 1, 1, 1) };;

)");

                if (jeecs::game_world gworld = get_world())
                {
                    auto e = gworld.add_entity<
                        jeecs::Transform::LocalPosition,
                        jeecs::Transform::LocalRotation,
                        jeecs::Transform::LocalToWorld,
                        jeecs::Transform::Translation,
                        jeecs::Renderer::Shaders,
                        jeecs::Renderer::Shape,
                        Editor::EditorLife,
                        Editor::Invisable
                    >();

                    e.get_component<jeecs::Transform::LocalPosition>()->pos = _camera_ray.orgin;
                    e.get_component<jeecs::Transform::LocalRotation>()->rot = math::quat::rotation(_camera_ray.direction, vec3(0, 0, 1));
                    e.get_component<jeecs::Renderer::Shaders>()->shaders.push_back(shad);
                    e.get_component<jeecs::Renderer::Shape>()->vertex = line;
                    e.get_component<Editor::EditorLife>()->life = 240;
                }
            }

            if (_drag_viewing && keydown(keycode::MOUSE_R_BUTTION))
            {
                rotation->rot = rotation->rot * quat(30.f * -mouse_position.y, 0, 0);
                _camera_rot = trans->world_rotation;
            }
        }
        void LifeDyingEntity(

            game_entity entity,
            Editor::EditorLife* life)
        {
            if (life->life-- < 0)
                entity.close();
        }

        void SelectEntity(
            game_entity entity,
            read<Transform::Translation> trans,
            maynot<read<Renderer::Shape>> shape)
        {
            if (input::keydown(input::keycode::MOUSE_L_BUTTION))
            {
                auto result = _camera_ray.intersect_entity(trans, shape);

                if (result.intersected && result.distance < intersect_result.distance)
                {
                    intersect_result = result;
                    intersect_entity = entity;
                }
            }
        }

        void UpdateSelectedEntity()
        {
            if (nullptr == _grab_axis_translation)
            {
                if (intersect_result.intersected)
                    jedbg_set_editing_entity(&intersect_entity);
                //else if(input::keydown(input::keycode::MOUSE_L_BUTTION))
                //    jedbg_set_editing_entity(nullptr);
            }
            intersect_result = math::ray::intersect_result();
        }

        void EntityMoverRootMgr(
            game_entity mover_entity,
            read<Transform::ChildAnchor> anchor,
            Transform::LocalPosition* position,
            Transform::LocalRotation* rotation,
            Transform::LocalScale* scale,
            Editor::EntityMoverRoot* mover)
        {
            if (!mover->init)
            {
                mover->init = true;

                static basic::resource<graphic::vertex>
                    axis_x =
                    new graphic::vertex(graphic::vertex::type::LINES,
                        { -0.5f,0,0,        0.25f,0,0,
                          0.5f,0,0,      1,0,0 },
                        { 3, 3 }),
                    axis_y =
                    new graphic::vertex(graphic::vertex::type::LINES,
                        { 0,-0.5f,0,        0,0.25f,0,
                          0,0.5f,0,      0,1,0 },
                        { 3, 3 }),
                    axis_z =
                    new graphic::vertex(graphic::vertex::type::LINES,
                        { 0,0,-0.5f,        0,0,0.25f,
                          0,0,0.5f,      0,0,1 },
                        { 3, 3 });

                axis_x->resouce()->m_raw_vertex_data->m_size_y
                    = axis_x->resouce()->m_raw_vertex_data->m_size_z
                    = axis_y->resouce()->m_raw_vertex_data->m_size_x
                    = axis_y->resouce()->m_raw_vertex_data->m_size_z
                    = axis_z->resouce()->m_raw_vertex_data->m_size_x
                    = axis_z->resouce()->m_raw_vertex_data->m_size_y
                    = 0.1f;

                static basic::resource<graphic::shader>
                    axis_shader = new graphic::shader("je/debug/mover_axis.shader",
                        R"(
import je.shader;

ZTEST (ALWAYS);

using VAO_STRUCT vin = struct {
    vertex : float3,
    color  : float3
};
using v2f = struct {
    pos : float4,
    color : float3
};
using fout = struct {
    color : float4
};

let vert = \v: vin = v2f { pos = je_mvp * vertex_pos, 
                           color = v.color } 
                where vertex_pos = float4(v.vertex, 1.);;

let frag = \f: v2f = fout{ color = float4(show_color, 1) }
                where show_color = lerp(f.color, float3(1., 1., 1.), ratio),
                           ratio = step(float(0.5), high_light),
                      high_light = uniform("high_light", float(0.0));;

)");
                game_world current_world = mover_entity.game_world();
                game_entity axis_x_e = current_world.add_entity<
                    Transform::LocalPosition,
                    Transform::LocalToParent,
                    Transform::Translation,
                    Renderer::Shaders,
                    Renderer::Shape,
                    Renderer::Rendqueue,
                    Editor::Invisable,
                    Editor::EntityMover
                >();
                game_entity axis_y_e = current_world.add_entity<
                    Transform::LocalPosition,
                    Transform::LocalToParent,
                    Transform::Translation,
                    Renderer::Shaders,
                    Renderer::Shape,
                    Renderer::Rendqueue,
                    Editor::Invisable,
                    Editor::EntityMover
                >();
                game_entity axis_z_e = current_world.add_entity<
                    Transform::LocalPosition,
                    Transform::LocalToParent,
                    Transform::Translation,
                    Renderer::Shaders,
                    Renderer::Shape,
                    Renderer::Rendqueue,
                    Editor::Invisable,
                    Editor::EntityMover
                >();

                axis_x_e.get_component<Renderer::Shaders>()->shaders.push_back(new graphic::shader(axis_shader));
                axis_y_e.get_component<Renderer::Shaders>()->shaders.push_back(new graphic::shader(axis_shader));
                axis_z_e.get_component<Renderer::Shaders>()->shaders.push_back(new graphic::shader(axis_shader));

                axis_x_e.get_component<Editor::EntityMover>()->axis = math::vec3(1, 0, 0);
                axis_y_e.get_component<Editor::EntityMover>()->axis = math::vec3(0, 1, 0);
                axis_z_e.get_component<Editor::EntityMover>()->axis = math::vec3(0, 0, 1);

                axis_x_e.get_component<Renderer::Shape>()->vertex = axis_x;
                axis_y_e.get_component<Renderer::Shape>()->vertex = axis_y;
                axis_z_e.get_component<Renderer::Shape>()->vertex = axis_z;

                axis_x_e.get_component<Renderer::Rendqueue>()->rend_queue =
                    axis_y_e.get_component<Renderer::Rendqueue>()->rend_queue =
                    axis_z_e.get_component<Renderer::Rendqueue>()->rend_queue = 100000;

                axis_x_e.get_component<Transform::LocalPosition>()->pos = math::vec3(0.5f, 0, 0);
                axis_y_e.get_component<Transform::LocalPosition>()->pos = math::vec3(0, 0.5f, 0);
                axis_z_e.get_component<Transform::LocalPosition>()->pos = math::vec3(0, 0, 0.5f);

                axis_x_e.get_component<Transform::LocalToParent>()->parent_uid =
                    axis_y_e.get_component<Transform::LocalToParent>()->parent_uid =
                    axis_z_e.get_component<Transform::LocalToParent>()->parent_uid =
                    anchor->anchor_uid;
            }
            if (game_entity* current = jedbg_get_editing_entity())
            {
                if (auto* trans = current->get_component<Transform::Translation>())
                {
                    position->pos = trans->world_position;
                    rotation->rot = trans->world_rotation;

                    float distance = 0.25f * (_camera_ray.orgin - trans->world_position).length();

                    scale->scale = math::vec3(distance, distance, distance);
                }
            }
        }

        const Transform::Translation* _grab_axis_translation = nullptr;
        math::vec2 _grab_last_pos;

        void EntityMoverMgr(
            read<Editor::EntityMover> mover,
            read<Transform::Translation> trans,
            read<Renderer::Shape> shape,
            Renderer::Shaders* shaders)
        {
            auto* editing_entity = jedbg_get_editing_entity();
            Transform::LocalPosition* editing_pos = editing_entity
                ? editing_entity->get_component<Transform::LocalPosition>()
                : nullptr;
            Transform::Translation* editing_trans = editing_entity
                ? editing_entity->get_component<Transform::Translation>()
                : nullptr;
            Transform::LocalRotation* editing_rot_may_null = editing_entity
                ? editing_entity->get_component<Transform::LocalRotation>()
                : nullptr;

            if (!input::keydown(input::keycode::MOUSE_L_BUTTION) || nullptr == editing_pos || nullptr == editing_trans)
                _grab_axis_translation = nullptr;

            if (_grab_axis_translation && input::keydown(input::keycode::MOUSE_L_BUTTION) && editing_pos && editing_trans)
            {
                if (_grab_axis_translation == trans && _camera_porjection)
                {
                    math::vec4 p0 = trans->world_position;
                    p0.w = 1.0f;
                    p0 = math::mat4trans(_camera_porjection->projection, math::mat4trans(_camera_porjection->view, p0));
                    math::vec4 p1 = trans->world_position + trans->world_rotation * mover->axis;
                    p1.w = 1.0f;
                    p1 = math::mat4trans(_camera_porjection->projection, math::mat4trans(_camera_porjection->view, p1));

                    math::vec2 screen_axis = { p1.x - p0.x,p1.y - p0.y };
                    screen_axis = screen_axis.unit();

                    math::vec2 cur_mouse_pos = input::mousepos(0);
                    math::vec2 diff = cur_mouse_pos - _grab_last_pos;

                    editing_pos->set_world_position(
                        editing_trans->world_position + diff.dot(screen_axis) * (trans->world_rotation * mover->axis),
                        editing_trans,
                        editing_rot_may_null
                    );

                    _grab_last_pos = cur_mouse_pos;
                }
            }
            else
            {
                auto result = _camera_ray.intersect_entity(trans, shape);
                if (result.intersected)
                {
                    if (input::keydown(input::keycode::MOUSE_L_BUTTION))
                    {
                        _grab_axis_translation = trans;
                        _grab_last_pos = input::mousepos(0);
                    }
                    shaders->set_uniform("high_light", 1.0f);
                }
                else
                    shaders->set_uniform("high_light", 0.0f);
            }
        }

        DefaultEditorSystem(game_universe universe)
            : game_shared_system(universe)
        {
            register_system_func(&DefaultEditorSystem::EditorWalkerWork,
                {
                    contain<Editor::EditorWalker>(),
                    except<Camera::Projection>(),
                    system_read(&_camera_rot),
                });
            register_system_func(&DefaultEditorSystem::EditorCameraWork,
                {
                    contain<Editor::EditorWalker>(),
                    system_write(&_camera_ray),
                    system_write(&_camera_rot),
                    system_write(&_camera_porjection),
                });
            register_system_func(&DefaultEditorSystem::SelectEntity,
                {
                    except<Editor::Invisable>(),
                    system_read_updated(&_camera_ray),
                    system_write(&intersect_result),
                    system_write(&intersect_entity),
                });
            register_system_func(&DefaultEditorSystem::LifeDyingEntity);
            register_system_func(&DefaultEditorSystem::EntityMoverRootMgr);
            register_system_func(&DefaultEditorSystem::EntityMoverMgr,
                {
                    system_read_updated(&_camera_ray),
                    system_read_updated(&_camera_porjection),
                });
            register_system_func(&DefaultEditorSystem::UpdateSelectedEntity,
                {
                    system_read_updated(&intersect_result),
                    system_read_updated(&intersect_entity),
                    after(&DefaultEditorSystem::EntityMoverMgr),
                });
        }
    };
}