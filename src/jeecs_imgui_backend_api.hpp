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
void jegui_update_dx11(jegl_thread::custom_thread_data_t thread_context);
void jegui_shutdown_dx11(bool reboot);
bool jegui_win32_proc_handler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void jegui_win32_append_unicode16_char(wchar_t wch);
#endif

#if defined(JE_ENABLE_GL330_GAPI) \
 || defined(JE_ENABLE_GLES320_GAPI)
void jegui_init_gl330(
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*),
    void* window_handle, 
    bool reboot);
void jegui_update_gl330(jegl_thread::custom_thread_data_t thread_context);
void jegui_shutdown_gl330(bool reboot);
#endif
