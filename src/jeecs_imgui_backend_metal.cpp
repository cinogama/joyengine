#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_METAL_GAPI
#include "jeecs_imgui_backend_api.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_metal.h>

void jegui_init_metal(
    jegl_context* ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    MTL::Device* device)
{
    jegui_init_basic(ctx, get_img_res, apply_shader_sampler);
    // ImGui_ImplMetal_Init(device);
}

void jegui_update_metal()
{
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2(1920, 1080);
    io.DeltaTime = 1.0f / 60.0f;

    jegui_update_basic([](void*) {}, nullptr);
}

void jegui_shutdown_metal(bool reboot)
{
    jegui_shutdown_basic(reboot);
}
#endif