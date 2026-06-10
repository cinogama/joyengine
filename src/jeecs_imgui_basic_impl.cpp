#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_imgui_backend_api.hpp"

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

struct key_state
{
    bool m_last_frame_down;
    bool m_this_frame_down;
};

std::mutex _key_state_record_mx;
std::unordered_map<ImGuiKey, key_state> _key_state_record;

struct gui_wo_job_coroutine
{
    std::optional<woort_value> function_to_spawn;
    woort_vm* work_vm;

    gui_wo_job_coroutine* last;
};
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_job_list;
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_new_job_list;

struct _jegui_thread_local_context
{
    jegl_context* _jegl_context = nullptr;
    bool _jegui_stop_work_flag = false;

    jegui_user_image_loader_t _jegl_get_native_texture = nullptr;
    jegui_user_sampler_loader_t _jegl_bind_shader_sampler_state = nullptr;

    std::string _jegui_imgui_config_path;
    std::optional<jeecs::basic::resource<jeecs::graphic::shader>>
        _jegl_rend_texture_shader;
};
thread_local _jegui_thread_local_context _je_gui_tls_ctx;

struct _jegui_static_context
{
    woort_vm* _jegui_exit_callback_handler_vm = nullptr;
    woort_GCPin* _jegui_exit_callback_function = nullptr;
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
    _je_gui_static_ctx._jegui_specify_font_path = general_font_path ? std::optional(general_font_path) : std::nullopt;
    _je_gui_static_ctx._jegui_specify_latin_font_path = latin_font_path ? std::optional(latin_font_path) : std::nullopt;

    _je_gui_static_ctx._jegui_specify_font_size = size;
}

WOORT_API woort_api je_gui_begin_tool_tip(void)
{
    return woort_ret_bool(ImGui::BeginTooltip());
}

