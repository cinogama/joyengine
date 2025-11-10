#define JE_IMPL
#include "jeecs.hpp"

#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)

#include "jeecs_imgui_backend_api.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>

#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
#       include <imgui_impl_android.h>
#       include "jeecs_imgui_backend_android_api.hpp"
#   endif
#else
#   include <imgui_impl_glfw.h>
#   include <GLFW/glfw3.h>
#endif

void jegui_init_gl330(
    jegl_context *ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void *window_handle,
    bool reboot)
{
    jegui_init_basic(ctx, get_img_res, apply_shader_sampler);
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    jegui_android_init(window_handle);
#else
#   error Unsupport platform.
#endif
#else
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow *)window_handle, true);
#endif
    ImGui_ImplOpenGL3_Init(nullptr);
}

void jegui_update_gl330()
{
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    ImGuiIO &io = ImGui::GetIO();

    jegui_android_PollUnicodeChars();
    // Open on-screen (soft) input if requested by Dear ImGui
    static bool WantTextInputLast = false;
    if (io.WantTextInput && !WantTextInputLast)
        jegui_android_ShowSoftKeyboardInput();
    WantTextInputLast = io.WantTextInput;

    jegui_android_handleInputEvent();

#else
#error Unsupport platform.
#endif
#endif

    ImGui_ImplOpenGL3_NewFrame();
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    ImGui_ImplAndroid_NewFrame();
#else
#error Unsupport platform.
#endif
#else
    ImGui_ImplGlfw_NewFrame();
#endif

#ifndef JE_GL_USE_EGL_INSTEAD_GLFW
    GLFWwindow *backup_current_context = glfwGetCurrentContext();
#endif
    jegui_update_basic(
        [](void *)
        { ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); },
        nullptr);

#ifndef JE_GL_USE_EGL_INSTEAD_GLFW
    glfwMakeContextCurrent(backup_current_context);
#endif
}

void jegui_shutdown_gl330(bool reboot)
{
    jegui_shutdown_basic(reboot);
    ImGui_ImplOpenGL3_Shutdown();
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    jegui_android_shutdown();
#else
#error Unsupport platform.
#endif
#else
    ImGui_ImplGlfw_Shutdown();
#endif
    ImGui::DestroyContext();
}

#endif