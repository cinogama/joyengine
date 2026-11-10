#pragma once

// Private helper shared by the core system headers
// (jeecs_core_graphic_system.hpp, jeecs_core_editor_system.hpp, ...).
// Must be included AFTER the public graphic headers and with JE_IMPL defined.

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/graphic.hpp"
#include "jeecs_graphic_render_internal.hpp"

namespace jeecs
{
    // Pairs a command buffer with the builtin uniform slots of the pipeline
    // bound to it, so JE_CHECK_NEED_AND_SET_UNIFORM can set builtin uniforms
    // between je_graphic_cmd_bind_pipeline/bind_vertex and the explicit
    // je_graphic_cmd_draw_indexed call.
    struct render_builtin_uniforms
    {
        je_gl_command_buffer_t* m_command;
        const je_gl_builtin_uniform_slots_t* m_builtin_slots;

        render_builtin_uniforms(
            je_gl_command_buffer_t* command,
            je_gl_pipeline_t* pipeline)
            : m_command(command)
            , m_builtin_slots(je_graphic_pipeline_builtin_uniform_slots(pipeline))
        {
        }
    };

    // Sets a builtin uniform on a draw action only when the shader actually
    // exposes it (offset != JE_GL_UNIFORM_SLOT_NONE).
#define JE_CHECK_NEED_AND_SET_UNIFORM(ACTION, ITEM, TYPE, ...)                                  \
    do                                                                                          \
    {                                                                                           \
        const auto& _je_uniform_slot = (ACTION).m_builtin_slots->ITEM;                           \
        if (_je_uniform_slot.m_offset != JE_GL_UNIFORM_SLOT_NONE)                                 \
            je_graphic_cmd_set_uniform_##TYPE(                                                   \
                (ACTION).m_command, _je_uniform_slot, __VA_ARGS__);                              \
    } while (0)
}
