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
        ImGuiWindowFlags_NoNavInputs            = 0x0002'0000,  // No gamepad/keyboard navigation within the window
        ImGuiWindowFlags_NoNavFocus             = 0x0004'0000,  // No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by CTRL+TAB)
        ImGuiWindowFlags_UnsavedDocument        = 0x0008'0000,  // Display a dot next to the title. When used in a tab/docking context, tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.
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
    extern("libjoyecs", "je_gui_begin")
    public func BeginAttr(title:string, attribute:WindowsAttribute)=> bool;
    extern("libjoyecs", "je_gui_begin_open")
    public func BeginAttrOpen(title:string, attribute:WindowsAttribute)=> bool;
    public func BeginOpen(title:string)
    {
        return BeginAttrOpen(title, WindowsAttribute::ImGuiWindowFlags_None);
    }

    extern("libjoyecs", "je_gui_end")
    public func End()=> void;

    extern("libjoyecs", "je_gui_begin_child")
    public func BeginChild(title:string)=> bool;
    extern("libjoyecs", "je_gui_begin_child")
    public func BeginChildAttr(title:string, attr: WindowsAttribute)=> bool;
    extern("libjoyecs", "je_gui_begin_child")
    public func BeginChildSize(title:string, sizex: real, sizey: real)=> bool;

    extern("libjoyecs", "je_gui_end_child")
    public func EndChild()=> void;

    extern("libjoyecs", "je_gui_progress_bar")
    public func ProgressBar(f: real)=> void;

    extern("libjoyecs", "je_gui_text")
    public func Text(msg:string)=> void;
    extern("libjoyecs", "je_gui_text_disabled")
    public func TextDisabled(msg:string)=> void;

    extern("libjoyecs", "je_gui_button")
    public func Button(msg:string)=> bool;
    extern("libjoyecs", "je_gui_button")
    public func ButtonSize(msg:string, size: ImVec2)=> bool;

    extern("libjoyecs", "je_gui_invisible_button")
    public func InvisibleButton(msg:string, size: ImVec2)=> bool;

    extern("libjoyecs", "je_gui_begin_main_menu_bar")
    public func BeginMainMenuBar()=> bool;

    extern("libjoyecs", "je_gui_menu_item")
    public func MenuItem(text:string)=> bool;
    extern("libjoyecs", "je_gui_menu_item")
    public func MenuItemShortcut(text:string, shortcut:string)=> bool;
    extern("libjoyecs", "je_gui_menu_item")
    public func MenuItemEnabled(text:string, enable:bool)=> bool;
    extern("libjoyecs", "je_gui_menu_item")
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
    extern("libjoyecs", "je_gui_begin_menu")
    public func BeginMenuEnabled(text:string, enable:bool)=> bool;

    extern("libjoyecs", "je_gui_listbox")
    public func ListBox(label:string, items:array<string>)=> option<int>;
    extern("libjoyecs", "je_gui_listbox")
    public func ListBoxSelect(label:string, items:array<string>, select_item:int)=> option<int>;
    extern("libjoyecs", "je_gui_listbox")
    public func ListBoxSelectHeight(label:string, items:array<string>, select_item:int, height_count:int)=> option<int>;
    extern("libjoyecs", "je_gui_listbox_withsize")
    public func ListBoxSelectSize(label:string, items:array<string>, select_item:int, width:real, height:real)=> option<int>;

    extern("libjoyecs", "je_gui_begin_listbox")
    public func BeginListBox(label:string, width:real, height:real)=>bool;
    extern("libjoyecs", "je_gui_begin_selectable")
    public func Selectable(label:string)=>bool;
    extern("libjoyecs", "je_gui_begin_selectable")
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

    extern("libjoyecs", "je_gui_is_item_active")
    public func IsItemActive()=> bool;

    extern("libjoyecs", "je_gui_is_mouse_dragging")
    public func IsMouseDragging(attr: ImGuiMouseButton)=> bool;

    extern("libjoyecs", "je_gui_set_tooltip")
    public func SetTooltip(msg: string)=> bool;

    extern("libjoyecs", "je_gui_beginpopup_contextitem")
    public func BeginPopupContextItemLabel(label:string)=>bool;
    extern("libjoyecs", "je_gui_beginpopup_contextitem")
    public func BeginPopupContextItem()=>bool;

    extern("libjoyecs", "je_gui_beginpopup_contextwindow")
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
    extern("libjoyecs", "je_gui_openpopup_on_item_click")
    public func OpenPopupOnItemClickAttr(flag:PopupAttribute)=>void;
    extern("libjoyecs", "je_gui_openpopup_on_item_click")
    public func OpenPopupOnItemClickLabel(label:string)=>void;
    extern("libjoyecs", "je_gui_openpopup_on_item_click")
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

    extern("libjoyecs", "je_gui_checkbox")
    public func CheckBox(label: string, checked: bool)=> option<bool>;

    extern("libjoyecs", "je_gui_colorpicker4")
    public func ColorPicker4(label:string, color: (real, real, real, real))=> option<(real, real, real, real)>;

    extern("libjoyecs", "je_gui_colorbutton")
    public func ColorButton(label:string, color: (real, real, real, real))=> bool;

    extern("libjoyecs", "je_gui_input_text_box")
    public func InputText(label:string, buffer: string)=> option<string>;

    extern("libjoyecs", "je_gui_input_float_box")
    public func InputFloat(label: string, value: real)=> option<real>;

    extern("libjoyecs", "je_gui_input_float_box")
    public func InputFloatFormat(label: string, value: real, format: string)=> option<real>;

    extern("libjoyecs", "je_gui_input_int_box")
    public func InputInt(label: string, value: int)=> option<int>;

    extern("libjoyecs", "je_gui_input_float2_box")
    public func InputFloat2(label: string, x: real, y: real)=> option<(real, real)>;
    extern("libjoyecs", "je_gui_input_float2_box")
    public func InputFloat2Format(label: string, x: real, y: real, format: string)=> option<(real, real)>;

    extern("libjoyecs", "je_gui_input_float3_box")
    public func InputFloat3(label: string, x: real, y: real, z: real)=> option<(real, real, real)>;
    extern("libjoyecs", "je_gui_input_float3_box")
    public func InputFloat3Format(label: string, x: real, y: real, z: real, format: string)=> option<(real, real, real)>;

    extern("libjoyecs", "je_gui_input_float4_box")
    public func InputFloat4(label: string, x: real, y: real, z: real, w: real)=> option<(real, real, real, real)>;
    extern("libjoyecs", "je_gui_input_float4_box")
    public func InputFloat4Format(label: string, x: real, y: real, z: real, w: real, format: string)=> option<(real, real, real, real)>;

    extern("libjoyecs", "je_gui_input_text_multiline")
    public func InputTextMultiline(label:string, buffer: string)=> option<string>;
    extern("libjoyecs", "je_gui_input_text_multiline")
    public func InputTextMultilineSize(label:string, buffer: string, width:real, height:real)=> option<string>;

    extern("libjoyecs", "je_gui_combo")
    public func Combo(label:string, items: array<string>, select_item: int)=> option<int>;

    extern("libjoyecs", "je_gui_end_menu")
    public func EndMenu()=> void;

    extern("libjoyecs", "je_gui_separator")
    public func Separator()=> void;

    extern("libjoyecs", "je_gui_image")
    public func Image(tex: je::graphic::texture)=> void;
    extern("libjoyecs", "je_gui_image")
    public func ImageScale(tex: je::graphic::texture, scale: real)=> void;
    extern("libjoyecs", "je_gui_image")
    public func ImageSize(tex: je::graphic::texture, width: real, height: real)=> void;

)"
R"(
    extern("libjoyecs", "je_gui_push_clip_rect")
    public func PushClipRect(from: ImVec2, to: ImVec2)=> void;

    extern("libjoyecs", "je_gui_pop_clip_rect")
    public func PopClipRect()=> void;

    using DrawListT = handle
    {
        extern("libjoyecs", "je_gui_draw_list_add_rect_filled")
        public func AddRectFilled(self: DrawListT, from: ImVec2, to: ImVec2, color: Color32RGBA)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_text")
        public func AddText(self: DrawListT, pos: ImVec2, color: Color32RGBA, text: string)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_image")
        public func AddImage(self: DrawListT, from: ImVec2, to: ImVec2, tex: graphic::texture)=> void;

        extern("libjoyecs", "je_gui_draw_list_add_line")
        public func AddLine(self: DrawListT, from: ImVec2, to: ImVec2, color: Color32RGBA)=> void;
    }
    extern("libjoyecs", "je_gui_get_window_draw_list")
    public func GetWindowDrawList()=> DrawListT;

    extern("libjoyecs", "je_gui_imagebutton")
    public func ImageButton(tex: je::graphic::texture)=> bool;
    extern("libjoyecs", "je_gui_imagebutton")
    public func ImageButtonScale(tex: je::graphic::texture, scale: real)=> bool;
    extern("libjoyecs", "je_gui_imagebutton")
    public func ImageButtonSize(tex: je::graphic::texture, width: real, height: real)=> bool;

    extern("libjoyecs", "je_gui_content_region_avail")
    public func GetContentRegionAvail()=> ImVec2;

    extern("libjoyecs", "je_gui_set_next_window_size_constraints")
    public func SetNextWindowSizeConstraints(minsz: ImVec2, maxsz: ImVec2)=> void;

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
    extern("libjoyecs", "je_gui_begin_drag_drop_source")
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
                ->  forall(\accept_type_result = accept_result->has
                    where (_, accept_result) = accept_type_result;)
                ->  get(0)
                ->> \e = (e[0], e[1]->val);
                ;

            EndDragDropTarget();

            return drag_result;
        }
        return option::none;
    }

    extern("libjoyecs", "je_gui_set_next_item_open")
    public func SetNextItemOpen(open: bool)=> void;

    //
    extern("libjoyecs", "je_gui_focus_on_any_windows")
    public func AnyWindowsFocused()=> bool;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    extern("libjoyecs", "je_gui_launch")
    private func _launch<LT, FT, ATs>(coloop:LT, job_func:FT, args: ATs)=> void
        where coloop(job_func, args) is void;

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
}

)";

