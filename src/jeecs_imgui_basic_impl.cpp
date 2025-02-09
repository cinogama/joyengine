#define JE_IMPL
#include "jeecs.hpp"
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

const char* gui_api_path = "je/gui.wo";
const char* gui_api_src = { R"(
// JoyEngineECS GUI API for woo.

import woo::std;
import je;

namespace je::gui
{    
    public alias ImVec2 = (real, real);
    public alias Color32RGBA = (int, int, int, int);

    public enum WindowsAttribute
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
        
        ImGuiWindowFlags_NoNavInputs            = 0x0004'0000,  // No gamepad/keyboard navigation within the window
        ImGuiWindowFlags_NoNavFocus             = 0x0008'0000,  // No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by CTRL+TAB)
        ImGuiWindowFlags_UnsavedDocument        = 0x0010'0000,  // Display a dot next to the title. When used in a tab/docking context, tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.
        ImGuiWindowFlags_NoDocking              = 0x0020'0000,  // Disable docking of this window
        //ImGuiWindowFlags_NoNav                  = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus,
        //ImGuiWindowFlags_NoDecoration           = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse,
        //ImGuiWindowFlags_NoInputs               = ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus,
    }

    public enum TreeNodeAttribute
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

    public enum ImGuiMouseButton
    {
        ImGuiMouseButton_Left = 0,
        ImGuiMouseButton_Right = 1,
        ImGuiMouseButton_Middle = 2,
        ImGuiMouseButton_COUNT = 5
    };

    extern("libjoyecs", "je_gui_job_vm_handle")
    public func JobID()=> handle;

    extern("libjoyecs", "je_gui_set_clip_board_text")
    public func SetClipboardText(text: string)=> void;
    extern("libjoyecs", "je_gui_get_clip_board_text")
    public func GetClipboardText()=> string;

    extern("libjoyecs", "je_gui_get_window_pos")
    public func GetWindowPos()=> ImVec2;
    extern("libjoyecs", "je_gui_get_mouse_pos")
    public func GetMousePos()=> ImVec2;
    extern("libjoyecs", "je_gui_get_mouse_delta_pos")
    public func GetMouseDeltaPos()=> ImVec2;
    extern("libjoyecs", "je_gui_get_cursor_pos")
    public func GetCursorPos()=> ImVec2;
    extern("libjoyecs", "je_gui_get_item_rect_size")
    public func GetItemRectSize()=> ImVec2;

    extern("libjoyecs", "je_gui_get_item_rect")
    public func GetItemRect()=> (ImVec2, ImVec2);

    extern("libjoyecs", "je_gui_begin")
    public func Begin(title:string)=> bool;
    extern("libjoyecs", "je_gui_begin_attr")
    public func BeginAttr(title:string, attribute:WindowsAttribute)=> bool;
    extern("libjoyecs", "je_gui_begin_open")
    public func BeginAttrOpen(title:string, attribute:WindowsAttribute)=> option<bool>;
    public func BeginOpen(title:string)
    {
        return BeginAttrOpen(title, WindowsAttribute::ImGuiWindowFlags_None);
    }

    extern("libjoyecs", "je_gui_end")
    public func End()=> void;

    extern("libjoyecs", "je_gui_begin_child")
    public func BeginChild(title:string)=> bool;
    extern("libjoyecs", "je_gui_begin_child_attr")
    public func BeginChildAttr(title:string, attr: WindowsAttribute)=> bool;
    extern("libjoyecs", "je_gui_begin_child_size")
    public func BeginChildSize(title:string, sizex: real, sizey: real)=> bool;

    extern("libjoyecs", "je_gui_is_window_focused")
    public func IsWindowFocused()=> bool;

    extern("libjoyecs", "je_gui_end_child")
    public func EndChild()=> void;

    extern("libjoyecs", "je_gui_progress_bar")
    public func ProgressBar(f: real)=> void;

    extern("libjoyecs", "je_gui_progress_bar_size")
    public func ProgressBarSize(f: real, size: (real, real))=> void;

    extern("libjoyecs", "je_gui_text")
    public func Text(msg:string)=> void;
    extern("libjoyecs", "je_gui_text_disabled")
    public func TextDisabled(msg:string)=> void;

    extern("libjoyecs", "je_gui_button")
    public func Button(msg:string)=> bool;
    extern("libjoyecs", "je_gui_button_size")
    public func ButtonSize(msg:string, size: ImVec2)=> bool;

    extern("libjoyecs", "je_gui_invisible_button")
    public func InvisibleButton(msg:string, size: ImVec2)=> bool;

    extern("libjoyecs", "je_gui_begin_main_menu_bar")
    public func BeginMainMenuBar()=> bool;

    extern("libjoyecs", "je_gui_menu_item")
    public func MenuItem(text:string)=> bool;
    extern("libjoyecs", "je_gui_menu_item_shortcut")
    public func MenuItemShortcut(text:string, shortcut:string)=> bool;
    extern("libjoyecs", "je_gui_menu_item_enabled")
    public func MenuItemEnabled(text:string, enable:bool)=> bool;
    extern("libjoyecs", "je_gui_menu_item_shortcut_enabled")
    public func MenuItemShortcutEnabled(text:string, shortcut:string, enable:bool)=> bool;
    extern("libjoyecs", "je_gui_menu_item_selected")
    public func MenuItemShortcutSelectedEnabled(text:string, shortcut:string, selected:bool, enable:bool)=> option<bool /*selected*/>;
    public func MenuItemSelectedEnabled(text:string, selected:bool, enable:bool)=> option<bool /*selected*/>
    {
        return MenuItemShortcutSelectedEnabled(text, "", selected, enable);
    }
    public func MenuItemSelected(text:string, selected:bool)=> option<bool /*selected*/>
    {
        return MenuItemShortcutSelectedEnabled(text, "", selected, true);
    }
)"
R"(
    extern("libjoyecs", "je_gui_end_main_menu_bar")
    public func EndMainMenuBar()=> void;

    extern("libjoyecs", "je_gui_begin_menu_bar")
    public func BeginMenuBar()=> bool;

    extern("libjoyecs", "je_gui_end_menu_bar")
    public func EndMenuBar()=> void;

    extern("libjoyecs", "je_gui_begin_menu")
    public func BeginMenu(text:string)=> bool;
    extern("libjoyecs", "je_gui_begin_menu_enabled")
    public func BeginMenuEnabled(text:string, enable:bool)=> bool;

    extern("libjoyecs", "je_gui_listbox")
    public func ListBox(label:string, items:array<string>)=> option<int>;
    extern("libjoyecs", "je_gui_listbox_select")
    public func ListBoxSelect(label:string, items:array<string>, select_item:int)=> option<int>;
    extern("libjoyecs", "je_gui_listbox_select_height")
    public func ListBoxSelectHeight(label:string, items:array<string>, select_item:int, height_count:int)=> option<int>;
    extern("libjoyecs", "je_gui_listbox_withsize")
    public func ListBoxSelectSize(label:string, items:array<string>, select_item:int, width:real, height:real)=> option<int>;

    extern("libjoyecs", "je_gui_begin_listbox")
    public func BeginListBox(label:string, width:real, height:real)=>bool;
    extern("libjoyecs", "je_gui_begin_selectable")
    public func Selectable(label:string)=>bool;
    extern("libjoyecs", "je_gui_begin_selectable_selected")
    public func SelectableSelected(label:string, selected:bool)=>bool;
    extern("libjoyecs", "je_gui_end_listbox")
    public func EndListBox()=> void;

    extern("libjoyecs", "je_gui_sameline")
    public func SameLine()=>void;

    extern("libjoyecs", "je_gui_begingroup")
    public func BeginGroup()=>void;
    extern("libjoyecs", "je_gui_endgroup")
    public func EndGroup()=>void;

    extern("libjoyecs", "je_gui_treenode")
    public func TreeNode(label:string)=>bool;
    extern("libjoyecs", "je_gui_treenodeex")
    public func TreeNodeEx(label:string, attrib: TreeNodeAttribute)=>bool;
    extern("libjoyecs", "je_gui_treepop")
    public func TreePop()=>void;

    extern("libjoyecs", "je_gui_is_itemclicked")
    public func IsItemClicked(button: ImGuiMouseButton)=> bool;

    public func IsItemClickedLeft()
    {
        return IsItemClicked(ImGuiMouseButton::ImGuiMouseButton_Left);
    }

    extern("libjoyecs", "je_gui_is_itemtoggledopen")
    public func IsItemToggledOpen()=> bool;
    extern("libjoyecs", "je_gui_is_itemhovered")
    public func IsItemHovered()=> bool;
    extern("libjoyecs", "je_gui_is_mousehoveringrect")
    public func IsMouseHoveringRect(p0: (real, real), p1: (real, real))=> bool;

    extern("libjoyecs", "je_gui_is_item_active")
    public func IsItemActive()=> bool;

    extern("libjoyecs", "je_gui_is_mouse_dragging")
    public func IsMouseDragging(attr: ImGuiMouseButton)=> bool;
    extern("libjoyecs", "je_gui_is_mouse_down")
    public func IsMouseDown(attr: ImGuiMouseButton)=> bool;
    extern("libjoyecs", "je_gui_is_mouse_released")
    public func IsMouseReleased(attr: ImGuiMouseButton)=> bool;

    extern("libjoyecs", "je_gui_set_tooltip")
    public func SetTooltip(msg: string)=> bool;

    extern("libjoyecs", "je_gui_beginpopup_contextitem_label")
    public func BeginPopupContextItemLabel(label:string)=>bool;
    extern("libjoyecs", "je_gui_beginpopup_contextitem")
    public func BeginPopupContextItem()=>bool;

    extern("libjoyecs", "je_gui_beginpopup_contextwindow_label")
    public func BeginPopupContextWindowLabel(label:string)=>bool;
    extern("libjoyecs", "je_gui_beginpopup_contextwindow")
    public func BeginPopupContextWindow()=>bool;

    public enum TabBarAttribute
    {
        ImGuiTabBarFlags_None                           = 0,
        ImGuiTabBarFlags_Reorderable                    = 1,   // Allow manually dragging tabs to re-order them + New tabs are appended at the end of list
        ImGuiTabBarFlags_AutoSelectNewTabs              = 2,   // Automatically select new tabs when they appear
        ImGuiTabBarFlags_TabListPopupButton             = 4,   // Disable buttons to open the tab list popup
        ImGuiTabBarFlags_NoCloseWithMiddleMouseButton   = 8,   // Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You can still repro this behavior on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false.
        ImGuiTabBarFlags_NoTabListScrollingButtons      = 16,   // Disable scrolling buttons (apply when fitting policy is ImGuiTabBarFlags_FittingPolicyScroll)
        ImGuiTabBarFlags_NoTooltip                      = 32,   // Disable tooltips when hovering a tab
        ImGuiTabBarFlags_FittingPolicyResizeDown        = 64,   // Resize tabs when they don't fit
        ImGuiTabBarFlags_FittingPolicyScroll            = 128,   // Add scroll buttons when tabs don't fit
        // ImGuiTabBarFlags_FittingPolicyMask_             = ImGuiTabBarFlags_FittingPolicyResizeDown | ImGuiTabBarFlags_FittingPolicyScroll,
        // ImGuiTabBarFlags_FittingPolicyDefault_          = ImGuiTabBarFlags_FittingPolicyResizeDown
    }

    public enum TabItemAttribute
    {
        ImGuiTabItemFlags_None                          = 0,
        ImGuiTabItemFlags_UnsavedDocument               = 1,   // Display a dot next to the title + tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.
        ImGuiTabItemFlags_SetSelected                   = 2,   // Trigger flag to programmatically make the tab selected when calling BeginTabItem()
        ImGuiTabItemFlags_NoCloseWithMiddleMouseButton  = 4,   // Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You can still repro this behavior on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false.
        ImGuiTabItemFlags_NoPushId                      = 8,   // Don't call PushID(tab->ID)/PopID() on BeginTabItem()/EndTabItem()
        ImGuiTabItemFlags_NoTooltip                     = 16,   // Disable tooltip for the given tab
        ImGuiTabItemFlags_NoReorder                     = 32,   // Disable reordering this tab or having another tab cross over this tab
        ImGuiTabItemFlags_Leading                       = 64,   // Enforce the tab position to the left of the tab bar (after the tab list popup button)
        ImGuiTabItemFlags_Trailing                      = 128    // Enforce the tab position to the right of the tab bar (before the scrolling buttons)
    }

    extern("libjoyecs", "je_gui_begintabbar")
    public func BeginTabBarAttr(label: string, attr: TabBarAttribute)=> bool;
    public func BeginTabBar(label: string)
    {
        return BeginTabBarAttr(label, TabBarAttribute::ImGuiTabBarFlags_None);
    } 
    extern("libjoyecs", "je_gui_endtabbar")
    public func EndTabBar()=> void;

    extern("libjoyecs", "je_gui_begintabitem")
    public func BeginTabItemAttr(label: string, attr: TabItemAttribute)=> bool;
    extern("libjoyecs", "je_gui_begintabitem_open")
    public func BeginTabItemAttrOpen(label: string, attr: TabItemAttribute)=> (bool /* show */, bool /* open */);

    public func BeginTabItem(label: string)
    {
        return BeginTabItemAttr(label, TabItemAttribute::ImGuiTabItemFlags_None);
    }
    public func BeginTabItemOpen(label: string)
    {
        return BeginTabItemAttrOpen(label, TabItemAttribute::ImGuiTabItemFlags_None);
    }

    extern("libjoyecs", "je_gui_endtabitem")
    public func EndTabItem()=> void;
    
    public enum PopupAttribute
    {
        NONE
    }

    extern("libjoyecs", "je_gui_openpopup_on_item_click")
    public func OpenPopupOnItemClick()=>void;
    extern("libjoyecs", "je_gui_openpopup_on_item_click_attr")
    public func OpenPopupOnItemClickAttr(flag:PopupAttribute)=>void;
    extern("libjoyecs", "je_gui_openpopup_on_item_click_label")
    public func OpenPopupOnItemClickLabel(label:string)=>void;
    extern("libjoyecs", "je_gui_openpopup_on_item_click_label_attr")
    public func OpenPopupOnItemClickLabelAttr(label:string, flag:PopupAttribute)=>void;

    extern("libjoyecs", "je_gui_openpopup")
    public func OpenPopup(label:string)=>void;     

    extern("libjoyecs", "je_gui_beginpopup")
    public func BeginPopup(label:string)=>bool;

    extern("libjoyecs", "je_gui_endpopup")
    public func EndPopup()=>void;

    extern("libjoyecs", "je_gui_push_id")
    public func PushIDInt(id:int)=>void;

    extern("libjoyecs", "je_gui_push_id_str")
    public func PushIDStr(id:string)=>void;

    extern("libjoyecs", "je_gui_pop_id")
    public func PopID()=>void;

    extern("libjoyecs", "je_gui_get_id")
    public func GetID(name: string)=>int;

    extern("libjoyecs", "je_gui_checkbox")
    public func CheckBox(label: string, checked: bool)=> option<bool>;

    extern("libjoyecs", "je_gui_colorpicker4")
    public func ColorPicker4(label:string, color: (real, real, real, real))=> option<(real, real, real, real)>;

    extern("libjoyecs", "je_gui_colorbutton")
    public func ColorButton(label:string, color: (real, real, real, real))=> bool;

    public enum ImGuiInputTextFlags
    {
        ImGuiInputTextFlags_None                = 0,
        ImGuiInputTextFlags_CharsDecimal        = 0x00000001,   // Allow 0123456789.+-*/
        ImGuiInputTextFlags_CharsHexadecimal    = 0x00000002,   // Allow 0123456789ABCDEFabcdef
        ImGuiInputTextFlags_CharsUppercase      = 0x00000004,   // Turn a..z into A..Z
        ImGuiInputTextFlags_CharsNoBlank        = 0x00000008,   // Filter out spaces, tabs
        ImGuiInputTextFlags_AutoSelectAll       = 0x00000010,   // Select entire text when first taking mouse focus
        ImGuiInputTextFlags_EnterReturnsTrue    = 0x00000020,   // Return 'true' when Enter is pressed (as opposed to every time the value was modified). Consider looking at the IsItemDeactivatedAfterEdit() function.
        ImGuiInputTextFlags_CallbackCompletion  = 0x00000040,   // Callback on pressing TAB (for completion handling)
        ImGuiInputTextFlags_CallbackHistory     = 0x00000080,   // Callback on pressing Up/Down arrows (for history handling)
        ImGuiInputTextFlags_CallbackAlways      = 0x00000100,   // Callback on each iteration. User code may query cursor position, modify text buffer.
        ImGuiInputTextFlags_CallbackCharFilter  = 0x00000200,   // Callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.
        ImGuiInputTextFlags_AllowTabInput       = 0x00000400,  // Pressing TAB input a '\t' character into the text field
        ImGuiInputTextFlags_CtrlEnterForNewLine = 0x00000800,  // In multi-line mode, unfocus with Enter, add new line with Ctrl+Enter (default is opposite: unfocus with Ctrl+Enter, add line with Enter).
        ImGuiInputTextFlags_NoHorizontalScroll  = 0x00001000,  // Disable following the cursor horizontally
        ImGuiInputTextFlags_AlwaysOverwrite     = 0x00002000,  // Overwrite mode
        ImGuiInputTextFlags_ReadOnly            = 0x00004000,  // Read-only mode
        ImGuiInputTextFlags_Password            = 0x00008000,  // Password mode, display all characters as '*'
        ImGuiInputTextFlags_NoUndoRedo          = 0x00010000,  // Disable undo/redo. Note that input text owns the text data while active, if you want to provide your own undo/redo stack you need e.g. to call ClearActiveID().
        ImGuiInputTextFlags_CharsScientific     = 0x00020000,  // Allow 0123456789.+-*/eE (Scientific notation input)
        ImGuiInputTextFlags_CallbackResize      = 0x00040000,  // Callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow. Notify when the string wants to be resized (for string types which hold a cache of their Size). You will be provided a new BufSize in the callback and NEED to honor it. (see misc/cpp/imgui_stdlib.h for an example of using this)
        ImGuiInputTextFlags_CallbackEdit        = 0x00080000,  // Callback on any edit (note that InputText() already returns true on edit, the callback is useful mainly to manipulate the underlying buffer while focus is active)
        ImGuiInputTextFlags_EscapeClearsAll     = 0x00100000,  // Escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)

        // Obsolete names
        //ImGuiInputTextFlags_AlwaysInsertMode  = ImGuiInputTextFlags_AlwaysOverwrite   // [renamed in 1.82] name was not matching behavior
    };

    extern("libjoyecs", "je_gui_input_text_box")
    public func InputText(label:string, buffer: string)=> option<string>;

    extern("libjoyecs", "je_gui_input_text_attr_box")
    public func InputTextAttr(label:string, buffer: string, flag: ImGuiInputTextFlags)=> option<string>;

    extern("libjoyecs", "je_gui_drag_float")
    public func DragFloat(label: string, value: real, step: real, min: real, max: real)=> option<real>;

    extern("libjoyecs", "je_gui_input_float_box")
    public func InputFloat(label: string, value: real)=> option<real>;
    extern("libjoyecs", "je_gui_input_float_format_box")
    public func InputFloatFormat(label: string, value: real, format: string)=> option<real>;

    extern("libjoyecs", "je_gui_input_int_box")
    public func InputInt(label: string, value: int)=> option<int>;

    extern("libjoyecs", "je_gui_input_int2_box")
    public func InputInt2(label: string, x: int, y: int)=> option<(int, int)>;

    extern("libjoyecs", "je_gui_input_int3_box")
    public func InputInt3(label: string, x: int, y: int, z: int)=> option<(int, int, int)>;

    extern("libjoyecs", "je_gui_input_int4_box")
    public func InputInt4(label: string, x: int, y: int, z: int, w: int)=> option<(int, int, int, int)>;

    extern("libjoyecs", "je_gui_input_float2_box")
    public func InputFloat2(label: string, x: real, y: real)=> option<(real, real)>;
    extern("libjoyecs", "je_gui_input_float2_format_box")
    public func InputFloat2Format(label: string, x: real, y: real, format: string)=> option<(real, real)>;

    extern("libjoyecs", "je_gui_input_float3_box")
    public func InputFloat3(label: string, x: real, y: real, z: real)=> option<(real, real, real)>;
    extern("libjoyecs", "je_gui_input_float3_format_box")
    public func InputFloat3Format(label: string, x: real, y: real, z: real, format: string)=> option<(real, real, real)>;

    extern("libjoyecs", "je_gui_input_float4_box")
    public func InputFloat4(label: string, x: real, y: real, z: real, w: real)=> option<(real, real, real, real)>;
    extern("libjoyecs", "je_gui_input_float4_format_box")
    public func InputFloat4Format(label: string, x: real, y: real, z: real, w: real, format: string)=> option<(real, real, real, real)>;

    extern("libjoyecs", "je_gui_input_text_multiline")
    public func InputTextMultiline(label:string, buffer: string)=> option<string>;
    extern("libjoyecs", "je_gui_input_text_multiline_size")
    public func InputTextMultilineSize(label:string, buffer: string, width:real, height:real)=> option<string>;

    extern("libjoyecs", "je_gui_combo")
    public func Combo(label:string, items: array<string>, select_item: int)=> option<int>;

    extern("libjoyecs", "je_gui_end_menu")
    public func EndMenu()=> void;

    extern("libjoyecs", "je_gui_separator")
    public func Separator()=> void;

    extern("libjoyecs", "je_gui_image")
    public func Image(tex: je::graphic::texture)=> void;
    extern("libjoyecs", "je_gui_image_scale")
    public func ImageScale(tex: je::graphic::texture, scale: real)=> void;
    extern("libjoyecs", "je_gui_image_size")
    public func ImageSize(tex: je::graphic::texture, width: real, height: real)=> void;
    extern("libjoyecs", "je_gui_image_size_color")
    public func ImageSizeColor(tex: je::graphic::texture, width: real, height: real, color: Color32RGBA)=> void;

    extern("libjoyecs", "je_gui_push_item_width")
    public func PushItemWidth(width: real)=> void;

    extern("libjoyecs", "je_gui_pop_item_width")
    public func PopItemWidth()=> void;
)" R"(
    extern("libjoyecs", "je_gui_push_clip_rect")
    public func PushClipRect(from: ImVec2, to: ImVec2)=> void;

    extern("libjoyecs", "je_gui_pop_clip_rect")
    public func PopClipRect()=> void;

    public enum ImGuiCol
    {
        ImGuiCol_Text,
        ImGuiCol_TextDisabled,
        ImGuiCol_WindowBg,              // Background of normal windows
        ImGuiCol_ChildBg,               // Background of child windows
        ImGuiCol_PopupBg,               // Background of popups, menus, tooltips windows
        ImGuiCol_Border,
        ImGuiCol_BorderShadow,
        ImGuiCol_FrameBg,               // Background of checkbox, radio button, plot, slider, text input
        ImGuiCol_FrameBgHovered,
        ImGuiCol_FrameBgActive,
        ImGuiCol_TitleBg,
        ImGuiCol_TitleBgActive,
        ImGuiCol_TitleBgCollapsed,
        ImGuiCol_MenuBarBg,
        ImGuiCol_ScrollbarBg,
        ImGuiCol_ScrollbarGrab,
        ImGuiCol_ScrollbarGrabHovered,
        ImGuiCol_ScrollbarGrabActive,
        ImGuiCol_CheckMark,
        ImGuiCol_SliderGrab,
        ImGuiCol_SliderGrabActive,
        ImGuiCol_Button,
        ImGuiCol_ButtonHovered,
        ImGuiCol_ButtonActive,
        ImGuiCol_Header,                // Header* colors are used for CollapsingHeader, TreeNode, Selectable, MenuItem
        ImGuiCol_HeaderHovered,
        ImGuiCol_HeaderActive,
        ImGuiCol_Separator,
        ImGuiCol_SeparatorHovered,
        ImGuiCol_SeparatorActive,
        ImGuiCol_ResizeGrip,            // Resize grip in lower-right and lower-left corners of windows.
        ImGuiCol_ResizeGripHovered,
        ImGuiCol_ResizeGripActive,
        ImGuiCol_Tab,                   // TabItem in a TabBar
        ImGuiCol_TabHovered,
        ImGuiCol_TabActive,
        ImGuiCol_TabUnfocused,
        ImGuiCol_TabUnfocusedActive,
        ImGuiCol_DockingPreview,        // Preview overlay color when about to docking something
        ImGuiCol_DockingEmptyBg,        // Background color for empty node (e.g. CentralNode with no window docked into it)
        ImGuiCol_PlotLines,
        ImGuiCol_PlotLinesHovered,
        ImGuiCol_PlotHistogram,
        ImGuiCol_PlotHistogramHovered,
        ImGuiCol_TableHeaderBg,         // Table header background
        ImGuiCol_TableBorderStrong,     // Table outer and header borders (prefer using Alpha=1.0 here)
        ImGuiCol_TableBorderLight,      // Table inner borders (prefer using Alpha=1.0 here)
        ImGuiCol_TableRowBg,            // Table row background (even rows)
        ImGuiCol_TableRowBgAlt,         // Table row background (odd rows)
        ImGuiCol_TextSelectedBg,
        ImGuiCol_DragDropTarget,        // Rectangle highlighting a drop target
        ImGuiCol_NavHighlight,          // Gamepad/keyboard: current highlighted item
        ImGuiCol_NavWindowingHighlight, // Highlight window when using CTRL+TAB
        ImGuiCol_NavWindowingDimBg,     // Darken/colorize entire screen behind the CTRL+TAB window list, when active
        ImGuiCol_ModalWindowDimBg,      // Darken/colorize entire screen behind a modal window, when one is active
        ImGuiCol_COUNT
    };

    extern("libjoyecs", "je_gui_push_style_color")
    public func PushStyleColor(item: ImGuiCol, col: Color32RGBA)=> void;

    extern("libjoyecs", "je_gui_pop_style_color")
    public func PopStyleColor(n: int)=> void;

    using DrawListT = handle
    {
        extern("libjoyecs", "je_gui_draw_list_add_text")
        public func AddText(self: DrawListT, pos: ImVec2, color: Color32RGBA, text: string)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_image")
        public func AddImage(self: DrawListT, from: ImVec2, to: ImVec2, tex: graphic::texture, color: Color32RGBA)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_line")
        public func AddLine(self: DrawListT, from: ImVec2, to: ImVec2, color: Color32RGBA, board: real)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_rect")
        public func AddRect(self: DrawListT, from: ImVec2, to: ImVec2, color: Color32RGBA, board: real)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_rect_filled")
        public func AddRectFilled(self: DrawListT, from: ImVec2, to: ImVec2, color: Color32RGBA)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_triangle")
        public func AddTriangle(self: DrawListT, p1: ImVec2, p2: ImVec2, p3: ImVec2, color: Color32RGBA, board: real)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_triangle_filled")
        public func AddTriangleFilled(self: DrawListT, p1: ImVec2, p2: ImVec2, p3: ImVec2, color: Color32RGBA)=> void;
        
        extern("libjoyecs", "je_gui_draw_list_add_circle")
        public func AddCircle(self: DrawListT, center: ImVec2, r: real, color: Color32RGBA, board: real)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_filled_circle")
        public func AddCircleFilled(self: DrawListT, center: ImVec2, r: real, color: Color32RGBA)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_bezier_quad")
        public func AddBezierQuadratic(self: DrawListT, p1: ImVec2, p2: ImVec2, p3: ImVec2, color: Color32RGBA, board: real)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_bezier_cubic")
        public func AddBezierCubic(self: DrawListT, p1: ImVec2, p2: ImVec2, p3: ImVec2, p4: ImVec2, color: Color32RGBA, board: real)=> void;
    }
    extern("libjoyecs", "je_gui_get_window_draw_list")
    public func GetWindowDrawList()=> DrawListT;

    extern("libjoyecs", "je_gui_imagebutton")
    public func ImageButton(label: string, tex: je::graphic::texture)=> bool;
    extern("libjoyecs", "je_gui_imagebutton_scale")
    public func ImageButtonScale(label: string, tex: je::graphic::texture, scale: real)=> bool;
    extern("libjoyecs", "je_gui_imagebutton_size")
    public func ImageButtonSize(label: string, tex: je::graphic::texture, width: real, height: real)=> bool;

    extern("libjoyecs", "je_gui_content_region_avail")
    public func GetContentRegionAvail()=> ImVec2;

    extern("libjoyecs", "je_gui_set_next_window_size_constraints")
    public func SetNextWindowSizeConstraints(minsz: ImVec2, maxsz: ImVec2)=> void;

    extern("libjoyecs", "je_gui_set_next_window_size")
    public func SetNextWindowSize(sz: ImVec2)=> void;

    extern("libjoyecs", "je_gui_set_next_window_pos")
    public func SetNextWindowPos(pos: ImVec2)=> void;

    public enum DragAttribute
    {
        ImGuiDragDropFlags_None                         = 0,
        // BeginDragDropSource() flags
        ImGuiDragDropFlags_SourceNoPreviewTooltip       = 0x0001,   // By default, a successful call to BeginDragDropSource opens a tooltip so you can display a preview or description of the source contents. This flag disable this behavior.
        ImGuiDragDropFlags_SourceNoDisableHover         = 0x0002,   // By default, when dragging we clear data so that IsItemHovered() will return false, to avoid subsequent user code submitting tooltips. This flag disable this behavior so you can still call IsItemHovered() on the source item.
        ImGuiDragDropFlags_SourceNoHoldToOpenOthers     = 0x0004,   // Disable the behavior that allows to open tree nodes and collapsing header by holding over them while dragging a source item.
        ImGuiDragDropFlags_SourceAllowNullID            = 0x0008,   // Allow items such as Text(), Image() that have no unique identifier to be used as drag source, by manufacturing a temporary identifier based on their window-relative position. This is extremely unusual within the dear imgui ecosystem and so we made it explicit.
        ImGuiDragDropFlags_SourceExtern                 = 0x0010,   // External source (from outside of dear imgui), won't attempt to read current item/window info. Will always return true. Only one Extern source can be active simultaneously.
        ImGuiDragDropFlags_SourceAutoExpirePayload      = 0x0020,   // Automatically expire the payload if the source cease to be submitted (otherwise payloads are persisting while being dragged)
        // AcceptDragDropPayload() flags
        ImGuiDragDropFlags_AcceptBeforeDelivery         = 0x0040,  // AcceptDragDropPayload() will returns true even before the mouse button is released. You can then call IsDelivery() to test if the payload needs to be delivered.
        ImGuiDragDropFlags_AcceptNoDrawDefaultRect      = 0x0080,  // Do not draw the default highlight rectangle when hovering over target.
        ImGuiDragDropFlags_AcceptNoPreviewTooltip       = 0x0100,  // Request hiding the BeginDragDropSource tooltip from the BeginDragDropTarget site.
        // ImGuiDragDropFlags_AcceptPeekOnly               = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect  // For peeking ahead and inspecting the payload before delivery.
    }

    extern("libjoyecs", "je_gui_begin_drag_drop_source")
    public func BeginDragDropSource()=> bool;
    extern("libjoyecs", "je_gui_begin_drag_drop_source_attr")
    public func BeginDragDropSourceAttr(attrib: DragAttribute)=> bool;
    extern("libjoyecs", "je_gui_set_drag_drop_payload")
    public func SetDragDropPayload(type: string, data: string)=> bool;
    extern("libjoyecs", "je_gui_end_drag_drop_source")
    public func EndDragDropSource()=> void;

    public func DragItem(type: string, payload: string, holddo: ()=>void)
    {
        if (BeginDragDropSource())
        {
            do SetDragDropPayload(type, payload);
            holddo();
            do EndDragDropSource();
        }
    }

    extern("libjoyecs", "je_gui_begin_drag_drop_target")
    public func BeginDragDropTarget()=> bool;
    extern("libjoyecs", "je_gui_accept_drag_drop_payload")
    public func AcceptDragDropPayload(type: string)=> option<string>;
    extern("libjoyecs", "je_gui_end_accept_drop_source")
    public func EndDragDropTarget()=> void;

    public func AcceptDrag(types: array<string>)=> option<(string, string)>
    {
        if (BeginDragDropTarget())
        {
            let drag_result = types
                ->> \type = (type, AcceptDragDropPayload(type));
                ->  forall(\accept_type_result = accept_result->is_value
                    where (_, accept_result) = accept_type_result;)
                ->  get(0)
                ->> \e = (e[0], e[1]->unwrap);
                ;

            EndDragDropTarget();

            return drag_result;
        }
        return option::none;
    }

    extern("libjoyecs", "je_gui_set_next_item_open")
    public func SetNextItemOpen(open: bool)=> void;

    extern("libjoyecs", "je_gui_begin_tool_tip")
    public func BeginTooltip()=> void;

    extern("libjoyecs", "je_gui_end_tool_tip")
    public func EndTooltip()=> void;

    extern("libjoyecs", "je_gui_begin_disabled")
    public func BeginDisabled(able: bool) => void;

    extern("libjoyecs", "je_gui_end_disabled")
    public func EndDisabled() => void;

    extern("libjoyecs", "je_gui_style_get_config_color")
    public func GetStyleColor(item: ImGuiCol)=> (real, real, real, real);    

    extern("libjoyecs", "je_gui_style_set_config_color")
    public func SetStyleColor(item: ImGuiCol, col: (real, real, real, real))=>void;  

    extern("libjoyecs", "je_gui_style_set_config_color_dark")
    public func SetStyleColorDark()=>void;  

    extern("libjoyecs", "je_gui_style_set_config_color_classic")
    public func SetStyleColorClassic()=>void; 

    extern("libjoyecs", "je_gui_style_set_config_color_light")
    public func SetStyleColorLight()=>void; 
)" R"(
    public enum DockNodeFlags
    {
        ImGuiDockNodeFlags_None                         = 0,
        ImGuiDockNodeFlags_KeepAliveOnly                = 0x0000'0001,   //       // Don't display the dockspace node but keep it alive. Windows docked into this dockspace node won't be undocked.
        //ImGuiDockNodeFlags_NoCentralNode              = 0x0000'0002,   //       // Disable Central Node (the node which can stay empty)
        ImGuiDockNodeFlags_NoDockingOverCentralNode     = 0x0000'0004,   //       // Disable docking over the Central Node, which will be always kept empty.
        ImGuiDockNodeFlags_PassthruCentralNode          = 0x0000'0008,   //       // Enable passthru dockspace: 1) DockSpace() will render a ImGuiCol_WindowBg background covering everything excepted the Central Node when empty. Meaning the host window should probably use SetNextWindowBgAlpha(0.0f) prior to Begin() when using this. 2) When Central Node is empty: let inputs pass-through + won't display a DockingEmptyBg background. See demo for details.
        ImGuiDockNodeFlags_NoDockingSplit               = 0x0000'0010,   //       // Disable other windows/nodes from splitting this node.
        ImGuiDockNodeFlags_NoResize                     = 0x0000'0020,   // Saved // Disable resizing node using the splitter/separators. Useful with programmatically setup dockspaces.
        ImGuiDockNodeFlags_AutoHideTabBar               = 0x0000'0040,   //       // Tab bar will automatically hide when there is a single window in the dock node.
        ImGuiDockNodeFlags_NoUndocking                  = 0x0000'0080,   //       // Disable undocking this node.
    };

    extern("libjoyecs", "je_gui_dock_space")
    public func DockSpace(id: int, size: ImVec2, attrib: DockNodeFlags)=> void;

    extern("libjoyecs", "je_gui_dock_space_over_viewport")
    public func DockSpaceOverViewport()=> void;

    public enum StyleVar
    {
        // Enum name --------------------- // Member in ImGuiStyle structure (see ImGuiStyle for descriptions)
        ImGuiStyleVar_Alpha,               // float     Alpha
        ImGuiStyleVar_DisabledAlpha,       // float     DisabledAlpha
        ImGuiStyleVar_WindowPadding,       // ImVec2    WindowPadding
        ImGuiStyleVar_WindowRounding,      // float     WindowRounding
        ImGuiStyleVar_WindowBorderSize,    // float     WindowBorderSize
        ImGuiStyleVar_WindowMinSize,       // ImVec2    WindowMinSize
        ImGuiStyleVar_WindowTitleAlign,    // ImVec2    WindowTitleAlign
        ImGuiStyleVar_ChildRounding,       // float     ChildRounding
        ImGuiStyleVar_ChildBorderSize,     // float     ChildBorderSize
        ImGuiStyleVar_PopupRounding,       // float     PopupRounding
        ImGuiStyleVar_PopupBorderSize,     // float     PopupBorderSize
        ImGuiStyleVar_FramePadding,        // ImVec2    FramePadding
        ImGuiStyleVar_FrameRounding,       // float     FrameRounding
        ImGuiStyleVar_FrameBorderSize,     // float     FrameBorderSize
        ImGuiStyleVar_ItemSpacing,         // ImVec2    ItemSpacing
        ImGuiStyleVar_ItemInnerSpacing,    // ImVec2    ItemInnerSpacing
        ImGuiStyleVar_IndentSpacing,       // float     IndentSpacing
        ImGuiStyleVar_CellPadding,         // ImVec2    CellPadding
        ImGuiStyleVar_ScrollbarSize,       // float     ScrollbarSize
        ImGuiStyleVar_ScrollbarRounding,   // float     ScrollbarRounding
        ImGuiStyleVar_GrabMinSize,         // float     GrabMinSize
        ImGuiStyleVar_GrabRounding,        // float     GrabRounding
        ImGuiStyleVar_TabRounding,         // float     TabRounding
        ImGuiStyleVar_TabBarBorderSize,    // float     TabBarBorderSize
        ImGuiStyleVar_ButtonTextAlign,     // ImVec2    ButtonTextAlign
        ImGuiStyleVar_SelectableTextAlign, // ImVec2    SelectableTextAlign
        ImGuiStyleVar_SeparatorTextBorderSize,// float  SeparatorTextBorderSize
        ImGuiStyleVar_SeparatorTextAlign,  // ImVec2    SeparatorTextAlign
        ImGuiStyleVar_SeparatorTextPadding,// ImVec2    SeparatorTextPadding
        ImGuiStyleVar_DockingSeparatorSize,// float     DockingSeparatorSize
        ImGuiStyleVar_COUNT
    };

    extern("libjoyecs", "je_gui_push_style_real")
    public func PushStyleReal(style: StyleVar, val: real)=> void;
    extern("libjoyecs", "je_gui_push_style_vec2")
    public func PushStyleVec2(style: StyleVar, val: ImVec2)=> void;

    extern("libjoyecs", "je_gui_pop_style_var")
    public func PopStyleVar(n: int)=> void;

    extern("libjoyecs", "je_gui_set_window_font_scale")
    public func SetWindowFontScale(factor: real)=> void;

    extern("libjoyecs", "je_gui_dummy")
    public func Dummy(sz: ImVec2)=> void;

    using CodeEditorContext = gchandle
    {
	    public enum PaletteIndex
	    {
		    Default,
		    Keyword,
		    Number,
		    String,
		    CharLiteral,
		    Punctuation,
		    Preprocessor,
		    Identifier,
		    KnownIdentifier,
            Type,
		    PreprocIdentifier,
		    Comment,
		    MultiLineComment,
		    Background,
		    Cursor,
		    Selection,
		    ErrorMarker,
		    Breakpoint,
		    LineNumber,
		    CurrentLineFill,
		    CurrentLineFillInactive,
		    CurrentLineEdge,
		    Max
	    };
        using LanguageDefinition = gchandle
        {
            extern("libjoyecs", "je_gui_code_editor_language_definition_create")
            public func create(name: string)=> LanguageDefinition;

            extern("libjoyecs", "je_gui_code_editor_language_definition_add_keyword")
            public func AddKeyword(
                self: LanguageDefinition, keyword: string)=> void;

            extern("libjoyecs", "je_gui_code_editor_language_definition_add_identifier")
            public func AddIdentifier(
                self: LanguageDefinition, 
                ident: string, 
                declare: string, 
                row: int, 
                col: int)=> void;

            extern("libjoyecs", "je_gui_code_editor_language_definition_add_token_regex")
            public func AddTokenRegex(
                self: LanguageDefinition, 
                regex: string, 
                pidx: PaletteIndex)=> void;

            extern("libjoyecs", "je_gui_code_editor_language_definition_set_mlcomment")
            public func SetMultilineComment(
                self: LanguageDefinition, begin: string, end: string)=> void;

            extern("libjoyecs", "je_gui_code_editor_language_definition_set_slcomment")
            public func SetSingleLineComment(
                self: LanguageDefinition, text: string)=> void;

            extern("libjoyecs", "je_gui_code_editor_language_definition_set_case_sensitive")
            public func SetCaseSensitive(
                self: LanguageDefinition, able: bool)=> void;

            extern("libjoyecs", "je_gui_code_editor_language_definition_set_auto_indentation")
            public func SetAutoIndentation(
                self: LanguageDefinition, able: bool)=> void;

            extern("libjoyecs", "je_gui_code_editor_language_definition_set_preproc_char")
            public func SetPreprocChar(
                self: LanguageDefinition, ch: cchar)=> void;
        }

        extern("libjoyecs", "je_gui_code_editor_create")
            public func create()=> CodeEditorContext;

        extern("libjoyecs", "je_gui_code_editor_get_text")
            public func GetText(self: CodeEditorContext)=> string;
        extern("libjoyecs", "je_gui_code_editor_set_text")
            public func SetText(self: CodeEditorContext, text: string)=> void;

        extern("libjoyecs", "je_gui_code_editor_set_language_definition")
            public func SetLanguageDefinition(self: CodeEditorContext, ldef: LanguageDefinition)=> void;

        extern("libjoyecs", "je_gui_code_editor_undoable")
            public func CanUndo(self: CodeEditorContext)=> bool;
        extern("libjoyecs", "je_gui_code_editor_redoable")
            public func CanRedo(self: CodeEditorContext)=> bool;

        extern("libjoyecs", "je_gui_code_editor_undo")
            public func Undo(self: CodeEditorContext)=> void;
        extern("libjoyecs", "je_gui_code_editor_redo")
            public func Redo(self: CodeEditorContext)=> void;

        extern("libjoyecs", "je_gui_code_editor_get_cursor_pos")
            public func GetCursorPosition(self: CodeEditorContext)=> (int, int);
        extern("libjoyecs", "je_gui_code_editor_set_cursor_pos")
            public func SetCursorPosition(self: CodeEditorContext, pos: (int, int))=> void;
    
        extern("libjoyecs", "je_gui_code_editor_insert_text")
            public func InsertText(self: CodeEditorContext, text: string)=> void;

        extern("libjoyecs", "je_gui_code_editor_copy")
            public func Copy(self: CodeEditorContext)=> void;
        extern("libjoyecs", "je_gui_code_editor_cut")
            public func Cut(self: CodeEditorContext)=> void;
        extern("libjoyecs", "je_gui_code_editor_paste")
            public func Paste(self: CodeEditorContext)=> void;
        extern("libjoyecs", "je_gui_code_editor_delete")
            public func Delete(self: CodeEditorContext)=> void;
    }
    extern("libjoyecs", "je_gui_code_editor_show")
        public func CodeEditor(ctx: CodeEditorContext, label: string)=> void;
    extern("libjoyecs", "je_gui_code_editor_show_size")
        public func CodeEditorSize(ctx: CodeEditorContext, label: string, sz: ImVec2, board: bool)=> void;
)" R"(
    namespace node_editor
    {
        using EditorContext = gchandle
        {
            public func close(self: EditorContext)
            {
                return self: gchandle->close;
            }
        }

        public using NodeId = int;
        public using PinId = int;
        public using LinkId = int;

        extern("libjoyecs", "je_gui_node_editor_context_create")
        public func CreateContext()=> EditorContext;

        extern("libjoyecs", "je_gui_node_editor_begin")
        public func Begin(label: string, ctx: EditorContext)=> void;    

        extern("libjoyecs", "je_gui_node_editor_end")
        public func End()=> void;

        extern("libjoyecs", "je_gui_node_editor_begin_node")
        public func BeginNode(id: NodeId)=> void;

        extern("libjoyecs", "je_gui_node_editor_end_node")  
        public func EndNode()=> void;

        extern("libjoyecs", "je_gui_node_editor_begin_input_pin")
        public func BeginInputPin(id: PinId)=> void;

        extern("libjoyecs", "je_gui_node_editor_begin_output_pin")
        public func BeginOutputPin(id: PinId)=> void;

        extern("libjoyecs", "je_gui_node_editor_end_pin")
        public func EndPin()=> void;

        extern("libjoyecs", "je_gui_node_editor_pin_pivot_rect")
        public func PinPivotRect(a: ImVec2, b: ImVec2)=> void;

        extern("libjoyecs", "je_gui_node_editor_pin_rect")
        public func PinRect(a: ImVec2, b: ImVec2)=> void;

        extern("libjoyecs", "je_gui_node_editor_link")
        public func Link(linkid: LinkId, output_pinid: PinId, input_pinid: PinId, color: Color32RGBA, board: real)=> void;

        extern("libjoyecs", "je_gui_node_editor_begin_create")
        public func BeginCreate(color: Color32RGBA, board: real)=> bool;

        extern("libjoyecs", "je_gui_node_editor_end_create")
        public func EndCreate()=> void;

        extern("libjoyecs", "je_gui_node_editor_begin_delete")
        public func BeginDelete()=> bool;

        extern("libjoyecs", "je_gui_node_editor_end_delete")
        public func EndDelete()=> void;

        extern("libjoyecs", "je_gui_node_editor_accept_new_item")
        public func AcceptNewItem()=> bool;

        extern("libjoyecs", "je_gui_node_editor_reject_new_item")
        public func RejectNewItem()=> void;

        extern("libjoyecs", "je_gui_node_editor_accept_new_item_color")
        public func AcceptNewItemColor(color: Color32RGBA, board: real)=> bool;

        extern("libjoyecs", "je_gui_node_editor_reject_new_item_color")
        public func RejectNewItemColor(color: Color32RGBA, board: real)=> void;

        extern("libjoyecs", "je_gui_node_editor_accept_deleted_item")
        public func AcceptDeletedItem()=> bool;

        extern("libjoyecs", "je_gui_node_editor_reject_deleted_item")
        public func RejectDeletedItem()=> void;

        extern("libjoyecs", "je_gui_node_editor_query_new_link")
        public func QueryNewLink()=> option<(PinId, PinId)>;

        extern("libjoyecs", "je_gui_node_editor_query_new_node")
        public func QueryNewNode()=> option<PinId>;

        extern("libjoyecs", "je_gui_node_editor_query_deleted_node")
        public func QueryDeletedNode()=> option<NodeId>;

        extern("libjoyecs", "je_gui_node_editor_query_deleted_link")
        public func QueryDeletedLink()=> option<LinkId>;

        public enum StyleColor
        {
            StyleColor_Bg,
            StyleColor_Grid,
            StyleColor_NodeBg,
            StyleColor_NodeBorder,
            StyleColor_HovNodeBorder,
            StyleColor_SelNodeBorder,
            StyleColor_NodeSelRect,
            StyleColor_NodeSelRectBorder,
            StyleColor_HovLinkBorder,
            StyleColor_SelLinkBorder,
            StyleColor_HighlightLinkBorder,
            StyleColor_LinkSelRect,
            StyleColor_LinkSelRectBorder,
            StyleColor_PinRect,
            StyleColor_PinRectBorder,
            StyleColor_Flow,
            StyleColor_FlowMarker,
            StyleColor_GroupBg,
            StyleColor_GroupBorder,

            StyleColor_Count
        }

        extern("libjoyecs", "je_gui_node_editor_push_style_color")
        public func PushStyleColor(style: StyleColor, color: Color32RGBA)=> void;

        extern("libjoyecs", "je_gui_node_editor_pop_style_color")
        public func PopStyleColor(n: int)=> void;

        extern("libjoyecs", "je_gui_node_editor_delete_node")
        public func DeleteNode(id: NodeId)=> bool;

        extern("libjoyecs", "je_gui_node_editor_get_node_position")
        public func GetNodePosition(id: NodeId)=> (real, real);

        extern("libjoyecs", "je_gui_node_editor_set_node_position")
        public func SetNodePosition(id: NodeId, pos: (real, real))=> void;

        extern("libjoyecs", "je_gui_node_editor_delete_link")
        public func DeleteLink(id: LinkId)=> bool;

        extern("libjoyecs", "je_gui_node_editor_get_hovered_node")
        public func GetHoveredNode()=> option<NodeId>;

        extern("libjoyecs", "je_gui_node_editor_get_hovered_pin")
        public func GetHoveredPin()=> option<PinId>;

        extern("libjoyecs", "je_gui_node_editor_get_hovered_link")
        public func GetHoveredLink()=> option<LinkId>;

        extern("libjoyecs", "je_gui_node_editor_is_node_selected")
        public func IsNodeSelected()=> option<LinkId>;

        extern("libjoyecs", "je_gui_node_editor_canvas_to_screen")
        public func CanvasToScreen(pos: (real, real))=> (real, real);

        extern("libjoyecs", "je_gui_node_editor_screen_to_canvas")
        public func ScreenToCanvas(pos: (real, real))=> (real, real);

        extern("libjoyecs", "je_gui_node_editor_suspend")
        public func Suspend()=> void;

        extern("libjoyecs", "je_gui_node_editor_resume")
        public func Resume()=> void;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    extern("libjoyecs", "je_gui_launch")
    private func _launch<LT, FT, ATs>(coloop:LT, job_func:FT, args: ATs)=> void
        where coloop(job_func, args) is void;

    namespace unsafe
    {
        extern("libjoyecs", "je_gui_stop_all_work")
        public func shutdown()=> void;
    }

    private func dialog<FT, ATs>(job_func:FT, args: ATs)=> void
        where job_func(args...) is bool;
    {
        while (job_func(args...))
            std::yield();
    }

    public func launch<FT, ATs>(job_func: FT, args: ATs)=> void
        where job_func(args...) is bool;
    {
        return _launch(dialog:<FT, ATs>, job_func, args);
    }

    namespace input
    {
        extern("libjoyecs", "je_gui_get_input_state")
        public func keystate(kcode: je::keycode)=> (bool, bool);
    }
    
    extern("libjoyecs", "je_gui_register_exit_callback")
    public func register_exit_callback(callback: ()=> bool)=> void;
    
    extern("libjoyecs", "je_gui_unregister_exit_callback")
    public func unregister_exit_callback()=> void;

    extern("libjoyecs", "je_gui_set_font")
    public func set_font(font: option<string>, latin_font: option<string>, size: int)=> void;
}

)" };

