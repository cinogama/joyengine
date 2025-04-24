#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_imgui_backend_api.hpp"

namespace jeecs::graphic::api::none
{
    jegl_context::userdata_t
        startup(jegl_context* glthread, const jegl_interface_config*, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) start!");

        jegui_init_none(
            glthread,
            [](jegl_context*, jegl_resource*) 
            {
                return (uint64_t)nullptr; 
            },
            [](jegl_context*, jegl_resource*)
            {
            });

        return nullptr;
    }
    void pre_shutdown(jegl_context*, jegl_context::userdata_t, bool)
    {
    }
    void shutdown(jegl_context*, jegl_context::userdata_t, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) shutdown!");

        jegui_shutdown_none(reboot);
    }

    jegl_update_action pre_update(jegl_context::userdata_t)
    {
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    jegl_update_action commit_update(
        jegl_context::userdata_t, jegl_update_action)
    {
        jegui_update_none();
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }

    jegl_resource_blob create_resource_blob(jegl_context::userdata_t, jegl_resource*)
    {
        return nullptr;
    }
    void close_resource_blob(jegl_context::userdata_t, jegl_resource_blob)
    {
    }

    void create_resource(jegl_context::userdata_t, jegl_resource_blob, jegl_resource*)
    {
    }
    void using_resource(jegl_context::userdata_t, jegl_resource*)
    {
    }
    void close_resource(jegl_context::userdata_t, jegl_resource*)
    {
    }

    void bind_uniform_buffer(jegl_context::userdata_t, jegl_resource*)
    {
    }
    void bind_shader(jegl_context::userdata_t, jegl_resource*)
    {
    }
    void bind_texture(jegl_context::userdata_t, jegl_resource*, size_t)
    {
    }
    void draw_vertex_with_shader(jegl_context::userdata_t, jegl_resource*)
    {
    }

    void bind_framebuffer(jegl_context::userdata_t, jegl_resource*, size_t, size_t, size_t, size_t)
    {
    }
    void clear_framebuffer_color(jegl_context::userdata_t, float[4])
    {
    }
    void clear_framebuffer_depth(jegl_context::userdata_t)
    {
    }

    void set_uniform(jegl_context::userdata_t, uint32_t, jegl_shader::uniform_type, const void*)
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

    write_to_apis->create_resource_blob_cache = create_resource_blob;
    write_to_apis->close_resource_blob_cache = close_resource_blob;

    write_to_apis->create_resource = create_resource;
    write_to_apis->using_resource = using_resource;
    write_to_apis->close_resource = close_resource;

    write_to_apis->bind_uniform_buffer = bind_uniform_buffer;
    write_to_apis->bind_texture = bind_texture;
    write_to_apis->bind_shader = bind_shader;
    write_to_apis->draw_vertex = draw_vertex_with_shader;

    write_to_apis->bind_framebuf = bind_framebuffer;
    write_to_apis->clear_frame_color = clear_framebuffer_color;
    write_to_apis->clear_frame_depth = clear_framebuffer_depth;

    write_to_apis->set_uniform = set_uniform;
}