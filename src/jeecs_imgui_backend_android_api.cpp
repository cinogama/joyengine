#define JE_IMPL
#include "jeecs.hpp"

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
#   include <imgui.h>
#   include <imgui_impl_android.h>
#   include <jni.h>
#   include <game-activity/native_app_glue/android_native_app_glue.h>
#   include "jeecs_imgui_backend_android_api.hpp"

thread_local struct android_app* _je_tg_android_app = nullptr;

void jegui_android_handleInputEvent()
{
    ImGuiIO& io = ImGui::GetIO();

    auto pos = jeecs::input::mousepos(0);
    io.AddMousePosEvent(pos.x, pos.y);
    io.AddMouseButtonEvent(0, jeecs::input::mousedown(0, jeecs::input::mousecode::LEFT));
}

void jegui_android_init(struct android_app* app)
{
    _je_tg_android_app = app;
    ImGui_ImplAndroid_Init(_je_tg_android_app->window);
}
void jegui_android_shutdown()
{
    _je_tg_android_app = nullptr;
    ImGui_ImplAndroid_Shutdown();
}

int jegui_android_ShowSoftKeyboardInput()
{
    JavaVM* java_vm = _je_tg_android_app->activity->vm;
    JNIEnv* java_env = nullptr;

    jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
    if (jni_return == JNI_ERR)
        return -1;

    jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
    if (jni_return != JNI_OK)
        return -2;

    jclass native_activity_clazz = java_env->GetObjectClass(_je_tg_android_app->activity->javaGameActivity);
    if (native_activity_clazz == nullptr)
        return -3;

    jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "showSoftInput", "()V");
    if (method_id == nullptr)
        return -4;

    java_env->CallVoidMethod(_je_tg_android_app->activity->javaGameActivity, method_id);

    jni_return = java_vm->DetachCurrentThread();
    if (jni_return != JNI_OK)
        return -5;

    return 0;
}
int jegui_android_PollUnicodeChars()
{
    JavaVM* java_vm = _je_tg_android_app->activity->vm;
    JNIEnv* java_env = nullptr;

    jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
    if (jni_return == JNI_ERR)
        return -1;

    jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
    if (jni_return != JNI_OK)
        return -2;

    jclass native_activity_clazz = java_env->GetObjectClass(_je_tg_android_app->activity->javaGameActivity);
    if (native_activity_clazz == nullptr)
        return -3;

    jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "pollUnicodeChar", "()I");
    if (method_id == nullptr)
        return -4;

    // Send the actual characters to Dear ImGui
    ImGuiIO& io = ImGui::GetIO();
    jint unicode_character;
    while ((unicode_character = java_env->CallIntMethod(_je_tg_android_app->activity->javaGameActivity, method_id)) != 0)
        io.AddInputCharacter(unicode_character);

    jni_return = java_vm->DetachCurrentThread();
    if (jni_return != JNI_OK)
        return -5;

    return 0;
}
#endif