bool any_window_focused_on = false;
bool next_frame_any_window_focused_on = false;
WO_API wo_api je_gui_focus_on_any_windows(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, any_window_focused_on || ImGui::GetFocusID() != 0);
}
WO_API wo_api je_gui_begin_drag_drop_source(wo_vm vm, wo_value args, size_t argc)
{
    bool result;
    if (argc == 0)
        result = ImGui::BeginDragDropSource(ImGuiDragDropFlags_None);
    else
        result = ImGui::BeginDragDropSource((ImGuiDragDropFlags)wo_int(args + 0));
    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_set_drag_drop_payload(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t buf = wo_string(args + 1);

    bool result = ImGui::SetDragDropPayload(wo_string(args + 0), buf, strlen(buf) + 1);
    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_end_drag_drop_source(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndDragDropSource();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begin_drag_drop_target(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::BeginDragDropTarget());
}
WO_API wo_api je_gui_accept_drag_drop_payload(wo_vm vm, wo_value args, size_t argc)
{
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(wo_string(args + 0)))
    {
        return wo_ret_option_string(vm, (const char*)payload->Data);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api je_gui_end_accept_drop_source(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndDragDropTarget();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_set_next_item_open(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::SetNextItemOpen(wo_bool(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_set_clip_board_text(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::SetClipboardText(wo_string(args));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_get_clip_board_text(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, ImGui::GetClipboardText());
}

//
WO_API wo_api je_gui_get_window_pos(wo_vm vm, wo_value args, size_t argc)
{
    auto&& wpos = ImGui::GetWindowPos();

    wo_value ret = wo_push_struct(vm, 2);
    wo_set_float(wo_struct_get(ret, 0), wpos.x);
    wo_set_float(wo_struct_get(ret, 1), wpos.y);

    return wo_ret_val(vm, ret);
}

WO_API wo_api je_gui_get_mouse_pos(wo_vm vm, wo_value args, size_t argc)
{
    auto&& mpos = ImGui::GetMousePos();

    wo_value ret = wo_push_struct(vm, 2);
    wo_set_float(wo_struct_get(ret, 0), mpos.x);
    wo_set_float(wo_struct_get(ret, 1), mpos.y);

    return wo_ret_val(vm, ret);
}

WO_API wo_api je_gui_get_mouse_delta_pos(wo_vm vm, wo_value args, size_t argc)
{
    auto&& mdpos = ImGui::GetIO().MouseDelta;

    wo_value ret = wo_push_struct(vm, 2);
    wo_set_float(wo_struct_get(ret, 0), mdpos.x);
    wo_set_float(wo_struct_get(ret, 1), mdpos.y);

    return wo_ret_val(vm, ret);
}

WO_API wo_api je_gui_get_cursor_pos(wo_vm vm, wo_value args, size_t argc)
{
    auto&& cpos = ImGui::GetCursorPos();

    wo_value ret = wo_push_struct(vm, 2);
    wo_set_float(wo_struct_get(ret, 0), cpos.x);
    wo_set_float(wo_struct_get(ret, 1), cpos.y);

    return wo_ret_val(vm, ret);
}
WO_API wo_api je_gui_get_item_rect_size(wo_vm vm, wo_value args, size_t argc)
{
    auto&& isize = ImGui::GetItemRectSize();

    wo_value ret = wo_push_struct(vm, 2);
    wo_set_float(wo_struct_get(ret, 0), isize.x);
    wo_set_float(wo_struct_get(ret, 1), isize.y);

    return wo_ret_val(vm, ret);
}

WO_API wo_api je_gui_get_item_rect(wo_vm vm, wo_value args, size_t argc)
{
    auto&& isizemin = ImGui::GetItemRectMin();
    auto&& isizemax = ImGui::GetItemRectMax();

    wo_value ret = wo_push_struct(vm, 2);

    wo_value minpos = wo_struct_get(ret, 0);
    wo_value maxpos = wo_struct_get(ret, 1);

    wo_set_struct(minpos, 2);
    wo_set_struct(maxpos, 2);

    wo_set_float(wo_struct_get(minpos, 0), isizemin.x);
    wo_set_float(wo_struct_get(minpos, 1), isizemin.y);

    wo_set_float(wo_struct_get(maxpos, 0), isizemax.x);
    wo_set_float(wo_struct_get(maxpos, 1), isizemax.y);

    return wo_ret_val(vm, ret);
}

WO_API wo_api je_gui_push_id_str(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::PushID(wo_string(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_push_id(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::PushID((int)wo_int(args + 0));
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

WO_API wo_api je_gui_begintabbar(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::BeginTabBar(wo_string(args + 0), (ImGuiTabBarFlags)wo_int(args + 1)));
}

WO_API wo_api je_gui_endtabbar(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndTabBar();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_begintabitem(wo_vm vm, wo_value args, size_t argc)
{
    bool display = ImGui::BeginTabItem(wo_string(args + 0), nullptr, (ImGuiTabBarFlags)wo_int(args + 1));
    return wo_ret_bool(vm, display);
}

WO_API wo_api je_gui_begintabitem_open(wo_vm vm, wo_value args, size_t argc)
{
    bool open = true;
    bool display = ImGui::BeginTabItem(wo_string(args + 0), &open, (ImGuiTabBarFlags)wo_int(args + 1));

    wo_value ret = wo_push_struct(vm, 2);
    wo_set_bool(wo_struct_get(ret, 0), display);
    wo_set_bool(wo_struct_get(ret, 1), open);

    return wo_ret_val(vm, ret);
}

ImVec2 val2vec2(wo_value v)
{
    return ImVec2(wo_float(wo_struct_get(v, 0)), wo_float(wo_struct_get(v, 1)));
}
ImVec4 val2vec4(wo_value v)
{
    return ImVec4(
        wo_float(wo_struct_get(v, 0)),
        wo_float(wo_struct_get(v, 1)),
        wo_float(wo_struct_get(v, 2)),
        wo_float(wo_struct_get(v, 3)));
}

WO_API wo_api je_gui_push_clip_rect(wo_vm vm, wo_value args, size_t argc)
{
    wo_value from = args + 0;
    wo_value to = args + 1;
    ImGui::PushClipRect(
        val2vec2(from),
        val2vec2(to),
        true);

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_pop_clip_rect(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::PopClipRect();

    return wo_ret_void(vm);
}

ImU32 val2color32(wo_value v)
{
    return IM_COL32(
        wo_int(wo_struct_get(v, 0)),
        wo_int(wo_struct_get(v, 1)),
        wo_int(wo_struct_get(v, 2)),
        wo_int(wo_struct_get(v, 3)));
}

WO_API wo_api je_gui_get_window_draw_list(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_pointer(vm, ImGui::GetWindowDrawList());
}

WO_API wo_api je_gui_draw_list_add_rect_filled(wo_vm vm, wo_value args, size_t argc)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddRectFilled(val2vec2(args + 1), val2vec2(args + 2), val2color32(args + 3));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_draw_list_add_text(wo_vm vm, wo_value args, size_t argc)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddText(val2vec2(args + 1), val2color32(args + 2), wo_string(args + 3));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_draw_list_add_image(wo_vm vm, wo_value args, size_t argc)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);

    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 3);
    jegl_using_resource((*texture)->resouce());

    list->AddImage((ImTextureID)(intptr_t)(*texture)->resouce()->m_uint1, val2vec2(args + 1), val2vec2(args + 2), ImVec2(0, 1), ImVec2(1, 0));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_draw_list_add_line(wo_vm vm, wo_value args, size_t argc)
{
    ImDrawList* list = (ImDrawList*)wo_pointer(args + 0);
    list->AddLine(val2vec2(args + 1), val2vec2(args + 2), val2color32(args + 3));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_endtabitem(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndTabItem();
    return wo_ret_void(vm);
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
            ImGui::OpenPopupOnItemClick(nullptr, (ImGuiPopupFlags)wo_int(args + 0));
    }
    else
        ImGui::OpenPopupOnItemClick(wo_string(args + 0), (ImGuiPopupFlags)wo_int(args + 1));
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
    return wo_ret_bool(vm, ImGui::IsItemClicked((ImGuiMouseButton)wo_int(args + 0)));
}

WO_API wo_api je_gui_is_itemtoggledopen(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::IsItemToggledOpen());
}

WO_API wo_api je_gui_is_itemhovered(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::IsItemHovered());
}

WO_API wo_api je_gui_is_item_active(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::IsItemActive());
}

WO_API wo_api je_gui_is_mouse_dragging(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::IsMouseDragging((ImGuiMouseButton)wo_int(args + 0)));
}

WO_API wo_api je_gui_set_tooltip(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::SetTooltip("%s", wo_string(args + 0));
    return wo_ret_void(vm);
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

    bool val_changed = ImGui::ListBox(wo_string(args + 0), &selected_item, items.data(), (int)items.size(), max_height_item);

    if (val_changed)
        return wo_ret_option_int(vm, selected_item);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_listbox_withsize(wo_vm vm, wo_value args, size_t argc)
{
    //func ListBox(label:string, items:array<string>, ref select_item:int, width:real, height:real)=> int;
    int origin_selected_index = (int)wo_int(args + 2);
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

WO_API wo_api je_gui_job_vm_handle(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_pointer(vm, vm);
}

WO_API wo_api je_gui_begin(wo_vm vm, wo_value args, size_t argc)
{
    bool windows_flag = false;
    if (argc == 3)
    {
        bool showing = true;
        ImGui::Begin(wo_string(args), &showing, (ImGuiWindowFlags)wo_int(args + 1));
        windows_flag = showing;
    }
    else if (argc == 2)
        windows_flag = ImGui::Begin(wo_string(args), 0, (ImGuiWindowFlags)wo_int(args + 1));
    else
        windows_flag = ImGui::Begin(wo_string(args));

    if (ImGui::IsWindowFocused())
        next_frame_any_window_focused_on = true;

    return wo_ret_bool(vm, windows_flag);
}
WO_API wo_api je_gui_begin_open(wo_vm vm, wo_value args, size_t argc)
{
    bool windows_flag = false;

    bool showing = true;
    ImGui::Begin(wo_string(args), &showing, (ImGuiWindowFlags)wo_int(args + 1));
    windows_flag = showing;

    if (ImGui::IsWindowFocused())
        next_frame_any_window_focused_on = true;

    return wo_ret_bool(vm, windows_flag);
}
WO_API wo_api je_gui_end(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::End();
    return wo_ret_void(vm);
}
WO_API wo_api je_gui_begin_child(wo_vm vm, wo_value args, size_t argc)
{
    if (argc == 3)
    {
        return wo_ret_bool(vm, ImGui::BeginChild(wo_string(args + 0),
            ImVec2(wo_float(args + 1), wo_float(args + 2)), true));
    }
    else if (argc == 2)
    {
        return wo_ret_bool(vm, ImGui::BeginChild(wo_string(args + 0),
            ImVec2(0.f, 0.f), true, (ImGuiWindowFlags)wo_int(args + 1)));
    }
    return wo_ret_bool(vm, ImGui::BeginChild(wo_string(args + 0), ImVec2(0.f, 0.f), true));
}
WO_API wo_api je_gui_end_child(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::EndChild();
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_progress_bar(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::ProgressBar(wo_float(args + 0));
    return wo_ret_void(vm);
}


WO_API wo_api je_gui_text(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::Text("%s", wo_string(args));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_text_disabled(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::TextDisabled("%s", wo_string(args));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_button(wo_vm vm, wo_value args, size_t argc)
{
    if (argc == 2)
        return wo_ret_bool(vm, ImGui::Button(wo_string(args + 0), val2vec2(args + 1)));
    return wo_ret_bool(vm, ImGui::Button(wo_string(args + 0)));
}

WO_API wo_api je_gui_invisible_button(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::InvisibleButton(wo_string(args + 0), val2vec2(args + 1)));
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

WO_API wo_api je_gui_menu_item_selected(wo_vm vm, wo_value args, size_t argc)
{
    bool selected = wo_bool(args + 2);
    bool clicked = ImGui::MenuItem(wo_string(args + 0), wo_string(args + 1), &selected, wo_bool(args + 3));
    wo_set_bool(args + 2, selected);

    if (clicked)
        return wo_ret_option_bool(vm, selected);
    return wo_ret_option_none(vm);
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

WO_API wo_api je_gui_image(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resouce());

    if (argc == 1)
        ImGui::Image((ImTextureID)(intptr_t)((*texture)->resouce())->m_uint1,
            ImVec2(
                (float)((*texture)->resouce())->m_raw_texture_data->m_width,
                (float)((*texture)->resouce())->m_raw_texture_data->m_height
            ), ImVec2(0, 1), ImVec2(1, 0));
    else if (argc == 2)
        ImGui::Image((ImTextureID)(intptr_t)((*texture)->resouce())->m_uint1,
            ImVec2(
                ((*texture)->resouce())->m_raw_texture_data->m_width * wo_float(args + 1),
                ((*texture)->resouce())->m_raw_texture_data->m_height * wo_float(args + 1)
            ), ImVec2(0, 1), ImVec2(1, 0));
    else if (argc == 3)
        ImGui::Image((ImTextureID)(intptr_t)((*texture)->resouce())->m_uint1,
            ImVec2(
                wo_float(args + 1),
                wo_float(args + 2)
            ), ImVec2(0, 1), ImVec2(1, 0));

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_imagebutton(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resouce());
    bool result = false;
    if (argc == 1)
        result = ImGui::ImageButton((ImTextureID)(intptr_t)((*texture)->resouce())->m_uint1,
            ImVec2(
                (float)((*texture)->resouce())->m_raw_texture_data->m_width,
                (float)((*texture)->resouce())->m_raw_texture_data->m_height
            ), ImVec2(0, 1), ImVec2(1, 0));
    else if (argc == 2)
        result = ImGui::ImageButton((ImTextureID)(intptr_t)((*texture)->resouce())->m_uint1,
            ImVec2(
                ((*texture)->resouce())->m_raw_texture_data->m_width * wo_float(args + 1),
                ((*texture)->resouce())->m_raw_texture_data->m_height * wo_float(args + 1)
            ), ImVec2(0, 1), ImVec2(1, 0));
    else if (argc == 3)
        result = ImGui::ImageButton((ImTextureID)(intptr_t)((*texture)->resouce())->m_uint1,
            ImVec2(
                wo_float(args + 1),
                wo_float(args + 2)
            ), ImVec2(0, 1), ImVec2(1, 0));

    return wo_ret_bool(vm, result);
}

WO_API wo_api je_gui_content_region_avail(wo_vm vm, wo_value args, size_t argc)
{
    auto&& sz = ImGui::GetContentRegionAvail();

    wo_value result = wo_push_struct(vm, 2);

    wo_set_float(wo_struct_get(result, 0), sz.x);
    wo_set_float(wo_struct_get(result, 1), sz.y);

    return wo_ret_val(vm, result);
}

WO_API wo_api je_gui_set_next_window_size_constraints(wo_vm vm, wo_value args, size_t argc)
{
    ImGui::SetNextWindowSizeConstraints(val2vec2(args + 0), val2vec2(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api je_gui_colorbutton(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, ImGui::ColorButton(wo_string(args + 0), val2vec4(args + 1)));
}
WO_API wo_api je_gui_colorpicker4(wo_vm vm, wo_value args, size_t argc)
{
    float rgba[4] = {
        wo_float(wo_struct_get(args + 1,0)),
        wo_float(wo_struct_get(args + 1,1)),
        wo_float(wo_struct_get(args + 1,2)),
        wo_float(wo_struct_get(args + 1,3)), };

    if (ImGui::ColorPicker4(wo_string(args + 0), rgba))
    {
        wo_value result = wo_push_struct(vm, 4);
        wo_set_float(wo_struct_get(result, 0), rgba[0]);
        wo_set_float(wo_struct_get(result, 1), rgba[1]);
        wo_set_float(wo_struct_get(result, 2), rgba[2]);
        wo_set_float(wo_struct_get(result, 3), rgba[3]);
        return wo_ret_option_val(vm, result);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_text_box(wo_vm vm, wo_value args, size_t argc)
{
    std::string buf = wo_string(args + 1);
    if (ImGui::InputText(wo_string(args + 0), &buf))
    {
        return wo_ret_option_string(vm, buf.c_str());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_checkbox(wo_vm vm, wo_value args, size_t argc)
{
    bool checked = wo_bool(args + 1);
    if (ImGui::Checkbox(wo_string(args + 0), &checked))
        return wo_ret_option_bool(vm, checked);
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_int_box(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t label = wo_string(args + 0);
    int value = (int)wo_int(args + 1);
    bool update = false;

    update = ImGui::InputInt(label, &value);

    if (update)
        return wo_ret_option_int(vm, value);
    return wo_ret_option_none(vm);
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

    if (update)
        return wo_ret_option_float(vm, value);
    return wo_ret_option_none(vm);
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

    if (update)
    {
        wo_value result = wo_push_struct(vm, 2);
        wo_set_float(wo_struct_get(result, 0), values[0]);
        wo_set_float(wo_struct_get(result, 1), values[1]);

        return wo_ret_option_val(vm, result);
    }
    return wo_ret_option_none(vm);
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

    if (update)
    {
        wo_value result = wo_push_struct(vm, 3);
        wo_set_float(wo_struct_get(result, 0), values[0]);
        wo_set_float(wo_struct_get(result, 1), values[1]);
        wo_set_float(wo_struct_get(result, 2), values[2]);
        return wo_ret_option_val(vm, result);
    }
    return wo_ret_option_none(vm);
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
    if (update)
    {
        wo_value result = wo_push_struct(vm, 4);
        wo_set_float(wo_struct_get(result, 0), values[0]);
        wo_set_float(wo_struct_get(result, 1), values[1]);
        wo_set_float(wo_struct_get(result, 2), values[2]);
        wo_set_float(wo_struct_get(result, 3), values[3]);
        return wo_ret_option_val(vm, result);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_input_text_multiline(wo_vm vm, wo_value args, size_t argc)
{
    std::string buf = wo_string(args + 1);
    bool updated = false;
    if (argc == 4)
    {
        updated = ImGui::InputTextMultiline(wo_string(args + 0), &buf,
            ImVec2(wo_float(args + 2), wo_float(args + 3)));
    }
    else
        updated = ImGui::InputTextMultiline(wo_string(args + 0), &buf);

    if (updated)
        return wo_ret_option_string(vm, buf.c_str());
    return wo_ret_option_none(vm);
}

WO_API wo_api je_gui_combo(wo_vm vm, wo_value args, size_t argc)
{
    std::vector<const char*> combo_items;
    for (wo_integer_t i = 0; i < wo_lengthof(args + 1); ++i)
    {
        combo_items.push_back(wo_string(wo_arr_get(args + 1, i)));
    }
    int current_item = (int)wo_int(args + 2);
    auto updated = ImGui::Combo(wo_string(args + 0), &current_item, combo_items.data(), (int)combo_items.size());

    if (updated)
        return wo_ret_option_int(vm, current_item);
    return wo_ret_option_none(vm);
}

struct gui_wo_job_coroutine
{
    wo_vm work_vm;
    gui_wo_job_coroutine* last;
};
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_job_list;
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_new_job_list;

WO_API wo_api je_gui_launch(wo_vm vm, wo_value args, size_t argc)
{
    wo_integer_t loopfunc = wo_int(args + 0);
    wo_value jobfunc = args + 1;
    wo_value argpacks = args + 2;

    wo_vm vmm = wo_sub_vm(vm, 1024);

    wo_push_val(vmm, argpacks);
    wo_push_val(vmm, jobfunc);

    wo_dispatch_rsfunc(vmm, loopfunc, 2);

    gui_wo_job_coroutine* guico = new gui_wo_job_coroutine;
    guico->work_vm = vmm;
    _wo_new_job_list.add_one(guico);

    return wo_ret_void(vm);
}

void jegui_init(void* window_handle, bool reboot)
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

    auto* ttf_file = jeecs_file_open("@/resource/font/default.ttf");
    if (ttf_file == nullptr)
        ttf_file = jeecs_file_open("!/builtin/font/default.ttf");

    if (ttf_file)
    {
        auto* file_buf = je_mem_alloc(ttf_file->m_file_length);
        jeecs_file_read(file_buf, sizeof(char), ttf_file->m_file_length, ttf_file);

        io.Fonts->AddFontFromMemoryTTF(file_buf, (int)ttf_file->m_file_length, 18, nullptr,
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

    any_window_focused_on = next_frame_any_window_focused_on;
    next_frame_any_window_focused_on = false;

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

void jegui_shutdown(bool reboot)
{
    if (!reboot)
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
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}