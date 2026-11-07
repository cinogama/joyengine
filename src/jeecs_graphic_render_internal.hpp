#pragma once

// Private definitions shared by the render-API recorder
// (jeecs_graphic_cmd.cpp), the resource lifetime layer
// (jeecs_graphic_api_basic.cpp) and the backends.
// Must be included after "jeecs.hpp" with JE_IMPL defined.

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs.hpp"

// Refcounted CPU-side pipeline description. The backend owns any native
// pipeline object and stores it in m_backend_ptr.
struct je_gl_pipeline_t
{
    je_gl_resource_handle_t m_handle;

    je_gl_context_t* m_context;
    je_gl_shader_t* m_shader;
    je_gl_pipeline_state_t m_state;
    je_gl_builtin_uniform_slots_t m_builtin_slots;

    void* m_backend_ptr;
    bool m_backend_initialized;
};
static_assert(offsetof(je_gl_pipeline_t, m_handle) == 0);

// Refcounted sampler description. The backend owns the native sampler object
// and stores it in m_backend_ptr.
struct je_gl_sampler_t
{
    je_gl_resource_handle_t m_handle;

    je_gl_context_t* m_context;
    je_gl_sampler_desc_t m_desc;

    void* m_backend_ptr;
    bool m_backend_initialized;
};
static_assert(offsetof(je_gl_sampler_t, m_handle) == 0);

namespace jeecs::graphic
{
    // Defined in jeecs_graphic_api_basic.cpp. The command-buffer commit path
    // asserts that it runs on the context's graphic thread.
    extern thread_local je_gl_context_t* _current_graphic_thread;

    // Result of preparing a resource on the graphic thread.
    // READY      : already initialized and not modified
    // UPDATED    : backend *_update hook was called
    // INITIALIZED: backend *_init hook was called
    // INVALID    : wrong graphic thread / invalid resource
    enum class ResourcePrepareResult : uint8_t
    {
        INVALID,
        READY,
        UPDATED,
        INITIALIZED,
    };

    // Resource preparation helpers implemented in jeecs_graphic_api_basic.cpp.
    // They perform lazy init / update on the graphic thread and do not bind or
    // draw anything. The render-API commit loop calls them before issuing the
    // corresponding backend call.
    ResourcePrepareResult prepare_shader_resource(je_gl_shader_t* shader);
    ResourcePrepareResult prepare_pipeline_resource(je_gl_pipeline_t* pipeline);
    ResourcePrepareResult prepare_sampler_resource(je_gl_sampler_t* sampler);
    bool prepare_vertex_resource(je_gl_vertex_t* vertex);
    bool prepare_uniform_buffer_resource(je_gl_uniform_buffer_t* buffer);
    bool prepare_framebuffer_resource(je_gl_frame_buffer_t* framebuffer);

    bool prepare_texture_2d_resource(je_gl_texture_2d_t* texture);
    bool prepare_texture_3d_resource(je_gl_texture_3d_t* texture);
    bool prepare_texture_array_resource(je_gl_texture_array_t* texture);
    bool prepare_texture_cube_resource(je_gl_texture_cube_t* texture);
    bool prepare_texture_msaa_resource(je_gl_texture_msaa_t* texture);

    void init_resource_handle(
        je_gl_resource_handle_t* resource_handle,
        const char* path_may_null_if_builtin);

    // Patch the built-in JE_NDC_SCALE uniform inside an already-copied CPU
    // uniform block. DX11/Metal flip y for offscreen targets; Vulkan flips it
    // only for the sRGB encode pass.
    inline void patch_ndc_scale(
        void* uniform_data, uint32_t uniform_size,
        uint32_t ndc_offset, bool flip_y)
    {
        if (ndc_offset == jeecs::graphic::INVALID_UNIFORM_LOCATION
            || ndc_offset + sizeof(float) * 4 > uniform_size)
            return;

        const float ndc_scale[4] = { 1.f, flip_y ? -1.f : 1.f, 1.f, 1.f };
        memcpy(
            static_cast<uint8_t*>(uniform_data) + ndc_offset,
            ndc_scale, sizeof(ndc_scale));
    }
}
