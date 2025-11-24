#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include "jeecs_graphic_api_interface.hpp"

#include <GLFW/glfw3.h>

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
#   if defined(JE_ENABLE_DX11_GAPI)
#       define GLFW_EXPOSE_NATIVE_WIN32 1
#   endif
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_MACOS
#   if defined(JE_ENABLE_METAL_GAPI)
#       define GLFW_EXPOSE_NATIVE_COCOA 1
#   endif
#endif
#include <GLFW/glfw3native.h>

namespace jeecs::graphic
{
    class glfw : public basic_interface
    {
        JECS_DISABLE_MOVE_AND_COPY(glfw);

        enum class mouse_lock_state_t
        {
            NO_SPECIFY,
            LOCKED,
            UNLOCKED,
        };

        GLFWwindow* _m_windows;
        bool _m_window_resized;
        bool _m_window_state_paused;
        mouse_lock_state_t _m_mouse_locked;

        inline static std::mutex _m_glfw_instance_mx;
        inline static size_t _m_glfw_taking_count = 0;

    public:
        enum interface_type
        {
            HOLD,
            OPENGL330,
            OPENGLES300,
            VULKAN120,
            DIRECTX11,
            METAL,
        };

    public:
        static void glfw_callback_windows_size_changed(GLFWwindow* fw, int x, int y)
        {
            glfw* context = reinterpret_cast<glfw*>(glfwGetWindowUserPointer(fw));

            je_io_update_window_size(x, y);

            if (x == 0 || y == 0)
                context->_m_window_state_paused = true;
            else
                context->_m_window_state_paused = false;
            context->_m_window_resized = true;
        }
        static void glfw_callback_windows_pos_changed(GLFWwindow* fw, int x, int y)
        {
            // glfw* context = reinterpret_cast<glfw*>(glfwGetWindowUserPointer(fw));

            je_io_update_window_pos(x, y);
        }
        static void glfw_callback_mouse_pos_changed(GLFWwindow* fw, double x, double y)
        {
            je_io_update_mouse_pos(0, (int)x, (int)y);
        }
        static void glfw_callback_mouse_key_clicked(GLFWwindow* fw, int key, int state, int mod)
        {
            jeecs::input::mousecode keycode;
            switch (key)
            {
            case GLFW_MOUSE_BUTTON_LEFT:
                keycode = jeecs::input::mousecode::LEFT;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                keycode = jeecs::input::mousecode::MID;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                keycode = jeecs::input::mousecode::RIGHT;
                break;
            default:
                // do nothing.
                return;
            }
            je_io_update_mouse_state(0, keycode, state != 0);
        }
        static void glfw_callback_mouse_scroll_changed(GLFWwindow* fw, double xoffset, double yoffset)
        {
            float ox, oy;
            je_io_get_wheel(0, &ox, &oy);
            je_io_update_wheel(0, ox + (float)xoffset, oy + (float)yoffset);
        }
        static void glfw_callback_keyboard_stage_changed(GLFWwindow* fw, int key, int w, int stage, int v)
        {
            static_assert(GLFW_KEY_A == 'A');
            static_assert(GLFW_KEY_0 == '0');

            jeecs::input::keycode keycode;

            switch (key)
            {
            case GLFW_KEY_LEFT_SHIFT:
                keycode = jeecs::input::keycode::L_SHIFT;
                break;
            case GLFW_KEY_RIGHT_SHIFT:
                keycode = jeecs::input::keycode::R_SHIFT;
                break;
            case GLFW_KEY_LEFT_ALT:
                keycode = jeecs::input::keycode::L_ALT;
                break;
            case GLFW_KEY_RIGHT_ALT:
                keycode = jeecs::input::keycode::R_ALT;
                break;
            case GLFW_KEY_LEFT_CONTROL:
                keycode = jeecs::input::keycode::L_CTRL;
                break;
            case GLFW_KEY_RIGHT_CONTROL:
                keycode = jeecs::input::keycode::R_CTRL;
                break;
            case GLFW_KEY_TAB:
                keycode = jeecs::input::keycode::TAB;
                break;
            case GLFW_KEY_ENTER:
                keycode = jeecs::input::keycode::ENTER;
                break;
            case GLFW_KEY_ESCAPE:
                keycode = jeecs::input::keycode::ESC;
                break;
            case GLFW_KEY_BACKSPACE:
                keycode = jeecs::input::keycode::BACKSPACE;
                break;
            case GLFW_KEY_KP_0:
            case GLFW_KEY_KP_1:
            case GLFW_KEY_KP_2:
            case GLFW_KEY_KP_3:
            case GLFW_KEY_KP_4:
            case GLFW_KEY_KP_5:
            case GLFW_KEY_KP_6:
            case GLFW_KEY_KP_7:
            case GLFW_KEY_KP_8:
            case GLFW_KEY_KP_9:
                keycode = (jeecs::input::keycode)(
                    (uint16_t)jeecs::input::keycode::NP_0 + (key - GLFW_KEY_KP_0));
                break;
            case GLFW_KEY_KP_ADD:
                keycode = jeecs::input::keycode::NP_ADD;
                break;
            case GLFW_KEY_KP_SUBTRACT:
                keycode = jeecs::input::keycode::NP_SUBTRACT;
                break;
            case GLFW_KEY_KP_MULTIPLY:
                keycode = jeecs::input::keycode::NP_MULTIPLY;
                break;
            case GLFW_KEY_KP_DIVIDE:
                keycode = jeecs::input::keycode::NP_DIVIDE;
                break;
            case GLFW_KEY_KP_DECIMAL:
                keycode = jeecs::input::keycode::NP_DECIMAL;
                break;
            case GLFW_KEY_KP_ENTER:
                keycode = jeecs::input::keycode::NP_ENTER;
                break;
            case GLFW_KEY_UP:
                keycode = jeecs::input::keycode::UP;
                break;
            case GLFW_KEY_DOWN:
                keycode = jeecs::input::keycode::DOWN;
                break;
            case GLFW_KEY_LEFT:
                keycode = jeecs::input::keycode::LEFT;
                break;
            case GLFW_KEY_RIGHT:
                keycode = jeecs::input::keycode::RIGHT;
                break;
            case GLFW_KEY_F1:
            case GLFW_KEY_F2:
            case GLFW_KEY_F3:
            case GLFW_KEY_F4:
            case GLFW_KEY_F5:
            case GLFW_KEY_F6:
            case GLFW_KEY_F7:
            case GLFW_KEY_F8:
            case GLFW_KEY_F9:
            case GLFW_KEY_F10:
            case GLFW_KEY_F11:
            case GLFW_KEY_F12:
            case GLFW_KEY_F13:
            case GLFW_KEY_F14:
            case GLFW_KEY_F15:
            case GLFW_KEY_F16:
                keycode = (jeecs::input::keycode)(
                    (uint16_t)jeecs::input::keycode::F1 + (key - GLFW_KEY_F1));
                break;
            default:
                if (key >= 0 && key <= 127)
                    keycode = (jeecs::input::keycode)key;
                else
                    return;
                break;
            }

            je_io_update_key_state(keycode, stage != 0);
        }
    public:
        glfw(interface_type type)
            : _m_windows(nullptr)
            , _m_window_resized(true)
            , _m_window_state_paused(false)
            , _m_mouse_locked(mouse_lock_state_t::NO_SPECIFY)
        {
            if (type == interface_type::HOLD)
                return;

            do
            {
                std::lock_guard g1(_m_glfw_instance_mx);

                if (0 == _m_glfw_taking_count++)
                {
                    if (!glfwInit())
                        jeecs::debug::logfatal("Failed to init glfw.");

#ifndef NDEBUG
                    glfwSetErrorCallback([](int ecode, const char* desc) {
                        jeecs::debug::logwarn("Glfw error %d: %s", ecode, desc);
                        });
#endif
                }

            } while (0);

            switch (type)
            {
            case interface_type::OPENGL330:
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_MACOS
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
                break;
            case interface_type::OPENGLES300:
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
                // ATTENTION: Windows等平台上，运行opengles的时候，环境要求EGL
                //          但是opengles的支持仅限于移动端设备和Windows借助ARMEMU
                //          所以这里只针对Windows做了处理
                glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#else
                glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
#endif
                break;
            case interface_type::VULKAN120:
            case interface_type::DIRECTX11:
            case interface_type::METAL:
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

        virtual void create_interface(const jegl_interface_config* config) override
        {
            _m_mouse_locked = mouse_lock_state_t::NO_SPECIFY;

            auto display_mode = config->m_display_mode;
            auto* primary_monitor = glfwGetPrimaryMonitor();

            int interface_width, interface_height;

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WEBGL
            interface_width = config->m_width;
            interface_height = config->m_height;

            display_mode = jegl_interface_config::display_mode::WINDOWED;

            assert(config->m_width != 0 && config->m_height != 0);
#else
            auto* primary_monitor_video_mode = glfwGetVideoMode(primary_monitor);

            interface_width = config->m_width == 0
                ? primary_monitor_video_mode->width
                : config->m_width;

            interface_height = config->m_height == 0
                ? primary_monitor_video_mode->height
                : config->m_height;

            glfwWindowHint(GLFW_REFRESH_RATE,
                config->m_fps == 0
                ? primary_monitor_video_mode->refreshRate
                : (int)config->m_fps);
#endif
            glfwWindowHint(GLFW_RESIZABLE, config->m_enable_resize ? GLFW_TRUE : GLFW_FALSE);
            glfwWindowHint(GLFW_SAMPLES, (int)config->m_msaa);

            switch (display_mode)
            {
            case jegl_interface_config::display_mode::BOARDLESS:
                glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

                /*fallthrough*/
                [[fallthrough]];
            case jegl_interface_config::display_mode::WINDOWED:
                _m_windows = glfwCreateWindow(
                    (int)interface_width,
                    (int)interface_height,
                    config->m_title,
                    NULL, NULL);
                break;
            case jegl_interface_config::display_mode::FULLSCREEN:
                _m_windows = glfwCreateWindow(
                    (int)interface_width,
                    (int)interface_height,
                    config->m_title,
                    primary_monitor, NULL);
                break;
            default:
                jeecs::debug::logfatal("Unknown display mode to start up graphic thread.");
                je_clock_sleep_for(1.);
                abort();
                break;
            }

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WEBGL
            // Do nothing.
#else
            const char* reason;
            auto err_code = glfwGetError(&reason);
            if (err_code != GLFW_NO_ERROR)
            {
                jeecs::debug::logfatal("Glfw reports an error(%d): %s.",
                    err_code, reason);
                je_clock_sleep_for(1.);
                abort();
            }
#endif

            // Try load icon from @/icon.png or !/builtin/icon/icon.png.
            // Do nothing if both not exist.
            auto icon = jeecs::graphic::texture::load(nullptr, "@/icon.png");

            if (!icon.has_value())
                icon = jeecs::graphic::texture::load(nullptr, "!/builtin/icon/icon.png");

            if (icon.has_value())
            {
                auto& icon_texture = icon.value();

                GLFWimage icon_data;
                icon_data.width = (int)icon_texture->width();
                icon_data.height = (int)icon_texture->height();
                // Here need a y-direct flip.
                auto* image_pixels = icon_texture->resource()->m_raw_texture_data->m_pixels;
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

            if (glfwGetWindowAttrib(_m_windows, GLFW_CLIENT_API) != GLFW_NO_API)
            {
                glfwMakeContextCurrent(_m_windows);
                // Donot sync for webgl, and donot process mouse event.
#if JE4_CURRENT_PLATFORM != JE4_PLATFORM_WEBGL
                if (config->m_fps == 0)
                    glfwSwapInterval(1);
                else
#endif
                    glfwSwapInterval(0);
            }

            glfwSetWindowSizeCallback(_m_windows, glfw_callback_windows_size_changed);
            glfwSetWindowPosCallback(_m_windows, glfw_callback_windows_pos_changed);
            glfwSetKeyCallback(_m_windows, glfw_callback_keyboard_stage_changed);
            glfwSetWindowUserPointer(_m_windows, this);

            int window_x, window_y;
            glfwGetWindowPos(_m_windows, &window_x, &window_y);
            glfw_callback_windows_pos_changed(_m_windows, window_x, window_y);

            glfw_callback_windows_size_changed(
                _m_windows,
                (int)interface_width,
                (int)interface_height);

            // We will process mouse event in javascript(WebGL).
#if JE4_CURRENT_PLATFORM != JE4_PLATFORM_WEBGL
            glfwSetCursorPosCallback(_m_windows, glfw_callback_mouse_pos_changed);
            glfwSetMouseButtonCallback(_m_windows, glfw_callback_mouse_key_clicked);
            glfwSetScrollCallback(_m_windows, glfw_callback_mouse_scroll_changed);
#endif
        }
        virtual void swap_for_opengl() override
        {
            glfwSwapBuffers(_m_windows);
        }
        virtual update_result update() override
        {
            if (je_io_get_lock_mouse())
            {
                if (_m_mouse_locked != mouse_lock_state_t::LOCKED)
                {
                    glfwSetInputMode(_m_windows, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    _m_mouse_locked = mouse_lock_state_t::LOCKED;
                }
            }
            else
            {
                if (_m_mouse_locked != mouse_lock_state_t::UNLOCKED)
                {
                    glfwSetInputMode(_m_windows, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    _m_mouse_locked = mouse_lock_state_t::UNLOCKED;
                }
            }

            int window_width, window_height;
            if (je_io_fetch_update_window_size(&window_width, &window_height))
                glfwSetWindowSize(_m_windows, window_width, window_height);

            const char* title;
            if (je_io_fetch_update_window_title(&title))
                glfwSetWindowTitle(_m_windows, title);

            glfwPollEvents();
            if (glfwWindowShouldClose(_m_windows) == GLFW_TRUE)
            {
                glfwSetWindowShouldClose(_m_windows, GLFW_FALSE);
                return update_result::CLOSE;
            }

            if (_m_window_state_paused)
                return update_result::PAUSE;

            if (_m_window_resized)
            {
                _m_window_resized = false;
                return update_result::RESIZE;
            }

            return update_result::NORMAL;
        }
        virtual void shutdown(bool reboot) override
        {
            glfwDestroyWindow(_m_windows);
            if (!reboot)
            {
                do
                {
                    std::lock_guard g1(_m_glfw_instance_mx);

                    if (0 == --_m_glfw_taking_count)
                        glfwTerminate();

                } while (0);
            }
        }
        virtual void* interface_handle() const override
        {
            return _m_windows;
        }
    };
}
