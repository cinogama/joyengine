#include <jni.h>

#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>

#include "jeecs.hpp"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <sys/stat.h>

#include <optional>

extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>
}

struct _jegl_window_android_app
{
    void* m_android_app;
    void* m_android_window;
};

class je_game_engine_context_for_android : jeecs::game_engine_context
{
    JECS_DISABLE_MOVE_AND_COPY(je_game_engine_context_for_android);

    struct app_and_graphic_context
    {
        _jegl_window_android_app m_app_context;
        jeecs::graphic::graphic_syncer_host* m_graphic_syncer;
    };
    struct application_request
    {
        enum class request_kind
        {
            INIT_REND_WINDOW,
            TERM_REND_WINDOW,
        };
        union request_argument
        {
            _jegl_window_android_app m_app_context;
        };
        request_kind m_type;
        request_argument m_argm;
    };

    bool m_frame_update_paused;
    std::optional<app_and_graphic_context> m_android_app_context;
    std::optional<application_request> m_request;

    je_log_regid_t m_registered_log_callback_id;
    static void _je_log_to_android(int level, const char* msg, void*)
    {
        switch (level)
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

public:
    je_game_engine_context_for_android(struct android_app* app)
        : game_engine_context(0, nullptr)
        , m_frame_update_paused(true)
        , m_android_app_context(std::nullopt)
        , m_request(std::nullopt)
    {
        m_registered_log_callback_id =
            je_log_register_callback(
                &je_game_engine_context_for_android::_je_log_to_android,
                nullptr);

        JavaVM* java_vm = app->activity->vm;
        JNIEnv* java_env = nullptr;

        jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
        if (jni_return == JNI_ERR)
            abort();

        jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
        if (jni_return != JNI_OK)
            abort();

        jclass native_activity_clazz = java_env->GetObjectClass(app->activity->javaGameActivity);
        if (native_activity_clazz == nullptr)
            abort();

        jmethodID method_id = java_env->GetMethodID(
            native_activity_clazz, "doInitJoyEngineBasicConfig", "()V");

        if (method_id == nullptr)
            abort();

        java_env->CallVoidMethod(app->activity->javaGameActivity, method_id);

        jni_return = java_vm->DetachCurrentThread();
        if (jni_return != JNI_OK)
            abort();

        jeecs::debug::loginfo("Android application started!");
    }
    ~je_game_engine_context_for_android()
    {
        je_log_unregister_callback(m_registered_log_callback_id);
    }

    enum class app_request_result
    {
        GRAPHIC_FRAME_NEED_TO_UPDATE,
        GRAPHIC_FRAME_NOT_READY,
        GRAPHIC_FAILED_TO_INIT,
    };

    void request_to_init(
        android_app* app)
    {
        auto& request = m_request.emplace();

        request.m_type = application_request::request_kind::INIT_REND_WINDOW;
        request.m_argm.m_app_context.m_android_app = app;
        request.m_argm.m_app_context.m_android_window = app->window;
    }

    void request_to_term()
    {
        auto& request = m_request.emplace();
        request.m_type = application_request::request_kind::TERM_REND_WINDOW;
    }

    app_request_result process_app_request()
    {
        if (m_request.has_value())
        {
            auto request = m_request.value();
            m_request.reset();

            switch (request.m_type)
            {
            case application_request::request_kind::INIT_REND_WINDOW:
                if (m_frame_update_paused)
                {
                    if (!m_android_app_context.has_value())
                    {
                        auto* graphic_syncer = prepare_graphic(true);
                        if (!graphic_syncer->check_context_ready_block(false))
                        {
                            // Entry script ended.
                            jeecs::debug::logerr(
                                "No graphic context requested during whole entry script execution.");

                            return app_request_result::GRAPHIC_FAILED_TO_INIT;
                        }

                        auto& app_context = m_android_app_context.emplace();

                        // Set graphic syncer:
                        app_context.m_graphic_syncer = graphic_syncer;

                        // Update app context:
                        app_context.m_app_context = request.m_argm.m_app_context;


                        auto* graphic_context =
                            graphic_syncer->get_graphic_context_after_context_ready();
                        graphic_context->m_config.m_userdata = &app_context.m_app_context;

                        jegl_sync_init(graphic_context, false);
                    }
                    else
                    {
                        auto& app_context = m_android_app_context.value();

                        assert(app_context.m_graphic_syncer != nullptr);

                        // Update app context:
                        app_context.m_app_context = request.m_argm.m_app_context;

                        auto* graphic_context =
                            app_context.m_graphic_syncer->get_graphic_context_after_context_ready();
                        graphic_context->m_config.m_userdata = &app_context.m_app_context;

                        jegl_reboot_graphic_thread(graphic_context, nullptr);
                    }

                    m_frame_update_paused = false;
                }
                else
                    // Or bad request? log warning:
                    jeecs::debug::logwarn("Bad application request in process_app_request.");
                break;
            case application_request::request_kind::TERM_REND_WINDOW:
                if (!m_frame_update_paused)
                {
                    m_frame_update_paused = true;
                }
                break;
            }
        }
        if (m_frame_update_paused)
            return app_request_result::GRAPHIC_FRAME_NOT_READY;
        return app_request_result::GRAPHIC_FRAME_NEED_TO_UPDATE;
    }
    /* RETURN FALSE TO EXIT PROCESS */
    bool update_frame()
    {
        switch (process_app_request())
        {
        case app_request_result::GRAPHIC_FRAME_NEED_TO_UPDATE:
            if (frame() == jeecs::game_engine_context::frame_update_result::FRAME_UPDATE_CLOSE_REQUESTED)
                return false;
            break;
        case app_request_result::GRAPHIC_FRAME_NOT_READY:
            je_clock_sleep_for(0.1);
            break;
        case app_request_result::GRAPHIC_FAILED_TO_INIT:
            return false;
        default:
            jeecs::debug::logfatal("Unknown app_request_result value.");
            return false;
        }
        return true;
    }
};

AAssetManager* _asset_manager = nullptr;
je_game_engine_context_for_android* g_android_game_engine_context = nullptr;

struct _je_file_instance
{
    FILE* m_raw_file;
    AAsset* m_asset_file;

