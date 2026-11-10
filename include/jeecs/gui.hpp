#pragma once

// ============================================================================
// jeecs/gui.hpp —— GUI 后端对接：je_gui_backend_*（ImGui 胶水层）C API
// =============================================================================

#include "jeecs/graphic.hpp"

JE_FORCE_CAPI

/*
je_gui_backend_font_set [基本接口]
让ImGUI使用指定的路径和字体大小设置
    * 若 general_font_path 是空指针，则使用默认字体
    * 若 general_font_path 非空，则针对拉丁字符集使用 latin_font_path 指示的字体
    * 仅在 je_gui_backend_init 前调用生效
    * 此接口仅适合用于对接自定义渲染API时使用
请参见：
    je_gui_backend_init
*/
JE_API void je_gui_backend_font_set(
    /* OPTIONAL */ const char* general_font_path,
    /* OPTIONAL */ const char* latin_font_path,
    size_t size);

typedef uint64_t je_gui_user_image_handle_t;
typedef je_gui_user_image_handle_t(*je_gui_user_image_loader_t)(je_gl_context_t*, je_gl_texture_2d_t*);
typedef void  (*je_gui_user_sampler_loader_t)(je_gl_context_t*, je_gl_shader_t*);

/*
je_gui_backend_init [基本接口]
初始化ImGUI
    * 参数1 是一个指向引擎图形上下文的指针
    * 参数2 是一个回调函数指针，用于获取一个纹理对应的底层图形库句柄以供ImGUI使用
    * 参数3 是一个回调函数指针，用于应用一个指定着色器指定的采样设置
    * 此接口仅适合用于对接自定义渲染API时使用
*/
JE_API void je_gui_backend_init(
    je_gl_context_t* gl_context,
    je_gui_user_image_loader_t get_img_res,
    je_gui_user_sampler_loader_t apply_shader_sampler);

typedef void (*je_gui_platform_draw_callback_t)(void*);

/*
je_gui_backend_update [基本接口]
一帧渲染开始之后，需要调用此接口以完成ImGUI的绘制和更新操作
    * 此接口仅适合用于对接自定义渲染API时使用
*/
JE_API void je_gui_backend_update(
    je_gui_platform_draw_callback_t platform_draw_callback,
    /* OPTIONAL */ void* data);

/*
je_gui_backend_shutdown [基本接口]
图形接口关闭时，请调用此接口关闭ImGUI
    * 参数reboot用于指示当前是否正在重启图形接口，由统一基础图形接口给出
*/
JE_API void je_gui_backend_shutdown(bool reboot);

/*
je_gui_backend_shutdown_callback [基本接口]
当通过关闭窗口等，请求关闭图形接口时，可以调用此函数用于询问ImGUI注册的
退出回调，获取是否确认关闭
    * 返回 true 表示注册的回调函数不存在或建议关闭图形接口
*/
JE_API bool je_gui_backend_shutdown_callback(void);
typedef je_gui_user_image_handle_t je_GuiUserImageHandle;
typedef je_gui_user_image_loader_t je_GuiUserImageLoader;
typedef je_gui_user_sampler_loader_t je_GuiUserSamplerLoader;
typedef je_gui_platform_draw_callback_t je_GuiPlatformDrawCallback;

JE_FORCE_CAPI_END
