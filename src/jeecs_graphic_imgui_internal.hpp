#pragma once

// ImGui 模块内部共享状态：声明于此处，定义位于
// jeecs_graphic_imgui_basic_impl.cpp（后端实现），
// 由后端函数与 Woolang 绑定（jeecs_gui_woolang_api.cpp）共同读写。
// Must be included after the public gui/graphic headers with JE_IMPL defined.

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/basic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/gui.hpp"
#include "jeecs/prefix.hpp"

#include <imgui.h>

#include <optional>
#include <string>
#include <mutex>
#include <unordered_map>

namespace jeecs::graphic::gui
{
    struct KeyState
    {
        bool m_last_frame_down;
        bool m_this_frame_down;
    };
    struct WoolangJobCoroutine
    {
        std::optional<woort_value> function_to_spawn;
        woort_vm* work_vm;

        WoolangJobCoroutine* last;
    };
    struct GUIThreadContext
    {
        je_gl_context_t* _jegl_context = nullptr;
        bool _jegui_stop_work_flag = false;

        je_gui_user_image_loader_t _jegl_get_native_texture = nullptr;
        je_gui_user_sampler_loader_t _jegl_bind_shader_sampler_state = nullptr;

        std::string _jegui_imgui_config_path;
        std::optional<basic::resource<graphic::shader>>
            _jegl_rend_texture_shader;

        // 默认帧缓冲不支持硬件 sRGB 时的全屏编码资源。着色器通过
        // graphic::shader::load 加载，由引擎翻译为各后端代码。
        std::optional<basic::resource<graphic::shader>>
            _jegl_srgb_encode_shader;
        std::optional<basic::resource<graphic::vertex>>
            _jegl_srgb_encode_quad;
        je_gl_pipeline_t* _jegl_srgb_encode_pipeline = nullptr;
        je_gl_command_buffer_t* _jegl_srgb_encode_cmd = nullptr;
    };
    struct GUIContext
    {
        woort_vm* _jegui_exit_callback_handler_vm = nullptr;
        woort_GCPin* _jegui_exit_callback_function = nullptr;
        std::optional<std::string> _jegui_specify_font_path = std::nullopt;
        std::optional<std::string> _jegui_specify_latin_font_path = std::nullopt;
        size_t _jegui_specify_font_size = 18;
    };

    extern GUIContext _je_gui_static_ctx;
    extern thread_local GUIThreadContext _je_gui_tls_ctx;

    extern std::mutex _key_state_record_mx;
    extern std::unordered_map<ImGuiKey, KeyState> _key_state_record;

    extern basic::atomic_list<WoolangJobCoroutine> _wo_job_list;
    extern basic::atomic_list<WoolangJobCoroutine> _wo_new_job_list;
}
