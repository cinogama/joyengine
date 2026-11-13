#define JE_IMPL
#include "jeecs/basic.hpp"
#include "jeecs/components.hpp"
#include "jeecs/file.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/tilemap.hpp"
#include "jeecs_core_tilemap_model.hpp"

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cassert>

// ======================================================================
// wojeapi_tilemap_*：Woolang 绑定（je/tilemap.wo 经 extern 声明引用）
// 文档在 Woolang 侧为 gchandle：载荷即文档实例指针，值被回收（GC 或
// gchandle::close）时经终结器 unload 释放其携带的 1 个引用。
// ======================================================================

// 读取第 slot 个参数（文档 gchandle）的载荷实例指针，按文档族具体化
static inline je_tilemap_document_t* woje_map_doc(woort_StackValue slot)
{
    return (je_tilemap_document_t*)woort_gcpointer(slot);
}
static inline je_tileset_document_t* woje_ts_doc(woort_StackValue slot)
{
    return (je_tileset_document_t*)woort_gcpointer(slot);
}
// 终结器：释放 gchandle 携带的文档引用。注意 Woolang 为并发 GC——
// 终结器在 GC worker 线程上执行，可能与编辑器线程、世界更新线程同时
// 调用 unload（注册表内部已用互斥锁串行化；调用方 bug 导致的双重释放
// 由引用计数下限 0 与已逐出防护兜底）。
static void woje_tilemap_release_map(void* ptr)
{
    je_tilemap_map_unload((je_tilemap_document_t*)ptr);
}
static void woje_tilemap_release_tileset(void* ptr)
{
    je_tilemap_tileset_unload((je_tileset_document_t*)ptr);
}

