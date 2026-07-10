#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include "jeecs_graphic_api_interface.hpp"

#if JE4_CURRENT_PLATFORM != JE4_PLATFORM_ANDROID
#   error EGL interface is only implemented for Android platform.
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/native_window.h>

namespace jeecs::graphic
{
    class egl : public basic_interface
    {
        JECS_DISABLE_MOVE_AND_COPY(egl);

        struct _jegl_window_android_app
        {
            struct android_app* m_android_app;
            ANativeWindow* m_android_window;
        };

    public:
        enum interface_type
        {
            OPENGLES300,
            VULKAN120,
        };
        struct egl_context
        {
            interface_type m_type;
            EGLNativeWindowType m_window;

            struct android_app* m_app;

            // Used for Opengl ES only.
            EGLDisplay m_display;
            EGLSurface m_surface;
            EGLContext m_context;
        };

    private:
        egl_context m_context;

        int32_t _m_recorded_width;
        int32_t _m_recorded_height;

    public:
        egl(interface_type type)
            : _m_recorded_width(0)
            , _m_recorded_height(0)
        {
            m_context.m_type = type;
            m_context.m_window = nullptr;
            m_context.m_app = nullptr;
            m_context.m_display = EGL_NO_DISPLAY;
            m_context.m_surface = EGL_NO_SURFACE;
            m_context.m_context = EGL_NO_CONTEXT;
        }

