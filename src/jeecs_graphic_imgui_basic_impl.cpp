#define JE_IMPL
#include "jeecs/basic.hpp"
#include "jeecs/file.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/gui.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_graphic_render_internal.hpp"

#include "jeecs_graphic_imgui_backend_api.hpp"

#include <string>
#include <unordered_set>
#include <optional>
#include <list>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <imgui_node_editor.h>

// Text editor support:
#include <TextEditor.h>

#include "jeecs_graphic_imgui_internal.hpp"

namespace jeecs::graphic::gui {}

using namespace std;
using namespace jeecs;
using namespace jeecs::graphic;
using namespace jeecs::graphic::gui;

namespace jeecs::graphic::gui
{
    // 状态结构体与 extern 声明见 jeecs_graphic_imgui_internal.hpp；
    // 此处为全模块共享的唯一实例定义（后端与 Woolang 绑定共用）。
    GUIContext _je_gui_static_ctx;
    thread_local GUIThreadContext _je_gui_tls_ctx;

    mutex _key_state_record_mx;
    unordered_map<ImGuiKey, KeyState> _key_state_record;

    basic::atomic_list<WoolangJobCoroutine> _wo_job_list;
    basic::atomic_list<WoolangJobCoroutine> _wo_new_job_list;
}

void je_gui_backend_font_set(
    /* OPTIONAL */ const char* general_font_path,
    /* OPTIONAL */ const char* latin_font_path,
    size_t size)
{
    _je_gui_static_ctx._jegui_specify_font_path = general_font_path ? optional(general_font_path) : nullopt;
    _je_gui_static_ctx._jegui_specify_latin_font_path = latin_font_path ? optional(latin_font_path) : nullopt;

    _je_gui_static_ctx._jegui_specify_font_size = size;
}


////////////////////////////////////////////////////////////////////////////////////////////

