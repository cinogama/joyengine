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

const char* gui_api_path = "je/gui.wo";
const char* gui_api_src = R"(
// JoyEngineECS GUI API for woo.

import woo.std;

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

    enum TreeNodeAttribute
    {
        ImGuiTreeNodeFlags_None                 = 0,
        ImGuiTreeNodeFlags_Selected             = 0x0000'0001,   // Draw as selected
        ImGuiTreeNodeFlags_Framed               = 0x0000'0002,   // Draw frame with background (e.g. for CollapsingHeader)
        ImGuiTreeNodeFlags_AllowItemOverlap     = 0x0000'0004,   // Hit testing to allow subsequent widgets to overlap this one
        ImGuiTreeNodeFlags_NoTreePushOnOpen     = 0x0000'0008,   // Don't do a TreePush() when open (e.g. for CollapsingHeader) = no extra indent nor pushing on ID stack
        ImGuiTreeNodeFlags_NoAutoOpenOnLog      = 0x0000'0010,   // Don't automatically and temporarily open node when Logging is active (by default logging will automatically open tree nodes)
        ImGuiTreeNodeFlags_DefaultOpen          = 0x0000'0020,   // Default node to be open
        ImGuiTreeNodeFlags_OpenOnDoubleClick    = 0x0000'0040,   // Need double-click to open node
        ImGuiTreeNodeFlags_OpenOnArrow          = 0x0000'0080,   // Only open when clicking on the arrow part. If ImGuiTreeNodeFlags_OpenOnDoubleClick is also set, single-click arrow or double-click all box to open.
        ImGuiTreeNodeFlags_Leaf                 = 0x0000'0100,   // No collapsing, no arrow (use as a convenience for leaf nodes).
        ImGuiTreeNodeFlags_Bullet               = 0x0000'0200,   // Display a bullet instead of arrow
        ImGuiTreeNodeFlags_FramePadding         = 0x0000'0400,  // Use FramePadding (even for an unframed text node) to vertically align text baseline to regular widget height. Equivalent to calling AlignTextToFramePadding().
        ImGuiTreeNodeFlags_SpanAvailWidth       = 0x0000'0800,  // Extend hit box to the right-most edge, even if not framed. This is not the default in order to allow adding other items on the same line. In the future we may refactor the hit system to be front-to-back, allowing natural overlaps and then this can become the default.
        ImGuiTreeNodeFlags_SpanFullWidth        = 0x0000'1000,  // Extend hit box to the left-most and right-most edges (bypass the indented area).
        ImGuiTreeNodeFlags_NavLeftJumpsBackHere = 0x0000'2000,  // (WIP) Nav: left direction may move to this TreeNode() from any of its child (items submitted between TreeNode and TreePop)
        // ImGuiTreeNodeFlags_NoScrollOnOpen     = 0x0000'4000,  // FIXME: TODO: Disable automatic scroll on TreePop() if node got just open and contents is not visible
        // ImGuiTreeNodeFlags_CollapsingHeader     = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_NoAutoOpenOnLog
    };

    extern("libjoyecs", "je_gui_job_vm_handle")
    func JobID() : handle;

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
    extern("libjoyecs", "je_gui_treenodeex")
    func TreeNodeEx(var label:string, var attrib: int):bool;
    extern("libjoyecs", "je_gui_treepop")
    func TreePop():void;

    extern("libjoyecs", "je_gui_is_itemclicked")
    func IsItemClicked(): bool;
    extern("libjoyecs", "je_gui_is_itemtoggledopen")
    func IsItemToggledOpen(): bool;

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

    extern("libjoyecs", "je_gui_input_float_box")
    func InputFloat(var label: string, ref value: real) : bool;
    extern("libjoyecs", "je_gui_input_float_box")
    func InputFloat(var label: string, ref value: real, var format: string) : bool;

    extern("libjoyecs", "je_gui_input_int_box")
    func InputInt(var label: string, ref value: int) : bool;

    extern("libjoyecs", "je_gui_input_float2_box")
    func InputFloat2(var label: string, ref x: real, ref y: real) : bool;
    extern("libjoyecs", "je_gui_input_float2_box")
    func InputFloat2(var label: string, ref x: real, ref y: real, var format: string) : bool;

    extern("libjoyecs", "je_gui_input_float3_box")
    func InputFloat3(var label: string, ref x: real, ref y: real, ref z: real) : bool;
    extern("libjoyecs", "je_gui_input_float3_box")
    func InputFloat3(var label: string, ref x: real, ref y: real, ref z: real, var format: string) : bool;

    extern("libjoyecs", "je_gui_input_float4_box")
    func InputFloat4(var label: string, ref x: real, ref y: real, ref z: real, ref w: real) : bool;
    extern("libjoyecs", "je_gui_input_float4_box")
    func InputFloat4(var label: string, ref x: real, ref y: real, ref z: real, ref w: real, var format: string) : bool;

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
WO_API wo_api je_gui_push_id(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::PushID(wo_int(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_pop_id(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::PopID();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_beginpopup_contextitem(wo_vm vm, wo_value args, size_t argc)
{
    if (argc)
        return wo_ret_bool(vm, ImGui::BeginPopupContextItem(wo_string(args + 0)));
    return wo_ret_bool(vm, ImGui::BeginPopupContextItem());
}
WO_API wo_api je_gui_beginpopup_contextwindow(wo_vm vm, wo_value args, size_t argc)
{
    if (argc)
        return wo_ret_bool(vm, ImGui::BeginPopupContextWindow(wo_string(args + 0)));
    return wo_ret_bool(vm, ImGui::BeginPopupContextWindow());
}

WO_API wo_api je_gui_beginpopup(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::BeginPopup(wo_string(args + 0)));
}

WO_API wo_api je_gui_openpopup_on_item_click(wo_vm vm, wo_value args, size_t argc)
{
    if (argc == 0)
        ImGui::OpenPopupOnItemClick();
    else if (argc == 1)
    {
        if (wo_valuetype(args + 0) == WO_STRING_TYPE)
            ImGui::OpenPopupOnItemClick(wo_string(args + 0));
        else
            ImGui::OpenPopupOnItemClick(nullptr, wo_int(args + 0));
    }
    else
        ImGui::OpenPopupOnItemClick(wo_string(args + 0), wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_openpopup(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::OpenPopup(wo_string(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_endpopup(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndPopup();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begin_listbox(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::BeginListBox(wo_string(args + 0), ImVec2(wo_float(args + 1), wo_float(args + 2))));
}
WO_API wo_api je_gui_begin_selectable(wo_vm vm, wo_value args, size_t argc)
{
    if (argc == 2)
        return wo_ret_bool(vm, ImGui::Selectable(wo_string(args + 0), wo_bool(args + 1)));
    return wo_ret_bool(vm, ImGui::Selectable(wo_string(args + 0)));
}
WO_API wo_api je_gui_end_listbox(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndListBox();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_sameline(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::SameLine();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begingroup(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::BeginGroup();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_endgroup(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndGroup();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_is_itemclicked(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::IsItemClicked());
}

WO_API wo_api je_gui_is_itemtoggledopen(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::IsItemToggledOpen());
}

WO_API wo_api je_gui_treenodeex(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::TreeNodeEx(wo_string(args + 0), (ImGuiTreeNodeFlags)wo_int(args + 1)));
}

WO_API wo_api je_gui_treenode(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::TreeNode(wo_string(args + 0)));
}
WO_API wo_api je_gui_treepop(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::TreePop();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_listbox(wo_vm vm, wo_value args, size_t argc)
{
    int selected_item = argc >= 3 ? (int)wo_int(args + 2) : -1;
    int max_height_item = argc == 4 ? (int)wo_int(args + 3) : -1;

    std::vector<const char*> items(wo_lengthof(args + 1));
    for (size_t i = 0; i < items.size(); i++)
        items[i] = wo_string(wo_arr_get(args + 1, i));

    bool val_changed = ImGui::ListBox(wo_string(args + 0), &selected_item, items.data(), items.size(), max_height_item);

    if (argc >= 3)
        wo_set_int(args + 2, selected_item);

    return wo_ret_bool(vm, val_changed);
}

WO_API wo_api je_gui_listbox_withsize(wo_vm vm, wo_value args, size_t argc)
{
    //func ListBox(var label:string, var items:array<string>, ref select_item:int, var width:real, var height:real) : int;
    int origin_selected_index = wo_int(args + 2);
    int selected_index = -1;
    bool value_updated = false;
    if (ImGui::BeginListBox(wo_string(args + 0), ImVec2(wo_float(args + 3), wo_float(args + 4))))
    {
        size_t sz = (size_t)wo_lengthof(args + 1);
        for (size_t i = 0; i < sz; i++)
        {
            wo_string_t item = wo_string(wo_arr_get(args + 1, i));
            if (ImGui::Selectable(item, i == origin_selected_index))
            {
                value_updated = true;
                selected_index = i;
            }

        }
        if (!value_updated)
            selected_index = origin_selected_index;

        wo_set_int(args + 2, selected_index);
        ImGui::EndListBox();
    }
    return wo_ret_bool(vm, value_updated);
}

WO_API wo_api je_gui_job_vm_handle(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_pointer(vm, vm);
}

WO_API wo_api je_gui_begin(wo_vm vm, wo_value args, size_t argc)
{
    if (argc == 3)
    {
        bool showing = true;
        ImGui::Begin(wo_string(args), &showing, wo_int(args + 1));
        wo_set_bool(args + 2, showing);
        return wo_ret_bool(vm, showing);
    }
    if (argc == 2)
        return wo_ret_bool(vm, ImGui::Begin(wo_string(args), 0, wo_int(args + 1)));
    return wo_ret_bool(vm, ImGui::Begin(wo_string(args)));
}
WO_API wo_api je_gui_end(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::End();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_text(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::Text(wo_string(args));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_button(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::Button(wo_string(args)));
}
WO_API wo_api je_gui_begin_main_menu_bar(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::BeginMainMenuBar());
}
WO_API wo_api je_gui_menu_item(wo_vm vm, wo_value args, size_t argc)
{
    if (argc == 2)
    {
        if (wo_valuetype(args + 1) == WO_STRING_TYPE)
            return wo_ret_bool(vm, ImGui::MenuItem(wo_string(args + 0), wo_string(args + 1)));
        else
            return wo_ret_bool(vm, ImGui::MenuItem(wo_string(args + 0), 0, false, wo_bool(args + 1)));
    }
    if (argc == 3)
        return wo_ret_bool(vm, ImGui::MenuItem(wo_string(args + 0), wo_string(args + 1), false, wo_bool(args + 2)));
    if (argc == 4)
    {
        bool selected = wo_bool(args + 2);
        bool clicked = ImGui::MenuItem(wo_string(args + 0), wo_string(args + 1), &selected, wo_bool(args + 3));
        wo_set_bool(args + 2, selected);

        return wo_ret_bool(vm, clicked);
    }
    return wo_ret_bool(vm, ImGui::MenuItem(wo_string(args + 0)));
}
WO_API wo_api je_gui_end_main_menu_bar(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndMainMenuBar();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_begin_menu_bar(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::BeginMenuBar());
}
WO_API wo_api je_gui_end_menu_bar(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndMenuBar();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_begin_menu(wo_vm vm, wo_value args, size_t argc)
{
    if (argc == 2)
        return wo_ret_bool(vm, ImGui::BeginMenu(wo_string(args + 0), wo_bool(args + 1)));
    return wo_ret_bool(vm, ImGui::BeginMenu(wo_string(args + 0)));
}
WO_API wo_api je_gui_end_menu(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndMenu();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_separator(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::Separator();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_create_text_buffer(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm, new std::string(wo_string(args + 0)), nullptr, [](void* ptr) {delete (std::string*)ptr; });
}

WO_API wo_api je_gui_clear_text_buffer(wo_vm vm, wo_value args, size_t argc)
{
    std::string* str = (std::string*)wo_pointer(args + 0);
    str->clear();

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_get_text_buffer(wo_vm vm, wo_value args, size_t argc)
{
    std::string* str = (std::string*)wo_pointer(args + 0);

    return wo_ret_string(vm, str->c_str());
}

WO_API wo_api je_gui_set_text_buffer(wo_vm vm, wo_value args, size_t argc)
{
    std::string* str = (std::string*)wo_pointer(args + 0);
    *str = wo_string(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_input_text_box(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::InputText(wo_string(args + 0), (std::string*)wo_pointer(args + 1)));
}
WO_API wo_api je_gui_input_int_box(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t label = wo_string(args + 0);
    int value = (int)wo_int(args + 1);
    bool update = false;

    update = ImGui::InputInt(label, &value);

    wo_set_int(args + 1, value);
    return wo_ret_bool(vm, update);
}
WO_API wo_api je_gui_input_float_box(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t label = wo_string(args + 0);
    float value = wo_float(args + 1);
    bool update = false;

    if (argc == 2)
        update = ImGui::InputFloat(label, &value);
    else if (argc == 3)
    {
        wo_string_t format = wo_string(args + 2);
        update = ImGui::InputFloat(label, &value, 0.f, 0.f, format);
    }
    wo_set_float(args + 1, value);
    return wo_ret_bool(vm, update);
}
WO_API wo_api je_gui_input_float2_box(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2) };
    bool update = false;

    if (argc == 3)
        update = ImGui::InputFloat2(label, values);
    else if (argc == 4)
    {
        wo_string_t format = wo_string(args + 3);
        update = ImGui::InputFloat2(label, values, format);
    }
    wo_set_float(args + 1, values[0]);
    wo_set_float(args + 2, values[1]);
    return wo_ret_bool(vm, update);
}
WO_API wo_api je_gui_input_float3_box(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2), wo_float(args + 3) };
    bool update = false;

    if (argc == 4)
        update = ImGui::InputFloat3(label, values);
    else if (argc == 5)
    {
        wo_string_t format = wo_string(args + 4);
        update = ImGui::InputFloat3(label, values, format);
    }
    wo_set_float(args + 1, values[0]);
    wo_set_float(args + 2, values[1]);
    wo_set_float(args + 3, values[2]);
    return wo_ret_bool(vm, update);
}
WO_API wo_api je_gui_input_float4_box(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2), wo_float(args + 3), wo_float(args + 4) };
    bool update = false;

    if (argc == 5)
        update = ImGui::InputFloat4(label, values);
    else if (argc == 6)
    {
        wo_string_t format = wo_string(args + 5);
        update = ImGui::InputFloat4(label, values, format);
    }
    wo_set_float(args + 1, values[0]);
    wo_set_float(args + 2, values[1]);
    wo_set_float(args + 3, values[2]);
    wo_set_float(args + 4, values[3]);
    return wo_ret_bool(vm, update);
}

WO_API wo_api je_gui_input_text_multiline(wo_vm vm, wo_value args, size_t argc)
{
    if (argc == 4)
        return wo_ret_bool(vm, ImGui::InputTextMultiline(wo_string(args + 0), (std::string*)wo_pointer(args + 1),
            ImVec2(wo_float(args + 2), wo_float(args + 3))));
    return wo_ret_bool(vm, ImGui::InputTextMultiline(wo_string(args + 0), (std::string*)wo_pointer(args + 1)));
}

struct gui_wo_job_coroutine
{
    wo_vm work_vm;
    wo_integer_t job_handle;

    gui_wo_job_coroutine* last;
};
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_job_list;
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_new_job_list;

WO_API wo_api je_gui_launch(wo_vm vm, wo_value args, size_t argc)
{
    wo_integer_t loopfunc = wo_int(args + 0);
    wo_integer_t jobfunc = wo_int(args + 1);

    wo_vm vmm = wo_sub_vm(vm, 1024);

    for (size_t i = argc - 1; i > 1; --i)
        wo_push_valref(vmm, args + i);
    wo_push_int(vmm, jobfunc);

    wo_dispatch_rsfunc(vmm, loopfunc, argc - 1);

    gui_wo_job_coroutine* guico = new gui_wo_job_coroutine;
    guico->work_vm = vmm;
    guico->job_handle = jobfunc;
    _wo_new_job_list.add_one(guico);

    return wo_ret_void(vm);
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

    auto* ttf_file = jeecs_file_open((wo_exe_path() + std::string("builtin/cino_ipix_12px.ttf")).c_str());
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

    auto chain = _wo_job_list.pick_all();
    while (chain)
    {
        auto cur_job = chain;
        chain = chain->last;

        auto result = wo_dispatch(cur_job->work_vm);
        if (result == WO_CONTINUE)
        {
            _wo_job_list.add_one(cur_job);
            continue;
        }

        wo_close_vm(cur_job->work_vm);
        delete cur_job;
    }

    auto new_job_chain = _wo_new_job_list.pick_all();
    while (new_job_chain)
    {
        auto cur_job = new_job_chain;
        new_job_chain = new_job_chain->last;

        _wo_job_list.add_one(cur_job);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void jegui_shutdown()
{
    auto chain = _wo_job_list.pick_all();
    while (chain)
    {
        auto cur_job = chain;
        chain = chain->last;

        wo_close_vm(cur_job->work_vm);
        delete cur_job;
    }

    auto new_job_chain = _wo_new_job_list.pick_all();
    while (new_job_chain)
    {
        auto cur_job = new_job_chain;
        new_job_chain = new_job_chain->last;

        wo_close_vm(cur_job->work_vm);
        delete cur_job;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}