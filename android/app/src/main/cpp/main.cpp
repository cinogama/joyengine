#include <jni.h>

#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>

#include "jeecs.hpp"

struct _jegl_window_android_app
{
    void* m_android_app;
    void* m_android_window;
};

struct jegl_android_surface_manager
{
    inline static jegl_thread* _jegl_graphic_thread;
    inline static jegl_sync_state _jegl_graphic_thread_state;

    inline static bool _jegl_android_update_paused = false;
    inline static _jegl_window_android_app _jegl_window_android_app;

    static void _jegl_android_sync_thread_created(jegl_thread* gthread, void*)
    {
        _jegl_graphic_thread = gthread;
    }

    static void sync_begin(void* android_app, void* android_window)
    {
        _jegl_window_android_app.m_android_app = android_app;
        _jegl_window_android_app.m_android_window = android_window;

        if (_jegl_android_update_paused)
        {
            assert(_jegl_graphic_thread != nullptr);
            _jegl_android_update_paused = false;
        }
        else
        {
            _jegl_android_update_paused = false;
            _jegl_graphic_thread_state = jegl_sync_state::JEGL_SYNC_SHUTDOWN;
            jegl_register_sync_thread_callback(
                _jegl_android_sync_thread_created, &_jegl_window_android_app);

            // Execute script entry in another thread.
            std::thread(je_main_script_entry).detach();
        }

    }
    static void sync_pause()
    {
        // The app enters the background, will destroy surface & context..
        // Send a reboot signal, and stop update until awake by sync_begin.
        assert(_jegl_graphic_thread != nullptr);
        _jegl_android_update_paused = true;

        jegl_reboot_graphic_thread(_jegl_graphic_thread, nullptr);

        // Let update to close interface here.
        sync_update();
    }
    static bool sync_update()
    {
        if (_jegl_android_update_paused &&
            _jegl_graphic_thread_state != jegl_sync_state::JEGL_SYNC_COMPLETE)
        {
            je_clock_sleep_for(0.1);
            return true;
        }

        if (_jegl_graphic_thread != nullptr)
        {
            if (_jegl_graphic_thread_state != jegl_sync_state::JEGL_SYNC_COMPLETE)
                jegl_sync_init(
                    _jegl_graphic_thread,
                    _jegl_graphic_thread_state == jegl_sync_state::JEGL_SYNC_REBOOT);

            _jegl_graphic_thread_state = jegl_sync_update(_jegl_graphic_thread);
            if (_jegl_graphic_thread_state != jegl_sync_state::JEGL_SYNC_COMPLETE)
            {
                if (jegl_sync_shutdown(
                    _jegl_graphic_thread,
                    _jegl_graphic_thread_state == jegl_sync_state::JEGL_SYNC_REBOOT))
                    _jegl_graphic_thread = nullptr;
            }

            return true;
        }
        return false;
    }
    static void sync_shutdown()
    {
        if (_jegl_graphic_thread != nullptr)
        {
            _jegl_graphic_thread_state = jegl_sync_state::JEGL_SYNC_SHUTDOWN;
            _jegl_graphic_thread = nullptr;
        }
    }
};

extern "C" {

#include <game-activity/native_app_glue/android_native_app_glue.c>

/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            // A new window is created, associate a renderer with it. You may replace this with a
            // "game" class if that suits your needs. Remember to change all instances of userData
            // if you change the class here as a reinterpret_cast is dangerous this in the
            // android_main function and the APP_CMD_TERM_WINDOW handler case.

            jegl_android_surface_manager::sync_begin(pApp, pApp->window);

            break;
        }
        case APP_CMD_TERM_WINDOW:
            // The window is being destroyed. Use this to clean up your userData to avoid leaking
            // resources.
            //
            // We have to check if userData is assigned just in case this comes in really quickly
            jegl_android_surface_manager::sync_pause();
            break;
        default:
            break;
    }
}