void je_gui_backend_init(
    je_gl_context_t* gl_context,
    je_gui_user_image_loader_t get_img_res,
    je_gui_user_sampler_loader_t apply_shader_sampler)
{
    _je_gui_tls_ctx._jegl_context = gl_context;
    _je_gui_tls_ctx._jegl_get_native_texture = get_img_res;
    _je_gui_tls_ctx._jegl_bind_shader_sampler_state = apply_shader_sampler;

    _je_gui_tls_ctx._jegl_rend_texture_shader =
        graphic::shader::load(
            gl_context,
            "!/builtin/shader/internal/imgui_image_displayer.shader")
        .value();

    _je_gui_tls_ctx._jegui_imgui_config_path =
        je_file_host_path_get() + string("/builtin/imgui.ini.je4cache");

    _je_gui_tls_ctx._jegui_stop_work_flag = false;
    ImGui::CreateContext();

    // Set style:
    // ImGui::StyleColorsLight();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#ifndef JE_GL_USE_EGL_INSTEAD_GLFW
    // 仅普通窗口模式下启用多视口支持
    if (gl_context->m_config.m_display_mode == JE_GL_DISPLAY_MODE_WINDOWED)
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif

    io.IniFilename = _je_gui_tls_ctx._jegui_imgui_config_path.c_str();

    auto* general_ttf_file =
        _je_gui_static_ctx._jegui_specify_font_path.has_value()
        ? je_file_open(_je_gui_static_ctx._jegui_specify_font_path.value().c_str())
        : nullptr;
    auto* latin_ttf_file =
        _je_gui_static_ctx._jegui_specify_latin_font_path.has_value()
        ? je_file_open(_je_gui_static_ctx._jegui_specify_latin_font_path.value().c_str())
        : nullptr;

    if (general_ttf_file == nullptr)
        // Default font
        general_ttf_file = je_file_open("!/builtin/font/HarmonyOS_Sans_SC_Regular.ttf");

    if (latin_ttf_file == nullptr)
        // Default font
        latin_ttf_file = je_file_open("!/builtin/font/JetBrainsMono_Regular.ttf");

    if (general_ttf_file)
    {
        const size_t general_ttf_size = je_file_size(general_ttf_file);
        const size_t latin_ttf_size
            = latin_ttf_file != nullptr ? je_file_size(latin_ttf_file) : 0;

        void* file_buf = malloc(general_ttf_size);
        je_file_read(file_buf, sizeof(char), general_ttf_size, general_ttf_file);

        void* latin_file_buf = nullptr;
        if (latin_ttf_file != nullptr)
        {
            latin_file_buf = malloc(latin_ttf_size);
            je_file_read(latin_file_buf, sizeof(char), latin_ttf_size, latin_ttf_file);
        }

        if (latin_file_buf != nullptr)
        {
            // Latin 字体（西文与符号）：默认范围只有 Latin-1，箭头、
            // 数学运算符、勾叉等符号块必须显式列出，否则界面显示方框
            static const ImWchar latin_ranges[] =
            {
                0x0020,
                0x00FF, // Basic Latin + Latin Supplement
                0x2190,
                0x21FF, // Arrows (→ ← ↑ ↓ ↔ ...)
                0x2200,
                0x22FF, // Mathematical Operators (√ ≤ ≥ ≈ ...)
                0x2700,
                0x27BF, // Dingbats (✓ ✗ ...)
                0,
            };

            // Non latin characters
            static const ImWchar ranges[] =
            {
                // 0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x2000,
                0x206F, // General Punctuation
                0x2460,
                0x24FF, // Enclosed Alphanumerics (① ② ...)
                0x3000,
                0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
                0x31F0,
                0x31FF, // Katakana Phonetic Extensions
                0xFF00,
                0xFFEF, // Half-width characters
                0xFFFD,
                0xFFFD, // Invalid
                0x4e00,
                0x9FAF, // CJK Ideograms
                0,
            };

            io.Fonts->AddFontFromMemoryTTF(
                latin_file_buf,
                (int)latin_ttf_size,
                (float)_je_gui_static_ctx._jegui_specify_font_size,
                nullptr,
                latin_ranges);

            ImFontConfig merge_config;
            merge_config.MergeMode = true;

            io.Fonts->AddFontFromMemoryTTF(
                file_buf,
                (int)general_ttf_size,
                (float)_je_gui_static_ctx._jegui_specify_font_size,
                &merge_config,
                ranges);

            io.Fonts->Build();
        }
        else
            io.Fonts->AddFontFromMemoryTTF(
                file_buf,
                (int)general_ttf_size,
                (float)_je_gui_static_ctx._jegui_specify_font_size,
                nullptr,
                io.Fonts->GetGlyphRangesChineseFull());

        // free(file_buf); // No need to free.
        je_file_close(general_ttf_file);
    }
}

