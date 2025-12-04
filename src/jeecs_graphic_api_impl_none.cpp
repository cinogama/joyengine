#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_imgui_backend_api.hpp"

namespace jeecs::graphic::api::none
{
    jegl_context::graphic_impl_context_t
        startup(jegl_context* glthread, const jegl_interface_config*, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) start!");

        jegui_init_none(
            glthread,
            [](jegl_context*, jegl_texture *)
            {
                return (uint64_t)nullptr;
            },
            [](jegl_context*, jegl_shader*) 
            {
            });

        return nullptr;
    }
    void pre_shutdown(jegl_context*, jegl_context::graphic_impl_context_t, bool)
    {
    }
    void shutdown(jegl_context*, jegl_context::graphic_impl_context_t, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) shutdown!");

        jegui_shutdown_none(reboot);
    }

    jegl_update_action pre_update(jegl_context::graphic_impl_context_t)
    {
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    jegl_update_action commit_update(
        jegl_context::graphic_impl_context_t, jegl_update_action)
    {
        jegui_update_none();
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }

    jegl_resource_blob create_shader_blob(jegl_context::graphic_impl_context_t, jegl_shader*)
    {
        return nullptr;
    }
    void close_shader_blob(jegl_context::graphic_impl_context_t, jegl_resource_blob)
    {
    }
    jegl_resource_blob create_texture_blob(jegl_context::graphic_impl_context_t, jegl_texture*)
    {
        return nullptr;
    }
    void close_texture_blob(jegl_context::graphic_impl_context_t, jegl_resource_blob)
    {
    }
    jegl_resource_blob create_vertex_blob(jegl_context::graphic_impl_context_t, jegl_vertex*)
    {
        return nullptr;
    }
    void close_vertex_blob(jegl_context::graphic_impl_context_t, jegl_resource_blob)
    {
    }

    void init_shader(jegl_context::graphic_impl_context_t, jegl_resource_blob, jegl_shader*)
    {
    }
    void init_texture(jegl_context::graphic_impl_context_t, jegl_resource_blob, jegl_texture*)
    {
    }
    void init_vertex(jegl_context::graphic_impl_context_t, jegl_resource_blob, jegl_vertex*)
    {
    }
    void init_framebuffer(jegl_context::graphic_impl_context_t, jegl_frame_buffer*)
    {
    }
    void init_ubuffer(jegl_context::graphic_impl_context_t, jegl_uniform_buffer*)
    {
    }

    void update_shader(jegl_context::graphic_impl_context_t, jegl_shader*)
    {
    }
    void update_texture(jegl_context::graphic_impl_context_t, jegl_texture*)
    {
    }
    void update_vertex(jegl_context::graphic_impl_context_t, jegl_vertex*)
    {
    }
    void update_framebuffer(jegl_context::graphic_impl_context_t, jegl_frame_buffer*)
    {
    }
    void update_ubuffer(jegl_context::graphic_impl_context_t, jegl_uniform_buffer*)
    {
    }

    void close_shader(jegl_context::graphic_impl_context_t, jegl_shader*)
    {
    }
    void close_texture(jegl_context::graphic_impl_context_t, jegl_texture*)
    {
    }
    void close_vertex(jegl_context::graphic_impl_context_t, jegl_vertex*)
    {
    }
    void close_framebuffer(jegl_context::graphic_impl_context_t, jegl_frame_buffer*)
    {
    }
    void close_ubuffer(jegl_context::graphic_impl_context_t, jegl_uniform_buffer*)
    {
    }

    void bind_uniform_buffer(jegl_context::graphic_impl_context_t, jegl_uniform_buffer*)
    {
    }
    bool bind_shader(jegl_context::graphic_impl_context_t, jegl_shader*)
    {
        return true;
    }
    void bind_texture(jegl_context::graphic_impl_context_t, jegl_texture*, size_t)
    {
    }
    void draw_vertex_with_shader(jegl_context::graphic_impl_context_t, jegl_vertex*)
    {
    }

    void bind_framebuffer(
        jegl_context::graphic_impl_context_t,
        jegl_frame_buffer*,
        const int32_t(*)[4],
        const jegl_frame_buffer_clear_operation*)
    {
    }

    void set_uniform(jegl_context::graphic_impl_context_t, uint32_t, jegl_shader::uniform_type, const void*)
    {
    }
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
    write_to_apis->texture_create_blob = create_texture_blob;
    write_to_apis->vertex_create_blob = create_vertex_blob;

    write_to_apis->shader_close_blob = close_shader_blob;
    write_to_apis->texture_close_blob = close_texture_blob;
    write_to_apis->vertex_close_blob = close_vertex_blob;

    write_to_apis->shader_init = init_shader;
    write_to_apis->texture_init = init_texture;
    write_to_apis->vertex_init = init_vertex;
    write_to_apis->framebuffer_init = init_framebuffer;
    write_to_apis->ubuffer_init = init_ubuffer;

    write_to_apis->shader_update = update_shader;
    write_to_apis->texture_update = update_texture;
    write_to_apis->vertex_update = update_vertex;
    write_to_apis->framebuffer_update = update_framebuffer;
    write_to_apis->ubuffer_update = update_ubuffer;

    write_to_apis->shader_close = close_shader;
    write_to_apis->texture_close = close_texture;
    write_to_apis->vertex_close = close_vertex;
    write_to_apis->framebuffer_close = close_framebuffer;
    write_to_apis->ubuffer_close = close_ubuffer;

    write_to_apis->set_uniform = set_uniform;

    write_to_apis->bind_framebuf = bind_framebuffer;

    write_to_apis->bind_uniform_buffer = bind_uniform_buffer;
    write_to_apis->bind_texture = bind_texture;
    write_to_apis->bind_shader = bind_shader;
    write_to_apis->draw_vertex = draw_vertex_with_shader;
}
