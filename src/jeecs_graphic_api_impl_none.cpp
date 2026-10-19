#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_imgui_backend_api.hpp"

namespace jeecs::graphic::api::none
{
    je_GraphicImplContext
        startup(jegl_context* glthread, const jegl_interface_config*, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) start!");

        jegui_init_none(
            glthread,
            [](jegl_context*, jegl_texture_2d *)
            {
                return (uint64_t)nullptr;
            },
            [](jegl_context*, jegl_shader*)
            {
            });

        return nullptr;
    }
    void pre_shutdown(jegl_context*, je_GraphicImplContext, bool)
    {
    }
    void shutdown(jegl_context*, je_GraphicImplContext, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) shutdown!");

        jegui_shutdown_none(reboot);
    }

    jegl_update_action pre_update(je_GraphicImplContext)
    {
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    jegl_update_action commit_update(
        je_GraphicImplContext, jegl_update_action)
    {
        jegui_update_none();
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }

    jegl_resource_blob create_shader_blob(je_GraphicImplContext, jegl_shader*)
    {
        return nullptr;
    }
    void close_shader_blob(je_GraphicImplContext, jegl_resource_blob)
    {
    }
    jegl_resource_blob create_texture_blob_2d(je_GraphicImplContext, jegl_texture_2d*)
    {
        return nullptr;
    }
    jegl_resource_blob create_texture_blob_3d(je_GraphicImplContext, jegl_texture_3d*)
    {
        return nullptr;
    }
    jegl_resource_blob create_texture_blob_array(je_GraphicImplContext, jegl_texture_array*)
    {
        return nullptr;
    }
    jegl_resource_blob create_texture_blob_cube(je_GraphicImplContext, jegl_texture_cube*)
    {
        return nullptr;
    }
    jegl_resource_blob create_texture_blob_msaa(je_GraphicImplContext, jegl_texture_msaa*)
    {
        return nullptr;
    }
    void close_texture_blob(je_GraphicImplContext, jegl_resource_blob)
    {
    }
    jegl_resource_blob create_vertex_blob(je_GraphicImplContext, jegl_vertex*)
    {
        return nullptr;
    }
    void close_vertex_blob(je_GraphicImplContext, jegl_resource_blob)
    {
    }

    void init_shader(je_GraphicImplContext, jegl_resource_blob, jegl_shader*)
    {
    }
    void init_texture_2d(je_GraphicImplContext, jegl_resource_blob, jegl_texture_2d*)
    {
    }
    void init_texture_3d(je_GraphicImplContext, jegl_resource_blob, jegl_texture_3d*)
    {
    }
    void init_texture_array(je_GraphicImplContext, jegl_resource_blob, jegl_texture_array*)
    {
    }
    void init_texture_cube(je_GraphicImplContext, jegl_resource_blob, jegl_texture_cube*)
    {
    }
    void init_texture_msaa(je_GraphicImplContext, jegl_resource_blob, jegl_texture_msaa*)
    {
    }
    void init_vertex(je_GraphicImplContext, jegl_resource_blob, jegl_vertex*)
    {
    }
    void init_framebuffer(je_GraphicImplContext, jegl_frame_buffer*)
    {
    }
    void init_ubuffer(je_GraphicImplContext, jegl_uniform_buffer*)
    {
    }

    void update_shader(je_GraphicImplContext, jegl_shader*)
    {
    }
    void update_texture_2d(je_GraphicImplContext, jegl_texture_2d*)
    {
    }
    void update_texture_3d(je_GraphicImplContext, jegl_texture_3d*)
    {
    }
    void update_texture_array(je_GraphicImplContext, jegl_texture_array*)
    {
    }
    void update_texture_cube(je_GraphicImplContext, jegl_texture_cube*)
    {
    }
    void update_texture_msaa(je_GraphicImplContext, jegl_texture_msaa*)
    {
    }
    void update_vertex(je_GraphicImplContext, jegl_vertex*)
    {
    }
    void update_framebuffer(je_GraphicImplContext, jegl_frame_buffer*)
    {
    }
    void update_ubuffer(je_GraphicImplContext, jegl_uniform_buffer*)
    {
    }

    void close_shader(je_GraphicImplContext, jegl_shader*)
    {
    }
    void close_texture_2d(je_GraphicImplContext, jegl_texture_2d*)
    {
    }
    void close_texture_3d(je_GraphicImplContext, jegl_texture_3d*)
    {
    }
    void close_texture_array(je_GraphicImplContext, jegl_texture_array*)
    {
    }
    void close_texture_cube(je_GraphicImplContext, jegl_texture_cube*)
    {
    }
    void close_texture_msaa(je_GraphicImplContext, jegl_texture_msaa*)
    {
    }
    void close_vertex(je_GraphicImplContext, jegl_vertex*)
    {
    }
    void close_framebuffer(je_GraphicImplContext, jegl_frame_buffer*)
    {
    }
    void close_ubuffer(je_GraphicImplContext, jegl_uniform_buffer*)
    {
    }

                                            // Render API (v2) no-ops.
    void render_pipeline_init(je_GraphicImplContext, jegl_pipeline*)
    {
    }
    void render_pipeline_close(je_GraphicImplContext, jegl_pipeline*)
    {
    }
    void render_pipeline_bind(je_GraphicImplContext, jegl_pipeline*)
    {
    }
    void render_sampler_init(je_GraphicImplContext, jegl_sampler*)
    {
    }
    void render_sampler_close(je_GraphicImplContext, jegl_sampler*)
    {
    }
    void render_sampler_bind(je_GraphicImplContext, uint32_t, jegl_sampler*)
    {
    }
    void render_begin_pass(
        je_GraphicImplContext, jegl_frame_buffer*, const jegl_viewport*)
    {
    }
    void render_clear_color(je_GraphicImplContext, uint32_t, const float[4])
    {
    }
    void render_clear_depth(je_GraphicImplContext, float)
    {
    }
    void render_end_pass(je_GraphicImplContext)
    {
    }
    void render_bind_vertex(je_GraphicImplContext, jegl_vertex*)
    {
    }
    void render_bind_uniform_buffer(je_GraphicImplContext, jegl_uniform_buffer*)
    {
    }
    void render_bind_texture_2d(je_GraphicImplContext, uint32_t, jegl_texture_2d*)
    {
    }
    void render_bind_texture_3d(je_GraphicImplContext, uint32_t, jegl_texture_3d*)
    {
    }
    void render_bind_texture_array(je_GraphicImplContext, uint32_t, jegl_texture_array*)
    {
    }
    void render_bind_texture_cube(je_GraphicImplContext, uint32_t, jegl_texture_cube*)
    {
    }
    void render_draw_indexed(
        je_GraphicImplContext,
        uint32_t, uint32_t, uint32_t,
        const void*, uint32_t)
    {
    }
    void render_draw(
        je_GraphicImplContext,
        uint32_t, uint32_t, uint32_t,
        const void*, uint32_t)
    {
    }
    void render_commit_begin(je_GraphicImplContext)
    {
    }
    void render_commit_end(je_GraphicImplContext)
    {
    }

    static const jegl_graphic_api_render _none_render_apis = {
        JEGL_RENDER_API_ABI_VERSION,
        (uint32_t)sizeof(jegl_graphic_api_render),

        render_pipeline_init,
        render_pipeline_close,
        render_pipeline_bind,

        render_sampler_init,
        render_sampler_close,
        render_sampler_bind,

        render_begin_pass,
        render_clear_color,
        render_clear_depth,
        render_end_pass,

        render_bind_vertex,
        render_bind_uniform_buffer,
        render_bind_texture_2d,
        render_bind_texture_3d,
        render_bind_texture_array,
        render_bind_texture_cube,

        render_draw_indexed,
        render_draw,

        render_commit_begin,
        render_commit_end,
    };
}

