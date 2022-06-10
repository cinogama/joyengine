#define JE_IMPL
#include "jeecs.hpp"
#include <string>
#include <unordered_set>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

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
    enum WindowsAttribute
    {
        ImGuiWindowFlags_None                   = 0,
        ImGuiWindowFlags_NoTitleBar             = 0x0000'0001,   // Disable title-bar
        ImGuiWindowFlags_NoResize               = 0x0000'0002,   // Disable user resizing with the lower-right grip
        ImGuiWindowFlags_NoMove                 = 0x0000'0004,   // Disable user moving the window
        ImGuiWindowFlags_NoScrollbar            = 0x0000'0008,   // Disable scrollbars (window can still scroll with mouse or programmatically)
        ImGuiWindowFlags_NoScrollWithMouse      = 0x0000'0010,   // Disable user vertically scrolling with mouse wheel. On child window, mouse wheel will be forwarded to the parent unless NoScrollbar is also set.
        ImGuiWindowFlags_NoCollapse             = 0x0000'0020,   // Disable user collapsing window by double-clicking on it. Also referred to as Window Menu Button (e.g. within a docking node).
        ImGuiWindowFlags_AlwaysAutoResize       = 0x0000'0040,   // Resize every window to its content every frame
        ImGuiWindowFlags_NoBackground           = 0x0000'0080,   // Disable drawing background color (WindowBg, etc.) and outside border. Similar as using SetNextWindowBgAlpha(0.0f).
        ImGuiWindowFlags_NoSavedSettings        = 0x0000'0100,   // Never load/save settings in .ini file
        ImGuiWindowFlags_NoMouseInputs          = 0x0000'0200,   // Disable catching mouse, hovering test with pass through.
        ImGuiWindowFlags_MenuBar                = 0x0000'0400,  // Has a menu-bar
        ImGuiWindowFlags_HorizontalScrollbar    = 0x0000'0800,  // Allow horizontal scrollbar to appear (off by default). You may use SetNextWindowContentSize(ImVec2(width,0.0f)); prior to calling Begin() to specify width. Read code in imgui_demo in the "Horizontal Scrolling" section.
        ImGuiWindowFlags_NoFocusOnAppearing     = 0x0000'1000,  // Disable taking focus when transitioning from hidden to visible state
        ImGuiWindowFlags_NoBringToFrontOnFocus  = 0x0000'2000,  // Disable bringing window to front when taking focus (e.g. clicking on it or programmatically giving it focus)
        ImGuiWindowFlags_AlwaysVerticalScrollbar= 0x0000'4000,  // Always show vertical scrollbar (even if ContentSize.y < Size.y)
        ImGuiWindowFlags_AlwaysHorizontalScrollbar=0x0000'8000,  // Always show horizontal scrollbar (even if ContentSize.x < Size.x)
        ImGuiWindowFlags_AlwaysUseWindowPadding = 0x0001'0000,  // Ensure child windows without border uses style.WindowPadding (ignored by default for non-bordered child windows, because more convenient)
        ImGuiWindowFlags_NoNavInputs            = 0x0002'0000,  // No gamepad/keyboard navigation within the window
        ImGuiWindowFlags_NoNavFocus             = 0x0004'0000,  // No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by CTRL+TAB)
        ImGuiWindowFlags_UnsavedDocument        = 0x0008'0000,  // Display a dot next to the title. When used in a tab/docking context, tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.
        //ImGuiWindowFlags_NoNav                  = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus,
        //ImGuiWindowFlags_NoDecoration           = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse,
        //ImGuiWindowFlags_NoInputs               = ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus,
    }

    extern("libjoyecs", "je_gui_begin")
    func Begin(var title:string) : bool;
    extern("libjoyecs", "je_gui_begin")
    func Begin(var title:string, var attribute:int) : bool;
    extern("libjoyecs", "je_gui_begin")
    func Begin(var title:string, var attribute:int, ref closed:bool) : bool;

    extern("libjoyecs", "je_gui_end")
    func End() : bool;

    extern("libjoyecs", "je_gui_text")
    func Text(var msg:string) : void;

    extern("libjoyecs", "je_gui_button")
    func Button(var msg:string) : void;

    extern("libjoyecs", "je_gui_begin_main_menu_bar")
    func BeginMainMenuBar() : bool;

    extern("libjoyecs", "je_gui_menu_item")
    func MenuItem(var text:string) : bool;
    extern("libjoyecs", "je_gui_menu_item")
    func MenuItem(var text:string, var shortcut:string) : bool;
    extern("libjoyecs", "je_gui_menu_item")
    func MenuItem(var text:string, var enable:bool) : bool;
    extern("libjoyecs", "je_gui_menu_item")
    func MenuItem(var text:string, var shortcut:string, var enable:bool) : bool;
    extern("libjoyecs", "je_gui_menu_item")
    func MenuItem(var text:string, var shortcut:string, ref selected:bool, var enable:bool) : bool;
    func MenuItem(var text:string, ref selected:bool, var enable:bool) : bool
    {
        return MenuItem(text, "", ref selected, enable);
    }

    extern("libjoyecs", "je_gui_end_main_menu_bar")
    func EndMainMenuBar() : void;

    extern("libjoyecs", "je_gui_begin_menu_bar")
    func BeginMenuBar() : bool;

    extern("libjoyecs", "je_gui_end_menu_bar")
    func EndMenuBar() : void;

    extern("libjoyecs", "je_gui_begin_menu")
    func BeginMenu(var text:string) : bool;
    extern("libjoyecs", "je_gui_begin_menu")
    func BeginMenu(var text:string, var enable:bool) : bool;

    extern("libjoyecs", "je_gui_listbox")
    func ListBox(var label:string, var items:array<string>) : int;
    extern("libjoyecs", "je_gui_listbox")
    func ListBox(var label:string, var items:array<string>, ref select_item:int) : int;
    extern("libjoyecs", "je_gui_listbox")
    func ListBox(var label:string, var items:array<string>, ref select_item:int, var height_count:int) : int;
    extern("libjoyecs", "je_gui_listbox_withsize")
    func ListBox(var label:string, var items:array<string>, ref select_item:int, var width:real, var height:real) : int;

    extern("libjoyecs", "je_gui_begin_listbox")
    func BeginListBox(var label:string, var width:real, var height:real):bool;
    extern("libjoyecs", "je_gui_begin_selectable")
    func Selectable(var label:string):bool;
    extern("libjoyecs", "je_gui_begin_selectable")
    func Selectable(var label:string, var selected:bool):bool;
    extern("libjoyecs", "je_gui_end_listbox")
    func EndListBox() : void;

    extern("libjoyecs", "je_gui_sameline")
    func SameLine():void;

    extern("libjoyecs", "je_gui_begingroup")
    func BeginGroup():void;
    extern("libjoyecs", "je_gui_endgroup")
    func EndGroup():void;

    extern("libjoyecs", "je_gui_treenode")
    func TreeNode(var label:string):bool;
    extern("libjoyecs", "je_gui_treepop")
    func TreePop():void;

    extern("libjoyecs", "je_gui_beginpopup_contextitem")
    func BeginPopupContextItem(var label:string):bool;
    extern("libjoyecs", "je_gui_beginpopup_contextitem")
    func BeginPopupContextItem():bool;

    extern("libjoyecs", "je_gui_beginpopup_contextwindow")
    func BeginPopupContextWindow(var label:string):bool;
    extern("libjoyecs", "je_gui_beginpopup_contextwindow")
    func BeginPopupContextWindow():bool;

    enum PopupAttribute
    {
        NONE
    }

    extern("libjoyecs", "je_gui_openpopup_on_item_click")
    func OpenPopupOnItemClick():void;
    extern("libjoyecs", "je_gui_openpopup_on_item_click")
    func OpenPopupOnItemClick(var flag:PopupAttribute):void;
    extern("libjoyecs", "je_gui_openpopup_on_item_click")
    func OpenPopupOnItemClick(var label:string):void;
    extern("libjoyecs", "je_gui_openpopup_on_item_click")
    func OpenPopupOnItemClick(var label:string, var flag:PopupAttribute):void;

    extern("libjoyecs", "je_gui_openpopup")
    func OpenPopup(var label:string):void;     

    extern("libjoyecs", "je_gui_beginpopup")
    func BeginPopup(var label:string):bool;

    extern("libjoyecs", "je_gui_endpopup")
    func EndPopup():void;

    extern("libjoyecs", "je_gui_push_id")
    func PushID(var id:int):void;

    extern("libjoyecs", "je_gui_pop_id")
    func PopID():void;

    using TextBuffer = gchandle;
    namespace TextBuffer
    {
        extern("libjoyecs", "je_gui_create_text_buffer")
        func create(var text:string):TextBuffer;

        extern("libjoyecs", "je_gui_clear_text_buffer")
        func clear(var self:TextBuffer):void;
        
        extern("libjoyecs", "je_gui_get_text_buffer")
        func get(var self:TextBuffer):string;

        extern("libjoyecs", "je_gui_set_text_buffer")
        func set(var self:TextBuffer, var text:string):void;
    }

    extern("libjoyecs", "je_gui_input_text_box")
    func InputText(var label:string, var buffer:TextBuffer) : bool;

    extern("libjoyecs", "je_gui_input_text_multiline")
    func InputTextMultiline(var label:string, var buffer:TextBuffer) : bool;
    extern("libjoyecs", "je_gui_input_text_multiline")
    func InputTextMultiline(var label:string, var buffer:TextBuffer, var width:real, var height:real) : bool;

    extern("libjoyecs", "je_gui_end_menu")
    func EndMenu() : void;

    extern("libjoyecs", "je_gui_separator")
    func Separator() : void;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    using job_handle_t = gchandle;

    extern("libjoyecs", "je_gui_launch")
    private func _launch<LT, FT>(var coloop:LT, var job_func:FT, ...) : job_handle_t;

    private func dialog(var job_func:dynamic, ...) : job_handle_t
    {
        var result = nil:dynamic;
        while (!result)
        {
            result = (job_func:dynamic(...))(......);
            if (result)
            {
                // If function return something, handle them here
                /* Do something... */
            }
            std::break_yield();
        }
    }

    func launch<FT>(var job_func:FT, ...) : job_handle_t
    {
        return _launch(dialog, job_func, ......);
    }
}

)";

