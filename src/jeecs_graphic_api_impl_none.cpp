#define JE_IMPL
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/logging.hpp"

#include "jeecs_graphic_imgui_backend_api.hpp"

namespace jeecs::graphic::api::none
{
    je_graphic_impl_context_t
        startup(je_gl_context_t* glthread, const je_gl_interface_config_t*, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) start!");

        jegui_init_none(
            glthread,
            [](je_gl_context_t*, je_gl_texture_2d_t *)
            {
                return (uint64_t)nullptr;
            },
            [](je_gl_context_t*, je_gl_shader_t*)
            {
            });

        return nullptr;
    }
    void pre_shutdown(je_gl_context_t*, je_graphic_impl_context_t, bool)
    {
    }
    void shutdown(je_gl_context_t*, je_graphic_impl_context_t, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) shutdown!");

        jegui_shutdown_none(reboot);
    }

    je_gl_update_action_t pre_update(je_graphic_impl_context_t)
    {
        return je_gl_update_action_t::JE_GL_UPDATE_CONTINUE;
    }
    je_gl_update_action_t commit_update(
        je_graphic_impl_context_t, je_gl_update_action_t)
    {
        jegui_update_none();
        return je_gl_update_action_t::JE_GL_UPDATE_CONTINUE;
    }

    je_gl_resource_blob_t create_shader_blob(je_graphic_impl_context_t, je_gl_shader_t*)
    {
        return nullptr;
    }
    void close_shader_blob(je_graphic_impl_context_t, je_gl_resource_blob_t)
    {
    }
    je_gl_resource_blob_t create_texture_blob_2d(je_graphic_impl_context_t, je_gl_texture_2d_t*)
    {
        return nullptr;
    }
    je_gl_resource_blob_t create_texture_blob_3d(je_graphic_impl_context_t, je_gl_texture_3d_t*)
    {
        return nullptr;
    }
    je_gl_resource_blob_t create_texture_blob_array(je_graphic_impl_context_t, je_gl_texture_array_t*)
    {
        return nullptr;
    }
    je_gl_resource_blob_t create_texture_blob_cube(je_graphic_impl_context_t, je_gl_texture_cube_t*)
    {
        return nullptr;
    }
    je_gl_resource_blob_t create_texture_blob_msaa(je_graphic_impl_context_t, je_gl_texture_msaa_t*)
    {
        return nullptr;
    }
    void close_texture_blob(je_graphic_impl_context_t, je_gl_resource_blob_t)
    {
    }
    je_gl_resource_blob_t create_vertex_blob(je_graphic_impl_context_t, je_gl_vertex_t*)
    {
        return nullptr;
    }
    void close_vertex_blob(je_graphic_impl_context_t, je_gl_resource_blob_t)
    {
    }

    void init_shader(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_shader_t*)
    {
    }
    void init_texture_2d(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_2d_t*)
    {
    }
    void init_texture_3d(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_3d_t*)
    {
    }
    void init_texture_array(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_array_t*)
    {
    }
    void init_texture_cube(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_cube_t*)
    {
    }
    void init_texture_msaa(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_msaa_t*)
    {
    }
    void init_vertex(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_vertex_t*)
    {
    }
    void init_framebuffer(je_graphic_impl_context_t, je_gl_frame_buffer_t*)
    {
    }
    void init_ubuffer(je_graphic_impl_context_t, je_gl_uniform_buffer_t*)
    {
    }

    void update_shader(je_graphic_impl_context_t, je_gl_shader_t*)
    {
    }
    void update_texture_2d(je_graphic_impl_context_t, je_gl_texture_2d_t*)
    {
    }
    void update_texture_3d(je_graphic_impl_context_t, je_gl_texture_3d_t*)
    {
    }
    void update_texture_array(je_graphic_impl_context_t, je_gl_texture_array_t*)
    {
    }
    void update_texture_cube(je_graphic_impl_context_t, je_gl_texture_cube_t*)
    {
    }
    void update_texture_msaa(je_graphic_impl_context_t, je_gl_texture_msaa_t*)
    {
    }
    void update_vertex(je_graphic_impl_context_t, je_gl_vertex_t*)
    {
    }
    void update_framebuffer(je_graphic_impl_context_t, je_gl_frame_buffer_t*)
    {
    }
    void update_ubuffer(je_graphic_impl_context_t, je_gl_uniform_buffer_t*)
    {
    }

    void close_shader(je_graphic_impl_context_t, je_gl_shader_t*)
    {
    }
    void close_texture_2d(je_graphic_impl_context_t, je_gl_texture_2d_t*)
    {
    }
    void close_texture_3d(je_graphic_impl_context_t, je_gl_texture_3d_t*)
    {
    }
    void close_texture_array(je_graphic_impl_context_t, je_gl_texture_array_t*)
    {
    }
    void close_texture_cube(je_graphic_impl_context_t, je_gl_texture_cube_t*)
    {
    }
    void close_texture_msaa(je_graphic_impl_context_t, je_gl_texture_msaa_t*)
    {
    }
    void close_vertex(je_graphic_impl_context_t, je_gl_vertex_t*)
    {
    }
    void close_framebuffer(je_graphic_impl_context_t, je_gl_frame_buffer_t*)
    {
    }
    void close_ubuffer(je_graphic_impl_context_t, je_gl_uniform_buffer_t*)
    {
    }

                                            // Render API (v2) no-ops.
    void render_pipeline_init(je_graphic_impl_context_t, je_gl_pipeline_t*)
    {
    }
    void render_pipeline_close(je_graphic_impl_context_t, je_gl_pipeline_t*)
    {
    }
    void render_pipeline_bind(je_graphic_impl_context_t, je_gl_pipeline_t*)
    {
    }
    void render_sampler_init(je_graphic_impl_context_t, je_gl_sampler_t*)
    {
    }
    void render_sampler_close(je_graphic_impl_context_t, je_gl_sampler_t*)
    {
    }
    void render_sampler_bind(je_graphic_impl_context_t, uint32_t, je_gl_sampler_t*)
    {
    }
    void render_begin_pass(
        je_graphic_impl_context_t, je_gl_frame_buffer_t*, const je_gl_viewport_t*)
    {
    }
    void render_clear_color(je_graphic_impl_context_t, uint32_t, const float[4])
    {
    }
    void render_clear_depth(je_graphic_impl_context_t, float)
    {
    }
    void render_end_pass(je_graphic_impl_context_t)
    {
    }
    void render_bind_vertex(je_graphic_impl_context_t, je_gl_vertex_t*)
    {
    }
    void render_bind_uniform_buffer(je_graphic_impl_context_t, je_gl_uniform_buffer_t*)
    {
    }
    void render_bind_texture_2d(je_graphic_impl_context_t, uint32_t, je_gl_texture_2d_t*)
    {
    }
    void render_bind_texture_3d(je_graphic_impl_context_t, uint32_t, je_gl_texture_3d_t*)
    {
    }
    void render_bind_texture_array(je_graphic_impl_context_t, uint32_t, je_gl_texture_array_t*)
    {
    }
    void render_bind_texture_cube(je_graphic_impl_context_t, uint32_t, je_gl_texture_cube_t*)
    {
    }
    void render_draw_indexed(
        je_graphic_impl_context_t,
        uint32_t, uint32_t, uint32_t,
        const void*, uint32_t)
    {
    }
    void render_draw(
        je_graphic_impl_context_t,
        uint32_t, uint32_t, uint32_t,
        const void*, uint32_t)
    {
    }
    void render_commit_begin(je_graphic_impl_context_t)
    {
    }
    void render_commit_end(je_graphic_impl_context_t)
    {
    }
}