    JECS_DISABLE_MOVE_AND_COPY(_je_file_instance);
public:
    int close()
    {
        if (m_asset_file != nullptr)
        {
            AAsset_close(m_asset_file);
            m_asset_file = nullptr;
            return 0;
        }

        assert(m_raw_file != nullptr);
        auto result = fclose(m_raw_file);
        m_raw_file = nullptr;
        return result;
    }

    _je_file_instance(AAsset* asset)
        : m_raw_file(nullptr)
        , m_asset_file(asset)
    {
        assert(m_asset_file != nullptr);
    }
    _je_file_instance(FILE* file)
        : m_raw_file(file)
        , m_asset_file(nullptr)
    {
        assert(m_raw_file != nullptr);
    }

    ~_je_file_instance()
    {
        assert(m_raw_file == nullptr && m_asset_file == nullptr);
    }

public:
    static jeecs_raw_file _je_android_file_open(const char* path, size_t* out_len)
    {
        if (_asset_manager != nullptr && path[0] == '#')
        {
            AAsset* asset = AAssetManager_open(
                _asset_manager,
                path[1] == '/' ? path + 2 : path + 1,
                AASSET_MODE_BUFFER);

            if (asset)
            {
                *out_len = AAsset_getLength(asset);
                return new _je_file_instance(asset);
            }
        }

        FILE* fhandle = fopen(path, "rb");
        if (fhandle)
        {
            struct stat cfstat;
            if (stat(path, &cfstat) != 0)
            {
                fclose(fhandle);
                return nullptr;
            }
            *out_len = cfstat.st_size;
            return new _je_file_instance(fhandle);
        }
        return nullptr;
    }
    static size_t _je_android_file_read(void* buffer, size_t elemsz, size_t elemcount, jeecs_raw_file file)
    {
        auto* finstance = (_je_file_instance*)file;
        if (finstance->m_asset_file != nullptr)
        {
            // TODO: Uncomplete elem, need rewind?
            return AAsset_read(finstance->m_asset_file, buffer, elemsz * elemcount) / elemsz;
        }
        else
        {
            assert(finstance->m_raw_file != nullptr);
            return fread(buffer, elemsz, elemcount, finstance->m_raw_file);
        }
    }
    static size_t _je_android_file_tell(jeecs_raw_file file)
    {
        auto* finstance = (_je_file_instance*)file;
        if (finstance->m_asset_file != nullptr)
        {
            return AAsset_getLength64(finstance->m_asset_file)
                - AAsset_getRemainingLength64(finstance->m_asset_file);
        }
        else
        {
            assert(finstance->m_raw_file != nullptr);
            return ftell(finstance->m_raw_file);
        }
    }
    static int _je_android_file_seek(jeecs_raw_file file, int64_t offset, je_read_file_seek_mode mode)
    {
        auto* finstance = (_je_file_instance*)file;
        if (finstance->m_asset_file != nullptr)
        {
            if (-1 != AAsset_seek64(finstance->m_asset_file, offset, mode))
                return 0;
            return -1;
        }
        else
        {
            assert(finstance->m_raw_file != nullptr);
            return fseek(finstance->m_raw_file, offset, mode);
        }
    }
    static int _je_android_file_close(jeecs_raw_file file)
    {
        auto* finstance = (_je_file_instance*)file;

        auto ret = finstance->close();
        delete finstance;

        return ret;
    }
};

std::string jni_cstring(JNIEnv* env, jstring str)
{
    jboolean is_copy = false;
    return env->GetStringUTFChars(str, &is_copy);
}
jstring jni_jstring(JNIEnv* env, const char* str)
{
    return env->NewStringUTF(str);
}

extern "C" {
    /*!
     * Handles commands sent to this Android application
     * @param pApp the app the commands are coming from
     * @param cmd the command to handle
     */
    void handle_cmd(android_app* pApp, int32_t cmd) {
        switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            // A new window is created, associate a renderer with it. You may replace this with a
            // "game" class if that suits your needs. Remember to change all instances of userData
            // if you change the class here as a reinterpret_cast is dangerous this in the
            // android_main function and the APP_CMD_TERM_WINDOW handler case.
            g_android_game_engine_context->request_to_init(pApp);
            break;
        }
        case APP_CMD_TERM_WINDOW:
            // The window is being destroyed. Use this to clean up your userData to avoid leaking
            // resources.
            //
            // We have to check if userData is assigned just in case this comes in really quickly
            g_android_game_engine_context->request_to_term();
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
    bool motion_event_filter_func(const GameActivityMotionEvent* motionEvent) {
        auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
        return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
    }

    extern "C" JNIEXPORT void JNICALL
        Java_net_cinogama_joyengineecs4a_MainActivity_initJoyEngine(
            JNIEnv* env,
            jobject /* this */,
            jobject asset_manager,
            jstring library_path,
            jstring cache_path,
            jstring asset_path)
    {
        _asset_manager = AAssetManager_fromJava(env, asset_manager);

        jeecs_register_native_file_operator(
            &_je_file_instance::_je_android_file_open,
            &_je_file_instance::_je_android_file_read,
            &_je_file_instance::_je_android_file_tell,
            &_je_file_instance::_je_android_file_seek,
            &_je_file_instance::_je_android_file_close);

        wo_set_exe_path(jni_cstring(env, library_path).c_str());
        jeecs_file_set_host_path(jni_cstring(env, cache_path).c_str());
        jeecs_file_set_runtime_path(jni_cstring(env, asset_path).c_str());

        jeecs_file_update_default_fimg("#");
    }

    void _je_handle_inputs(struct android_app* app_) {
        // handle all queued inputs
        auto* inputBuffer = android_app_swap_input_buffers(app_);
        if (!inputBuffer) {
            // no inputs yet.
            return;
        }

        // handle motion events (motionEventsCounts can be 0).
        for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
            auto& motionEvent = inputBuffer->motionEvents[i];
            auto action = motionEvent.action;

            // Find the pointer index, mask and bitshift to turn it into a readable value.
            auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

            // get the x and y position of this event if it is not ACTION_MOVE.
            auto& pointer = motionEvent.pointers[pointerIndex];
            auto x = GameActivityPointerAxes_getX(&pointer);
            auto y = GameActivityPointerAxes_getY(&pointer);

            // determine the action type and process the event accordingly.
            switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                je_io_update_mouse_state(
                    pointer.id, jeecs::input::mousecode::LEFT, true);
                je_io_update_mouse_pos(pointer.id, x, y);
                break;

            case AMOTION_EVENT_ACTION_CANCEL:
                // treat the CANCEL as an UP event: doing nothing in the app, except
                // removing the pointer from the cache if pointers are locally saved.
                // code pass through on purpose.
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                je_io_update_mouse_state(
                    pointer.id, jeecs::input::mousecode::LEFT, false);
                je_io_update_mouse_pos(pointer.id, x, y);
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                // There is no pointer index for ACTION_MOVE, only a snapshot of
                // all active pointers; app needs to cache previous active pointers
                // to figure out which ones are actually moved.
                for (auto index = 0; index < motionEvent.pointerCount; index++) {
                    pointer = motionEvent.pointers[index];
                    x = GameActivityPointerAxes_getX(&pointer);
                    y = GameActivityPointerAxes_getY(&pointer);
                    je_io_update_mouse_pos(pointer.id, x, y);
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
            auto& keyEvent = inputBuffer->keyEvents[i];

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
    void android_main(struct android_app* pApp) {
        // Register an event handler for Android events
        pApp->onAppCmd = handle_cmd;

        // Set input event filters (set it to NULL if the app wants to process all inputs).
        // Note that for key inputs, this example uses the default default_key_filter()
        // implemented in android_native_app_glue.c.
        android_app_set_motion_event_filter(pApp, motion_event_filter_func);

        je_game_engine_context_for_android game_engine_context(pApp);
        g_android_game_engine_context = &game_engine_context;

        // This sets up a typical game/event loop. It will run until the app is destroyed.
        int events;
        android_poll_source* pSource;
        do {
            // Process all pending events before running game logic.
            if (ALooper_pollAll(0, nullptr, &events, (void**)&pSource) >= 0) {
                if (pSource) {
                    pSource->process(pApp, pSource);
                }
            }
            
            // Update frame sync.
            _je_handle_inputs(pApp);

            if (!game_engine_context.update_frame())
                // Exit requested.
                break;

        } while (!pApp->destroyRequested);

        jeecs::debug::loginfo("Android application exiting...");
    }
}