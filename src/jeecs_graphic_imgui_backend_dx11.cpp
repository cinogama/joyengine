#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_DX11_GAPI
#include "jeecs_graphic_imgui_backend_api.hpp"

#include <imgui.h>
#include <Windows.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx11.h>

void jegui_init_dx11(
    je_gl_context_t *ctx,
    je_gui_user_image_loader_t get_img_res,
    je_gui_user_sampler_loader_t apply_shader_sampler,
    void *window_handle,
    void *d11device,
    void *d11context,
    bool reboot)
{
    je_gui_backend_init(ctx, get_img_res, apply_shader_sampler);
    ImGui_ImplGlfw_InitForOther((GLFWwindow *)window_handle, true);
    ImGui_ImplDX11_Init(
        (ID3D11Device *)d11device,
        (ID3D11DeviceContext *)d11context);
}

void jegui_update_dx11()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    je_gui_backend_update(
        [](void *)
        { ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); },
        nullptr);
}

void jegui_shutdown_dx11(bool reboot)
{
    je_gui_backend_shutdown(reboot);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
#endif