void je_graphic_using_none_apis(je_gl_graphic_api_t* write_to_apis)
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

    write_to_apis->pipeline_init = render_pipeline_init;
    write_to_apis->pipeline_close = render_pipeline_close;
    write_to_apis->pipeline_bind = render_pipeline_bind;

    write_to_apis->sampler_init = render_sampler_init;
    write_to_apis->sampler_close = render_sampler_close;
    write_to_apis->sampler_bind = render_sampler_bind;

    write_to_apis->begin_pass = render_begin_pass;
    write_to_apis->clear_color = render_clear_color;
    write_to_apis->clear_depth = render_clear_depth;
    write_to_apis->end_pass = render_end_pass;

    write_to_apis->bind_vertex = render_bind_vertex;
    write_to_apis->bind_uniform_buffer = render_bind_uniform_buffer;
    write_to_apis->bind_texture_2d = render_bind_texture_2d;
    write_to_apis->bind_texture_3d = render_bind_texture_3d;
    write_to_apis->bind_texture_array = render_bind_texture_array;
    write_to_apis->bind_texture_cube = render_bind_texture_cube;

    write_to_apis->draw_indexed = render_draw_indexed;
    write_to_apis->draw = render_draw;

    write_to_apis->commit_begin = render_commit_begin;
    write_to_apis->commit_end = render_commit_end;
}
