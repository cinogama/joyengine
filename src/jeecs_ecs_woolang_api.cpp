#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/clock.hpp"
#include "jeecs/components.hpp"
#include "jeecs/core.hpp"
#include "jeecs/debug.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/game.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"

#include "wo.h"

#include "jeecs_core_editor_components.hpp"

#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace jeecs;

// ECS UNIVERSE
WOORT_API woort_api wojeapi_create_universe(void)
{
    return woort_ret_pointer(je_ecs_universe_create());
}

WOORT_API woort_api wojeapi_close_universe(void)
{
    je_game_universe_t* const universe =
        static_cast<je_game_universe_t*>(woort_pointer(0));

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        je_ecs_universe_destroy(universe);
    }
    (void)woort_vm_swap(last);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_universe_from_world(void)
{
    je_game_universe_t* universe = je_ecs_world_in_universe(static_cast<je_game_world_t*>(woort_pointer(0)));
    return woort_ret_pointer(universe);
}

WOORT_API woort_api wojeapi_universe_extend_life(void)
{
    game_universe(static_cast<je_game_universe_t*>(woort_pointer(0))).grow();
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_universe_reduce_life(void)
{
    game_universe(static_cast<je_game_universe_t*>(woort_pointer(0))).trim();
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_universe_wait(void)
{
    game_universe u(static_cast<je_game_universe_t*>(woort_pointer(0)));
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        u.wait();
    }
    (void)woort_vm_swap(last);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_time(void)
{
    return woort_ret_real(je_clock_time());
}

WOORT_API woort_api wojeapi_universe_get_frame_deltatime(void)
{
    return woort_ret_real(je_ecs_universe_frame_deltatime_get(
        static_cast<je_game_universe_t*>(woort_pointer(0))));
}

WOORT_API woort_api wojeapi_universe_set_frame_deltatime(void)
{
    je_ecs_universe_frame_deltatime_set(
        static_cast<je_game_universe_t*>(woort_pointer(0)), woort_real(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_universe_get_max_deltatime(void)
{
    return woort_ret_real(je_ecs_universe_max_deltatime_get(
        static_cast<je_game_universe_t*>(woort_pointer(0))));
}

WOORT_API woort_api wojeapi_universe_set_max_deltatime(void)
{
    je_ecs_universe_max_deltatime_set(
        static_cast<je_game_universe_t*>(woort_pointer(0)), woort_real(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_universe_get_timescale(void)
{
    return woort_ret_real(je_ecs_universe_time_scale_get(
        static_cast<je_game_universe_t*>(woort_pointer(0))));
}

WOORT_API woort_api wojeapi_universe_set_timescale(void)
{
    je_ecs_universe_time_scale_set(
        static_cast<je_game_universe_t*>(woort_pointer(0)), woort_real(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_create_world_in_universe(void)
{
    return woort_ret_pointer(
        game_universe(static_cast<je_game_universe_t*>(woort_pointer(0))).create_world().handle());
}

WOORT_API woort_api wojeapi_get_all_worlds_in_universe(void)
{
    je_game_universe_t* universe = static_cast<je_game_universe_t*>(woort_pointer(0));

    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);

    auto result = je_debug_universe_worlds_get_all(universe);
    {
        auto worldlist = result;
        while (*worldlist)
        {
            woort_set_box_pointer(elem, *(worldlist++));
            woort_vec_push(out_array, elem);
        }
    }
    je_mem_free(result);
    return woort_ret_value(out_array);
}

// ECS WORLD
WOORT_API woort_api wojeapi_close_world(void)
{
    game_world(static_cast<je_game_world_t*>(woort_pointer(0))).close();
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_enable_world(void)
{
    game_world(static_cast<je_game_world_t*>(woort_pointer(0))).set_enable(woort_bool(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_add_system_to_world(void)
{
    /*
    extern("libjoyecs", "wojeapi_add_system_to_world")
    func add_system(self: world, systype : typeinfo) = > bool;
    */
    game_world gworld(static_cast<je_game_world_t*>(woort_pointer(0)));
    const je_type_info_t* system_type = static_cast<const je_type_info_t*>(woort_pointer(1));

    bool added = false;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        if (nullptr != je_ecs_world_system_instance_add(gworld.handle(), system_type->m_id))
            added = true;
    }
    (void)woort_vm_swap(last);

    return woort_ret_bool(added);
}

WOORT_API woort_api wojeapi_get_system_from_world(void)
{
    game_world gworld(static_cast<je_game_world_t*>(woort_pointer(0)));
    const je_type_info_t* system_type =
        static_cast<const je_type_info_t*>(woort_pointer(1));

    auto* const system_addr =
        je_ecs_world_system_instance_get(
            gworld.handle(), system_type->m_id);

    if (system_addr == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(system_addr);
}

WOORT_API woort_api wojeapi_remove_system_from_world(void)
{
    /*
    extern("libjoyecs", "wojeapi_remove_system_from_world")
    func remove_system(self: world, sysinfo: typeinfo)=> void;
    */
    game_world gworld(static_cast<je_game_world_t*>(woort_pointer(0)));
    const je_type_info_t* system_type =
        static_cast<const je_type_info_t*>(woort_pointer(1));

    je_ecs_world_system_instance_remove(
        gworld.handle(), system_type->m_id);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_world_get_all_systems(void)
{
    /*
    extern("libjoyecs", "wojeapi_world_get_all_systems")
    private func _get_systems_from_world(self: world, out_result: array<typeinfo>)=> array<typeinfo>;
    */
    const je_type_info_t** types =
        je_debug_world_systems_attached_get_all(
            static_cast<je_game_world_t*>(woort_pointer(0)));

    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(result);

    auto* cur_type = types;
    while (*cur_type)
    {
        woort_set_box_pointer(elem, (void*)*(cur_type++));
        woort_vec_push(result, elem);
    }
    je_mem_free(types);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_add_entity_to_world_with_components(void)
{
    game_world gworld = static_cast<je_game_world_t*>(woort_pointer(0));
    woort_value components_list = 1;

    vector<je_type_id_t> components;

    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value elem = s + 0;

    const size_t component_count = woort_vec_len(components_list);
    for (size_t i = 0; i < component_count; ++i)
    {
        (void)woort_vec_get(elem, components_list, i);
        components.push_back(
            static_cast<const je_type_info_t*>(woort_unbox_pointer(elem))->m_id);
    }

    auto new_entity = gworld._add_entity(components);
    if (new_entity.has_value())
    {
        return woort_ret_option_gchandle(
            new je_game_entity_t(new_entity.value()._m_raw),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (je_game_entity_t*)ptr;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_add_entity_to_world_with_prefab(void)
{
    game_world gworld(static_cast<je_game_world_t*>(woort_pointer(0)));
    je_game_entity_t* const prefab_entity = static_cast<je_game_entity_t*>(woort_gcpointer(1));

    je_game_entity_t* _e = new je_game_entity_t{};
    je_ecs_world_entity_create_with_prefab(gworld.handle(), _e, prefab_entity);

    return woort_ret_gchandle(
        _e,
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (je_game_entity_t*)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_add_prefab_to_world_with_components(void)
{
    game_world gworld(static_cast<je_game_world_t*>(woort_pointer(0)));
    woort_value components_list = 1;

    vector<je_type_id_t> components;

    woort_value s;

    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value elem = s + 0;
    const size_t component_count = woort_vec_len(components_list);
    for (size_t i = 0; i < component_count; ++i)
    {
        (void)woort_vec_get(elem, components_list, i);
        components.push_back(
            static_cast<const je_type_info_t*>(woort_unbox_pointer(elem))->m_id);
    }

    auto new_prefab = gworld._add_prefab(components);
    if (new_prefab.has_value())
    {
        return woort_ret_option_gchandle(
            new je_game_entity_t(new_prefab.value()._m_raw),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (je_game_entity_t*)ptr;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_world_get_all_entities(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_arr = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_arr);

    void* world_instance = woort_pointer(0);

    auto entities = je_debug_world_entities_get_all(
        static_cast<je_game_world_t*>(world_instance));

    auto entity_iter = entities;

    while (*entity_iter)
    {
        auto* current_e = *(entity_iter++);
        woort_set_gchandle(
            elem,
            current_e,
            WOORT_IGNORE,
            [](void* entity_ptr)
            {
                je_debug_entity_free((je_game_entity_t*)entity_ptr);
            },
            nullptr);
        woort_vec_push(out_arr, elem);
    }
    je_mem_free(entities);

    return woort_ret_value(out_arr);
}

// ===========================================================================
// Persistent entity selector based on je_requirement_collection_t.
//
// entity_selector holds a long-lived je_requirement_collection_t. It is iterable:
// iter() produces an entity_selector_iter. The version-change check and the
// cache refresh happen in native code when the iterator is created (iter),
// not on every step - so a single `for` loop observes a consistent arch set.
// ===========================================================================
struct wo_entity_selector_state
{
    je_game_world_t* world;
    je_requirement_collection_t collection;
    // arch-change version captured at the selector's last refresh.
    woort_Int arch_version;

    ~wo_entity_selector_state()
    {
        je_ecs_requirements_free(collection.m_collected_requirement);
    }
};

// Per-iteration cursor. Lives only for the duration of a single `for` pass;
// holds a non-owning back-pointer to the selector (kept alive by woolang GC
// for at least as long as the loop is running).
struct wo_entity_iter_state
{
    wo_entity_selector_state* selector;
    size_t arch_index;
    je_chunk_t* chunk;
    je_entity_id_in_chunk_t entity_index;
};

// woort_pointer(0) = world
// woort_vec(1)    = array<requirement>
//   requirement is a union:
//     contains(typeinfo)     -> JE_COMPONENT_REQUIRE_CONTAINS
//     except(typeinfo)       -> JE_COMPONENT_REQUIRE_EXCEPT
//     anyof(array<typeinfo>) -> JE_COMPONENT_REQUIRE_ANYOF_0 + group,
//                                each anyof(...) entry gets its own group
WOORT_API woort_api wojeapi_entity_selector_create(void)
{
    je_game_world_t* const world = static_cast<je_game_world_t*>(woort_pointer(0));

    const size_t requirement_count = woort_vec_len(1);
    vector<je_component_requirement_t> requirements;
    requirements.reserve(requirement_count);

    if (requirement_count > 0)
    {
        woort_value s;
        if (!woort_push_reserve(2, &s))
            return woort_ret_panic("Stack overflow.");

        const woort_value requirement_info = s + 0;
        const woort_value elem = s + 1;

        int anyof_group = 0;

        for (size_t i = 0; i < requirement_count; ++i)
        {
            (void)woort_vec_get(requirement_info, 1, i);

            const woort_Int variant = woort_union_get(elem, requirement_info);
            switch (variant)
            {
            case 0: // contains(typeinfo)
            {
                const auto* typeinfo =
                    static_cast<const je_type_info_t*>(woort_pointer(elem));
                requirements.push_back(je_component_requirement_t{
                    JE_COMPONENT_REQUIRE_CONTAINS, typeinfo->m_id });
                break;
            }
            case 1: // except(typeinfo)
            {
                const auto* typeinfo =
                    static_cast<const je_type_info_t*>(woort_pointer(elem));
                requirements.push_back(je_component_requirement_t{
                    JE_COMPONENT_REQUIRE_EXCEPT, typeinfo->m_id });
                break;
            }
            case 2: // anyof(array<typeinfo>)
            {
                const size_t anyof_count = woort_vec_len(elem);
                for (size_t j = 0; j < anyof_count; ++j)
                {
                    (void)woort_vec_get(requirement_info, elem, j);
                    const auto* typeinfo =
                        static_cast<const je_type_info_t*>(woort_pointer(requirement_info));
                    requirements.push_back(je_component_requirement_t{
                        JE_COMPONENT_REQUIRE_ANYOF_0 + anyof_group,
                        typeinfo->m_id });
                }
                ++anyof_group;
                break;
            }
            default:
                return woort_ret_panic("Unknown requirement union variant.");
            }
        }
    }

    auto* state = new wo_entity_selector_state{};
    state->world = world;
    state->collection.m_collected_requirement = je_ecs_requirements_collect(
        requirements.data(), 0, requirements.size());
    je_ecs_world_collection_update(world, &state->collection);
    state->arch_version =
        (woort_Int)je_ecs_world_arch_change_version_get(world);

    return woort_ret_gchandle(
        state,
        WOORT_IGNORE,
        [](void* p)
        { delete static_cast<wo_entity_selector_state*>(p); },
        nullptr);
}

// Produce an iterator over the selector. This is where the arch-type change
// check runs: if the world's arch-change version differs from the version
// captured at the selector's last refresh, the cached arch info is refreshed
// before the cursor starts.
WOORT_API woort_api wojeapi_entity_selector_iter(void)
{
    auto* selector = static_cast<wo_entity_selector_state*>(woort_gcpointer(0));

    const woort_Int current_version =
        (woort_Int)je_ecs_world_arch_change_version_get(selector->world);
    if (current_version != selector->arch_version)
    {
        je_ecs_world_collection_update(selector->world, &selector->collection);
        selector->arch_version = current_version;
    }

    auto* iter = new wo_entity_iter_state{};
    iter->selector = selector;

    return woort_ret_gchandle(
        iter,
        WOORT_IGNORE,
        [](void* p)
        { delete static_cast<wo_entity_iter_state*>(p); },
        nullptr);
}

// Advance the cursor to the next READY entity, or option::none when exhausted.
WOORT_API woort_api wojeapi_entity_selector_iter_next(void)
{
    auto* iter = static_cast<wo_entity_iter_state*>(woort_gcpointer(0));
    auto& col = iter->selector->collection;

    for (;;)
    {
        if (iter->chunk == nullptr)
        {
            if (iter->arch_index >= col.m_cached_arch_count)
                return woort_ret_option_none();

            iter->chunk = je_arch_chunk_get(
                col.m_cached_archs[iter->arch_index].m_arch);
            iter->entity_index = 0;

            if (iter->chunk == nullptr)
            {
                ++iter->arch_index;
                continue;
            }
        }

        const auto& archinfo = col.m_cached_archs[iter->arch_index];
        const auto* meta = je_arch_entity_meta_addr_in_chunk(iter->chunk);

        while (iter->entity_index < archinfo.m_entity_count)
        {
            if (meta[iter->entity_index].m_stat == JE_ENTITY_STAT_READY)
            {
                auto* entity = new je_game_entity_t{
                    iter->chunk,
                    iter->entity_index,
                    meta[iter->entity_index].m_version };
                ++iter->entity_index;

                return woort_ret_option_gchandle(
                    entity,
                    WOORT_IGNORE,
                    [](void* p)
                    { delete (je_game_entity_t*)p; },
                    nullptr);
            }
            ++iter->entity_index;
        }

        // current chunk exhausted -> next chunk (or next arch)
        iter->chunk = je_arch_chunk_next(iter->chunk);
        iter->entity_index = 0;
        if (iter->chunk == nullptr)
            ++iter->arch_index;
    }
}

// ECS ENTITY
WOORT_API woort_api wojeapi_close_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    if (entity->_m_in_chunk != nullptr)
    {
        je_game_world_t* _w = je_ecs_world_of_entity(entity);
        if (_w != nullptr)
            je_ecs_world_entity_destroy(_w, entity);
    }
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_world_from_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    je_game_world_t* world = je_ecs_world_of_entity(entity);
    return woort_ret_pointer(world);
}

WOORT_API woort_api wojeapi_get_entity_uid(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    je_debug_eid_t uid = je_debug_entity_uid_get(entity);

    if (uid != 0)
        return woort_ret_option_int((woort_Int)uid);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_set_entity_uid(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    je_debug_entity_uid_set(entity, static_cast<je_debug_eid_t>(woort_int(1)));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_entity_anchor_uuid(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    if (auto* anc = (Transform::Anchor*)je_ecs_world_entity_component_get(entity, typing::id<Transform::Anchor>()))
    {
        anc->uid.JEParseToScriptType(WOORT_RETURN_SLOT);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_get_parent_anchor_uid(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    if (auto* l2p = (Transform::LocalToParent*)je_ecs_world_entity_component_get(entity, typing::id<Transform::LocalToParent>()))
    {
        l2p->parent_uid.JEParseToScriptType(WOORT_RETURN_SLOT);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_set_parent(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    je_game_entity_t* parent = (je_game_entity_t*)woort_gcpointer(1);
    bool force = woort_bool(2);

    auto* l2p = (Transform::LocalToParent*)je_ecs_world_entity_component_get(entity, typing::id<Transform::LocalToParent>());
    auto* ca = (Transform::Anchor*)je_ecs_world_entity_component_get(parent, typing::id<Transform::Anchor>());
    if (force)
    {
        if (nullptr == l2p)
            l2p = (Transform::LocalToParent*)je_ecs_world_entity_component_add(entity, typing::id<Transform::LocalToParent>());
        if (nullptr == ca)
            ca = (Transform::Anchor*)je_ecs_world_entity_component_add(parent, typing::id<Transform::Anchor>());
    }

    if (l2p && ca)
    {
        if ((Transform::LocalToWorld*)je_ecs_world_entity_component_get(entity, typing::id<Transform::LocalToWorld>()))
            je_ecs_world_entity_component_remove(entity, typing::id<Transform::LocalToWorld>());

        l2p->parent_uid = ca->uid;
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}

WOORT_API woort_api wojeapi_set_parent_with_uid(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    bool force = woort_bool(2);

    auto* l2p = (Transform::LocalToParent*)je_ecs_world_entity_component_get(entity, typing::id<Transform::LocalToParent>());
    if (force)
    {
        if (nullptr == l2p)
            l2p = (Transform::LocalToParent*)je_ecs_world_entity_component_add(entity, typing::id<Transform::LocalToParent>());
    }

    if (l2p)
    {
        if ((Transform::LocalToWorld*)je_ecs_world_entity_component_get(entity, typing::id<Transform::LocalToWorld>()))
            je_ecs_world_entity_component_remove(entity, typing::id<Transform::LocalToWorld>());

        l2p->parent_uid.JEParseFromScriptType(1);
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}

WOORT_API woort_api wojeapi_get_entity_name(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    return woort_ret_string(je_ecs_entity_name_get(entity));
}

WOORT_API woort_api wojeapi_set_entity_name(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    je_ecs_entity_name_set(entity, woort_string(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_entity_chunk_info(void)
{
    char buf[64];
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    int result = snprintf(buf, sizeof(buf), "[%p:%uv%u]", entity->_m_in_chunk, entity->_m_id, entity->_m_version);
    assert(result > 0 && result < (int)sizeof(buf));
    (void)result;

    return woort_ret_string(buf);
}

WOORT_API woort_api wojeapi_find_entity_with_chunk_info(void)
{
    je_game_entity_t* entity = new je_game_entity_t();
    ((void)sscanf(woort_string(0), "[%p:%uv%u]", &entity->_m_in_chunk, &entity->_m_id, &entity->_m_version));

    return woort_ret_gchandle(
        entity,
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (je_game_entity_t*)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_get_all_components_types_from_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_arr = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_arr);

    auto types = je_debug_entity_components_get_all(entity);
    auto typeindex = types;

    while (*typeindex)
    {
        woort_set_box_pointer(elem, (void*)*(typeindex++));
        woort_vec_push(out_arr, elem);
    }
    je_mem_free(types);

    return woort_ret_value(out_arr);
}

WOORT_API woort_api wojeapi_get_component_from_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    auto* const component_addr =
        je_ecs_world_entity_component_get(entity,
            ((const je_type_info_t*)woort_pointer(1))->m_id);

    if (component_addr == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(component_addr);
}

WOORT_API woort_api wojeapi_add_component_to_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    return woort_ret_pointer(je_ecs_world_entity_component_add(entity,
        ((const je_type_info_t*)woort_pointer(1))->m_id));
}

WOORT_API woort_api wojeapi_remove_component_from_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    je_ecs_world_entity_component_remove(
        entity, ((const je_type_info_t*)woort_pointer(1))->m_id);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_is_top_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    return woort_ret_bool(nullptr == (Transform::LocalToParent*)je_ecs_world_entity_component_get(entity, typing::id<Transform::LocalToParent>()));
}

WOORT_API woort_api wojeapi_is_child_of_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    je_game_entity_t* parent = (je_game_entity_t*)woort_gcpointer(1);

    Transform::LocalToParent* l2p = (Transform::LocalToParent*)je_ecs_world_entity_component_get(entity, typing::id<Transform::LocalToParent>());
    Transform::Anchor* archor = (Transform::Anchor*)je_ecs_world_entity_component_get(parent, typing::id<Transform::Anchor>());

    if (l2p && archor)
    {
        return woort_ret_bool(l2p->parent_uid == archor->uid);
    }
    return woort_ret_bool(false);
}

// ECS COMPONENT

WOORT_API woort_api wojeapi_component_get_all_members(void)
{
    woort_value s;

    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value elem = s + 0;
    const woort_value result = s + 1;
    const woort_value elem2 = s + 2;

    woort_struct_get(elem, 0, 0);

    void* component_addr = woort_pointer(elem);

    woort_struct_get(elem, 0, 1);
    const je_type_info_t* component_type =
        (const je_type_info_t*)woort_pointer(elem);

    woort_set_vec(result);

    if (component_type->m_member_types != nullptr)
    {
        auto* member_type = component_type->m_member_types->m_members;
        while (member_type)
        {
            woort_set_struct(elem, 3);

            woort_set_string(elem2, member_type->m_member_name);
            woort_struct_set(elem, 0, elem2);
            woort_set_pointer(elem2, (void*)member_type->m_member_type);
            woort_struct_set(elem, 1, elem2);
            woort_set_pointer(elem2, (member_type->m_member_offset + (intptr_t)component_addr));
            woort_struct_set(elem, 2, elem2);

            woort_vec_push(result, elem);

            member_type = member_type->m_next_member;
        }
    }
    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_get_components_member(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value elem = s + 0;
    const woort_value result = s + 1;

    woort_struct_get(elem, 0, 0);

    void* component_addr = woort_pointer(elem);

    woort_struct_get(elem, 0, 1);
    const je_type_info_t* component_type =
        (const je_type_info_t*)woort_pointer(elem);

    const woort_U8CString member_name = woort_string(1);

    if (auto* je_member_info_t = typing::find_member_by_name(component_type, member_name))
    {
        woort_set_struct(result, 2);

        woort_set_pointer(elem, je_member_info_t->m_member_type);
        woort_struct_set(result, 0, elem);
        woort_set_pointer(elem, (je_member_info_t->m_member_offset + (intptr_t)component_addr));
        woort_struct_set(result, 1, elem);

        return woort_ret_option_value(result);
    }
    else
        return woort_ret_option_none();
}

// Entity architecture info
WOORT_API woort_api wojeapi_get_entity_arch_information(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    size_t chunk_size = 0, entity_size = 0, entity_count = 0;

    je_debug_entity_arch_information_get(entity, &chunk_size, &entity_size, &entity_count);

    woort_set_struct(WOORT_RETURN_SLOT, 3);

    woort_struct_set_int(WOORT_RETURN_SLOT, 0, (woort_Int)chunk_size);
    woort_struct_set_int(WOORT_RETURN_SLOT, 1, (woort_Int)entity_size);
    woort_struct_set_int(WOORT_RETURN_SLOT, 2, (woort_Int)entity_count);

    return woort_ret();
}


// Entity prefab path (editor)
WOORT_API woort_api wojeapi_entity_get_prefab_path(void)
{
    game_entity* const entity = static_cast<game_entity*>(woort_gcpointer(0));
    if (auto* prefab = entity->get_component<Editor::Prefab>())
        return woort_ret_option_string(prefab->path.c_str());
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_entity_set_prefab_path(void)
{
    game_entity* const entity = static_cast<game_entity*>(woort_gcpointer(0));
    if (auto* prefab = entity->get_component<Editor::Prefab>())
    {
        prefab->path = woort_string(1);
    }
    return woort_ret_void();
}
