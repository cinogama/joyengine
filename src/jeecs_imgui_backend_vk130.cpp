#define JE_IMPL
#include "jeecs.hpp"

#if defined(JE_ENABLE_VK120_GAPI)
#include "jeecs_imgui_backend_api.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
#       include <imgui_impl_android.h>
#       include "jeecs_imgui_backend_android_api.hpp"
#   endif
#else
#   include <imgui_impl_glfw.h>
#   include <GLFW/glfw3.h>
#endif

void jegui_init_vk120(
    jegl_context *ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void *window_handle,
    ImGui_ImplVulkan_InitInfo *vkinfo,
    VkRenderPass pass,
    VkCommandBuffer cmdbuf,
    jegui_vkapi_loader_func_t loader_func_maynull,
    void *user_data)
{
    jegui_init_basic(ctx, get_img_res, apply_shader_sampler);

    if (loader_func_maynull != nullptr)
        ImGui_ImplVulkan_LoadFunctions(loader_func_maynull, user_data);

#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    jegui_android_init(window_handle);
#else
#error Unsupport platform.
#endif
#else
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)window_handle, true);
#endif
    ImGui_ImplVulkan_Init(vkinfo, pass);

    ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);
}

void jegui_update_vk120(VkCommandBuffer cmdbuf)
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

    ImGui_ImplVulkan_NewFrame();
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
    ImGui_ImplAndroid_NewFrame();
#else
#error Unsupport platform.
#endif
#else
    ImGui_ImplGlfw_NewFrame();
#endif
    jegui_update_basic(
        [](void *p_cmdbuf)
        {
            ImGui_ImplVulkan_RenderDrawData(
                ImGui::GetDrawData(),
                *reinterpret_cast<VkCommandBuffer *>(p_cmdbuf));
        },
        &cmdbuf);
}

void jegui_shutdown_vk120(bool reboot)
{
    jegui_shutdown_basic(reboot);
    ImGui_ImplVulkan_Shutdown();
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