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
                // typing::register_member(guard, &EntityId::eid, "eid");
            }
        };
    }
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

je_DebugEid jedbg_get_entity_uid(const je_GameEntity* e)
{
    auto* eid = static_cast<jeecs::Editor::EntityId*>(
        je_ecs_world_entity_get_component(
            e, jeecs::typing::id<jeecs::Editor::EntityId>()));
    if (eid == nullptr)
    {
        (void)je_ecs_world_entity_add_component(
            e, jeecs::typing::id<jeecs::Editor::EntityId>());
        return 0 /* invalid */;
    }
    return eid->eid;
}

JE_API void jedbg_set_entity_uid(const je_GameEntity* e, je_DebugEid uid)
{
    auto* eid = static_cast<jeecs::Editor::EntityId*>(
        je_ecs_world_entity_get_component(
            e, jeecs::typing::id<jeecs::Editor::EntityId>()));
    
    if (eid == nullptr)
    {
        eid = static_cast<jeecs::Editor::EntityId*>(
            je_ecs_world_entity_add_component(
                e, jeecs::typing::id<jeecs::Editor::EntityId>()));
    }

    eid->eid = uid;
}