        virtual void create_interface(
            const jegl_interface_config* config) override
        {
            auto* data = static_cast<_jegl_window_android_app*>(config->m_userdata);
            assert(data != nullptr);

            m_context.m_window = data->m_android_window;
            m_context.m_app = data->m_android_app;

            switch (m_context.m_type)
            {
            case OPENGLES300:
            {
                const EGLint attribs[] = {
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_BLUE_SIZE, 8,
                    EGL_GREEN_SIZE, 8,
                    EGL_RED_SIZE, 8,
                    EGL_DEPTH_SIZE, 24,
                    EGL_NONE
                };

                auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
                if (display == EGL_NO_DISPLAY)
                {
                    jeecs::debug::logfatal("Failed to get EGL display: 0x%x.", eglGetError());
                    abort();
                }

                if (!eglInitialize(display, nullptr, nullptr))
                {
                    jeecs::debug::logfatal("Failed to initialize EGL: 0x%x.", eglGetError());
                    abort();
                }

                if (!eglBindAPI(EGL_OPENGL_ES_API))
                {
                    jeecs::debug::logfatal("Failed to bind OpenGL ES API: 0x%x.", eglGetError());
                    abort();
                }

                // figure out how many configs there are
                EGLint numConfigs;
                if (!eglChooseConfig(display, attribs, nullptr, 0, &numConfigs) || numConfigs <= 0)
                {
                    jeecs::debug::logfatal("No matching EGL configs found: 0x%x.", eglGetError());
                    abort();
                }

                // get the list of configurations
                std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
                if (!eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs) || numConfigs <= 0)
                {
                    jeecs::debug::logfatal("Failed to retrieve EGL configs: 0x%x.", eglGetError());
                    abort();
                }

                // Find a config we like.
                // Could likely just grab the first if we don't care about anything else in the config.
                // Otherwise hook in your own heuristic
                auto configEnd = supportedConfigs.get() + numConfigs;
                auto configIt = std::find_if(
                    supportedConfigs.get(),
                    configEnd,
                    [&display](const EGLConfig& eglconfig)
                    {
                        EGLint red, green, blue, depth;
                        if (eglGetConfigAttrib(display, eglconfig, EGL_RED_SIZE, &red)
                            && eglGetConfigAttrib(display, eglconfig, EGL_GREEN_SIZE, &green)
                            && eglGetConfigAttrib(display, eglconfig, EGL_BLUE_SIZE, &blue)
                            && eglGetConfigAttrib(display, eglconfig, EGL_DEPTH_SIZE, &depth))
                        {
                            return red == 8 && green == 8 && blue == 8 && depth == 24;
                        }
                        return false;
                    });

                EGLConfig egl_config = (configIt != configEnd) ? *configIt : supportedConfigs[0];

                m_context.m_display = display;

                EGLint format;
                eglGetConfigAttrib(display, egl_config, EGL_NATIVE_VISUAL_ID, &format);

                EGLSurface surface = eglCreateWindowSurface(display, egl_config, m_context.m_window, nullptr);
                if (surface == EGL_NO_SURFACE)
                {
                    jeecs::debug::logfatal("Failed to create EGL window surface: 0x%x.", eglGetError());
                    abort();
                }

                // Create a GLES 3 context
                EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
                EGLContext eglcontext = eglCreateContext(display, egl_config, nullptr, contextAttribs);
                if (eglcontext == EGL_NO_CONTEXT)
                {
                    jeecs::debug::logfatal("Failed to create EGL context: 0x%x.", eglGetError());
                    abort();
                }

                // get some window metrics
                if (!eglMakeCurrent(display, surface, surface, eglcontext))
                {
                    jeecs::debug::logfatal("Failed to make EGL context current: 0x%x.", eglGetError());
                    abort();
                }

                // TODO-LIST:
                // * MSAA support
                // * Direction ?
                // * Double buffer
                m_context.m_surface = surface;
                m_context.m_context = eglcontext;

                if (config->m_fps == 0)
                {
                    if (!eglSwapInterval(m_context.m_display, 1))
                        jeecs::debug::logwarn("Failed to set EGL swap interval (vsync): 0x%x.", eglGetError());
                }
                else
                {
                    if (!eglSwapInterval(m_context.m_display, 0))
                        jeecs::debug::logwarn("Failed to set EGL swap interval (no vsync): 0x%x.", eglGetError());
                }
                break;
            }
            case VULKAN120:
                break;
            default:
                // Unknown interface type.
                abort();
            }

        }
        virtual void swap_for_opengl() override
        {
            if (m_context.m_display != EGL_NO_DISPLAY && m_context.m_surface != EGL_NO_SURFACE)
                eglSwapBuffers(m_context.m_display, m_context.m_surface);
        }
        virtual update_result update() override
        {
            // After APP_CMD_TERM_WINDOW, native_app_glue sets app->window to nullptr.
            // The cached m_context.m_window then points to a freed surface.
            // Return PAUSE until a reboot provides a fresh window via APP_CMD_INIT_WINDOW.
            if (m_context.m_app == nullptr || m_context.m_app->window != m_context.m_window)
                return update_result::PAUSE;

            int32_t width = ANativeWindow_getWidth(m_context.m_window);
            int32_t height = ANativeWindow_getHeight(m_context.m_window);

            bool _window_size_resized = false;

            if (_m_recorded_width != width || _m_recorded_height != height)
            {
                _m_recorded_width = width;
                _m_recorded_height = height;

                je_io_update_window_size(static_cast<int>(width), static_cast<int>(height));

                _window_size_resized = true;
            }

            if (_m_recorded_width == 0 || _m_recorded_height == 0)
                return update_result::PAUSE;

            if (_window_size_resized)
                return update_result::RESIZE;

            return update_result::NORMAL;
        }
        virtual void shutdown(bool reboot) override
        {
            switch (m_context.m_type)
            {
            case OPENGLES300:
                if (m_context.m_display != EGL_NO_DISPLAY)
                {
                    eglMakeCurrent(m_context.m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    if (m_context.m_context != EGL_NO_CONTEXT)
                    {
                        eglDestroyContext(m_context.m_display, m_context.m_context);
                        m_context.m_context = EGL_NO_CONTEXT;
                    }
                    if (m_context.m_surface != EGL_NO_SURFACE)
                    {
                        eglDestroySurface(m_context.m_display, m_context.m_surface);
                        m_context.m_surface = EGL_NO_SURFACE;
                    }
                    eglTerminate(m_context.m_display);
                    m_context.m_display = EGL_NO_DISPLAY;
                }
                break;
            case VULKAN120:
                break;
            default:
                // Unknown interface type.
                abort();
            }
        }

        virtual void* interface_handle() const override
        {
            return static_cast<void*>(&m_context);
        }
    };
}
