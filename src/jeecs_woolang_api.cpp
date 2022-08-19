#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

// ECS UNIVERSE
WO_API wo_api wojeapi_get_edit_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = jedbg_get_editor_universe();
    if (!universe)
        return wo_ret_halt(vm, "failed to get editor universe.");
    return wo_ret_pointer(vm, universe);
}

WO_API wo_api wojeapi_create_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = je_ecs_universe_create();
    return wo_ret_pointer(vm, universe);
}

WO_API wo_api wojeapi_set_current_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = wo_pointer(args + 0);

    jedbg_set_editor_universe(universe);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_stop_universe(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_universe(wo_pointer(args + 0)).stop();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_wait_universe(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_universe(wo_pointer(args + 0)).wait();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_close_universe(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_universe::destroy_universe(jeecs::game_universe(wo_pointer(args + 0)));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_create_world_in_universe(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_pointer(vm,
        jeecs::game_universe(wo_pointer(args + 0)).create_world().handle());
}

WO_API wo_api wojeapi_get_all_worlds_in_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = wo_pointer(args + 0);
    wo_value out_array = args + 1;

    auto result = jedbg_get_all_worlds_in_universe(universe);
    {
        auto worldlist = result;
        while (*worldlist)
            wo_set_pointer(wo_arr_add(out_array, nullptr), *(worldlist++));
    }
    je_mem_free(result);
    return wo_ret_val(vm, out_array);
}

// ECS WORLD
WO_API wo_api wojeapi_close_world(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world(wo_pointer(args + 0)).close();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_world_name(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, jedbg_get_world_name(wo_pointer(args + 0)));
}

WO_API wo_api wojeapi_set_world_name(wo_vm vm, wo_value args, size_t argc)
{
    jedbg_set_world_name(wo_pointer(args + 0), wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_add_system_to_world(wo_vm vm, wo_value args, size_t argc)
{
    /*
    extern("libjoyecs", "wojeapi_add_system_to_world")
    func add_system(self: world, systype : typeinfo) = > bool;
    */
    jeecs::game_world gworld = wo_pointer(args + 0);
    const jeecs::typing::type_info* system_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    if (gworld.add_system(system_type))
        return wo_ret_bool(vm, true);
    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_get_system_from_world(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    const jeecs::typing::type_info* system_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    return wo_ret_option_ptr(vm, gworld.get_system(system_type));
}

WO_API wo_api wojeapi_remove_system_from_world(wo_vm vm, wo_value args, size_t argc)
{
    /*
    extern("libjoyecs", "wojeapi_remove_system_from_world")
    func remove_system(self: world, sysinfo: typeinfo)=> void;
    */
    jeecs::game_world gworld = wo_pointer(args + 0);
    const jeecs::typing::type_info* system_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    gworld.remove_system(system_type);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_all_systems_from_world(wo_vm vm, wo_value args, size_t argc)
{
    /*
    extern("libjoyecs", "wojeapi_get_all_systems_from_world")
    private func _get_systems_from_world(self: world, out_result: array<typeinfo>)=> array<typeinfo>;
    */
    const jeecs::typing::type_info** types = jedbg_get_all_system_attached_in_world(wo_pointer(args + 0));
    wo_value result = args + 1;

    auto* cur_type = types;
    while (*cur_type)
       wo_set_pointer(wo_arr_add(result, nullptr), (void*)*(cur_type++));
    je_mem_free(types);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_add_entity_to_world_with_components(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    wo_value components_list = args + 1;

    std::vector<jeecs::typing::typeid_t> components;
    for (wo_integer_t i = 0; i < wo_lengthof(components_list); ++i)
        components.push_back(((const jeecs::typing::type_info*)wo_pointer(wo_arr_get(components_list, i)))->m_id);

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld._add_entity(components)),
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_get_all_entities_from_world(wo_vm vm, wo_value args, size_t argc)
{
    wo_value out_arr = args + 1;

    auto entities = jedbg_get_all_entities_in_world(wo_pointer(args + 0));
    auto entity_iter = entities;
    while (*entity_iter)
    {
        wo_set_gchandle(wo_arr_add(out_arr, nullptr), *(entity_iter++), nullptr,
            [](void* entity_ptr) {
                jedbg_free_entity((jeecs::game_entity*)entity_ptr);
            });
    }
    je_mem_free(entities);

    return wo_ret_val(vm, out_arr);
}

// ECS ENTITY
WO_API wo_api wojeapi_close_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    entity->close();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_editing_entity(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_option_ptr(vm, (void*)jedbg_get_editing_entity());
}

WO_API wo_api wojeapi_set_editing_entity(wo_vm vm, wo_value args, size_t argc)
{
    if (argc)
        jedbg_set_editing_entity((jeecs::game_entity*)wo_pointer(args + 0));
    else
        jedbg_set_editing_entity(nullptr);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_entity_uid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* anc = entity->get_component<jeecs::Transform::ChildAnchor>())
        return wo_ret_option_string(vm, anc->anchor_uid.to_string().c_str());

    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_get_parent_uid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* l2p = entity->get_component<jeecs::Transform::LocalToParent>())
        return wo_ret_option_string(vm, l2p->parent_uid.to_string().c_str());

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_set_parent(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::game_entity* parent = (jeecs::game_entity*)wo_pointer(args + 1);
    bool force = wo_bool(args + 2);

    auto* l2p = entity->get_component<jeecs::Transform::LocalToParent>();
    auto* ca = parent->get_component<jeecs::Transform::ChildAnchor>();
    if (force)
    {
        if (nullptr == l2p)
            l2p = entity->add_component<jeecs::Transform::LocalToParent>();
        if (nullptr == ca)
            ca = parent->add_component<jeecs::Transform::ChildAnchor>();
    }

    if (l2p && ca)
    {
        if (entity->get_component<jeecs::Transform::LocalToWorld>())
            entity->remove_component<jeecs::Transform::LocalToWorld>();

        l2p->parent_uid = ca->anchor_uid;
        return wo_ret_bool(vm, true);
    }

    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_set_parent_with_uid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    const char* parent_uid = wo_string(args + 1);

    bool force = wo_bool(args + 2);

    auto* l2p = entity->get_component<jeecs::Transform::LocalToParent>();
    if (force)
    {
        if (nullptr == l2p)
            l2p = entity->add_component<jeecs::Transform::LocalToParent>();
    }

    if (l2p)
    {
        if (entity->get_component<jeecs::Transform::LocalToWorld>())
            entity->remove_component<jeecs::Transform::LocalToWorld>();

        l2p->parent_uid.parse(parent_uid);
        return wo_ret_bool(vm, true);
    }

    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_get_entity_name(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    return wo_ret_string(vm, entity->name().c_str());
}

WO_API wo_api wojeapi_set_entity_name(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    return wo_ret_string(vm, entity->name(wo_string(args + 1)).c_str());
}

WO_API wo_api wojeapi_get_entity_chunk_info(wo_vm vm, wo_value args, size_t argc)
{
    char buf[64];
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    sprintf(buf, "[%p:%zuv%zu]", entity->_m_in_chunk, entity->_m_id, entity->_m_version);
    return wo_ret_string(vm, buf);
}

WO_API wo_api wojeapi_find_entity_with_chunk_info(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = new jeecs::game_entity();
    sscanf(wo_string(args + 0), "[%p:%zuv%zu]", &entity->_m_in_chunk, &entity->_m_id, &entity->_m_version);

    if (entity->valid())
        return wo_ret_option_gchandle(vm, entity,
            nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
    delete entity;
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_is_entity_valid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    return wo_ret_bool(vm, entity->valid());
}

WO_API wo_api wojeapi_get_all_components_types_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_arr = args + 1;

    auto types = jedbg_get_all_components_from_entity(entity);
    auto typeindex = types;
    while (*typeindex)
        wo_set_pointer(wo_arr_add(out_arr, nullptr), (void*)*(typeindex++));
    je_mem_free(types);

    return wo_ret_val(vm, out_arr);
}

WO_API wo_api wojeapi_get_component_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    return wo_ret_option_ptr(vm, je_ecs_world_entity_get_component(entity,
        (const jeecs::typing::type_info*)wo_pointer(args + 1)));
}

WO_API wo_api wojeapi_add_component_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    return wo_ret_pointer(vm, je_ecs_world_entity_add_component(
        je_ecs_world_of_entity(entity), entity,
        (const jeecs::typing::type_info*)wo_pointer(args + 1)));
}

WO_API wo_api wojeapi_remove_component_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    je_ecs_world_entity_remove_component(je_ecs_world_of_entity(entity), entity, (const jeecs::typing::type_info*)wo_pointer(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_is_top_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    return wo_ret_bool(vm, nullptr == entity->get_component<jeecs::Transform::LocalToParent>());
}

WO_API wo_api wojeapi_is_child_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::game_entity* parent = (jeecs::game_entity*)wo_pointer(args + 1);

    jeecs::Transform::LocalToParent* l2p = entity->get_component<jeecs::Transform::LocalToParent>();
    jeecs::Transform::ChildAnchor* archor = parent->get_component<jeecs::Transform::ChildAnchor>();

    if (l2p && archor)
    {
        return wo_ret_bool(vm, l2p->parent_uid == archor->anchor_uid);
    }
    return wo_ret_bool(vm, false);
}

// ECS COMPONENT

struct component_member_iter
{
    void* component_addr;
    const jeecs::typing::member_info* iter;

};
WO_API wo_api wojeapi_iter_components_member(wo_vm vm, wo_value args, size_t argc)
{
    void* component_addr = wo_pointer(args + 0);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    component_member_iter* iter = new component_member_iter;
    iter->component_addr = component_addr;
    iter->iter = component_type->m_member_types;

    return wo_ret_gchandle(vm, iter, nullptr, [](void* ptr) {delete (component_member_iter*)ptr; });
}
WO_API wo_api wojeapi_member_iterator_next(wo_vm vm, wo_value args, size_t argc)
{
    //func next(self: member_iterator, ref out_name: string, ref out_type: typeinfo, ref out_addr: native_value)=> bool;
    component_member_iter* iter = (component_member_iter*)wo_pointer(args + 0);
    wo_value out_name = args + 1;
    wo_value out_type = args + 2;
    wo_value out_addr = args + 3;

    if (nullptr == iter->iter)
        return wo_ret_bool(vm, false);

    wo_set_string(out_name, iter->iter->m_member_name);
    wo_set_pointer(out_type, (void*)iter->iter->m_member_type);
    wo_set_handle(out_addr, (wo_handle_t)(iter->iter->m_member_offset + (intptr_t)iter->component_addr));
    iter->iter = iter->iter->m_next_member;

    return wo_ret_bool(vm, true);
}


// ECS OTHER
WO_API wo_api wojeapi_exit(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_universe(jedbg_get_editor_universe()).stop();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_log(wo_vm vm, wo_value args, size_t argc)
{
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::log("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_loginfo(wo_vm vm, wo_value args, size_t argc)
{
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::log_info("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logwarn(wo_vm vm, wo_value args, size_t argc)
{
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::log_warn("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logerr(wo_vm vm, wo_value args, size_t argc)
{
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::log_error("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logfatal(wo_vm vm, wo_value args, size_t argc)
{
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::log_fatal("%s", disp.c_str());
    return wo_ret_void(vm);
}


// ECS TYPEINFO
WO_API wo_api wojeapi_type_of(wo_vm vm, wo_value args, size_t argc)
{
    if (wo_valuetype(args + 0) == WO_INTEGER_TYPE)
        return wo_ret_option_ptr(vm, (void*)jeecs::typing::type_info::of((jeecs::typing::typeid_t)wo_int(args + 0)));
    else //if (wo_valuetype(args + 0) == WO_STRING_TYPE)
        return wo_ret_option_ptr(vm, (void*)jeecs::typing::type_info::of(wo_string(args + 0)));
}

WO_API wo_api wojeapi_get_all_registed_types(wo_vm vm, wo_value args, size_t argc)
{
    wo_value out_array = args + 0;
    auto** types = jedbg_get_all_registed_types();

    auto** cur_type = types;
    while (*cur_type)
        wo_set_pointer(wo_arr_add(out_array, nullptr), (void*)*(cur_type++));

    je_mem_free(types);
    return wo_ret_val(vm, out_array);
}

WO_API wo_api wojeapi_type_is_component(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_bool(vm, type->m_type_class == je_typing_class::JE_COMPONENT);
}

WO_API wo_api wojeapi_type_is_system(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_bool(vm, type->is_system());
}

WO_API wo_api wojeapi_type_id(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_int(vm, type->m_id);
}

WO_API wo_api wojeapi_type_name(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_string(vm, type->m_typename);
}

WO_API wo_api wojeapi_type_basic_type(wo_vm vm, wo_value args, size_t argc)
{
    enum basic_type
    {
        INT, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT, TEXTURE
    };
    basic_type type = (basic_type)wo_int(args + 0);

    switch (type)
    {
    case INT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<int>(nullptr));
    case FLOAT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<float>(nullptr));
    case FLOAT2:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec2>(nullptr));
    case FLOAT3:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec3>(nullptr));
    case FLOAT4:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec4>(nullptr));
    case STRING:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::string>(nullptr));
    case QUAT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::quat>(nullptr));
    case TEXTURE:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::graphic::texture*>(nullptr));
    default:
        return wo_ret_panic(vm, "Unknown basic type.");
    }
}

// Native value
WO_API wo_api wojeapi_native_value_int(wo_vm vm, wo_value args, size_t argc)
{
    int* value = (int*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_int(args + 1, *value);
    else
        *value = wo_int(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_float(wo_vm vm, wo_value args, size_t argc)
{
    float* value = (float*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, *value);
    else
        *value = wo_float(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_float2(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec2* value = (jeecs::math::vec2*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, value->x);
    else
        value->x = wo_float(args + 1);

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, value->y);
    else
        value->y = wo_float(args + 2);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_float3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec3* value = (jeecs::math::vec3*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, value->x);
    else
        value->x = wo_float(args + 1);

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, value->y);
    else
        value->y = wo_float(args + 2);

    if (wo_is_ref(args + 3))
        wo_set_float(args + 3, value->z);
    else
        value->z = wo_float(args + 3);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_float4(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec4* value = (jeecs::math::vec4*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, value->x);
    else
        value->x = wo_float(args + 1);

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, value->y);
    else
        value->y = wo_float(args + 2);

    if (wo_is_ref(args + 3))
        wo_set_float(args + 3, value->z);
    else
        value->z = wo_float(args + 3);

    if (wo_is_ref(args + 4))
        wo_set_float(args + 4, value->w);
    else
        value->w = wo_float(args + 4);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_je_string(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::string* value = (jeecs::string*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_string(args + 1, value->c_str());
    else
        *value = wo_string(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_rot_euler3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::quat* value = (jeecs::math::quat*)wo_pointer(args + 0);
    auto&& euler_v3 = value->euler_angle();

    bool need_update = false;

    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, euler_v3.x);
    else
        euler_v3.x = wo_float(args + 1), need_update = true;

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, euler_v3.y);
    else
        euler_v3.y = wo_float(args + 2), need_update = true;

    if (wo_is_ref(args + 3))
        wo_set_float(args + 3, euler_v3.z);
    else
        euler_v3.z = wo_float(args + 3), need_update = true;

    if (need_update)
        value->set_euler_angle(euler_v3);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_je_to_string(wo_vm vm, wo_value args, size_t argc)
{
    void* native_val = wo_pointer(args + 0);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    return wo_ret_string(vm, jeecs::basic::make_cpp_string(component_type->m_to_string(native_val)).c_str());
}

WO_API wo_api wojeapi_native_value_je_parse(wo_vm vm, wo_value args, size_t argc)
{
    void* native_val = wo_pointer(args + 0);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);
    wo_string_t str = wo_string(args + 2);

    component_type->m_parse(native_val, str);

    return wo_ret_void(vm);
}

///////////////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_texture_open(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = new jeecs::graphic::texture(wo_string(args + 0));
    if (loaded_texture->enabled())
    {
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
            });
    }
    delete loaded_texture;
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_texture_create(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = new jeecs::graphic::texture(
        wo_int(args + 0), wo_int(args + 1), jegl_texture::texture_format::RGBA);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture), nullptr,
        [](void* ptr) {
            delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}

WO_API wo_api wojeapi_texture_get_pixel(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    auto* pix = new jeecs::graphic::texture::pixel((*loaded_texture)->resouce(), wo_int(args + 1), wo_int(args + 2));

    return wo_ret_gchandle(vm, pix, args + 0, [](void* ptr)
        {
            delete (jeecs::graphic::texture::pixel*)ptr;
        });
}

WO_API wo_api wojeapi_texture_pixel_color(wo_vm vm, wo_value args, size_t argc)
{
    auto* pix = (jeecs::graphic::texture::pixel*)wo_pointer(args + 0);
    auto color = pix->get();

    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, color.x);
    else
        color.x = wo_float(args + 1);

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, color.y);
    else
        color.y = wo_float(args + 2);

    if (wo_is_ref(args + 3))
        wo_set_float(args + 3, color.z);
    else
        color.z = wo_float(args + 3);

    if (wo_is_ref(args + 4))
        wo_set_float(args + 4, color.w);
    else
        color.w = wo_float(args + 4);

    pix->set(color);

    return wo_ret_void(vm);
}
/////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_shader_open(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_shader = new jeecs::graphic::shader(wo_string(args + 0));
    if (loaded_shader->enabled())
    {
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::shader>(loaded_shader), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
            });
    }
    delete loaded_shader;
    return wo_ret_option_none(vm);

}

WO_API wo_api wojeapi_shader_create(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_shader = jeecs::basic::create_new<jeecs::graphic::shader>(wo_string(args + 0), wo_string(args + 1));
    if (loaded_shader->enabled())
    {
        return wo_ret_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::shader>(loaded_shader), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
            });
    }
    delete loaded_shader;
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_textures_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_map = args + 1;

    if (entity->valid())
    {
        if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
        {
            wo_value key = wo_push_empty(vm);

            for (auto& texture : textures->textures)
            {
                if (!texture.m_texture)
                    return wo_ret_halt(vm, "Texture cannot be nullptr.");
                wo_set_int(key, (wo_integer_t)texture.m_pass_id);
                wo_set_gchandle(wo_map_set(out_map, key, nullptr),
                    new jeecs::basic::resource<jeecs::graphic::texture>(texture.m_texture), nullptr,
                    [](void* ptr) {
                        delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
                    });
            }
            wo_pop_stack(vm);
        }
    }
    return wo_ret_val(vm, out_map);
}

WO_API wo_api wojeapi_bind_texture_for_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    if (entity->valid())
        if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
            textures->bind_texture(wo_int(args + 1), *(jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 2));

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_shaders_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_array = args + 1;

    if (entity->valid())
    {
        if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
        {
            for (auto& shader : shaders->shaders)
            {
                if (!shader)
                    return wo_ret_halt(vm, "Shader cannot be nullptr.");
                wo_set_gchandle(wo_arr_add(out_array, nullptr),
                    new jeecs::basic::resource<jeecs::graphic::shader>(shader), nullptr,
                    [](void* ptr) {
                        delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
                    });
            }
        }
    }
    return wo_ret_val(vm, out_array);
}
WO_API wo_api wojeapi_set_shaders_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value shader_array = args + 1;

    if (entity->valid())
    {
        if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
        {
            shaders->shaders.clear();
            size_t arrsize = wo_lengthof(shader_array);
            for (size_t i = 0; i < arrsize; ++i)
            {
                jeecs::basic::resource<jeecs::graphic::shader>* shader =
                    (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(wo_arr_get(shader_array, i));
                shaders->shaders.push_back(*shader);
            }
        }
    }
    return wo_ret_void(vm);
}



