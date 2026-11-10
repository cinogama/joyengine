#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#ifndef JE_ENABLE_DEBUG_API
#   define JE_ENABLE_DEBUG_API
#endif
#include "jeecs/basic.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"
#include "jeecs_core_render_helpers.hpp"

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
            inline static std::atomic<je_debug_eid_t> ALLOCATED_EID;

            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(EntityId);
            je_debug_eid_t eid;

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
        };
    }
}
