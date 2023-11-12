#include <jni.h>
#include <string>

#include "jeecs.hpp"

std::string jni_cstring(JNIEnv* env, jstring str)
{
    jboolean is_copy = false;
    return env->GetStringUTFChars(str, &is_copy);
}
jstring jni_jstring(JNIEnv* env, const char* str)
{
    return env->NewStringUTF(str);
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