#define JE_IMPL
#include "jeecs.hpp"
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

const char* gui_api_path = "je/gui.rsn";
const char* gui_api_src = R"(
// JoyEngineECS GUI API for rscene.

import rscene.std;

namespace je
    namespace gui
{
    extern("libjoyecs", "je_gui_begin")
    func Begin(var title:string) : bool;

    extern("libjoyecs", "je_gui_end")
    func End() : bool;

    extern("libjoyecs", "je_gui_text")
    func Text(var msg:string) : void;

    extern("libjoyecs", "je_gui_button")
    func Button(var msg:string) : void;

    extern("libjoyecs", "je_gui_begin_main_menu_bar")
    func BeginMainMenuBar() : bool;

    extern("libjoyecs", "je_gui_end_menu_item")
    func MenuItem(var text:string) : void;

    extern("libjoyecs", "je_gui_end_main_menu_bar")
    func EndMainMenuBar() : void;

    extern("libjoyecs", "je_gui_begin_menu_bar")
    func BeginMenuBar() : bool;

    extern("libjoyecs", "je_gui_end_menu_bar")
    func EndMenuBar() : void;

    extern("libjoyecs", "je_gui_begin_menu")
    func BeginMenu(var text:string) : bool;

    extern("libjoyecs", "je_gui_end_menu")
    func EndMenu() : void;

    using job_handle_t = gchandle;

    extern("libjoyecs", "je_gui_launch")
    private func _launch<LT, FT>(var coloop:LT, var job_func:FT, ...) : job_handle_t;

    func launch<FT>(var job_func:FT, ...) : job_handle_t
    {
        var _loop_job = func(var job_func:FT, ...)
                        {
                            var result = nil:dynamic;
                            while (!result)
                            {
                                result = job_func(......);
                                std::break_yield();
                            }
                        };
        return _launch(_loop_job, job_func, ......);
    }
}

)";

RS_API rs_api je_gui_begin(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::Begin(rs_string(args)));
}
RS_API rs_api je_gui_end(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::End();
    return rs_ret_nil(vm);
}
RS_API rs_api je_gui_text(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::Text(rs_string(args));
    return rs_ret_nil(vm);
}
RS_API rs_api je_gui_button(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::Button(rs_string(args)));
}
RS_API rs_api je_gui_begin_main_menu_bar(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::BeginMainMenuBar());
}
RS_API rs_api je_gui_end_menu_item(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::MenuItem(rs_string(args)));
}
RS_API rs_api je_gui_end_main_menu_bar(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::EndMainMenuBar();
    return rs_ret_nil(vm);
}
RS_API rs_api je_gui_begin_menu_bar(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::BeginMenuBar());
}
RS_API rs_api je_gui_end_menu_bar(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::EndMenuBar();
    return rs_ret_nil(vm);
}
RS_API rs_api je_gui_begin_menu(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::BeginMenu(rs_string(args + 0)));
}
RS_API rs_api je_gui_end_menu(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::EndMenu();
    return rs_ret_nil(vm);
}

struct gui_rs_job_coroutine
{
    rs_vm work_vm;

    gui_rs_job_coroutine* last;
};
jeecs::basic::atomic_list<gui_rs_job_coroutine> _rs_job_list;

RS_API rs_api je_gui_launch(rs_vm vm, rs_value args, size_t argc)
{
    rs_integer_t loopfunc = rs_int(args + 0);
    rs_integer_t jobfunc = rs_int(args + 1);

    rs_vm vmm = rs_sub_vm(vm, 1024);

    for (size_t i = argc - 1; i > 1; --i)
        rs_push_valref(vmm, args + i);
    rs_push_int(vmm, jobfunc);

    rs_dispatch_rsfunc(vmm, loopfunc, argc - 1);

    gui_rs_job_coroutine* guico = new gui_rs_job_coroutine;
    guico->work_vm = vmm;
    _rs_job_list.add_one(guico);

    return rs_ret_nil(vm);
}

void jegui_init(void* window_handle)
{
    ImGui::CreateContext();

    // Set style:
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors = style->Colors;
    colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.46f, 0.54f, 0.80f, 0.60f);
    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.39f, 0.39f, 0.39f, 0.62f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.14f, 0.44f, 0.80f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.14f, 0.44f, 0.80f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.35f, 0.35f, 0.35f, 0.17f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.90f);
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_TabActive] = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
    colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.78f, 0.87f, 0.98f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.57f, 0.57f, 0.64f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.68f, 0.68f, 0.74f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_NavHighlight] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    ImGuiIO& io = ImGui::GetIO();

    auto* ttf_file = jeecs_file_open((rs_exe_path() + std::string("builtin/cino_ipix_12px.ttf")).c_str());
    if (ttf_file)
    {
        auto* file_buf = je_mem_alloc(ttf_file->m_file_length);
        jeecs_file_read(file_buf, sizeof(char), ttf_file->m_file_length, ttf_file);

        io.Fonts->AddFontFromMemoryTTF(file_buf, ttf_file->m_file_length, 18, nullptr,
            io.Fonts->GetGlyphRangesChineseFull());

        // je_mem_free(file_buf); // No need to free.
        jeecs_file_close(ttf_file);
    }

    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)window_handle, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void jegui_update()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto chain = _rs_job_list.pick_all();
    while (chain)
    {
        auto cur_job = chain;
        chain = chain->last;

        auto result = rs_dispatch(cur_job->work_vm);
        if (result == RS_CONTINUE)
            _rs_job_list.add_one(cur_job);
        else
        {
            rs_close_vm(cur_job->work_vm);
            delete cur_job;
        }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void jegui_shutdown()
{
    auto chain = _rs_job_list.pick_all();
    while (chain)
    {
        auto cur_job = chain;
        chain = chain->last;

        rs_close_vm(cur_job->work_vm);
        delete cur_job;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}