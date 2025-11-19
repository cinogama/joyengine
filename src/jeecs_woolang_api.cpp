#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <list>
#include <optional>

static std::atomic_flag _jewo_log_buffer_mx = {};
static std::list<std::pair<int, std::string>> _jewo_log_buffer;

WO_API wo_api wojeapi_get_current_platform(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, JE4_CURRENT_PLATFORM);
}

WO_API wo_api wojeapi_generate_uid(wo_vm vm, wo_value args)
{
    wo_value result = wo_reserve_stack(vm, 1, &args);
    jeecs::typing::uid_t::generate().JEParseToScriptType(vm, result);

    assert(wo_valuetype(result) == WO_STRING_TYPE);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_build_version(wo_vm vm, wo_value args)
{
#define JE_VERSION_WRAP(A, B, C) #A "." #B "." #C
    return wo_ret_string(vm, JE_CORE_VERSION);
#undef JE_VERSION_WRAP
}

WO_API wo_api wojeapi_build_version_info(wo_vm vm, wo_value args)
{
    return wo_ret_string(vm, je_build_version());
}

WO_API wo_api wojeapi_build_commit(wo_vm vm, wo_value args)
{
    return wo_ret_string(vm, je_build_commit());
}

WO_API wo_api wojeapi_get_sleep_suppression(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, je_clock_get_sleep_suppression());
}

