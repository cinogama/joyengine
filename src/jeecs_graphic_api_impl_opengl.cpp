#define JE_IMPL
#include "jeecs.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Here is low-level-graphic-api impl.
// OpenGL version.

constexpr size_t MAX_MOUSE_BUTTON_COUNT = 16;
constexpr size_t MAX_KEYBOARD_BUTTON_COUNT = 65535;

thread_local size_t WINDOWS_SIZE_WIDTH = 0;
thread_local size_t WINDOWS_SIZE_HEIGHT = 0;
thread_local const char* WINDOWS_TITLE = nullptr;
thread_local GLFWwindow* WINDOWS_HANDLE = nullptr;
thread_local std::thread::id GRAPHIC_THREAD_ID;

thread_local float MOUSE_POS_X = 0;
thread_local float MOUSE_POS_Y = 0;
thread_local int MOUSE_KEY_STATE[MAX_MOUSE_BUTTON_COUNT];
thread_local float MOUSE_SCROLL_X_COUNT = 0.f;
thread_local float MOUSE_SCROLL_Y_COUNT = 0.f;
thread_local int KEYBOARD_STATE[MAX_KEYBOARD_BUTTON_COUNT];

void glfw_callback_windows_size_changed(GLFWwindow* fw, int x, int y)
{
    WINDOWS_SIZE_WIDTH = (size_t)x;
    WINDOWS_SIZE_HEIGHT = (size_t)y;
}

void glfw_callback_mouse_pos_changed(GLFWwindow* fw, double x, double y)
{
    MOUSE_POS_X = (float)x / WINDOWS_SIZE_WIDTH * 2.0f - 1.0f;
    MOUSE_POS_Y = -((float)y / WINDOWS_SIZE_HEIGHT * 2.0f - 1.0f);
}

void glfw_callback_mouse_key_clicked(GLFWwindow* fw, int key, int state, int mod)
{
    assert(key < MAX_MOUSE_BUTTON_COUNT);
    MOUSE_KEY_STATE[key] = state;
}

void glfw_callback_mouse_scroll_changed(GLFWwindow* fw, double xoffset, double yoffset)
{
    MOUSE_SCROLL_X_COUNT += (float)yoffset;
    MOUSE_SCROLL_Y_COUNT += (float)yoffset;
}

void glfw_callback_keyboard_stage_changed(GLFWwindow* fw, int key, int w, int stage, int v)
{
    assert(key < MAX_KEYBOARD_BUTTON_COUNT);
    KEYBOARD_STATE[key] = stage;
}

jegl_graphic_api::custom_interface_info_t gl_startup(jegl_thread*, const jegl_interface_config* config)
{
    jeecs::debug::log("Graphic thread start!");

    GRAPHIC_THREAD_ID = std::this_thread::get_id();

    glfwInit();
    glewInit();

    WINDOWS_SIZE_WIDTH = config->m_windows_width ? config->m_windows_width : config->m_resolution_x;
    WINDOWS_SIZE_HEIGHT = config->m_windows_height ? config->m_windows_height : config->m_resolution_y;
    WINDOWS_TITLE = config->m_title ? config->m_title : WINDOWS_TITLE;

    WINDOWS_HANDLE = glfwCreateWindow((int)WINDOWS_SIZE_WIDTH, (int)WINDOWS_SIZE_HEIGHT, WINDOWS_TITLE,
        config->m_fullscreen ? glfwGetPrimaryMonitor() : NULL, NULL);

    glfwMakeContextCurrent(WINDOWS_HANDLE);
    glfwSetWindowSizeCallback(WINDOWS_HANDLE, glfw_callback_windows_size_changed);
    glfwSetCursorPosCallback(WINDOWS_HANDLE, glfw_callback_mouse_pos_changed);
    glfwSetMouseButtonCallback(WINDOWS_HANDLE, glfw_callback_mouse_key_clicked);
    glfwSetScrollCallback(WINDOWS_HANDLE, glfw_callback_mouse_scroll_changed);
    glfwSetKeyCallback(WINDOWS_HANDLE, glfw_callback_keyboard_stage_changed);
    glfwSwapInterval(0);

    glEnable(GL_MULTISAMPLE);

    return nullptr;
}

bool gl_update(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    glfwSwapBuffers(WINDOWS_HANDLE);
    glfwPollEvents();

    if (glfwWindowShouldClose(WINDOWS_HANDLE))
    {
        jeecs::debug::log_warn("Graphic interface has been closed, graphic thread will exit soon.");
        return false;
    }
    return true;
}

void gl_shutdown(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    jeecs::debug::log("Graphic thread shutdown!");

    glfwDestroyWindow(WINDOWS_HANDLE);

}

JE_API void jegl_using_opengl_apis(jegl_graphic_api* write_to_apis)
{
    write_to_apis->init_interface = gl_startup;
    write_to_apis->update_interface = gl_update;
    write_to_apis->shutdown_interface = gl_shutdown;
}