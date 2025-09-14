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

#ifdef JE_ENABLE_VK130_GAPI
#include <imgui_impl_vulkan.h>

typedef PFN_vkVoidFunction(*jegui_vkapi_loader_func_t)(const char*, void*);
void jegui_init_vk130(
    jegl_context *ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void *window_handle,
    ImGui_ImplVulkan_InitInfo *vkinfo,
    VkRenderPass pass,
    VkCommandBuffer cmdbuf,
    jegui_vkapi_loader_func_t loader_func_maynull,
    void *user_data);
void jegui_update_vk130(VkCommandBuffer cmdbuf);
void jegui_shutdown_vk130(bool reboot);

#endif

#ifdef JE_ENABLE_METAL_GAPI
namespace MTL
{
    class Device;
    class RenderPassDescriptor;
    class CommandBuffer;
    class RenderCommandEncoder;
}

void jegui_init_metal(
    jegl_context* ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void* window_handle,
    MTL::Device* device);
void jegui_update_metal(
    MTL::RenderPassDescriptor* rend_pass_desc,
    MTL::CommandBuffer* command_buffer,
    MTL::RenderCommandEncoder* command_encoder)
void jegui_shutdown_metal(bool reboot);

#endif

void jegui_init_none(
    jegl_context *ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler);
void jegui_update_none();
void jegui_shutdown_none(bool reboot);