void jegui_commit_srgb_encode_pass(je_gl_texture_2d_t* source_texture)
{
    if (source_texture == nullptr)
        return;

    if (!_je_gui_tls_ctx._jegl_srgb_encode_shader.has_value())
    {
        auto shader = graphic::shader::load(
            _je_gui_tls_ctx._jegl_context,
            "!/builtin/shader/internal/srgb_encode.shader");
        if (!shader.has_value())
        {
            debug::logfatal(
                "Failed to load the sRGB encode shader: %s",
                "!/builtin/shader/internal/srgb_encode.shader");
            abort();
        }
        _je_gui_tls_ctx._jegl_srgb_encode_shader = shader.value();
    }

    // pipeline 创建需要 shader 的反射信息，必须等资源准备完成。
    if (_je_gui_tls_ctx._jegl_srgb_encode_pipeline == nullptr)
    {
        if (prepare_shader_resource(
                _je_gui_tls_ctx._jegl_srgb_encode_shader.value()->resource())
            == ResourcePrepareResult::INVALID)
        {
            debug::logfatal(
                "Failed to prepare the sRGB encode shader.");
            abort();
        }

        static const float quad_data[] = {
            /* vpos, vuv */
            -1.f, -1.f, 0.f,  0.f, 0.f,
             1.f, -1.f, 0.f,  1.f, 0.f,
            -1.f,  1.f, 0.f,  0.f, 1.f,
             1.f,  1.f, 0.f,  1.f, 1.f,
        };
        auto quad = graphic::vertex::create(
            JE_GL_VERTEX_TYPE_TRIANGLESTRIP,
            quad_data,
            sizeof(quad_data),
            { 0, 1, 2, 3 },
            {
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3 },
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2 },
            });
        if (!quad.has_value())
        {
            debug::logfatal(
                "Failed to create the sRGB encode full-screen quad.");
            abort();
        }
        _je_gui_tls_ctx._jegl_srgb_encode_quad = quad.value();

        je_gl_pipeline_state_t state = je_graphic_pipeline_state_from_shader(
            _je_gui_tls_ctx._jegl_srgb_encode_shader.value()->resource());
        state.m_topology = je_gl_topology_t::TRIANGLE_STRIP;
        state.m_depth_test = false;

        je_gl_pipeline_desc_t desc{};
        desc.m_shader =
            _je_gui_tls_ctx._jegl_srgb_encode_shader.value()->resource();
        desc.m_state = &state;

        _je_gui_tls_ctx._jegl_srgb_encode_pipeline =
            je_graphic_pipeline_create(
                _je_gui_tls_ctx._jegl_context, &desc);
    }

    auto* cmd = _je_gui_tls_ctx._jegl_srgb_encode_cmd;
    if (cmd == nullptr)
        cmd = _je_gui_tls_ctx._jegl_srgb_encode_cmd =
            je_graphic_cmd_create(_je_gui_tls_ctx._jegl_context);
    je_graphic_cmd_reset(cmd);

    const je_gl_viewport_t viewport{
        0, 0,
        (uint32_t)source_texture->m_width,
        (uint32_t)source_texture->m_height };

    // begin_pass(nullptr) 由后端在编码标志置位时解析为真正的屏幕缓冲区。
    je_graphic_cmd_begin_pass(cmd, nullptr, &viewport);
    je_graphic_cmd_bind_pipeline(
        cmd, _je_gui_tls_ctx._jegl_srgb_encode_pipeline);
    je_graphic_cmd_bind_vertex(
        cmd,
        _je_gui_tls_ctx._jegl_srgb_encode_quad.value()->resource());
    je_graphic_cmd_bind_texture_2d(cmd, 0, source_texture);
    je_graphic_cmd_draw(cmd, 4, 0, 1);
    je_graphic_cmd_end_pass(cmd);
    je_graphic_cmd_commit(cmd);
}

// These backends present through either a hardware sRGB target or a final
// full-screen sRGB encode pass, so linear vertex colors are always expected.
#if defined(JE_ENABLE_DX11_GAPI) || defined(JE_ENABLE_VK120_GAPI) \
    || defined(JE_ENABLE_METAL_GAPI) || defined(JE_ENABLE_GL330_GAPI) \
    || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
#define JEGUI_LINEARIZE_VERTEX_COLORS 1
#endif

#ifdef JEGUI_LINEARIZE_VERTEX_COLORS
namespace
{
    // ImGui colors are authored in sRGB. The swapchain is an sRGB target and
    // the scene shaders already output linear, so linearize ImGui's vertex
    // colors before uploading them: the framebuffer then re-encodes them.
    void _jegui_linearize_draw_data(ImDrawData* draw_data)
    {
        for (int n = 0; n < draw_data->CmdListsCount; ++n)
        {
            ImDrawList* list = draw_data->CmdLists[n];
            ImDrawVert* verts = list->VtxBuffer.Data;

            for (int i = 0; i < list->VtxBuffer.Size; ++i)
            {
                const ImU32 col = verts[i].col;
                const float r = (float)(col & 0xFF) / 255.f;
                const float g = (float)((col >> 8) & 0xFF) / 255.f;
                const float b = (float)((col >> 16) & 0xFF) / 255.f;
                const float a = (float)((col >> 24) & 0xFF) / 255.f;

                verts[i].col = IM_COL32(
                    (int)(math::srgb_to_linear(r) * 255.f + 0.5f),
                    (int)(math::srgb_to_linear(g) * 255.f + 0.5f),
                    (int)(math::srgb_to_linear(b) * 255.f + 0.5f),
                    (int)(a * 255.f + 0.5f));
            }
        }
    }

    using _jegui_render_window_func_t = void (*)(ImGuiViewport*, void*);
    _jegui_render_window_func_t _g_original_render_window = nullptr;