/*!
 * Enable the motion events you want to handle; not handled events are
 * passed back to OS for further processing. For this example case,
 * only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent) {
    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}

std::string jni_cstring(JNIEnv* env, jstring str)
{
    jboolean is_copy = false;
    return env->GetStringUTFChars(str, &is_copy);
}
jstring jni_jstring(JNIEnv* env, const char* str)
{
    return env->NewStringUTF(str);
}

extern "C" JNIEXPORT void JNICALL
Java_net_cinogama_joyengineecs4a_MainActivity_initJoyEngine(
        JNIEnv * env,
        jobject /* this */,
        jstring library_path,
        jstring cache_path,
        jstring asset_path)
{
    wo_set_exe_path(jni_cstring(env, library_path).c_str());
    jeecs_file_set_host_path(jni_cstring(env, cache_path).c_str());
    jeecs_file_set_runtime_path(jni_cstring(env, asset_path).c_str());
}

void _je_log_to_android(int level, const char* msg, void*)
{
    switch(level)
    {
    case JE_LOG_FATAL:
        LOGE("%s", msg); break;
    case JE_LOG_ERROR:
        LOGE("%s", msg); break;
    case JE_LOG_WARNING:
        LOGW("%s", msg); break;
    case JE_LOG_INFO:
        LOGI("%s", msg); break;
    case JE_LOG_NORMAL:
    default:
        LOGV("%s", msg); break;
    }
}

void _je_handle_inputs(struct android_app *app_) {
    // handle all queued inputs
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) {
        // no inputs yet.
        return;
    }

    // handle motion events (motionEventsCounts can be 0).
    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;

        // Find the pointer index, mask and bitshift to turn it into a readable value.
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        // get the x and y position of this event if it is not ACTION_MOVE.
        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);

        // determine the action type and process the event accordingly.
        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                je_io_update_mouse_state(
                    pointer.id, jeecs::input::mousecode::LEFT, true);
                je_io_update_mousepos(pointer.id, x, y);
                break;

            case AMOTION_EVENT_ACTION_CANCEL:
                // treat the CANCEL as an UP event: doing nothing in the app, except
                // removing the pointer from the cache if pointers are locally saved.
                // code pass through on purpose.
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                je_io_update_mouse_state(
                    pointer.id, jeecs::input::mousecode::LEFT, false);
                je_io_update_mousepos(pointer.id, x, y);
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                // There is no pointer index for ACTION_MOVE, only a snapshot of
                // all active pointers; app needs to cache previous active pointers
                // to figure out which ones are actually moved.
                for (auto index = 0; index < motionEvent.pointerCount; index++) {
                    pointer = motionEvent.pointers[index];
                    x = GameActivityPointerAxes_getX(&pointer);
                    y = GameActivityPointerAxes_getY(&pointer);
                    je_io_update_mousepos(pointer.id, x, y);
                }
                break;
            default:
                // ?
                ;
        }
    }
    // clear the motion input count in this buffer for main thread to re-use.
    android_app_clear_motion_events(inputBuffer);

    // handle input key events.
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];

        switch (keyEvent.action) {
            case AKEY_EVENT_ACTION_DOWN:
                break;
            case AKEY_EVENT_ACTION_UP:
                break;
            case AKEY_EVENT_ACTION_MULTIPLE:
                // Deprecated since Android API level 29.
                break;
            default:
                ;
        }
    }
    // clear the key input count too.
    android_app_clear_key_events(inputBuffer);
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
    // Register an event handler for Android events
    pApp->onAppCmd = handle_cmd;

    // Set input event filters (set it to NULL if the app wants to process all inputs).
    // Note that for key inputs, this example uses the default default_key_filter()
    // implemented in android_native_app_glue.c.
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    using namespace jeecs;

    je_init(0, nullptr);

    auto logcallback = je_log_register_callback(_je_log_to_android, nullptr);

    auto* guard = new typing::type_unregister_guard();
    {
        entry::module_entry(guard);
        {
            // This sets up a typical game/event loop. It will run until the app is destroyed.
            int events;
            android_poll_source *pSource;
            do {
                // Process all pending events before running game logic.
                if (ALooper_pollAll(0, nullptr, &events, (void **) &pSource) >= 0) {
                    if (pSource) {
                        pSource->process(pApp, pSource);
                    }
                }

                // Update frame sync.
                _je_handle_inputs(pApp);
                jegl_android_surface_manager::sync_update();
            } while (!pApp->destroyRequested);

            // Application is requesting to exit.
            jegl_android_surface_manager::sync_shutdown();
        }
        entry::module_leave(guard);
    }

    je_log_unregister_callback(logcallback);
    je_finish();
}
}