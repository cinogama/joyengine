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
#include <atomic>

namespace jeecs
{
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
            basic::string path;
            static void JERefRegsiter(typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Prefab::path, "path");
            }
        };
        struct EntityId
        {
            inline static std::atomic<je_DebugEid> ALLOCATED_EID;

            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(EntityId);
            je_DebugEid eid;

            EntityId()
                : eid(1 + ALLOCATED_EID.fetch_add(1, std::memory_order::relaxed))
            {
            }
            EntityId(const EntityId&)
                : eid(1 + ALLOCATED_EID.fetch_add(1, std::memory_order::relaxed))
            {
            }
            EntityId(EntityId&& another)
                : eid(another.eid)
            {
            }

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
                auto reloaded = jeecs::graphic::shader::load(
                    gcontext, new_shader_instance->resource()->m_handle.m_path_may_null_if_builtin);
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

je_DebugEid jedbg_get_entity_uid(const jeecs::game_entity* e)
{
    auto* eid = e->get_component<jeecs::Editor::EntityId>();
    if (eid == nullptr)
    {
        eid = e->add_component<jeecs::Editor::EntityId>();
        return 0 /* invalid */;
    }
    return eid->eid;
}