WO_API wo_api wojeapi_shader_is_valid(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    return wo_ret_bool(vm, (*shader)->enabled());
}

WO_API wo_api wojeapi_get_uniforms_from_shader(wo_vm vm, wo_value args, size_t argc)
{
    /*
    extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
                func _get_uniforms_from_shader(
                    shad: shader,
                    out_datas: map<string, (typeinfo, uniform_value_data)>
                )=> map<string, (typeinfo, uniform_value_data)>;
    */
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    wo_value out_map = args + 1;

    if ((*shader)->enabled())
    {
        auto* uniforms = (*shader)->resouce()->m_raw_shader_data->m_custom_uniforms;
        wo_value key = wo_push_empty(vm);
        while (uniforms)
        {
            const jeecs::typing::type_info* type;
            switch (uniforms->m_uniform_type)
            {
            case jegl_shader::uniform_type::FLOAT:
                type = jeecs::typing::type_info::of<float>(nullptr); break;
            case jegl_shader::uniform_type::FLOAT2:
                type = jeecs::typing::type_info::of<jeecs::math::vec2>(nullptr); break;
            case jegl_shader::uniform_type::FLOAT3:
                type = jeecs::typing::type_info::of<jeecs::math::vec3>(nullptr); break;
            case jegl_shader::uniform_type::FLOAT4:
                type = jeecs::typing::type_info::of<jeecs::math::vec4>(nullptr); break;
            case jegl_shader::uniform_type::INT:
                type = jeecs::typing::type_info::of<int>(nullptr); break;
            case jegl_shader::uniform_type::TEXTURE2D:
                type = jeecs::typing::type_info::of<jeecs::graphic::texture*>(nullptr); break;
            default:
                // Unknown / Unsupport type, just give this things.
                type = jeecs::typing::type_info::of<jeecs::typing::type_info*>(nullptr); break;
                break;
            }

            wo_set_string(key, uniforms->m_name);

            wo_value val_in_map = wo_map_set(out_map, key, nullptr);
            wo_set_struct(val_in_map, 2);
            wo_set_pointer(wo_struct_get(val_in_map, 0), (void*)type);

            wo_value uniform_value_data = wo_struct_get(val_in_map, 1);
            wo_set_struct(uniform_value_data, 5);

            wo_set_int(wo_struct_get(uniform_value_data, 0), uniforms->n);
            wo_set_float(wo_struct_get(uniform_value_data, 1), uniforms->x);
            wo_set_float(wo_struct_get(uniform_value_data, 2), uniforms->y);
            wo_set_float(wo_struct_get(uniform_value_data, 3), uniforms->z);
            wo_set_float(wo_struct_get(uniform_value_data, 4), uniforms->w);

            uniforms = uniforms->m_next;
        }
        wo_pop_stack(vm);
    }
    return wo_ret_val(vm, out_map);

    if (auto str = (*shader)->resouce()->m_raw_shader_data->m_path)
        return wo_ret_string(vm, str);
    return wo_ret_string(vm, "< Built-in shader >");
}

