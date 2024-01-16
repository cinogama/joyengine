#include "jeecs_graphic_api_interface.hpp"

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace jeecs::graphic
{
    class glfw : public basic_interface
    {
        GLFWwindow* _m_windows;
        JECS_DISABLE_MOVE_AND_COPY(glfw);

    public:
        enum interface_type
        {
            HOLD,
            OPENGL330,
            OPENGLES300,
            VULKAN130,
        };
    public:
        static void glfw_callback_windows_size_changed(GLFWwindow* fw, int x, int y)
        {
            glfw* context = std::launder(reinterpret_cast<glfw*>(glfwGetWindowUserPointer(fw)));

            context->m_interface_width = (size_t)x;
            context->m_interface_height = (size_t)y;

            je_io_update_windowsize(x, y);
        }
        static void glfw_callback_mouse_pos_changed(GLFWwindow* fw, double x, double y)
        {
            je_io_update_mousepos(0, (int)x, (int)y);
        }
        static void glfw_callback_mouse_key_clicked(GLFWwindow* fw, int key, int state, int mod)
        {
            switch (key)
            {
            case GLFW_MOUSE_BUTTON_LEFT:
                je_io_update_mouse_state(0, jeecs::input::mousecode::LEFT, state); break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                je_io_update_mouse_state(0, jeecs::input::mousecode::MID, state); break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                je_io_update_mouse_state(0, jeecs::input::mousecode::RIGHT, state); break;
            default:
                // do nothing.
                break;
            }
        }
        static void glfw_callback_mouse_scroll_changed(GLFWwindow* fw, double xoffset, double yoffset)
        {
            float ox, oy;
            je_io_get_wheel(0, &ox, &oy);
            je_io_update_wheel(0, ox + (float)xoffset, oy + (float)yoffset);
        }
        static void glfw_callback_keyboard_stage_changed(GLFWwindow* fw, int key, int w, int stage, int v)
        {
            assert(GLFW_KEY_A == 'A');
            assert(GLFW_KEY_0 == '0');
            switch (key)
            {
            case GLFW_KEY_LEFT_SHIFT:
                je_io_update_keystate(jeecs::input::keycode::L_SHIFT, stage); break;
            case GLFW_KEY_LEFT_ALT:
                je_io_update_keystate(jeecs::input::keycode::L_ALT, stage); break;
            case GLFW_KEY_LEFT_CONTROL:
                je_io_update_keystate(jeecs::input::keycode::L_CTRL, stage); break;
            case GLFW_KEY_TAB:
                je_io_update_keystate(jeecs::input::keycode::TAB, stage); break;
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER:
                je_io_update_keystate(jeecs::input::keycode::ENTER, stage); break;
            case GLFW_KEY_ESCAPE:
                je_io_update_keystate(jeecs::input::keycode::ESC, stage); break;
            case GLFW_KEY_BACKSPACE:
                je_io_update_keystate(jeecs::input::keycode::BACKSPACE, stage); break;
            default:
                je_io_update_keystate((jeecs::input::keycode)key, stage); break;
            }

        }

    public:
        glfw(interface_type type)
        {
            if (type == interface_type::HOLD)
                return;

            if (!glfwInit())
                jeecs::debug::logfatal("Failed to init glfw.");

            switch (type)
            {
            case interface_type::OPENGL330:
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
                break;
            case interface_type::OPENGLES300:
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef JE_OS_WINDOWS
                // ATTENTION: Windows等平台上，运行opengles的时候，环境要求EGL
                //          但是opengles的支持仅限于移动端设备和Windows借助ARMEMU
                //          所以这里只针对Windows做了处理
                glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#else
                glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
#endif
                break;
            case interface_type::VULKAN130:
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                break;
            default:
                jeecs::debug::logfatal("Unknown glfw interface type.");
                break;
            }
#ifndef NDEBUG
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
        }

        virtual void create_interface(jegl_context* thread, const jegl_interface_config* config) override
        {
            auto* primary_monitor = glfwGetPrimaryMonitor();
            auto* primary_monitor_video_mode = glfwGetVideoMode(primary_monitor);

            m_interface_width = config->m_width == 0
                ? primary_monitor_video_mode->width
                : config->m_width;

            m_interface_height = config->m_height == 0
                ? primary_monitor_video_mode->height
                : config->m_height;

            glfwWindowHint(GLFW_REFRESH_RATE,
                config->m_fps == 0
                ? primary_monitor_video_mode->refreshRate
                : (int)config->m_fps);
            glfwWindowHint(GLFW_RESIZABLE, config->m_enable_resize ? GLFW_TRUE : GLFW_FALSE);
            glfwWindowHint(GLFW_SAMPLES, (int)config->m_msaa);

            je_io_update_windowsize(
                (int)m_interface_width, (int)m_interface_height);

            switch (config->m_display_mode)
            {
            case jegl_interface_config::display_mode::BOARDLESS:
                glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
            case jegl_interface_config::display_mode::WINDOWED:
                _m_windows = glfwCreateWindow(
                    (int)m_interface_width,
                    (int)m_interface_height,
                    config->m_title,
                    NULL, NULL);
                break;
            case jegl_interface_config::display_mode::FULLSCREEN:
                _m_windows = glfwCreateWindow(
                    (int)m_interface_width,
                    (int)m_interface_height,
                    config->m_title,
                    primary_monitor, NULL);
                break;
            default:
                jeecs::debug::logfatal("Unknown display mode to start up graphic thread.");
                je_clock_sleep_for(1.);
                abort();
                break;
            }

            const char* reason;
            auto err_code = glfwGetError(&reason);
            if (err_code != GLFW_NO_ERROR)
            {
                auto x = (const char*)glGetString(GL_VERSION);
                jeecs::debug::logfatal("Opengl3 glfw reports an error(%d): %s.",
                    err_code, reason);
                je_clock_sleep_for(1.);
                abort();
            }

            // Try load icon from @/icon.png or !/builtin/icon/icon.png.
            // Do nothing if both not exist.
            jeecs::basic::resource<jeecs::graphic::texture> icon = jeecs::graphic::texture::load("@/icon.png");
            if (icon == nullptr)
                icon = jeecs::graphic::texture::load("!/builtin/icon/icon.png");

            if (icon != nullptr)
            {
                GLFWimage icon_data;
                icon_data.width = (int)icon->width();
                icon_data.height = (int)icon->height();
                // Here need a y-direct flip.
                auto* image_pixels = icon->resouce()->m_raw_texture_data->m_pixels;
                icon_data.pixels = (unsigned char*)je_mem_alloc((size_t)icon_data.width * (size_t)icon_data.height * 4);
                assert(icon_data.pixels != nullptr);

                for (size_t iy = 0; iy < (size_t)icon_data.height; ++iy)
                {
                    size_t target_place_offset = iy * (size_t)icon_data.width * 4;
                    size_t origin_place_offset = ((size_t)icon_data.height - iy - 1) * (size_t)icon_data.width * 4;
                    memcpy(icon_data.pixels + target_place_offset, image_pixels + origin_place_offset, (size_t)icon_data.width * 4);
                }

                glfwSetWindowIcon(_m_windows, 1, &icon_data);

                je_mem_free(icon_data.pixels);
            }

            glfwMakeContextCurrent(_m_windows);
            glfwSetWindowSizeCallback(_m_windows, glfw_callback_windows_size_changed);
            glfwSetCursorPosCallback(_m_windows, glfw_callback_mouse_pos_changed);
            glfwSetMouseButtonCallback(_m_windows, glfw_callback_mouse_key_clicked);
            glfwSetScrollCallback(_m_windows, glfw_callback_mouse_scroll_changed);
            glfwSetKeyCallback(_m_windows, glfw_callback_keyboard_stage_changed);
            glfwSetWindowUserPointer(_m_windows, this);

            if (config->m_fps == 0)
                glfwSwapInterval(1);
            else
                glfwSwapInterval(0);
        }
        virtual void swap() override
        {
            glfwSwapBuffers(_m_windows);
        }
        virtual bool update() override
        {
            glfwPollEvents();
            int mouse_lock_x, mouse_lock_y;
            if (je_io_get_lock_mouse(&mouse_lock_x, &mouse_lock_y))
                glfwSetCursorPos(_m_windows, mouse_lock_x, mouse_lock_y);

            int window_width, window_height;
            if (je_io_fetch_update_windowsize(&window_width, &window_height))
                glfwSetWindowSize(_m_windows, window_width, window_height);

            const char* title;
            if (je_io_fetch_update_windowtitle(&title))
                glfwSetWindowTitle(_m_windows, title);

            if (glfwWindowShouldClose(_m_windows) == GLFW_TRUE)
            {
                glfwSetWindowShouldClose(_m_windows, GLFW_FALSE);
                return false;
            }
            return true;
        }
        virtual void shutdown(bool reboot) override
        {
            glfwDestroyWindow(_m_windows);
            if (!reboot)
                glfwTerminate();
        }

        virtual void* native_handle() override
        {
            return _m_windows;
        }
    };
}