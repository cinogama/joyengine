#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_DX11_GAPI
#   include "jeecs_imgui_backend_api.hpp"

#   include <imgui.h>
#   include <Windows.h>
#   include <imgui_impl_win32.h>
#   include <imgui_impl_dx11.h>

void jegui_init_dx11(
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*),
    void* window_handle,
    void* d11device,
    void* d11context,
    bool reboot)
{
    jegui_init_basic(true, get_img_res, apply_shader_sampler);
    ImGui_ImplWin32_Init(window_handle);
    ImGui_ImplDX11_Init(
        (ID3D11Device*)d11device,
        (ID3D11DeviceContext*)d11context);
}

void jegui_update_dx11()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    jegui_update_basic();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void jegui_shutdown_dx11(bool reboot)
{
    jegui_shutdown_basic(reboot);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
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