WO_API wo_api wojeapi_shader_path(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);

    if (auto str = (*shader)->resouce()->m_raw_shader_data->m_path)
        return wo_ret_string(vm, str);
    return wo_ret_string(vm, "< Built-in shader >");
}

WO_API wo_api wojeapi_texture_is_valid(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    return wo_ret_bool(vm, (*shader)->enabled());
}

WO_API wo_api wojeapi_texture_path(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    if (auto str = (*shader)->resouce()->m_raw_texture_data->m_path)
        return wo_ret_string(vm, str);
    return wo_ret_string(vm, "< Built-in texture >");
}

const char* jeecs_woolang_api_path = "je.wo";
const char* jeecs_woolang_api_src = R"(
import woo.std;
namespace je
{
    extern("libjoyecs", "wojeapi_log")
    func log(...)=> void;
    extern("libjoyecs", "wojeapi_loginfo")
    func loginfo(...)=> void;
    extern("libjoyecs", "wojeapi_logerr")
    func logerr(...)=> void;
    extern("libjoyecs", "wojeapi_logwarn")
    func logwarn(...)=> void;
    extern("libjoyecs", "wojeapi_logfatal")
    func logfatal(...)=> void;

    extern("libjoyecs", "wojeapi_exit")
    func exit()=> void;

