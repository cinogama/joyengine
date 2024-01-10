#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_imgui_backend_api.hpp"

namespace jeecs::graphic::api::none
{
    jegl_thread::custom_thread_data_t
        startup(jegl_thread* gthread, const jegl_interface_config* config, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) start!");

        jegui_init_none(
            [](auto* res)->void* {return nullptr; },
            [](auto* res){});

        return nullptr;
    }
    void pre_shutdown(jegl_thread*, jegl_thread::custom_thread_data_t, bool)
    {
    }
    void shutdown(jegl_thread*, jegl_thread::custom_thread_data_t ctx, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (None) shutdown!");

        jegui_shutdown_none(reboot);
    }

    bool pre_update(jegl_thread::custom_thread_data_t)
    {
        return true;
    }
    jegl_graphic_api::update_result update(jegl_thread::custom_thread_data_t)
    {
        return jegl_graphic_api::update_result::DO_FRAME_WORK;
    }
    bool late_update(jegl_thread::custom_thread_data_t ctx)
    {
        jegui_update_none();
        return true;
    }

    jegl_resource_blob create_resource_blob(jegl_thread::custom_thread_data_t, jegl_resource*)
    {
        return nullptr;
    }
    void close_resource_blob(jegl_thread::custom_thread_data_t, jegl_resource_blob)
    {
    }

    void init_resource(jegl_thread::custom_thread_data_t, jegl_resource_blob, jegl_resource*)
    {
    }
    void using_resource(jegl_thread::custom_thread_data_t, jegl_resource*)
    {
    }
    void close_resource(jegl_thread::custom_thread_data_t, jegl_resource*)
    {
    }

    void draw_vertex_with_shader(jegl_thread::custom_thread_data_t, jegl_resource*)
    {
    }

    void bind_texture(jegl_thread::custom_thread_data_t, jegl_resource*, size_t)
    {
    }

    void set_rend_to_framebuffer(jegl_thread::custom_thread_data_t, jegl_resource*, size_t, size_t, size_t, size_t)
    {
    }
    void clear_framebuffer_color(jegl_thread::custom_thread_data_t, float[4])
    {
    }
    void clear_framebuffer_depth(jegl_thread::custom_thread_data_t)
    {
    }

    void set_uniform(jegl_thread::custom_thread_data_t, uint32_t, jegl_shader::uniform_type, const void*)
    {
    }
}

void jegl_using_none_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::none;

    write_to_apis->init_interface = startup;
    write_to_apis->pre_shutdown_interface = pre_shutdown;
    write_to_apis->shutdown_interface = shutdown;

    write_to_apis->pre_update_interface = pre_update;
    write_to_apis->update_interface = update;
    write_to_apis->late_update_interface = late_update;

    write_to_apis->create_resource_blob = create_resource_blob;
    write_to_apis->close_resource_blob = close_resource_blob;

    write_to_apis->init_resource = init_resource;
    write_to_apis->using_resource = using_resource;
    write_to_apis->close_resource = close_resource;

    write_to_apis->draw_vertex = draw_vertex_with_shader;
    write_to_apis->bind_texture = bind_texture;

    write_to_apis->set_rend_buffer = set_rend_to_framebuffer;
    write_to_apis->clear_rend_buffer_color = clear_framebuffer_color;
    write_to_apis->clear_rend_buffer_depth = clear_framebuffer_depth;

    write_to_apis->set_uniform = set_uniform;
}