void jegl_using_none_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::none;

    write_to_apis->interface_startup = startup;
    write_to_apis->interface_shutdown_before_resource_release = pre_shutdown;
    write_to_apis->interface_shutdown = shutdown;

    write_to_apis->update_frame_ready = pre_update;
    write_to_apis->update_draw_commit = commit_update;

    write_to_apis->shader_create_blob = create_shader_blob;
    write_to_apis->texture_2d_create_blob = create_texture_blob_2d;
    write_to_apis->texture_3d_create_blob = create_texture_blob_3d;
    write_to_apis->texture_array_create_blob = create_texture_blob_array;
    write_to_apis->texture_cube_create_blob = create_texture_blob_cube;
    write_to_apis->texture_msaa_create_blob = create_texture_blob_msaa;
    write_to_apis->vertex_create_blob = create_vertex_blob;

    write_to_apis->shader_close_blob = close_shader_blob;
    write_to_apis->texture_close_blob = close_texture_blob;
    write_to_apis->vertex_close_blob = close_vertex_blob;

    write_to_apis->shader_init = init_shader;
    write_to_apis->texture_2d_init = init_texture_2d;
    write_to_apis->texture_3d_init = init_texture_3d;
    write_to_apis->texture_array_init = init_texture_array;
    write_to_apis->texture_cube_init = init_texture_cube;
    write_to_apis->texture_msaa_init = init_texture_msaa;
    write_to_apis->vertex_init = init_vertex;
    write_to_apis->framebuffer_init = init_framebuffer;
    write_to_apis->ubuffer_init = init_ubuffer;

    write_to_apis->shader_update = update_shader;
    write_to_apis->texture_2d_update = update_texture_2d;
    write_to_apis->texture_3d_update = update_texture_3d;
    write_to_apis->texture_array_update = update_texture_array;
    write_to_apis->texture_cube_update = update_texture_cube;
    write_to_apis->texture_msaa_update = update_texture_msaa;
    write_to_apis->vertex_update = update_vertex;
    write_to_apis->framebuffer_update = update_framebuffer;
    write_to_apis->ubuffer_update = update_ubuffer;

    write_to_apis->shader_close = close_shader;
    write_to_apis->texture_2d_close = close_texture_2d;
    write_to_apis->texture_3d_close = close_texture_3d;
    write_to_apis->texture_array_close = close_texture_array;
    write_to_apis->texture_cube_close = close_texture_cube;
    write_to_apis->texture_msaa_close = close_texture_msaa;
    write_to_apis->vertex_close = close_vertex;
    write_to_apis->framebuffer_close = close_framebuffer;
    write_to_apis->ubuffer_close = close_ubuffer;




    write_to_apis->m_render_apis = &_none_render_apis;
}