struct key_state
{
    bool m_last_frame_down;
    bool m_this_frame_down;
};

std::mutex _key_state_record_mx;
std::unordered_map<jeecs::input::keycode, key_state> _key_state_record;

struct gui_wo_job_coroutine
{
    wo_vm work_vm;
    gui_wo_job_coroutine* last;
};
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_job_list;
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_new_job_list;

struct _jegui_thread_local_context
{
    bool _jegui_stop_work_flag = false;
    bool _jegui_need_flip_frambuf = false;

    jeecs::basic::resource<jeecs::graphic::shader>
        _jegl_rend_texture_shader = nullptr;

    jegui_user_image_loader_t _jegl_get_native_texture = nullptr;
    jegui_user_sampler_loader_t _jegl_bind_shader_sampler_state = nullptr;

    std::string _jegui_imgui_config_path;
};
thread_local _jegui_thread_local_context _je_gui_tls_ctx;

struct _jegui_static_context
{
    wo_vm _jegui_exit_callback_handler_vm = nullptr;
    wo_pin_value _jegui_exit_callback_function = nullptr;
    std::optional<std::string> _jegui_specify_font_path = std::nullopt;
    std::optional<std::string> _jegui_specify_latin_font_path = std::nullopt;
    size_t _jegui_specify_font_size = 18;
};
_jegui_static_context _je_gui_static_ctx;

