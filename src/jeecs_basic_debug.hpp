#pragma once

// ============================================================================
// jeecs_basic_debug.hpp —— 编辑器/调试支撑的内部接口
//
// 世界/实体/组件/系统的枚举与实体的调试元数据（名称、调试 EID）。
// 原 je_debug_* 与 je_ecs_entity_name_* C API 的内部化版本：消费者是
// 引擎内部的 wojeapi 调试绑定（jeecs_ecs/typeinfo/editor_woolang_api.cpp）
// 与 towoo 内核，不对游戏项目开放。线程安全性与原 C API 一致——
// 无承诺，仅限主循环/编辑器线程使用。
//
// 实现分散于：
//   jeecs_basic_arch_system.cpp           （世界/实体/组件/系统枚举）
//   jeecs_basic_type_mgr.cpp              （已注册类型枚举）
//   jeecs_core_systems_and_components.cpp （实体名称与调试 EID）
// ============================================================================

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/ecs.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/typing.hpp"

namespace jeecs::debug
{
    // 返回 universe 中所有存活的世界（以 nullptr 结尾的数组），
    // 结果用 je_mem_free 释放
    je_game_world_t** get_all_worlds(je_game_universe_t* universe);

    // 释放 get_all_entities 返回的实体包装
    void free_entity(je_game_entity_t* entity);

    // 返回世界中所有实体（get_all_worlds 的实体版：元素是新建的实体
    // 包装，须逐个用 free_entity 释放，数组本身用 je_mem_free 释放）
    je_game_entity_t** get_all_entities(je_game_world_t* world);

    // 返回实体当前持有的全部组件类型（以 nullptr 结尾，按类型 id 排序），
    // 结果用 je_mem_free 释放
    const je_type_info_t** get_all_components_of_entity(const je_game_entity_t* entity);

    // 返回类型管理器中已注册的全部类型（以 nullptr 结尾），
    // 结果用 je_mem_free 释放
    const je_type_info_t** get_all_registered_types();

    // 返回世界挂载的全部系统类型（以 nullptr 结尾），
    // 结果用 je_mem_free 释放
    const je_type_info_t** get_all_attached_systems(je_game_world_t* world);

    // 实体调试 EID（je_debug_eid_t，0 为无效值）：实体缺少 Editor::EntityId
    // 组件时 get 会补挂组件并返回 0；set 在缺少组件时先创建再设置
    je_debug_eid_t get_entity_uid(const je_game_entity_t* entity);
    void set_entity_uid(const je_game_entity_t* entity, je_debug_eid_t uid);

    // 实体名称（Editor::Name 组件，一般只用于编辑器层级面板等调试用途）。
    // 实体缺少组件时 get 返回空串；set 在缺少组件时先创建再设置，返回
    // 设置完成之后的新名称。返回的是组件内字符串地址——实体发生更新、
    // 重命名或其他操作后可能失效，须立即复制或使用完毕。
    const char* get_entity_name(const je_game_entity_t* entity);
    const char* set_entity_name(const je_game_entity_t* entity, const char* name);
}
