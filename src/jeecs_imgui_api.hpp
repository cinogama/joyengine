#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs.hpp"

// jeecs_imgui_api.hpp

void jegui_set_font(const char* path, size_t size);

#ifdef _WIN32
#include <Windows.h>

void jegui_init_dx11(
    void* window_handle,
    void* d11device, 
    void* d11context, 
    bool reboot);
void jegui_update_dx11(jegl_thread* thread_context);
void jegui_shutdown_dx11(bool reboot);
bool jegui_win32_proc_handler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

void jegui_init_gl330(void* window_handle, bool reboot);
void jegui_update_gl330(jegl_thread* thread_context);
void jegui_shutdown_gl330(bool reboot);

// 用于点击退出窗口时的最终回调，仅允许解除一次。
// 再次设置之前需要先解除注册
// 允许在woolang脚本中注册退出回调。
//
// 返回值由注册的回调函数指定
// 返回 true 表示退出操作被确认
// 
bool jegui_shutdown_callback(void);
