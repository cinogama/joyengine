#pragma once

// Private definitions shared by the render-API recorder
// (jeecs_graphic_cmd.cpp), the resource lifetime layer
// (jeecs_graphic_api_basic.cpp) and the backends.
// Must be included after "jeecs.hpp" with JE_IMPL defined.

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check the including translation unit.
#endif

#include "jeecs.hpp"

namespace jeecs::graphic
{
    // Defined in jeecs_graphic_api_basic.cpp. The command-buffer commit path
    // asserts that it runs on the context's graphic thread.
    extern thread_local jegl_context* _current_graphic_thread;
}

// Result of preparing a resource on the graphic thread.
// READY      : already initialized and not modified
// UPDATED    : backend *_update hook was called
// INITIALIZED: backend *_init hook was called
// INVALID    : wrong graphic thread / invalid resource
enum class jegl_resource_prepare_result : uint8_t
{
    INVALID,
    READY,
    UPDATED,
    INITIALIZED,
};

// Refcounted CPU-side pipeline description. The backend owns any native
// pipeline object and stores it in m_backend_ptr.
struct jegl_pipeline
{
    jegl_resource_handle m_handle;

    jegl_context* m_context;
    jegl_shader* m_shader;
    jegl_pipeline_state m_state;
    jegl_builtin_uniform_slots m_builtin_slots;

    void* m_backend_ptr;
    bool m_backend_initialized;
};
static_assert(offsetof(jegl_pipeline, m_handle) == 0);

// Refcounted sampler description. The backend owns the native sampler object
// and stores it in m_backend_ptr.
struct jegl_sampler
{
    jegl_resource_handle m_handle;

    jegl_context* m_context;
    jegl_sampler_desc m_desc;

    void* m_backend_ptr;
    bool m_backend_initialized;
};
static_assert(offsetof(jegl_sampler, m_handle) == 0);

// Resource preparation helpers implemented in jeecs_graphic_api_basic.cpp.
// They perform lazy init / update on the graphic thread and do not bind or
// draw anything. The render-API commit loop calls them before issuing the
// corresponding backend call.
jegl_resource_prepare_result _jegl_prepare_shader_resource(jegl_shader* shader);
jegl_resource_prepare_result _jegl_prepare_pipeline_resource(jegl_pipeline* pipeline);
jegl_resource_prepare_result _jegl_prepare_sampler_resource(jegl_sampler* sampler);
bool _jegl_prepare_vertex_resource(jegl_vertex* vertex);
bool _jegl_prepare_uniform_buffer_resource(jegl_uniform_buffer* buffer);
bool _jegl_prepare_framebuffer_resource(jegl_frame_buffer* framebuffer);

bool _jegl_prepare_texture_2d_resource(jegl_texture_2d* texture);
bool _jegl_prepare_texture_3d_resource(jegl_texture_3d* texture);
bool _jegl_prepare_texture_array_resource(jegl_texture_array* texture);
bool _jegl_prepare_texture_cube_resource(jegl_texture_cube* texture);
bool _jegl_prepare_texture_msaa_resource(jegl_texture_msaa* texture);

// Deferred draw used by the engine's render systems and the Woolang bindings.
// Mirrors the old "draw returns an action, uniforms are set on it, the next
// operation flushes it" pattern. Public users should prefer the explicit
// jegl_cmd_set_uniform_* + jegl_cmd_draw_indexed ordering.
void _jegl_cmd_begin_deferred_draw(
    jegl_command_buffer* cmd,
    jegl_pipeline* pipeline,
    jegl_vertex* vertex,
    uint32_t index_count,
    uint32_t instance_count);
void _jegl_cmd_end_deferred_draw(jegl_command_buffer* cmd);
