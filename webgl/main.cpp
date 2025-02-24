#include <emscripten/html5.h>

/*
 * JoyECS
 * -----------------------------------
 * JoyECS is a interesting ecs-impl.
 *
 */
#include "jeecs.hpp"

#include <cmath>
#include <optional>
#include <cassert>

using namespace jeecs;

class je_webgl_surface_context
{
    int canvas_width = 0;
    int canvas_height = 0;

    typing::type_unregister_guard *_je_type_guard = nullptr;
    jegl_context *_jegl_graphic_thread = nullptr;
    bool _jegl_inited = false;

    static void _jegl_webgl_sync_thread_created(
        jegl_context *gl_context, void *context)
    {
        je_webgl_surface_context *surface_context =
            std::launder(reinterpret_cast<je_webgl_surface_context *>(context));

        surface_context->_jegl_graphic_thread = gl_context;
    }

public:
    je_webgl_surface_context()
    {
        je_init(0, nullptr);
        jeecs::debug::loginfo("WebGL application started!");

        // Update engine paths settings.
        jeecs_file_set_host_path("/.je4");
        jeecs_file_set_runtime_path("/.je4");
        jeecs_file_update_default_fimg("");

        _je_type_guard = new typing::type_unregister_guard();
        entry::module_entry(_je_type_guard);

        jegl_register_sync_thread_callback(
            _jegl_webgl_sync_thread_created, this);

        // Execute script entry in another thread.
        std::thread(je_main_script_entry).detach();
    }

    ~je_webgl_surface_context()
    {
        jegl_sync_shutdown(_jegl_graphic_thread, false);

        entry::module_leave(_je_type_guard);

        jeecs::debug::loginfo("WebGL application shutdown!");
        je_finish();
    }

    bool update_frame()
    {
        if (!_jegl_inited)
        {
            if (_jegl_graphic_thread != nullptr)
            {
                _jegl_inited = true;
                jegl_sync_init(_jegl_graphic_thread, false);
            }
        }
        else
        {
            if (jegl_sync_state::JEGL_SYNC_COMPLETE != jegl_sync_update(_jegl_graphic_thread))
                return false;

            double width, height;
            if (EMSCRIPTEN_RESULT_SUCCESS == emscripten_get_element_css_size("#canvas", &width, &height))
            {
                int rwidth = static_cast<int>(std::round(width));
                int rheight = static_cast<int>(std::round(height));

                if (canvas_width != rwidth || canvas_height != rheight)
                {
                    canvas_width = rwidth;
                    canvas_height = rheight;

                    if (width > 0 && height > 0)
                    {
                        // debug::loginfo("Canvas size changed: %d, %d", rwidth, rheight);
                        je_io_set_windowsize(rwidth, rheight);
                    }
                }
            }
        }

        return true;
    }
};

je_webgl_surface_context *g_surface_context = nullptr;

void webgl_rend_job_callback()
{
    if (g_surface_context != nullptr)
    {
        if (!g_surface_context->update_frame())
            // Surface has been requested to close, stop the main loop.
            emscripten_cancel_main_loop();
    }
}

extern "C"
{
    void EMSCRIPTEN_KEEPALIVE _je4_js_callback_prepare_surface_context()
    {
        assert(g_surface_context == nullptr);
        g_surface_context = new je_webgl_surface_context();
    }
}

int main(int argc, char **argv)
{
    // Mount idbfs to /builtin
    EM_ASM(
        if (!FS.analyzePath('/.je4').exists) {
            FS.mkdir('/.je4');
        } FS.mount(IDBFS,
                   {
                       autoPersist : true
                   },
                   '/.je4');

        FS.syncfs(true, function(err) {
            if (err != null) {
                console.error("Failed to sync filesystem: " + err);
                alert("Failed to sync IDBFS.");
            }
            else
            {
                // Invoke _je4_js_callback_prepare_surface_context
                const start_context = 
                    Module.cwrap(
                        "_je4_js_callback_prepare_surface_context",
                        null, 
                        []);

                // Start the context.
                start_context();
                
            } }););

    emscripten_set_main_loop(webgl_rend_job_callback, 0, 1);
    if (g_surface_context != nullptr)
        delete g_surface_context;

    return 0;
}
