#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_GLES320_GAPI

#include "GLES3/gl32.h"

namespace jeecs::graphic::api::gles320
{
    jegl_thread::custom_thread_data_t gles_startup_todo(jegl_thread*, const jegl_interface_config*, bool)
    {
        return (jegl_thread::custom_thread_data_t)1;
    }
    void gles_shutdown_todo(jegl_thread*, jegl_thread::custom_thread_data_t, bool)
    {
    }

    bool gles_update_todo(jegl_thread::custom_thread_data_t)
    {
        return true;
    }

    jegl_resource_blob gles_create_resource_blob_todo(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
        return nullptr;
    }
    void gles_close_resource_blob_todo(jegl_thread::custom_thread_data_t ctx, jegl_resource_blob blob)
    {
    }

    void gles_init_resource_todo(jegl_thread::custom_thread_data_t ctx, jegl_resource_blob blob, jegl_resource* resource)
    {
    }
    void gles_using_resource_todo(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
    }
    void gles_close_resource_todo(jegl_thread::custom_thread_data_t, jegl_resource* resource)
    {
    }

    void gles_draw_vertex_with_shader_todo(jegl_thread::custom_thread_data_t, jegl_resource* vert)
    {
    }

    void gles_bind_texture_todo(jegl_thread::custom_thread_data_t, jegl_resource* texture, size_t pass)
    {
    }

    void gles_set_rend_to_framebuffer_todo(jegl_thread::custom_thread_data_t ctx, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
    {
    }
    void gles_clear_framebuffer_color_todo(jegl_thread::custom_thread_data_t, float color[4])
    {
    }
    void gles_clear_framebuffer_depth_todo(jegl_thread::custom_thread_data_t)
    {
    }

    void gles_set_uniform_todo(jegl_thread::custom_thread_data_t ctx, uint32_t location, jegl_shader::uniform_type type, const void* val)
    {
    }
}

void jegl_using_opengles320_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::gles320;

    write_to_apis->init_interface = gles_startup_todo;
    write_to_apis->shutdown_interface = gles_shutdown_todo;

    write_to_apis->pre_update_interface = gles_update_todo;
    write_to_apis->update_interface = gles_update_todo;
    write_to_apis->late_update_interface = gles_update_todo;

    write_to_apis->create_resource_blob = gles_create_resource_blob_todo;
    write_to_apis->close_resource_blob = gles_close_resource_blob_todo;

    write_to_apis->init_resource = gles_init_resource_todo;
    write_to_apis->using_resource = gles_using_resource_todo;
    write_to_apis->close_resource = gles_close_resource_todo;

    write_to_apis->draw_vertex = gles_draw_vertex_with_shader_todo;
    write_to_apis->bind_texture = gles_bind_texture_todo;

    write_to_apis->set_rend_buffer = gles_set_rend_to_framebuffer_todo;
    write_to_apis->clear_rend_buffer_color = gles_clear_framebuffer_color_todo;
    write_to_apis->clear_rend_buffer_depth = gles_clear_framebuffer_depth_todo;

    write_to_apis->set_uniform = gles_set_uniform_todo;
}
#else
void jegl_using_opengles320_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("GLES320 not available.");
}
#endif