#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs.hpp"

#ifdef JE_ENABLE_DX11_GAPI
#include <Windows.h>

void jegui_init_dx11(
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*),
    void* window_handle,
    void* d11device, 
    void* d11context, 
    bool reboot);
void jegui_update_dx11();
void jegui_shutdown_dx11(bool reboot);
bool jegui_win32_proc_handler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void jegui_win32_append_unicode16_char(wchar_t wch);
#endif

#if defined(JE_ENABLE_GL330_GAPI) \
 || defined(JE_ENABLE_GLES300_GAPI)

#   if defined(JE_OS_ANDROID) && defined(JE_ENABLE_GLES300_GAPI)
#       define JE_GL_USE_EGL_INSTEAD_GLFW
#   endif 

void jegui_init_gl330(
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*),
    void* window_handle, 
    bool reboot);
void jegui_update_gl330();
void jegui_shutdown_gl330(bool reboot);
#endif

#if defined(JE_ENABLE_VK130_GAPI)
#   include <imgui_impl_vulkan.h>

void jegui_init_vk130(
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*),
    void* window_handle,
    ImGui_ImplVulkan_InitInfo* vkinfo,
    VkRenderPass pass,
    VkCommandBuffer cmdbuf);
void jegui_update_vk130(VkCommandBuffer cmdbuf);
void jegui_shutdown_vk130(bool reboot);

#endif

void jegui_init_none(
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*));
void jegui_update_none();
void jegui_shutdown_none(bool reboot);