void jegui_set_font(
    const char* general_font_path,
    const char* latin_font_path,
    size_t size)
{
    _je_gui_static_ctx._jegui_specify_font_path
        = general_font_path ? std::optional(general_font_path) : std::nullopt;
    _je_gui_static_ctx._jegui_specify_latin_font_path
        = latin_font_path ? std::optional(latin_font_path) : std::nullopt;

    _je_gui_static_ctx._jegui_specify_font_size = size;
}

WO_API wo_api je_gui_begin_tool_tip(wo_vm vm, wo_value args)
{
    ImGui::BeginTooltip();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_end_tool_tip(wo_vm vm, wo_value args)
{
    ImGui::EndTooltip();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begin_disabled(wo_vm vm, wo_value args)
{
    ImGui::BeginDisabled(wo_bool(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_end_disabled(wo_vm vm, wo_value args)
{
    ImGui::EndDisabled();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begin_drag_drop_source(wo_vm vm, wo_value args)
{
    bool result;
    result = ImGui::BeginDragDropSource(ImGuiDragDropFlags_None);
    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_begin_drag_drop_source_attr(wo_vm vm, wo_value args)
{
    bool result;
    result = ImGui::BeginDragDropSource((ImGuiDragDropFlags)wo_int(args + 0));
    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_set_drag_drop_payload(wo_vm vm, wo_value args)
{
    wo_string_t buf = wo_string(args + 1);

    bool result = ImGui::SetDragDropPayload(wo_string(args + 0), buf, strlen(buf) + 1);
    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_end_drag_drop_source(wo_vm vm, wo_value args)
{
    ImGui::EndDragDropSource();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begin_drag_drop_target(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginDragDropTarget());
}
WO_API wo_api je_gui_accept_drag_drop_payload(wo_vm vm, wo_value args)
{
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(wo_string(args + 0)))
    {
        return wo_ret_option_string(vm, (const char*)payload->Data);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_end_accept_drop_source(wo_vm vm, wo_value args)
{
    ImGui::EndDragDropTarget();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_set_next_item_open(wo_vm vm, wo_value args)
{
    ImGui::SetNextItemOpen(wo_bool(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_set_clip_board_text(wo_vm vm, wo_value args)
{
    ImGui::SetClipboardText(wo_string(args));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_get_clip_board_text(wo_vm vm, wo_value args)
{
    const char* t = ImGui::GetClipboardText();
    return wo_ret_string(vm, t == nullptr ? "" : t);
}

wo_value set_float2_to_struct(wo_value v, wo_vm vm, float x, float y)
{
    wo_set_struct(v, vm, 2);
    _wo_value elem;
    wo_set_float(&elem, x);
    wo_struct_set(v, 0, &elem);
    wo_set_float(&elem, y);
    wo_struct_set(v, 1, &elem);
    return v;
}
wo_value set_float3_to_struct(wo_value v, wo_vm vm, float x, float y, float z)
{
    wo_set_struct(v, vm, 3);
    _wo_value elem;
    wo_set_float(&elem, x);
    wo_struct_set(v, 0, &elem);
    wo_set_float(&elem, y);
    wo_struct_set(v, 1, &elem);
    wo_set_float(&elem, z);
    wo_struct_set(v, 2, &elem);
    return v;
}
wo_value set_float4_to_struct(wo_value v, wo_vm vm, float x, float y, float z, float w)
{
    wo_set_struct(v, vm, 4);
    _wo_value elem;
    wo_set_float(&elem, x);
    wo_struct_set(v, 0, &elem);
    wo_set_float(&elem, y);
    wo_struct_set(v, 1, &elem);
    wo_set_float(&elem, z);
    wo_struct_set(v, 2, &elem);
    wo_set_float(&elem, w);
    wo_struct_set(v, 3, &elem);
    return v;
}

WO_API wo_api je_gui_get_window_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto&& wpos = ImGui::GetWindowPos();

    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, wpos.x, wpos.y));
}

WO_API wo_api je_gui_get_mouse_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto&& mpos = ImGui::GetMousePos();
    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, mpos.x, mpos.y));
}

WO_API wo_api je_gui_get_mouse_delta_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto&& mdpos = ImGui::GetIO().MouseDelta;
    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, mdpos.x, mdpos.y));
}

WO_API wo_api je_gui_get_cursor_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto&& cpos = ImGui::GetCursorPos();
    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, cpos.x, cpos.y));
}
WO_API wo_api je_gui_get_item_rect_size(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto&& isize = ImGui::GetItemRectSize();
    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, isize.x, isize.y));
}

WO_API wo_api je_gui_get_item_rect(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto&& isizemin = ImGui::GetItemRectMin();
    auto&& isizemax = ImGui::GetItemRectMax();

    wo_set_struct(s + 0, vm, 2);

    wo_struct_set(s + 0, 0, set_float2_to_struct(s + 1, vm, isizemin.x, isizemin.y));
    wo_struct_set(s + 0, 1, set_float2_to_struct(s + 1, vm, isizemax.x, isizemax.y));

    return wo_ret_val(vm, s + 0);
}

WO_API wo_api je_gui_push_id_str(wo_vm vm, wo_value args)
{
    ImGui::PushID(wo_string(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_push_id(wo_vm vm, wo_value args)
{
    ImGui::PushID((int)wo_int(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_pop_id(wo_vm vm, wo_value args)
{
    ImGui::PopID();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_get_id(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, (wo_integer_t)ImGui::GetID(wo_string(args + 0)));
}

WO_API wo_api je_gui_beginpopup_contextitem(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginPopupContextItem());
}
WO_API wo_api je_gui_beginpopup_contextitem_label(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginPopupContextItem(wo_string(args + 0)));
}
WO_API wo_api je_gui_beginpopup_contextwindow(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginPopupContextWindow());
}
WO_API wo_api je_gui_beginpopup_contextwindow_label(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginPopupContextWindow(wo_string(args + 0)));
}
WO_API wo_api je_gui_begintabbar(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginTabBar(wo_string(args + 0), (ImGuiTabBarFlags)wo_int(args + 1)));
}

WO_API wo_api je_gui_endtabbar(wo_vm vm, wo_value args)
{
    ImGui::EndTabBar();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begintabitem(wo_vm vm, wo_value args)
{
    bool display = ImGui::BeginTabItem(wo_string(args + 0), nullptr, (ImGuiTabBarFlags)wo_int(args + 1));
    return wo_ret_bool(vm, display);
}

WO_API wo_api je_gui_begintabitem_open(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    bool open = true;
    bool display = ImGui::BeginTabItem(wo_string(args + 0), &open, (ImGuiTabBarFlags)wo_int(args + 1));

    wo_value ret = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(ret, vm, 2);

    wo_set_bool(elem, display);
    wo_struct_set(ret, 0, elem);
    wo_set_bool(elem, open);
    wo_struct_set(ret, 1, elem);

    return wo_ret_val(vm, ret);
}

ImVec2 val2vec2(wo_value v)
{
    _wo_value tmp;
    wo_struct_get(&tmp, v, 0);
    float x = wo_float(&tmp);
    wo_struct_get(&tmp, v, 1);
    float y = wo_float(&tmp);
    return ImVec2(x, y);
}
ImVec4 val2vec4(wo_value v)
{
    _wo_value tmp;
    wo_struct_get(&tmp, v, 0);
    float x = wo_float(&tmp);
    wo_struct_get(&tmp, v, 1);
    float y = wo_float(&tmp);
    wo_struct_get(&tmp, v, 2);
    float z = wo_float(&tmp);
    wo_struct_get(&tmp, v, 3);
    float w = wo_float(&tmp);
    return ImVec4(x, y, z, w);
}

WO_API wo_api je_gui_push_clip_rect(wo_vm vm, wo_value args)
{
    wo_value from = args + 0;
    wo_value to = args + 1;
    ImGui::PushClipRect(
        val2vec2(from),
        val2vec2(to),
        true);

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_pop_clip_rect(wo_vm vm, wo_value args)
{
    ImGui::PopClipRect();

    return wo_ret_void(vm);
}

ImU32 val2color32(wo_value v)
{
    _wo_value tmp;
    wo_struct_get(&tmp, v, 0);
    int x = wo_int(&tmp);
    wo_struct_get(&tmp, v, 1);
    int y = wo_int(&tmp);
    wo_struct_get(&tmp, v, 2);
    int z = wo_int(&tmp);
    wo_struct_get(&tmp, v, 3);
    int w = wo_int(&tmp);

    return IM_COL32(x, y, z, w);
}

ImVec4 val2colorf4(wo_value v)
{
    _wo_value tmp;
    wo_struct_get(&tmp, v, 0);
    float x = (float)wo_int(&tmp);
    wo_struct_get(&tmp, v, 1);
    float y = (float)wo_int(&tmp);
    wo_struct_get(&tmp, v, 2);
    float z = (float)wo_int(&tmp);
    wo_struct_get(&tmp, v, 3);
    float w = (float)wo_int(&tmp);

    return ImVec4(
        x / 255.0f,
        y / 255.0f,
        z / 255.0f,
        w / 255.0f);
}

WO_API wo_api je_gui_push_style_color(wo_vm vm, wo_value args)
{
    ImGui::PushStyleColor((ImGuiCol)wo_int(args + 0), val2color32(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_pop_style_color(wo_vm vm, wo_value args)
{
    ImGui::PopStyleColor((int)wo_int(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_get_window_draw_list(wo_vm vm, wo_value args)
{
    return wo_ret_pointer(vm, ImGui::GetWindowDrawList());
}

WO_API wo_api je_gui_draw_list_add_rect(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddRect(val2vec2(args + 1), val2vec2(args + 2), val2color32(args + 3), 0.f, 0, wo_float(args + 4));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_draw_list_add_rect_filled(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddRectFilled(val2vec2(args + 1), val2vec2(args + 2), val2color32(args + 3));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_draw_list_add_triangle(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddTriangle(val2vec2(args + 1), val2vec2(args + 2), val2vec2(args + 3), val2color32(args + 4), wo_float(args + 5));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_draw_list_add_triangle_filled(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddTriangleFilled(val2vec2(args + 1), val2vec2(args + 2), val2vec2(args + 3), val2color32(args + 4));
    return wo_ret_void(vm);
}


WO_API wo_api je_gui_draw_list_add_text(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddText(val2vec2(args + 1), val2color32(args + 2), wo_string(args + 3));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_draw_list_add_image(wo_vm vm, wo_value args)
{
    ImDrawList* dlist = (ImDrawList*)wo_pointer(args + 0);

    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 3);
    jegl_using_resource((*texture)->resouce());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resouce()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resouce()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }
    dlist->AddCallback([](auto, auto) {_je_gui_tls_ctx._jegl_bind_shader_sampler_state(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce()); }, nullptr);
    dlist->AddImage((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture((*texture)->resouce()), val2vec2(args + 1), val2vec2(args + 2), uvmin, uvmax, val2color32(args + 4));
    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_draw_list_add_line(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddLine(val2vec2(args + 1), val2vec2(args + 2), val2color32(args + 3), wo_float(args + 4));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_draw_list_add_circle(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddCircle(val2vec2(args + 1), wo_float(args + 2), val2color32(args + 3), 0, wo_float(args + 4));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_draw_list_add_filled_circle(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddCircleFilled(val2vec2(args + 1), wo_float(args + 2), val2color32(args + 3));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_draw_list_add_bezier_quad(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddBezierQuadratic(val2vec2(args + 1), val2vec2(args + 2), val2vec2(args + 3), val2color32(args + 4), wo_float(args + 5));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_draw_list_add_bezier_cubic(wo_vm vm, wo_value args)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddBezierCubic(val2vec2(args + 1), val2vec2(args + 2), val2vec2(args + 3), val2vec2(args + 4), val2color32(args + 5), wo_float(args + 6));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_endtabitem(wo_vm vm, wo_value args)
{
    ImGui::EndTabItem();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_beginpopup(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginPopup(wo_string(args + 0)));
}

WO_API wo_api je_gui_openpopup_on_item_click(wo_vm vm, wo_value args)
{
    ImGui::OpenPopupOnItemClick();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_openpopup_on_item_click_label(wo_vm vm, wo_value args)
{
    ImGui::OpenPopupOnItemClick(wo_string(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_openpopup_on_item_click_attr(wo_vm vm, wo_value args)
{
    ImGui::OpenPopupOnItemClick(nullptr, (ImGuiPopupFlags)wo_int(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_openpopup_on_item_click_label_attr(wo_vm vm, wo_value args)
{
    ImGui::OpenPopupOnItemClick(wo_string(args + 0), (ImGuiPopupFlags)wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_openpopup(wo_vm vm, wo_value args)
{
    ImGui::OpenPopup(wo_string(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_endpopup(wo_vm vm, wo_value args)
{
    ImGui::EndPopup();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begin_listbox(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginListBox(wo_string(args + 0), ImVec2(wo_float(args + 1), wo_float(args + 2))));
}
WO_API wo_api je_gui_begin_selectable(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::Selectable(wo_string(args + 0)));
}
WO_API wo_api je_gui_begin_selectable_selected(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::Selectable(wo_string(args + 0), wo_bool(args + 1)));
}
WO_API wo_api je_gui_end_listbox(wo_vm vm, wo_value args)
{
    ImGui::EndListBox();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_sameline(wo_vm vm, wo_value args)
{
    ImGui::SameLine();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begingroup(wo_vm vm, wo_value args)
{
    ImGui::BeginGroup();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_endgroup(wo_vm vm, wo_value args)
{
    ImGui::EndGroup();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_is_itemclicked(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsItemClicked((ImGuiMouseButton)wo_int(args + 0)));
}

WO_API wo_api je_gui_is_itemtoggledopen(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsItemToggledOpen());
}

WO_API wo_api je_gui_is_itemhovered(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsItemHovered());
}

WO_API wo_api je_gui_is_mousehoveringrect(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsMouseHoveringRect(
        val2vec2(args + 0), val2vec2(args + 1)));
}

WO_API wo_api je_gui_is_item_active(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsItemActive());
}

WO_API wo_api je_gui_is_mouse_dragging(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsMouseDragging((ImGuiMouseButton)wo_int(args + 0)));
}
WO_API wo_api je_gui_is_mouse_down(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsMouseDown((ImGuiMouseButton)wo_int(args + 0)));
}
WO_API wo_api je_gui_is_mouse_released(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsMouseReleased((ImGuiMouseButton)wo_int(args + 0)));
}

WO_API wo_api je_gui_set_tooltip(wo_vm vm, wo_value args)
{
    ImGui::SetTooltip("%s", wo_string(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_treenodeex(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm,
        ImGui::TreeNodeEx(
            wo_string(args + 0),
            (ImGuiTreeNodeFlags)wo_int(args + 1)));
}

WO_API wo_api je_gui_treenode(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::TreeNode(wo_string(args + 0)));
}
WO_API wo_api je_gui_treepop(wo_vm vm, wo_value args)
{
    ImGui::TreePop();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_listbox(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    int selected_item = -1;
    int max_height_item = -1;

    std::vector<const char*> items((size_t)wo_lengthof(args + 1));
    wo_value elem = s + 0;
    for (size_t i = 0; i < items.size(); i++)
    {
        wo_arr_get(elem, args + 1, i);
        items[i] = wo_string(elem);
    }

    bool val_changed = ImGui::ListBox(wo_string(args + 0), &selected_item, items.data(), (int)items.size(), max_height_item);

    if (val_changed)
        return wo_ret_option_int(vm, selected_item);
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_listbox_select(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    int selected_item = (int)wo_int(args + 2);
    int max_height_item = -1;

    std::vector<const char*> items((size_t)wo_lengthof(args + 1));
    wo_value elem = s + 0;
    for (size_t i = 0; i < items.size(); i++)
    {
        wo_arr_get(elem, args + 1, i);
        items[i] = wo_string(elem);
    }

    bool val_changed = ImGui::ListBox(wo_string(args + 0), &selected_item, items.data(), (int)items.size(), max_height_item);

    if (val_changed)
        return wo_ret_option_int(vm, selected_item);
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_listbox_select_height(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    int selected_item = (int)wo_int(args + 2);
    int max_height_item = (int)wo_int(args + 3);

    std::vector<const char*> items((size_t)wo_lengthof(args + 1));
    wo_value elem = s + 0;
    for (size_t i = 0; i < items.size(); i++)
    {
        wo_arr_get(elem, args + 1, i);
        items[i] = wo_string(elem);
    }

    bool val_changed = ImGui::ListBox(wo_string(args + 0), &selected_item, items.data(), (int)items.size(), max_height_item);

    if (val_changed)
        return wo_ret_option_int(vm, selected_item);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_listbox_withsize(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    //func ListBox(label:string, items:array<string>, ref select_item:int, width:real, height:real)=> int;
    int origin_selected_index = (int)wo_int(args + 2);
    int selected_index = -1;
    bool value_updated = false;

    wo_value elem = s + 0;

    if (ImGui::BeginListBox(wo_string(args + 0), ImVec2(wo_float(args + 3), wo_float(args + 4))))
    {
        size_t sz = (size_t)wo_lengthof(args + 1);
        for (size_t i = 0; i < sz; i++)
        {
            wo_arr_get(elem, args + 1, i);
            wo_string_t item = wo_string(elem);
            if (ImGui::Selectable(item, i == origin_selected_index))
            {
                value_updated = true;
                selected_index = (int)i;
            }

        }
        if (!value_updated)
            selected_index = origin_selected_index;

        ImGui::EndListBox();
    }
    if (value_updated)
        return wo_ret_option_int(vm, selected_index);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_job_vm_handle(wo_vm vm, wo_value args)
{
    return wo_ret_pointer(vm, vm);
}

WO_API wo_api je_gui_begin(wo_vm vm, wo_value args)
{
    bool showing = ImGui::Begin(wo_string(args));
    return wo_ret_bool(vm, showing);
}
WO_API wo_api je_gui_begin_attr(wo_vm vm, wo_value args)
{
    bool showing = ImGui::Begin(wo_string(args), 0, (ImGuiWindowFlags)wo_int(args + 1));
    return wo_ret_bool(vm, showing);
}
WO_API wo_api je_gui_begin_open(wo_vm vm, wo_value args)
{
    bool windows_flag = true;
    bool showing = ImGui::Begin(wo_string(args), &windows_flag, (ImGuiWindowFlags)wo_int(args + 1));

    if (windows_flag)
        return wo_ret_option_bool(vm, showing);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_is_window_focused(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsWindowFocused());
}

WO_API wo_api je_gui_end(wo_vm vm, wo_value args)
{
    ImGui::End();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_begin_child(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginChild(wo_string(args + 0), ImVec2(0.f, 0.f), true));
}
WO_API wo_api je_gui_begin_child_attr(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginChild(wo_string(args + 0),
        ImVec2(0.f, 0.f), true, (ImGuiWindowFlags)wo_int(args + 1)));
}
WO_API wo_api je_gui_begin_child_size(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginChild(wo_string(args + 0),
        ImVec2(wo_float(args + 1), wo_float(args + 2)), true));
}

WO_API wo_api je_gui_end_child(wo_vm vm, wo_value args)
{
    ImGui::EndChild();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_progress_bar(wo_vm vm, wo_value args)
{
    ImGui::ProgressBar(wo_float(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_progress_bar_size(wo_vm vm, wo_value args)
{
    ImGui::ProgressBar(wo_float(args + 0), val2vec2(args + 1));
    return wo_ret_void(vm);
}


WO_API wo_api je_gui_text(wo_vm vm, wo_value args)
{
    ImGui::Text("%s", wo_string(args));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_text_disabled(wo_vm vm, wo_value args)
{
    ImGui::TextDisabled("%s", wo_string(args));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_button(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::Button(wo_string(args + 0)));
}

WO_API wo_api je_gui_button_size(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::Button(wo_string(args + 0), val2vec2(args + 1)));
}

WO_API wo_api je_gui_invisible_button(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::InvisibleButton(wo_string(args + 0), val2vec2(args + 1)));
}

WO_API wo_api je_gui_begin_main_menu_bar(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginMainMenuBar());
}
WO_API wo_api je_gui_menu_item(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::MenuItem(wo_string(args + 0)));
}
WO_API wo_api je_gui_menu_item_shortcut(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::MenuItem(wo_string(args + 0), wo_string(args + 1)));
}
WO_API wo_api je_gui_menu_item_enabled(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::MenuItem(wo_string(args + 0), 0, false, wo_bool(args + 1)));
}
WO_API wo_api je_gui_menu_item_shortcut_enabled(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::MenuItem(wo_string(args + 0), wo_string(args + 1), false, wo_bool(args + 2)));
}
WO_API wo_api je_gui_menu_item_selected(wo_vm vm, wo_value args)
{
    bool selected = wo_bool(args + 2);
    bool clicked = ImGui::MenuItem(wo_string(args + 0), wo_string(args + 1), &selected, wo_bool(args + 3));
    wo_set_bool(args + 2, selected);

    if (clicked)
        return wo_ret_option_bool(vm, selected);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_end_main_menu_bar(wo_vm vm, wo_value args)
{
    ImGui::EndMainMenuBar();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_begin_menu_bar(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginMenuBar());
}
WO_API wo_api je_gui_end_menu_bar(wo_vm vm, wo_value args)
{
    ImGui::EndMenuBar();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_begin_menu(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginMenu(wo_string(args + 0)));
}
WO_API wo_api je_gui_begin_menu_enabled(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::BeginMenu(wo_string(args + 0), wo_bool(args + 1)));
}
WO_API wo_api je_gui_end_menu(wo_vm vm, wo_value args)
{
    ImGui::EndMenu();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_separator(wo_vm vm, wo_value args)
{
    ImGui::Separator();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_image(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resouce());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resouce()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resouce()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {_je_gui_tls_ctx._jegl_bind_shader_sampler_state(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce()); }, nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture((*texture)->resouce()),
        ImVec2(
            (float)((*texture)->resouce())->m_raw_texture_data->m_width,
            (float)((*texture)->resouce())->m_raw_texture_data->m_height
        ), uvmin, uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_image_scale(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resouce());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resouce()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resouce()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {_je_gui_tls_ctx._jegl_bind_shader_sampler_state(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce()); }, nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture((*texture)->resouce()),
        ImVec2(
            ((*texture)->resouce())->m_raw_texture_data->m_width * wo_float(args + 1),
            ((*texture)->resouce())->m_raw_texture_data->m_height * wo_float(args + 1)
        ), uvmin, uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_image_size(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resouce());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resouce()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resouce()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {_je_gui_tls_ctx._jegl_bind_shader_sampler_state(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce()); }, nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture((*texture)->resouce()),
        ImVec2(
            wo_float(args + 1),
            wo_float(args + 2)
        ), uvmin, uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_image_size_color(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resouce());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resouce()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resouce()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {_je_gui_tls_ctx._jegl_bind_shader_sampler_state(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce()); }, nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture((*texture)->resouce()),
        ImVec2(
            wo_float(args + 1),
            wo_float(args + 2)
        ), uvmin, uvmax, val2colorf4(args + 3));

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_imagebutton(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 1);

    jegl_using_resource((*texture)->resouce());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resouce()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resouce()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {_je_gui_tls_ctx._jegl_bind_shader_sampler_state(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce()); }, nullptr);
    result = ImGui::ImageButton(label, (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture((*texture)->resouce()),
        ImVec2(
            (float)((*texture)->resouce())->m_raw_texture_data->m_width,
            (float)((*texture)->resouce())->m_raw_texture_data->m_height
        ), uvmin, uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_imagebutton_scale(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 1);

    jegl_using_resource((*texture)->resouce());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resouce()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resouce()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {_je_gui_tls_ctx._jegl_bind_shader_sampler_state(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce()); }, nullptr);
    result = ImGui::ImageButton(label, (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture((*texture)->resouce()),
        ImVec2(
            ((*texture)->resouce())->m_raw_texture_data->m_width * wo_float(args + 2),
            ((*texture)->resouce())->m_raw_texture_data->m_height * wo_float(args + 2)
        ), uvmin, uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_imagebutton_size(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 1);

    jegl_using_resource((*texture)->resouce());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resouce()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resouce()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {_je_gui_tls_ctx._jegl_bind_shader_sampler_state(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce()); }, nullptr);
    result = ImGui::ImageButton(label, (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture((*texture)->resouce()),
        ImVec2(
            wo_float(args + 2),
            wo_float(args + 3)
        ), uvmin, uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_bool(vm, result);
}

WO_API wo_api je_gui_content_region_avail(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto&& sz = ImGui::GetContentRegionAvail();
    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, sz.x, sz.y));
}

WO_API wo_api je_gui_set_next_window_size_constraints(wo_vm vm, wo_value args)
{
    ImGui::SetNextWindowSizeConstraints(val2vec2(args + 0), val2vec2(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_set_next_window_size(wo_vm vm, wo_value args)
{
    ImGui::SetNextWindowSize(val2vec2(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_set_next_window_pos(wo_vm vm, wo_value args)
{
    ImGui::SetNextWindowPos(val2vec2(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_colorbutton(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::ColorButton(wo_string(args + 0), val2vec4(args + 1)));
}
WO_API wo_api je_gui_colorpicker4(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    float rgba[4] = {};

    wo_value elem = s + 0;
    wo_struct_get(elem, args + 1, 0);
    rgba[0] = wo_float(elem);
    wo_struct_get(elem, args + 1, 1);
    rgba[1] = wo_float(elem);
    wo_struct_get(elem, args + 1, 2);
    rgba[2] = wo_float(elem);
    wo_struct_get(elem, args + 1, 3);
    rgba[3] = wo_float(elem);

    if (ImGui::ColorPicker4(wo_string(args + 0), rgba))
    {
        wo_value result = s + 1;
        set_float4_to_struct(result, vm, rgba[0], rgba[1], rgba[2], rgba[3]);

        return wo_ret_option_val(vm, result);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_push_item_width(wo_vm vm, wo_value args)
{
    ImGui::PushItemWidth(wo_float(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_pop_item_width(wo_vm vm, wo_value args)
{
    ImGui::PopItemWidth();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_input_text_box(wo_vm vm, wo_value args)
{
    std::string buf = wo_string(args + 1);
    if (ImGui::InputText(wo_string(args + 0), &buf))
    {
        return wo_ret_option_string(vm, buf.c_str());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_text_attr_box(wo_vm vm, wo_value args)
{
    std::string buf = wo_string(args + 1);
    if (ImGui::InputText(wo_string(args + 0), &buf, (ImGuiInputFlags)wo_int(args + 2)))
    {
        return wo_ret_option_string(vm, buf.c_str());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_checkbox(wo_vm vm, wo_value args)
{
    bool checked = wo_bool(args + 1);
    if (ImGui::Checkbox(wo_string(args + 0), &checked))
        return wo_ret_option_bool(vm, checked);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_int_box(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    int value = (int)wo_int(args + 1);
    bool update = false;

    update = ImGui::InputInt(label, &value);

    if (update)
        return wo_ret_option_int(vm, value);
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_int2_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_string_t label = wo_string(args + 0);
    int values[] = { (int)wo_int(args + 1), (int)wo_int(args + 2) };
    bool update = false;

    update = ImGui::InputInt2(label, values);

    if (update)
    {
        wo_value result = s + 0;
        wo_value elem = s + 1;

        wo_set_struct(result, vm, 2);

        wo_set_int(elem, (int)values[0]);
        wo_struct_set(result, 0, elem);
        wo_set_int(elem, (int)values[1]);
        wo_struct_set(result, 1, elem);
        return wo_ret_ok_val(vm, result);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_int3_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_string_t label = wo_string(args + 0);
    int values[] = { (int)wo_int(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3) };
    bool update = false;

    update = ImGui::InputInt3(label, values);

    if (update)
    {
        wo_value result = s + 0;
        wo_value elem = s + 1;

        wo_set_struct(result, vm, 3);

        wo_set_int(elem, (int)values[0]);
        wo_struct_set(result, 0, elem);
        wo_set_int(elem, (int)values[1]);
        wo_struct_set(result, 1, elem);
        wo_set_int(elem, (int)values[2]);
        wo_struct_set(result, 2, elem);
        return wo_ret_ok_val(vm, result);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_int4_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_string_t label = wo_string(args + 0);
    int values[] = { (int)wo_int(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3), (int)wo_int(args + 4) };
    bool update = false;

    update = ImGui::InputInt3(label, values);

    if (update)
    {
        wo_value result = s + 0;
        wo_value elem = s + 1;

        wo_set_struct(result, vm, 4);

        wo_set_int(elem, (int)values[0]);
        wo_struct_set(result, 0, elem);
        wo_set_int(elem, (int)values[1]);
        wo_struct_set(result, 1, elem);
        wo_set_int(elem, (int)values[2]);
        wo_struct_set(result, 2, elem);
        wo_set_int(elem, (int)values[3]);
        wo_struct_set(result, 3, elem);
        return wo_ret_ok_val(vm, result);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_drag_float(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    float value = wo_float(args + 1);
    float speed = wo_float(args + 2);
    float minval = wo_float(args + 3);
    float maxval = wo_float(args + 4);
    if (ImGui::DragFloat(label, &value, speed, minval, maxval))
        return wo_ret_option_float(vm, value);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_float_box(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    float value = wo_float(args + 1);
    bool update = false;

    update = ImGui::InputFloat(label, &value);

    if (update)
        return wo_ret_option_float(vm, value);
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_float2_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2) };
    bool update = false;

    update = ImGui::InputFloat2(label, values);

    if (update)
    {
        return wo_ret_option_val(vm, set_float2_to_struct(s + 0, vm, values[0], values[1]));
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_float3_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2), wo_float(args + 3) };
    bool update = false;

    update = ImGui::InputFloat3(label, values);

    if (update)
    {
        return wo_ret_option_val(vm, set_float3_to_struct(s + 0, vm, values[0], values[1], values[2]));
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_float4_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2), wo_float(args + 3), wo_float(args + 4) };
    bool update = false;

    update = ImGui::InputFloat4(label, values);

    if (update)
    {
        return wo_ret_option_val(vm, set_float4_to_struct(s + 0, vm, values[0], values[1], values[2], values[3]));
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_float_format_box(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    float value = wo_float(args + 1);
    bool update = false;

    wo_string_t format = wo_string(args + 2);
    update = ImGui::InputFloat(label, &value, 0.f, 0.f, format);

    if (update)
        return wo_ret_option_float(vm, value);
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_float2_format_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2) };
    bool update = false;

    wo_string_t format = wo_string(args + 3);
    update = ImGui::InputFloat2(label, values, format);

    if (update)
    {
        return wo_ret_option_val(vm, set_float2_to_struct(s + 0, vm, values[0], values[1]));
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_float3_format_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2), wo_float(args + 3) };
    bool update = false;

    wo_string_t format = wo_string(args + 4);
    update = ImGui::InputFloat3(label, values, format);

    if (update)
    {
        return wo_ret_option_val(vm, set_float3_to_struct(s + 0, vm, values[0], values[1], values[2]));
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_input_float4_format_box(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_string_t label = wo_string(args + 0);
    float values[] = { wo_float(args + 1), wo_float(args + 2), wo_float(args + 3), wo_float(args + 4) };
    bool update = false;

    wo_string_t format = wo_string(args + 5);
    update = ImGui::InputFloat4(label, values, format);

    if (update)
    {
        return wo_ret_option_val(vm, set_float4_to_struct(s + 0, vm, values[0], values[1], values[2], values[3]));
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_text_multiline(wo_vm vm, wo_value args)
{
    std::string buf = wo_string(args + 1);
    bool updated = false;

    updated = ImGui::InputTextMultiline(wo_string(args + 0), &buf);

    if (updated)
        return wo_ret_option_string(vm, buf.c_str());
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_text_multiline_size(wo_vm vm, wo_value args)
{
    std::string buf = wo_string(args + 1);
    bool updated = false;

    updated = ImGui::InputTextMultiline(wo_string(args + 0), &buf,
        ImVec2(wo_float(args + 2), wo_float(args + 3)));

    if (updated)
        return wo_ret_option_string(vm, buf.c_str());
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_combo(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    std::vector<const char*> combo_items;
    wo_value elem = s + 0;
    for (wo_integer_t i = 0; i < wo_lengthof(args + 1); ++i)
    {
        wo_arr_get(elem, args + 1, i);
        combo_items.push_back(wo_string(elem));
    }
    int current_item = (int)wo_int(args + 2);
    auto updated = ImGui::Combo(wo_string(args + 0), &current_item, combo_items.data(), (int)combo_items.size());

    if (updated)
        return wo_ret_option_int(vm, current_item);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_launch(wo_vm vm, wo_value args)
{
    wo_value jobfunc = args + 1;
    wo_value argpacks = args + 2;

    wo_vm vmm = wo_borrow_vm(vm);
    wo_value vmm_s = wo_reserve_stack(vmm, 2, nullptr);

    wo_set_val(vmm_s + 0, jobfunc);
    wo_set_val(vmm_s + 1, argpacks);

    wo_dispatch_value(vmm, args + 0, 2, nullptr, &vmm_s);

    gui_wo_job_coroutine* guico = new gui_wo_job_coroutine;
    guico->work_vm = vmm;
    _wo_new_job_list.add_one(guico);

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_stop_all_work(wo_vm vm, wo_value args)
{
    _je_gui_tls_ctx._jegui_stop_work_flag = true;
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_get_input_state(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);
    jeecs::input::keycode kcode = (jeecs::input::keycode)wo_int(args + 0);

    std::lock_guard g1(_key_state_record_mx);
    auto fnd = _key_state_record.find(kcode);
    if (fnd == _key_state_record.end())
    {
        _key_state_record[kcode] = key_state{
            false,
            jeecs::input::keydown(kcode),
        };

        fnd = _key_state_record.find(kcode);
    }

    wo_value v = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(v, vm, 2);

    wo_set_bool(elem, fnd->second.m_last_frame_down);
    wo_struct_set(v, 0, elem);
    wo_set_bool(elem, fnd->second.m_this_frame_down);
    wo_struct_set(v, 1, elem);
    return wo_ret_val(vm, v);
}

WO_API wo_api je_gui_register_exit_callback(wo_vm vm, wo_value args)
{
    if (_je_gui_static_ctx._jegui_exit_callback_handler_vm != nullptr)
        return wo_ret_panic(vm, "Callback has been registered.");

    assert(
        _je_gui_static_ctx._jegui_exit_callback_handler_vm == nullptr 
        && _je_gui_static_ctx._jegui_exit_callback_function == nullptr);

    _je_gui_static_ctx._jegui_exit_callback_handler_vm = wo_borrow_vm(vm);
    _je_gui_static_ctx._jegui_exit_callback_function = wo_create_pin_value();

    wo_pin_value_set(_je_gui_static_ctx._jegui_exit_callback_function, args + 0);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_unregister_exit_callback(wo_vm vm, wo_value args)
{
    if (_je_gui_static_ctx._jegui_exit_callback_handler_vm == nullptr)
        return wo_ret_panic(vm, "Callback not found.");

    wo_release_vm(_je_gui_static_ctx._jegui_exit_callback_handler_vm);
    wo_close_pin_value(_je_gui_static_ctx._jegui_exit_callback_function);

    _je_gui_static_ctx._jegui_exit_callback_handler_vm = nullptr;
    _je_gui_static_ctx._jegui_exit_callback_function = nullptr;

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_set_font(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    size_t argc = (size_t)wo_argc(vm);

    wo_value elem = s + 0;
    const char* font_path = nullptr;
    const char* latin_font_path = nullptr;

    if (wo_option_get(elem, args + 0))
        font_path = wo_string(elem);
    
    if (wo_option_get(elem, args + 1))
        latin_font_path = wo_string(elem);

    jegui_set_font(font_path, latin_font_path, (size_t)wo_int(args + 2));

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_style_get_config_color(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    ImGuiStyle* style = &ImGui::GetStyle();
    auto color = style->Colors[wo_int(args + 0)];

    return wo_ret_val(vm, set_float4_to_struct(s + 0, vm, color.x, color.y, color.z, color.w));
}

WO_API wo_api je_gui_style_set_config_color(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    ImGuiStyle* style = &ImGui::GetStyle();
    auto& color = style->Colors[wo_int(args + 0)];

    wo_value elem = s + 0;
    wo_struct_get(elem, args + 1, 0);
    color.x = wo_float(elem);
    wo_struct_get(elem, args + 1, 1);
    color.y = wo_float(elem);
    wo_struct_get(elem, args + 1, 2);
    color.z = wo_float(elem);
    wo_struct_get(elem, args + 1, 3);
    color.w = wo_float(elem);
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_style_set_config_color_dark(wo_vm vm, wo_value args)
{
    ImGui::StyleColorsDark();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_style_set_config_color_classic(wo_vm vm, wo_value args)
{
    ImGui::StyleColorsClassic();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_style_set_config_color_light(wo_vm vm, wo_value args)
{
    ImGui::StyleColorsLight();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_dock_space(wo_vm vm, wo_value args)
{
    ImGui::DockSpace((ImGuiID)wo_int(args + 0), val2vec2(args + 1), (ImGuiDockNodeFlags)wo_int(args + 2));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_dock_space_over_viewport(wo_vm vm, wo_value args)
{
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_push_style_real(wo_vm vm, wo_value args)
{
    ImGui::PushStyleVar((ImGuiStyleVar)wo_int(args + 0), wo_float(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_push_style_vec2(wo_vm vm, wo_value args)
{
    ImGui::PushStyleVar((ImGuiStyleVar)wo_int(args + 0), val2vec2(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_pop_style_var(wo_vm vm, wo_value args)
{
    ImGui::PopStyleVar((int)wo_int(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_set_window_font_scale(wo_vm vm, wo_value args)
{
    ImGui::SetWindowFontScale(wo_float(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_dummy(wo_vm vm, wo_value args)
{
    ImGui::Dummy(val2vec2(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_code_editor_language_definition_create(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs = new TextEditor::LanguageDefinition;
    defs->mName = wo_string(args + 0);
    return wo_ret_gchandle(vm, defs, nullptr,
        [](void* p)
        {
            delete std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(p));
        });
}
WO_API wo_api je_gui_code_editor_language_definition_add_keyword(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 0)));

    defs->mKeywords.insert(wo_string(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_language_definition_add_identifier(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 0)));

    TextEditor::Identifier ident;
    ident.mDeclaration = wo_string(args + 2);
    ident.mLocation.mLine = (int)wo_int(args + 3);
    ident.mLocation.mColumn = (int)wo_int(args + 4);

    defs->mIdentifiers.insert(std::make_pair(wo_string(args + 1), ident));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_language_definition_add_token_regex(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 0)));

    defs->mTokenRegexStrings.push_back(
        std::make_pair(wo_string(args + 1), (TextEditor::PaletteIndex)wo_int(args + 2)));

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_language_definition_set_mlcomment(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 0)));

    defs->mCommentStart = wo_string(args + 1);
    defs->mCommentEnd = wo_string(args + 2);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_language_definition_set_slcomment(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 0)));

    defs->mSingleLineComment = wo_string(args + 1);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_language_definition_set_case_sensitive(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 0)));

    defs->mCaseSensitive = wo_bool(args + 1);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_language_definition_set_auto_indentation(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 0)));

    defs->mAutoIndentation = wo_bool(args + 1);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_language_definition_set_preproc_char(wo_vm vm, wo_value args)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 0)));

    defs->mPreprocChar = (char)wo_char(args + 1);

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_code_editor_set_language_definition(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(wo_pointer(args + 1)));

    text_editor->SetLanguageDefinition(*defs);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_undoable(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    return wo_ret_bool(vm, text_editor->CanUndo());
}
WO_API wo_api je_gui_code_editor_redoable(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    return wo_ret_bool(vm, text_editor->CanRedo());
}
WO_API wo_api je_gui_code_editor_undo(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->Undo();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_redo(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->Redo();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_get_cursor_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));

    wo_value result = s + 0;
    wo_value elem = s + 1;

    auto pos = text_editor->GetCursorPosition();

    wo_set_struct(result, vm, 2);

    wo_set_int(elem, pos.mLine);
    wo_struct_set(result, 0, elem);

    wo_set_int(elem, pos.mColumn);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}
WO_API wo_api je_gui_code_editor_set_cursor_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));

    wo_value elem = s + 0;

    wo_struct_get(elem, args + 1, 0);
    int line = (int)wo_int(elem);

    wo_struct_get(elem, args + 1, 1);
    int column = (int)wo_int(elem);

    text_editor->SetCursorPosition(TextEditor::Coordinates(line, column));

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_insert_text(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->InsertText(wo_string(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_copy(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->Copy();

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_cut(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->Cut();

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_paste(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->Paste();

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_delete(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->Delete();

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_code_editor_create(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = new TextEditor();

    text_editor->SetPalette(TextEditor::GetDarkPalette());

    return wo_ret_gchandle(vm, text_editor, nullptr,
        [](void* p)
        {
            delete std::launder(reinterpret_cast<TextEditor*>(p));
        });
}

WO_API wo_api je_gui_code_editor_get_text(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));

    return wo_ret_string(vm, text_editor->GetText().c_str());
}

WO_API wo_api je_gui_code_editor_set_text(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));

    text_editor->SetText(wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_code_editor_show(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->Render(wo_string(args + 0));

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_code_editor_show_size(wo_vm vm, wo_value args)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(wo_pointer(args + 0)));
    text_editor->Render(wo_string(args + 1), val2vec2(args + 2), wo_bool(args + 3));

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_context_create(wo_vm vm, wo_value args)
{
    ax::NodeEditor::Config c;
    c.SettingsFile = nullptr;

    ax::NodeEditor::EditorContext* ctx = ax::NodeEditor::CreateEditor(&c);
    return wo_ret_gchandle(vm, ctx, nullptr,
        [](void* p)
        {
            ax::NodeEditor::DestroyEditor(
                reinterpret_cast<ax::NodeEditor::EditorContext*>(p));
        });
}

WO_API wo_api je_gui_node_editor_begin(wo_vm vm, wo_value args)
{
    ax::NodeEditor::EditorContext* ctx =
        reinterpret_cast<ax::NodeEditor::EditorContext*>(wo_pointer(args + 1));

    ax::NodeEditor::SetCurrentEditor(ctx);
    ax::NodeEditor::Begin(wo_string(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_end(wo_vm vm, wo_value args)
{
    ax::NodeEditor::End();
    ax::NodeEditor::SetCurrentEditor(nullptr);
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_begin_node(wo_vm vm, wo_value args)
{
    ax::NodeEditor::BeginNode((ax::NodeEditor::NodeId)wo_int(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_end_node(wo_vm vm, wo_value args)
{
    ax::NodeEditor::EndNode();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_begin_input_pin(wo_vm vm, wo_value args)
{
    ax::NodeEditor::BeginPin((ax::NodeEditor::PinId)wo_int(args + 0), ax::NodeEditor::PinKind::Input);
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_begin_output_pin(wo_vm vm, wo_value args)
{
    ax::NodeEditor::BeginPin((ax::NodeEditor::PinId)wo_int(args + 0), ax::NodeEditor::PinKind::Output);
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_end_pin(wo_vm vm, wo_value args)
{
    ax::NodeEditor::EndPin();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_pin_pivot_rect(wo_vm vm, wo_value args)
{
    ax::NodeEditor::PinPivotRect(val2vec2(args + 0), val2vec2(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_pin_rect(wo_vm vm, wo_value args)
{
    ax::NodeEditor::PinRect(val2vec2(args + 0), val2vec2(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_link(wo_vm vm, wo_value args)
{
    ax::NodeEditor::Link(
        wo_int(args + 0),
        wo_int(args + 1),
        wo_int(args + 2),
        val2colorf4(args + 3),
        wo_float(args + 4));

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_begin_create(wo_vm vm, wo_value args)
{
    return wo_ret_bool(
        vm,
        ax::NodeEditor::BeginCreate(
            val2colorf4(args + 0),
            wo_float(args + 1)));
}

WO_API wo_api je_gui_node_editor_end_create(wo_vm vm, wo_value args)
{
    ax::NodeEditor::EndCreate();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_begin_delete(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ax::NodeEditor::BeginDelete());
}

WO_API wo_api je_gui_node_editor_end_delete(wo_vm vm, wo_value args)
{
    ax::NodeEditor::EndDelete();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_accept_new_item(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ax::NodeEditor::AcceptNewItem());
}

WO_API wo_api je_gui_node_editor_reject_new_item(wo_vm vm, wo_value args)
{
    ax::NodeEditor::RejectNewItem();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_accept_new_item_color(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ax::NodeEditor::AcceptNewItem(
        val2colorf4(args + 0),
        wo_float(args + 1)));
}

WO_API wo_api je_gui_node_editor_reject_new_item_color(wo_vm vm, wo_value args)
{
    ax::NodeEditor::RejectNewItem(
        val2colorf4(args + 0),
        wo_float(args + 1));
    return wo_ret_void(vm);
}


WO_API wo_api je_gui_node_editor_accept_deleted_item(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ax::NodeEditor::AcceptDeletedItem());
}

WO_API wo_api je_gui_node_editor_reject_deleted_item(wo_vm vm, wo_value args)
{
    ax::NodeEditor::RejectDeletedItem();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_query_new_link(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    ax::NodeEditor::PinId start, end;
    if (ax::NodeEditor::QueryNewLink(&start, &end))
    {
        wo_value v = s + 0;
        wo_value elem = s + 1;

        wo_set_struct(v, vm, 2);

        wo_set_int(elem, (wo_int_t)start.Get());
        wo_struct_set(v, 0, elem);

        wo_set_int(elem, (wo_int_t)end.Get());
        wo_struct_set(v, 1, elem);

        return wo_ret_option_val(vm, v);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_node_editor_query_new_node(wo_vm vm, wo_value args)
{
    ax::NodeEditor::PinId pin;
    if (ax::NodeEditor::QueryNewNode(&pin))
    {
        return wo_ret_option_int(vm, (wo_int_t)pin.Get());
    }

    return wo_ret_option_none(vm);

}

WO_API wo_api je_gui_node_editor_query_deleted_node(wo_vm vm, wo_value args)
{
    ax::NodeEditor::NodeId node;
    if (ax::NodeEditor::QueryDeletedNode(&node))
    {
        return wo_ret_option_int(vm, (wo_int_t)node.Get());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_node_editor_query_deleted_link(wo_vm vm, wo_value args)
{
    ax::NodeEditor::LinkId link;
    if (ax::NodeEditor::QueryDeletedLink(&link))
    {
        return wo_ret_option_int(vm, (wo_int_t)link.Get());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_node_editor_push_style_color(wo_vm vm, wo_value args)
{
    ax::NodeEditor::PushStyleColor(
        (ax::NodeEditor::StyleColor)wo_int(args + 0),
        val2colorf4(args + 1));

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_pop_style_color(wo_vm vm, wo_value args)
{
    ax::NodeEditor::PopStyleColor((int)wo_int(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_delete_node(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ax::NodeEditor::DeleteNode((ax::NodeEditor::NodeId)wo_int(args + 0)));
}

WO_API wo_api je_gui_node_editor_delete_link(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ax::NodeEditor::DeleteLink((ax::NodeEditor::LinkId)wo_int(args + 0)));
}

WO_API wo_api je_gui_node_editor_get_node_position(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto v2 = ax::NodeEditor::GetNodePosition((ax::NodeEditor::NodeId)wo_int(args + 0));

    wo_value result = s + 0;
    wo_value val = s + 1;

    wo_set_struct(result, vm, 2);

    wo_set_float(val, v2.x);
    wo_struct_set(result, 0, val);

    wo_set_float(val, v2.y);
    wo_struct_set(result, 1, val);

    return wo_ret_val(vm, result);
}

WO_API wo_api je_gui_node_editor_set_node_position(wo_vm vm, wo_value args)
{
    ax::NodeEditor::SetNodePosition((ax::NodeEditor::NodeId)wo_int(args + 0), val2vec2(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_get_hovered_node(wo_vm vm, wo_value args)
{
    auto id = ax::NodeEditor::GetHoveredNode();

    if ((bool)id)
        return wo_ret_option_int(vm, (wo_int_t)id.Get());
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_node_editor_get_hovered_pin(wo_vm vm, wo_value args)
{
    auto id = ax::NodeEditor::GetHoveredPin();

    if ((bool)id)
        return wo_ret_option_int(vm, (wo_int_t)id.Get());
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_node_editor_get_hovered_link(wo_vm vm, wo_value args)
{
    auto id = ax::NodeEditor::GetHoveredLink();

    if ((bool)id)
        return wo_ret_option_int(vm, (wo_int_t)id.Get());
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_node_editor_is_node_selected(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm,
        ax::NodeEditor::IsNodeSelected(
            (ax::NodeEditor::NodeId)wo_int(args + 0)));
}

WO_API wo_api je_gui_node_editor_canvas_to_screen(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto v2 = ax::NodeEditor::CanvasToScreen(val2vec2(args + 0));

    wo_value result = s + 0;
    wo_value val = s + 1;

    wo_set_struct(result, vm, 2);

    wo_set_float(val, v2.x);
    wo_struct_set(result, 0, val);

    wo_set_float(val, v2.y);
    wo_struct_set(result, 1, val);

    return wo_ret_val(vm, result);
}

WO_API wo_api je_gui_node_editor_screen_to_canvas(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto v2 = ax::NodeEditor::ScreenToCanvas(val2vec2(args + 0));

    wo_value result = s + 0;
    wo_value val = s + 1;

    wo_set_struct(result, vm, 2);

    wo_set_float(val, v2.x);
    wo_struct_set(result, 0, val);

    wo_set_float(val, v2.y);
    wo_struct_set(result, 1, val);

    return wo_ret_val(vm, result);
}

WO_API wo_api je_gui_node_editor_suspend(wo_vm vm, wo_value args)
{
    ax::NodeEditor::Suspend();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_node_editor_resume(wo_vm vm, wo_value args)
{
    ax::NodeEditor::Resume();
    return wo_ret_void(vm);
}

////////////////////////////////////////////////////////////////////////////////////////////

void jegui_init_basic(
    bool need_flip_frame_buf,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler)
{
    _je_gui_tls_ctx._jegui_need_flip_frambuf = need_flip_frame_buf;
    _je_gui_tls_ctx._jegl_get_native_texture = get_img_res;
    _je_gui_tls_ctx._jegl_bind_shader_sampler_state = apply_shader_sampler;

    _je_gui_tls_ctx._jegl_rend_texture_shader = jeecs::graphic::shader::create(
        "!/builtin/imgui_image_displayer.shader",
        R"(
import je::shader;
ZTEST   (OFF);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (NONE);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    color: float4,
};

public func vert(v: vin)
{
    return v2f{
        pos = vec4(v.vertex, 1.),
        uv = v.uv,
    };
}
public func frag(vf: v2f)
{
    let nearest_clamp = sampler2d::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
    let Main = uniform_texture:<texture2d>("Main", nearest_clamp, 0);
    return fout{
        color = texture(Main, vf.uv)
    };
}
)");

    _je_gui_tls_ctx._jegui_imgui_config_path =
        jeecs_file_get_host_path() + std::string("/builtin/imgui.ini.je4cache");

    _je_gui_tls_ctx._jegui_stop_work_flag = false;
    ImGui::CreateContext();

    // Set style:
    // ImGui::StyleColorsLight();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = _je_gui_tls_ctx._jegui_imgui_config_path.c_str();

    auto* general_ttf_file =
        _je_gui_static_ctx._jegui_specify_font_path.has_value()
        ? jeecs_file_open(_je_gui_static_ctx._jegui_specify_font_path.value().c_str())
        : nullptr;
    auto* latin_ttf_file =
        _je_gui_static_ctx._jegui_specify_latin_font_path.has_value()
        ? jeecs_file_open(_je_gui_static_ctx._jegui_specify_latin_font_path.value().c_str())
        : nullptr;

    if (general_ttf_file == nullptr)
        // Default font
        general_ttf_file = jeecs_file_open("!/builtin/font/HarmonyOS_Sans_SC_Regular.ttf");

    if (latin_ttf_file == nullptr)
        // Default font
        latin_ttf_file = jeecs_file_open("!/builtin/font/JetBrainsMono_Regular.ttf");

    if (general_ttf_file)
    {
        void* file_buf = je_mem_alloc(general_ttf_file->m_file_length);
        jeecs_file_read(file_buf, sizeof(char), general_ttf_file->m_file_length, general_ttf_file);

        void* latin_file_buf = nullptr;
        if (latin_ttf_file != nullptr)
        {
            latin_file_buf = je_mem_alloc(latin_ttf_file->m_file_length);
            jeecs_file_read(latin_file_buf, sizeof(char), latin_ttf_file->m_file_length, latin_ttf_file);
        }

        if (latin_file_buf != nullptr)
        {
            // Non latin characters
            static const ImWchar ranges[] =
            {
                // 0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x2000, 0x206F, // General Punctuation
                0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
                0x31F0, 0x31FF, // Katakana Phonetic Extensions
                0xFF00, 0xFFEF, // Half-width characters
                0xFFFD, 0xFFFD, // Invalid
                0x4e00, 0x9FAF, // CJK Ideograms
                0,
            };

            io.Fonts->AddFontFromMemoryTTF(
                latin_file_buf,
                (int)latin_ttf_file->m_file_length,
                (float)_je_gui_static_ctx._jegui_specify_font_size,
                nullptr,
                io.Fonts->GetGlyphRangesDefault());
            
            ImFontConfig merge_config;
            merge_config.MergeMode = true;

            io.Fonts->AddFontFromMemoryTTF(
                file_buf,
                (int)general_ttf_file->m_file_length,
                (float)_je_gui_static_ctx._jegui_specify_font_size,
                &merge_config,
                ranges);

            io.Fonts->Build();
        }
        else
            io.Fonts->AddFontFromMemoryTTF(
                file_buf,
                (int)general_ttf_file->m_file_length,
                (float)_je_gui_static_ctx._jegui_specify_font_size,
                nullptr,
                io.Fonts->GetGlyphRangesChineseFull());

        // je_mem_free(file_buf); // No need to free.
        jeecs_file_close(general_ttf_file);
    }
}

void jegui_update_basic()
{
    jegl_using_resource(_je_gui_tls_ctx._jegl_rend_texture_shader->resouce());

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
        std::lock_guard g1(_key_state_record_mx);
        for (auto& [k, v] : _key_state_record)
        {
            v.m_last_frame_down = v.m_this_frame_down;
            v.m_this_frame_down = jeecs::input::keydown(k);
        }
    } while (0);

    if (!_je_gui_tls_ctx._jegui_stop_work_flag)
    {
        auto chain = _wo_job_list.pick_all();
        while (chain)
        {
            auto cur_job = chain;
            chain = chain->last;

            auto result = wo_dispatch(cur_job->work_vm, nullptr, nullptr);
            if (result == WO_CONTINUE)
            {
                _wo_job_list.add_one(cur_job);
                continue;
            }

            wo_release_vm(cur_job->work_vm);
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
}
void jegui_shutdown_basic(bool reboot)
{
    if (!reboot)
    {
        auto chain = _wo_job_list.pick_all();
        while (chain)
        {
            auto cur_job = chain;
            chain = chain->last;

            wo_release_vm(cur_job->work_vm);
            delete cur_job;
        }

        auto new_job_chain = _wo_new_job_list.pick_all();
        while (new_job_chain)
        {
            auto cur_job = new_job_chain;
            new_job_chain = new_job_chain->last;

            wo_release_vm(cur_job->work_vm);
            delete cur_job;
        }
    }
}

bool jegui_shutdown_callback()
{
#ifndef NDEBUG
    jeecs::debug::loginfo("Graphic interface has been requested to close.");
#endif

    if (_je_gui_static_ctx._jegui_exit_callback_handler_vm == nullptr)
        return true;

    wo_value tmp = wo_register(_je_gui_static_ctx._jegui_exit_callback_handler_vm, WO_REG_T0);

    wo_pin_value_get(tmp, _je_gui_static_ctx._jegui_exit_callback_function);
    auto result = wo_invoke_value(_je_gui_static_ctx._jegui_exit_callback_handler_vm, tmp, 0, nullptr, nullptr);
    if (result == nullptr)
        return false;

    return wo_bool(result);
}
