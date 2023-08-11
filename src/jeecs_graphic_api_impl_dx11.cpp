#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_OS_WINDOWS

#include <D3D11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "winmm.lib")

#include "imgui_impl_dx11.h"

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("DirectX 11 Graphic API not support now.");

    //write_to_apis->prepare_interface = gl_prepare;
    //write_to_apis->finish_interface = gl_finish;

    //write_to_apis->init_interface = gl_startup;
    //write_to_apis->pre_update_interface = gl_pre_update;
    //write_to_apis->update_interface = gl_update;
    //write_to_apis->late_update_interface = gl_lateupdate;
    //write_to_apis->shutdown_interface = gl_shutdown;

    //write_to_apis->get_windows_size = gl_get_windows_size;

    //write_to_apis->init_resource = gl_init_resource;
    //write_to_apis->using_resource = gl_using_resource;
    //write_to_apis->close_resource = gl_close_resource;

    //write_to_apis->draw_vertex = gl_draw_vertex_with_shader;
    //write_to_apis->bind_texture = gl_bind_texture;

    //write_to_apis->set_rend_buffer = gl_set_rend_to_framebuffer;
    //write_to_apis->clear_rend_buffer = gl_clear_framebuffer;
    //write_to_apis->clear_rend_buffer_color = gl_clear_framebuffer_color;
    //write_to_apis->clear_rend_buffer_depth = gl_clear_framebuffer_depth;

    //write_to_apis->get_uniform_location = gl_get_uniform_location;
    //write_to_apis->set_uniform = gl_set_uniform;
}

#else

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("Current platform not support dx11, try using opengl3 instead.");
}

#endif