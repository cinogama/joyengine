#include "jeecs_graphic_api_interface.hpp"

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#   include <EGL/egl.h>
#   include <EGL/eglext.h>

namespace jeecs::graphic
{
    class egl : public basic_interface
    {
        JECS_DISABLE_MOVE_AND_COPY(egl);

        struct egl_context
        {
            EGLDisplay m_display;
            EGLSurface m_surface;
            EGLContext m_context;
            EGLNativeWindowType m_window;
        };

#   ifdef JE_OS_ANDROID
        struct _jegl_window_android_app
        {
            void* m_android_app;
            void* m_android_window;
        };
        struct android_app* m_app;
#   else
#       error Unknown platform.
#   endif

        egl_context m_context;

    public:
        egl()
        {
        }

        virtual void create_interface(jegl_context* thread, const jegl_interface_config* config) override
        {
            constexpr EGLint attribs[] = {
              EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
              EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
              EGL_BLUE_SIZE, 8,
              EGL_GREEN_SIZE, 8,
              EGL_RED_SIZE, 8,
              EGL_DEPTH_SIZE, 24,
              EGL_NONE
            };

            auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            eglInitialize(display, nullptr, nullptr);

            // figure out how many configs there are
            EGLint numConfigs;
            eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

            // get the list of configurations
            std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
            eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

            // Find a config we like.
            // Could likely just grab the first if we don't care about anything else in the config.
            // Otherwise hook in your own heuristic
            auto egl_config = *std::find_if(
                supportedConfigs.get(),
                supportedConfigs.get() + numConfigs,
                [&display](const EGLConfig& eglconfig) {
                    EGLint red, green, blue, depth;
                    if (eglGetConfigAttrib(display, eglconfig, EGL_RED_SIZE, &red)
                        && eglGetConfigAttrib(display, eglconfig, EGL_GREEN_SIZE, &green)
                        && eglGetConfigAttrib(display, eglconfig, EGL_BLUE_SIZE, &blue)
                        && eglGetConfigAttrib(display, eglconfig, EGL_DEPTH_SIZE, &depth)) {

                        return red == 8 && green == 8 && blue == 8 && depth == 24;
                    }
                    return false;
                });

            assert(thread->_m_sync_callback_arg != nullptr);

#   ifdef JE_OS_ANDROID
            auto* data = (_jegl_window_android_app*)thread->_m_sync_callback_arg;
            m_context.m_window = (EGLNativeWindowType)data->m_android_window;
            m_app = (struct android_app*)data->m_android_app;
#   else
#       error Unknown platform.
#   endif

            EGLint format;
            eglGetConfigAttrib(display, egl_config, EGL_NATIVE_VISUAL_ID, &format);
            EGLSurface surface = eglCreateWindowSurface(display, egl_config, m_context.m_window, nullptr);

            // Create a GLES 3 context
            EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
            EGLContext eglcontext = eglCreateContext(display, egl_config, nullptr, contextAttribs);

            // get some window metrics
            auto madeCurrent = eglMakeCurrent(display, surface, surface, eglcontext);
            assert(madeCurrent);

            // TODO-LIST: 
            // * MSAA support
            // * Direction ?
            // * Double buffer
            m_context.m_display = display;
            m_context.m_surface = surface;
            m_context.m_context = eglcontext;

            if (config->m_fps == 0)
                eglSwapInterval(m_context.m_display, 1);
            else
                eglSwapInterval(m_context.m_display, 0);
        }
        virtual void swap_for_opengl() override
        {
            eglSwapBuffers(m_context.m_display, m_context.m_surface);
        }
        virtual update_result update() override
        {
            EGLint width;
            eglQuerySurface(m_context.m_display, m_context.m_surface, EGL_WIDTH, &width);

            EGLint height;
            eglQuerySurface(m_context.m_display, m_context.m_surface, EGL_HEIGHT, &height);

            bool _window_size_resized = false;

            if (m_interface_width != (size_t)width || m_interface_height != (size_t)height)
            {
                m_interface_width = (size_t)width;
                m_interface_height = (size_t)height;

                je_io_update_windowsize((int)width, (int)height);

                _window_size_resized = true;
            }

            if (m_interface_width == 0 || m_interface_height == 0)
                return update_result::PAUSE;

            if (_window_size_resized)
                return update_result::RESIZE;

            return update_result::NORMAL ;
        }
        virtual void shutdown(bool reboot) override
        {
            eglMakeCurrent(m_context.m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(m_context.m_display, m_context.m_context);
            eglDestroySurface(m_context.m_display, m_context.m_surface);
            eglTerminate(m_context.m_display);
        }

        virtual void* interface_handle() const override
        {
#ifdef JE_OS_ANDROID
            return m_app;
#else
            return nullptr;
#endif
        }
    };
}