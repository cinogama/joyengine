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
    wo_vm work_vm;
    gui_wo_job_coroutine* last;
};
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_job_list;
jeecs::basic::atomic_list<gui_wo_job_coroutine> _wo_new_job_list;

struct _jegui_thread_local_context
{
    jegl_context* _jegl_context = nullptr;
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

WO_API wo_api je_gui_get_main_viewport_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    auto vpos = ImGui::GetMainViewport()->Pos;

    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, vpos.x, vpos.y));
}

WO_API wo_api je_gui_get_window_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    auto wpos = ImGui::GetWindowPos();

    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, wpos.x, wpos.y));
}

WO_API wo_api je_gui_get_mouse_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto&& mpos = ImGui::GetMousePos();
    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, mpos.x, mpos.y));
}

WO_API wo_api je_gui_get_mouse_wheel(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto& imgui_io = ImGui::GetIO();
    return wo_ret_val(vm, set_float2_to_struct(s + 0, vm, imgui_io.MouseWheelH, imgui_io.MouseWheel));
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
    jegl_using_resource((*texture)->resource());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resource()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resource()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }
    dlist->AddCallback(
        [](auto, auto) {
            _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
                _je_gui_tls_ctx._jegl_context,
                _je_gui_tls_ctx._jegl_rend_texture_shader->resource());
        },
        nullptr);
    dlist->AddImage(
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        val2vec2(args + 1),
        val2vec2(args + 2),
        uvmin,
        uvmax,
        val2color32(args + 4));
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

WO_API wo_api je_gui_is_key_down(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, ImGui::IsKeyDown((ImGuiKey)wo_int(args + 0)));
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

    jegl_using_resource((*texture)->resource());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resource()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resource()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {
        _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader->resource());
        },
        nullptr);
    ImGui::Image(
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        ImVec2(
            (float)((*texture)->resource())->m_raw_texture_data->m_width,
            (float)((*texture)->resource())->m_raw_texture_data->m_height
        ),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_image_scale(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resource());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resource()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resource()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {
        _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader->resource()); },
        nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
        _je_gui_tls_ctx._jegl_context,
        (*texture)->resource()),
        ImVec2(
            ((*texture)->resource())->m_raw_texture_data->m_width * wo_float(args + 1),
            ((*texture)->resource())->m_raw_texture_data->m_height * wo_float(args + 1)
        ),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_void(vm);
}
WO_API wo_api je_gui_image_size(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resource());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resource()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resource()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {
        _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader->resource());
        },
        nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
        _je_gui_tls_ctx._jegl_context,
        (*texture)->resource()),
        ImVec2(
            wo_float(args + 1),
            wo_float(args + 2)
        ),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_image_size_color(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    jegl_using_resource((*texture)->resource());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resource()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resource()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {
        _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader->resource());
        },
        nullptr);
    ImGui::Image((ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
        _je_gui_tls_ctx._jegl_context,
        (*texture)->resource()),
        ImVec2(
            wo_float(args + 1),
            wo_float(args + 2)
        ),
        uvmin,
        uvmax,
        val2colorf4(args + 3));

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_void(vm);
}

WO_API wo_api je_gui_imagebutton(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 1);

    jegl_using_resource((*texture)->resource());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resource()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resource()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {
        _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader->resource());
        },
        nullptr);
    result = ImGui::ImageButton(
        label,
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        ImVec2(
            (float)((*texture)->resource())->m_raw_texture_data->m_width,
            (float)((*texture)->resource())->m_raw_texture_data->m_height
        ),
        uvmin,
        uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_imagebutton_scale(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 1);

    jegl_using_resource((*texture)->resource());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resource()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resource()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {
        _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader->resource());
        },
        nullptr);
    result = ImGui::ImageButton(
        label,
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        ImVec2(
            ((*texture)->resource())->m_raw_texture_data->m_width * wo_float(args + 2),
            ((*texture)->resource())->m_raw_texture_data->m_height * wo_float(args + 2)
        ), uvmin, uvmax);

    dlist->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return wo_ret_bool(vm, result);
}
WO_API wo_api je_gui_imagebutton_size(wo_vm vm, wo_value args)
{
    wo_string_t label = wo_string(args + 0);
    jeecs::basic::resource<jeecs::graphic::texture>* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 1);

    jegl_using_resource((*texture)->resource());

    ImVec2 uvmin = ImVec2(0.0f, 1.0f), uvmax = ImVec2(1.0f, 0.0f);
    if (_je_gui_tls_ctx._jegui_need_flip_frambuf
        && (*texture)->resource()->m_raw_texture_data != nullptr
        && 0 != ((*texture)->resource()->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF))
    {
        uvmin = ImVec2(0.0f, 0.0f);
        uvmax = ImVec2(1.0f, 1.0f);
    }

    bool result = false;

    auto* dlist = ImGui::GetWindowDrawList();
    dlist->AddCallback([](auto, auto) {
        _je_gui_tls_ctx._jegl_bind_shader_sampler_state(
            _je_gui_tls_ctx._jegl_context,
            _je_gui_tls_ctx._jegl_rend_texture_shader->resource());
        },
        nullptr);
    result = ImGui::ImageButton(
        label,
        (ImTextureID)_je_gui_tls_ctx._jegl_get_native_texture(
            _je_gui_tls_ctx._jegl_context,
            (*texture)->resource()),
        ImVec2(
            wo_float(args + 2),
            wo_float(args + 3)
        ),
        uvmin,
        uvmax);

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

    int current_item = -1;
    if (wo_option_get(elem, args + 2))
        current_item = (int)wo_int(elem);

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
    ImGuiKey kcode = (ImGuiKey)wo_int(args + 0);

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
    jegl_context* gl_context,
    bool need_flip_frame_buf,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler)
{
    _je_gui_tls_ctx._jegl_context = gl_context;
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

void jegui_update_basic(
    jegui_platform_draw_callback_t platform_draw_callback,
    void* data)
{
    jegl_using_resource(_je_gui_tls_ctx._jegl_rend_texture_shader->resource());

    ImGui::NewFrame();
    auto* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags
        |= ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoDocking;
    host_window_flags
        |= ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground;

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