    void _jegui_linearized_render_window(
        ImGuiViewport* viewport, void* render_arg)
    {
        _jegui_linearize_draw_data(viewport->DrawData);
        _g_original_render_window(viewport, render_arg);
    }

    void _jegui_install_linearize_hooks()
    {
        ImGuiPlatformIO& io = ImGui::GetPlatformIO();
        if (io.Renderer_RenderWindow != nullptr
            && io.Renderer_RenderWindow != _jegui_linearized_render_window)
        {
            _g_original_render_window = io.Renderer_RenderWindow;
            io.Renderer_RenderWindow = _jegui_linearized_render_window;
        }
    }
}
#endif

void je_gui_backend_update(
    je_gui_platform_draw_callback_t platform_draw_callback,
    /* OPTIONAL */ void* data)
{
    je_gl_shader_t* imgui_shader =
        _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource();
    const auto prepare_result = prepare_shader_resource(imgui_shader);
    assert(prepare_result != ResourcePrepareResult::INVALID);
    (void)prepare_result;

    ImGui::NewFrame();
    auto* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Main DockSpace", NULL, host_window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    do
    {
        lock_guard g1(_key_state_record_mx);
        for (auto& [k, v] : _key_state_record)
        {
            v.m_last_frame_down = v.m_this_frame_down;
            v.m_this_frame_down = ImGui::IsKeyDown(k);
        }
    } while (0);

    if (!_je_gui_tls_ctx._jegui_stop_work_flag)
    {
        auto chain = _wo_job_list.pick_all();
        while (chain)
        {
            auto cur_job = chain;
            chain = chain->last;

            woort_VmCallStatus result;

            woort_VMRuntime* prev = woort_vm_swap(cur_job->work_vm);
            {
                if (cur_job->function_to_spawn.has_value())
                {
                    result = woort_spawn(WOORT_IGNORE, cur_job->function_to_spawn.value());
                    cur_job->function_to_spawn.reset();
                }
                else
                    result = woort_resume(WOORT_IGNORE);
            }
            (void)woort_vm_swap(prev);
            if (result == WOORT_VM_CALL_STATUS_YIELD)
            {
                _wo_job_list.add_one(cur_job);
                continue;
            }

            woort_vm_close(cur_job->work_vm);
            delete cur_job;
        }

        auto new_job_chain = _wo_new_job_list.pick_all();
        while (new_job_chain)
        {
            auto cur_job = new_job_chain;
            new_job_chain = new_job_chain->last;

            _wo_job_list.add_one(cur_job);
        }
    }

    ImGui::Render();

#ifdef JEGUI_LINEARIZE_VERTEX_COLORS
    _jegui_install_linearize_hooks();
    _jegui_linearize_draw_data(ImGui::GetDrawData());
#endif

    platform_draw_callback(data);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
void je_gui_backend_shutdown(bool reboot)
{
    if (!reboot)
    {
        auto chain = _wo_job_list.pick_all();
        while (chain)
        {
            auto cur_job = chain;
            chain = chain->last;

            woort_vm_close(cur_job->work_vm);
            delete cur_job;
        }

        auto new_job_chain = _wo_new_job_list.pick_all();
        while (new_job_chain)
        {
            auto cur_job = new_job_chain;
            new_job_chain = new_job_chain->last;

            woort_vm_close(cur_job->work_vm);
            delete cur_job;
        }
    }
}

bool je_gui_backend_shutdown_callback()
{
#ifndef NDEBUG
    debug::loginfo("Graphic interface has been requested to close.");
#endif

    if (_je_gui_static_ctx._jegui_exit_callback_handler_vm == nullptr)
        return true;

    bool close = false;
    woort_vm* const last = woort_vm_swap(_je_gui_static_ctx._jegui_exit_callback_handler_vm);
    {
        woort_value tmp;
        if (!woort_push_reserve(1, &tmp))
            woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow.");
        else
        {
            woort_GCPin_get(tmp, _je_gui_static_ctx._jegui_exit_callback_function, 0);
            if (WOORT_VM_CALL_STATUS_NORMAL == woort_invoke(tmp, tmp))
                close = woort_bool(tmp);
        }
    }
    (void)woort_vm_swap(last);

    return close;
}
