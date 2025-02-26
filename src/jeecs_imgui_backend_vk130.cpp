#define JE_IMPL
#include "jeecs.hpp"

#if defined(JE_ENABLE_VK130_GAPI)
#   include "jeecs_imgui_backend_api.hpp"

#   include <imgui.h>
#   include <imgui_impl_vulkan.h>

#   ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#       if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
#           include "imgui_impl_android.h"
#           include <jni.h>
#           include <game-activity/native_app_glue/android_native_app_glue.h>

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
#       else
#           error Unsupport platform.
#       endif
#   else
#       include <imgui_impl_glfw.h>
#       include <GLFW/glfw3.h>
#   endif

void jegui_init_vk130(
    jegl_context::userdata_t ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void* window_handle,
    ImGui_ImplVulkan_InitInfo* vkinfo,
    VkRenderPass pass,
    VkCommandBuffer cmdbuf,
    PFN_vkVoidFunction(*loader_func)(const char* function_name, void* user_data),
    void* user_data)
{
    jegui_init_basic(ctx, true, get_img_res, apply_shader_sampler);
    ImGui_ImplVulkan_LoadFunctions(loader_func, user_data);
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    jegui_android_init((struct android_app*)window_handle);
#   else
#       error Unsupport platform.
#   endif
#else
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)window_handle, true);
#endif
    ImGui_ImplVulkan_Init(vkinfo, pass);

    ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);
}

void jegui_update_vk130(VkCommandBuffer cmdbuf)
{
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    ImGuiIO& io = ImGui::GetIO();

    jegui_android_PollUnicodeChars();
    // Open on-screen (soft) input if requested by Dear ImGui
    static bool WantTextInputLast = false;
    if (io.WantTextInput && !WantTextInputLast)
        jegui_android_ShowSoftKeyboardInput();
    WantTextInputLast = io.WantTextInput;

    jegui_android_handleInputEvent();

#   else
#       error Unsupport platform.
#   endif
#endif

    ImGui_ImplVulkan_NewFrame();
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    ImGui_ImplAndroid_NewFrame();
#   else
#       error Unsupport platform.
#   endif
#else
    ImGui_ImplGlfw_NewFrame();
#endif
    jegui_update_basic();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
}

void jegui_shutdown_vk130(bool reboot)
{
    jegui_shutdown_basic(reboot);
    ImGui_ImplVulkan_Shutdown();
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    ImGui_ImplAndroid_Shutdown();
    // _je_tg_android_app->onInputEvent = nullptr;
    _je_tg_android_app = nullptr;
#   else
#       error Unsupport platform.
#   endif
#else
    ImGui_ImplGlfw_Shutdown();
#endif
    ImGui::DestroyContext();
}

#endif