    using typeinfo = handle;
    namespace typeinfo
    {
        extern("libjoyecs", "wojeapi_type_is_component")
        func is_component(self: typeinfo)=> bool;

        extern("libjoyecs", "wojeapi_type_is_system")
        func is_system(self: typeinfo)=> bool;

        namespace editor
        {
            func get_all_registed_types()
            {
                extern("libjoyecs", "wojeapi_get_all_registed_types")
                func _get_all_registed_types(out_result: array<typeinfo>)=> array<typeinfo>;

                return _get_all_registed_types([]);
            }

            func get_all_components_types()
            {
                let result = []: array<typeinfo>;
                for (let type : get_all_registed_types())
                    if (type->is_component())
                        result->add(type);
                return result;
            }

            func get_all_systems_types()
            {
                let result = []: array<typeinfo>;
                for (let type : get_all_registed_types())
                    if (type->is_system())
                        result->add(type);
                return result;
            }
        }
        extern("libjoyecs", "wojeapi_type_of")
        func create(name: string)=> option<typeinfo>;

        extern("libjoyecs", "wojeapi_type_of")
        func create(id: int)=> option<typeinfo>;

        extern("libjoyecs", "wojeapi_type_id")
        func id(self: typeinfo)=> int;

        extern("libjoyecs", "wojeapi_type_name")
        func name(self: typeinfo)=> string;