WOORT_API woort_api wojeapi_tilemap_open_map(void)
{
    je_tilemap_document_t* doc = je_tilemap_map_open(woort_string(0));
    if (doc == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_gchandle(
        doc, WOORT_IGNORE, woje_tilemap_release_map, nullptr);
}
WOORT_API woort_api wojeapi_tilemap_create_map(void)
{
    je_tilemap_document_t* doc = je_tilemap_map_create(
        (int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    if (doc == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_gchandle(
        doc, WOORT_IGNORE, woje_tilemap_release_map, nullptr);
}
WOORT_API woort_api wojeapi_tilemap_bind_map_path(void)
{
    return woort_ret_bool(je_tilemap_map_path_bind(
        woje_map_doc(0), woort_string(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_reload_map(void)
{
    je_tilemap_map_reload(woort_string(0));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_map_size(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t w = 0, h = 0, tpx = 0, lc = 0;
    je_tilemap_map_size(woje_map_doc(0), &w, &h, &tpx, &lc);
    woort_set_struct(s + 0, 4);
    woort_struct_set_int(s + 0, 0, w);
    woort_struct_set_int(s + 0, 1, h);
    woort_struct_set_int(s + 0, 2, tpx);
    woort_struct_set_int(s + 0, 3, lc);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_map_path(void)
{
    const char* path;
    if (je_tilemap_map_path(woje_map_doc(0), &path))
        return woort_ret_option_string(path);

    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_tilemap_map_version(void)
{
    return woort_ret_int((woort_Int)je_tilemap_map_version(woje_map_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_loaded_maps(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);

    // 快照枚举：每个实例自带 1 个引用（收集与计引用同锁完成，不会被
    // GC 终结器并发逐出），元素被 GC 回收时经终结器释放
    for (je_tilemap_document_t* doc :
        jeecs::Tilemap::documents::inst().snapshot_maps())
    {
        woort_set_gchandle(elem, doc, WOORT_IGNORE,
            woje_tilemap_release_map, nullptr);
        woort_vec_push(out_array, elem);
    }
    return woort_ret_value(out_array);
}
WOORT_API woort_api wojeapi_tilemap_resize_map(void)
{
    je_tilemap_map_resize(woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_add_layer(void)
{
    je_layer_id_t id = 0;
    if (!je_tilemap_layer_add(woje_map_doc(0), woort_string(1), &id))
        return woort_ret_option_none();
    return woort_ret_option_int(id);
}
WOORT_API woort_api wojeapi_tilemap_remove_layer(void)
{
    return woort_ret_bool(je_tilemap_layer_remove(
        woje_map_doc(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_move_layer(void)
{
    return woort_ret_bool(je_tilemap_layer_move(
        woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_get_layer(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* name = "";
    int32_t visible = 0, locked = 0;
    je_tilemap_layer_get(woje_map_doc(0), (int32_t)woort_int(1),
        &name, &visible, &locked);
    woort_set_struct(s + 0, 3);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, visible);
    woort_struct_set_int(s + 0, 2, locked);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_layer(void)
{
    je_tilemap_layer_set(woje_map_doc(0), (int32_t)woort_int(1),
        woort_string(2), (int32_t)woort_int(3), (int32_t)woort_int(4));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_z(void)
{
    return woort_ret_float(je_tilemap_layer_z(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_z(void)
{
    je_tilemap_layer_z_set(woje_map_doc(0), (int32_t)woort_int(1),
        woort_float(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_rend_queue(void)
{
    return woort_ret_int(je_tilemap_layer_rend_queue(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_rend_queue(void)
{
    je_tilemap_layer_rend_queue_set(woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_selfshadow(void)
{
    return woort_ret_int((woort_Int)je_tilemap_layer_selfshadow(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_selfshadow(void)
{
    je_tilemap_layer_selfshadow_set(woje_map_doc(0), (int32_t)woort_int(1),
        (je_layer_self_shadow_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_selfshadow_factor(void)
{
    return woort_ret_float(je_tilemap_layer_selfshadow_factor(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_selfshadow_factor(void)
{
    je_tilemap_layer_selfshadow_factor_set(woje_map_doc(0),
        (int32_t)woort_int(1), woort_float(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_blockshadow(void)
{
    return woort_ret_int((woort_Int)je_tilemap_layer_blockshadow(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_blockshadow(void)
{
    je_tilemap_layer_blockshadow_set(woje_map_doc(0), (int32_t)woort_int(1),
        (je_layer_block_shadow_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_blockshadow_factor(void)
{
    return woort_ret_float(je_tilemap_layer_blockshadow_factor(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_blockshadow_factor(void)
{
    je_tilemap_layer_blockshadow_factor_set(woje_map_doc(0),
        (int32_t)woort_int(1), woort_float(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_shader(void)
{
    je_tilemap_shader_ref_t* shader = je_tilemap_layer_shader(
        woje_map_doc(0), (int32_t)woort_int(1));
    if (shader == nullptr)
        return woort_ret_option_none();
    // 新建引用副本交由 GC 释放（与 wojeapi_shader_open 同款）
    return woort_ret_option_gchandle(
        shader, WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
        },
        nullptr);
}
WOORT_API woort_api wojeapi_tilemap_set_layer_shader(void)
{
    // 第 2 参 shader 为 option<shader>：none 表示清除样式
    woort_value s;
    je_tilemap_shader_ref_t* shader = nullptr;
    if (woort_push_reserve(1, &s) && woort_option_get(s, 2))
        shader = (je_tilemap_shader_ref_t*)woort_gcpointer(s);
    je_tilemap_layer_shader_set(
        woje_map_doc(0), (int32_t)woort_int(1), shader);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_texture_count(void)
{
    return woort_ret_int(je_tilemap_layer_texture_count(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_layer_set_texture(void)
{
    // 第 3 参 texture 为 option<texture>：none 表示删除该槽绑定
    woort_value s;
    je_tilemap_texture_ref_t* texture = nullptr;
    if (woort_push_reserve(1, &s) && woort_option_get(s, 3))
        texture = (je_tilemap_texture_ref_t*)woort_gcpointer(s);
    return woort_ret_bool(je_tilemap_layer_texture_set(
        woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2),
        texture) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_layer_get_texture(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t slot = 0;
    je_tilemap_texture_ref_t* texture = nullptr;
    je_tilemap_layer_texture_get(woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), &slot, &texture);
    woort_set_struct(s + 0, 2);
    woort_struct_set_int(s + 0, 0, slot);
    if (texture != nullptr)
        woort_set_option_gchandle(
            s + 1, texture, WOORT_IGNORE,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::texture2d> *)ptr;
            },
            nullptr);
    else
        woort_set_option_none(s + 1);
    woort_struct_set(s + 0, 1, s + 1);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_get_tile(void)
{
    return woort_ret_int(je_tilemap_tile_get(
        woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2), (int32_t)woort_int(3)));
}
WOORT_API woort_api wojeapi_tilemap_layer_grid(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    int32_t count = 0;
    const int32_t* grid = je_tilemap_layer_grid(woje_map_doc(0),
        (int32_t)woort_int(1), &count);

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);
    for (int32_t i = 0; i < count; ++i)
    {
        woort_set_box_int(elem, grid[i]);
        woort_vec_push(out_array, elem);
    }
    return woort_ret_value(out_array);
}
WOORT_API woort_api wojeapi_tilemap_set_tile(void)
{
    je_tilemap_tile_set(woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), (int32_t)woort_int(4));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_fill_tiles(void)
{
    je_tilemap_tile_fill(woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3),
        (int32_t)woort_int(4), (int32_t)woort_int(5), (int32_t)woort_int(6));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_count(void)
{
    return woort_ret_int(je_tilemap_tileset_count(woje_map_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_add_tileset(void)
{
    int32_t index = 0;
    if (!je_tilemap_tileset_add(woje_map_doc(0), woort_string(1), &index))
        return woort_ret_option_none();
    return woort_ret_option_int(index);
}
WOORT_API woort_api wojeapi_tilemap_remove_tileset(void)
{
    return woort_ret_bool(je_tilemap_tileset_remove(
        woje_map_doc(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tileset_path(void)
{
    return woort_ret_string(je_tilemap_tileset_path(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
// ---- 逐格属性覆盖枚举（编辑器序列化用） ----
WOORT_API woort_api wojeapi_tilemap_cell_property_entries(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const int32_t count =
        je_tilemap_cell_property_entries(woje_map_doc(0), nullptr, 0);
    std::vector<je_tilemap_cell_property_entry_t> entries((size_t)count);
    if (count > 0)
        (void)je_tilemap_cell_property_entries(
            woje_map_doc(0), entries.data(), count);

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);
    for (const auto& entry : entries)
    {
        woort_set_struct(elem, 4);
        woort_struct_set_int(elem, 0, entry.m_layer);
        woort_struct_set_int(elem, 1, entry.m_x);
        woort_struct_set_int(elem, 2, entry.m_y);
        woort_struct_set_int(elem, 3, entry.m_property_count);
        woort_vec_push(out_array, elem);
    }
    return woort_ret_value(out_array);
}
WOORT_API woort_api wojeapi_tilemap_cell_property_entry_property_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    const char* name = nullptr;
    je_property_type_t type = JE_PROPERTY_TYPE_BOOL;
    if (!je_tilemap_cell_property_entry_property_info(woje_map_doc(0),
        (int32_t)woort_int(1), (int32_t)woort_int(2), (int32_t)woort_int(3),
        (int32_t)woort_int(4), &name, &type))
        return woort_ret_option_none();

    woort_set_struct(s + 0, 2);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, (int)type);
    // 载荷为元组：经 set_option_value 包装进 option 返回
    woort_set_option_value(WOORT_RETURN_SLOT, s + 0);
    return woort_ret();
}
// ---- 属性值的 Woolang 编组 ----
// 属性接口按 je_property_type_t 拆成每类型一个 extern（Woolang 侧用泛型
// 封装按值的静态类型分发，与 shader::set_uniform 同款做法）。
// 各宏中 s/pv 由使用点声明：PROPVAL_* 从 option<T> 参数解出值指针
//（none → nullptr，即清除/零默认）；SETTER/GETTER/ADDER/FINDER 生成
// 单个 extern 的完整函数体。
#define JE_WO_PROPVAL_BOOL(SLOT)                                       \
    bool propv = false;                                                \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        propv = woort_bool(s);                                         \
        pv = &propv;                                                   \
    }
#define JE_WO_PROPVAL_INT(SLOT)                                        \
    int32_t propv = 0;                                                 \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        propv = (int32_t)woort_int(s);                                 \
        pv = &propv;                                                   \
    }
#define JE_WO_PROPVAL_REAL(SLOT)                                       \
    float propv = 0.f;                                                 \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        propv = (float)woort_real(s);                                  \
        pv = &propv;                                                   \
    }
#define JE_WO_PROPVAL_STRING(SLOT)                                     \
    const char* propv = nullptr;                                       \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        propv = woort_string(s);                                       \
        pv = propv;                                                    \
    }
#define JE_WO_PROPVAL_VEC(SLOT, N)                                     \
    float propv[N] = { 0 };                                            \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        for (size_t i = 0; i < N; ++i)                                 \
            propv[i] = (float)woort_struct_get_real(s, i);             \
        pv = propv;                                                    \
    }
#define JE_WO_PROPVAL_VEC2(SLOT) JE_WO_PROPVAL_VEC(SLOT, 2)
#define JE_WO_PROPVAL_VEC3(SLOT) JE_WO_PROPVAL_VEC(SLOT, 3)
#define JE_WO_PROPVAL_VEC4(SLOT) JE_WO_PROPVAL_VEC(SLOT, 4)

// set 型：option<T> 值（nullptr = 清除）→ bool
#define JE_WO_PROP_SETTER(FN, CALL, SLOT, PROPVAL)                     \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    woort_value s;                                                     \
    const void* pv = nullptr;                                          \
    PROPVAL(SLOT)                                                      \
    return woort_ret_bool(CALL(pv) ? true : false);                    \
}
// get 型：C 调用成功（bool）→ option<T>；vecN 打包为 real 元组
#define JE_WO_PROP_GETTER(FN, CALL, CTYPE, INIT, RETOPT)               \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    CTYPE propv INIT;                                                  \
    if (CALL(&propv))                                                  \
        return RETOPT(propv);                                          \
    return woort_ret_option_none();                                    \
}
#define JE_WO_PROP_VECGETTER(FN, CALL, N)                              \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    float propv[N] = { 0 };                                            \
    if (CALL(propv))                                                   \
    {                                                                  \
        woort_value s;                                                 \
        if (!woort_push_reserve(1, &s))                                \
            return woort_ret_panic("Stack overflow.");                 \
        woort_set_struct(s + 0, N);                                    \
        for (size_t i = 0; i < N; ++i)                                 \
            woort_struct_set_real(s + 0, i, propv[i]);                 \
        woort_set_option_value(WOORT_RETURN_SLOT, s + 0);              \
        return woort_ret();                                            \
    }                                                                  \
    return woort_ret_option_none();                                    \
}
// add_property 型：option<T> 默认值 → option<int>（新属性下标）
#define JE_WO_PROP_ADDER(FN, TYPE, PROPVAL)                            \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    woort_value s;                                                     \
    const void* pv = nullptr;                                          \
    PROPVAL(2)                                                         \
    int32_t index = 0;                                                 \
    if (!je_tilemap_property_add(woje_ts_doc(0), woort_string(1),      \
        TYPE, pv, &index))                                             \
        return woort_ret_option_none();                                \
    return woort_ret_option_int(index);                                \
}
// find_cells 型：T 值（非 option）→ int（命中数）
#define JE_WO_PROP_FINDER(FN, TYPE, FINDARG)                           \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    FINDARG;                                                           \
    return woort_ret_int(je_tilemap_cell_find(woje_map_doc(0),        \
        (int32_t)woort_int(1), woort_string(2), TYPE, fvptr));         \
}
#define JE_WO_FINDARG_BOOL                                             \
    bool fv = woort_bool(3);                                           \
    const void* fvptr = &fv
#define JE_WO_FINDARG_INT                                              \
    int32_t fv = (int32_t)woort_int(3);                                \
    const void* fvptr = &fv
#define JE_WO_FINDARG_REAL                                             \
    float fv = (float)woort_real(3);                                   \
    const void* fvptr = &fv
#define JE_WO_FINDARG_STRING                                           \
    const char* fv = woort_string(3);                                  \
    const void* fvptr = fv
#define JE_WO_FINDARG_VEC2                                             \
    float fv[2] = { (float)woort_struct_get_real(3, 0),                \
                    (float)woort_struct_get_real(3, 1) };              \
    const void* fvptr = fv
#define JE_WO_FINDARG_VEC3                                             \
    float fv[3] = { (float)woort_struct_get_real(3, 0),                \
                    (float)woort_struct_get_real(3, 1),                \
                    (float)woort_struct_get_real(3, 2) };              \
    const void* fvptr = fv
#define JE_WO_FINDARG_VEC4                                             \
    float fv[4] = { (float)woort_struct_get_real(3, 0),                \
                    (float)woort_struct_get_real(3, 1),                \
                    (float)woort_struct_get_real(3, 2),                \
                    (float)woort_struct_get_real(3, 3) };              \
    const void* fvptr = fv

// ---- 地图侧：逐格覆盖 / 取值链解析 / 查找 ----
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_bool,
    ([&](const void* pv)
        { return je_tilemap_cell_property_set(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_BOOL, pv); }),
    5, JE_WO_PROPVAL_BOOL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_int,
    ([&](const void* pv)
        { return je_tilemap_cell_property_set(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_INT, pv); }),
    5, JE_WO_PROPVAL_INT)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_real,
    ([&](const void* pv)
        { return je_tilemap_cell_property_set(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_FLOAT, pv); }),
    5, JE_WO_PROPVAL_REAL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_string,
    ([&](const void* pv)
        { return je_tilemap_cell_property_set(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_STRING, pv); }),
    5, JE_WO_PROPVAL_STRING)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_vec2,
    ([&](const void* pv)
        { return je_tilemap_cell_property_set(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC2, pv); }),
    5, JE_WO_PROPVAL_VEC2)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_vec3,
    ([&](const void* pv)
        { return je_tilemap_cell_property_set(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC3, pv); }),
    5, JE_WO_PROPVAL_VEC3)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_vec4,
    ([&](const void* pv)
        { return je_tilemap_cell_property_set(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC4, pv); }),
    5, JE_WO_PROPVAL_VEC4)

JE_WO_PROP_GETTER(wojeapi_tilemap_get_cell_property_bool,
    ([&](void* out)
        { return je_tilemap_cell_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_cell_property_int,
    ([&](void* out)
        { return je_tilemap_cell_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_cell_property_real,
    ([&](void* out)
        { return je_tilemap_cell_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_cell_property_string,
    ([&](void* out)
        { return je_tilemap_cell_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_cell_property_vec2,
    ([&](void* out)
        { return je_tilemap_cell_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_cell_property_vec3,
    ([&](void* out)
        { return je_tilemap_cell_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_cell_property_vec4,
    ([&](void* out)
        { return je_tilemap_cell_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC4, out); }),
    4)

JE_WO_PROP_GETTER(wojeapi_tilemap_get_property_bool,
    ([&](void* out)
        { return je_tilemap_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_property_int,
    ([&](void* out)
        { return je_tilemap_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_property_real,
    ([&](void* out)
        { return je_tilemap_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_property_string,
    ([&](void* out)
        { return je_tilemap_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_property_vec2,
    ([&](void* out)
        { return je_tilemap_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_property_vec3,
    ([&](void* out)
        { return je_tilemap_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_property_vec4,
    ([&](void* out)
        { return je_tilemap_property_get(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC4, out); }),
    4)

JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_bool,
    JE_PROPERTY_TYPE_BOOL, JE_WO_FINDARG_BOOL)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_int,
    JE_PROPERTY_TYPE_INT, JE_WO_FINDARG_INT)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_real,
    JE_PROPERTY_TYPE_FLOAT, JE_WO_FINDARG_REAL)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_string,
    JE_PROPERTY_TYPE_STRING, JE_WO_FINDARG_STRING)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_vec2,
    JE_PROPERTY_TYPE_VEC2, JE_WO_FINDARG_VEC2)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_vec3,
    JE_PROPERTY_TYPE_VEC3, JE_WO_FINDARG_VEC3)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_vec4,
    JE_PROPERTY_TYPE_VEC4, JE_WO_FINDARG_VEC4)
WOORT_API woort_api wojeapi_tilemap_is_walkable(void)
{
    return woort_ret_int(je_tilemap_is_walkable(
        woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2)));
}
WOORT_API woort_api wojeapi_tilemap_is_walkable_on(void)
{
    return woort_ret_int(je_tilemap_is_walkable_on(
        woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3)));
}
WOORT_API woort_api wojeapi_tilemap_find_get(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t x = -1, y = -1;
    je_tilemap_cell_find_get(woje_map_doc(0), (int32_t)woort_int(1), &x, &y);
    woort_set_struct(s + 0, 2);
    woort_struct_set_int(s + 0, 0, x);
    woort_struct_set_int(s + 0, 1, y);
    return woort_ret_value(s + 0);
}

// ---- 引用计数与相等性（gchandle 载荷即文档实例指针；同一文档按路径
// 共享唯一实例，指针相等必为同一文档，比较安全）----
WOORT_API woort_api wojeapi_tilemap_keep_map(void)
{
    return woort_ret_int(je_tilemap_map_keep(woje_map_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_unload_map(void)
{
    return woort_ret_int(je_tilemap_map_unload(woje_map_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_map_same(void)
{
    return woort_ret_bool(
        woje_map_doc(0) == woje_map_doc(1) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_keep_tileset(void)
{
    return woort_ret_int(je_tilemap_tileset_keep(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_unload_tileset(void)
{
    return woort_ret_int(je_tilemap_tileset_unload(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_same(void)
{
    return woort_ret_bool(
        woje_ts_doc(0) == woje_ts_doc(1) ? true : false);
}

WOORT_API woort_api wojeapi_tilemap_open_tileset(void)
{
    je_tileset_document_t* doc = je_tilemap_tileset_open(woort_string(0));
    if (doc == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_gchandle(
        doc, WOORT_IGNORE, woje_tilemap_release_tileset, nullptr);
}
WOORT_API woort_api wojeapi_tilemap_create_tileset(void)
{
    je_tileset_document_t* doc = je_tilemap_tileset_create(woort_string(0));
    if (doc == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_gchandle(
        doc, WOORT_IGNORE, woje_tilemap_release_tileset, nullptr);
}
WOORT_API woort_api wojeapi_tilemap_bind_tileset_path(void)
{
    return woort_ret_bool(je_tilemap_tileset_path_bind(
        woje_ts_doc(0), woort_string(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_reload_tileset(void)
{
    je_tilemap_tileset_reload(woort_string(0));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_version(void)
{
    return woort_ret_int((woort_Int)je_tilemap_tileset_version(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_doc_path(void)
{
    return woort_ret_string(je_tilemap_tileset_doc_path(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_set_name(void)
{
    je_tilemap_tileset_name_set(woje_ts_doc(0), woort_string(1));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_name(void)
{
    return woort_ret_string(je_tilemap_tileset_name(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_next_tile_id(void)
{
    return woort_ret_int(
        je_tilemap_tileset_next_tile_id(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_add_source(void)
{
    int32_t index = 0;
    if (!je_tilemap_source_add(
        woje_ts_doc(0), woort_string(1), (int32_t)woort_int(2), &index))
        return woort_ret_option_none();
    return woort_ret_option_int(index);
}
WOORT_API woort_api wojeapi_tilemap_remove_source(void)
{
    return woort_ret_bool(je_tilemap_source_remove(
        woje_ts_doc(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_source_count(void)
{
    return woort_ret_int(je_tilemap_source_count(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_source_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* path = "";
    int32_t tpx = 0, xc = 0, yc = 0;
    je_tilemap_source_info(woje_ts_doc(0), (int32_t)woort_int(1),
        &path, &tpx, &xc, &yc);
    woort_set_struct(s + 0, 4);
    woort_struct_set_string(s + 0, 0, path);
    woort_struct_set_int(s + 0, 1, tpx);
    woort_struct_set_int(s + 0, 2, xc);
    woort_struct_set_int(s + 0, 3, yc);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_source_tex_size(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t tw = 0, th = 0;
    je_tilemap_source_tex_size(woje_ts_doc(0), (int32_t)woort_int(1),
        &tw, &th);
    woort_set_struct(s + 0, 2);
    woort_struct_set_int(s + 0, 0, tw);
    woort_struct_set_int(s + 0, 1, th);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_source_cell_ids(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    int32_t count = 0;
    const int32_t* ids = je_tilemap_source_cell_ids(woje_ts_doc(0),
        (int32_t)woort_int(1), &count);

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);
    for (int32_t i = 0; i < count; ++i)
    {
        woort_set_box_int(elem, ids[i]);
        woort_vec_push(out_array, elem);
    }
    return woort_ret_value(out_array);
}
WOORT_API woort_api wojeapi_tilemap_source_extra_count(void)
{
    return woort_ret_int(je_tilemap_source_extra_count(
        woje_ts_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_add_source_extra(void)
{
    return woort_ret_bool(je_tilemap_source_extra_add(
        woje_ts_doc(0), (int32_t)woort_int(1),
        woort_string(2), (int32_t)woort_int(3)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_remove_source_extra(void)
{
    return woort_ret_bool(je_tilemap_source_extra_remove(
        woje_ts_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2))
        ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_source_extra_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* path = "";
    int32_t pass = 0;
    je_tilemap_source_extra_info(woje_ts_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), &pass, &path);
    woort_set_struct(s + 0, 2);
    woort_struct_set_int(s + 0, 0, pass);
    woort_struct_set_string(s + 0, 1, path);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_source_extra_tex_size(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t tw = 0, th = 0;
    je_tilemap_source_extra_tex_size(woje_ts_doc(0),
        (int32_t)woort_int(1), (int32_t)woort_int(2), &tw, &th);
    woort_set_struct(s + 0, 2);
    woort_struct_set_int(s + 0, 0, tw);
    woort_struct_set_int(s + 0, 1, th);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_source_extra_pass(void)
{
    return woort_ret_bool(je_tilemap_source_extra_pass_set(
        woje_ts_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tile_id_at(void)
{
    je_tile_id_t id = 0;
    if (!je_tilemap_tile_id_at(woje_ts_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), &id))
        return woort_ret_int(0);
    return woort_ret_int(id);
}
WOORT_API woort_api wojeapi_tilemap_tile_count(void)
{
    return woort_ret_int(je_tilemap_tile_count(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tile_at_index(void)
{
    je_tile_id_t id = 0;
    if (!je_tilemap_tile_at_index(woje_ts_doc(0), (int32_t)woort_int(1), &id))
        return woort_ret_int(0);
    return woort_ret_int(id);
}
WOORT_API woort_api wojeapi_tilemap_tile_defs(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const int32_t count = je_tilemap_tile_defs(woje_ts_doc(0), nullptr, 0);
    std::vector<je_tile_id_t> ids((size_t)count);
    if (count > 0)
        (void)je_tilemap_tile_defs(woje_ts_doc(0), ids.data(), count);

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);
    for (je_tile_id_t id : ids)
    {
        woort_set_box_int(elem, id);
        woort_vec_push(out_array, elem);
    }
    return woort_ret_value(out_array);
}
WOORT_API woort_api wojeapi_tilemap_tile_def_property_count(void)
{
    return woort_ret_int(je_tilemap_tile_def_property_count(
        woje_ts_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_tile_def_property_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    const char* name = nullptr;
    je_property_type_t type = JE_PROPERTY_TYPE_BOOL;
    if (!je_tilemap_tile_def_property_info(woje_ts_doc(0),
        (int32_t)woort_int(1), (int32_t)woort_int(2), &name, &type))
        return woort_ret_option_none();

    woort_set_struct(s + 0, 2);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, (int)type);
    // 载荷为元组：经 set_option_value 包装进 option 返回
    woort_set_option_value(WOORT_RETURN_SLOT, s + 0);
    return woort_ret();
}
WOORT_API woort_api wojeapi_tilemap_tile_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t src = 0, ix = 0, iy = 0, wk = 1;
    if (!je_tilemap_tile_info(woje_ts_doc(0), (int32_t)woort_int(1),
        &src, &ix, &iy, &wk))
        src = -1;
    woort_set_struct(s + 0, 4);
    woort_struct_set_int(s + 0, 0, src);
    woort_struct_set_int(s + 0, 1, ix);
    woort_struct_set_int(s + 0, 2, iy);
    woort_struct_set_int(s + 0, 3, wk);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_tile_name(void)
{
    return woort_ret_string(
        je_tilemap_tile_name(woje_ts_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_tile_name(void)
{
    return woort_ret_bool(je_tilemap_tile_name_set(woje_ts_doc(0),
        (int32_t)woort_int(1), woort_string(2)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tile_kind(void)
{
    return woort_ret_int((int32_t)je_tilemap_tile_kind(
        woje_ts_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_tile_kind(void)
{
    return woort_ret_bool(je_tilemap_tile_kind_set(woje_ts_doc(0),
        (int32_t)woort_int(1), (je_tile_kind_t)woort_int(2)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tile_variant_count(void)
{
    return woort_ret_int(je_tilemap_tile_variant_count(
        woje_ts_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_tile_variant(void)
{
    je_tile_id_t id = 0;
    if (!je_tilemap_tile_variant(woje_ts_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), &id))
        return woort_ret_int(0);
    return woort_ret_int(id);
}
WOORT_API woort_api wojeapi_tilemap_tile_clear_variants(void)
{
    je_tilemap_tile_variant_clear(woje_ts_doc(0), (int32_t)woort_int(1));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tile_add_variant(void)
{
    return woort_ret_bool(je_tilemap_tile_variant_add(
        woje_ts_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_set_tile_walkable(void)
{
    je_tilemap_tile_walkable_set(woje_ts_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tile_audio(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    float occlusion = 0.f, muffle = 0.f;
    je_tilemap_tile_audio(woje_ts_doc(0), (int32_t)woort_int(1),
        &occlusion, &muffle);
    woort_set_struct(s + 0, 2);
    woort_struct_set_float(s + 0, 0, occlusion);
    woort_struct_set_float(s + 0, 1, muffle);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_tile_audio(void)
{
    return woort_ret_bool(je_tilemap_tile_audio_set(woje_ts_doc(0),
        (int32_t)woort_int(1), woort_float(2), woort_float(3))
        ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tile_normal(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    float x = 0.f, y = 0.f, z = -1.f;
    je_tilemap_tile_normal(woje_ts_doc(0), (int32_t)woort_int(1), &x, &y, &z);
    woort_set_struct(s + 0, 3);
    woort_struct_set_float(s + 0, 0, x);
    woort_struct_set_float(s + 0, 1, y);
    woort_struct_set_float(s + 0, 2, z);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_tile_normal(void)
{
    return woort_ret_bool(je_tilemap_tile_normal_set(woje_ts_doc(0),
        (int32_t)woort_int(1), woort_float(2), woort_float(3), woort_float(4))
        ? true : false);
}
// ---- 图集侧：schema 与默认值（按类型展开）----
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_bool,
    JE_PROPERTY_TYPE_BOOL, JE_WO_PROPVAL_BOOL)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_int,
    JE_PROPERTY_TYPE_INT, JE_WO_PROPVAL_INT)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_real,
    JE_PROPERTY_TYPE_FLOAT, JE_WO_PROPVAL_REAL)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_string,
    JE_PROPERTY_TYPE_STRING, JE_WO_PROPVAL_STRING)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_vec2,
    JE_PROPERTY_TYPE_VEC2, JE_WO_PROPVAL_VEC2)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_vec3,
    JE_PROPERTY_TYPE_VEC3, JE_WO_PROPVAL_VEC3)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_vec4,
    JE_PROPERTY_TYPE_VEC4, JE_WO_PROPVAL_VEC4)

WOORT_API woort_api wojeapi_tilemap_remove_property(void)
{
    return woort_ret_bool(je_tilemap_property_remove(
        woje_ts_doc(0), woort_string(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_property_count(void)
{
    return woort_ret_int(je_tilemap_property_count(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_property_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* name = "";
    je_property_type_t type = JE_PROPERTY_TYPE_BOOL;
    je_tilemap_property_info(woje_ts_doc(0), (int32_t)woort_int(1),
        &name, &type);
    woort_set_struct(s + 0, 2);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, (int32_t)type);
    return woort_ret_value(s + 0);
}

JE_WO_PROP_GETTER(wojeapi_tilemap_property_default_bool,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_property_default_int,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_property_default_real,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_property_default_string,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_property_default_vec2,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_property_default_vec3,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_property_default_vec4,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_VEC4, out); }),
    4)

// ---- 瓦片默认值 ----
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_bool,
    ([&](const void* pv)
        { return je_tilemap_tile_property_set(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_BOOL, pv); }),
    3, JE_WO_PROPVAL_BOOL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_int,
    ([&](const void* pv)
        { return je_tilemap_tile_property_set(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_INT, pv); }),
    3, JE_WO_PROPVAL_INT)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_real,
    ([&](const void* pv)
        { return je_tilemap_tile_property_set(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_FLOAT, pv); }),
    3, JE_WO_PROPVAL_REAL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_string,
    ([&](const void* pv)
        { return je_tilemap_tile_property_set(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_STRING, pv); }),
    3, JE_WO_PROPVAL_STRING)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_vec2,
    ([&](const void* pv)
        { return je_tilemap_tile_property_set(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC2, pv); }),
    3, JE_WO_PROPVAL_VEC2)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_vec3,
    ([&](const void* pv)
        { return je_tilemap_tile_property_set(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC3, pv); }),
    3, JE_WO_PROPVAL_VEC3)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_vec4,
    ([&](const void* pv)
        { return je_tilemap_tile_property_set(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC4, pv); }),
    3, JE_WO_PROPVAL_VEC4)

JE_WO_PROP_GETTER(wojeapi_tilemap_get_tile_property_bool,
    ([&](void* out)
        { return je_tilemap_tile_property_get(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_tile_property_int,
    ([&](void* out)
        { return je_tilemap_tile_property_get(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_tile_property_real,
    ([&](void* out)
        { return je_tilemap_tile_property_get(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_tile_property_string,
    ([&](void* out)
        { return je_tilemap_tile_property_get(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_tile_property_vec2,
    ([&](void* out)
        { return je_tilemap_tile_property_get(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_tile_property_vec3,
    ([&](void* out)
        { return je_tilemap_tile_property_get(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_tile_property_vec4,
    ([&](void* out)
        { return je_tilemap_tile_property_get(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC4, out); }),
    4)

WOORT_API woort_api wojeapi_tilemap_cell_quad_count(void)
{
    return woort_ret_int(je_tilemap_cell_quad_count(
        woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3)));
}
WOORT_API woort_api wojeapi_tilemap_cell_quad(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* path = "";
    float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;
    int32_t qx = 0, qy = 0, qcols = 1;
    if (je_tilemap_cell_quad(
        woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), (int32_t)woort_int(4),
        &path, &u0, &v0, &u1, &v1, &qx, &qy, &qcols))
    {
        woort_set_struct(s + 0, 8);
        woort_struct_set_string(s + 0, 0, path);
        woort_struct_set_real(s + 0, 1, u0);
        woort_struct_set_real(s + 0, 2, v0);
        woort_struct_set_real(s + 0, 3, u1);
        woort_struct_set_real(s + 0, 4, v1);
        woort_struct_set_int(s + 0, 5, qx);
        woort_struct_set_int(s + 0, 6, qy);
        woort_struct_set_int(s + 0, 7, qcols);
        // 载荷为元组：经 set_option_value 包装进 option 返回
        woort_set_option_value(WOORT_RETURN_SLOT, s + 0);
        return woort_ret();
    }
    return woort_ret_option_none();
}

// ======================================================================
// 缓存预生成（导出流程用）：校验 <path>.je4cache，缺失/失效时调用
// builtin/script/internal/cache/{tileset,tilemap}.wo 重建。
// 导出的运行时没有 je/pkg 脚本源码，无法在游戏内加载生成器，因此由
// 编辑器进程提前生成；随后的打包流程会按通用 .je4cache 规则一并打包。
// ======================================================================
WOORT_API woort_api wojeapi_tileset_cache_ensure(void)
{
    je_file_t* cache = je_tileset_cache_open_or_create(woort_string(0));
    if (cache == nullptr)
        return woort_ret_bool(false);

    je_file_close(cache);
    return woort_ret_bool(true);
}
WOORT_API woort_api wojeapi_tilemap_cache_ensure(void)
{
    je_file_t* cache = je_tilemap_cache_open_or_create(woort_string(0));
    if (cache == nullptr)
        return woort_ret_bool(false);

    je_file_close(cache);
    return woort_ret_bool(true);
}
