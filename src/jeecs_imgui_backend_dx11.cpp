#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_DX11_GAPI
#   include "jeecs_imgui_backend_api.hpp"

#   include <imgui.h>
#   include <Windows.h>
#   include <imgui_impl_glfw.h>
#   include <imgui_impl_dx11.h>

void jegui_init_dx11(
    jegl_context* ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void* window_handle,
    void* d11device,
    void* d11context,
    bool reboot)
{
    jegui_init_basic(ctx, true, get_img_res, apply_shader_sampler);
    ImGui_ImplGlfw_InitForOther((GLFWwindow*)window_handle, true);
    ImGui_ImplDX11_Init(
        (ID3D11Device*)d11device,
        (ID3D11DeviceContext*)d11context);
}

void jegui_update_dx11()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    jegui_update_basic(
        [](void*) {ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); },
        nullptr);
}

void jegui_shutdown_dx11(bool reboot)
{
    jegui_shutdown_basic(reboot);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool jegui_win32_proc_handler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;
    return false;
}

void jegui_win32_append_unicode16_char(wchar_t wch)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacterUTF16(wch);
}

#endif
