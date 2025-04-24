#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_imgui_backend_api.hpp"

#include <imgui.h>

void jegui_init_none(
    jegl_context* ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler)
{
    jegui_init_basic(ctx, false, get_img_res, apply_shader_sampler);
    ImGuiIO& io = ImGui::GetIO();

    unsigned char* tex_pixels = nullptr;
    int tex_w, tex_h;
    io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);
}

void jegui_update_none()
{
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2(1920, 1080);
    io.DeltaTime = 1.0f / 60.0f;

    jegui_update_basic([](void*) {}, nullptr);
}

void jegui_shutdown_none(bool reboot)
{
    jegui_shutdown_basic(reboot);
}