WO_API wo_api wojeapi_read_file_all(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);

    if (auto* file = jeecs_file_open(wo_string(args + 0)))
    {
        std::vector<char> readed_buf(file->m_file_length);
        auto readed_len = jeecs_file_read(readed_buf.data(), sizeof(char), file->m_file_length, file);
        readed_buf.resize(readed_len);

        jeecs_file_close(file);

        if (leaved)
            wo_enter_gcguard(vm);

        return wo_ret_option_raw_string(vm, readed_buf.data(), readed_buf.size());
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_file_cache_write_all(wo_vm vm, wo_value args)
{
    bool ok = false;
    auto leaved = wo_leave_gcguard(vm);
    if (auto* cache = jeecs_create_cache_file(wo_string(args + 0), 0, 1))
    {
        auto data_length = wo_str_byte_len(args + 1);
        auto written_len = jeecs_write_cache_file(wo_string(args + 1), sizeof(char), data_length, cache);

        jeecs_close_cache_file(cache);

        if (data_length == written_len)
            ok = true;
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_bool(vm, ok);
}
WO_API wo_api wojeapi_file_cache_read_all(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    if (auto* cache = jeecs_load_cache_file(wo_string(args + 0), 0, -1))
    {
        std::vector<char> readed_buf(cache->m_file_length);
        auto readed_len = jeecs_file_read(readed_buf.data(), sizeof(char), cache->m_file_length, cache);
        readed_buf.resize(readed_len);

        jeecs_file_close(cache);

        if (leaved)
            wo_enter_gcguard(vm);

        return wo_ret_option_raw_string(vm, readed_buf.data(), readed_buf.size());
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_mark_shared_glresource_outdated(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, jegl_mark_shared_resources_outdated(wo_string(args + 0)));
}

WO_API wo_api wojeapi_init_graphic_pipeline_for_editor(wo_vm vm, wo_value args)
{
    auto* uhost = jegl_uhost_get_or_create_for_universe(wo_pointer(args + 0), nullptr);
    jegl_uhost_set_skip_behavior(uhost, false);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_set_runtime_path(wo_vm vm, wo_value args)
{
    jeecs_file_set_runtime_path(wo_string(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_create_fimg_packer(wo_vm vm, wo_value args)
{
    return wo_ret_pointer(vm, jeecs_file_image_begin(
        wo_string(args + 0),
        (size_t)wo_int(args + 1)));
}

WO_API wo_api wojeapi_pack_file_to_fimg_packer(wo_vm vm, wo_value args)
{
    auto* ctx = (fimg_creating_context*)wo_pointer(args + 0);
    return wo_ret_bool(vm, jeecs_file_image_pack_file(ctx, wo_string(args + 1), wo_string(args + 2)));
}

WO_API wo_api wojeapi_pack_buffer_to_fimg_packer(wo_vm vm, wo_value args)
{
    auto* ctx = (fimg_creating_context*)wo_pointer(args + 0);
    return wo_ret_bool(vm, jeecs_file_image_pack_buffer(ctx, wo_pointer(args + 1), (size_t)wo_int(args + 2), wo_string(args + 3)));
}

WO_API wo_api wojeapi_finish_fimg_packer(wo_vm vm, wo_value args)
{
    auto* ctx = (fimg_creating_context*)wo_pointer(args + 0);
    jeecs_file_image_finish(ctx);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_set_sleep_suppression(wo_vm vm, wo_value args)
{
    je_clock_set_sleep_suppression(wo_real(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_woolang_version(wo_vm vm, wo_value args)
{
    std::string woolang_version_info = "Woolang ";
    return wo_ret_string(vm, (woolang_version_info + wo_version() + " " + wo_compile_date()).c_str());
}

WO_API wo_api wojeapi_crc64_file(wo_vm vm, wo_value args)
{
    wo_integer_t result = wo_crc64_file(wo_string(args + 0));
    if (result)
        return wo_ret_option_int(vm, result);
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_crc64_string(wo_vm vm, wo_value args)
{
    wo_integer_t result = wo_crc64_str(wo_string(args + 0));
    return wo_ret_int(vm, result);
}

WO_API wo_api wojeapi_register_log_callback(wo_vm vm, wo_value args)
{
    std::function<void(int, const char*)>* callbacks =
        new std::function<void(int, const char*)>([&](int level, const char* msg)
            {
                while (_jewo_log_buffer_mx.test_and_set());
                _jewo_log_buffer.push_back({ level, msg });
                _jewo_log_buffer_mx.clear(); });

    return wo_ret_handle(vm,
        je_log_register_callback([](int level, const char* msg, void* func)
            { (*(std::function<void(int, const char*)> *)func)(level, msg); }, callbacks));
}

WO_API wo_api wojeapi_unregister_log_callback(wo_vm vm, wo_value args)
{
    auto func = (std::function<void(int, const char*)> *)je_log_unregister_callback((size_t)wo_handle(args + 0));
    delete func;

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_all_logs(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);

    wo_value result = s + 0;
    wo_set_arr(result, 0);

    std::list<std::pair<int, std::string>> logs;

    while (_jewo_log_buffer_mx.test_and_set())
        ;
    logs.swap(_jewo_log_buffer);
    assert(_jewo_log_buffer.empty());
    _jewo_log_buffer_mx.clear();

    wo_value elem = s + 1;
    wo_value val = s + 2;
    for (auto& [i, s] : logs)
    {
        wo_set_struct(elem, 2);
        wo_set_int(val, i);
        wo_struct_set(elem, 0, val);
        wo_set_string(val, s.c_str());
        wo_struct_set(elem, 1, val);
        wo_arr_add(result, elem);
    }
    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_load_module(wo_vm vm, wo_value args)
{
    return wo_ret_option_ptr_may_null(vm, je_module_load(wo_string(args + 0), wo_string(args + 1)));
}

WO_API wo_api wojeapi_unload_module(wo_vm vm, wo_value args)
{
    je_module_unload(wo_pointer(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_apply_camera_framebuf_setting(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (jeecs::Camera::RendToFramebuffer* rbf = entity->get_component<jeecs::Camera::RendToFramebuffer>())
    {
        rbf->framebuffer = jeecs::graphic::framebuffer::create(
            (size_t)wo_int(args + 1),
            (size_t)wo_int(args + 2),
            {
                jegl_texture::format::RGBA,
            },
            true);
    }
    else
        jeecs::debug::logfatal("No RendToFramebuffer in specify entity when 'wojeapi_apply_camera_framebuf_setting'.");
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_framebuf_texture(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (jeecs::Camera::RendToFramebuffer* rbf = entity->get_component<jeecs::Camera::RendToFramebuffer>())
    {
        if (!rbf->framebuffer.has_value())
            return wo_ret_option_none(vm);

        auto tex = rbf->framebuffer.value()->get_attachment((size_t)wo_int(args + 1));
        if (tex.has_value())
            return wo_ret_option_gchandle(vm,
                new jeecs::basic::resource<jeecs::graphic::texture>(tex.value()), nullptr,
                [](void* ptr)
                {
                    delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
                });

        jeecs::debug::logerr("RendToFramebuffer(%p).framebuffer not contain attach(%zu) in entity when 'wojeapi_get_framebuf_texture'.",
            rbf, (size_t)wo_int(args + 1));
        return wo_ret_option_none(vm);
    }
    else
        jeecs::debug::logerr("No RendToFramebuffer in specify entity when 'wojeapi_get_framebuf_texture'.");
    return wo_ret_option_none(vm);
}

// ECS UNIVERSE
WO_API wo_api wojeapi_create_universe(wo_vm vm, wo_value args)
{
    void* universe = je_ecs_universe_create();
    return wo_ret_gchandle(vm, universe, nullptr, [](void* universe)
        { jeecs::game_universe::destroy_universe(jeecs::game_universe(universe)); });
}

WO_API wo_api wojeapi_get_universe_from_world(wo_vm vm, wo_value args)
{
    void* universe = je_ecs_world_in_universe(wo_pointer(args + 0));
    return wo_ret_pointer(vm, universe);
}

WO_API wo_api wojeapi_universe_extend_life(wo_vm vm, wo_value args)
{
    jeecs::game_universe(wo_pointer(args + 0)).grow();
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_universe_reduce_life(wo_vm vm, wo_value args)
{
    jeecs::game_universe(wo_pointer(args + 0)).trim();
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_universe_wait(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    {
        jeecs::game_universe(wo_pointer(args + 0)).wait();
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_universe_get_frame_deltatime(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, je_ecs_universe_get_frame_deltatime(wo_pointer(args + 0)));
}

WO_API wo_api wojeapi_universe_set_frame_deltatime(wo_vm vm, wo_value args)
{
    je_ecs_universe_set_frame_deltatime(wo_pointer(args + 0), wo_real(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_universe_get_max_deltatime(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, je_ecs_universe_get_max_deltatime(wo_pointer(args + 0)));
}
WO_API wo_api wojeapi_universe_set_max_deltatime(wo_vm vm, wo_value args)
{
    je_ecs_universe_set_max_deltatime(wo_pointer(args + 0), wo_real(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_universe_get_timescale(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, je_ecs_universe_get_time_scale(wo_pointer(args + 0)));
}
WO_API wo_api wojeapi_universe_set_timescale(wo_vm vm, wo_value args)
{
    je_ecs_universe_set_time_scale(wo_pointer(args + 0), wo_real(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_create_world_in_universe(wo_vm vm, wo_value args)
{
    return wo_ret_pointer(vm,
        jeecs::game_universe(wo_pointer(args + 0)).create_world().handle());
}

WO_API wo_api wojeapi_get_all_worlds_in_universe(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    void* universe = wo_pointer(args + 0);
    wo_value out_array = s + 0;
    wo_set_arr(out_array, 0);

    auto result = jedbg_get_all_worlds_in_universe(universe);
    {
        wo_value elem = s + 1;

        auto worldlist = result;
        while (*worldlist)
        {
            wo_set_pointer(elem, *(worldlist++));
            wo_arr_add(out_array, elem);
        }
    }
    je_mem_free(result);
    return wo_ret_val(vm, out_array);
}

// ECS WORLD
WO_API wo_api wojeapi_close_world(wo_vm vm, wo_value args)
{
    jeecs::game_world(wo_pointer(args + 0)).close();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_set_able_world(wo_vm vm, wo_value args)
{
    jeecs::game_world(wo_pointer(args + 0)).set_able(wo_bool(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_world_name(wo_vm vm, wo_value args)
{
    return wo_ret_string(vm, jedbg_get_world_name(wo_pointer(args + 0)));
}

WO_API wo_api wojeapi_set_world_name(wo_vm vm, wo_value args)
{
    jedbg_set_world_name(wo_pointer(args + 0), wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_add_system_to_world(wo_vm vm, wo_value args)
{
    /*
    extern("libjoyecs", "wojeapi_add_system_to_world")
    func add_system(self: world, systype : typeinfo) = > bool;
    */
    jeecs::game_world gworld = wo_pointer(args + 0);
    const jeecs::typing::type_info* system_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    bool added;

    auto leaved = wo_leave_gcguard(vm);
    {
        added = gworld.add_system(system_type->m_id);
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (added)
        return wo_ret_bool(vm, true);
    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_get_system_from_world(wo_vm vm, wo_value args)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    const jeecs::typing::type_info* system_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    return wo_ret_option_ptr_may_null(vm, gworld.get_system(system_type->m_id));
}

WO_API wo_api wojeapi_remove_system_from_world(wo_vm vm, wo_value args)
{
    /*
    extern("libjoyecs", "wojeapi_remove_system_from_world")
    func remove_system(self: world, sysinfo: typeinfo)=> void;
    */
    jeecs::game_world gworld = wo_pointer(args + 0);
    const jeecs::typing::type_info* system_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    gworld.remove_system(system_type->m_id);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_all_systems_from_world(wo_vm vm, wo_value args)
{
    /*
    extern("libjoyecs", "wojeapi_get_all_systems_from_world")
    private func _get_systems_from_world(self: world, out_result: array<typeinfo>)=> array<typeinfo>;
    */
    wo_value s = wo_reserve_stack(vm, 2, &args);

    const jeecs::typing::type_info** types = jedbg_get_all_system_attached_in_world(wo_pointer(args + 0));
    wo_value result = s + 0;
    wo_set_arr(result, 0);

    wo_value elem = s + 1;

    auto* cur_type = types;
    while (*cur_type)
    {
        wo_set_pointer(elem, (void*)*(cur_type++));
        wo_arr_add(result, elem);
    }
    je_mem_free(types);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_add_entity_to_world_with_components(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::game_world gworld = wo_pointer(args + 0);
    wo_value components_list = args + 1;

    std::vector<jeecs::typing::typeid_t> components;

    wo_value elem = s + 0;
    for (wo_size_t i = 0; i < wo_arr_len(components_list); ++i)
    {
        wo_arr_get(elem, components_list, i);
        components.push_back(((const jeecs::typing::type_info*)wo_pointer(elem))->m_id);
    }

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld._add_entity(components)),
        nullptr, [](void* ptr)
        { delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_add_entity_to_world_with_prefab(wo_vm vm, wo_value args)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    jeecs::game_entity* prefab_entity = (jeecs::game_entity*)wo_pointer(args + 1);

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld.add_entity(*prefab_entity)),
        nullptr, [](void* ptr)
        { delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_add_prefab_to_world_with_components(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::game_world gworld = wo_pointer(args + 0);
    wo_value components_list = args + 1;

    std::vector<jeecs::typing::typeid_t> components;

    wo_value elem = s + 0;
    for (wo_size_t i = 0; i < wo_arr_len(components_list); ++i)
    {
        wo_arr_get(elem, components_list, i);
        components.push_back(((const jeecs::typing::type_info*)wo_pointer(elem))->m_id);
    }

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld._add_prefab(components)),
        nullptr, [](void* ptr)
        { delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_get_all_entities_from_world(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_value out_arr = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(s, 0);

    void* world_instance = wo_pointer(args + 0);

    auto required_components_len = wo_arr_len(args + 1);
    std::vector<jeecs::typing::typeid_t> required_components(required_components_len);

    for (size_t i = 0; i < required_components_len; ++i)
    {
        wo_arr_get(elem, args + 1, i);

        const jeecs::typing::type_info* t = (const jeecs::typing::type_info*)wo_pointer(elem);
        required_components[i] = t->m_id;
    }

    auto entities = jedbg_get_all_entities_in_world(world_instance);
    auto entity_iter = entities;

    while (*entity_iter)
    {
        auto* current_e = *(entity_iter++);

        bool has_required = true;
        for (auto& required_component : required_components)
        {
            if (nullptr == je_ecs_world_entity_get_component(current_e, required_component))
            {
                jedbg_free_entity(current_e);
                has_required = false;
                break;
            }
        }

        if (has_required)
        {
            wo_set_gchandle(elem, vm, current_e, nullptr,
                [](void* entity_ptr)
                {
                    jedbg_free_entity((jeecs::game_entity*)entity_ptr);
                });
            wo_arr_add(out_arr, elem);
        }
    }
    je_mem_free(entities);

    return wo_ret_val(vm, out_arr);
}

// ECS ENTITY
WO_API wo_api wojeapi_close_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    entity->close();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_world_from_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    void* world = je_ecs_world_of_entity(entity);
    return wo_ret_pointer(vm, world);
}

WO_API wo_api wojeapi_set_editing_entity_uid(wo_vm vm, wo_value args)
{
    jedbg_set_editing_entity_uid((jeecs::typing::debug_eid_t)wo_handle(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_reset_editing_entity_uid(wo_vm vm, wo_value args)
{
    jedbg_set_editing_entity_uid(0);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_editing_entity_uid(wo_vm vm, wo_value args)
{
    jeecs::typing::debug_eid_t uid = jedbg_get_editing_entity_uid();

    if (uid != 0)
        return wo_ret_option_handle(vm, (wo_handle_t)uid);
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_get_entity_uid(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::typing::debug_eid_t uid = jedbg_get_entity_uid(entity);

    if (uid != 0)
        return wo_ret_option_handle(vm, (wo_handle_t)uid);
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_get_entity_anchor_uuid(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    if (auto* anc = entity->get_component<jeecs::Transform::Anchor>())
    {
        wo_value result = s + 0;
        anc->uid.JEParseToScriptType(vm, result);

        assert(wo_valuetype(result) == WO_STRING_TYPE);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_get_parent_anchor_uid(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* l2p = entity->get_component<jeecs::Transform::LocalToParent>())
    {
        wo_value result = s + 0;
        l2p->parent_uid.JEParseToScriptType(vm, result);

        assert(wo_valuetype(result) == WO_STRING_TYPE);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_set_parent(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::game_entity* parent = (jeecs::game_entity*)wo_pointer(args + 1);
    bool force = wo_bool(args + 2);

    auto* l2p = entity->get_component<jeecs::Transform::LocalToParent>();
    auto* ca = parent->get_component<jeecs::Transform::Anchor>();
    if (force)
    {
        if (nullptr == l2p)
            l2p = entity->add_component<jeecs::Transform::LocalToParent>();
        if (nullptr == ca)
            ca = parent->add_component<jeecs::Transform::Anchor>();
    }

    if (l2p && ca)
    {
        if (entity->get_component<jeecs::Transform::LocalToWorld>())
            entity->remove_component<jeecs::Transform::LocalToWorld>();

        l2p->parent_uid = ca->uid;
        return wo_ret_bool(vm, true);
    }

    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_set_parent_with_uid(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

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

        l2p->parent_uid.JEParseFromScriptType(vm, args + 1);
        return wo_ret_bool(vm, true);
    }

    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_get_entity_name(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    return wo_ret_string(vm, je_ecs_get_name_of_entity(entity));
}

WO_API wo_api wojeapi_set_entity_name(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    je_ecs_set_name_of_entity(entity, wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_entity_chunk_info(wo_vm vm, wo_value args)
{
    char buf[64];
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    int result = snprintf(buf, sizeof(buf), "[%p:%uv%u]", entity->_m_in_chunk, entity->_m_id, entity->_m_version);
    assert(result > 0 && result < (int)sizeof(buf));
    (void)result;

    return wo_ret_string(vm, buf);
}

WO_API wo_api wojeapi_find_entity_with_chunk_info(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = new jeecs::game_entity();
    ((void)sscanf(wo_string(args + 0), "[%p:%uv%u]", &entity->_m_in_chunk, &entity->_m_id, &entity->_m_version));

    return wo_ret_gchandle(vm, entity,
        nullptr, [](void* ptr)
        { delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_get_all_components_types_from_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_arr = s + 0;
    wo_set_arr(out_arr, 0);

    auto types = jedbg_get_all_components_from_entity(entity);
    auto typeindex = types;
    wo_value elem = s + 1;
    while (*typeindex)
    {
        wo_set_pointer(elem, (void*)*(typeindex++));
        wo_arr_add(out_arr, elem);
    }
    je_mem_free(types);

    return wo_ret_val(vm, out_arr);
}

WO_API wo_api wojeapi_get_component_from_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    return wo_ret_option_ptr_may_null(
        vm,
        je_ecs_world_entity_get_component(entity,
            ((const jeecs::typing::type_info*)wo_pointer(args + 1))->m_id));
}

WO_API wo_api wojeapi_add_component_from_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    return wo_ret_pointer(vm, je_ecs_world_entity_add_component(entity,
        ((const jeecs::typing::type_info*)wo_pointer(args + 1))->m_id));
}

WO_API wo_api wojeapi_remove_component_from_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    je_ecs_world_entity_remove_component(
        entity, ((const jeecs::typing::type_info*)wo_pointer(args + 1))->m_id);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_is_top_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    return wo_ret_bool(vm, nullptr == entity->get_component<jeecs::Transform::LocalToParent>());
}

WO_API wo_api wojeapi_is_child_of_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::game_entity* parent = (jeecs::game_entity*)wo_pointer(args + 1);

    jeecs::Transform::LocalToParent* l2p = entity->get_component<jeecs::Transform::LocalToParent>();
    jeecs::Transform::Anchor* archor = parent->get_component<jeecs::Transform::Anchor>();

    if (l2p && archor)
    {
        return wo_ret_bool(vm, l2p->parent_uid == archor->uid);
    }
    return wo_ret_bool(vm, false);
}

// ECS COMPONENT

WO_API wo_api wojeapi_component_get_all_members(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);

    wo_value elem = s + 0;
    wo_struct_get(elem, args + 0, 0);

    void* component_addr = wo_pointer(elem);

    wo_struct_get(elem, args + 0, 1);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(elem);

    wo_value result = s + 1;
    wo_value elem2 = s + 2;
    wo_set_arr(result, 0);

    if (component_type->m_member_types != nullptr)
    {
        auto* member_type = component_type->m_member_types->m_members;
        while (member_type)
        {
            wo_set_struct(elem, 3);

            wo_set_string(elem2, member_type->m_member_name);
            wo_struct_set(elem, 0, elem2);
            wo_set_pointer(elem2, (void*)member_type->m_member_type);
            wo_struct_set(elem, 1, elem2);
            wo_set_handle(elem2, (wo_handle_t)(member_type->m_member_offset + (intptr_t)component_addr));
            wo_struct_set(elem, 2, elem2);

            wo_arr_add(result, elem);

            member_type = member_type->m_next_member;
        }
    }
    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_get_components_member(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_value elem = s + 0;
    wo_struct_get(elem, args + 0, 0);

    void* component_addr = wo_pointer(elem);

    wo_struct_get(elem, args + 0, 1);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(elem);

    wo_string_t member_name = wo_string(args + 1);

    if (auto* member_info = component_type->find_member_by_name(member_name))
    {
        wo_value result = s + 1;
        wo_set_struct(result, 2);

        wo_set_pointer(elem, (wo_ptr_t)member_info->m_member_type);
        wo_struct_set(result, 0, elem);
        wo_set_handle(elem,
            (wo_handle_t)(member_info->m_member_offset + (intptr_t)component_addr));
        wo_struct_set(result, 1, elem);
        return wo_ret_option_val(vm, result);
    }
    else
        return wo_ret_option_none(vm);
}

// INPUTS
WO_API wo_api wojeapi_input_keydown(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, jeecs::input::keydown((jeecs::input::keycode)wo_int(args + 0)));
}

WO_API wo_api wojeapi_input_mousedown(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, jeecs::input::mousedown(
        (size_t)wo_int(args + 0), (jeecs::input::mousecode)wo_int(args + 1)));
}

WO_API wo_api wojeapi_wheel_count(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto wheel = jeecs::input::wheel((size_t)wo_int(args + 0));

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 2);

    wo_set_float(elem, wheel.x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, wheel.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_window_size(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto winsz = jeecs::input::windowsize();

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 2);

    wo_set_int(elem, (wo_int_t)winsz.x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)winsz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_window_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto winsz = jeecs::input::windowpos();

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 2);

    wo_set_int(elem, (wo_int_t)winsz.x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)winsz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_mouse_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto winsz = jeecs::input::mousepos((size_t)wo_int(args + 0));

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 2);

    wo_set_int(elem, (wo_int_t)winsz.x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)winsz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_mouse_view_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto winsz = jeecs::input::mouseviewpos((size_t)wo_int(args + 0));

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 2);

    wo_set_float(elem, winsz.x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, winsz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_gamepad_button(wo_vm vm, wo_value args)
{
    jeecs::input::gamepad* gamepad =
        (jeecs::input::gamepad*)wo_pointer(args + 0);
    jeecs::input::gamepadcode kcode =
        (jeecs::input::gamepadcode)wo_int(args + 1);

    return wo_ret_bool(vm, gamepad->button(kcode));
}
WO_API wo_api wojeapi_input_gamepad_axis(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    jeecs::input::gamepad* gamepad =
        (jeecs::input::gamepad*)wo_pointer(args + 0);
    jeecs::input::joystickcode kcode =
        (jeecs::input::joystickcode)wo_int(args + 1);

    auto axis = gamepad->stick(kcode);

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 2);
    wo_set_float(elem, axis.x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, axis.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_input_gamepad_actived(wo_vm vm, wo_value args)
{
    jeecs::input::gamepad* gamepad =
        (jeecs::input::gamepad*)wo_pointer(args + 0);

    jeecs::typing::timestamp_ms_t actived;
    if (gamepad->actived(&actived))
        return wo_ret_option_int(vm, actived);

    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_input_gamepad_get_all(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto gamepads = jeecs::input::gamepad::all();

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(result, 0);
    for (auto& gamepad : gamepads)
    {
        wo_set_gchandle(elem, vm, new jeecs::input::gamepad(gamepad), nullptr,
            [](void* p)
            {
                delete (jeecs::input::gamepad*)p;
            });
        wo_arr_add(result, elem);
    }

    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_input_gamepad_last(wo_vm vm, wo_value args)
{
    auto gamepad = jeecs::input::gamepad::last();
    if (gamepad.has_value())
    {
        return wo_ret_option_gchandle(vm, new jeecs::input::gamepad(gamepad.value()), nullptr,
            [](void* p)
            {
                delete (jeecs::input::gamepad*)p;
            });
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_input_update_window_size(wo_vm vm, wo_value args)
{
    je_io_set_window_size((int)wo_int(args + 0), (int)wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_input_update_window_title(wo_vm vm, wo_value args)
{
    je_io_set_window_title(wo_string(args + 0));
    return wo_ret_void(vm);
}

// ECS OTHER
WO_API wo_api wojeapi_log(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_argc(vm);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::log("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_loginfo(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_argc(vm);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::loginfo("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logwarn(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_argc(vm);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::logwarn("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logerr(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_argc(vm);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::logerr("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logfatal(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_argc(vm);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::logfatal("%s", disp.c_str());
    return wo_ret_void(vm);
}

// ECS TYPEINFO
WO_API wo_api wojeapi_type_of(wo_vm vm, wo_value args)
{
    if (wo_valuetype(args + 0) == WO_INTEGER_TYPE)
        return wo_ret_option_ptr_may_null(vm, (void*)jeecs::typing::type_info::of((jeecs::typing::typeid_t)wo_int(args + 0)));
    else // if (wo_valuetype(args + 0) == WO_STRING_TYPE)
        return wo_ret_option_ptr_may_null(vm, (void*)jeecs::typing::type_info::of(wo_string(args + 0)));
}

WO_API wo_api wojeapi_get_all_registed_types(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_value out_array = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(out_array, 0);

    auto** types = jedbg_get_all_registed_types();

    auto** cur_type = types;
    while (*cur_type)
    {
        wo_set_pointer(elem, (void*)*(cur_type++));
        wo_arr_add(out_array, elem);
    }

    je_mem_free(types);
    return wo_ret_val(vm, out_array);
}

WO_API wo_api wojeapi_type_is_component(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_bool(vm, type->m_type_class == je_typing_class::JE_COMPONENT);
}

WO_API wo_api wojeapi_type_is_system(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_bool(vm, type->is_system());
}

WO_API wo_api wojeapi_type_id(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_int(vm, type->m_id);
}

WO_API wo_api wojeapi_type_name(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_string(vm, type->m_typename);
}
WO_API wo_api wojeapi_script_type_name(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    auto* parser = type->get_script_parser();
    if (parser != nullptr)
        return wo_ret_option_string(vm, parser->m_woolang_typename);

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_type_members(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);

    wo_value result = s + 0;
    wo_value elem = s + 1;
    wo_value elem2 = s + 2;

    wo_set_arr(result, 0);

    if (type->m_member_types != nullptr)
    {
        auto* member_iter = type->m_member_types->m_members;
        while (member_iter != nullptr)
        {
            wo_set_struct(elem, 2);

            wo_set_string(elem2, member_iter->m_member_name);
            wo_struct_set(elem, 0, elem2);
            wo_set_pointer(elem2, (void*)member_iter->m_member_type);
            wo_struct_set(elem, 1, elem2);

            wo_arr_add(result, elem);

            member_iter = member_iter->m_next_member;
        }
    }

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_type_basic_type(wo_vm vm, wo_value args)
{
    enum basic_type
    {
        INT,
        INT2,
        BOOL,
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        STRING,
        QUAT,
    };
    basic_type type = (basic_type)wo_int(args + 0);

    switch (type)
    {
    case INT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<int>());
    case INT2:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::ivec2>());
    case BOOL:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<bool>());
    case FLOAT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<float>());
    case FLOAT2:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec2>());
    case FLOAT3:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec3>());
    case FLOAT4:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec4>());
    case STRING:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::basic::string>());
    case QUAT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::quat>());
    default:
        return wo_ret_panic(vm, "Unknown basic type.");
    }
}

///////////////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_graphic_shrink_cache(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    {
        jegl_context* gcontext = jegl_uhost_get_context(
            jegl_uhost_get_or_create_for_universe(
                wo_pointer(args + 0), nullptr));

        jegl_shrink_shared_resource_cache(gcontext, (size_t)wo_int(args + 1));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_texture_open(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jegl_context* gcontext = nullptr;

    wo_value universe_ptr = s + 0;
    auto has_value = wo_option_get(universe_ptr, args + 0);

    std::optional<jeecs::basic::resource<jeecs::graphic::texture>> loaded_texture;

    auto leaved = wo_leave_gcguard(vm);
    {
        if (has_value)
        {
            gcontext = jegl_uhost_get_context(
                jegl_uhost_get_or_create_for_universe(
                    wo_pointer(universe_ptr), nullptr));
        }
        loaded_texture = jeecs::graphic::texture::load(gcontext, wo_string(args + 1));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (loaded_texture.has_value())
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture.value()), nullptr,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
            });

    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_texture_create(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);

    auto loaded_texture = jeecs::graphic::texture::create(
        (size_t)wo_int(args + 0), (size_t)wo_int(args + 1), jegl_texture::format::RGBA);

    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture), nullptr,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
        });
}
WO_API wo_api wojeapi_texture_clip(wo_vm vm, wo_value args)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture> *)wo_pointer(args + 0);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(
            jeecs::graphic::texture::clip(
                *loaded_texture,
                (size_t)wo_int(args + 1),
                (size_t)wo_int(args + 2),
                (size_t)wo_int(args + 3),
                (size_t)wo_int(args + 4))),
        nullptr,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
        });
}

WO_API wo_api wojeapi_texture_bind_path(wo_vm vm, wo_value args)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture> *)wo_pointer(args + 0);
    auto& path = (*loaded_texture)->resource()->m_path;
    if (path != nullptr)
        je_mem_free((void*)path);
    path = jeecs::basic::make_new_string(wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_texture_get_pixel(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture> *)wo_pointer(args + 0);
    wo_value elem = s + 0;
    wo_struct_get(elem, args + 1, 0);
    size_t x = (size_t)wo_int(elem);
    wo_struct_get(elem, args + 1, 1);
    size_t y = (size_t)wo_int(elem);
    auto* pix = new jeecs::graphic::texture::pixel((*loaded_texture)->resource(), x, y);

    return wo_ret_gchandle(vm, pix, args + 0, [](void* ptr)
        { delete (jeecs::graphic::texture::pixel*)ptr; });
}

WO_API wo_api wojeapi_texture_take_snapshot(wo_vm vm, wo_value args)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture> *)wo_pointer(args + 0);

    auto tex_raw = loaded_texture->get()->resource()->m_raw_texture_data;
    if (tex_raw->m_pixels)
    {
        auto memsz = tex_raw->m_width * tex_raw->m_height *
            (tex_raw->m_format & jegl_texture::format::COLOR_DEPTH_MASK);
        if (memsz > 0)
        {
            auto* membuf = malloc(memsz);
            memcpy(membuf, tex_raw->m_pixels, memsz);
            return wo_ret_ok_gchandle(vm, membuf, nullptr, [](void* buf)
                { free(buf); });
        }
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_texture_restore_snapshot(wo_vm vm, wo_value args)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture> *)wo_pointer(args + 0);
    auto* texture_buf = wo_pointer(args + 1);

    auto tex_raw = loaded_texture->get()->resource()->m_raw_texture_data;
    if (tex_raw->m_pixels)
    {
        auto memsz = tex_raw->m_width * tex_raw->m_height *
            (tex_raw->m_format & jegl_texture::format::COLOR_DEPTH_MASK);
        memcpy(tex_raw->m_pixels, texture_buf, memsz);

        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_texture_pixel_color(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto* pix = (jeecs::graphic::texture::pixel*)wo_pointer(args + 0);
    auto color = pix->get();

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 4);

    wo_set_float(elem, color.x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, color.y);
    wo_struct_set(result, 1, elem);
    wo_set_float(elem, color.z);
    wo_struct_set(result, 2, elem);
    wo_set_float(elem, color.w);
    wo_struct_set(result, 3, elem);

    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_texture_set_pixel_color(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto* pix = (jeecs::graphic::texture::pixel*)wo_pointer(args + 0);
    auto color = jeecs::math::vec4();

    wo_value elem = s + 0;
    wo_struct_get(elem, args + 1, 0);
    color.x = wo_float(elem);
    wo_struct_get(elem, args + 1, 1);
    color.y = wo_float(elem);
    wo_struct_get(elem, args + 1, 2);
    color.z = wo_float(elem);
    wo_struct_get(elem, args + 1, 3);
    color.w = wo_float(elem);

    pix->set(color);

    return wo_ret_void(vm);
}
/////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_font_open(wo_vm vm, wo_value args)
{
    std::optional<jeecs::basic::resource<jeecs::graphic::font>> loaded_font;
    
    auto leaved = wo_leave_gcguard(vm);
    {
        loaded_font = jeecs::graphic::font::load(
            wo_string(args + 0),
            (size_t)wo_int(args + 1));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (loaded_font.has_value())
    {
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::font>(loaded_font.value()), nullptr,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::font> *)ptr;
            });
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_font_load_char(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto* loaded_font =
        reinterpret_cast<jeecs::basic::resource<jeecs::graphic::font> *>(
            wo_pointer(args + 0));
    assert(loaded_font != nullptr);

    const jeecs::graphic::character* ch;
    
    auto leaved = wo_leave_gcguard(vm);
    {
        ch = loaded_font->get()->get_character(wo_char(args + 1));
        assert(ch != nullptr);
    }
    if (leaved)
        wo_enter_gcguard(vm);

    /*
    public using character = struct{
            m_texture: texture,
            m_character: char,
            m_width: int,
            m_height: int,
            m_advise_width: int,
            m_advise_height: int,
            m_baseline_offset_x: int,
            m_baseline_offset_y: int,
        };
    */
    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 8);

    wo_set_gchandle(
        elem,
        vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(ch->m_texture),
        nullptr,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
        });
    wo_struct_set(result, 0, elem);

    wo_set_char(elem, ch->m_char);
    wo_struct_set(result, 1, elem);

    wo_set_int(elem, ch->m_width);
    wo_struct_set(result, 2, elem);
    wo_set_int(elem, ch->m_height);
    wo_struct_set(result, 3, elem);

    wo_set_int(elem, ch->m_advance_x);
    wo_struct_set(result, 4, elem);
    wo_set_int(elem, ch->m_advance_y);
    wo_struct_set(result, 5, elem);

    wo_set_int(elem, ch->m_baseline_offset_x);
    wo_struct_set(result, 6, elem);
    wo_set_int(elem, ch->m_baseline_offset_y);
    wo_struct_set(result, 7, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_font_string_texture(wo_vm vm, wo_value args)
{
    auto* loaded_font =
        reinterpret_cast<jeecs::basic::resource<jeecs::graphic::font> *>(
            wo_pointer(args + 0));
    jeecs::basic::resource<jeecs::graphic::texture>* text_texture;
    
    auto leaved = wo_leave_gcguard(vm);
    {
        text_texture =
            new jeecs::basic::resource<jeecs::graphic::texture>(
                loaded_font->get()->u32text_texture(
                    wo_str_to_u32str(wo_string(args + 1))));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_gchandle(vm, text_texture, nullptr, [](void* ptr)
        { delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr; });
}

/////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_shader_open(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jegl_context* gcontext = nullptr;

    wo_value universe_ptr = s + 0;
    auto has_value = wo_option_get(universe_ptr, args + 0);

    std::optional<jeecs::basic::resource<jeecs::graphic::shader>> loaded_shader;

    auto leaved = wo_leave_gcguard(vm);
    {
        if (has_value)
        {
            gcontext = jegl_uhost_get_context(
                jegl_uhost_get_or_create_for_universe(
                    wo_pointer(universe_ptr), nullptr));
        }
        loaded_shader = jeecs::graphic::shader::load(gcontext, wo_string(args + 1));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (loaded_shader.has_value())
    {
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::shader>(loaded_shader.value()), nullptr,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
            });
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_shader_create(wo_vm vm, wo_value args)
{
    std::optional<jeecs::basic::resource<jeecs::graphic::shader>> loaded_shader;
    
    auto leaved = wo_leave_gcguard(vm);
    {
        loaded_shader = jeecs::graphic::shader::create(wo_string(args + 0), wo_string(args + 1));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (loaded_shader.has_value())
    {
        return wo_ret_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::shader>(loaded_shader.value()), nullptr,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
            });
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_textures_of_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_map = s + 0;
    wo_set_map(out_map, 0);

    if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
    {
        wo_value key = s + 1;
        wo_value val = s + 2;

        for (auto& texture : textures->textures)
        {
            wo_set_int(key, (wo_integer_t)texture.m_pass_id);
            wo_set_gchandle(val, vm,
                new jeecs::basic::resource<jeecs::graphic::texture>(texture.m_texture), nullptr,
                [](void* ptr)
                {
                    delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
                });
            wo_map_set(out_map, key, val);
        }
    }

    return wo_ret_val(vm, out_map);
}

WO_API wo_api wojeapi_bind_texture_for_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
    {
        wo_value elem = s + 0;
        if (wo_option_get(elem, args + 2))
        {
            textures->bind_texture(
                (size_t)wo_int(args + 1),
                *(jeecs::basic::resource<jeecs::graphic::texture> *)wo_pointer(elem));
        }
        else
        {
            textures->remove_texture((size_t)wo_int(args + 1));
        }
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Textures组件，在此panic?

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_set_shape_for_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    if (jeecs::Renderer::Shape* shape = entity->get_component<jeecs::Renderer::Shape>())
    {
        wo_value elem = s + 0;
        if (wo_option_get(elem, args + 1))
            shape->vertex.emplace(*(jeecs::basic::resource<jeecs::graphic::vertex> *)wo_pointer(elem));
        else
            shape->vertex.reset();
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Textures组件，在此panic?

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_shape_of_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    if (jeecs::Renderer::Shape* shape = entity->get_component<jeecs::Renderer::Shape>())
    {
        if (shape->vertex.has_value())
            return wo_ret_option_pointer(vm,
                new jeecs::basic::resource<jeecs::graphic::vertex>(shape->vertex.value()));
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_vertex_load(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jegl_context* gcontext = nullptr;
    std::optional<jeecs::basic::resource<jeecs::graphic::vertex>> loaded_vertex;

    wo_value universe_ptr = s + 0;
    auto has_value = wo_option_get(universe_ptr, args + 0);

    auto leaved = wo_leave_gcguard(vm);
    {
        if (has_value)
        {
            gcontext = jegl_uhost_get_context(
                jegl_uhost_get_or_create_for_universe(
                    wo_pointer(universe_ptr), nullptr));
        }

        loaded_vertex = jeecs::graphic::vertex::load(gcontext, wo_string(args + 1));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (loaded_vertex.has_value())
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::vertex>(loaded_vertex.value()), nullptr,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::vertex> *)ptr;
            });

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_vertex_create(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    // vertices: array<real>, indices: array<int>
    jegl_vertex::type vertex_type = (jegl_vertex::type)wo_int(args + 0);
    std::vector<float> vertices(wo_arr_len(args + 1));
    std::vector<uint32_t> indices(wo_arr_len(args + 2));
    std::vector<jegl_vertex::data_layout> formats(wo_arr_len(args + 3));

    wo_value elem = s + 0;
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        wo_arr_get(elem, args + 1, i);
        vertices[i] = wo_float(elem);
    }
    for (size_t i = 0; i < indices.size(); ++i)
    {
        wo_arr_get(elem, args + 2, i);
        indices[i] = (uint32_t)wo_int(elem);
    }
    for (size_t i = 0; i < formats.size(); ++i)
    {
        wo_arr_get(elem, args + 3, i);
        formats[i] = { jegl_vertex::data_type::FLOAT32, (size_t)wo_int(elem) };
    }

    std::optional<jeecs::basic::resource<jeecs::graphic::vertex>> loaded_vertex;

    auto leaved = wo_leave_gcguard(vm);
    {
        loaded_vertex = jeecs::graphic::vertex::create(
            vertex_type,
            vertices.data(),
            vertices.size() * sizeof(float),
            indices,
            formats);
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (loaded_vertex.has_value())
        return wo_ret_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::vertex>(loaded_vertex.value()), nullptr,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::vertex> *)ptr;
            });

    return wo_ret_panic(vm, "Bad vertex format.");
}

WO_API wo_api wojeapi_vertex_path(wo_vm vm, wo_value args)
{
    auto* loaded_vertex = (jeecs::basic::resource<jeecs::graphic::vertex> *)wo_pointer(args + 0);

    if (auto path = (*loaded_vertex)->resource()->m_path)
        return wo_ret_option_string(vm, path);
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_shaders_of_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_array = s + 0;
    wo_set_arr(out_array, 0);

    if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
    {
        wo_value elem = s + 1;

        for (auto& shader : shaders->shaders)
        {
            wo_set_gchandle(elem, vm,
                new jeecs::basic::resource<jeecs::graphic::shader>(shader), nullptr,
                [](void* ptr)
                {
                    delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
                });
            wo_arr_add(out_array, elem);
        }
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Shaders组件，在此panic?
    return wo_ret_val(vm, out_array);
}

WO_API wo_api wojeapi_reload_shader_of_entity(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_reload_texture_of_entity(wo_vm vm, wo_value args);

WO_API wo_api wojeapi_set_shaders_of_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value shader_array = args + 1;

    if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
    {
        wo_value elem = s + 0;

        shaders->shaders.clear();
        size_t arrsize = (size_t)wo_arr_len(shader_array);
        for (size_t i = 0; i < arrsize; ++i)
        {
            wo_arr_get(elem, shader_array, i);
            jeecs::basic::resource<jeecs::graphic::shader>* shader =
                (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(elem);
            shaders->shaders.push_back(*shader);
        }
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Shaders组件，在此panic?
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_uniforms_from_shader(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 4, &args);

    /*
    extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
                func _get_uniforms_from_shader(
                    shad: shader
                )=> map<string, uniform_variable>;
    */
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    wo_value out_map = s + 0;
    wo_set_map(out_map, 0);

    auto* uniforms = (*shader)->resource()->m_raw_shader_data->m_custom_uniforms;
    wo_value key = s + 1;
    wo_value val = s + 2;
    wo_value elem = s + 3;
    while (uniforms)
    {
        wo_set_string(key, uniforms->m_name);
        wo_set_struct(val, 2);
        wo_map_set(out_map, key, val);

        if (uniforms->m_uniform_type >= jegl_shader::uniform_type::INT
            && uniforms->m_uniform_type <= jegl_shader::uniform_type::FLOAT4)
        {
            wo_set_int(elem, uniforms->m_uniform_type);
        }
        else if (uniforms->m_uniform_type == jegl_shader::uniform_type::TEXTURE)
        {
            wo_set_int(elem, jegl_shader::uniform_type::TEXTURE);
        }
        else
            // Others
            wo_set_int(elem, 1 + jegl_shader::uniform_type::TEXTURE);

        wo_struct_set(val, 0, elem);

        switch (uniforms->m_uniform_type)
        {
        case jegl_shader::uniform_type::INT:
        case jegl_shader::uniform_type::TEXTURE:
            wo_set_int(elem, uniforms->m_value.m_int);
            break;
        case jegl_shader::uniform_type::INT2:
            wo_set_struct(elem, 2);
            wo_set_int(key, uniforms->m_value.m_int2[0]);
            wo_struct_set(elem, 0, key);
            wo_set_int(key, uniforms->m_value.m_int2[1]);
            wo_struct_set(elem, 1, key);
            break;
        case jegl_shader::uniform_type::INT3:
            wo_set_struct(elem, 3);
            wo_set_int(key, uniforms->m_value.m_int3[0]);
            wo_struct_set(elem, 0, key);
            wo_set_int(key, uniforms->m_value.m_int3[1]);
            wo_struct_set(elem, 1, key);
            wo_set_int(key, uniforms->m_value.m_int3[2]);
            wo_struct_set(elem, 2, key);
            break;
        case jegl_shader::uniform_type::INT4:
            wo_set_struct(elem, 4);
            wo_set_int(key, uniforms->m_value.m_int4[0]);
            wo_struct_set(elem, 0, key);
            wo_set_int(key, uniforms->m_value.m_int4[1]);
            wo_struct_set(elem, 1, key);
            wo_set_int(key, uniforms->m_value.m_int4[2]);
            wo_struct_set(elem, 2, key);
            wo_set_int(key, uniforms->m_value.m_int4[3]);
            wo_struct_set(elem, 3, key);
            break;
        case jegl_shader::uniform_type::FLOAT:
            wo_set_float(elem, uniforms->m_value.m_float);
            break;
        case jegl_shader::uniform_type::FLOAT2:
            wo_set_struct(elem, 2);
            wo_set_float(key, uniforms->m_value.m_float2[0]);
            wo_struct_set(elem, 0, key);
            wo_set_float(key, uniforms->m_value.m_float2[1]);
            wo_struct_set(elem, 1, key);
            break;
        case jegl_shader::uniform_type::FLOAT3:
            wo_set_struct(elem, 3);
            wo_set_float(key, uniforms->m_value.m_float3[0]);
            wo_struct_set(elem, 0, key);
            wo_set_float(key, uniforms->m_value.m_float3[1]);
            wo_struct_set(elem, 1, key);
            wo_set_float(key, uniforms->m_value.m_float3[2]);
            wo_struct_set(elem, 2, key);
            break;
        case jegl_shader::uniform_type::FLOAT4:
            wo_set_struct(elem, 4);
            wo_set_float(key, uniforms->m_value.m_float4[0]);
            wo_struct_set(elem, 0, key);
            wo_set_float(key, uniforms->m_value.m_float4[1]);
            wo_struct_set(elem, 1, key);
            wo_set_float(key, uniforms->m_value.m_float4[2]);
            wo_struct_set(elem, 2, key);
            wo_set_float(key, uniforms->m_value.m_float4[3]);
            wo_struct_set(elem, 3, key);
            break;
        default:
            // Unsupport type
            wo_set_nil(elem);
        }

        wo_struct_set(val, 1, elem);

        uniforms = uniforms->m_next;
    }

    return wo_ret_val(vm, out_map);
}

WO_API wo_api wojeapi_set_uniforms_int(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_int2(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_int3(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3), (int)wo_int(args + 4));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_int4(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3), (int)wo_int(args + 4), (int)wo_int(args + 5));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), wo_float(args + 2));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float2(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1),
        jeecs::math::vec2(wo_float(args + 2), wo_float(args + 3)));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float3(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1),
        jeecs::math::vec3(wo_float(args + 2), wo_float(args + 3), wo_float(args + 4)));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float4(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1),
        jeecs::math::vec4(wo_float(args + 2), wo_float(args + 3), wo_float(args + 4), wo_float(args + 5)));

    return wo_ret_void(vm);
}

// defined in 'jeecs_core_editor_system.hpp'
WO_API wo_api wojeapi_get_bad_shader_list_of_entity(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_name(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_remove_bad_shader_name(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_uniforms_int(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_uniforms_int2(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_uniforms_int3(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_uniforms_int4(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_uniforms_float(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_uniforms_float2(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_uniforms_float3(wo_vm vm, wo_value args);
WO_API wo_api wojeapi_store_bad_shader_uniforms_float4(wo_vm vm, wo_value args);

WO_API wo_api wojeapi_shader_path(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)wo_pointer(args + 0);

    if (auto str = (*shader)->resource()->m_path)
        return wo_ret_string(vm, str);
    return wo_ret_string(vm, "< Built-in shader >");
}

WO_API wo_api wojeapi_texture_get_size(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)wo_pointer(args + 0);
    auto sz = texture->get()->size();

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 2);

    wo_set_int(elem, (wo_int_t)sz.x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)sz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_texture_path(wo_vm vm, wo_value args)
{
    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)wo_pointer(args + 0);

    if (auto str = (*texture)->resource()->m_path)
        return wo_ret_option_string(vm, str);
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_get_entity_arch_information(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    size_t chunk_size = 0, entity_size = 0, entity_count = 0;

    jedbg_get_entity_arch_information(entity, &chunk_size, &entity_size, &entity_count);
    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, 3);

    wo_set_int(elem, (wo_int_t)chunk_size);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)entity_size);
    wo_struct_set(result, 1, elem);
    wo_set_int(elem, (wo_int_t)entity_count);
    wo_struct_set(result, 2, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_towoo_register_system(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* result;

    auto leaved = wo_leave_gcguard(vm);
    {
        result = je_towoo_register_system(wo_string(args + 0), wo_string(args + 1));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (result != nullptr)
        return wo_ret_option_pointer(vm, (void*)result);

    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_towoo_unregister_system(wo_vm vm, wo_value args)
{
    je_towoo_unregister_system((const jeecs::typing::type_info*)wo_pointer(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_update_component(wo_vm vm, wo_value args)
{
    wo_string_t component_name = wo_string(args + 0);
    wo_string_t component_path = wo_string(args + 1);

    auto leaved = wo_leave_gcguard(vm);

    if (jeecs_file* texfile = jeecs_file_open(component_path))
    {
        char* src = (char*)malloc(texfile->m_file_length + 1);
        jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
        src[texfile->m_file_length] = 0;

        wo_vm cvm = wo_create_vm();
        bool result = wo_load_binary(cvm, component_path, src, texfile->m_file_length);

        jeecs_file_close(texfile);

        free(src);
        if (result)
        {
            // Invoke "_init_towoo_component", if failed... boom!
            wo_unref_value initfunc;
            if (wo_extern_symb(&initfunc, cvm, "_init_towoo_component") == WO_FALSE)
            {
                jeecs::debug::logerr("Failed to register: '%s' cannot find '_init_towoo_component' in '%s', "
                    "forget to import je/towoo/component.wo ?",
                    component_name, component_path);
                wo_close_vm(cvm);
            }
            else
            {
                if (nullptr == wo_run(cvm))
                {
                    jeecs::debug::logerr("Failed to register: '%s', init failed: '%s'.",
                        component_name, wo_get_runtime_error(cvm));
                    wo_close_vm(cvm);
                }
                else
                {
                    auto entered = wo_enter_gcguard(cvm);

                    wo_value cvm_s = wo_reserve_stack(cvm, 1, nullptr);
                    wo_set_string(cvm_s + 0, component_name);
                    auto* retval = wo_invoke_value(cvm, &initfunc, 1, nullptr, &cvm_s);
                    wo_pop_stack(cvm, 1);

                    if (entered)
                        wo_leave_gcguard(cvm);

                    if (nullptr == retval)
                    {
                        jeecs::debug::logerr("Failed to register: '%s', '_init_towoo_component' failed: '%s'.",
                            component_name, wo_get_runtime_error(cvm));
                        wo_close_vm(cvm);
                    }
                    else
                    {
                        auto result = wo_ret_option_val(vm, retval);
                        wo_close_vm(cvm);

                        if (leaved)
                            wo_enter_gcguard(vm);

                        return result;
                    }
                }
            }
        }
        else
        {
            jeecs::debug::logerr("Failed to register: '%s' failed to compile:\n%s",
                component_name, wo_get_compile_error(cvm, WO_NEED_COLOR));
            wo_close_vm(cvm);
        }
    }
    else
    {
        jeecs::debug::logerr("Failed to register: '%s' unable to open file '%s'.",
            component_name, component_path);
    }

    if (leaved)
        wo_enter_gcguard(vm);
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_towoo_unregister_component(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* t = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    je_typing_unregister(t);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_update_api(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    {
        je_towoo_update_api();
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_typemgr_get_unregister_count(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, (wo_integer_t)jedbg_get_unregister_type_count());
}
WO_API wo_api wojeapi_get_woolang_commit_sha(wo_vm vm, wo_value args)
{
    return wo_ret_string(vm, wo_commit_sha());
}
WO_API wo_api wojeapi_get_all_internal_scripts(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);

    wo_value result = s + 0;
    wo_value key = s + 1;
    wo_value val = s + 2;

    wo_set_map(result, 0);

    auto* iter = wo_open_virtual_file_iter();
    while (wo_string_t vpath = wo_next_virtual_file_iter(iter))
    {
        if (auto* vfhandle = wo_open_virtual_file(vpath))
        {
            size_t len;
            auto* dat = wo_virtual_file_data(vfhandle, &len);

            wo_set_string(key, vpath);
            wo_set_buffer(val, dat, len);
            wo_map_set(result, key, val);
        }
    }
    wo_close_virtual_file_iter(iter);
    return wo_ret_val(vm, result);
}

struct dynamic_parser_impl_t
{
    wo_unref_value m_saving;
    wo_unref_value m_restoring;
    wo_unref_value m_edit;

    const jeecs::typing::typeinfo_script_parser*
        m_script_parser;
};

struct dynamic_parser_global_context_t
{
    using parser_table_t =
        std::unordered_map<jeecs::typing::typeid_t, std::unique_ptr<dynamic_parser_impl_t>>;

    std::mutex _je_dynamic_parser_mx;

    parser_table_t _je_dynamic_parser_impls;
    wo_vm _je_dynamic_parser_vm = nullptr;
};
static dynamic_parser_global_context_t _je_dynamic_parser_global_context;


void _je_dynamic_parser_clear()
{
    if (_je_dynamic_parser_global_context._je_dynamic_parser_vm != nullptr)
    {
        _je_dynamic_parser_global_context._je_dynamic_parser_impls.clear();

        wo_close_vm(_je_dynamic_parser_global_context._je_dynamic_parser_vm);
        _je_dynamic_parser_global_context._je_dynamic_parser_vm = nullptr;
    }

    assert(_je_dynamic_parser_global_context._je_dynamic_parser_impls.empty());
}
void _je_dynamic_parser_update_types()
{
    if (_je_dynamic_parser_global_context._je_dynamic_parser_vm != nullptr)
    {
        _je_dynamic_parser_global_context._je_dynamic_parser_impls.clear();

        auto** types = jedbg_get_all_registed_types();

        auto** cur_type = types;
        while (*cur_type)
        {
            auto* script_parser = (*cur_type)->get_script_parser();
            if (script_parser != nullptr)
            {
                std::string script_woolang_typename = script_parser->m_woolang_typename;

                wo_unref_value saving_func;
                wo_unref_value restoring_func;
                wo_unref_value edit_func;

                if (wo_extern_symb(
                    &saving_func, _je_dynamic_parser_global_context._je_dynamic_parser_vm, (script_woolang_typename + "::parser::saving").c_str()) == WO_TRUE
                    && wo_extern_symb(
                        &restoring_func, _je_dynamic_parser_global_context._je_dynamic_parser_vm, (script_woolang_typename + "::parser::restoring").c_str()) == WO_TRUE
                    && wo_extern_symb(
                        &edit_func, _je_dynamic_parser_global_context._je_dynamic_parser_vm, (script_woolang_typename + "::parser::edit").c_str()) == WO_TRUE)
                {
                    auto p = std::make_unique<dynamic_parser_impl_t>();

                    wo_set_val(&p->m_saving, &saving_func);
                    wo_set_val(&p->m_restoring, &restoring_func);
                    wo_set_val(&p->m_edit, &edit_func);
                    p->m_script_parser = script_parser;

                    _je_dynamic_parser_global_context._je_dynamic_parser_impls.insert(
                        std::make_pair((*cur_type)->m_id, std::move(p)));
                }
            }

            ++cur_type;
        }

        je_mem_free(types);
    }
    else
    {
        assert(_je_dynamic_parser_global_context._je_dynamic_parser_impls.empty());
    }
}
std::optional<std::string> _je_dynamic_parser_update_all(const char* path)
{
    using namespace std;

    auto* file = jeecs_file_open(path);
    if (file == nullptr)
        return std::optional("Failed to open '"s + path + "'.");

    size_t filelen = file->m_file_length;

    char* content = (char*)malloc(filelen + 1);
    jeecs_file_read(content, sizeof(char), filelen, file);

    content[filelen] = 0;

    jeecs_file_close(file);

    wo_vm newvm = wo_create_vm();
    if (wo_load_binary(newvm, path, content, filelen) == WO_FALSE)
    {
        free(content);

        std::string result = wo_get_compile_error(newvm, WO_DEFAULT);
        wo_close_vm(newvm);

        return std::optional(result);
    }

    wo_jit(newvm);
    wo_run(newvm);

    free(content);

    _je_dynamic_parser_clear();
    _je_dynamic_parser_global_context._je_dynamic_parser_vm = newvm;
    _je_dynamic_parser_update_types();

    return std::nullopt;
}

////////////////////////////////////////////////////////////////

WO_API wo_api wojeapi_dynamic_parser_update_script(wo_vm vm, wo_value args)
{
    std::optional<std::string> result;

    auto leaved = wo_leave_gcguard(vm);
    {
        std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
        result = _je_dynamic_parser_update_all(wo_string(args + 0));
    }
    if (leaved)
        wo_enter_gcguard(vm);

    if (result.has_value())
        return wo_ret_err_string(vm, result.value().c_str());
    return wo_ret_ok_void(vm);
}

WO_API wo_api wojeapi_dynamic_parser_update_type(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    {
        std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
        _je_dynamic_parser_update_types();
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_dynamic_parser_clear(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    {
        std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
        _je_dynamic_parser_clear();
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_dynamic_parser_saving(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);

    auto* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_global_context._je_dynamic_parser_vm != nullptr);

        auto* val = wo_pointer(args + 1);
        auto& parser = fnd->second;

        wo_value value = s + 0;
        parser->m_script_parser->m_script_parse_c2w(val, vm, value);

        wo_value _je_dynamic_parser_vm_s = wo_reserve_stack(
            _je_dynamic_parser_global_context._je_dynamic_parser_vm, 1, nullptr);

        wo_set_val(_je_dynamic_parser_vm_s + 0, value);
        wo_value result = wo_invoke_value(
            _je_dynamic_parser_global_context._je_dynamic_parser_vm,
            &parser->m_saving,
            1,
            nullptr,
            &_je_dynamic_parser_vm_s);

        wo_pop_stack(_je_dynamic_parser_global_context._je_dynamic_parser_vm, 1);

        if (result != nullptr)
            return wo_ret_option_val(vm, result);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_dynamic_parser_restoring(wo_vm vm, wo_value args)
{
    std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);

    auto* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_global_context._je_dynamic_parser_vm != nullptr);

        void* val = wo_pointer(args + 1);
        auto& parser = fnd->second;

        wo_value _je_dynamic_parser_vm_s = wo_reserve_stack(
            _je_dynamic_parser_global_context._je_dynamic_parser_vm, 1, nullptr);

        wo_set_val(_je_dynamic_parser_vm_s + 0, args + 2);

        wo_value result = wo_invoke_value(
            _je_dynamic_parser_global_context._je_dynamic_parser_vm,
            &parser->m_restoring,
            1,
            nullptr,
            &_je_dynamic_parser_vm_s);

        wo_pop_stack(_je_dynamic_parser_global_context._je_dynamic_parser_vm, 1);

        if (result != nullptr)
        {
            parser->m_script_parser->m_script_parse_w2c(val, vm, result);
            return wo_ret_bool(vm, WO_TRUE);
        }
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api wojeapi_dynamic_parser_edit(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto leaved = wo_leave_gcguard(vm);
    std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
    if (leaved)
        wo_enter_gcguard(vm);

    auto* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_global_context._je_dynamic_parser_vm != nullptr);

        auto* val = wo_pointer(args + 1);
        auto* tag = wo_string(args + 2);

        auto& parser = fnd->second;

        wo_value value = s + 0;
        parser->m_script_parser->m_script_parse_c2w(val, vm, value);

        wo_value result;
        auto swapback = wo_swap_gcguard(_je_dynamic_parser_global_context._je_dynamic_parser_vm);
        {
            wo_value _je_dynamic_parser_vm_s = wo_reserve_stack(
                _je_dynamic_parser_global_context._je_dynamic_parser_vm, 2, nullptr);

            wo_set_val(_je_dynamic_parser_vm_s + 0, value);
            wo_set_string(
                _je_dynamic_parser_vm_s + 1, tag);

            result = wo_invoke_value(
                _je_dynamic_parser_global_context._je_dynamic_parser_vm,
                &parser->m_edit,
                2,
                nullptr,
                &_je_dynamic_parser_vm_s);

            wo_pop_stack(_je_dynamic_parser_global_context._je_dynamic_parser_vm, 2);
        }
        wo_swap_gcguard(swapback);

        if (result != nullptr)
        {
            if (wo_option_get(value, result))
                parser->m_script_parser->m_script_parse_w2c(val, vm, value);

            return wo_ret_bool(vm, WO_TRUE);
        }
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api wojeapi_audio_buffer_load(wo_vm vm, wo_value args)
{
    auto buffer = jeecs::audio::buffer::load(wo_string(args + 0));
    if (buffer)
    {
        return wo_ret_option_gchandle(
            vm,
            new jeecs::basic::resource<jeecs::audio::buffer>(buffer.value()),
            nullptr,
            [](void* p)
            {
                delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::buffer>*>(p);
            });
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_audio_buffer_info(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 2, &args);
    wo_value result = stacks + 0;
    wo_value elem = stacks + 1;

    jeecs::basic::resource<jeecs::audio::buffer>* buffer =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::buffer> *>(wo_pointer(args + 0));

    wo_set_struct(result, 5);
    auto* buffer_instance = (*buffer)->handle();

    wo_set_int(elem, (wo_int_t)buffer_instance->m_size);
    wo_struct_set(result, 0, elem);

    wo_set_int(elem, (wo_int_t)buffer_instance->m_sample_rate);
    wo_struct_set(result, 1, elem);

    wo_set_int(elem, (wo_int_t)buffer_instance->m_sample_size);
    wo_struct_set(result, 2, elem);

    wo_set_int(elem, (wo_int_t)buffer_instance->m_byte_rate);
    wo_struct_set(result, 3, elem);

    wo_set_int(elem, (wo_int_t)buffer_instance->m_format);
    wo_struct_set(result, 4, elem);

    return wo_ret_val(vm, result);
}
/*
extern("libjoyecs", "wojeapi_audio_filter_create")
    public func create()=> filter;

extern("libjoyecs", "wojeapi_audio_filter_info")
    public func info(self: filter)=> filter_data_t;

extern("libjoyecs", "wojeapi_audio_filter_update")
    func _update(self: filter, info: filter_data_t)=> void;
*/
WO_API wo_api wojeapi_audio_filter_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(
        vm,
        new jeecs::basic::resource<jeecs::audio::filter>(jeecs::audio::filter::create()),
        nullptr,
        [](void* p)
        {
            delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::filter>*>(p);
        });
}

WO_API wo_api wojeapi_audio_filter_info(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 2, &args);
    wo_value result = stacks + 0;
    wo_value elem = stacks + 1;

    jeecs::basic::resource<jeecs::audio::filter>* filter =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::filter> *>(wo_pointer(args + 0));

    wo_set_struct(result, 4);

    auto* filter_instance = (*filter)->handle();

    wo_set_int(elem, (wo_int_t)filter_instance->m_type);
    wo_struct_set(result, 0, elem);

    wo_set_float(elem, filter_instance->m_gain);
    wo_struct_set(result, 1, elem);

    wo_set_float(elem, filter_instance->m_gain_lf);
    wo_struct_set(result, 2, elem);

    wo_set_float(elem, filter_instance->m_gain_hf);
    wo_struct_set(result, 3, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_audio_filter_update(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 1, &args);
    wo_value elem = stacks + 0;

    jeecs::basic::resource<jeecs::audio::filter>* filter =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::filter> *>(wo_pointer(args + 0));
    wo_value updated_info = args + 1;

    (*filter)->update([&](jeal_filter* flt)
        {
            wo_struct_get(elem, updated_info, 0);
            flt->m_type = (jeal_filter_type)wo_int(elem);

            wo_struct_get(elem, updated_info, 1);
            flt->m_gain = wo_float(elem);

            wo_struct_get(elem, updated_info, 2);
            flt->m_gain_lf = wo_float(elem);

            wo_struct_get(elem, updated_info, 3);
            flt->m_gain_hf = wo_float(elem);
        });

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_audio_source_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(
        vm,
        new jeecs::basic::resource<jeecs::audio::source>(jeecs::audio::source::create()),
        nullptr,
        [](void* p)
        {
            delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::source>*>(p);
        });
}
WO_API wo_api wojeapi_audio_source_info(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 3, &args);
    wo_value result = stacks + 0;
    wo_value elem = stacks + 1;
    wo_value elem2 = stacks + 2;

    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    wo_set_struct(result, 5);

    auto* source_instance = (*source)->handle();

    wo_set_bool(elem, source_instance->m_loop);
    wo_struct_set(result, 0, elem);

    wo_set_float(elem, source_instance->m_gain);
    wo_struct_set(result, 1, elem);

    wo_set_float(elem, source_instance->m_pitch);
    wo_struct_set(result, 2, elem);

    wo_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        wo_set_float(elem2, source_instance->m_location[i]);
        wo_struct_set(elem, i, elem2);
    }
    wo_struct_set(result, 3, elem);

    wo_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        wo_set_float(elem2, source_instance->m_velocity[i]);
        wo_struct_set(elem, i, elem2);
    }
    wo_struct_set(result, 4, elem);

    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_audio_source_update(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 2, &args);
    wo_value elem = stacks + 0;
    wo_value elem2 = stacks + 1;

    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));
    wo_value updated_info = args + 1;

    (*source)->update(
        [&](jeal_source* src)
        {
            wo_struct_get(elem, updated_info, 0);
            src->m_loop = wo_bool(elem);

            wo_struct_get(elem, updated_info, 1);
            src->m_gain = wo_float(elem);

            wo_struct_get(elem, updated_info, 2);
            src->m_pitch = wo_float(elem);

            wo_struct_get(elem, updated_info, 3);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                src->m_location[i] = wo_float(elem2);
            }

            wo_struct_get(elem, updated_info, 4);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                src->m_velocity[i] = wo_float(elem2);
            }
        });

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_play(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    (*source)->play();

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_stop(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    (*source)->stop();

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_pause(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    (*source)->pause();

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_buffer(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));
    jeecs::basic::resource<jeecs::audio::buffer>* buffer =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::buffer> *>(wo_pointer(args + 1));

    (*source)->set_playing_buffer(*buffer);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_filter(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 1, &args);
    wo_value elem = stacks + 0;

    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    std::optional<jeecs::basic::resource<jeecs::audio::filter>> filter;
    if (wo_option_get(elem, args + 1))
    {
        filter.emplace(
            *reinterpret_cast<jeecs::basic::resource<jeecs::audio::filter> *>(
                wo_pointer(elem)));
    }

    (*source)->set_filter(filter);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_bind_effect_slot_and_filter(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 1, &args);
    wo_value elem = stacks + 0;

    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    std::optional<jeecs::basic::resource<jeecs::audio::effect_slot>> effect_slot;
    std::optional<jeecs::basic::resource<jeecs::audio::filter>> filter;

    if (wo_option_get(elem, args + 1))
    {
        effect_slot.emplace(
            *reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot> *>(
                wo_pointer(elem)));
    }
    if (wo_option_get(elem, args + 2))
    {
        filter.emplace(
            *reinterpret_cast<jeecs::basic::resource<jeecs::audio::filter> *>(
                wo_pointer(elem)));
    }

    (*source)->bind_effect_slot((size_t)wo_int(args + 3), effect_slot, filter);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_get_state(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    return wo_ret_int(vm, (wo_integer_t)(*source)->get_state());
}
WO_API wo_api wojeapi_audio_source_get_offset(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    return wo_ret_int(vm, (wo_integer_t)(*source)->get_playing_offset());
}
WO_API wo_api wojeapi_audio_source_set_offset(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(wo_pointer(args + 0));

    (*source)->set_playing_offset((size_t)wo_int(args + 1));

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_audio_listener_info(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 3, &args);
    wo_value result = stacks + 0;
    wo_value elem = stacks + 1;
    wo_value elem2 = stacks + 2;

    auto* listener_instance = jeal_get_listener();

    wo_set_struct(result, 6);

    wo_set_float(elem, listener_instance->m_gain);
    wo_struct_set(result, 0, elem);

    wo_set_float(elem, listener_instance->m_global_gain);
    wo_struct_set(result, 1, elem);

    wo_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        wo_set_float(elem2, listener_instance->m_location[i]);
        wo_struct_set(elem, i, elem2);
    }
    wo_struct_set(result, 2, elem);

    wo_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        wo_set_float(elem2, listener_instance->m_velocity[i]);
        wo_struct_set(elem, i, elem2);
    }
    wo_struct_set(result, 3, elem);

    wo_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        wo_set_float(elem2, listener_instance->m_forward[i]);
        wo_struct_set(elem, i, elem2);
    }
    wo_struct_set(result, 4, elem);

    wo_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        wo_set_float(elem2, listener_instance->m_upward[i]);
        wo_struct_set(elem, i, elem2);
    }
    wo_struct_set(result, 5, elem);

    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_audio_listener_update(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 2, &args);
    wo_value elem = stacks + 0;
    wo_value elem2 = stacks + 1;

    wo_value updated_info = args + 0;

    jeecs::audio::listener::update(
        [&](jeal_listener* lstn)
        {
            wo_struct_get(elem, updated_info, 0);
            lstn->m_gain = wo_float(elem);

            wo_struct_get(elem, updated_info, 1);
            lstn->m_global_gain = wo_float(elem);

            wo_struct_get(elem, updated_info, 2);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                lstn->m_location[i] = wo_float(elem2);
            }

            wo_struct_get(elem, updated_info, 3);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                lstn->m_velocity[i] = wo_float(elem2);
            }

            wo_struct_get(elem, updated_info, 4);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                lstn->m_forward[i] = wo_float(elem2);
            }

            wo_struct_get(elem, updated_info, 5);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                lstn->m_upward[i] = wo_float(elem2);
            }
        });

    return wo_ret_void(vm);
}

enum class woolang_je_audio_effect_kind
{
    REVERB,
    CHORUS,
    DISTORTION,
    ECHO,
    FLANGER,
    FREQUENCY_SHIFTER,
    VOCAL_MORPHER,
    PITCH_SHIFTER,
    RING_MODULATOR,
    AUTOWAH,
    COMPRESSOR,
    EQUALIZER,
    EAXREVERB,

    UNKOWN,
};

#define wo_je_effect_res_t(effect_type_name) \
    jeecs::basic::resource<jeecs::audio::effect<effect_type_name>>

WO_API wo_api wojeapi_audio_effect_create(wo_vm vm, wo_value args)
{
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)wo_int(args + 0);
    switch (kind)
    {
#define wo_ret_jeal_new_effect(effect_type_name)                                                          \
    wo_ret_gchandle(                                                                                      \
        vm,                                                                                               \
        new wo_je_effect_res_t(effect_type_name)(                                                         \
            jeecs::audio::effect<effect_type_name>::create()),                                            \
        nullptr,                                                                                          \
        [](void *p)                                                                                       \
        {                                                                                                 \
            delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect<effect_type_name>> *>(p); \
        })

    case woolang_je_audio_effect_kind::REVERB:
        return wo_ret_jeal_new_effect(jeal_effect_reverb);
    case woolang_je_audio_effect_kind::CHORUS:
        return wo_ret_jeal_new_effect(jeal_effect_chorus);
    case woolang_je_audio_effect_kind::DISTORTION:
        return wo_ret_jeal_new_effect(jeal_effect_distortion);
    case woolang_je_audio_effect_kind::ECHO:
        return wo_ret_jeal_new_effect(jeal_effect_echo);
    case woolang_je_audio_effect_kind::FLANGER:
        return wo_ret_jeal_new_effect(jeal_effect_flanger);
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
        return wo_ret_jeal_new_effect(jeal_effect_frequency_shifter);
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
        return wo_ret_jeal_new_effect(jeal_effect_vocal_morpher);
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
        return wo_ret_jeal_new_effect(jeal_effect_pitch_shifter);
    case woolang_je_audio_effect_kind::RING_MODULATOR:
        return wo_ret_jeal_new_effect(jeal_effect_ring_modulator);
    case woolang_je_audio_effect_kind::AUTOWAH:
        return wo_ret_jeal_new_effect(jeal_effect_autowah);
    case woolang_je_audio_effect_kind::COMPRESSOR:
        return wo_ret_jeal_new_effect(jeal_effect_compressor);
    case woolang_je_audio_effect_kind::EQUALIZER:
        return wo_ret_jeal_new_effect(jeal_effect_equalizer);
    case woolang_je_audio_effect_kind::EAXREVERB:
        return wo_ret_jeal_new_effect(jeal_effect_eaxreverb);
    default:
        return wo_ret_panic(vm, "Unknown audio effect type: %d", (int)kind);

#undef wo_ret_jeal_new_effect
    }
}
WO_API wo_api wojeapi_audio_effect_info(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 3, &args);
    wo_value result = stacks + 0;
    wo_value elem = stacks + 1;
    wo_value elem2 = stacks + 2;

    void* effect_res_ptr = wo_pointer(args + 0);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)wo_int(args + 1);

    switch (kind)
    {
    case woolang_je_audio_effect_kind::REVERB:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_reverb)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 13);

        wo_set_float(elem, info->m_density);
        wo_struct_set(result, 0, elem);

        wo_set_float(elem, info->m_diffusion);
        wo_struct_set(result, 1, elem);

        wo_set_float(elem, info->m_gain);
        wo_struct_set(result, 2, elem);

        wo_set_float(elem, info->m_gain_hf);
        wo_struct_set(result, 3, elem);

        wo_set_float(elem, info->m_decay_time);
        wo_struct_set(result, 4, elem);

        wo_set_float(elem, info->m_decay_hf_ratio);
        wo_struct_set(result, 5, elem);

        wo_set_float(elem, info->m_reflections_gain);
        wo_struct_set(result, 6, elem);

        wo_set_float(elem, info->m_reflections_delay);
        wo_struct_set(result, 7, elem);

        wo_set_float(elem, info->m_late_reverb_gain);
        wo_struct_set(result, 8, elem);

        wo_set_float(elem, info->m_late_reverb_delay);
        wo_struct_set(result, 9, elem);

        wo_set_float(elem, info->m_air_absorption_gain_hf);
        wo_struct_set(result, 10, elem);

        wo_set_float(elem, info->m_room_rolloff_factor);
        wo_struct_set(result, 11, elem);

        wo_set_bool(elem, info->m_decay_hf_limit);
        wo_struct_set(result, 12, elem);

        break;
    }
    case woolang_je_audio_effect_kind::CHORUS:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_chorus)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 6);

        wo_set_int(elem, (wo_integer_t)info->m_waveform);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (wo_integer_t)info->m_phase);
        wo_struct_set(result, 1, elem);

        wo_set_float(elem, info->m_rate);
        wo_struct_set(result, 2, elem);

        wo_set_float(elem, info->m_depth);
        wo_struct_set(result, 3, elem);

        wo_set_float(elem, info->m_feedback);
        wo_struct_set(result, 4, elem);

        wo_set_float(elem, info->m_delay);
        wo_struct_set(result, 5, elem);

        break;
    }
    case woolang_je_audio_effect_kind::DISTORTION:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_distortion)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 5);

        wo_set_float(elem, info->m_edge);
        wo_struct_set(result, 0, elem);

        wo_set_float(elem, info->m_gain);
        wo_struct_set(result, 1, elem);

        wo_set_float(elem, info->m_lowpass_cutoff);
        wo_struct_set(result, 2, elem);

        wo_set_float(elem, info->m_equalizer_center_freq);
        wo_struct_set(result, 3, elem);

        wo_set_float(elem, info->m_equalizer_bandwidth);
        wo_struct_set(result, 4, elem);

        break;
    }
    case woolang_je_audio_effect_kind::ECHO:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_echo)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 5);

        wo_set_float(elem, info->m_delay);
        wo_struct_set(result, 0, elem);

        wo_set_float(elem, info->m_lr_delay);
        wo_struct_set(result, 1, elem);

        wo_set_float(elem, info->m_damping);
        wo_struct_set(result, 2, elem);

        wo_set_float(elem, info->m_feedback);
        wo_struct_set(result, 3, elem);

        wo_set_float(elem, info->m_spread);
        wo_struct_set(result, 4, elem);

        break;
    }
    case woolang_je_audio_effect_kind::FLANGER:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_flanger)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 6);

        wo_set_int(elem, (wo_integer_t)info->m_waveform);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (wo_integer_t)info->m_phase);
        wo_struct_set(result, 1, elem);

        wo_set_float(elem, info->m_rate);
        wo_struct_set(result, 2, elem);

        wo_set_float(elem, info->m_depth);
        wo_struct_set(result, 3, elem);

        wo_set_float(elem, info->m_feedback);
        wo_struct_set(result, 4, elem);

        wo_set_float(elem, info->m_delay);
        wo_struct_set(result, 5, elem);

        break;
    }
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_frequency_shifter)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 3);

        wo_set_float(elem, info->m_frequency);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (wo_integer_t)info->m_left_direction);
        wo_struct_set(result, 1, elem);

        wo_set_int(elem, (wo_integer_t)info->m_right_direction);
        wo_struct_set(result, 2, elem);

        break;
    }
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_vocal_morpher)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 6);

        wo_set_int(elem, (wo_integer_t)info->m_phoneme_a);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (wo_integer_t)info->m_phoneme_a_coarse_tuning);
        wo_struct_set(result, 1, elem);

        wo_set_int(elem, (wo_integer_t)info->m_phoneme_b);
        wo_struct_set(result, 2, elem);

        wo_set_int(elem, (wo_integer_t)info->m_phoneme_b_coarse_tuning);
        wo_struct_set(result, 3, elem);

        wo_set_int(elem, (wo_integer_t)info->m_waveform);
        wo_struct_set(result, 4, elem);

        wo_set_float(elem, info->m_rate);
        wo_struct_set(result, 5, elem);

        break;
    }
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_pitch_shifter)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 2);

        wo_set_int(elem, (wo_integer_t)info->m_coarse_tune);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (wo_integer_t)info->m_fine_tune);
        wo_struct_set(result, 1, elem);

        break;
    }
    case woolang_je_audio_effect_kind::RING_MODULATOR:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_ring_modulator)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 3);

        wo_set_float(elem, info->m_frequency);
        wo_struct_set(result, 0, elem);

        wo_set_float(elem, info->m_highpass_cutoff);
        wo_struct_set(result, 1, elem);

        wo_set_int(elem, (wo_integer_t)info->m_waveform);
        wo_struct_set(result, 2, elem);

        break;
    }
    case woolang_je_audio_effect_kind::AUTOWAH:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_autowah)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 4);

        wo_set_float(elem, info->m_attack_time);
        wo_struct_set(result, 0, elem);

        wo_set_float(elem, info->m_release_time);
        wo_struct_set(result, 1, elem);

        wo_set_float(elem, info->m_resonance);
        wo_struct_set(result, 2, elem);

        wo_set_float(elem, info->m_peak_gain);
        wo_struct_set(result, 3, elem);

        break;
    }
    case woolang_je_audio_effect_kind::COMPRESSOR:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_compressor)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 1);

        wo_set_bool(elem, info->m_enabled);
        wo_struct_set(result, 0, elem);

        break;
    }
    case woolang_je_audio_effect_kind::EQUALIZER:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_equalizer)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 10);

        wo_set_float(elem, info->m_low_gain);
        wo_struct_set(result, 0, elem);

        wo_set_float(elem, info->m_low_cutoff);
        wo_struct_set(result, 1, elem);

        wo_set_float(elem, info->m_mid1_gain);
        wo_struct_set(result, 2, elem);

        wo_set_float(elem, info->m_mid1_center);
        wo_struct_set(result, 3, elem);

        wo_set_float(elem, info->m_mid1_width);
        wo_struct_set(result, 4, elem);

        wo_set_float(elem, info->m_mid2_gain);
        wo_struct_set(result, 5, elem);

        wo_set_float(elem, info->m_mid2_center);
        wo_struct_set(result, 6, elem);

        wo_set_float(elem, info->m_mid2_width);
        wo_struct_set(result, 7, elem);

        wo_set_float(elem, info->m_high_gain);
        wo_struct_set(result, 8, elem);

        wo_set_float(elem, info->m_high_cutoff);
        wo_struct_set(result, 9, elem);

        break;
    }
    case woolang_je_audio_effect_kind::EAXREVERB:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_eaxreverb)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 23);

        wo_set_float(elem, info->m_density);
        wo_struct_set(result, 0, elem);

        wo_set_float(elem, info->m_diffusion);
        wo_struct_set(result, 1, elem);

        wo_set_float(elem, info->m_gain);
        wo_struct_set(result, 2, elem);

        wo_set_float(elem, info->m_gain_hf);
        wo_struct_set(result, 3, elem);

        wo_set_float(elem, info->m_gain_lf);
        wo_struct_set(result, 4, elem);

        wo_set_float(elem, info->m_decay_time);
        wo_struct_set(result, 5, elem);

        wo_set_float(elem, info->m_decay_hf_ratio);
        wo_struct_set(result, 6, elem);

        wo_set_float(elem, info->m_decay_lf_ratio);
        wo_struct_set(result, 7, elem);

        wo_set_float(elem, info->m_reflections_gain);
        wo_struct_set(result, 8, elem);

        wo_set_float(elem, info->m_reflections_delay);
        wo_struct_set(result, 9, elem);

        wo_set_struct(elem, 3);
        for (uint16_t i = 0; i < 3; ++i)
        {
            wo_set_float(elem2, info->m_reflections_pan_xyz[i]);
            wo_struct_set(elem, i, elem2);
        }
        wo_struct_set(result, 10, elem);

        wo_set_float(elem, info->m_late_reverb_gain);
        wo_struct_set(result, 11, elem);

        wo_set_float(elem, info->m_late_reverb_delay);
        wo_struct_set(result, 12, elem);

        wo_set_struct(elem, 3);
        for (uint16_t i = 0; i < 3; ++i)
        {
            wo_set_float(elem2, info->m_late_reverb_pan_xyz[i]);
            wo_struct_set(elem, i, elem2);
        }
        wo_struct_set(result, 13, elem);

        wo_set_float(elem, info->m_echo_time);
        wo_struct_set(result, 14, elem);

        wo_set_float(elem, info->m_echo_depth);
        wo_struct_set(result, 15, elem);

        wo_set_float(elem, info->m_modulation_time);
        wo_struct_set(result, 16, elem);

        wo_set_float(elem, info->m_modulation_depth);
        wo_struct_set(result, 17, elem);

        wo_set_float(elem, info->m_air_absorption_gain_hf);
        wo_struct_set(result, 18, elem);

        wo_set_float(elem, info->m_hf_reference);
        wo_struct_set(result, 19, elem);

        wo_set_float(elem, info->m_lf_reference);
        wo_struct_set(result, 20, elem);

        wo_set_float(elem, info->m_room_rolloff_factor);
        wo_struct_set(result, 21, elem);

        wo_set_bool(elem, info->m_decay_hf_limit);
        wo_struct_set(result, 22, elem);

        break;
    }
    default:
        return wo_ret_panic(vm, "Unknown audio effect type: %d", (int)kind);
    }

    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_audio_effect_update(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 2, &args);
    wo_value elem = stacks + 0;
    wo_value elem2 = stacks + 1;

    void* effect = wo_pointer(args + 0);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)wo_int(args + 1);
    wo_value updated_info = args + 2;

    switch (kind)
    {
    case woolang_je_audio_effect_kind::REVERB:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_reverb)*>(effect);

        (*effect_instance)->update([&](jeal_effect_reverb* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_density = wo_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_diffusion = wo_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_gain_hf = wo_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_decay_time = wo_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_decay_hf_ratio = wo_float(elem);

                wo_struct_get(elem, updated_info, 6);
                eff->m_reflections_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 7);
                eff->m_reflections_delay = wo_float(elem);

                wo_struct_get(elem, updated_info, 8);
                eff->m_late_reverb_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 9);
                eff->m_late_reverb_delay = wo_float(elem);

                wo_struct_get(elem, updated_info, 10);
                eff->m_air_absorption_gain_hf = wo_float(elem);

                wo_struct_get(elem, updated_info, 11);
                eff->m_room_rolloff_factor = wo_float(elem);

                wo_struct_get(elem, updated_info, 12);
                eff->m_decay_hf_limit = (wo_bool_t)wo_bool(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::CHORUS:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_chorus)*>(effect);

        (*effect_instance)->update([&](jeal_effect_chorus* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_waveform = (jeal_effect_chorus::waveform)wo_int(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_phase = (int)wo_int(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_rate = wo_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_depth = wo_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_feedback = wo_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_delay = wo_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::DISTORTION:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_distortion)*>(effect);

        (*effect_instance)->update([&](jeal_effect_distortion* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_edge = wo_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_lowpass_cutoff = wo_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_equalizer_center_freq = wo_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_equalizer_bandwidth = wo_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::ECHO:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_echo)*>(effect);

        (*effect_instance)->update([&](jeal_effect_echo* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_delay = wo_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_lr_delay = wo_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_damping = wo_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_feedback = wo_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_spread = wo_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::FLANGER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_flanger)*>(effect);

        (*effect_instance)->update([&](jeal_effect_flanger* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_waveform = (jeal_effect_flanger::waveform)wo_int(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_phase = (int)wo_int(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_rate = wo_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_depth = wo_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_feedback = wo_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_delay = wo_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_frequency_shifter)*>(effect);

        (*effect_instance)->update([&](jeal_effect_frequency_shifter* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_frequency = wo_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_left_direction = (jeal_effect_frequency_shifter::direction)wo_int(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_right_direction = (jeal_effect_frequency_shifter::direction)wo_int(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_vocal_morpher)*>(effect);

        (*effect_instance)->update([&](jeal_effect_vocal_morpher* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_phoneme_a = (jeal_effect_vocal_morpher::phoneme)wo_int(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_phoneme_a_coarse_tuning = (int)wo_int(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_phoneme_b = (jeal_effect_vocal_morpher::phoneme)wo_int(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_phoneme_b_coarse_tuning = (int)wo_int(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_waveform = (jeal_effect_vocal_morpher::waveform)wo_int(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_rate = wo_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_pitch_shifter)*>(effect);

        (*effect_instance)->update([&](jeal_effect_pitch_shifter* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_coarse_tune = (int)wo_int(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_fine_tune = (int)wo_int(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::RING_MODULATOR:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_ring_modulator)*>(effect);

        (*effect_instance)->update([&](jeal_effect_ring_modulator* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_frequency = wo_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_highpass_cutoff = wo_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_waveform = (jeal_effect_ring_modulator::waveform)wo_int(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::AUTOWAH:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_autowah)*>(effect);

        (*effect_instance)->update([&](jeal_effect_autowah* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_attack_time = wo_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_release_time = wo_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_resonance = wo_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_peak_gain = wo_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::COMPRESSOR:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_compressor)*>(effect);

        (*effect_instance)->update([&](jeal_effect_compressor* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_enabled = (wo_bool_t)wo_bool(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::EQUALIZER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_equalizer)*>(effect);

        (*effect_instance)->update([&](jeal_effect_equalizer* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_low_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_low_cutoff = wo_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_mid1_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_mid1_center = wo_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_mid1_width = wo_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_mid2_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 6);
                eff->m_mid2_center = wo_float(elem);

                wo_struct_get(elem, updated_info, 7);
                eff->m_mid2_width = wo_float(elem);

                wo_struct_get(elem, updated_info, 8);
                eff->m_high_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 9);
                eff->m_high_cutoff = wo_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::EAXREVERB:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_eaxreverb)*>(effect);

        (*effect_instance)->update([&](jeal_effect_eaxreverb* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_density = wo_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_diffusion = wo_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_gain_hf = wo_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_gain_lf = wo_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_decay_time = wo_float(elem);

                wo_struct_get(elem, updated_info, 6);
                eff->m_decay_hf_ratio = wo_float(elem);

                wo_struct_get(elem, updated_info, 7);
                eff->m_decay_lf_ratio = wo_float(elem);

                wo_struct_get(elem, updated_info, 8);
                eff->m_reflections_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 9);
                eff->m_reflections_delay = wo_float(elem);

                wo_struct_get(elem, updated_info, 10);
                for (uint16_t i = 0; i < 3; ++i)
                {
                    wo_struct_get(elem2, elem, i);
                    eff->m_reflections_pan_xyz[i] = wo_float(elem2);
                }

                wo_struct_get(elem, updated_info, 11);
                eff->m_late_reverb_gain = wo_float(elem);

                wo_struct_get(elem, updated_info, 12);
                eff->m_late_reverb_delay = wo_float(elem);

                wo_struct_get(elem, updated_info, 13);
                for (uint16_t i = 0; i < 3; ++i)
                {
                    wo_struct_get(elem2, elem, i);
                    eff->m_late_reverb_pan_xyz[i] = wo_float(elem2);
                }

                wo_struct_get(elem, updated_info, 14);
                eff->m_echo_time = wo_float(elem);

                wo_struct_get(elem, updated_info, 15);
                eff->m_echo_depth = wo_float(elem);

                wo_struct_get(elem, updated_info, 16);
                eff->m_modulation_time = wo_float(elem);

                wo_struct_get(elem, updated_info, 17);
                eff->m_modulation_depth = wo_float(elem);

                wo_struct_get(elem, updated_info, 18);
                eff->m_air_absorption_gain_hf = wo_float(elem);

                wo_struct_get(elem, updated_info, 19);
                eff->m_hf_reference = wo_float(elem);

                wo_struct_get(elem, updated_info, 20);
                eff->m_lf_reference = wo_float(elem);

                wo_struct_get(elem, updated_info, 21);
                eff->m_room_rolloff_factor = wo_float(elem);

                wo_struct_get(elem, updated_info, 22);
                eff->m_decay_hf_limit = (wo_bool_t)wo_bool(elem); });
        break;
    }
    default:
        return wo_ret_panic(vm, "Unknown audio effect type: %d", (int)kind);
    }

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_audio_effect_slot_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(
        vm,
        new jeecs::basic::resource<jeecs::audio::effect_slot>(jeecs::audio::effect_slot::create()),
        nullptr,
        [](void* p)
        { delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot>*>(p); });
}

WO_API wo_api wojeapi_audio_effect_slot_bind_effect(wo_vm vm, wo_value args)
{
    auto* effect_slot =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot> *>(wo_pointer(args + 0));
    auto* effect = wo_pointer(args + 1);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)wo_int(args + 2);

    switch (kind)
    {
    case woolang_je_audio_effect_kind::REVERB:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_reverb)*>(effect));
        break;
    case woolang_je_audio_effect_kind::CHORUS:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_chorus)*>(effect));
        break;
    case woolang_je_audio_effect_kind::DISTORTION:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_distortion)*>(effect));
        break;
    case woolang_je_audio_effect_kind::ECHO:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_echo)*>(effect));
        break;
    case woolang_je_audio_effect_kind::FLANGER:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_flanger)*>(effect));
        break;
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_frequency_shifter)*>(effect));
        break;
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_vocal_morpher)*>(effect));
        break;
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_pitch_shifter)*>(effect));
        break;
    case woolang_je_audio_effect_kind::RING_MODULATOR:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_ring_modulator)*>(effect));
        break;
    case woolang_je_audio_effect_kind::AUTOWAH:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_autowah)*>(effect));
        break;
    case woolang_je_audio_effect_kind::COMPRESSOR:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_compressor)*>(effect));
        break;
    case woolang_je_audio_effect_kind::EQUALIZER:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_equalizer)*>(effect));
        break;
    case woolang_je_audio_effect_kind::EAXREVERB:
        (*effect_slot)->bind_effect(*reinterpret_cast<wo_je_effect_res_t(jeal_effect_eaxreverb)*>(effect));
        break;
    default:
        return wo_ret_panic(vm, "Unknown audio effect type: %d", (int)kind);
    }
    return wo_ret_void(vm);
}

