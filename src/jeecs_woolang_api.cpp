#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <list>

std::atomic_flag log_buffer_mx = {};
std::list<std::pair<int, std::string>> log_buffer;

WO_API wo_api wojeapi_generate_uid(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, je_uid_generate().to_string().c_str());
}

WO_API wo_api wojeapi_build_version(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, je_build_version());
}

WO_API wo_api wojeapi_woolang_version(wo_vm vm, wo_value args, size_t argc)
{
    std::string woolang_version_info = "Woolang ";
    return wo_ret_string(vm, (woolang_version_info + wo_version() + " " + wo_compile_date()).c_str());
}

WO_API wo_api wojeapi_get_rendering_world(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_option_ptr(vm, jedbg_get_rendering_world(wo_pointer(args + 0)));
}

WO_API wo_api wojeapi_crc64_file(wo_vm vm, wo_value args, size_t argc)
{
    wo_integer_t result = wo_crc64_file(wo_string(args + 0));
    if (result)
        return wo_ret_option_int(vm, result);
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_crc64_string(wo_vm vm, wo_value args, size_t argc)
{
    wo_integer_t result = wo_crc64_str(wo_string(args + 0));
    return wo_ret_int(vm, result);
}

WO_API wo_api wojeapi_register_log_callback(wo_vm vm, wo_value args, size_t argc)
{
    std::function<void(int, const char*)>* callbacks =
        new std::function<void(int, const char*)>([&](int level, const char* msg) {
        while (log_buffer_mx.test_and_set());
        log_buffer.push_back({ level, msg });
        log_buffer_mx.clear();
            });

    return wo_ret_handle(vm,
        je_log_register_callback([](int level, const char* msg, void* func)
            {
                (*(std::function<void(int, const char*)>*)func)(level, msg);
            }, callbacks));
}

WO_API wo_api wojeapi_unregister_log_callback(wo_vm vm, wo_value args, size_t argc)
{
    auto func = (std::function<void(int, const char*)>*) je_log_unregister_callback(wo_handle(args + 0));
    delete func;

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_all_logs(wo_vm vm, wo_value args, size_t argc)
{
    wo_value result = wo_push_arr(vm, 0);
    std::list<std::pair<int, std::string>> logs;

    while (log_buffer_mx.test_and_set());
    logs.swap(log_buffer);
    assert(log_buffer.empty());
    log_buffer_mx.clear();

    for (auto& [i, s] : logs)
    {
        wo_value a = wo_arr_add(result, nullptr);
        wo_set_struct(a, 2);
        wo_set_int(wo_struct_get(a, 0), i);
        wo_set_string(wo_struct_get(a, 1), s.c_str());
    }
    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_restart_graphic_interface(wo_vm vm, wo_value args, size_t argc)
{
    void* cur_universe = wo_pointer(args + 0);
    jegl_interface_config config;
    config.m_title = wo_string(args + 1);
    config.m_resolution_x = config.m_windows_width = wo_int(args + 2);
    config.m_resolution_y = config.m_windows_height = wo_int(args + 3);
    config.m_fps = wo_int(args + 4);
    config.m_fullscreen = wo_bool(args + 5);
    jegl_reboot_graphic_thread(jedbg_get_editing_graphic_thread(cur_universe), config);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_current_platform_config(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm,
#if defined(JE_OS_WINDOWS)
        "win32_"
#elif defined(JE_OS_ANDROID)
        "android_"
#elif defined(JE_OS_LINUX)
        "linux_"
#elif defined(JE_OS_UNKNOWN)
        "unknown_"
#else
#error "Known os type."
#endif

#if defined(NDEBUG)
        "release_"
#else
        "debug_"
#endif

#if defined (JE_PLATFORM_X86)
        "x86"
#elif defined (JE_PLATFORM_X64)
        "x64"
#elif defined (JE_PLATFORM_ARM)
        "arm"
#elif defined (JE_PLATFORM_ARM64)
        "arm64"
#elif defined (JE_PLATFORM_UNKNOWN)
        "unknown"
#else
#error "Known os type."
#endif
    );
}

WO_API wo_api wojeapi_load_module(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_option_ptr(vm, je_module_load(wo_string(args + 0), wo_string(args + 1)));
}

WO_API wo_api wojeapi_unload_module(wo_vm vm, wo_value args, size_t argc)
{
    je_module_unload(wo_pointer(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_apply_camera_framebuf_setting(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (jeecs::Camera::RendToFramebuffer* rbf = entity->get_component<jeecs::Camera::RendToFramebuffer>())
    {
        rbf->framebuffer = new jeecs::graphic::framebuffer(
            wo_int(args + 1), wo_int(args + 2), {
                jegl_texture::texture_format::RGB,
                jegl_texture::texture_format::DEPTH,
            }
        );
    }
    else
        jeecs::debug::logfatal("No RendToFramebuffer in specify entity when 'wojeapi_apply_camera_framebuf_setting'.");
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_framebuf_texture(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (jeecs::Camera::RendToFramebuffer* rbf = entity->get_component<jeecs::Camera::RendToFramebuffer>())
    {
        if (!rbf->framebuffer)
        {
            jeecs::debug::logerr("RendToFramebuffer(%p).framebuffer not specify in entity when 'wojeapi_get_framebuf_texture'.",
                rbf);
            return wo_ret_option_none(vm);
        }
        auto tex = rbf->framebuffer->get_attachment(wo_int(args + 1));
        if (!tex)
        {
            jeecs::debug::logerr("RendToFramebuffer(%p).framebuffer not contain attach(%zu) in entity when 'wojeapi_get_framebuf_texture'.",
                rbf, (size_t)wo_int(args + 1));
            return wo_ret_option_none(vm);
        }
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::texture>(tex), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
            });
    }
    else
        jeecs::debug::logerr("No RendToFramebuffer in specify entity when 'wojeapi_get_framebuf_texture'.");
    return wo_ret_option_none(vm);
}

bool _jegl_read_texture_sampling_cache(const char* path, jegl_texture::texture_sampling* samp);
bool _jegl_write_texture_sampling_cache(const char* path, jegl_texture::texture_sampling samp);

WO_API wo_api wojeapi_get_texture_sampling_method_by_path(wo_vm vm, wo_value args, size_t argc)
{
    //extern("libjoyecs", "wojeapi_get_texture_sampling_method_by_path")
    //public func get_texture_sampling_method_by_path(path: string)=> texture_sampling;
    jegl_texture::texture_sampling samp;
    if (_jegl_read_texture_sampling_cache(wo_string(args + 0), &samp))
        return wo_ret_int(vm, (wo_integer_t)samp);
    return wo_ret_int(vm, (wo_integer_t)jegl_texture::texture_sampling::DEFAULT);
}

WO_API wo_api wojeapi_update_texture_sampling_method_by_path(wo_vm vm, wo_value args, size_t argc)
{
    //extern("libjoyecs", "wojeapi_update_texture_sampling_method_by_path")
    //public func update_texture_sampling_method_by_path(path: string, method: texture_sampling)=> result<void, string>;
    if (_jegl_write_texture_sampling_cache(wo_string(args + 0), (jegl_texture::texture_sampling)wo_int(args + 1)))
        return wo_ret_ok_void(vm);
    return wo_ret_err_string(vm, "Failed to write image sampling method, maybe image file not exist.");
}


// ECS UNIVERSE
WO_API wo_api wojeapi_create_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = je_ecs_universe_create();
    return wo_ret_gchandle(vm, universe, nullptr, [](void* universe) {
        jeecs::game_universe::destroy_universe(jeecs::game_universe(universe));
        });
}

WO_API wo_api wojeapi_get_universe_from_world(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = je_ecs_world_in_universe(wo_pointer(args + 0));
    return wo_ret_pointer(vm, universe);
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

WO_API wo_api wojeapi_create_world_in_universe(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_pointer(vm,
        jeecs::game_universe(wo_pointer(args + 0)).create_world().handle());
}

WO_API wo_api wojeapi_get_all_worlds_in_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = wo_pointer(args + 0);
    wo_value out_array = wo_push_arr(vm, 0);

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
    wo_value result = wo_push_arr(vm, 0);;

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
    wo_value out_arr = wo_push_arr(vm, 0);

    auto entities = jedbg_get_all_entities_in_world(wo_pointer(args + 0));
    auto entity_iter = entities;
    while (*entity_iter)
    {
        wo_set_gchandle(wo_arr_add(out_arr, nullptr), vm, *(entity_iter++), nullptr,
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

WO_API wo_api wojeapi_set_editing_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::typing::uid_t uid{ /*0000*/ };
    if (argc)
        uid.parse(wo_string(args + 0));

    jedbg_set_editing_entity_uid(uid);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_editing_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::typing::uid_t uid = jedbg_get_editing_entity_uid();

    if (uid != jeecs::typing::uid_t{})
        return wo_ret_ok_string(vm, uid.to_string().c_str());

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_get_entity_uid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* anc = entity->get_component<jeecs::Transform::Anchor>())
        return wo_ret_option_string(vm, anc->uid.to_string().c_str());

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
    entity->name(wo_string(args + 1));
    return wo_ret_void(vm);
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

    return wo_ret_gchandle(vm, entity,
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_get_all_components_types_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_arr = wo_push_arr(vm, 0);

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
    jeecs::Transform::Anchor* archor = parent->get_component<jeecs::Transform::Anchor>();

    if (l2p && archor)
    {
        return wo_ret_bool(vm, l2p->parent_uid == archor->uid);
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

    if (nullptr == iter->iter)
        return wo_ret_option_none(vm);

    wo_value result = wo_push_struct(vm, 3);

    wo_set_string(wo_struct_get(result, 0), iter->iter->m_member_name);
    wo_set_pointer(wo_struct_get(result, 1), (void*)iter->iter->m_member_type);
    wo_set_handle(wo_struct_get(result, 2), (wo_handle_t)(iter->iter->m_member_offset + (intptr_t)iter->component_addr));
    iter->iter = iter->iter->m_next_member;

    return wo_ret_option_val(vm, result);
}

WO_API wo_api wojeapi_get_components_member(wo_vm vm, wo_value args, size_t argc)
{
    void* component_addr = wo_pointer(args + 0);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);
    wo_string_t member_name = wo_string(args + 2);

    if (auto* member_info = component_type->find_member_by_name(member_name))
    {
        wo_value result = wo_push_struct(vm, 2);
        wo_set_pointer(wo_struct_get(result, 0), (wo_ptr_t)member_info->m_member_type);
        wo_set_handle(wo_struct_get(result, 1),
            (wo_handle_t)(member_info->m_member_offset + (intptr_t)component_addr));
        return wo_ret_option_val(vm, result);
    }
    else
        return wo_ret_option_none(vm);
}

// INPUTS
WO_API wo_api wojeapi_input_keydown(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, jeecs::input::keydown((jeecs::input::keycode)wo_int(args + 0)));
}

WO_API wo_api wojeapi_wheel_count(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_float(vm, jeecs::input::wheel(0));
}

WO_API wo_api wojeapi_input_window_size(wo_vm vm, wo_value args, size_t argc)
{
    auto winsz = jeecs::input::windowsize();

    wo_value result = wo_push_struct(vm, 2);
    wo_set_int(wo_struct_get(result, 0), (wo_int_t)winsz.x);
    wo_set_int(wo_struct_get(result, 1), (wo_int_t)winsz.y);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_update_window_size(wo_vm vm, wo_value args, size_t argc)
{
    je_io_update_windowsize((int)wo_int(args + 0), (int)wo_int(args + 1));
    auto winsz = jeecs::input::windowsize();
    wo_set_int(args + 0, (wo_int_t)winsz.x);
    wo_set_int(args + 1, (wo_int_t)winsz.y);
    return wo_ret_void(vm);
}

// ECS OTHER
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

    jeecs::debug::loginfo("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logwarn(wo_vm vm, wo_value args, size_t argc)
{
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::logwarn("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logerr(wo_vm vm, wo_value args, size_t argc)
{
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::logerr("%s", disp.c_str());
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_logfatal(wo_vm vm, wo_value args, size_t argc)
{
    std::string disp;

    for (size_t i = 0; i < argc; i++)
        disp += wo_cast_string(args + i);

    jeecs::debug::logfatal("%s", disp.c_str());
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
    wo_value out_array = wo_push_arr(vm, 0);

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
        INT, BOOL, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT, TEXTURE
    };
    basic_type type = (basic_type)wo_int(args + 0);

    switch (type)
    {
    case INT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<int>(nullptr));
    case BOOL:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<bool>(nullptr));
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
WO_API wo_api wojeapi_native_value_bool(wo_vm vm, wo_value args, size_t argc)
{
    bool* value = (bool*)wo_pointer(args + 0);
    return wo_ret_bool(vm, *value);
}

WO_API wo_api wojeapi_native_value_int(wo_vm vm, wo_value args, size_t argc)
{
    int* value = (int*)wo_pointer(args + 0);
    return wo_ret_int(vm, *value);
}

WO_API wo_api wojeapi_native_value_float(wo_vm vm, wo_value args, size_t argc)
{
    float* value = (float*)wo_pointer(args + 0);
    return wo_ret_float(vm, *value);
}

WO_API wo_api wojeapi_native_value_float2(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec2* value = (jeecs::math::vec2*)wo_pointer(args + 0);

    wo_value result = wo_push_struct(vm, 2);
    wo_set_float(wo_struct_get(result, 0), value->x);
    wo_set_float(wo_struct_get(result, 1), value->y);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_native_value_float3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec3* value = (jeecs::math::vec3*)wo_pointer(args + 0);

    wo_value result = wo_push_struct(vm, 3);
    wo_set_float(wo_struct_get(result, 0), value->x);
    wo_set_float(wo_struct_get(result, 1), value->y);
    wo_set_float(wo_struct_get(result, 2), value->z);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_native_value_float4(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec4* value = (jeecs::math::vec4*)wo_pointer(args + 0);

    wo_value result = wo_push_struct(vm, 4);
    wo_set_float(wo_struct_get(result, 0), value->x);
    wo_set_float(wo_struct_get(result, 1), value->y);
    wo_set_float(wo_struct_get(result, 2), value->z);
    wo_set_float(wo_struct_get(result, 3), value->w);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_native_value_je_string(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::string* value = (jeecs::string*)wo_pointer(args + 0);
    return wo_ret_string(vm, value->c_str());
}

WO_API wo_api wojeapi_native_value_rot_euler3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::quat* value = (jeecs::math::quat*)wo_pointer(args + 0);
    auto&& euler_v3 = value->euler_angle();

    wo_value result = wo_push_struct(vm, 3);
    wo_set_float(wo_struct_get(result, 0), euler_v3.x);
    wo_set_float(wo_struct_get(result, 1), euler_v3.y);
    wo_set_float(wo_struct_get(result, 2), euler_v3.z);

    return wo_ret_val(vm, result);
}

// set
WO_API wo_api wojeapi_native_value_set_bool(wo_vm vm, wo_value args, size_t argc)
{
    bool* value = (bool*)wo_pointer(args + 0);

    *value = wo_bool(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_set_int(wo_vm vm, wo_value args, size_t argc)
{
    int* value = (int*)wo_pointer(args + 0);

    *value = (int)wo_int(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_set_float(wo_vm vm, wo_value args, size_t argc)
{
    float* value = (float*)wo_pointer(args + 0);

    *value = wo_float(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_set_float2(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec2* value = (jeecs::math::vec2*)wo_pointer(args + 0);

    value->x = wo_float(args + 1);
    value->y = wo_float(args + 2);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_set_float3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec3* value = (jeecs::math::vec3*)wo_pointer(args + 0);

    value->x = wo_float(args + 1);
    value->y = wo_float(args + 2);
    value->z = wo_float(args + 3);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_set_float4(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec4* value = (jeecs::math::vec4*)wo_pointer(args + 0);

    value->x = wo_float(args + 1);
    value->y = wo_float(args + 2);
    value->z = wo_float(args + 3);
    value->w = wo_float(args + 4);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_set_je_string(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::string* value = (jeecs::string*)wo_pointer(args + 0);

    *value = wo_string(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_set_rot_euler3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::quat* value = (jeecs::math::quat*)wo_pointer(args + 0);
    auto&& euler_v3 = value->euler_angle();

    euler_v3.x = wo_float(args + 1);
    euler_v3.y = wo_float(args + 2);
    euler_v3.z = wo_float(args + 3);

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
    auto* loaded_texture = jeecs::graphic::texture::load(wo_string(args + 0));

    if (loaded_texture != nullptr)
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
            });

    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_texture_create(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = jeecs::graphic::texture::create(
        wo_int(args + 0), wo_int(args + 1), jegl_texture::texture_format::RGBA);

    memset(loaded_texture->resouce()->m_raw_texture_data->m_pixels, 255,
        wo_int(args + 0) * wo_int(args + 1) * jegl_texture::texture_format::RGBA);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture), nullptr,
        [](void* ptr) {
            delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}

WO_API wo_api wojeapi_texture_get_pixel(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    auto* pix = new jeecs::graphic::texture::pixel((*loaded_texture)->resouce(),
        wo_int(wo_struct_get(args + 1, 0)),
        wo_int(wo_struct_get(args + 1, 1)));

    return wo_ret_gchandle(vm, pix, args + 0, [](void* ptr)
        {
            delete (jeecs::graphic::texture::pixel*)ptr;
        });
}

WO_API wo_api wojeapi_texture_take_snapshot(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    auto tex_raw = loaded_texture->get()->resouce()->m_raw_texture_data;
    if (tex_raw->m_pixels)
    {
        auto memsz = tex_raw->m_width * tex_raw->m_height *
            (tex_raw->m_format & jegl_texture::texture_format::COLOR_DEPTH_MASK);
        if (memsz > 0)
        {
            auto* membuf = malloc(memsz);
            memcpy(membuf, tex_raw->m_pixels, memsz);
            return wo_ret_ok_gchandle(vm, membuf, nullptr, [](void* buf) {free(buf); });
        }
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_texture_restore_snapshot(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    auto* texture_buf = wo_pointer(args + 1);

    auto tex_raw = loaded_texture->get()->resouce()->m_raw_texture_data;
    if (tex_raw->m_pixels)
    {
        auto memsz = tex_raw->m_width * tex_raw->m_height *
            (tex_raw->m_format & jegl_texture::texture_format::COLOR_DEPTH_MASK);
        memcpy(tex_raw->m_pixels, texture_buf, memsz);

        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api wojeapi_texture_pixel_color(wo_vm vm, wo_value args, size_t argc)
{
    auto* pix = (jeecs::graphic::texture::pixel*)wo_pointer(args + 0);
    auto color = pix->get();

    wo_value result = wo_push_struct(vm, 4);
    wo_set_float(wo_struct_get(result, 0), color.x);
    wo_set_float(wo_struct_get(result, 1), color.y);
    wo_set_float(wo_struct_get(result, 2), color.z);
    wo_set_float(wo_struct_get(result, 3), color.w);

    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_texture_set_pixel_color(wo_vm vm, wo_value args, size_t argc)
{
    auto* pix = (jeecs::graphic::texture::pixel*)wo_pointer(args + 0);
    auto color = jeecs::math::vec4();

    color.x = wo_float(wo_struct_get(args + 1, 0));
    color.y = wo_float(wo_struct_get(args + 1, 1));
    color.z = wo_float(wo_struct_get(args + 1, 2));
    color.w = wo_float(wo_struct_get(args + 1, 3));

    pix->set(color);

    return wo_ret_void(vm);
}
/////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_font_open(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_font = new jeecs::graphic::font(wo_string(args + 0), wo_int(args + 1));
    if (loaded_font->enabled())
    {
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::font>(loaded_font), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::font>*)ptr;
            });
    }
    delete loaded_font;
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_font_load_char(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_font = (jeecs::basic::resource<jeecs::graphic::font>*)wo_pointer(args + 0);
    assert(loaded_font->get()->enabled());

    return wo_ret_gchandle(vm, loaded_font->get()->get_character(wo_str_get_char(wo_string(args + 1), 0)), args + 0, nullptr);
}

WO_API wo_api wojeapi_font_string_texture(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_font = (jeecs::basic::resource<jeecs::graphic::font>*)wo_pointer(args + 0);
    auto* text_texture = new jeecs::basic::resource<jeecs::graphic::texture>(loaded_font->get()->text_texture(wo_string(args + 1)));

    return wo_ret_gchandle(vm, text_texture, nullptr, [](void* ptr) {
        delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}

WO_API wo_api wojeapi_character_get_texture(wo_vm vm, wo_value args, size_t argc)
{
    auto* charac = (jeecs::graphic::character*)wo_pointer(args + 0);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(charac->m_texture), nullptr,
        [](void* ptr) {
            delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}


/////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_shader_open(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_shader = jeecs::graphic::shader::load(wo_string(args + 0));

    if (loaded_shader != nullptr)
    {
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::shader>(loaded_shader), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
            });
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_shader_create(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_shader = jeecs::graphic::shader::create(wo_string(args + 0), wo_string(args + 1));
    if (loaded_shader != nullptr)
    {
        return wo_ret_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::shader>(loaded_shader), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
            });
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_textures_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_map = wo_push_map(vm);

    if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
    {
        wo_value key = wo_push_empty(vm);

        for (auto& texture : textures->textures)
        {
            if (!texture.m_texture)
                return wo_ret_panic(vm, "Texture cannot be nullptr.");
            wo_set_int(key, (wo_integer_t)texture.m_pass_id);
            wo_set_gchandle(wo_map_set(out_map, key, nullptr), vm,
                new jeecs::basic::resource<jeecs::graphic::texture>(texture.m_texture), nullptr,
                [](void* ptr) {
                    delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
                });
        }
        wo_pop_stack(vm);
    }

    return wo_ret_val(vm, out_map);
}

WO_API wo_api wojeapi_bind_texture_for_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
        textures->bind_texture(wo_int(args + 1), *(jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 2));

    // TODO: 如果当前实体不包含jeecs::Renderer::Textures组件，在此panic?

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_shaders_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_array = wo_push_arr(vm, 0);

    if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
    {
        for (auto& shader : shaders->shaders)
        {
            if (!shader)
                return wo_ret_halt(vm, "Shader cannot be nullptr.");
            wo_set_gchandle(wo_arr_add(out_array, nullptr), vm,
                new jeecs::basic::resource<jeecs::graphic::shader>(shader), nullptr,
                [](void* ptr) {
                    delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
                });
        }
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Shaders组件，在此panic?
    return wo_ret_val(vm, out_array);
}

WO_API wo_api wojeapi_reload_shader_of_entity(wo_vm vm, wo_value args, size_t argc);

WO_API wo_api wojeapi_set_shaders_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value shader_array = args + 1;

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

    // TODO: 如果当前实体不包含jeecs::Renderer::Shaders组件，在此panic?

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_uniforms_from_shader(wo_vm vm, wo_value args, size_t argc)
{
    /*
    extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
                func _get_uniforms_from_shader(
                    shad: shader
                )=> map<string, (typeinfo, uniform_value_data)>;
    */
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    wo_value out_map = wo_push_map(vm);

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
        case jegl_shader::uniform_type::TEXTURE:
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

    return wo_ret_val(vm, out_map);

    if (auto str = (*shader)->resouce()->m_path)
        return wo_ret_string(vm, str);
    return wo_ret_string(vm, "< Built-in shader >");
}

WO_API wo_api wojeapi_set_uniforms_int(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), wo_float(args + 2));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float2(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1),
        jeecs::math::vec2(wo_float(args + 2), wo_float(args + 3)));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float3(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1),
        jeecs::math::vec3(wo_float(args + 2), wo_float(args + 3), wo_float(args + 4)));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float4(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1),
        jeecs::math::vec4(wo_float(args + 2), wo_float(args + 3), wo_float(args + 4), wo_float(args + 5)));

    return wo_ret_void(vm);
}

// defined in 'jeecs_core_editor_system.hpp'
WO_API wo_api wojeapi_store_bad_shader_name(wo_vm vm, wo_value args, size_t argc);
WO_API wo_api wojeapi_store_bad_shader_uniforms_int(wo_vm vm, wo_value args, size_t argc);
WO_API wo_api wojeapi_store_bad_shader_uniforms_float(wo_vm vm, wo_value args, size_t argc);
WO_API wo_api wojeapi_store_bad_shader_uniforms_float2(wo_vm vm, wo_value args, size_t argc);
WO_API wo_api wojeapi_store_bad_shader_uniforms_float3(wo_vm vm, wo_value args, size_t argc);
WO_API wo_api wojeapi_store_bad_shader_uniforms_float4(wo_vm vm, wo_value args, size_t argc);

WO_API wo_api wojeapi_shader_path(wo_vm vm, wo_value args, size_t argc)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);

    if (auto str = (*shader)->resouce()->m_path)
        return wo_ret_string(vm, str);
    return wo_ret_string(vm, "< Built-in shader >");
}

WO_API wo_api wojeapi_texture_get_size(wo_vm vm, wo_value args, size_t argc)
{
    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    auto sz = texture->get()->size();

    wo_value result = wo_push_struct(vm, 2);
    wo_set_int(wo_struct_get(result, 0), (wo_int_t)sz.x);
    wo_set_int(wo_struct_get(result, 1), (wo_int_t)sz.y);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_texture_path(wo_vm vm, wo_value args, size_t argc)
{
    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    if (auto str = (*texture)->resouce()->m_path)
        return wo_ret_option_string(vm, str);
    return wo_ret_option_none(vm);
}

const char* jeecs_woolang_api_path = "je.wo";
const char* jeecs_woolang_api_src = R"(
import woo.std;
namespace je
{
    extern("libjoyecs", "wojeapi_generate_uid")
        public func uid()=> string;

    namespace editor
    {
        extern("libjoyecs", "wojeapi_build_version")
        public func build_version()=> string;
    
        extern("libjoyecs", "wojeapi_woolang_version")
        public func woolang_version()=> string;

        extern("libjoyecs", "wojeapi_crc64_file")
        public func crc64file(file_path: string)=> option<int>;

        extern("libjoyecs", "wojeapi_crc64_string")
        public func crc64str(file_path: string)=> int;

        public enum loglevel
        {
            NORMAL = 0,
            INFO,
            WARNING,
            ERROR,
            FATAL,
        }

        extern("libjoyecs", "wojeapi_register_log_callback")
        public func hooklog()=> handle;

        extern("libjoyecs", "wojeapi_unregister_log_callback")
        public func unhooklog(i: handle)=> void;

        extern("libjoyecs", "wojeapi_get_all_logs")
        public func getlogs()=> array<(loglevel, string)>;

        extern("libjoyecs", "wojeapi_restart_graphic_interface")
        public func restart_graphic_interface(
            u: universe,
            title: string, 
            reso_x: int, 
            reso_y: int,
            fps:    int,
            fullscreen: bool)=> void;

        extern("libjoyecs", "wojeapi_current_platform_config")
        public func platform()=> string;

        extern("libjoyecs", "wojeapi_load_module")
        public func load_module(name: string, path: string)=> option<handle>;

        extern("libjoyecs", "wojeapi_unload_module")
        public func unload_module(module: handle)=> void;

        extern("libjoyecs", "wojeapi_update_editor_mouse_pos")
        public func update_editor_mouse_pos(x: real, y: real, lockposx: int, lockposy: int)=> void;

        extern("libjoyecs", "wojeapi_setable_editor_system")
        public func enable_editor_system(able: bool)=> void;

        extern("libjoyecs", "wojeapi_apply_camera_framebuf_setting")
        public func apply_camera_framebuf_setting(camera: entity, width: int, height: int)=> void;

        extern("libjoyecs", "wojeapi_get_framebuf_texture")
        public func get_framebuf_texture(camera: entity, index: int)=> option<graphic::texture>;

        public enum texture_sampling
        {
            MIN_LINEAR = 0x0000,
            MIN_NEAREST = 0x0001,
            MIN_NEAREST_NEAREST_MIP = 0x0002,
            MIN_LINEAR_NEAREST_MIP = 0x0003,
            MIN_NEAREST_LINEAR_MIP = 0x0004,
            MIN_LINEAR_LINEAR_MIP = 0x0005,

            MAG_LINEAR = 0x0000,
            MAG_NEAREST = 0x0010,

            CLAMP_EDGE_X = 0x0000,
            REPEAT_X = 0x0100,
            CLAMP_EDGE_Y = 0x0000,
            REPEAT_Y = 0x1000,

            FILTER_METHOD_MASK = 0x00FF,
            MIN_FILTER_MASK = 0x000F,
            MAG_FILTER_MASK = 0x00F0,

            WRAP_METHOD_MASK = 0xFF00,
            WRAP_X_METHOD_MASK = 0x0F00,
            WRAP_Y_METHOD_MASK = 0xF000,

            // LINEAR = MIN_LINEAR | MAG_LINEAR,
            // NEAREST = MIN_NEAREST | MAG_NEAREST,
            // CLAMP_EDGE = CLAMP_EDGE_X | CLAMP_EDGE_Y,
            // REPEAT = REPEAT_X | REPEAT_Y,

            // DEFAULT = LINEAR | CLAMP_EDGE,
        };

        extern("libjoyecs", "wojeapi_get_texture_sampling_method_by_path")
        public func get_texture_sampling_method_by_path(path: string)=> texture_sampling;

        extern("libjoyecs", "wojeapi_update_texture_sampling_method_by_path")
        public func update_texture_sampling_method_by_path(path: string, method: texture_sampling)=> result<void, string>;
    }

    extern("libjoyecs", "wojeapi_log")
    public func log(...)=> void;
    extern("libjoyecs", "wojeapi_loginfo")
    public func loginfo(...)=> void;
    extern("libjoyecs", "wojeapi_logerr")
    public func logerr(...)=> void;
    extern("libjoyecs", "wojeapi_logwarn")
    public func logwarn(...)=> void;
    extern("libjoyecs", "wojeapi_logfatal")
    public func logfatal(...)=> void;

    public enum keycode
    {
        A = 65 , B, C, D, E, F, G, H, I, J, K, L,
        M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        _1 = 49 , _2, _3, _4, _5, _6, _7, _8, _9,
        _0, _ = 32,

        L_SHIFT = 256,
        L_CTRL,
        L_ALT,
        TAB, ENTER, ESC, BACKSPACE,

        MOUSE_L_BUTTION = 512,
        MOUSE_M_BUTTION,
        MOUSE_R_BUTTION,

        MAX_KEY_CODE = 1024,
    };

    namespace input
    {
        extern("libjoyecs", "wojeapi_wheel_count")
        public func wheel()=> real;

        extern("libjoyecs", "wojeapi_input_keydown")
        public func keydown(kcode: keycode)=> bool;

        public func set_window_size(x: int, y: int)
        {
            extern("libjoyecs", "wojeapi_input_update_window_size")
            func _windowsize(width: int, height: int)=> void;

            _windowsize(x, y);
        }  

        extern("libjoyecs", "wojeapi_input_window_size")
        public func window_size()=> (int, int);
    }

    public using typeinfo = handle;
    namespace typeinfo
    {
        extern("libjoyecs", "wojeapi_type_is_component")
        public func is_component(self: typeinfo)=> bool;

        extern("libjoyecs", "wojeapi_type_is_system")
        public func is_system(self: typeinfo)=> bool;

        namespace editor
        {
            public func get_all_registed_types()
            {
                extern("libjoyecs", "wojeapi_get_all_registed_types")
                func _get_all_registed_types()=> array<typeinfo>;

                return _get_all_registed_types();
            }

            public func get_all_components_types()
            {
                return get_all_registed_types()
                    ->forall(\type: typeinfo = type->is_component(););
            }

            public func get_all_systems_types()
            {
                return get_all_registed_types()
                    ->forall(\type: typeinfo = type->is_system(););
            }
        }
        extern("libjoyecs", "wojeapi_type_of")
        public func load(name: string)=> option<typeinfo>;

        extern("libjoyecs", "wojeapi_type_of")
        public func loadid(id: int)=> option<typeinfo>;

        extern("libjoyecs", "wojeapi_type_id")
        public func id(self: typeinfo)=> int;

        extern("libjoyecs", "wojeapi_type_name")
        public func name(self: typeinfo)=> string;

        enum basic_type
        {
            INT, BOOL, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT, TEXTURE
        }
        extern("libjoyecs", "wojeapi_type_basic_type")
        private public func create(tid: basic_type)=> typeinfo;

        public let int = typeinfo::create(basic_type::INT);
        public let bool = typeinfo::create(basic_type::BOOL);
        public let float = typeinfo::create(basic_type::FLOAT);
        public let float2 = typeinfo::create(basic_type::FLOAT2);
        public let float3 = typeinfo::create(basic_type::FLOAT3);
        public let float4 = typeinfo::create(basic_type::FLOAT4);
        public let quat = typeinfo::create(basic_type::QUAT);
        public let string = typeinfo::create(basic_type::STRING);

        public let texture = typeinfo::create(basic_type::TEXTURE);
    }

    namespace graphic
    {
        public using texture = gchandle
        {
            extern("libjoyecs", "wojeapi_texture_open")
            public func load(path: string)=> option<texture>;

            extern("libjoyecs", "wojeapi_texture_create")
            public func create(width: int, height: int)=> texture;

            extern("libjoyecs", "wojeapi_texture_path")
            public func path(self: texture)=> option<string>;

            extern("libjoyecs", "wojeapi_texture_get_size")
            public func size(self: texture)=> (int, int);

            extern("libjoyecs", "wojeapi_texture_get_pixel")
            public func pix(self: texture, pos: (int, int))=> pixel;

            public using pixel = gchandle;
            namespace pixel
            {
                extern("libjoyecs", "wojeapi_texture_set_pixel_color")
                public func set_color(self: pixel, rgba: (real,real,real,real))=> void;

                extern("libjoyecs", "wojeapi_texture_pixel_color")
                public func get_color(self: pixel)=> (real, real, real, real);
            }

            namespace editor
            {
                public using snapshot_t = gchandle;

                extern("libjoyecs", "wojeapi_texture_take_snapshot")
                public func snapshot(self: texture)=> option<snapshot_t>;

                extern("libjoyecs", "wojeapi_texture_restore_snapshot")
                public func restore_snapshot(self: texture, snapshot: snapshot_t)=> bool;
            }
        }

        public using character = gchandle
        {
            extern("libjoyecs", "wojeapi_character_get_texture")
            public func get_texture(self: character)=> texture;
        }

        public using font = gchandle
        {
            extern("libjoyecs", "wojeapi_font_open")
            public func load(path: string, font_width: int)=> option<font>;

            extern("libjoyecs", "wojeapi_font_load_char")
            public func load_char(self: font, ch: string)=> character;

            extern("libjoyecs", "wojeapi_font_string_texture")
            public func load_string(self: font, str: string)=> texture;
        }

        public using shader = gchandle
        {
            extern("libjoyecs", "wojeapi_shader_open")
            public func load(path: string)=> option<shader>;
            
            extern("libjoyecs", "wojeapi_shader_create")
            public func create(vpath: string, src: string)=> option<shader>;

            extern("libjoyecs", "wojeapi_shader_path")
            public func path(self: shader)=> string;

            public union uniform_variable
            {
                integer(int),
                float(real),
                float2((real, real)),
                float3((real, real, real)),
                float4((real, real, real, real)),
                texture(int),
                others,
            }

            public using uniform_value_data = struct {
                n : int,
                x : real,
                y : real,
                z : real,
                w : real
            };

            public func get_uniforms(self: shader)=> map<string, uniform_variable>
            {
                extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
                func _get_uniforms_from_shader(shad: shader)=> map<string, (typeinfo, uniform_value_data)>;

                return
                    _get_uniforms_from_shader(self)
                        ->map(\name: string, tv: (typeinfo, uniform_value_data) = (name, 
                            type == typeinfo::int
                            ? uniform_variable::integer(val.n)
                            | type == typeinfo::float
                                ? uniform_variable::float(val.x)
                                | type == typeinfo::float2
                                    ? uniform_variable::float2((val.x, val.y))
                                    | type == typeinfo::float3
                                        ? uniform_variable::float3((val.x, val.y, val.z))
                                        | type == typeinfo::float4
                                            ? uniform_variable::float4((val.x, val.y, val.z, val.w))
                                            | type == typeinfo::texture
                                                ? uniform_variable::texture(val.n)
                                                | uniform_variable::others
                            )
                            where (type, val) = tv;
                        );
            }
            public func set_uniform<T>(self: shader, name: string, val: T)
                where std::declval:<T>() is int
                   || std::declval:<T>() is real
                   || std::declval:<T>() is (real, real)
                   || std::declval:<T>() is (real, real, real)
                   || std::declval:<T>() is (real, real, real, real);
            {
                if (std::declval:<T>() is int)
                {
                    extern("libjoyecs", "wojeapi_set_uniforms_int")
                    func _set_uniform_int(shad: shader, name: string, val: int)=> void;

                    _set_uniform_int(self, name, val);
                }
                else if (std::declval:<T>() is real)
                {
                    extern("libjoyecs", "wojeapi_set_uniforms_float")
                    func _set_uniform_float(shad: shader, name: string, val: real)=> void;

                    _set_uniform_float(self, name, val);
                }
                else if (std::declval:<T>() is (real, real))
                {
                    extern("libjoyecs", "wojeapi_set_uniforms_float2")
                    func _set_uniform_float2(shad: shader, name: string, x: real, y: real)=> void;
                    let (x, y) = val;
                    _set_uniform_float2(self, name, x, y);
                }
                else if (std::declval:<T>() is (real, real, real))
                {
                    extern("libjoyecs", "wojeapi_set_uniforms_float3")
                    func _set_uniform_float3(shad: shader, name: string, x: real, y: real, z: real)=> void;
                    let (x, y, z) = val;
                    _set_uniform_float3(self, name, x, y, z);
                }
                else if (std::declval:<T>() is (real, real, real, real))
                {
                    extern("libjoyecs", "wojeapi_set_uniforms_float4")
                    func _set_uniform_float4(shad: shader, name: string, x: real, y: real, z: real, w: real)=> void;
                    let (x, y, z, w) = val;
                    _set_uniform_float4(self, name, x, y, z, w);
                }
                else
                    std::panic("Here should not been exec.");
            }
        }
    }

    public using universe = gchandle;
    namespace universe
    {
        extern("libjoyecs", "wojeapi_create_universe")
        public func create()=> universe;

        public func close(self: universe)
        {
            return self: gchandle->close();
        }

        extern("libjoyecs", "wojeapi_stop_universe")
        public func stop(self: universe)=> void;

        extern("libjoyecs", "wojeapi_wait_universe")
        public func wait(self: universe)=> void;

        namespace editor
        {
            extern("libjoyecs", "wojeapi_get_all_worlds_in_universe")
            public func get_all_worlds(self: universe)=> array<world>;
        }
    }

    public using world = handle;
    namespace world
    {
        extern("libjoyecs", "wojeapi_create_world_in_universe")
        public func create(self: universe)=> world;

        extern("libjoyecs", "wojeapi_close_world")
        public func close(self: world) => void;

        extern("libjoyecs", "wojeapi_add_system_to_world")
        public func add_system(self: world, systype: typeinfo)=> bool;
)"
R"(
        extern("libjoyecs", "wojeapi_get_rendering_world")
        public func rend(u: universe)=> option<world>;

        extern("libjoyecs", "wojeapi_add_entity_to_world_with_components")
        public func add_entity(self: world, components: array<typeinfo>)=> entity;

        extern("libjoyecs", "wojeapi_remove_system_from_world")
        public func remove_system(self: world, sysinfo: typeinfo)=> void;
    
        namespace editor
        {
            extern("libjoyecs", "wojeapi_get_universe_from_world")
            public func located_universe(self: world)=> universe;

            extern("libjoyecs", "wojeapi_get_system_from_world")
            public func get_system(self: world, systype: typeinfo)=> option<handle>;

            extern("libjoyecs", "wojeapi_get_world_name")
            public func name(self: world)=> string;

            extern("libjoyecs", "wojeapi_set_world_name")
            public func set_name(self: world, _name: string)=> void;

            public func get_all_entities(self: world)=> array<entity>
            {
                extern("libjoyecs", "wojeapi_get_all_entities_from_world")
                public func _get_all_entities_from_world(world: world)=> array<entity>;

                return _get_all_entities_from_world(self);
            }

            public func get_systems_types(self: world)=> array<typeinfo>
            {
                extern("libjoyecs", "wojeapi_get_all_systems_from_world")
                    private public func _get_systems_from_world(self: world)=> array<typeinfo>;
                return _get_systems_from_world(self);
            }

            public func top_entity_iter(self: world)=> entity::editor::entity_iter
            {
                return entity::editor::entity_iter::create(self->get_all_entities());
            }
        }
    }
    public using entity = gchandle;
    namespace entity
    {
        public func operator == (a: entity, b: entity)
        {
            return a->editor::chunkinfo() == b->editor::chunkinfo();
        }
        public func operator != (a: entity, b: entity)
        {
            return a->editor::chunkinfo() != b->editor::chunkinfo();
        }

        public func close(self: entity)
        {
            extern("libjoyecs", "wojeapi_close_entity")
            public func _close(self: entity)=> void;

            _close(self);
        }

        namespace editor
        {
            extern("libjoyecs", "wojeapi_store_bad_shader_name")
                public func store_name_for_bad_shader_update(e: entity, shad_path: string)=> void;

            public func store_uniform_dat_for_bad_shader_update<T>(self: entity, shad_path: string, name: string, val: T)
                where std::declval:<T>() is int
                       || std::declval:<T>() is real
                       || std::declval:<T>() is (real, real)
                       || std::declval:<T>() is (real, real, real)
                       || std::declval:<T>() is (real, real, real, real);
            {
                if (std::declval:<T>() is int)
                {
                    extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_int")
                    func _set_uniform_int(e: entity, shad_path: string, name: string, val: int)=> void;

                    _set_uniform_int(self, shad_path, name, val);
                }
                else if (std::declval:<T>() is real)
                {
                    extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float")
                    func _set_uniform_float(e: entity, shad_path: string, name: string, val: real)=> void;

                    _set_uniform_float(self, shad_path, name, val);
                }
                else if (std::declval:<T>() is (real, real))
                {
                    extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float2")
                    func _set_uniform_float2(e: entity, shad_path: string, name: string, x: real, y: real)=> void;
                    let (x, y) = val;
                    _set_uniform_float2(self, shad_path, name, x, y);
                }
                else if (std::declval:<T>() is (real, real, real))
                {
                    extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float3")
                    func _set_uniform_float3(e: entity, shad_path: string, name: string, x: real, y: real, z: real)=> void;
                    let (x, y, z) = val;
                    _set_uniform_float3(self, shad_path, name, x, y, z);
                }
                else if (std::declval:<T>() is (real, real, real, real))
                {
                    extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float4")
                    func _set_uniform_float4(e: entity, shad_path: string, name: string, x: real, y: real, z: real, w: real)=> void;
                    let (x, y, z, w) = val;
                    _set_uniform_float4(self, shad_path, name, x, y, z, w);
                }
                else
                    std::panic("Here should not been exec.");
            }

            public using euid_t = string;

            public func set_editing_uid(self: option<euid_t>)
            {
                extern("libjoyecs", "wojeapi_set_editing_entity")
                public func _set_editing_entity(euid: euid_t)=> void;
                extern("libjoyecs", "wojeapi_set_editing_entity")
                public func _reset_editing_entity()=> void;

                match(self)
                {
                value(e)? _set_editing_entity(e);
                none? _reset_editing_entity();
                }
            }

            extern("libjoyecs", "wojeapi_get_editing_entity")
            public func get_editing_uid()=> option<euid_t>;

            extern("libjoyecs", "wojeapi_set_parent")
            public func set_parent(self: entity, parent: entity, force: bool)=> bool;

            extern("libjoyecs", "wojeapi_set_parent_with_uid")
            public func set_parent_with_uid(self: entity, parent_uid: string, force: bool)=> bool;

            extern("libjoyecs", "wojeapi_get_entity_uid")
            public func get_uid(self: entity)=> option<string>;

            extern("libjoyecs", "wojeapi_get_parent_uid")
            public func get_parent_uid(self: entity)=> option<string>;

            extern("libjoyecs", "wojeapi_get_entity_name")
            public func name(self: entity)=> string;

            extern("libjoyecs", "wojeapi_set_entity_name")
            public func set_name(self: entity, name: string)=> void;

            extern("libjoyecs", "wojeapi_get_entity_chunk_info")
            public func chunkinfo(self: entity)=> string;

            extern("libjoyecs", "wojeapi_find_entity_with_chunk_info")
            public func find_entity_by_chunkinfo(chunkinfo: string)=> entity;

            public func get_components(self: entity)
            {
                return self->get_components_types()
                    // If current entity died, we can still get types from chunk, but 
                    // failed to get component instance. Check here.
                    =>> \tid = comp->has ? [(tid, comp->val)] | []
                        where comp = self->get_component(tid);
                    ->  mapping;
            }

            public func get_components_types(self: entity)=> array<typeinfo>
            {
                extern("libjoyecs", "wojeapi_get_all_components_types_from_entity")
                public func _get_components_types_from_entity(entity: entity)=> array<typeinfo>;

                return _get_components_types_from_entity(self);
            }

            public func get_component(self: entity, type: typeinfo)=> option<component>
            {
                extern("libjoyecs", "wojeapi_get_component_from_entity")
                func _get_component(self: entity, type: typeinfo)=> option<handle>;
    
                return _get_component(self, type)->map(\addr:handle = component{
                    addr = addr, type = type,
                };);
            };

            public func add_component(self: entity, type: typeinfo)=> component
            {
                extern("libjoyecs", "wojeapi_add_component_from_entity")
                func _add_component(self: entity, type: typeinfo)=> handle;
    
                return component{addr = _add_component(self, type), type = type,};
            };

            extern("libjoyecs", "wojeapi_remove_component_from_entity")
            public func remove_component(self: entity, type: typeinfo)=> void;

            extern("libjoyecs", "wojeapi_is_top_entity")
            public func is_top(self: entity)=> bool;

            extern("libjoyecs", "wojeapi_is_child_of_entity")
            public func is_child_of(self: entity, parent: entity)=> bool;

            public using entity_iter = struct {
                m_cur_iter          : array::iterator<entity>,
                m_judge_func        : (entity)=>bool,
                mut m_current_entity: option<entity>,

                m_all_entity_list   : array<entity>,
                m_not_top_entities  : vec<entity>,
                m_outed_entities    : vec<entity>,
            };
            namespace entity_iter
            {
                private public func create(entitys: array<entity>)
                {
                    let not_top_entities = []mut: vec<entity>;
                    return entity_iter{
                        m_cur_iter = entitys->iter(),
                        m_judge_func = func(e: entity)
                                       {
                                            let result = e->editor::is_top();
                                            if (!result)
                                                not_top_entities->add(e);
                                            return result;
                                       },
                        m_current_entity = mut option::none:<entity>,
                        m_all_entity_list = entitys,
                        m_not_top_entities = not_top_entities,
                        m_outed_entities = []mut: vec<entity>,
                    };
                }

                public func childs_iter(self: entity_iter)
                {
                    let parent_entity = self.m_current_entity->val();
                    return entity_iter{
                        m_cur_iter = self.m_all_entity_list->iter(),
                        m_judge_func = \e = e->is_child_of(parent_entity);,
                        m_current_entity = mut option::none:<entity>,
                        m_all_entity_list = self.m_all_entity_list,
                        m_not_top_entities = []mut: vec<entity>,
                        m_outed_entities = self.m_outed_entities,
                    };
                }
    
                public func iter(self: entity_iter)
                {
                    return self;
                }
                public func next(self: entity_iter)=> option<(entity)>
                {
                    let current_iter = self.m_cur_iter;

                    for (let _, out_entity : current_iter)
                    {
                        if (self.m_judge_func(out_entity))
                        {
                            self.m_current_entity = option::value(out_entity);
                            self.m_outed_entities->add(out_entity);

                            return option::value((out_entity,));
                        }
                    }

                continue_find_not_displayed_entity@
                    while (!self.m_not_top_entities->empty())
                    {
                        let top = self.m_not_top_entities[0];
                        self.m_not_top_entities->remove(0);

                        if (self.m_outed_entities->find(top) == -1)
                        {
                            for (let _, entity: self.m_all_entity_list)
                            {
                                if (top != entity && top->is_child_of(entity))
                                    // Parent finded, it's not a orphan entity.
                                    continue continue_find_not_displayed_entity;
                            }

                            // Current entity have jeecs::Transform::LocalToParent,
                            // but it's LocalToParent donot point to any other entity;
                            self.m_current_entity = option::value(top);
                            self.m_outed_entities->add(top);

                            return option::value((top,));
                        }
                    }
                    return option::none;
                }
            } // end of namespace entity_iter
)"
R"(
            namespace graphic
            {
                // Used for reload specify shader; called when shader updated or moved;
                // RETURN TRUE MEANS OK
                extern("libjoyecs", "wojeapi_reload_shader_of_entity")
                public func try_reload_shaders(self: entity, old_shad: string, new_shad: string)=> bool;

                public func get_shaders(self: entity)=> array<graphic::shader>
                {
                    extern("libjoyecs", "wojeapi_shaders_of_entity")
                    public func get_shaders_from_entity(e: entity)=> array<graphic::shader>;

                    return get_shaders_from_entity(self);
                }

                extern("libjoyecs", "wojeapi_set_shaders_of_entity")
                public func set_shaders(self: entity, shaders: array<graphic::shader>)=> void;

                public func get_textures(self: entity)=> dict<int, graphic::texture>
                {
                    extern("libjoyecs", "wojeapi_textures_of_entity")
                    public func get_textures_from_entity(e: entity)=> dict<int, graphic::texture>;

                    return get_textures_from_entity(self);
                }

                extern("libjoyecs", "wojeapi_bind_texture_for_entity")
                public func bind_texture(self: entity, id: int, tex: graphic::texture)=> void;
            }
        }// end of namespace editor
    } // end of namespace entity

    public using native_value = handle;
    namespace native_value
    {
        extern("libjoyecs", "wojeapi_native_value_bool")
        public func bool(self: native_value)=> bool;

        extern("libjoyecs", "wojeapi_native_value_int")
        public func int(self: native_value)=> int;

        extern("libjoyecs", "wojeapi_native_value_float")
        public func float(self: native_value)=> real;

        extern("libjoyecs", "wojeapi_native_value_float2")
        public func float2(self: native_value)=> (real, real);

        extern("libjoyecs", "wojeapi_native_value_float3")
        public func float3(self: native_value)=> (real, real, real);

        extern("libjoyecs", "wojeapi_native_value_float4")
        public func float4(self: native_value)=> (real, real, real, real);

        extern("libjoyecs", "wojeapi_native_value_rot_euler3")
        public func euler3(self: native_value)=> (real, real, real);

        extern("libjoyecs", "wojeapi_native_value_je_string")
        public func string(self: native_value)=> string;

        extern("libjoyecs", "wojeapi_native_value_je_to_string")
        public func to_string(self: native_value, types: typeinfo)=> string; 

        extern("libjoyecs", "wojeapi_native_value_je_parse")
        public func parse(self: native_value, types: typeinfo, str: string)=> void; 

        
        extern("libjoyecs", "wojeapi_native_value_set_bool")
        public func set_bool(self: native_value, value: bool)=> void;

        extern("libjoyecs", "wojeapi_native_value_set_int")
        public func set_int(self: native_value, value: int)=> void;

        extern("libjoyecs", "wojeapi_native_value_set_float")
        public func set_float(self: native_value, value: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_set_float2")
        public func set_float2(self: native_value, x: real, y: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_set_float3")
        public func set_float3(self: native_value, x: real, y: real, z: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_set_float4")
        public func set_float4(self: native_value, x: real, y: real, z: real, w: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_set_rot_euler3")
        public func set_euler3(self: native_value, x: real, y: real, z: real)=> void;

        extern("libjoyecs", "wojeapi_native_value_set_je_string")
        public func set_string(self: native_value, val: string)=> void;
    }

    public using component = struct{addr: handle, type: typeinfo}
    {
        namespace editor
        {
            public func get_member(self: component, name: string)
            {
                extern("libjoyecs", "wojeapi_get_components_member")
                func _get_member(addr: handle, type: typeinfo, name: string)=> option<(typeinfo, native_value)>;

                return _get_member(self.addr, self.type, name);
            }

            public using member_iterator = gchandle;
            namespace member_iterator
            {
                public func iter(self: member_iterator)
                {
                    return self;
                }

                extern("libjoyecs", "wojeapi_member_iterator_next")
                public func next(self: member_iterator)=> option<(string, typeinfo, native_value)>;
            }

            func iter_member(self: component)
            {
                extern("libjoyecs", "wojeapi_iter_components_member")
                func _iter_member(self: handle, type: typeinfo) => member_iterator;

                return _iter_member(self.addr, self.type);
            }
            
            public func members(self: component)
            {
                let result = {}mut: map<string, (typeinfo, native_value)>;
                for (let name, type, addr : self->iter_member())
                    result->set(name, (type, addr));

                return result->unsafe::cast:<dict<string, (typeinfo, native_value)>>; 
            }
        }
    }
}

)";

