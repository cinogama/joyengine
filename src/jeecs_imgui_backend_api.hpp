#pragma once

#ifndef JE_IMPL
#define JE_IMPL
#endif
#include "jeecs.hpp"

#ifdef JE_ENABLE_DX11_GAPI
#include <Windows.h>

void jegui_init_dx11(
    jegl_context *ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void *window_handle,
    void *d11device,
    void *d11context,
    bool reboot);
void jegui_update_dx11();
void jegui_shutdown_dx11(bool reboot);
bool jegui_win32_proc_handler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void jegui_win32_append_unicode16_char(wchar_t wch);
#endif

#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)

#if defined(JE_ENABLE_GLES300_GAPI) && JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
#define JE_GL_USE_EGL_INSTEAD_GLFW
#endif

void jegui_init_gl330(
    jegl_context *ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void *window_handle,
    bool reboot);
void jegui_update_gl330();
void jegui_shutdown_gl330(bool reboot);
#endif

#if defined(JE_ENABLE_VK130_GAPI)
#undef IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <imgui_impl_vulkan.h>

void jegui_init_vk130(
    jegl_context *ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void *window_handle,
    ImGui_ImplVulkan_InitInfo *vkinfo,
    VkRenderPass pass,
    VkCommandBuffer cmdbuf,
    PFN_vkVoidFunction (*loader_func)(const char *function_name, void *user_data),
    void *user_data);
void jegui_update_vk130(VkCommandBuffer cmdbuf);
void jegui_shutdown_vk130(bool reboot);

#endif

void jegui_init_none(
    jegl_context *ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler);
void jegui_update_none();
void jegui_shutdown_none(bool reboot);

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
#   include <vulkan/vulkan_win32.h>
#   define JE4_VK_USE_DYNAMIC_VK_LIB 1
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
#   include <vulkan/vulkan_android.h>
#   define JE4_VK_USE_DYNAMIC_VK_LIB 0
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_LINUX
#   include <vulkan/vulkan_xlib.h>
#   define JE4_VK_USE_DYNAMIC_VK_LIB 1
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_MACOS
#   include <vulkan/vulkan_macos.h>
#   define JE4_VK_USE_DYNAMIC_VK_LIB 0
#else
#   error Unsupport platform.
#   define JE4_VK_USE_DYNAMIC_VK_LIB 1
#endif
