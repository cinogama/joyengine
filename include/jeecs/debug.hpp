#pragma once

// ============================================================================
// jeecs/debug.hpp —— 调试 API（编辑器专用，需 JE_ENABLE_DEBUG_API）：je_debug_* C API
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/typing.hpp"
#include "jeecs/logging.hpp"

// 本头整体内容在 JE_ENABLE_DEBUG_API 开启时才可见（与拆分前一致）。
JE_FORCE_CAPI

// DEBUG API, SHOULD NOT BE USED IN GAME PROJECT, ONLY USED FOR EDITOR
#ifdef JE_ENABLE_DEBUG_API

// NOTE: need free the return result by 'je_mem_free'
// will return all alive world pointer in the universe.
// [world1, world2,..., nullptr]
JE_API je_game_world_t** je_debug_universe_worlds_get_all(je_game_universe_t* _universes);

JE_API void je_debug_entity_free(je_game_entity_t* _entity_list);

// NOTE: need free the return result by 'je_mem_free'(and elem with je_debug_entity_free)
JE_API je_game_entity_t** je_debug_world_entities_get_all(je_game_world_t* _world);

// NOTE: need free the return result by 'je_mem_free'
JE_API const je_type_info_t** je_debug_entity_components_get_all(const je_game_entity_t* _entity);

// NOTE: need free the return result by 'je_mem_free'
JE_API const je_type_info_t** je_debug_types_registered_get_all(void);

// NOTE: need free the return result by 'je_mem_free'
JE_API const je_type_info_t** je_debug_world_systems_attached_get_all(je_game_world_t* _world);

JE_API je_debug_eid_t je_debug_entity_uid_get(const je_game_entity_t* e);

JE_API void je_debug_entity_uid_set(const je_game_entity_t* e, je_debug_eid_t uid);

JE_API void je_debug_entity_arch_information_get(
    je_game_entity_t* _entity,
    size_t* _out_chunk_size,
    size_t* _out_entity_size,
    size_t* _out_all_entity_count_in_chunk);

#endif

JE_FORCE_CAPI_END