        enum basic_type
        {
            INT, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT, TEXTURE
        }
        extern("libjoyecs", "wojeapi_type_basic_type")
        private func create(tid: basic_type)=> typeinfo;

        let const int = typeinfo(basic_type::INT);
        let const float = typeinfo(basic_type::FLOAT);
        let const float2 = typeinfo(basic_type::FLOAT2);
        let const float3 = typeinfo(basic_type::FLOAT3);
        let const float4 = typeinfo(basic_type::FLOAT4);
        let const quat = typeinfo(basic_type::QUAT);
        let const string = typeinfo(basic_type::STRING);

        let const texture = typeinfo(basic_type::TEXTURE);
    }

    namespace graphic
    {
        using texture = gchandle;
        namespace texture
        {
            extern("libjoyecs", "wojeapi_texture_open")
            func create(path: string)=> option<texture>;

            extern("libjoyecs", "wojeapi_texture_create")
            func create(width: int, height: int)=> texture;

            extern("libjoyecs", "wojeapi_texture_path")
            func path(self: texture)=> string;

            extern("libjoyecs", "wojeapi_texture_is_valid")
            func isvalid(self: shader)=> bool;

            extern("libjoyecs", "wojeapi_texture_get_pixel")
            func pix(self: texture, x: int, y: int)=> pixel;