WOORT_API woort_api je_gui_end_tool_tip(void)
{
    ImGui::EndTooltip();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_begin_disabled(void)
{
    ImGui::BeginDisabled(woort_bool(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_end_disabled(void)
{
    ImGui::EndDisabled();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_begin_drag_drop_source(void)
{
    bool result;
    result = ImGui::BeginDragDropSource(ImGuiDragDropFlags_None);
    return woort_ret_bool(result);
}
WOORT_API woort_api je_gui_begin_drag_drop_source_attr(void)
{
    bool result;
    result = ImGui::BeginDragDropSource((ImGuiDragDropFlags)woort_int(0));
    return woort_ret_bool(result);
}
WOORT_API woort_api je_gui_set_drag_drop_payload(void)
{
    woort_U8CString buf = woort_string(1);

    bool result = ImGui::SetDragDropPayload(woort_string(0), buf, strlen(buf) + 1);
    return woort_ret_bool(result);
}
WOORT_API woort_api je_gui_end_drag_drop_source(void)
{
    ImGui::EndDragDropSource();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_begin_drag_drop_target(void)
{
    return woort_ret_bool(ImGui::BeginDragDropTarget());
}
WOORT_API woort_api je_gui_accept_drag_drop_payload(void)
{
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(woort_string(0)))
    {
        return woort_ret_option_string((const char*)payload->Data);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_end_accept_drop_source(void)
{
    ImGui::EndDragDropTarget();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_set_next_item_open(void)
{
    ImGui::SetNextItemOpen(woort_bool(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_set_clip_board_text(void)
{
    ImGui::SetClipboardText(woort_string(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_get_clip_board_text(void)
{
    const char* t = ImGui::GetClipboardText();
    return woort_ret_string(t == nullptr ? "" : t);
}

woort_value set_float2_to_struct(woort_value v, float x, float y)
{
    woort_set_struct(v, 2);
    woort_struct_set_float(v, 0, x);
    woort_struct_set_float(v, 1, y);
    return v;
}
woort_value set_float3_to_struct(woort_value v, float x, float y, float z)
{
    woort_set_struct(v, 3);
    woort_struct_set_float(v, 0, x);
    woort_struct_set_float(v, 1, y);
    woort_struct_set_float(v, 2, z);
    return v;
}
woort_value set_float4_to_struct(woort_value v, float x, float y, float z, float w)
{
    woort_set_struct(v, 4);
    woort_struct_set_float(v, 0, x);
    woort_struct_set_float(v, 1, y);
    woort_struct_set_float(v, 2, z);
    woort_struct_set_float(v, 3, w);
    return v;
}

WOORT_API woort_api je_gui_get_main_viewport_pos(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    auto vpos = ImGui::GetMainViewport()->Pos;

    return woort_ret_value(set_float2_to_struct(s + 0, vpos.x, vpos.y));
}

WOORT_API woort_api je_gui_get_this_viewport_pos(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    auto vpos = ImGui::GetWindowViewport()->Pos;

    return woort_ret_value(set_float2_to_struct(s + 0, vpos.x, vpos.y));
}

WOORT_API woort_api je_gui_get_window_pos(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    auto wpos = ImGui::GetWindowPos();

    return woort_ret_value(set_float2_to_struct(s + 0, wpos.x, wpos.y));
}

WOORT_API woort_api je_gui_get_mouse_pos(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    auto&& mpos = ImGui::GetMousePos();
    return woort_ret_value(set_float2_to_struct(s + 0, mpos.x, mpos.y));
}

WOORT_API woort_api je_gui_get_mouse_wheel(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    auto& imgui_io = ImGui::GetIO();
    return woort_ret_value(set_float2_to_struct(s + 0, imgui_io.MouseWheelH, imgui_io.MouseWheel));
}

WOORT_API woort_api je_gui_get_mouse_delta_pos(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    auto&& mdpos = ImGui::GetIO().MouseDelta;
    return woort_ret_value(set_float2_to_struct(s + 0, mdpos.x, mdpos.y));
}

WOORT_API woort_api je_gui_get_cursor_pos(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    auto&& cpos = ImGui::GetCursorPos();
    return woort_ret_value(set_float2_to_struct(s + 0, cpos.x, cpos.y));
}
WOORT_API woort_api je_gui_get_item_rect_size(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    auto&& isize = ImGui::GetItemRectSize();
    return woort_ret_value(set_float2_to_struct(s + 0, isize.x, isize.y));
}

WOORT_API woort_api je_gui_get_item_rect(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto&& isizemin = ImGui::GetItemRectMin();
    auto&& isizemax = ImGui::GetItemRectMax();

    woort_set_struct(s + 0, 2);

    woort_struct_set(s + 0, 0, set_float2_to_struct(s + 1, isizemin.x, isizemin.y));
    woort_struct_set(s + 0, 1, set_float2_to_struct(s + 1, isizemax.x, isizemax.y));

    return woort_ret_value(s + 0);
}

WOORT_API woort_api je_gui_push_id_str(void)
{
    ImGui::PushID(woort_string(0));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_push_id(void)
{
    ImGui::PushID((int)woort_int(0));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_pop_id(void)
{
    ImGui::PopID();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_get_id(void)
{
    return woort_ret_int((woort_Int)ImGui::GetID(woort_string(0)));
}

WOORT_API woort_api je_gui_beginpopup_contextitem(void)
{
    return woort_ret_bool(ImGui::BeginPopupContextItem());
}
WOORT_API woort_api je_gui_beginpopup_contextitem_label(void)
{
    return woort_ret_bool(ImGui::BeginPopupContextItem(woort_string(0)));
}
WOORT_API woort_api je_gui_beginpopup_contextwindow(void)
{
    return woort_ret_bool(ImGui::BeginPopupContextWindow());
}
WOORT_API woort_api je_gui_beginpopup_contextwindow_label(void)
{
    return woort_ret_bool(ImGui::BeginPopupContextWindow(woort_string(0)));
}
WOORT_API woort_api je_gui_begintabbar(void)
{
    return woort_ret_bool(ImGui::BeginTabBar(woort_string(0), (ImGuiTabBarFlags)woort_int(1)));
}

WOORT_API woort_api je_gui_endtabbar(void)
{
    ImGui::EndTabBar();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_begintabitem(void)
{
    bool display = ImGui::BeginTabItem(woort_string(0), nullptr, (ImGuiTabBarFlags)woort_int(1));
    return woort_ret_bool(display);
}

WOORT_API woort_api je_gui_begintabitem_open(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    bool open = true;
    bool display = ImGui::BeginTabItem(woort_string(0), &open, (ImGuiTabBarFlags)woort_int(1));

    woort_value ret = s + 0;
    woort_value elem = s + 1;

    woort_set_struct(ret, 2);

    woort_set_bool(elem, display);
    woort_struct_set(ret, 0, elem);
    woort_set_bool(elem, open);
    woort_struct_set(ret, 1, elem);

    return woort_ret_value(ret);
}

ImVec2 val2vec2(woort_value v)
{
    const float x = woort_struct_get_float(v, 0);
    const float y = woort_struct_get_float(v, 1);
    return ImVec2(x, y);
}
ImVec4 val2vec4(woort_value v)
{
    const float x = woort_struct_get_float(v, 0);
    const float y = woort_struct_get_float(v, 1);
    const float z = woort_struct_get_float(v, 2);
    const float w = woort_struct_get_float(v, 3);
    return ImVec4(x, y, z, w);
}

WOORT_API woort_api je_gui_push_clip_rect(void)
{
    woort_value from = 0;
    woort_value to = 1;
    ImGui::PushClipRect(
        val2vec2(from),
        val2vec2(to),
        true);

    return woort_ret_void();
}

WOORT_API woort_api je_gui_pop_clip_rect(void)
{
    ImGui::PopClipRect();

    return woort_ret_void();
}

ImU32 val2color32(woort_value v)
{
    const int x = (int)woort_struct_get_int(v, 0);
    const int y = (int)woort_struct_get_int(v, 1);
    const int z = (int)woort_struct_get_int(v, 2);
    const int w = (int)woort_struct_get_int(v, 3);

    return IM_COL32(x, y, z, w);
}

ImVec4 val2colorf4(woort_value v)
{
    const float x = woort_struct_get_float(v, 0);
    const float y = woort_struct_get_float(v, 1);
    const float z = woort_struct_get_float(v, 2);
    const float w = woort_struct_get_float(v, 3);

    return ImVec4(
        x / 255.0f,
        y / 255.0f,
        z / 255.0f,
        w / 255.0f);
}

WOORT_API woort_api je_gui_push_style_color(void)
{
    ImGui::PushStyleColor((ImGuiCol)woort_int(0), val2color32(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_pop_style_color(void)
{
    ImGui::PopStyleColor((int)woort_int(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_get_window_draw_list(void)
{
    return woort_ret_pointer(ImGui::GetWindowDrawList());
}

WOORT_API woort_api je_gui_draw_list_add_rect(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddRect(val2vec2(1), val2vec2(2), val2color32(3), 0.f, 0, woort_float(4));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_draw_list_add_rect_filled(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddRectFilled(val2vec2(1), val2vec2(2), val2color32(3));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_draw_list_add_triangle(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddTriangle(val2vec2(1), val2vec2(2), val2vec2(3), val2color32(4), woort_float(5));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_draw_list_add_triangle_filled(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddTriangleFilled(val2vec2(1), val2vec2(2), val2vec2(3), val2color32(4));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_draw_list_add_text(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddText(val2vec2(1), val2color32(2), woort_string(3));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_draw_list_add_image(void)
{
    ImDrawList* dlist = (ImDrawList*)woort_pointer(0);

    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(3);
    jegl_bind_texture((*texture)->resource(), 0);

    const ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);

    dlist->AddCallback(
        [](auto, auto)
        {
            _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
                _je_gui_tls_ctx._jegl_context,
                _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource());
        },
        nullptr);
    dlist->AddImage(
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        val2vec2(1),
        val2vec2(2),
        uvmin,
        uvmax,
        val2color32(4));
    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    return woort_ret_void();
}

WOORT_API woort_api je_gui_draw_list_add_line(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddLine(val2vec2(1), val2vec2(2), val2color32(3), woort_float(4));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_draw_list_add_circle(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddCircle(val2vec2(1), woort_float(2), val2color32(3), 0, woort_float(4));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_draw_list_add_filled_circle(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddCircleFilled(val2vec2(1), woort_float(2), val2color32(3));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_draw_list_add_bezier_quad(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddBezierQuadratic(val2vec2(1), val2vec2(2), val2vec2(3), val2color32(4), woort_float(5));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_draw_list_add_bezier_cubic(void)
{
    ImDrawList* list = (ImDrawList*)woort_pointer(0);
    list->AddBezierCubic(val2vec2(1), val2vec2(2), val2vec2(3), val2vec2(4), val2color32(5), woort_float(6));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_endtabitem(void)
{
    ImGui::EndTabItem();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_beginpopup(void)
{
    return woort_ret_bool(ImGui::BeginPopup(woort_string(0)));
}

WOORT_API woort_api je_gui_openpopup_on_item_click(void)
{
    ImGui::OpenPopupOnItemClick();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_openpopup_on_item_click_label(void)
{
    ImGui::OpenPopupOnItemClick(woort_string(0));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_openpopup_on_item_click_attr(void)
{
    ImGui::OpenPopupOnItemClick(nullptr, (ImGuiPopupFlags)woort_int(0));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_openpopup_on_item_click_label_attr(void)
{
    ImGui::OpenPopupOnItemClick(woort_string(0), (ImGuiPopupFlags)woort_int(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_openpopup(void)
{
    ImGui::OpenPopup(woort_string(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_endpopup(void)
{
    ImGui::EndPopup();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_begin_listbox(void)
{
    return woort_ret_bool(ImGui::BeginListBox(woort_string(0), ImVec2(woort_float(1), woort_float(2))));
}
WOORT_API woort_api je_gui_begin_selectable(void)
{
    return woort_ret_bool(ImGui::Selectable(woort_string(0)));
}
WOORT_API woort_api je_gui_begin_selectable_selected(void)
{
    return woort_ret_bool(ImGui::Selectable(woort_string(0), woort_bool(1)));
}
WOORT_API woort_api je_gui_end_listbox(void)
{
    ImGui::EndListBox();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_sameline(void)
{
    ImGui::SameLine();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_begingroup(void)
{
    ImGui::BeginGroup();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_endgroup(void)
{
    ImGui::EndGroup();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_is_itemclicked(void)
{
    return woort_ret_bool(ImGui::IsItemClicked((ImGuiMouseButton)woort_int(0)));
}

WOORT_API woort_api je_gui_is_itemtoggledopen(void)
{
    return woort_ret_bool(ImGui::IsItemToggledOpen());
}

WOORT_API woort_api je_gui_is_itemhovered(void)
{
    return woort_ret_bool(ImGui::IsItemHovered());
}

WOORT_API woort_api je_gui_is_mousehoveringrect(void)
{
    return woort_ret_bool(ImGui::IsMouseHoveringRect(
        val2vec2(0), val2vec2(1)));
}

WOORT_API woort_api je_gui_is_item_active(void)
{
    return woort_ret_bool(ImGui::IsItemActive());
}

WOORT_API woort_api je_gui_is_mouse_dragging(void)
{
    return woort_ret_bool(ImGui::IsMouseDragging((ImGuiMouseButton)woort_int(0)));
}
WOORT_API woort_api je_gui_is_mouse_down(void)
{
    return woort_ret_bool(ImGui::IsMouseDown((ImGuiMouseButton)woort_int(0)));
}
WOORT_API woort_api je_gui_is_mouse_clicked(void)
{
    return woort_ret_bool(ImGui::IsMouseClicked((ImGuiMouseButton)woort_int(0)));
}
WOORT_API woort_api je_gui_is_mouse_released(void)
{
    return woort_ret_bool(ImGui::IsMouseReleased((ImGuiMouseButton)woort_int(0)));
}

WOORT_API woort_api je_gui_is_key_down(void)
{
    return woort_ret_bool(ImGui::IsKeyDown((ImGuiKey)woort_int(0)));
}

WOORT_API woort_api je_gui_set_tooltip(void)
{
    ImGui::SetTooltip("%s", woort_string(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_treenodeex(void)
{
    return woort_ret_bool(
        ImGui::TreeNodeEx(
            woort_string(0),
            (ImGuiTreeNodeFlags)woort_int(1)));
}

WOORT_API woort_api je_gui_treenode(void)
{
    return woort_ret_bool(ImGui::TreeNode(woort_string(0)));
}
WOORT_API woort_api je_gui_treepop(void)
{
    ImGui::TreePop();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_listbox(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    int selected_item = -1;
    int max_height_item = -1;

    std::vector<const char*> items((size_t)woort_vec_len(1));
    woort_value elem = s + 0;
    for (size_t i = 0; i < items.size(); i++)
    {
        (void)woort_vec_get(elem, 1, i);
        items[i] = woort_string(elem);
    }

    bool val_changed = ImGui::ListBox(woort_string(0), &selected_item, items.data(), (int)items.size(), max_height_item);

    if (val_changed)
        return woort_ret_option_int(selected_item);
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_listbox_select(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    int selected_item = (int)woort_int(2);
    int max_height_item = -1;

    std::vector<const char*> items((size_t)woort_vec_len(1));
    woort_value elem = s + 0;
    for (size_t i = 0; i < items.size(); i++)
    {
        (void)woort_vec_get(elem, 1, i);
        items[i] = woort_string(elem);
    }

    bool val_changed = ImGui::ListBox(woort_string(0), &selected_item, items.data(), (int)items.size(), max_height_item);

    if (val_changed)
        return woort_ret_option_int(selected_item);
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_listbox_select_height(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    int selected_item = (int)woort_int(2);
    int max_height_item = (int)woort_int(3);

    std::vector<const char*> items((size_t)woort_vec_len(1));
    woort_value elem = s + 0;
    for (size_t i = 0; i < items.size(); i++)
    {
        (void)woort_vec_get(elem, 1, i);
        items[i] = woort_string(elem);
    }

    bool val_changed = ImGui::ListBox(woort_string(0), &selected_item, items.data(), (int)items.size(), max_height_item);

    if (val_changed)
        return woort_ret_option_int(selected_item);
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_listbox_withsize(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    // func ListBox(label:string, items:array<string>, ref select_item:int, width:real, height:real)=> int;
    int origin_selected_index = (int)woort_int(2);
    int selected_index = -1;
    bool value_updated = false;

    woort_value elem = s + 0;

    if (ImGui::BeginListBox(woort_string(0), ImVec2(woort_float(3), woort_float(4))))
    {
        size_t sz = (size_t)woort_vec_len(1);
        for (size_t i = 0; i < sz; i++)
        {
            (void)woort_vec_get(elem, 1, i);
            woort_U8CString item = woort_string(elem);
            if (ImGui::Selectable(item, (int)i == origin_selected_index))
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
        return woort_ret_option_int(selected_index);
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_job_vm_handle(void)
{
    return woort_ret_pointer(woort_VMRuntime_current());
}

WOORT_API woort_api je_gui_begin(void)
{
    bool showing = ImGui::Begin(woort_string(0));
    return woort_ret_bool(showing);
}
WOORT_API woort_api je_gui_begin_attr(void)
{
    bool showing = ImGui::Begin(woort_string(0), 0, (ImGuiWindowFlags)woort_int(1));
    return woort_ret_bool(showing);
}
WOORT_API woort_api je_gui_begin_open(void)
{
    bool windows_flag = true;
    bool showing = ImGui::Begin(woort_string(0), &windows_flag, (ImGuiWindowFlags)woort_int(1));

    if (windows_flag)
        return woort_ret_option_bool(showing);
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_is_window_focused(void)
{
    return woort_ret_bool(ImGui::IsWindowFocused());
}

WOORT_API woort_api je_gui_end(void)
{
    ImGui::End();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_begin_child(void)
{
    return woort_ret_bool(ImGui::BeginChild(woort_string(0), ImVec2(0.f, 0.f), true));
}
WOORT_API woort_api je_gui_begin_child_attr(void)
{
    return woort_ret_bool(ImGui::BeginChild(woort_string(0),
        ImVec2(0.f, 0.f), true, (ImGuiWindowFlags)woort_int(1)));
}
WOORT_API woort_api je_gui_begin_child_size(void)
{
    return woort_ret_bool(ImGui::BeginChild(woort_string(0),
        ImVec2(woort_float(1), woort_float(2)), true));
}

WOORT_API woort_api je_gui_end_child(void)
{
    ImGui::EndChild();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_progress_bar(void)
{
    ImGui::ProgressBar(woort_float(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_progress_bar_size(void)
{
    ImGui::ProgressBar(woort_float(0), val2vec2(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_text(void)
{
    ImGui::Text("%s", woort_string(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_text_disabled(void)
{
    ImGui::TextDisabled("%s", woort_string(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_button(void)
{
    return woort_ret_bool(ImGui::Button(woort_string(0)));
}

WOORT_API woort_api je_gui_button_size(void)
{
    return woort_ret_bool(ImGui::Button(woort_string(0), val2vec2(1)));
}

WOORT_API woort_api je_gui_invisible_button(void)
{
    return woort_ret_bool(ImGui::InvisibleButton(woort_string(0), val2vec2(1)));
}

WOORT_API woort_api je_gui_begin_main_menu_bar(void)
{
    return woort_ret_bool(ImGui::BeginMainMenuBar());
}
WOORT_API woort_api je_gui_menu_item(void)
{
    return woort_ret_bool(ImGui::MenuItem(woort_string(0)));
}
WOORT_API woort_api je_gui_menu_item_shortcut(void)
{
    return woort_ret_bool(ImGui::MenuItem(woort_string(0), woort_string(1)));
}
WOORT_API woort_api je_gui_menu_item_enabled(void)
{
    return woort_ret_bool(ImGui::MenuItem(woort_string(0), 0, false, woort_bool(1)));
}
WOORT_API woort_api je_gui_menu_item_shortcut_enabled(void)
{
    return woort_ret_bool(ImGui::MenuItem(woort_string(0), woort_string(1), false, woort_bool(2)));
}
WOORT_API woort_api je_gui_menu_item_selected(void)
{
    bool selected = woort_bool(2);
    bool clicked = ImGui::MenuItem(woort_string(0), woort_string(1), &selected, woort_bool(3));
    woort_set_bool(2, selected);

    if (clicked)
        return woort_ret_option_bool(selected);
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_end_main_menu_bar(void)
{
    ImGui::EndMainMenuBar();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_begin_menu_bar(void)
{
    return woort_ret_bool(ImGui::BeginMenuBar());
}
WOORT_API woort_api je_gui_end_menu_bar(void)
{
    ImGui::EndMenuBar();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_begin_menu(void)
{
    return woort_ret_bool(ImGui::BeginMenu(woort_string(0)));
}
WOORT_API woort_api je_gui_begin_menu_enabled(void)
{
    return woort_ret_bool(ImGui::BeginMenu(woort_string(0), woort_bool(1)));
}
WOORT_API woort_api je_gui_end_menu(void)
{
    ImGui::EndMenu();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_separator(void)
{
    ImGui::Separator();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_image(void)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    jegl_bind_texture((*texture)->resource(), 0);

    const ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto)
        { _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource()); },
        nullptr);
    ImGui::Image(
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        ImVec2(
            (float)((*texture)->resource()->m_width),
            (float)((*texture)->resource()->m_height)),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return woort_ret_void();
}
WOORT_API woort_api je_gui_image_scale(void)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    jegl_bind_texture((*texture)->resource(), 0);

    const ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto)
        { _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource()); },
        nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
        _je_gui_tls_ctx._jegl_context,
        (*texture)->resource()),
        ImVec2(
            (float)((*texture)->resource()->m_width) * woort_float(1),
            (float)((*texture)->resource()->m_height) * woort_float(1)),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return woort_ret_void();
}
WOORT_API woort_api je_gui_image_size(void)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    jegl_bind_texture((*texture)->resource(), 0);

    const ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto)
        { _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource()); },
        nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
        _je_gui_tls_ctx._jegl_context,
        (*texture)->resource()),
        ImVec2(
            woort_float(1),
            woort_float(2)),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return woort_ret_void();
}

WOORT_API woort_api je_gui_image_size_color(void)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = 
        (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    jegl_bind_texture((*texture)->resource(), 0);

    const ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto)
        { _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource()); },
        nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
        _je_gui_tls_ctx._jegl_context,
        (*texture)->resource()),
        ImVec2(
            woort_float(1),
            woort_float(2)),
        uvmin,
        uvmax,
        val2colorf4(3));

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return woort_ret_void();
}

WOORT_API woort_api je_gui_imagebutton(void)
{
    woort_U8CString label = woort_string(0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(1);

    jegl_bind_texture((*texture)->resource(), 0);

    const ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto)
        { _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource()); },
        nullptr);
    result = ImGui::ImageButton(
        label,
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        ImVec2(
            (float)((*texture)->resource()->m_width),
            (float)((*texture)->resource()->m_height)),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return woort_ret_bool(result);
}
WOORT_API woort_api je_gui_imagebutton_scale(void)
{
    woort_U8CString label = woort_string(0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(1);

    jegl_bind_texture((*texture)->resource(), 0);

    const ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto)
        { _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource()); },
        nullptr);
    result = ImGui::ImageButton(
        label,
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        ImVec2(
            (float)((*texture)->resource()->m_width) * woort_float(2),
            (float)((*texture)->resource()->m_height) * woort_float(2)),
        uvmin, uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return woort_ret_bool(result);
}
WOORT_API woort_api je_gui_imagebutton_size(void)
{
    woort_U8CString label = woort_string(0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(1);

    jegl_bind_texture((*texture)->resource(), 0);

    const ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto)
        { _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource()); },
        nullptr);
    result = ImGui::ImageButton(
        label,
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        ImVec2(
            woort_float(2),
            woort_float(3)),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return woort_ret_bool(result);
}

WOORT_API woort_api je_gui_content_region_avail(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    auto&& sz = ImGui::GetContentRegionAvail();
    return woort_ret_value(set_float2_to_struct(s + 0, sz.x, sz.y));
}

WOORT_API woort_api je_gui_set_next_item_width(void)
{
    ImGui::SetNextItemWidth(woort_float(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_set_next_window_size_constraints(void)
{
    ImGui::SetNextWindowSizeConstraints(val2vec2(0), val2vec2(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_set_next_window_size(void)
{
    ImGui::SetNextWindowSize(val2vec2(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_set_next_window_pos(void)
{
    ImGui::SetNextWindowPos(val2vec2(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_colorbutton(void)
{
    return woort_ret_bool(ImGui::ColorButton(woort_string(0), val2vec4(1)));
}
WOORT_API woort_api je_gui_colorpicker4(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    float rgba[4] = {};

    woort_value elem = s + 0;
    woort_struct_get(elem, 1, 0);
    rgba[0] = woort_float(elem);
    woort_struct_get(elem, 1, 1);
    rgba[1] = woort_float(elem);
    woort_struct_get(elem, 1, 2);
    rgba[2] = woort_float(elem);
    woort_struct_get(elem, 1, 3);
    rgba[3] = woort_float(elem);

    if (ImGui::ColorPicker4(woort_string(0), rgba))
    {
        woort_value result = s + 1;
        set_float4_to_struct(result, rgba[0], rgba[1], rgba[2], rgba[3]);

        return woort_ret_option_value(result);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_push_item_width(void)
{
    ImGui::PushItemWidth(woort_float(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_pop_item_width(void)
{
    ImGui::PopItemWidth();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_input_text_box(void)
{
    std::string buf = woort_string(1);
    if (ImGui::InputText(woort_string(0), &buf))
    {
        return woort_ret_option_string(buf.c_str());
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_input_text_attr_box(void)
{
    std::string buf = woort_string(1);
    if (ImGui::InputText(woort_string(0), &buf, (ImGuiInputFlags)woort_int(2)))
    {
        return woort_ret_option_string(buf.c_str());
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_checkbox(void)
{
    bool checked = woort_bool(1);
    if (ImGui::Checkbox(woort_string(0), &checked))
        return woort_ret_option_bool(checked);
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_input_int_box(void)
{
    woort_U8CString label = woort_string(0);
    int value = (int)woort_int(1);
    bool update = false;

    update = ImGui::InputInt(label, &value);

    if (update)
        return woort_ret_option_int(value);
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_int2_box(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    int values[] = { (int)woort_int(1), (int)woort_int(2) };
    bool update = false;

    update = ImGui::InputInt2(label, values);

    if (update)
    {
        woort_value result = s + 0;
        woort_value elem = s + 1;

        woort_set_struct(result, 2);

        woort_set_int(elem, (int)values[0]);
        woort_struct_set(result, 0, elem);
        woort_set_int(elem, (int)values[1]);
        woort_struct_set(result, 1, elem);
        return woort_ret_result_ok_value(result);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_int3_box(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    int values[] = { (int)woort_int(1), (int)woort_int(2), (int)woort_int(3) };
    bool update = false;

    update = ImGui::InputInt3(label, values);

    if (update)
    {
        woort_value result = s + 0;
        woort_value elem = s + 1;

        woort_set_struct(result, 3);

        woort_set_int(elem, (int)values[0]);
        woort_struct_set(result, 0, elem);
        woort_set_int(elem, (int)values[1]);
        woort_struct_set(result, 1, elem);
        woort_set_int(elem, (int)values[2]);
        woort_struct_set(result, 2, elem);
        return woort_ret_result_ok_value(result);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_int4_box(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    int values[] = { (int)woort_int(1), (int)woort_int(2), (int)woort_int(3), (int)woort_int(4) };
    bool update = false;

    update = ImGui::InputInt3(label, values);

    if (update)
    {
        woort_value result = s + 0;
        woort_value elem = s + 1;

        woort_set_struct(result, 4);

        woort_set_int(elem, (int)values[0]);
        woort_struct_set(result, 0, elem);
        woort_set_int(elem, (int)values[1]);
        woort_struct_set(result, 1, elem);
        woort_set_int(elem, (int)values[2]);
        woort_struct_set(result, 2, elem);
        woort_set_int(elem, (int)values[3]);
        woort_struct_set(result, 3, elem);
        return woort_ret_result_ok_value(result);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_drag_float(void)
{
    woort_U8CString label = woort_string(0);
    float value = woort_float(1);
    float speed = woort_float(2);
    float minval = woort_float(3);
    float maxval = woort_float(4);
    if (ImGui::DragFloat(label, &value, speed, minval, maxval))
        return woort_ret_option_float(value);
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_input_float_box(void)
{
    woort_U8CString label = woort_string(0);
    float value = woort_float(1);
    bool update = false;

    update = ImGui::InputFloat(label, &value);

    if (update)
        return woort_ret_option_float(value);
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_float2_box(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    float values[] = { woort_float(1), woort_float(2) };
    bool update = false;

    update = ImGui::InputFloat2(label, values);

    if (update)
    {
        return woort_ret_option_value(set_float2_to_struct(s + 0, values[0], values[1]));
    }
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_float3_box(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    float values[] = { woort_float(1), woort_float(2), woort_float(3) };
    bool update = false;

    update = ImGui::InputFloat3(label, values);

    if (update)
    {
        return woort_ret_option_value(set_float3_to_struct(s + 0, values[0], values[1], values[2]));
    }
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_float4_box(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    float values[] = { woort_float(1), woort_float(2), woort_float(3), woort_float(4) };
    bool update = false;

    update = ImGui::InputFloat4(label, values);

    if (update)
    {
        return woort_ret_option_value(set_float4_to_struct(s + 0, values[0], values[1], values[2], values[3]));
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_input_float_format_box(void)
{
    woort_U8CString label = woort_string(0);
    float value = woort_float(1);
    bool update = false;

    woort_U8CString format = woort_string(2);
    update = ImGui::InputFloat(label, &value, 0.f, 0.f, format);

    if (update)
        return woort_ret_option_float(value);
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_float2_format_box(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    float values[] = { woort_float(1), woort_float(2) };
    bool update = false;

    woort_U8CString format = woort_string(3);
    update = ImGui::InputFloat2(label, values, format);

    if (update)
    {
        return woort_ret_option_value(set_float2_to_struct(s + 0, values[0], values[1]));
    }
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_float3_format_box(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    float values[] = { woort_float(1), woort_float(2), woort_float(3) };
    bool update = false;

    woort_U8CString format = woort_string(4);
    update = ImGui::InputFloat3(label, values, format);

    if (update)
    {
        return woort_ret_option_value(set_float3_to_struct(s + 0, values[0], values[1], values[2]));
    }
    return woort_ret_option_none();
}
WOORT_API woort_api je_gui_input_float4_format_box(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_U8CString label = woort_string(0);
    float values[] = { woort_float(1), woort_float(2), woort_float(3), woort_float(4) };
    bool update = false;

    woort_U8CString format = woort_string(5);
    update = ImGui::InputFloat4(label, values, format);

    if (update)
    {
        return woort_ret_option_value(set_float4_to_struct(s + 0, values[0], values[1], values[2], values[3]));
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_input_text_multiline(void)
{
    std::string buf = woort_string(1);
    bool updated = false;

    updated = ImGui::InputTextMultiline(woort_string(0), &buf);

    if (updated)
        return woort_ret_option_string(buf.c_str());
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_input_text_multiline_size(void)
{
    std::string buf = woort_string(1);
    bool updated = false;

    updated = ImGui::InputTextMultiline(woort_string(0), &buf,
        ImVec2(woort_float(2), woort_float(3)));

    if (updated)
        return woort_ret_option_string(buf.c_str());
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_combo(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    std::vector<const char*> combo_items;
    woort_value elem = s + 0;
    for (size_t i = 0; i < woort_vec_len(1); ++i)
    {
        (void)woort_vec_get(elem, 1, i);
        combo_items.push_back(woort_string(elem));
    }

    int current_item = -1;
    if (woort_option_get(elem, 2))
        current_item = (int)woort_int(elem);

    auto updated = ImGui::Combo(woort_string(0), &current_item, combo_items.data(), (int)combo_items.size());

    if (updated)
        return woort_ret_option_int(current_item);
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_launch(void)
{
    woort_VMRuntime* new_vm = woort_vm_create();

    woort_value fn;
    woort_VMRuntime* const last = woort_vm_swap(new_vm);
    {
        if (!woort_push_reserve(1, &fn))
        {
            (void)woort_vm_swap(last);
            return woort_ret_panic("New vm stack overflow.");
        }
        woort_import_value(fn, last, 0);
    }
    (void)woort_vm_swap(last);

    gui_wo_job_coroutine* guico = new gui_wo_job_coroutine;

    guico->function_to_spawn.emplace(fn);
    guico->work_vm = new_vm;

    _wo_new_job_list.add_one(guico);

    return woort_ret_void();
}

WOORT_API woort_api je_gui_stop_all_work(void)
{
    _je_gui_tls_ctx._jegui_stop_work_flag = true;
    return woort_ret_void();
}

WOORT_API woort_api je_gui_get_input_state(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");
    ImGuiKey kcode = (ImGuiKey)woort_int(0);

    std::lock_guard g1(_key_state_record_mx);
    auto fnd = _key_state_record.find(kcode);
    if (fnd == _key_state_record.end())
    {
        _key_state_record[kcode] = key_state{
            false,
            ImGui::IsKeyDown(kcode),
        };

        fnd = _key_state_record.find(kcode);
    }

    woort_value v = s + 0;
    woort_value elem = s + 1;

    woort_set_struct(v, 2);

    woort_set_bool(elem, fnd->second.m_last_frame_down);
    woort_struct_set(v, 0, elem);
    woort_set_bool(elem, fnd->second.m_this_frame_down);
    woort_struct_set(v, 1, elem);
    return woort_ret_value(v);
}

WOORT_API woort_api je_gui_register_exit_callback(void)
{
    if (_je_gui_static_ctx._jegui_exit_callback_handler_vm != nullptr)
        return woort_ret_panic("Callback has been registered.");

    assert(
        _je_gui_static_ctx._jegui_exit_callback_handler_vm == nullptr && _je_gui_static_ctx._jegui_exit_callback_function == nullptr);

    _je_gui_static_ctx._jegui_exit_callback_handler_vm = woort_vm_create();
    _je_gui_static_ctx._jegui_exit_callback_function = woort_GCPin_create(1);

    woort_GCPin_set(_je_gui_static_ctx._jegui_exit_callback_function, 0, 0);

    return woort_ret_void();
}
WOORT_API woort_api je_gui_unregister_exit_callback(void)
{
    if (_je_gui_static_ctx._jegui_exit_callback_handler_vm == nullptr)
        return woort_ret_panic("Callback not found.");

    woort_vm_close(_je_gui_static_ctx._jegui_exit_callback_handler_vm);
    woort_GCPin_destroy(_je_gui_static_ctx._jegui_exit_callback_function);

    _je_gui_static_ctx._jegui_exit_callback_handler_vm = nullptr;
    _je_gui_static_ctx._jegui_exit_callback_function = nullptr;

    return woort_ret_void();
}

WOORT_API woort_api je_gui_set_font(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_value elem = s + 0;
    const char* font_path = nullptr;
    const char* latin_font_path = nullptr;

    if (woort_option_get(elem, 0))
        font_path = woort_string(elem);

    if (woort_option_get(elem, 1))
        latin_font_path = woort_string(elem);

    jegui_set_font(font_path, latin_font_path, (size_t)woort_int(2));

    return woort_ret_void();
}

WOORT_API woort_api je_gui_style_get_config_color(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    ImGuiStyle* style = &ImGui::GetStyle();
    auto color = style->Colors[woort_int(0)];

    return woort_ret_value(set_float4_to_struct(s + 0, color.x, color.y, color.z, color.w));
}

WOORT_API woort_api je_gui_style_set_config_color(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    ImGuiStyle* style = &ImGui::GetStyle();
    auto& color = style->Colors[woort_int(0)];

    woort_value elem = s + 0;
    woort_struct_get(elem, 1, 0);
    color.x = woort_float(elem);
    woort_struct_get(elem, 1, 1);
    color.y = woort_float(elem);
    woort_struct_get(elem, 1, 2);
    color.z = woort_float(elem);
    woort_struct_get(elem, 1, 3);
    color.w = woort_float(elem);
    return woort_ret_void();
}

WOORT_API woort_api je_gui_style_set_config_color_dark(void)
{
    ImGui::StyleColorsDark();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_style_set_config_color_classic(void)
{
    ImGui::StyleColorsClassic();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_style_set_config_color_light(void)
{
    ImGui::StyleColorsLight();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_dock_space(void)
{
    ImGui::DockSpace((ImGuiID)woort_int(0), val2vec2(1), (ImGuiDockNodeFlags)woort_int(2));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_dock_space_over_viewport(void)
{
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
    return woort_ret_void();
}

WOORT_API woort_api je_gui_push_style_real(void)
{
    ImGui::PushStyleVar((ImGuiStyleVar)woort_int(0), woort_float(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_push_style_vec2(void)
{
    ImGui::PushStyleVar((ImGuiStyleVar)woort_int(0), val2vec2(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_pop_style_var(void)
{
    ImGui::PopStyleVar((int)woort_int(0));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_set_window_font_scale(void)
{
    ImGui::SetWindowFontScale(woort_float(0));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_dummy(void)
{
    ImGui::Dummy(val2vec2(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_code_editor_language_definition_create(void)
{
    TextEditor::LanguageDefinition* defs = new TextEditor::LanguageDefinition;
    defs->mName = woort_string(0);
    return woort_ret_gchandle(
        defs,
        WOORT_IGNORE,
        [](void* p)
        {
            delete std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(p));
        },
        nullptr);
}
WOORT_API woort_api je_gui_code_editor_language_definition_add_keyword(void)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(0)));

    defs->mKeywords.insert(woort_string(1));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_language_definition_add_identifier(void)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(0)));

    TextEditor::Identifier ident;
    ident.mDeclaration = woort_string(2);
    ident.mLocation.mLine = (int)woort_int(3);
    ident.mLocation.mColumn = (int)woort_int(4);

    defs->mIdentifiers.insert(std::make_pair(woort_string(1), ident));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_language_definition_add_token_regex(void)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(0)));

    defs->mTokenRegexStrings.push_back(
        std::make_pair(woort_string(1), (TextEditor::PaletteIndex)woort_int(2)));

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_language_definition_set_mlcomment(void)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(0)));

    defs->mCommentStart = woort_string(1);
    defs->mCommentEnd = woort_string(2);

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_language_definition_set_slcomment(void)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(0)));

    defs->mSingleLineComment = woort_string(1);

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_language_definition_set_case_sensitive(void)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(0)));

    defs->mCaseSensitive = woort_bool(1);

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_language_definition_set_auto_indentation(void)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(0)));

    defs->mAutoIndentation = woort_bool(1);

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_language_definition_set_preproc_char(void)
{
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(0)));

    defs->mPreprocChar = (char)(char)woort_int(1);

    return woort_ret_void();
}

WOORT_API woort_api je_gui_code_editor_set_language_definition(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    TextEditor::LanguageDefinition* defs =
        std::launder(reinterpret_cast<TextEditor::LanguageDefinition*>(woort_gcpointer(1)));

    text_editor->SetLanguageDefinition(*defs);

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_undoable(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    return woort_ret_bool(text_editor->CanUndo());
}
WOORT_API woort_api je_gui_code_editor_redoable(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    return woort_ret_bool(text_editor->CanRedo());
}
WOORT_API woort_api je_gui_code_editor_undo(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->Undo();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_redo(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->Redo();
    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_get_cursor_pos(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));

    woort_value result = s + 0;
    woort_value elem = s + 1;

    auto pos = text_editor->GetCursorPosition();

    woort_set_struct(result, 2);

    woort_set_int(elem, pos.mLine);
    woort_struct_set(result, 0, elem);

    woort_set_int(elem, pos.mColumn);
    woort_struct_set(result, 1, elem);

    return woort_ret_value(result);
}
WOORT_API woort_api je_gui_code_editor_set_cursor_pos(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));

    woort_value elem = s + 0;

    woort_struct_get(elem, 1, 0);
    int line = (int)woort_int(elem);

    woort_struct_get(elem, 1, 1);
    int column = (int)woort_int(elem);

    text_editor->SetCursorPosition(TextEditor::Coordinates(line, column));

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_insert_text(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->InsertText(woort_string(1));
    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_copy(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->Copy();

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_cut(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->Cut();

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_paste(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->Paste();

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_delete(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->Delete();

    return woort_ret_void();
}
WOORT_API woort_api je_gui_code_editor_create(void)
{
    TextEditor* text_editor = new TextEditor();

    text_editor->SetPalette(TextEditor::GetDarkPalette());

    return woort_ret_gchandle(
        text_editor, 
        WOORT_IGNORE,
        [](void* p)
        {
            delete std::launder(reinterpret_cast<TextEditor*>(p));
        },
        nullptr);
}

WOORT_API woort_api je_gui_code_editor_get_text(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));

    return woort_ret_string(text_editor->GetText().c_str());
}

WOORT_API woort_api je_gui_code_editor_set_text(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));

    text_editor->SetText(woort_string(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_code_editor_show(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->Render(woort_string(0));

    return woort_ret_void();
}

WOORT_API woort_api je_gui_code_editor_show_size(void)
{
    TextEditor* text_editor = std::launder(reinterpret_cast<TextEditor*>(woort_gcpointer(0)));
    text_editor->Render(woort_string(1), val2vec2(2), woort_bool(3));

    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_context_create(void)
{
    ax::NodeEditor::Config c;
    c.SettingsFile = nullptr;

    ax::NodeEditor::EditorContext* ctx = ax::NodeEditor::CreateEditor(&c);
    return woort_ret_gchandle(
        ctx, 
        WOORT_IGNORE,
        [](void* p)
        {
            ax::NodeEditor::DestroyEditor(
                reinterpret_cast<ax::NodeEditor::EditorContext*>(p));
        },
        nullptr);
}

WOORT_API woort_api je_gui_node_editor_begin(void)
{
    ax::NodeEditor::EditorContext* ctx =
        reinterpret_cast<ax::NodeEditor::EditorContext*>(woort_gcpointer(1));

    ax::NodeEditor::SetCurrentEditor(ctx);
    ax::NodeEditor::Begin(woort_string(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_end(void)
{
    ax::NodeEditor::End();
    ax::NodeEditor::SetCurrentEditor(nullptr);
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_begin_node(void)
{
    ax::NodeEditor::BeginNode((ax::NodeEditor::NodeId)woort_int(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_end_node(void)
{
    ax::NodeEditor::EndNode();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_begin_input_pin(void)
{
    ax::NodeEditor::BeginPin((ax::NodeEditor::PinId)woort_int(0), ax::NodeEditor::PinKind::Input);
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_begin_output_pin(void)
{
    ax::NodeEditor::BeginPin((ax::NodeEditor::PinId)woort_int(0), ax::NodeEditor::PinKind::Output);
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_end_pin(void)
{
    ax::NodeEditor::EndPin();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_pin_pivot_rect(void)
{
    ax::NodeEditor::PinPivotRect(val2vec2(0), val2vec2(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_pin_rect(void)
{
    ax::NodeEditor::PinRect(val2vec2(0), val2vec2(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_link(void)
{
    ax::NodeEditor::Link(
        woort_int(0),
        woort_int(1),
        woort_int(2),
        val2colorf4(3),
        woort_float(4));

    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_begin_create(void)
{
    return woort_ret_bool(
        ax::NodeEditor::BeginCreate(
            val2colorf4(0),
            woort_float(1)));
}

WOORT_API woort_api je_gui_node_editor_end_create(void)
{
    ax::NodeEditor::EndCreate();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_begin_delete(void)
{
    return woort_ret_bool(ax::NodeEditor::BeginDelete());
}

WOORT_API woort_api je_gui_node_editor_end_delete(void)
{
    ax::NodeEditor::EndDelete();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_accept_new_item(void)
{
    return woort_ret_bool(ax::NodeEditor::AcceptNewItem());
}

WOORT_API woort_api je_gui_node_editor_reject_new_item(void)
{
    ax::NodeEditor::RejectNewItem();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_accept_new_item_color(void)
{
    return woort_ret_bool(ax::NodeEditor::AcceptNewItem(
        val2colorf4(0),
        woort_float(1)));
}

WOORT_API woort_api je_gui_node_editor_reject_new_item_color(void)
{
    ax::NodeEditor::RejectNewItem(
        val2colorf4(0),
        woort_float(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_accept_deleted_item(void)
{
    return woort_ret_bool(ax::NodeEditor::AcceptDeletedItem());
}

WOORT_API woort_api je_gui_node_editor_reject_deleted_item(void)
{
    ax::NodeEditor::RejectDeletedItem();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_query_new_link(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    ax::NodeEditor::PinId start, end;
    if (ax::NodeEditor::QueryNewLink(&start, &end))
    {
        woort_value v = s + 0;
        woort_value elem = s + 1;

        woort_set_struct(v, 2);

        woort_set_int(elem, (woort_Int)start.Get());
        woort_struct_set(v, 0, elem);

        woort_set_int(elem, (woort_Int)end.Get());
        woort_struct_set(v, 1, elem);

        return woort_ret_option_value(v);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_node_editor_query_new_node(void)
{
    ax::NodeEditor::PinId pin;
    if (ax::NodeEditor::QueryNewNode(&pin))
    {
        return woort_ret_option_int((woort_Int)pin.Get());
    }

    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_node_editor_query_deleted_node(void)
{
    ax::NodeEditor::NodeId node;
    if (ax::NodeEditor::QueryDeletedNode(&node))
    {
        return woort_ret_option_int((woort_Int)node.Get());
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_node_editor_query_deleted_link(void)
{
    ax::NodeEditor::LinkId link;
    if (ax::NodeEditor::QueryDeletedLink(&link))
    {
        return woort_ret_option_int((woort_Int)link.Get());
    }
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_node_editor_push_style_color(void)
{
    ax::NodeEditor::PushStyleColor(
        (ax::NodeEditor::StyleColor)woort_int(0),
        val2colorf4(1));

    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_pop_style_color(void)
{
    ax::NodeEditor::PopStyleColor((int)woort_int(0));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_delete_node(void)
{
    return woort_ret_bool(ax::NodeEditor::DeleteNode((ax::NodeEditor::NodeId)woort_int(0)));
}

WOORT_API woort_api je_gui_node_editor_delete_link(void)
{
    return woort_ret_bool(ax::NodeEditor::DeleteLink((ax::NodeEditor::LinkId)woort_int(0)));
}

WOORT_API woort_api je_gui_node_editor_get_node_position(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto v2 = ax::NodeEditor::GetNodePosition((ax::NodeEditor::NodeId)woort_int(0));

    woort_value result = s + 0;
    woort_value val = s + 1;

    woort_set_struct(result, 2);

    woort_set_float(val, v2.x);
    woort_struct_set(result, 0, val);

    woort_set_float(val, v2.y);
    woort_struct_set(result, 1, val);

    return woort_ret_value(result);
}

WOORT_API woort_api je_gui_node_editor_set_node_position(void)
{
    ax::NodeEditor::SetNodePosition((ax::NodeEditor::NodeId)woort_int(0), val2vec2(1));
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_get_hovered_node(void)
{
    auto id = ax::NodeEditor::GetHoveredNode();

    if ((bool)id)
        return woort_ret_option_int((woort_Int)id.Get());
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_node_editor_get_hovered_pin(void)
{
    auto id = ax::NodeEditor::GetHoveredPin();

    if ((bool)id)
        return woort_ret_option_int((woort_Int)id.Get());
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_node_editor_get_hovered_link(void)
{
    auto id = ax::NodeEditor::GetHoveredLink();

    if ((bool)id)
        return woort_ret_option_int((woort_Int)id.Get());
    return woort_ret_option_none();
}

WOORT_API woort_api je_gui_node_editor_is_node_selected(void)
{
    return woort_ret_bool(
        ax::NodeEditor::IsNodeSelected(
            (ax::NodeEditor::NodeId)woort_int(0)));
}

WOORT_API woort_api je_gui_node_editor_canvas_to_screen(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto v2 = ax::NodeEditor::CanvasToScreen(val2vec2(0));

    woort_value result = s + 0;
    woort_value val = s + 1;

    woort_set_struct(result, 2);

    woort_set_float(val, v2.x);
    woort_struct_set(result, 0, val);

    woort_set_float(val, v2.y);
    woort_struct_set(result, 1, val);

    return woort_ret_value(result);
}

WOORT_API woort_api je_gui_node_editor_screen_to_canvas(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto v2 = ax::NodeEditor::ScreenToCanvas(val2vec2(0));

    woort_value result = s + 0;
    woort_value val = s + 1;

    woort_set_struct(result, 2);

    woort_set_float(val, v2.x);
    woort_struct_set(result, 0, val);

    woort_set_float(val, v2.y);
    woort_struct_set(result, 1, val);

    return woort_ret_value(result);
}

WOORT_API woort_api je_gui_node_editor_suspend(void)
{
    ax::NodeEditor::Suspend();
    return woort_ret_void();
}

WOORT_API woort_api je_gui_node_editor_resume(void)
{
    ax::NodeEditor::Resume();
    return woort_ret_void();
}

////////////////////////////////////////////////////////////////////////////////////////////

void jegui_init_basic(
    jegl_context* gl_context,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler)
{
    _je_gui_tls_ctx._jegl_context = gl_context;
    _je_gui_tls_ctx._jegl_get_native_texture = get_img_res;
    _je_gui_tls_ctx._jegl_bind_shader_sampler_state = apply_shader_sampler;

    _je_gui_tls_ctx._jegl_rend_texture_shader = jeecs::graphic::shader::create(
        gl_context,
        "!/builtin/imgui_image_displayer.shader",
        R"(
import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (NEVER);
ZWRITE  (DISABLE);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct
    {
        vertex: float3,
        uv: float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        color: float4,
    };
    
public func vert(v: vin)
{
    return v2f{
        pos = vec4!(v.vertex, 1.),
        uv = v.uv,
    };
}

let nearest_clamp = 
    Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);

WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, nearest_clamp);
    
public func frag(vf: v2f)
{
    return fout{
        color = tex2d(Main, vf.uv),
    };
}
)").value();

    _je_gui_tls_ctx._jegui_imgui_config_path =
        jeecs_file_get_host_path() + std::string("/builtin/imgui.ini.je4cache");

    _je_gui_tls_ctx._jegui_stop_work_flag = false;
    ImGui::CreateContext();

    // Set style:
    // ImGui::StyleColorsLight();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#ifndef JE_GL_USE_EGL_INSTEAD_GLFW
    // 仅普通窗口模式下启用多视口支持
    if (gl_context->m_config.m_display_mode == jegl_interface_config::display_mode::WINDOWED)
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif

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
        void* file_buf = malloc(general_ttf_file->m_file_length);
        jeecs_file_read(file_buf, sizeof(char), general_ttf_file->m_file_length, general_ttf_file);

        void* latin_file_buf = nullptr;
        if (latin_ttf_file != nullptr)
        {
            latin_file_buf = malloc(latin_ttf_file->m_file_length);
            jeecs_file_read(latin_file_buf, sizeof(char), latin_ttf_file->m_file_length, latin_ttf_file);
        }

        if (latin_file_buf != nullptr)
        {
            // Non latin characters
            static const ImWchar ranges[] =
            {
                // 0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x2000,
                0x206F, // General Punctuation
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

        // free(file_buf); // No need to free.
        jeecs_file_close(general_ttf_file);
    }
}

void jegui_update_basic(
    jegui_platform_draw_callback_t platform_draw_callback,
    void* data)
{
    jegl_bind_shader(_je_gui_tls_ctx._jegl_rend_texture_shader.value()->resource());

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

    platform_draw_callback(data);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
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

bool jegui_shutdown_callback()
{
#ifndef NDEBUG
    jeecs::debug::loginfo("Graphic interface has been requested to close.");
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
    (void)woort_vm_swap(nullptr);

    return close;
}
