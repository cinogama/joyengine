#include <jni.h>

//#include "AndroidOut.h"
//#include "Renderer.h"

#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>

#include "jeecs.hpp"

struct jegl_android_surface_manager
{
    inline static jegl_thread* _jegl_graphic_thread = nullptr;
    inline static jegl_sync_state _jegl_graphic_thread_state = jegl_sync_state::JEGL_SYNC_SHUTDOWN;

    inline static bool _jegl_android_update_pause_request = false;
    inline static bool _jegl_android_update_paused = false;

    static void _jegl_android_sync_thread_created(jegl_thread* gthread, void*)
    {
        _jegl_graphic_thread = gthread;
    }

    static void sync_begin(void* android_window)
    {
        _jegl_android_update_pause_request = false;
        _jegl_android_pause_update = false;
        _jegl_graphic_thread_state = jegl_sync_state::JEGL_SYNC_SHUTDOWN;
        jegl_register_sync_thread_callback(
            _jegl_android_sync_thread_created, android_window);
    }
    static void sync_pause()
    {
        // The app enters the background, will destroy surface & context..
        // Send a reboot signal, and stop update until awake by sync_begin.
        assert(_jegl_graphic_thread != nullptr);
        assert(_jegl_android_update_pause_request == false);
        assert(_jegl_android_pause_update == false);
        _jegl_android_update_pause_request = true;
        jegl_reboot_graphic_thread(_jegl_graphic_thread, _jegl_graphic_thread->m_config);
    }

    static void sync_update()
    {
        if (!_jegl_android_pause_update && _jegl_graphic_thread != nullptr)
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
                
                if (_jegl_android_update_pause_request)
                {
                    _jegl_android_update_pause_request = false;
                    _jegl_android_pause_update = true;
                }
            }
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

            jegl_android_surface_manager::sync_begin(pApp->window);

            // TODO: Execute script entry at another thread.
            // je_main_script_entry(true);

            // DEBUG:
            using namespace jeecs;

            auto u = game_universe::create_universe();
            auto w = u.create_world();

            w.add_system(typing::type_info::of("Graphic::UnlitGraphicPipelineSystem"));
            w.add_system(typing::type_info::of("Translation::TranslationUpdatingSystem"));

            auto camera_entity = w.add_entity<
                Transform::LocalPosition,
                Transform::LocalToWorld,
                Transform::Translation,
                Camera::OrthoProjection,
                Camera::Projection>();

            auto block_entity = w.add_entity<
                Transform::LocalPosition,
                Transform::LocalRotation,
                Transform::LocalToWorld,
                Transform::Translation,
                Renderer::Shaders,
                Renderer::Shape,
                Renderer::Textures>();

            block_entity.get_component<Transform::LocalPosition>()->pos.z = 5.0f;
            block_entity.get_component<Transform::LocalRotation>()->rot = math::quat::euler(0.f, 0.f, 15.0f);

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
        jstring external_path)
{
    wo_set_exe_path(jni_cstring(env, external_path).c_str());
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

                jegl_android_surface_manager::sync_update();

            } while (!pApp->destroyRequested);
        }
        entry::module_leave(guard);
    }

    je_log_unregister_callback(logcallback);
    je_finish();
}
}