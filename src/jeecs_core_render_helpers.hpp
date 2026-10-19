#pragma once

// Private helper shared by the core system headers
// (jeecs_core_graphic_system.hpp, jeecs_core_editor_system.hpp, ...).
// Must be included AFTER "jeecs.hpp" and with JE_IMPL defined.

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif

#include "jeecs.hpp"
#include "jeecs_graphic_render_internal.hpp"

namespace jeecs
{
    // RAII wrapper around the engine's old "draw returns an action" pattern:
    // binds the pipeline/vertex, collects per-draw uniforms, and records the
    // indexed draw when the action goes out of scope. The draw is flushed
    // before the next state change, so record order is preserved.
    struct render_draw_action
    {
        JECS_DISABLE_MOVE_AND_COPY(render_draw_action);

        jegl_command_buffer* m_command;
        const jegl_builtin_uniform_slots* m_builtin_slots;
        bool m_pending;

        render_draw_action(
            jegl_command_buffer* command,
            jegl_pipeline* pipeline,
            jegl_vertex* vertex,
            uint32_t index_count,
            uint32_t instance_count)
            : m_command(command)
            , m_builtin_slots(jegl_pipeline_builtin_uniform_slots(pipeline))
            , m_pending(true)
        {
            _jegl_cmd_begin_deferred_draw(
                command,
                pipeline,
                vertex,
                index_count,
                instance_count);
        }

        ~render_draw_action()
        {
            flush();
        }

        void flush()
        {
            if (m_pending)
            {
                _jegl_cmd_end_deferred_draw(m_command);
                m_pending = false;
            }
        }
    };

    // Sets a builtin uniform on a draw action only when the shader actually
    // exposes it (offset != JEGL_UNIFORM_SLOT_NONE).
#define JE_CHECK_NEED_AND_SET_UNIFORM(ACTION, ITEM, TYPE, ...)                                  \
    do                                                                                          \
    {                                                                                           \
        const auto& _je_uniform_slot = (ACTION).m_builtin_slots->ITEM;                           \
        if (_je_uniform_slot.m_offset != JEGL_UNIFORM_SLOT_NONE)                                 \
            jegl_cmd_set_uniform_##TYPE(                                                         \
                (ACTION).m_command, _je_uniform_slot, __VA_ARGS__);                              \
    } while (0)
}
