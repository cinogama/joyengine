#include "jeecs.hpp"

#include <jni.h>
#include <string>
#include <memory>

std::string jni_cstring(JNIEnv* env, jstring str)
{
    jboolean is_copy = false;
    return env->GetStringUTFChars(str, &is_copy);
}
jstring jni_jstring(JNIEnv* env, const char* str)
{
    return env->NewStringUTF(str);
}

class joyengine_client_context
{
    JECS_DISABLE_MOVE_AND_COPY(joyengine_client_context);

    jeecs::typing::type_unregister_guard* m_type_guard;

    jegl_sync_state m_graphic_sync_state;
    jegl_thread* m_graphic_thread_context;

public:
    static void _graphic_thread_created_callback(jegl_thread* gthread, void* _this_vp)
    {
        auto* _this = std::launder(reinterpret_cast<joyengine_client_context*>(_this_vp));
        assert(_this->m_graphic_thread_context == nullptr);
        _this->m_graphic_thread_context = gthread;
    }
    
    joyengine_client_context(const char* native_lib_path)
        : m_graphic_sync_state(jegl_sync_state::JEGL_SYNC_SHUTDOWN)
        , m_graphic_thread_context(nullptr)
    {
        using namespace jeecs;

        wo_set_exe_path(native_lib_path);
        je_init(0, nullptr);

        jegl_register_sync_thread_callback(
            _graphic_thread_created_callback, this);

        m_type_guard = new jeecs::typing::type_unregister_guard();
        entry::module_entry(m_type_guard);
    }

    ~joyengine_client_context()
    {
        entry::module_leave(m_type_guard);
        delete m_type_guard;

        je_finish();
    }

    void graphic_update()
    {
        if (m_graphic_thread_context == nullptr)
            return;

        if (m_graphic_sync_state != jegl_sync_state::JEGL_SYNC_COMPLETE)
        {
            jegl_sync_init(
                m_graphic_thread_context,
                m_graphic_sync_state == jegl_sync_state::JEGL_SYNC_REBOOT);
        }
        m_graphic_sync_state = jegl_sync_update(m_graphic_thread_context);
        if (m_graphic_sync_state != jegl_sync_state::JEGL_SYNC_COMPLETE)
        {
            if (jegl_sync_shutdown(
                m_graphic_thread_context,
                m_graphic_sync_state == jegl_sync_state::JEGL_SYNC_REBOOT))
            {
                m_graphic_thread_context = nullptr;
            }
        }
    }
};

std::unique_ptr<joyengine_client_context> _je4_client_context;

extern "C" JNIEXPORT jstring JNICALL
Java_com_cinogama_joyengineecs4a_MainActivity_initJoyEngine(
    JNIEnv * env,
    jobject /* this */,
    jstring external_path)
{
    const std::string app_dynamic_lib_path = jni_cstring(env, external_path);

    assert(_je4_client_context == nullptr);
    _je4_client_context = std::make_unique<joyengine_client_context>(app_dynamic_lib_path.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_cinogama_joyengineecs4a_MainActivity_updateJoyEngine(
    JNIEnv * env,
    jobject /* this */)
{
    assert(_je4_client_context != nullptr);
    _je4_client_context->graphic_update();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_cinogama_joyengineecs4a_MainActivity_shutdownJoyEngine(
    JNIEnv * env,
    jobject /* this */)
{
    _je4_client_context.reset();
}



extern "C" JNIEXPORT jstring JNICALL
Java_com_cinogama_joyengineecs4a_MainActivity_entry(
        JNIEnv* env,
        jobject /* this */,
        jstring external_path) {

    // 将动态库所在路径设置为woolang的exe-path，这被用于欺骗woolang使用指定
    // 目录内的动态库，同时也有助于后续JoyEngine的工作
    const std::string app_dynamic_lib_path = jni_cstring(env, external_path);
    wo_set_exe_path(app_dynamic_lib_path.c_str());

    using namespace jeecs;
    je_init(0, nullptr);

    auto* guard = new jeecs::typing::type_unregister_guard();
    {
        entry::module_entry(guard);
        {
            // je_main_script_entry();
        }
        entry::module_leave(guard);
    }
    delete guard;
    je_finish();

    return jni_jstring(env, "Hello, JoyEngine");
}