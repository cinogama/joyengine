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
                        je_io_set_window_size(rwidth, rheight);
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
    void EMSCRIPTEN_KEEPALIVE 
        _je4_js_callback_prepare_surface_context()
    {
        assert(g_surface_context == nullptr);
        g_surface_context = new je_webgl_surface_context();
    }
    void EMSCRIPTEN_KEEPALIVE 
        _je4_je_io_update_mousepos(int group, int x, int y)
    {
        je_io_update_mouse_pos((size_t)group, x, y);
    }
    void EMSCRIPTEN_KEEPALIVE 
        _je4_je_io_update_mouse_state(
            int group, int /*jeecs::input::mousecode*/ key, bool keydown)
    {
        je_io_update_mouse_state(
            (size_t)group, (jeecs::input::mousecode)key, keydown);
    }
}

int main(int argc, char **argv)
{
    // Mount idbfs to /builtin
    EM_ASM(
        const MOUSE_LEFT = 0;
        const MOUSE_MIDDLE = 1;
        const MOUSE_RIGHT = 2;

        // void _je4_js_callback_prepare_surface_context()
        const _je4_js_callback_prepare_surface_context = 
            Module.cwrap("_je4_js_callback_prepare_surface_context", null, []);

        // void _je4_je_io_update_mousepos(size_t group, int x, int y)
        // void _je4_je_io_update_mouse_state(size_t group, int /*jeecs::input::mousecode*/ key, bool keydown)
        const _je4_je_io_update_mousepos =
            Module.cwrap('_je4_je_io_update_mousepos', null, ['number', 'number', 'number']);
        const _je4_je_io_update_mouse_state =
            Module.cwrap('_je4_je_io_update_mouse_state', null, ['number', 'number', 'number']);

        if (!FS.analyzePath('/.je4').exists) {
            FS.mkdir('/.je4');
        }
        if (!FS.analyzePath('/.je4/builtin').exists) {
            FS.mkdir('/.je4/builtin');
        }

        FS.mount(
            IDBFS,
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
                //////////////////////////// User Input ////////////////////////////
                // Mouse move event
                canvas.addEventListener('mousemove', function (event) {
                    const rect = canvas.getBoundingClientRect();
                    const x = event.clientX - rect.left;
                    const y = event.clientY - rect.top;
                    _je4_je_io_update_mousepos(0, x, y);
                });

                function convert_mouse_button_code(code)
                {
                    switch (code) {
                        case 0:
                            return MOUSE_LEFT;
                        case 1:
                            return MOUSE_MIDDLE;
                        case 2:
                            return MOUSE_RIGHT;
                        default:
                            return null;
                    }
                }
                // Mouse down event
                canvas.addEventListener('mousedown', function (event) {
                    const je_button_code = convert_mouse_button_code(event.button);
                    if (je_button_code !== null)
                        _je4_je_io_update_mouse_state(0, je_button_code, true);
                });

                // Mouse up event
                canvas.addEventListener('mouseup', function (event) {
                    const je_button_code = convert_mouse_button_code(event.button);
                    if (je_button_code !== null)
                        _je4_je_io_update_mouse_state(0, je_button_code, false);
                });

                // Touch event for mobile devices
                const free_touch_point_ids = Array(16).fill(false /* In used? */);
                const recored_touch_points = new Map();

                function get_or_take_a_free_point(identifier)
                {
                    const existed_id = recored_touch_points.get(identifier);
                    if (existed_id !== undefined)
                        return existed_id;

                    for (let i = 0; i < free_touch_point_ids.length; i++)
                    {
                        if (!free_touch_point_ids[i])
                        {
                            free_touch_point_ids[i] = true;
                            recored_touch_points.set(identifier, i);
                            return i;
                        }
                    }
                    return null;
                }
                function release_a_point(identifier)
                {
                    const existed_id = recored_touch_points.get(identifier);
                    if (existed_id !== undefined)
                    {
                        free_touch_point_ids[existed_id] = false;
                        recored_touch_points.delete(identifier);

                        return existed_id;
                    }
                    return null;
                }
                function update_touch_pos(updateTouches, isDown)
                {
                    const rect = canvas.getBoundingClientRect();
                    for (const touch of updateTouches)
                    {
                        const id = touch.identifier;
                        const x = touch.clientX - rect.left;
                        const y = touch.clientY - rect.top;

                        const touch_id = isDown ? get_or_take_a_free_point(id) : release_a_point(id);
                        if (touch_id !== null)
                        {
                            _je4_je_io_update_mousepos(touch_id, x, y);
                            _je4_je_io_update_mouse_state(touch_id, MOUSE_LEFT, isDown);
                        }
                    }
                }
                canvas.addEventListener('touchstart', function (event) {
                    update_touch_pos(event.changedTouches, true);
                });
                canvas.addEventListener('touchmove', function (event) {
                    update_touch_pos(event.changedTouches, true);
                });
                canvas.addEventListener('touchend', function (event) {
                    update_touch_pos(event.changedTouches, false);
                });
                canvas.addEventListener('touchcancel', function (event) {
                    update_touch_pos(event.changedTouches, false);
                });

                // Start the context.
                _je4_js_callback_prepare_surface_context();
            } }););

    emscripten_set_main_loop(webgl_rend_job_callback, 0, 1);
    if (g_surface_context != nullptr)
        delete g_surface_context;

    return 0;
}