            using pixel = gchandle;
            namespace pixel
            {
                extern("libjoyecs", "wojeapi_texture_pixel_color")
                func color(self: pixel, ref r: real, ref g: real, ref b: real, ref a: real)=> void;
            }
        }

        using shader = gchandle;
        namespace shader
        {
            extern("libjoyecs", "wojeapi_shader_open")
            func create(path: string)=> option<shader>;
            
            extern("libjoyecs", "wojeapi_shader_create")
            func create(vpath: string, src: string)=> option<shader>;

            extern("libjoyecs", "wojeapi_shader_is_valid")
            func isvalid(self: shader)=> bool;

            extern("libjoyecs", "wojeapi_shader_path")
            func path(self: shader)=> string;

            union uniform_variable
            {
                integer(int),
                float(real),
                float2((real, real)),
                float3((real, real, real)),
                float4((real, real, real, real)),
                texture(int),
                others,
            }

            using uniform_value_data = struct {
                n : int,
                x : real,
                y : real,
                z : real,
                w : real
            };

            func get_uniforms(self: shader)=> map<string, uniform_variable>
            {
                extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
                func _get_uniforms_from_shader(
                    shad: shader, 
                    out_datas: map<string, (typeinfo, uniform_value_data)>
                )=> map<string, (typeinfo, uniform_value_data)>;
                
                let result = {}: map<string, uniform_variable>;

                for(let name, (type, val) : _get_uniforms_from_shader(self, {}: map<string, (typeinfo, uniform_value_data)>))
                {
                    if (type == typeinfo::int)
                        result[name] = uniform_variable::integer(val.n);
                    else if (type == typeinfo::float)
                        result[name] = uniform_variable::float(val.x);
                    else if (type == typeinfo::float2)
                        result[name] = uniform_variable::float2((val.x, val.y));
                    else if (type == typeinfo::float3)
                        result[name] = uniform_variable::float3((val.x, val.y, val.z));
                    else if (type == typeinfo::float4)
                        result[name] = uniform_variable::float4((val.x, val.y, val.z, val.w));
                    else if (type == typeinfo::texture)
                        result[name] = uniform_variable::texture(val.n);
                    else
                        result[name] = uniform_variable::others;
                }

                return result;
            }
            func set_uniform<T>(self: shader, name: string, val: T)
                where nil:dynamic:T is int
                   || nil:dynamic:T is real
                   || nil:dynamic:T is (real, real)
                   || nil:dynamic:T is (real, real, real)
                   || nil:dynamic:T is (real, real, real, real);
            {
                
            }
        }
    }

    using universe = handle;
    namespace universe
    {
        extern("libjoyecs", "wojeapi_get_edit_universe")
        func current()=> universe;

        extern("libjoyecs", "wojeapi_create_world_in_universe")
        func create_world(self: universe)=> world;

        namespace editor
        {
            extern("libjoyecs", "wojeapi_create_universe")
            func create()=> universe;

            extern("libjoyecs", "wojeapi_set_current_universe")
            func set_current_universe(self: universe)=> void;

            extern("libjoyecs", "wojeapi_stop_universe")
            func stop(self: universe)=> void;

            extern("libjoyecs", "wojeapi_wait_universe")
            func wait(self: universe)=> void;

            extern("libjoyecs", "wojeapi_close_universe")
            func close(self: universe)=> void;
        
            func worlds_list(self: universe)
            {
                extern("libjoyecs", "wojeapi_get_all_worlds_in_universe")
                func _get_all_worlds(universe:universe, out_arrs:array<world>) => array<world>;

                return _get_all_worlds(self, []:array<world>);
            }
        }
    }

    using world = handle;
    namespace world
    {
        extern("libjoyecs", "wojeapi_close_world")
        func close(self: world) => void;

        extern("libjoyecs", "wojeapi_add_system_to_world")
        func add_system(self: world, systype: typeinfo)=> bool;

        func rend(self: world)
        {
            static let const graphic_typeinfo = typeinfo("Graphic::DefaultGraphicPipelineSystem")->val();

            // Remove GraphicPipelineSystem immediately.
            universe::current()->editor::worlds_list()
                ->forall(\w:world = w->editor::get_system(graphic_typeinfo)->has();)
                ->trans(\w:world = w->editor::remove_system(graphic_typeinfo););

            self->add_system(graphic_typeinfo);

            return self;
        }