//
RS_API rs_api je_gui_push_id(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::PushID(rs_int(args + 0));
    return rs_ret_nil(vm);
}
RS_API rs_api je_gui_pop_id(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::PopID();
    return rs_ret_nil(vm);
}
RS_API rs_api je_gui_beginpopup_contextitem(rs_vm vm, rs_value args, size_t argc)
{
    if (argc)
        return rs_ret_bool(vm, ImGui::BeginPopupContextItem(rs_string(args + 0)));
    return rs_ret_bool(vm, ImGui::BeginPopupContextItem());
}
RS_API rs_api je_gui_beginpopup_contextwindow(rs_vm vm, rs_value args, size_t argc)
{
    if (argc)
        return rs_ret_bool(vm, ImGui::BeginPopupContextWindow(rs_string(args + 0)));
    return rs_ret_bool(vm, ImGui::BeginPopupContextWindow());
}

RS_API rs_api je_gui_beginpopup(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::BeginPopup(rs_string(args + 0)));
}

RS_API rs_api je_gui_openpopup_on_item_click(rs_vm vm, rs_value args, size_t argc)
{
    if (argc == 0)
        ImGui::OpenPopupOnItemClick();
    else if (argc == 1)
    {
        if (rs_valuetype(args + 0) == RS_STRING_TYPE)
            ImGui::OpenPopupOnItemClick(rs_string(args + 0));
        else
            ImGui::OpenPopupOnItemClick(nullptr, rs_int(args + 0));
    }
    else
        ImGui::OpenPopupOnItemClick(rs_string(args + 0), rs_int(args + 1));
    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_openpopup(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::OpenPopup(rs_string(args + 0));
    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_endpopup(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::EndPopup();
    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_begin_listbox(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::BeginListBox(rs_string(args + 0), ImVec2(rs_float(args + 1), rs_float(args + 2))));
}
RS_API rs_api je_gui_begin_selectable(rs_vm vm, rs_value args, size_t argc)
{
    if (argc == 2)
        return rs_ret_bool(vm, ImGui::Selectable(rs_string(args + 0), rs_bool(args + 1)));
    return rs_ret_bool(vm, ImGui::Selectable(rs_string(args + 0)));
}
RS_API rs_api je_gui_end_listbox(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::EndListBox();
    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_sameline(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::SameLine();
    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_begingroup(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::BeginGroup();
    return rs_ret_nil(vm);
}
RS_API rs_api je_gui_endgroup(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::EndGroup();
    return rs_ret_nil(vm);
}


RS_API rs_api je_gui_treenode(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::TreeNode(rs_string(args + 0)));
}
RS_API rs_api je_gui_treepop(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::TreePop();
    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_listbox(rs_vm vm, rs_value args, size_t argc)
{
    int selected_item = argc >= 3 ? (int)rs_int(args + 2) : -1;
    int max_height_item = argc == 4 ? (int)rs_int(args + 3) : -1;

    std::vector<const char*> items(rs_lengthof(args + 1));
    for (size_t i = 0; i < items.size(); i++)
        items[i] = rs_string(rs_arr_get(args + 1, i));

    bool val_changed = ImGui::ListBox(rs_string(args + 0), &selected_item, items.data(), items.size(), max_height_item);

    if (argc >= 3)
        rs_set_int(args + 2, selected_item);

    return rs_ret_bool(vm, val_changed);
}

RS_API rs_api je_gui_listbox_withsize(rs_vm vm, rs_value args, size_t argc)
{
    //func ListBox(var label:string, var items:array<string>, ref select_item:int, var width:real, var height:real) : int;
    int origin_selected_index = rs_int(args + 2);
    int selected_index = -1;
    bool value_updated = false;
    if (ImGui::BeginListBox(rs_string(args + 0), ImVec2(rs_float(args + 3), rs_float(args + 4))))
    {
        size_t sz = (size_t)rs_lengthof(args + 1);
        for (size_t i = 0; i < sz; i++)
        {
            rs_string_t item = rs_string(rs_arr_get(args + 1, i));
            if (ImGui::Selectable(item, i == origin_selected_index))
            {
                value_updated = true;
                selected_index = i;
            }

        }
        if (!value_updated)
            selected_index = origin_selected_index;

        rs_set_int(args + 2, selected_index);
        ImGui::EndListBox();
    }
    return rs_ret_bool(vm, value_updated);
}

RS_API rs_api je_gui_begin(rs_vm vm, rs_value args, size_t argc)
{
    if (argc == 3)
    {
        bool showing = true;
        ImGui::Begin(rs_string(args), &showing, rs_int(args + 1));
        rs_set_bool(args + 2, showing);
        return rs_ret_bool(vm, showing);
    }
    if (argc == 2)
        return rs_ret_bool(vm, ImGui::Begin(rs_string(args), 0, rs_int(args + 1)));
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
RS_API rs_api je_gui_menu_item(rs_vm vm, rs_value args, size_t argc)
{
    if (argc == 2)
    {
        if (rs_valuetype(args + 1) == RS_STRING_TYPE)
            return rs_ret_bool(vm, ImGui::MenuItem(rs_string(args + 0), rs_string(args + 1)));
        else
            return rs_ret_bool(vm, ImGui::MenuItem(rs_string(args + 0), 0, false, rs_bool(args + 1)));
    }
    if (argc == 3)
        return rs_ret_bool(vm, ImGui::MenuItem(rs_string(args + 0), rs_string(args + 1), false, rs_bool(args + 2)));
    if (argc == 4)
    {
        bool selected = rs_bool(args + 2);
        bool clicked = ImGui::MenuItem(rs_string(args + 0), rs_string(args + 1), &selected, rs_bool(args + 3));
        rs_set_bool(args + 2, selected);

        return rs_ret_bool(vm, clicked);
    }
    return rs_ret_bool(vm, ImGui::MenuItem(rs_string(args + 0)));
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
    if (argc == 2)
        return rs_ret_bool(vm, ImGui::BeginMenu(rs_string(args + 0), rs_bool(args + 1)));
    return rs_ret_bool(vm, ImGui::BeginMenu(rs_string(args + 0)));
}
RS_API rs_api je_gui_end_menu(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::EndMenu();
    return rs_ret_nil(vm);
}
RS_API rs_api je_gui_separator(rs_vm vm, rs_value args, size_t argc)
{
    ImGui::Separator();
    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_create_text_buffer(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm, new std::string(rs_string(args + 0)), nullptr, [](void* ptr) {delete (std::string*)ptr; });
}

RS_API rs_api je_gui_clear_text_buffer(rs_vm vm, rs_value args, size_t argc)
{
    std::string* str = (std::string*)rs_pointer(args + 0);
    str->clear();

    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_get_text_buffer(rs_vm vm, rs_value args, size_t argc)
{
    std::string* str = (std::string*)rs_pointer(args + 0);

    return rs_ret_string(vm, str->c_str());
}

RS_API rs_api je_gui_set_text_buffer(rs_vm vm, rs_value args, size_t argc)
{
    std::string* str = (std::string*)rs_pointer(args + 0);
    *str = rs_string(args + 1);
    return rs_ret_nil(vm);
}

RS_API rs_api je_gui_input_text_box(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, ImGui::InputText(rs_string(args + 0), (std::string*)rs_pointer(args + 1)));
}

RS_API rs_api je_gui_input_text_multiline(rs_vm vm, rs_value args, size_t argc)
{
    if (argc == 4)
        return rs_ret_bool(vm, ImGui::InputTextMultiline(rs_string(args + 0), (std::string*)rs_pointer(args + 1),
            ImVec2(rs_float(args + 2), rs_float(args + 3))));
    return rs_ret_bool(vm, ImGui::InputTextMultiline(rs_string(args + 0), (std::string*)rs_pointer(args + 1)));
}

struct gui_rs_job_coroutine
{
    rs_vm work_vm;
    rs_integer_t job_handle;

    gui_rs_job_coroutine* last;
};
jeecs::basic::atomic_list<gui_rs_job_coroutine> _rs_job_list;
jeecs::basic::atomic_list<gui_rs_job_coroutine> _rs_new_job_list;

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
    guico->job_handle = jobfunc;
    _rs_new_job_list.add_one(guico);

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

        io.Fonts->AddFontFromMemoryTTF(file_buf, ttf_file->m_file_length, 16, nullptr,
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
    std::unordered_set<rs_integer_t> _displayed_job_handle;

    while (chain)
    {
        auto cur_job = chain;
        chain = chain->last;

        if (_displayed_job_handle.find(cur_job->job_handle) == _displayed_job_handle.end())
        {
            _displayed_job_handle.insert(cur_job->job_handle);
            auto result = rs_dispatch(cur_job->work_vm);
            if (result == RS_CONTINUE)
            {
                _rs_job_list.add_one(cur_job);
                continue;
            }
        }

        rs_close_vm(cur_job->work_vm);
        delete cur_job;
    }

    auto new_job_chain = _rs_new_job_list.pick_all();
    while (new_job_chain)
    {
        auto cur_job = new_job_chain;
        new_job_chain = new_job_chain->last;

        if (_displayed_job_handle.find(cur_job->job_handle) == _displayed_job_handle.end())
        {
            _rs_job_list.add_one(cur_job);
            continue;
        }
        rs_close_vm(cur_job->work_vm);
        delete cur_job;
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

    auto new_job_chain = _rs_new_job_list.pick_all();
    while (new_job_chain)
    {
        auto cur_job = new_job_chain;
        new_job_chain = new_job_chain->last;

        rs_close_vm(cur_job->work_vm);
        delete cur_job;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}