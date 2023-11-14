#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_imgui_backend_api.hpp"

#include <imgui.h>

void jegui_init_none(
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*))
{
    jegui_init_basic(false, get_img_res, apply_shader_sampler);
    ImGuiIO& io = ImGui::GetIO();

    unsigned char* tex_pixels = nullptr;
    int tex_w, tex_h;
    io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);
}

void jegui_update_none()
{
    jegui_update_basic();
}

void jegui_shutdown_none(bool reboot)
{
    jegui_shutdown_basic(reboot);
}