#undef wo_je_effect_res_t

WO_API wo_api wojeapi_audio_effect_slot_info(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 2, &args);
    wo_value result = stacks + 0;
    wo_value elem = stacks + 1;

    auto* effect_slot =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot> *>(wo_pointer(args + 0));

    auto effect_slot_info = (*effect_slot)->handle();

    wo_set_struct(result, 1);

    wo_set_float(elem, effect_slot_info->m_gain);
    wo_struct_set(result, 0, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_audio_effect_slot_update(wo_vm vm, wo_value args)
{
    wo_value stacks = wo_reserve_stack(vm, 1, &args);
    wo_value elem = stacks + 0;

    auto* effect_slot =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot> *>(wo_pointer(args + 0));
    wo_value info = args + 1;

    (*effect_slot)->update([&](jeal_effect_slot* eff)
        {
            wo_struct_get(elem, info, 0);
            eff->m_gain = wo_float(elem); });

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_input_update_mouse_state(wo_vm vm, wo_value args)
{
    auto group = (size_t)wo_int(args + 0);
    auto key = (jeecs::input::mousecode)wo_int(args + 1);
    auto down = wo_bool(args + 2);

    je_io_update_mouse_state(group, key, down);

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_input_update_wheel(wo_vm vm, wo_value args)
{
    auto group = (size_t)wo_int(args + 0);
    je_io_update_wheel(group, wo_float(args + 1), wo_float(args + 2));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_input_update_key_state(wo_vm vm, wo_value args)
{
    auto key = (jeecs::input::keycode)wo_int(args + 0);
    auto down = wo_bool(args + 1);

    je_io_update_key_state(key, down);

    return wo_ret_void(vm);
}
