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
        struct Name
        {
            jeecs::string name;
        };
        struct Invisable
        {
            // Entity with this component will not display in editor, and will not be saved.
        };
        struct Anchor
        {
            typing::uid_t uid = je_uid_generate();

            static void JERefRegsiter()
            {
                typing::register_member(&Anchor::uid, "uid");
            }
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
        // Used for store uniform vars of failed-shader in entity. used for 'update' shaders
        struct BadShadersUniform
        {
            using uniform_inform = std::map<std::string, jegl_shader::unifrom_variables>;
            std::map<std::string, uniform_inform> uniforms;
        };
    }

    struct DefaultEditorSystem : public game_system
    {
        inline static bool _editor_enabled = true;

        math::vec3 _begin_drag;
        bool _drag_viewing = false;

        math::quat _camera_rot;
        math::ray _camera_ray;

        const Camera::Projection* _camera_porjection;

        struct input_msg
        {
            bool w = false;
            bool s = false;
            bool a = false;
            bool d = false;

            bool l_ctrl = false;

            bool l_buttom = false;
            bool r_buttom = false;

            bool l_buttom_click = false;
            bool l_buttom_pushed = false;
            bool r_buttom_click = false;
            bool r_buttom_pushed = false;

            jeecs::math::vec2 uniform_mouse_pos = {};
            jeecs::math::ivec2 advise_lock_mouse_pos = {};

            // Why write an empty constructor function here?
            // It's a bug of clang/gcc, fuck!
            input_msg()noexcept {}
        };

        inline static input_msg _inputs = {};

        DefaultEditorSystem(game_world w)
            : game_system(w)
        {

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
        std::set<SelectedResult> selected_list;

        const Transform::Translation* _grab_axis_translation = nullptr;
        math::vec2 _grab_last_pos;

        bool advise_lock_mouse = false;

        void MoveWalker(Transform::LocalPosition& position, Transform::LocalRotation& rotation, Transform::Translation& trans)
        {
            using namespace input;
            using namespace math;

            if (_inputs.r_buttom_pushed)
            {
                _begin_drag = _inputs.uniform_mouse_pos;
                _drag_viewing = false;
            }

            if (_inputs.r_buttom)
            {
                if (_drag_viewing || (_inputs.uniform_mouse_pos - _begin_drag).length() >= 0.01f)
                {
                    _drag_viewing = true;
                    advise_lock_mouse = true;

                    rotation.rot = rotation.rot * quat(0, 30.f * _inputs.uniform_mouse_pos.x, 0);
                }
                if (_inputs.w)
                    position.pos += _camera_rot * vec3(0, 0, 5.f / 60.f);
                if (_inputs.s)
                    position.pos += _camera_rot * vec3(0, 0, -5.f / 60.f);
                if (_inputs.a)
                    position.pos += _camera_rot * vec3(-5.f / 60.f, 0, 0);
                if (_inputs.d)
                    position.pos += _camera_rot * vec3(5.f / 60.f, 0, 0);
            }
            else
                advise_lock_mouse = false;
        }

        void CameraWalker(
            Transform::LocalRotation& rotation,
            Camera::Projection& proj,
            Transform::Translation& trans)
        {
            using namespace input;
            using namespace math;

            auto mouse_position = _inputs.uniform_mouse_pos;

            _camera_ray = math::ray(trans, proj, mouse_position, false);
            _camera_porjection = &proj;

            if (_inputs.l_ctrl && _inputs.l_buttom_click)
            {
                // һ
                static basic::resource<graphic::vertex> line = new graphic::vertex(
                    graphic::vertex::type::LINES,
                    { 0,0,0,
                      0,0,1000 }, { 3 });
                static basic::resource<graphic::shader> shad = new graphic::shader("*/builtin/drawline.shader", R"(
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
            where vertex_pos = float4(v.vertex, 1.);;

        public let frag = 
        \f: v2f = fout{ color = float4(1., 1., 1., 1.) };;
        
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

            if (_drag_viewing && _inputs.r_buttom)
            {
                rotation.rot = rotation.rot * quat(30.f * -mouse_position.y, 0, 0);
                _camera_rot = trans.world_rotation;
            }
        }

        void DebugLifeEntity(game_entity entity, Editor::EditorLife& life)
        {
            if (life.life-- < 0)
                entity.close();
        }

        void SelectEntity(game_entity entity, Transform::Translation& trans, Renderer::Shape* shape)
        {
            if (_inputs.l_buttom_pushed)
            {
                auto result = _camera_ray.intersect_entity(trans, shape);

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
                    axis_shader = new graphic::shader("*/builtin/mover_axis.shader",
                        R"(
import je.shader;
        
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
    color : float4
};
        
public let vert = 
\v: vin = v2f { pos = je_mvp * vertex_pos, 
                color = v.color } 
    where vertex_pos = float4::create(v.vertex, 1.);;
        
public let frag = 
\f: v2f = fout{ color = float4::create(show_color, 1.) }
    where show_color = lerp(f.color, float3::new(1., 1., 1.), ratio),
                ratio = step(float::new(0.5), high_light),
            high_light = uniform("high_light", float::new(0.));;
        
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
                    anchor.uid;
            }
            if (const game_entity* current = jedbg_get_editing_entity())
            {
                if (auto* trans = current->get_component<Transform::Translation>())
                {
                    position.pos = trans->world_position;
                    rotation.rot = trans->world_rotation;

                    float distance = 0.25f * (_camera_ray.orgin - trans->world_position).length();

                    scale.scale = math::vec3(distance, distance, distance);
                }
            }
            else
            {
                // Hide the mover
                scale.scale = math::vec3(0, 0, 0);
            }

        }

        void MoveEntity(
            Editor::EntityMover& mover,
            Transform::Translation& trans,
            Renderer::Shape* shape,
            Renderer::Shaders& shaders)
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

            if (!_inputs.l_buttom || nullptr == editing_pos || nullptr == editing_trans)
                _grab_axis_translation = nullptr;

            if (_grab_axis_translation && _inputs.l_buttom && editing_pos && editing_trans)
            {
                if (_grab_axis_translation == &trans && _camera_porjection)
                {
                    // advise_lock_mouse = true;

                    math::vec4 p0 = trans.world_position;
                    p0.w = 1.0f;
                    p0 = math::mat4trans(_camera_porjection->projection, math::mat4trans(_camera_porjection->view, p0));
                    math::vec4 p1 = trans.world_position + trans.world_rotation * mover.axis;
                    p1.w = 1.0f;
                    p1 = math::mat4trans(_camera_porjection->projection, math::mat4trans(_camera_porjection->view, p1));

                    math::vec2 screen_axis = { p1.x - p0.x,p1.y - p0.y };
                    screen_axis = screen_axis.unit();

                    math::vec2 cur_mouse_pos = _inputs.uniform_mouse_pos;
                    math::vec2 diff = cur_mouse_pos - _grab_last_pos;

                    editing_pos->set_world_position(
                        editing_trans->world_position + diff.dot(screen_axis) * (trans.world_rotation * mover.axis),
                        *editing_trans,
                        editing_rot_may_null
                    );

                    _grab_last_pos = cur_mouse_pos;
                }
            }
            else
            {
                auto result = _camera_ray.intersect_entity(trans, shape);
                bool select_click = _inputs.l_buttom_pushed;

                if (result.intersected)
                {
                    if (select_click)
                    {
                        _grab_axis_translation = &trans;
                        _grab_last_pos = _inputs.uniform_mouse_pos;
                    }
                    if (!_inputs.l_buttom)
                        shaders.set_uniform("high_light", 1.0f);
                }
                else
                    shaders.set_uniform("high_light", 0.0f);
            }
        }

        void Update()
        {
            if (!_editor_enabled)
                return;

            _inputs.w = input::keydown(input::keycode::W);
            _inputs.s = input::keydown(input::keycode::S);
            _inputs.a = input::keydown(input::keycode::A);
            _inputs.d = input::keydown(input::keycode::D);
            _inputs.l_ctrl = input::keydown(input::keycode::L_CTRL);
            _inputs.l_buttom = input::keydown(input::keycode::MOUSE_L_BUTTION);
            _inputs.r_buttom = input::keydown(input::keycode::MOUSE_R_BUTTION);
            _inputs.l_buttom_click = input::is_up(_inputs.l_buttom);
            _inputs.l_buttom_pushed = input::first_down(_inputs.l_buttom);
            _inputs.r_buttom_click = input::is_up(_inputs.r_buttom);
            _inputs.r_buttom_pushed = input::first_down(_inputs.r_buttom);

            select_from(get_world())
                // Move walker(root)
                .exec(&DefaultEditorSystem::MoveWalker).contain<Editor::EditorWalker>().except<Camera::Projection>()
                // Move walker(camera)
                .exec(&DefaultEditorSystem::CameraWalker).contain<Editor::EditorWalker>()
                // Let life entity die.
                .exec(&DefaultEditorSystem::DebugLifeEntity)
                // Select entity
                .exec(&DefaultEditorSystem::SelectEntity).except<Editor::Invisable>()
                // Create & create mover!
                .exec(&DefaultEditorSystem::UpdateAndCreateMover)
                // Mover mgr
                .exec(&DefaultEditorSystem::MoveEntity);

            if (nullptr == _grab_axis_translation)
            {
                if (!selected_list.empty())
                {
                    const game_entity* e = jedbg_get_editing_entity();
                    if (auto fnd = std::find_if(selected_list.begin(), selected_list.end(),
                        [e](const SelectedResult& s)->bool {return e ? s.entity == *e : false; });
                        fnd != selected_list.end())
                    {
                        if (++fnd == selected_list.end())
                            jedbg_set_editing_entity(&selected_list.begin()->entity);
                        else
                            jedbg_set_editing_entity(&fnd->entity);
                    }
                    else
                        jedbg_set_editing_entity(&selected_list.begin()->entity);
                }
                else if (_inputs.l_buttom_pushed)
                    jedbg_set_editing_entity(nullptr);
            }

            je_io_lock_mouse(advise_lock_mouse, _inputs.advise_lock_mouse_pos.x, _inputs.advise_lock_mouse_pos.y);

            selected_list.clear();
        }
    };
}

WO_API wo_api wojeapi_store_bad_shader_uniforms_int(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_string_t shader_path = wo_string(args + 1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (nullptr == badShadersUniform)
        return wo_ret_panic(vm, "Failed to store uniforms for bad shader, entity has not 'Editor::BadShadersUniform'.");

    auto& bad_uniform_var = badShadersUniform->uniforms[shader_path][wo_string(args + 2)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::INT;
    bad_uniform_var.n = wo_int(args + 3);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_string_t shader_path = wo_string(args + 1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (nullptr == badShadersUniform)
        return wo_ret_panic(vm, "Failed to store uniforms for bad shader, entity has not 'Editor::BadShadersUniform'.");

    auto& bad_uniform_var = badShadersUniform->uniforms[shader_path][wo_string(args + 2)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT;
    bad_uniform_var.x = wo_float(args + 3);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float2(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_string_t shader_path = wo_string(args + 1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (nullptr == badShadersUniform)
        return wo_ret_panic(vm, "Failed to store uniforms for bad shader, entity has not 'Editor::BadShadersUniform'.");

    auto& bad_uniform_var = badShadersUniform->uniforms[shader_path][wo_string(args + 2)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT2;
    bad_uniform_var.x = wo_float(args + 3);
    bad_uniform_var.y = wo_float(args + 4);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_string_t shader_path = wo_string(args + 1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (nullptr == badShadersUniform)
        return wo_ret_panic(vm, "Failed to store uniforms for bad shader, entity has not 'Editor::BadShadersUniform'.");

    auto& bad_uniform_var = badShadersUniform->uniforms[shader_path][wo_string(args + 2)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT3;
    bad_uniform_var.x = wo_float(args + 3);
    bad_uniform_var.y = wo_float(args + 4);
    bad_uniform_var.z = wo_float(args + 5);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_store_bad_shader_uniforms_float4(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_string_t shader_path = wo_string(args + 1);

    jeecs::Editor::BadShadersUniform* badShadersUniform = entity->get_component<jeecs::Editor::BadShadersUniform>();
    if (nullptr == badShadersUniform)
        return wo_ret_panic(vm, "Failed to store uniforms for bad shader, entity has not 'Editor::BadShadersUniform'.");

    auto& bad_uniform_var = badShadersUniform->uniforms[shader_path][wo_string(args + 2)];

    bad_uniform_var.m_uniform_type = jegl_shader::uniform_type::FLOAT4;
    bad_uniform_var.x = wo_float(args + 3);
    bad_uniform_var.y = wo_float(args + 4);
    bad_uniform_var.z = wo_float(args + 5);
    bad_uniform_var.w = wo_float(args + 6);

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

WO_API wo_api wojeapi_reload_shader_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::Renderer::Shaders* shads = entity->get_component<jeecs::Renderer::Shaders>();

    std::string old_shad = wo_string(args + 1);
    std::string new_shad = wo_string(args + 2);

    jeecs::Editor::BadShadersUniform* bad_uniforms = entity->get_component<jeecs::Editor::BadShadersUniform>();

    bool ok = true;

    if (shads)
    {
        for (auto& shad : shads->shaders)
        {
            auto* shad_res = shad->resouce();
            assert(shad_res); // NOTE: Some failed built in shader may cause this abort.
            if (shad_res->m_path && shad_res->m_path == old_shad)
            {
                // 1. load new shader, if failed, store uniforms
                jeecs::basic::resource<jeecs::graphic::shader> new_shader = new jeecs::graphic::shader(new_shad);
                if (!new_shader->enabled())
                {
                    if (nullptr == bad_uniforms)
                        bad_uniforms = entity->add_component<jeecs::Editor::BadShadersUniform>();

                    assert(bad_uniforms != nullptr);

                    auto& new_bad_shad_uniform_buf = bad_uniforms->uniforms[new_shad];

                    // 1.1 If bad uniforms already has old_shader's data, update them..
                    auto fnd = bad_uniforms->uniforms.find(old_shad);
                    if (fnd != bad_uniforms->uniforms.end())
                    {
                        assert(shad->enabled() == false);
                        auto res = fnd->second;
                        bad_uniforms->uniforms.erase(fnd->first);

                        new_bad_shad_uniform_buf = res;
                    }
                    else if (shad->enabled())
                    {
                        // Store old-enabled-shader data.
                        auto uni_var = shad_res->m_raw_shader_data->m_custom_uniforms;
                        while (uni_var)
                        {
                            new_bad_shad_uniform_buf[uni_var->m_name] = *uni_var;
                            uni_var = uni_var->m_next;
                        }
                    }
                    ok = false;
                }
                else
                {
                    if (shad->enabled())
                    {
                        auto uni_var = shad_res->m_raw_shader_data->m_custom_uniforms;
                        while (uni_var)
                        {
                            update_shader(uni_var, uni_var->m_name, new_shader);
                            uni_var = uni_var->m_next;
                        }
                    }
                    else if (bad_uniforms)
                    {
                        auto& old_bad_shad_uniform_buf = bad_uniforms->uniforms[old_shad];
                        for (auto& [name, dat] : old_bad_shad_uniform_buf)
                            update_shader(&dat, name, new_shader);
                        bad_uniforms->uniforms.erase(old_shad);
                    }
                }
                shad = new_shader;
            }
        }
    }
    return wo_ret_bool(vm, ok);
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