        func rend()=> option<world>
        {
            static let const graphic_typeinfo = typeinfo("Graphic::DefaultGraphicPipelineSystem")->val();

            let rending_world = universe::current()->editor::worlds_list()
                                    ->forall(\w:world = w->editor::get_system(graphic_typeinfo)->has(););
            if (!rending_world->empty())
                return option::value(rending_world[0]);

            return option::none;
        }

        extern("libjoyecs", "wojeapi_add_entity_to_world_with_components")
        func add_entity(self: world, components: array<typeinfo>)=> entity;

        namespace editor
        {
            extern("libjoyecs", "wojeapi_get_system_from_world")
            func get_system(self: world, systype: typeinfo)=> option<handle>;

            extern("libjoyecs", "wojeapi_remove_system_from_world")
            func remove_system(self: world, sysinfo: typeinfo)=> void;

            extern("libjoyecs", "wojeapi_get_world_name")
            func name(self: world)=> string;

            extern("libjoyecs", "wojeapi_set_world_name")
            func name(self: world, _name: string)=> void;

            func get_all_entities(self: world)=> array<entity>
            {
                extern("libjoyecs", "wojeapi_get_all_entities_from_world")
                func _get_all_entities_from_world(world: world, out_result: array<entity>)=> array<entity>;

                let result = []: array<entity>;
                return _get_all_entities_from_world(self, result);
            }

            func get_systems_types(self: world)=> array<typeinfo>
            {
                extern("libjoyecs", "wojeapi_get_all_systems_from_world")
                    private func _get_systems_from_world(self: world, out_result: array<typeinfo>)=> array<typeinfo>;
                return _get_systems_from_world(self, []: array<typeinfo>);
            }

