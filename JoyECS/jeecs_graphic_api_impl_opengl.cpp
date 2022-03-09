#pragma once

#define JE_IMPL
#include "jeecs.hpp"

#define GLEW_STATIC
#include <GL/glew.h>

#ifdef _WIN32
#pragma comment(lib,"OpenGL32.lib")
#endif

// Here is low-level-graphic-api impl.
// OpenGL version.

jegl_graphic_api::custom_interface_info_t gl_startup(jegl_thread*, const jegl_interface_config*)
{
    jeecs::debug::log("Graphic thread start!");

    glewInit();

    return nullptr;
}

void gl_update(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    jeecs::debug::log("Graphic thread update!");
}

void gl_shutdown(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    jeecs::debug::log("Graphic thread shutdown!");

}

JE_API void jegl_using_opengl_apis(jegl_graphic_api* write_to_apis)
{
    write_to_apis->init_interface = gl_startup;
    write_to_apis->update_interface = gl_update;
    write_to_apis->shutdown_interface = gl_shutdown;
}