            func top_entity_iter(self: world)=> entity::editor::entity_iter
            {
                return entity::editor::entity_iter(self->get_all_entities());
            }
        }
    }
)"
R"(
    using entity = gchandle;
    namespace entity
    {
        func operator == (a: entity, b: entity)
        {
            return a->editor::chunkinfo() == b->editor::chunkinfo();
        }
        func operator != (a: entity, b: entity)
        {
            return a->editor::chunkinfo() != b->editor::chunkinfo();
        }

        func close(self: entity)
        {
            extern("libjoyecs", "wojeapi_close_entity")
            func _close(self: entity)=> void;

            _close(self);
        }

        namespace editor
        {
            extern("libjoyecs", "wojeapi_get_editing_entity")
            func editing()=> option<entity>;

            func set_editing(self: option<entity>)
            {
                extern("libjoyecs", "wojeapi_set_editing_entity")
                func _set_editing_entity(e: entity)=> void;
                extern("libjoyecs", "wojeapi_set_editing_entity")
                func _set_editing_entity()=> void;

                match(self)
                {
                value(e)? _set_editing_entity(e);
                none? _set_editing_entity();
                }
            }

            extern("libjoyecs", "wojeapi_set_parent")
            func set_parent(self: entity, parent: entity, force: bool)=> bool;

            extern("libjoyecs", "wojeapi_set_parent_with_uid")
            func set_parent_with_uid(self: entity, parent_uid: string, force: bool)=> bool;

            extern("libjoyecs", "wojeapi_get_entity_uid")
            func get_uid(self: entity)=> option<string>;

            extern("libjoyecs", "wojeapi_get_parent_uid")
            func get_parent_uid(self: entity)=> option<string>;

            extern("libjoyecs", "wojeapi_get_entity_name")
            func name(self: entity)=> string;

            extern("libjoyecs", "wojeapi_set_entity_name")
            func name(self: entity, name: string)=> string;

            extern("libjoyecs", "wojeapi_get_entity_chunk_info")
            func chunkinfo(self: entity)=> string;

            extern("libjoyecs", "wojeapi_find_entity_with_chunk_info")
            func find_entity_by_chunkinfo(chunkinfo: string)=> option<entity>;

            extern("libjoyecs", "wojeapi_is_entity_valid")
            func valid(self: entity)=> bool;

            func get_components_types(self: entity)=> array<typeinfo>
            {
                extern("libjoyecs", "wojeapi_get_all_components_types_from_entity")
                func _get_components_types_from_entity(
                    entity: entity, out_result: array<typeinfo>)=> array<typeinfo>;

                return _get_components_types_from_entity(self, []: array<typeinfo>);
            }

            extern("libjoyecs", "wojeapi_get_component_from_entity")
            func get_component(self: entity, type: typeinfo)=> option<component>;

            extern("libjoyecs", "wojeapi_add_component_from_entity")
            func add_component(self: entity, type: typeinfo)=> option<component>;

            extern("libjoyecs", "wojeapi_remove_component_from_entity")
            func remove_component(self: entity, type: typeinfo)=> void;

            extern("libjoyecs", "wojeapi_is_top_entity")
            func is_top(self: entity)=> bool;

            extern("libjoyecs", "wojeapi_is_child_of_entity")
            func is_child_of(self: entity, parent: entity)=> bool;

            using entity_iter = struct {
                m_cur_iter          : array::iterator<entity>,
                m_judge_func        : (entity)=>bool,
                m_current_entity    : option<entity>,

                m_all_entity_list   : array<entity>,
                m_not_top_entities  : array<entity>,
                m_outed_entities    : array<entity>,
            };
            namespace entity_iter
            {
                private func create(entitys: array<entity>)
                {
                    let not_top_entities = []: array<entity>;
                    return entity_iter{
                        m_cur_iter = entitys->iter(),
                        m_judge_func = func(e: entity)
                                       {
                                            let result = e->editor::is_top();
                                            if (!result)
                                                not_top_entities->add(e);
                                            return result
                                       },
                        m_current_entity = option::none:<entity>,
                        m_all_entity_list = entitys,
                        m_not_top_entities = not_top_entities,
                        m_outed_entities = []: array<entity>,
                    };
                }

                func childs(self: entity_iter)
                {
                    let parent_entity = self.m_current_entity->val();
                    return entity_iter{
                        m_cur_iter = self.m_all_entity_list->iter(),
                        m_judge_func = func(e: entity)
                                       {
                                            return e->is_child_of(parent_entity);
                                       },
                        m_current_entity = option::none:<entity>,
                        m_all_entity_list = self.m_all_entity_list,
                        m_not_top_entities = []: array<entity>,
                        m_outed_entities = self.m_outed_entities,
                    };
                }
    
                func iter(self: entity_iter)
                {
                    return self;
                }
                func next(self: entity_iter, ref out_iter: entity_iter,ref out_entity: entity)=> bool
                {
                    let current_iter = self.m_cur_iter;
                    while (current_iter->next(0, ref out_entity))
                    {
                        if (self.m_judge_func(out_entity))
                        {
                            self.m_current_entity = option::value(out_entity);
                            out_iter = self;
                            self.m_outed_entities->add(out_entity);
                            return true;
                        }
                    }
                continue_find_not_displayed_entity@
                    while (!self.m_not_top_entities->empty())
                    {
                        let top = self.m_not_top_entities[0];
                        self.m_not_top_entities->remove(0);

                        if (self.m_outed_entities->find(top) == -1)
                        {
                            for (let entity: self.m_all_entity_list)
                            {
                                if (top != entity && top->is_child_of(entity))
                                    // Parent finded, it's not a orphan entity.
                                    continue continue_find_not_displayed_entity;
                            }

                            // Current entity have jeecs::Transform::LocalToParent,
                            // but it's LocalToParent donot point to any other entity;
                            self.m_current_entity = option::value(top);
                            out_iter = self;
                            out_entity = top;
                            self.m_outed_entities->add(top);
                            return true;
                        }
                    }
                    return false;
                }
            } // end of namespace entity_iter

            namespace graphic
            {
                func get_shaders(self: entity)=> array<graphic::shader>
                {
                    extern("libjoyecs", "wojeapi_shaders_of_entity")
                    func get_shaders_from_entity(e: entity, out_array: array<graphic::shader>)=> array<graphic::shader>;

                    return get_shaders_from_entity(self, []: array<graphic::shader>);
                }

                extern("libjoyecs", "wojeapi_set_shaders_of_entity")
                func set_shaders(self: entity, shaders: array<graphic::shader>)=> void;

                func get_textures(self: entity)=> map<int, graphic::texture>
                {
                    extern("libjoyecs", "wojeapi_textures_of_entity")
                    func get_textures_from_entity(e: entity, textures: map<int, graphic::texture>)=> map<int, graphic::texture>;

                    return get_textures_from_entity(self, {}: map<int, graphic::texture>);
                }

                extern("libjoyecs", "wojeapi_bind_texture_for_entity")
                func bind_texture(self: entity, id: int, tex: graphic::texture)=> void;
            }
        }// end of namespace editor
    } // end of namespace entity

    using native_value = handle;
    namespace native_value
    {
        extern("libjoyecs", "wojeapi_native_value_int")
        func int(self: native_value, ref value: int)=> void;

        extern("libjoyecs", "wojeapi_native_value_float")
        func float(self: native_value, ref value: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_float2")
        func float2(self: native_value, ref x: real, ref y: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_float3")
        func float3(self: native_value, ref x: real, ref y: real, ref z: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_float4")
        func float4(self: native_value, ref x: real, ref y: real, ref z: real, ref w: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_rot_euler3")
        func euler3(self: native_value, ref x: real, ref y: real, ref z: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_je_string")
        func string(self: native_value, ref val: string)=> void;


        extern("libjoyecs", "wojeapi_native_value_je_to_string")
        func to_string(self: native_value, types: typeinfo)=> string; 

        extern("libjoyecs", "wojeapi_native_value_je_parse")
        func parse(self: native_value, types: typeinfo, str: string)=> void; 
    }

    using component = handle;
    namespace component
    {
        namespace editor
        {
            using member_iterator = gchandle;
            namespace member_iterator
            {
                func iter(self: member_iterator)
                {
                    return self;
                }

                extern("libjoyecs", "wojeapi_member_iterator_next")
                func next(self: member_iterator, ref out_name: string, ref out_type: typeinfo, ref out_addr: native_value)=> bool;
            }

            extern("libjoyecs", "wojeapi_iter_components_member")
            func iter_member(self: component, type: typeinfo) => member_iterator;
        }
    }
}

)";

