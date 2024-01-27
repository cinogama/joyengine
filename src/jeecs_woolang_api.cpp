#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <list>

std::atomic_flag log_buffer_mx = {};
std::list<std::pair<int, std::string>> log_buffer;

WO_API wo_api wojeapi_startup_thread(wo_vm vm, wo_value args, size_t argc)
{
    wo_value arguments = args + 1;
    auto argument_count = wo_lengthof(arguments);

    wo_vm co_vmm = wo_borrow_vm(vm);
    wo_value cofunc = wo_push_val(co_vmm, args + 0);

    for (auto i = argument_count; i > 0; --i)
    {
        wo_struct_get(wo_push_empty(co_vmm), arguments, (uint16_t)(i - 1));
    }

    std::thread t([=]
        {
            wo_invoke_value(co_vmm, cofunc, argument_count);
            wo_release_vm(co_vmm);
        });
    t.detach();

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_generate_uid(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, jeecs::typing::uid_t::generate().to_string().c_str());
}

WO_API wo_api wojeapi_build_version(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, je_build_version());
}

WO_API wo_api wojeapi_build_commit(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, je_build_commit());
}

WO_API wo_api wojeapi_get_sleep_suppression(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, je_clock_get_sleep_suppression());
}

WO_API wo_api wojeapi_read_file_all(wo_vm vm, wo_value args, size_t argc)
{
    if (auto* file = jeecs_file_open(wo_string(args + 0)))
    {
        std::vector<char> readed_buf(file->m_file_length);
        auto readed_len = jeecs_file_read(readed_buf.data(), sizeof(char), file->m_file_length, file);
        readed_buf.resize(readed_len);

        jeecs_file_close(file);

        return wo_ret_option_buffer(vm, readed_buf.data(), readed_buf.size());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_mark_shared_glresource_outdated(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, jegl_mark_shared_resources_outdated(wo_string(args + 0)));
}

WO_API wo_api wojeapi_init_graphic_pipeline(wo_vm vm, wo_value args, size_t argc)
{
    jegl_uhost_get_or_create_for_universe(wo_pointer(args + 0), nullptr);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_set_runtime_path(wo_vm vm, wo_value args, size_t argc)
{
    jeecs_file_set_runtime_path(wo_string(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_create_fimg_packer(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_pointer(vm, jeecs_file_image_begin(
        wo_string(args + 0),
        (size_t)wo_int(args + 1)
    ));
}

WO_API wo_api wojeapi_pack_file_to_fimg_packer(wo_vm vm, wo_value args, size_t argc)
{
    auto* ctx = (fimg_creating_context*)wo_pointer(args + 0);
    return wo_ret_bool(vm, jeecs_file_image_pack_file(ctx, wo_string(args + 1), wo_string(args + 2)));
}

WO_API wo_api wojeapi_pack_buffer_to_fimg_packer(wo_vm vm, wo_value args, size_t argc)
{
    auto* ctx = (fimg_creating_context*)wo_pointer(args + 0);
    return wo_ret_bool(vm, jeecs_file_image_pack_buffer(ctx, wo_pointer(args + 1), (size_t)wo_int(args + 2), wo_string(args + 3)));
}

WO_API wo_api wojeapi_finish_fimg_packer(wo_vm vm, wo_value args, size_t argc)
{
    auto* ctx = (fimg_creating_context*)wo_pointer(args + 0);
    jeecs_file_image_finish(ctx);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_set_sleep_suppression(wo_vm vm, wo_value args, size_t argc)
{
    je_clock_set_sleep_suppression(wo_real(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_woolang_version(wo_vm vm, wo_value args, size_t argc)
{
    std::string woolang_version_info = "Woolang ";
    return wo_ret_string(vm, (woolang_version_info + wo_version() + " " + wo_compile_date()).c_str());
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
    auto func = (std::function<void(int, const char*)>*) je_log_unregister_callback((size_t)wo_handle(args + 0));
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

    wo_value elem = wo_push_empty(vm);
    wo_value val = wo_push_empty(vm);
    for (auto& [i, s] : logs)
    {
        wo_set_struct(elem, vm, 2);
        wo_set_int(val, i);
        wo_struct_set(elem, 0, val);
        wo_set_string(val, vm, s.c_str());
        wo_struct_set(elem, 1, val);
        wo_arr_add(result, elem);
    }
    return wo_ret_val(vm, result);
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

WO_API wo_api wojeapi_delay_unload_module(wo_vm vm, wo_value args, size_t argc)
{
    je_module_delay_unload(wo_pointer(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_apply_camera_framebuf_setting(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (jeecs::Camera::RendToFramebuffer* rbf = entity->get_component<jeecs::Camera::RendToFramebuffer>())
    {
        rbf->framebuffer = jeecs::graphic::framebuffer::create(
            (size_t)wo_int(args + 1), (size_t)wo_int(args + 2), {
                jegl_texture::format::RGBA,
                jegl_texture::format::DEPTH,
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
            return wo_ret_option_none(vm);
        }
        auto tex = rbf->framebuffer->get_attachment((size_t)wo_int(args + 1));
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

WO_API wo_api wojeapi_universe_get_frame_deltatime(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, je_ecs_universe_get_frame_deltatime(wo_pointer(args + 0)));
}

WO_API wo_api wojeapi_universe_set_frame_deltatime(wo_vm vm, wo_value args, size_t argc)
{
    je_ecs_universe_set_frame_deltatime(wo_pointer(args + 0), wo_real(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_universe_get_max_deltatime(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, je_ecs_universe_get_max_deltatime(wo_pointer(args + 0)));
}
WO_API wo_api wojeapi_universe_set_max_deltatime(wo_vm vm, wo_value args, size_t argc)
{
    je_ecs_universe_set_max_deltatime(wo_pointer(args + 0), wo_real(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_universe_get_timescale(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, je_ecs_universe_get_time_scale(wo_pointer(args + 0)));
}
WO_API wo_api wojeapi_universe_set_timescale(wo_vm vm, wo_value args, size_t argc)
{
    je_ecs_universe_set_time_scale(wo_pointer(args + 0), wo_real(args + 1));
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
        wo_value elem = wo_push_empty(vm);

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

    wo_value elem = wo_push_empty(vm);

    auto* cur_type = types;
    while (*cur_type)
    {
        wo_set_pointer(elem, (void*)*(cur_type++));
        wo_arr_add(result, elem);
    }
    je_mem_free(types);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_add_entity_to_world_with_components(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    wo_value components_list = args + 1;

    std::vector<jeecs::typing::typeid_t> components;

    wo_value elem = wo_push_empty(vm);
    for (wo_integer_t i = 0; i < wo_lengthof(components_list); ++i)
    {
        wo_arr_get(elem, components_list, i);
        components.push_back(((const jeecs::typing::type_info*)wo_pointer(elem))->m_id);
    }

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld._add_entity(components)),
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_add_entity_to_world_with_prefab(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    jeecs::game_entity* prefab_entity = (jeecs::game_entity*)wo_pointer(args + 1);

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld.add_entity(*prefab_entity)),
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_add_prefab_to_world_with_components(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    wo_value components_list = args + 1;

    std::vector<jeecs::typing::typeid_t> components;

    wo_value elem = wo_push_empty(vm);
    for (wo_integer_t i = 0; i < wo_lengthof(components_list); ++i)
    {
        wo_arr_get(elem, components_list, i);
        components.push_back(((const jeecs::typing::type_info*)wo_pointer(elem))->m_id);
    }

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld._add_prefab(components)),
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_get_all_entities_from_world(wo_vm vm, wo_value args, size_t argc)
{
    wo_value out_arr = wo_push_arr(vm, 0);

    void* world_instance = wo_pointer(args + 0);

    if (je_ecs_world_is_valid(world_instance))
    {
        auto entities = jedbg_get_all_entities_in_world(world_instance);
        auto entity_iter = entities;

        wo_value elem = wo_push_empty(vm);

        while (*entity_iter)
        {
            wo_set_gchandle(elem, vm, *(entity_iter++), nullptr,
                [](void* entity_ptr) {
                    jedbg_free_entity((jeecs::game_entity*)entity_ptr);
                });
            wo_arr_add(out_arr, elem);
        }
        je_mem_free(entities);
    }

    return wo_ret_val(vm, out_arr);
}

// ECS ENTITY
WO_API wo_api wojeapi_close_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    entity->close();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_world_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    void* world = je_ecs_world_of_entity(entity);
    return wo_ret_pointer(vm, world);
}

WO_API wo_api wojeapi_set_editing_entity_uid(wo_vm vm, wo_value args, size_t argc)
{
    if (argc)
        jedbg_set_editing_entity_uid((jeecs::typing::euid_t)wo_handle(args + 0));
    else
        jedbg_set_editing_entity_uid(0);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_world_is_valid(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, je_ecs_world_is_valid(wo_pointer(args + 0)));
}
WO_API wo_api wojeapi_get_editing_entity_uid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::typing::euid_t uid = jedbg_get_editing_entity_uid();

    if (uid != 0)
        return wo_ret_option_handle(vm, (wo_handle_t)uid);
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_get_entity_uid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    return wo_ret_handle(vm, (wo_handle_t)entity->get_euid());
}

WO_API wo_api wojeapi_get_entity_anchor_uuid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* anc = entity->get_component<jeecs::Transform::Anchor>())
        return wo_ret_option_string(vm, anc->uid.to_string().c_str());

    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_get_parent_anchor_uid(wo_vm vm, wo_value args, size_t argc)
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
    return wo_ret_string(vm, je_ecs_get_name_of_entity(entity));
}

WO_API wo_api wojeapi_set_entity_name(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    je_ecs_set_name_of_entity(entity, wo_string(args + 1));
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
    wo_value elem = wo_push_empty(vm);
    while (*typeindex)
    {
        wo_set_pointer(elem, (void*)*(typeindex++));
        wo_arr_add(out_arr, elem);
    }
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

    return wo_ret_pointer(vm, je_ecs_world_entity_add_component(entity,
        (const jeecs::typing::type_info*)wo_pointer(args + 1)));
}

WO_API wo_api wojeapi_remove_component_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    je_ecs_world_entity_remove_component(entity, (const jeecs::typing::type_info*)wo_pointer(args + 1));
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

WO_API wo_api wojeapi_component_get_all_members(wo_vm vm, wo_value args, size_t argc)
{
    wo_value elem = wo_push_empty(vm);
    wo_struct_get(elem, args + 0, 0);

    void* component_addr = wo_pointer(elem);

    wo_struct_get(elem, args + 0, 1);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(elem);

    wo_value result = wo_push_arr(vm, 0);

    wo_value elem2 = wo_push_empty(vm);
    auto* member_type = component_type->m_member_types;
    while (member_type)
    {
        wo_set_struct(elem, vm, 3);

        wo_set_string(elem2, vm, member_type->m_member_name);
        wo_struct_set(elem, 0, elem2);
        wo_set_pointer(elem2, (void*)member_type->m_member_type);
        wo_struct_set(elem, 1, elem2);
        wo_set_handle(elem2, (wo_handle_t)(member_type->m_member_offset + (intptr_t)component_addr));
        wo_struct_set(elem, 2, elem2);

        wo_arr_add(result, elem);

        member_type = member_type->m_next_member;
    }
    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_get_components_member(wo_vm vm, wo_value args, size_t argc)
{
    wo_value elem = wo_push_empty(vm);
    wo_struct_get(elem, args + 0, 0);

    void* component_addr = wo_pointer(elem);

    wo_struct_get(elem, args + 0, 1);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(elem);

    wo_string_t member_name = wo_string(args + 1);

    if (auto* member_info = component_type->find_member_by_name(member_name))
    {
        wo_value result = wo_push_struct(vm, 2);
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
WO_API wo_api wojeapi_input_keydown(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, jeecs::input::keydown((jeecs::input::keycode)wo_int(args + 0)));
}

WO_API wo_api wojeapi_input_mousedown(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, jeecs::input::mousedown(
        (size_t)wo_int(args + 0), (jeecs::input::mousecode)wo_int(args + 1)));
}

WO_API wo_api wojeapi_wheel_count(wo_vm vm, wo_value args, size_t argc)
{
    auto wheel = jeecs::input::wheel((size_t)wo_int(args + 0));

    wo_value result = wo_push_struct(vm, 2);
    wo_value elem = wo_push_empty(vm);
    wo_set_float(elem, wheel.x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, wheel.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_window_size(wo_vm vm, wo_value args, size_t argc)
{
    auto winsz = jeecs::input::windowsize();

    wo_value result = wo_push_struct(vm, 2);
    wo_value elem = wo_push_empty(vm);
    wo_set_int(elem, (wo_int_t)winsz.x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)winsz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_mouse_pos(wo_vm vm, wo_value args, size_t argc)
{
    auto winsz = jeecs::input::mousepos((size_t)wo_int(args + 0));

    wo_value result = wo_push_struct(vm, 2);
    wo_value elem = wo_push_empty(vm);
    wo_set_int(elem, (wo_int_t)winsz.x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)winsz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_mouse_view_pos(wo_vm vm, wo_value args, size_t argc)
{
    auto winsz = jeecs::input::mouseviewpos((size_t)wo_int(args + 0));

    wo_value result = wo_push_struct(vm, 2);
    wo_value elem = wo_push_empty(vm);
    wo_set_float(elem, winsz.x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, winsz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_update_window_size(wo_vm vm, wo_value args, size_t argc)
{
    je_io_set_windowsize((int)wo_int(args + 0), (int)wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_input_update_window_title(wo_vm vm, wo_value args, size_t argc)
{
    je_io_set_windowtitle(wo_string(args + 0));
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

    wo_value elem = wo_push_empty(vm);

    auto** cur_type = types;
    while (*cur_type)
    {
        wo_set_pointer(elem, (void*)*(cur_type++));
        wo_arr_add(out_array, elem);
    }

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

WO_API wo_api wojeapi_type_members(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);

    wo_value result = wo_push_arr(vm, 0);
    wo_value elem = wo_push_empty(vm);
    wo_value elem2 = wo_push_empty(vm);

    auto* member_iter = type->m_member_types;
    while (member_iter != nullptr)
    {
        wo_set_struct(elem, vm, 2);

        wo_set_string(elem2, vm, member_iter->m_member_name);
        wo_struct_set(elem, 0, elem2);
        wo_set_pointer(elem2, (void*)member_iter->m_member_type);
        wo_struct_set(elem, 1, elem2);

        wo_arr_add(result, elem);

        member_iter = member_iter->m_next_member;
    }

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_type_basic_type(wo_vm vm, wo_value args, size_t argc)
{
    enum basic_type
    {
        INT, INT2, BOOL, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT,
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

WO_API wo_api wojeapi_native_value_int2(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::ivec2* value = (jeecs::math::ivec2*)wo_pointer(args + 0);

    wo_value result = wo_push_struct(vm, 2);
    wo_value elem = wo_push_empty(vm);
    wo_set_int(elem, value->x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, value->y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
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
    wo_value elem = wo_push_empty(vm);
    wo_set_float(elem, value->x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, value->y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_native_value_float3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec3* value = (jeecs::math::vec3*)wo_pointer(args + 0);

    wo_value result = wo_push_struct(vm, 3);
    wo_value elem = wo_push_empty(vm);
    wo_set_float(elem, value->x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, value->y);
    wo_struct_set(result, 1, elem);
    wo_set_float(elem, value->z);
    wo_struct_set(result, 2, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_native_value_float4(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec4* value = (jeecs::math::vec4*)wo_pointer(args + 0);

    wo_value result = wo_push_struct(vm, 4);
    wo_value elem = wo_push_empty(vm);
    wo_set_float(elem, value->x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, value->y);
    wo_struct_set(result, 1, elem);
    wo_set_float(elem, value->z);
    wo_struct_set(result, 2, elem);
    wo_set_float(elem, value->w);
    wo_struct_set(result, 3, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_native_value_je_string(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::string* value = (jeecs::basic::string*)wo_pointer(args + 0);
    return wo_ret_string(vm, value->c_str());
}

WO_API wo_api wojeapi_native_value_rot_euler3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::quat* value = (jeecs::math::quat*)wo_pointer(args + 0);
    auto&& euler_v3 = value->euler_angle();

    wo_value result = wo_push_struct(vm, 3);
    wo_value elem = wo_push_empty(vm);
    wo_set_float(elem, euler_v3.x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, euler_v3.y);
    wo_struct_set(result, 1, elem);
    wo_set_float(elem, euler_v3.z);
    wo_struct_set(result, 2, elem);

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

WO_API wo_api wojeapi_native_value_set_int2(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::ivec2* value = (jeecs::math::ivec2*)wo_pointer(args + 0);

    value->x = wo_int(args + 1);
    value->y = wo_int(args + 2);

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
    jeecs::basic::string* value = (jeecs::basic::string*)wo_pointer(args + 0);

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
    const jeecs::typing::type_info* value_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    return wo_ret_string(vm, jeecs::basic::make_cpp_string(value_type->m_to_string(native_val)).c_str());
}

WO_API wo_api wojeapi_native_value_je_parse(wo_vm vm, wo_value args, size_t argc)
{
    void* native_val = wo_pointer(args + 0);
    const jeecs::typing::type_info* value_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);
    wo_string_t str = wo_string(args + 2);

    value_type->m_parse(native_val, str);

    return wo_ret_void(vm);
}

///////////////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_texture_open(wo_vm vm, wo_value args, size_t argc)
{
    jegl_context* gcontext = nullptr;

    wo_value universe_ptr = wo_push_empty(vm);
    if (wo_option_get(universe_ptr, args + 0))
    {
        gcontext = jegl_uhost_get_context(
            jegl_uhost_get_or_create_for_universe(
                wo_pointer(universe_ptr), nullptr));
    }
    auto loaded_texture = jeecs::graphic::texture::load(gcontext, wo_string(args + 1));

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
    auto loaded_texture = jeecs::graphic::texture::create(
        (size_t)wo_int(args + 0), (size_t)wo_int(args + 1), jegl_texture::format::RGBA);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture), nullptr,
        [](void* ptr) {
            delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}

WO_API wo_api wojeapi_texture_bind_path(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    auto& path = (*loaded_texture)->resouce()->m_path;
    if (path != nullptr)
        je_mem_free((void*)path);
    path = jeecs::basic::make_new_string(wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_texture_get_pixel(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    wo_value elem = wo_push_empty(vm);
    wo_struct_get(elem, args + 1, 0);
    size_t x = (size_t)wo_int(elem);
    wo_struct_get(elem, args + 1, 1);
    size_t y = (size_t)wo_int(elem);
    auto* pix = new jeecs::graphic::texture::pixel((*loaded_texture)->resouce(), x, y);

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
            (tex_raw->m_format & jegl_texture::format::COLOR_DEPTH_MASK);
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
            (tex_raw->m_format & jegl_texture::format::COLOR_DEPTH_MASK);
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
    wo_value elem = wo_push_empty(vm);
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
WO_API wo_api wojeapi_texture_set_pixel_color(wo_vm vm, wo_value args, size_t argc)
{
    auto* pix = (jeecs::graphic::texture::pixel*)wo_pointer(args + 0);
    auto color = jeecs::math::vec4();

    wo_value elem = wo_push_empty(vm);
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
WO_API wo_api wojeapi_font_open(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_font = jeecs::graphic::font::load(wo_string(args + 0), (size_t)wo_int(args + 1));
    if (loaded_font != nullptr)
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
    assert(loaded_font != nullptr);

    auto ch = loaded_font->get()->get_character(wo_char(args + 1));
    assert(ch != nullptr);

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
    wo_value result = wo_push_struct(vm, 8);
    wo_value elem = wo_push_empty(vm);

    wo_set_gchandle(elem, vm, new jeecs::basic::resource<jeecs::graphic::texture>(ch->m_texture), nullptr,
        [](void* ptr) {
            delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
    wo_struct_set(result, 0, elem);

    wo_set_char(elem, ch->m_char);
    wo_struct_set(result, 1, elem);

    wo_set_int(elem, ch->m_width);
    wo_struct_set(result, 2, elem);
    wo_set_int(elem, ch->m_height);
    wo_struct_set(result, 3, elem);

    wo_set_int(elem, ch->m_advised_w);
    wo_struct_set(result, 4, elem);
    wo_set_int(elem, ch->m_advised_h);
    wo_struct_set(result, 5, elem);

    wo_set_int(elem, ch->m_baseline_offset_x);
    wo_struct_set(result, 6, elem);
    wo_set_int(elem, ch->m_baseline_offset_y);
    wo_struct_set(result, 7, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_font_string_texture(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_font = (jeecs::basic::resource<jeecs::graphic::font>*)wo_pointer(args + 0);
    auto* text_texture = new jeecs::basic::resource<jeecs::graphic::texture>(loaded_font->get()->text_texture(wo_string(args + 1)));

    return wo_ret_gchandle(vm, text_texture, nullptr, [](void* ptr) {
        delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}

/////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_shader_open(wo_vm vm, wo_value args, size_t argc)
{
    jegl_context* gcontext = nullptr;

    wo_value universe_ptr = wo_push_empty(vm);
    if (wo_option_get(universe_ptr, args + 0))
    {
        gcontext = jegl_uhost_get_context(
            jegl_uhost_get_or_create_for_universe(
                wo_pointer(universe_ptr), nullptr));
    }
    auto loaded_shader = jeecs::graphic::shader::load(gcontext, wo_string(args + 1));

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
    auto loaded_shader = jeecs::graphic::shader::create(wo_string(args + 0), wo_string(args + 1));
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
        wo_value val = wo_push_empty(vm);

        for (auto& texture : textures->textures)
        {
            if (!texture.m_texture)
                return wo_ret_panic(vm, "Texture cannot be nullptr.");
            wo_set_int(key, (wo_integer_t)texture.m_pass_id);
            wo_set_gchandle(val, vm,
                new jeecs::basic::resource<jeecs::graphic::texture>(texture.m_texture), nullptr,
                [](void* ptr) {
                    delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
                });
            wo_map_set(out_map, key, val);
        }
        wo_pop_stack(vm);
    }

    return wo_ret_val(vm, out_map);
}

WO_API wo_api wojeapi_bind_texture_for_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
        textures->bind_texture((size_t)wo_int(args + 1), *(jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 2));

    // TODO: 如果当前实体不包含jeecs::Renderer::Textures组件，在此panic?

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_shaders_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_array = wo_push_arr(vm, 0);

    if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
    {
        wo_value elem = wo_push_empty(vm);

        for (auto& shader : shaders->shaders)
        {
            if (!shader)
                return wo_ret_halt(vm, "Shader cannot be nullptr.");
            wo_set_gchandle(elem, vm,
                new jeecs::basic::resource<jeecs::graphic::shader>(shader), nullptr,
                [](void* ptr) {
                    delete (jeecs::basic::resource<jeecs::graphic::shader>*)ptr;
                });
            wo_arr_add(out_array, elem);
        }
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Shaders组件，在此panic?
    return wo_ret_val(vm, out_array);
}

WO_API wo_api wojeapi_reload_shader_of_entity(wo_vm vm, wo_value args, size_t argc);
WO_API wo_api wojeapi_reload_texture_of_entity(wo_vm vm, wo_value args, size_t argc);

WO_API wo_api wojeapi_set_shaders_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value shader_array = args + 1;

    if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
    {
        wo_value elem = wo_push_empty(vm);

        shaders->shaders.clear();
        size_t arrsize = (size_t)wo_lengthof(shader_array);
        for (size_t i = 0; i < arrsize; ++i)
        {
            wo_arr_get(elem, shader_array, i);
            jeecs::basic::resource<jeecs::graphic::shader>* shader =
                (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(elem);
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
                )=> map<string, uniform_variable>;
    */
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    wo_value out_map = wo_push_map(vm);

    auto* uniforms = (*shader)->resouce()->m_raw_shader_data->m_custom_uniforms;
    wo_value key = wo_push_empty(vm);
    wo_value val = wo_push_empty(vm);
    wo_value elem = wo_push_empty(vm);
    while (uniforms)
    {
        wo_set_string(key, vm, uniforms->m_name);
        wo_set_struct(val, vm, 2);
        wo_map_set(out_map, key, val);

        if (uniforms->m_uniform_type >= jegl_shader::uniform_type::INT
            && uniforms->m_uniform_type <= jegl_shader::uniform_type::TEXTURE)
            wo_set_int(elem, 1 + uniforms->m_uniform_type);
        else
            // Others
            wo_set_int(elem, 2 + jegl_shader::uniform_type::TEXTURE);

        wo_struct_set(val, 0, elem);

        switch (uniforms->m_uniform_type)
        {
        case jegl_shader::uniform_type::INT:
        case jegl_shader::uniform_type::TEXTURE:
            wo_set_int(elem, uniforms->n); break;
        case jegl_shader::uniform_type::FLOAT:
            wo_set_float(elem, uniforms->x); break;
        case jegl_shader::uniform_type::FLOAT2:
            wo_set_struct(elem, vm, 2);
            wo_set_float(key, uniforms->x);
            wo_struct_set(elem, 0, key);
            wo_set_float(key, uniforms->y);
            wo_struct_set(elem, 1, key);
            break;
        case jegl_shader::uniform_type::FLOAT3:
            wo_set_struct(elem, vm, 3);
            wo_set_float(key, uniforms->x);
            wo_struct_set(elem, 0, key);
            wo_set_float(key, uniforms->y);
            wo_struct_set(elem, 1, key);
            wo_set_float(key, uniforms->z);
            wo_struct_set(elem, 2, key);
            break;
        case jegl_shader::uniform_type::FLOAT4:
            wo_set_struct(elem, vm, 4);
            wo_set_float(key, uniforms->x);
            wo_struct_set(elem, 0, key);
            wo_set_float(key, uniforms->y);
            wo_struct_set(elem, 1, key);
            wo_set_float(key, uniforms->z);
            wo_struct_set(elem, 2, key);
            wo_set_float(key, uniforms->w);
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
WO_API wo_api wojeapi_get_bad_shader_list_of_entity(wo_vm vm, wo_value args, size_t argc);
WO_API wo_api wojeapi_store_bad_shader_name(wo_vm vm, wo_value args, size_t argc);
WO_API wo_api wojeapi_remove_bad_shader_name(wo_vm vm, wo_value args, size_t argc);
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
    wo_value elem = wo_push_empty(vm);

    wo_set_int(elem, (wo_int_t)sz.x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)sz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_texture_path(wo_vm vm, wo_value args, size_t argc)
{
    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    if (auto str = (*texture)->resouce()->m_path)
        return wo_ret_option_string(vm, str);
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_get_entity_arch_information(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    size_t chunk_size = 0, entity_size = 0, entity_count = 0;

    jedbg_get_entity_arch_information(entity, &chunk_size, &entity_size, &entity_count);
    wo_value result = wo_push_struct(vm, 3);
    wo_value elem = wo_push_empty(vm);

    wo_set_int(elem, (wo_int_t)chunk_size);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)entity_size);
    wo_struct_set(result, 1, elem);
    wo_set_int(elem, (wo_int_t)entity_count);
    wo_struct_set(result, 2, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_audio_buffer_load(wo_vm vm, wo_value args, size_t argc)
{
    auto audiobuf = jeecs::audio::buffer::load(wo_string(args + 0));
    if (audiobuf)
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::audio::buffer>(audiobuf), nullptr,
            [](void* ptr)
            {
                delete std::launder(reinterpret_cast<jeecs::basic::resource<jeecs::audio::buffer>*>(ptr));
            });
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_audio_buffer_byte_size(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::buffer>* buf = (jeecs::basic::resource<jeecs::audio::buffer>*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)buf->get()->get_byte_size());
}
WO_API wo_api wojeapi_audio_buffer_byte_rate(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::buffer>* buf = (jeecs::basic::resource<jeecs::audio::buffer>*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)buf->get()->get_byte_rate());
}

WO_API wo_api wojeapi_audio_source_create(wo_vm vm, wo_value args, size_t argc)
{
    auto audiosrc = jeecs::audio::source::create();
    assert(audiosrc != nullptr);
    return wo_ret_option_gchandle(vm,
        new jeecs::basic::resource<jeecs::audio::source>(audiosrc), nullptr,
        [](void* ptr)
        {
            delete std::launder(reinterpret_cast<jeecs::basic::resource<jeecs::audio::buffer>*>(ptr));
        });
}
WO_API wo_api wojeapi_audio_source_get_state(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)src->get()->get_state());
}
WO_API wo_api wojeapi_audio_source_set_playing_buffer(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    jeecs::basic::resource<jeecs::audio::buffer>* buf = (jeecs::basic::resource<jeecs::audio::buffer>*)wo_pointer(args + 1);
    src->get()->set_playing_buffer(*buf);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_play(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->play();
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_pause(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->pause();
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_stop(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->stop();
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_get_playing_offset(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)src->get()->get_playing_offset());
}
WO_API wo_api wojeapi_audio_source_set_playing_offset(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_playing_offset((size_t)wo_int(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_pitch(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_pitch(wo_float(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_volume(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_volume(wo_float(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_position(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_position(jeecs::math::vec3(wo_float(args + 1), wo_float(args + 2), wo_float(args + 3)));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_velocity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_velocity(jeecs::math::vec3(wo_float(args + 1), wo_float(args + 2), wo_float(args + 3)));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_loop(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_loop(wo_bool(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_audio_listener_set_volume(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::audio::listener::set_volume(wo_float(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_listener_set_position(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::audio::listener::set_position(jeecs::math::vec3(wo_float(args + 0), wo_float(args + 1), wo_float(args + 2)));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_listener_set_direction(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::quat rot(wo_float(args + 0), wo_float(args + 1), wo_float(args + 2));
    jeecs::audio::listener::set_direction(rot);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_listener_set_velocity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::audio::listener::set_velocity(jeecs::math::vec3(wo_float(args + 0), wo_float(args + 1), wo_float(args + 2)));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_register_system(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* result =
        je_towoo_register_system(wo_string(args + 0), wo_string(args + 1));
    if (result != nullptr)
        return wo_ret_option_pointer(vm, (void*)result);

    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_towoo_unregister_system(wo_vm vm, wo_value args, size_t argc)
{
    je_towoo_unregister_system((const jeecs::typing::type_info*)wo_pointer(args + 0));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_update_component(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t component_name = wo_string(args + 0);
    wo_string_t component_path = wo_string(args + 1);
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
            wo_integer_t initfunc = wo_extern_symb(cvm, "_init_towoo_component");
            if (initfunc == 0)
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
                    wo_push_string(cvm, component_name);
                    auto* retval = wo_invoke_rsfunc(cvm, initfunc, 1);

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

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_towoo_unregister_component(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* t = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    je_typing_unregister(t);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_update_api(wo_vm vm, wo_value args, size_t argc)
{
    je_towoo_update_api();

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_typeinfo_get_unregister_count(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, (wo_integer_t)jedbg_get_unregister_type_count());
}

std::recursive_mutex _jewo_singleton_list_mx;
std::unordered_map<std::string, wo_pin_value> _jewo_singleton_list;

wo_pin_value _jewo_create_singleton(wo_vm vm, const char* token, wo_value func)
{
    std::lock_guard g1(_jewo_singleton_list_mx);

    auto fnd = _jewo_singleton_list.find(token);
    if (fnd != _jewo_singleton_list.end())
        return fnd->second;
    else
    {
        wo_value ret = wo_invoke_value(vm, func, 0);
        if (ret != nullptr)
        {
            wo_pin_value pinval = wo_create_pin_value(ret);
            _jewo_singleton_list[token] = pinval;
            return pinval;
        }
        return nullptr;
    }        
}
void _jewo_clear_singletons()
{
    std::lock_guard g1(_jewo_singleton_list_mx);
    for (auto&[_, p] : _jewo_singleton_list)
    {
        wo_close_pin_value(p);
    }
    _jewo_singleton_list.clear();
}

WO_API wo_api wojeapi_create_singleton(wo_vm vm, wo_value args, size_t argc)
{
    wo_pin_value pinvalue = _jewo_create_singleton(vm, wo_string(args + 0), args + 1);
    if (pinvalue != nullptr)
    {
        wo_value val = wo_push_empty(vm);
        wo_read_pin_value(val, pinvalue);

        return wo_ret_val(vm, val);
    }
    return wo_ret_panic(vm, "Failed to create singleton: '%s': %s.", 
        wo_string(args + 0), 
        wo_get_runtime_error(vm));
}
WO_API wo_api wojeapi_clear_singletons(wo_vm vm, wo_value args, size_t argc)
{
    _jewo_clear_singletons();
    return wo_ret_void(vm);
}

const char* jeecs_woolang_editor_api_path = "je/editor.wo";
const char* jeecs_woolang_editor_api_src = R"(
import woo::std;
import je;

namespace je
{
    namespace typeinfo
    {
        extern("libjoyecs", "wojeapi_typeinfo_get_unregister_count")
        public func get_unregister_count()=> int;
    }
    namespace graphic
    {
        namespace texture
        {
            namespace editor
            {
                public using snapshot_t = gchandle;

                extern("libjoyecs", "wojeapi_texture_take_snapshot")
                public func snapshot(self: texture)=> option<snapshot_t>;

                extern("libjoyecs", "wojeapi_texture_restore_snapshot")
                public func restore_snapshot(self: texture, snapshot: snapshot_t)=> bool;
            }
        }
    }
    namespace world
    {
        namespace editor
        {
            extern("libjoyecs", "wojeapi_world_is_valid")
            public func is_valid(self: world)=> bool;
        }
    }
    namespace entity
    {
        namespace editor
        {
            public using euid_t = handle;

            extern("libjoyecs", "wojeapi_get_editing_entity_uid")
            public func get_editing_uid()=> option<euid_t>;

            extern("libjoyecs", "wojeapi_get_entity_uid")
            public func get_entity_uid(self: entity)=> euid_t;

            extern("libjoyecs", "wojeapi_set_parent")
            public func set_parent(self: entity, parent: entity, force: bool)=> bool;

            extern("libjoyecs", "wojeapi_get_entity_anchor_uuid")
            public func get_anchor_uid(self: entity)=> option<string>;

            extern("libjoyecs", "wojeapi_set_parent_with_uid")
            public func set_parent_with_uid(self: entity, parent_uid: string, force: bool)=> bool;

            extern("libjoyecs", "wojeapi_get_parent_anchor_uid")
            public func get_parent_anchor_uid(self: entity)=> option<string>;

            extern("libjoyecs", "wojeapi_get_entity_name")
            public func name(self: entity)=> string;

            extern("libjoyecs", "wojeapi_set_entity_name")
            public func set_name(self: entity, name: string)=> void;

            extern("libjoyecs", "wojeapi_find_entity_with_chunk_info")
            public func find_entity_by_chunkinfo(chunkinfo: string)=> entity;

            extern("libjoyecs", "wojeapi_entity_get_prefab_path")
            public func get_prefab_path(self: entity)=> option<string>;

            extern("libjoyecs", "wojeapi_entity_set_prefab_path")
            public func set_prefab_path(self: entity, path: string)=> void;

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

            extern("libjoyecs", "wojeapi_is_top_entity")
            public func is_top(self: entity)=> bool;

            extern("libjoyecs", "wojeapi_is_child_of_entity")
            public func is_child_of(self: entity, parent: entity)=> bool;

            namespace graphic
            {
                // Used for reload specify shader; called when shader updated or moved;
                // RETURN TRUE MEANS OK
                extern("libjoyecs", "wojeapi_reload_shader_of_entity")
                public func try_reload_shaders(self: entity, old_shad: string, new_shad: string)=> bool;

                extern("libjoyecs", "wojeapi_reload_texture_of_entity")
                public func try_reload_textures(self: entity, old_tex: string, new_tex: string)=> bool;
            }

            using bad_shader_handle_t = handle;

            extern("libjoyecs", "wojeapi_get_bad_shader_list_of_entity")
                public func get_bad_shader_paths(e: entity)=> array<string>;

            extern("libjoyecs", "wojeapi_store_bad_shader_name")
                public func store_name_for_bad_shader_update(e: entity, shad_path: string)=> bad_shader_handle_t;

            extern("libjoyecs", "wojeapi_remove_bad_shader_name")
                public func remove_name_for_bad_shader_update(e: entity, shad_path: string)=> void;

            namespace bad_shader_handle_t
            {
                public func store_uniform_dat_for_bad_shader_update<T>(shadhandle: bad_shader_handle_t, name: string, val: T)
                    where std::declval:<T>() is int
                           || std::declval:<T>() is real
                           || std::declval:<T>() is (real, real)
                           || std::declval:<T>() is (real, real, real)
                           || std::declval:<T>() is (real, real, real, real);
                {
                    if (std::declval:<T>() is int)
                    {
                        extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_int")
                        func _set_uniform_int(shadhandle: bad_shader_handle_t, name: string, val: int)=> void;

                        _set_uniform_int(shadhandle, name, val);
                    }
                    else if (std::declval:<T>() is real)
                    {
                        extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float")
                        func _set_uniform_float(shadhandle: bad_shader_handle_t, name: string, val: real)=> void;

                        _set_uniform_float(shadhandle, name, val);
                    }
                    else if (std::declval:<T>() is (real, real))
                    {
                        extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float2")
                        func _set_uniform_float2(shadhandle: bad_shader_handle_t, name: string, x: real, y: real)=> void;
                        let (x, y) = val;
                        _set_uniform_float2(shadhandle, name, x, y);
                    }
                    else if (std::declval:<T>() is (real, real, real))
                    {
                        extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float3")
                        func _set_uniform_float3(shadhandle: bad_shader_handle_t, name: string, x: real, y: real, z: real)=> void;
                        let (x, y, z) = val;
                        _set_uniform_float3(shadhandle, name, x, y, z);
                    }
                    else if (std::declval:<T>() is (real, real, real, real))
                    {
                        extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float4")
                        func _set_uniform_float4(shadhandle: bad_shader_handle_t, name: string, x: real, y: real, z: real, w: real)=> void;
                        let (x, y, z, w) = val;
                        _set_uniform_float4(shadhandle, name, x, y, z, w);
                    }
                    else
                        std::panic("Here should not been exec.");
                }
            }
            public func set_editing_uid(id: option<euid_t>)
            {
                extern("libjoyecs", "wojeapi_set_editing_entity_uid")
                public func _set_editing_entity(euid: euid_t)=> void;
                extern("libjoyecs", "wojeapi_set_editing_entity_uid")
                public func _reset_editing_entity()=> void;

                match(id)
                {
                value(e)? _set_editing_entity(e);
                none? _reset_editing_entity();
                }
            }
        }
    }
    namespace editor
    {
        extern("libjoyecs", "wojeapi_clear_singletons")
        public func clear_singletons()=> void;

        extern("libjoyecs", "wojeapi_editor_register_panic_hook")
        public func register_panic_hook(f: (string, int, string, int, string, string)=> void)=> void;

        extern("libjoyecs", "wojeapi_mark_shared_glresource_outdated")
        public func mark_shared_glresource_outdated(respath: string)=> bool;

        extern("libjoyecs", "wojeapi_init_graphic_pipeline")
        public func init_graphic_pipeline(u: universe)=> void;

        extern("libjoyecs", "wojeapi_set_runtime_path")
        public func set_runtime_path(path: string)=> void;

        extern("libjoyecs", "wojeapi_input_update_window_size")
        public func set_window_size(x: int, y: int)=> void;

        extern("libjoyecs", "wojeapi_input_update_window_title")
        public func set_window_title(title: string)=> void;

        public using fimage_packer = handle
        {
            extern("libjoyecs", "wojeapi_create_fimg_packer")
            public func create(saving_path: string, max_img_size: int)=> fimage_packer;

            extern("libjoyecs", "wojeapi_pack_file_to_fimg_packer")
            public func pack(self: fimage_packer, file_path: string, pack_path: string)=> bool;

            extern("libjoyecs", "wojeapi_pack_buffer_to_fimg_packer")
            public func pack_buffer(self: fimage_packer, buffer: handle, len: int, pack_path: string)=> bool;

            extern("libjoyecs", "wojeapi_finish_fimg_packer")
            public func finish(self: fimage_packer)=> void;
        }

        extern("libjoyecs", "wojeapi_get_sleep_suppression")
        public func get_sleep_suppression()=> real;

        extern("libjoyecs", "wojeapi_set_sleep_suppression")
        public func set_sleep_suppression(tm: real)=> void;

        extern("libjoyecs", "wojeapi_build_version")
        public func build_version()=> string;

        extern("libjoyecs", "wojeapi_build_commit")
        public func build_commit()=> string;
    
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

        extern("libjoyecs", "wojeapi_update_editor_mouse_pos")
        public func update_editor_mouse_pos(x: real, y: real, lockposx: int, lockposy: int)=> void;

        extern("libjoyecs", "wojeapi_setable_editor_system")
        public func enable_editor_system(able: bool)=> void;

        extern("libjoyecs", "wojeapi_apply_camera_framebuf_setting")
        public func apply_camera_framebuf_setting(camera: entity, width: int, height: int)=> void;

        extern("libjoyecs", "wojeapi_get_framebuf_texture")
        public func get_framebuf_texture(camera: entity, index: int)=> option<graphic::texture>;

        extern("libjoyecs", "wojeapi_get_entity_arch_information")
        public func get_entity_arch_information(e: entity)=> (int, int, int); // chunk_size, entity_size, entity_count

        public enum mover_mode
        {
            nospecify,
            selection,
            movement,
            rotation,
            scale,
        }
        extern("libjoyecs", "wojeapi_get_editing_mover_mode")
        public func get_editing_mover_mode(w: je::world)=> mover_mode;

        extern("libjoyecs", "wojeapi_set_editing_mover_mode")
        public func set_editing_mover_mode(w: je::world, m: mover_mode)=> void;

        public enum coord_mode
        {
            global,
            local
        }
        extern("libjoyecs", "wojeapi_get_editing_coord_mode")
        public func get_editing_coord_mode(w: je::world)=> coord_mode;
        extern("libjoyecs", "wojeapi_set_editing_coord_mode")
        public func set_editing_coord_mode(w: je::world, a: coord_mode)=> void;
    }
}
)";

const char* jeecs_woolang_api_path = "je.wo";
const char* jeecs_woolang_api_src = R"(
import woo::std;
namespace je
{
    namespace towoo
    {
        extern("libjoyecs", "wojeapi_towoo_update_api", slow)
        public func update_api()=> void;

        extern("libjoyecs", "wojeapi_towoo_register_system", slow)
        public func update_system(name: string, path: string)=> option<typeinfo>;

        extern("libjoyecs", "wojeapi_towoo_unregister_system")
        public func unregister_system(t: typeinfo)=> void;

        extern("libjoyecs", "wojeapi_towoo_update_component", slow)
        public func update_component(name: string, path: string)=> option<typeinfo>;

        extern("libjoyecs", "wojeapi_towoo_unregister_component")
        public func unregister_component(t: typeinfo)=> void;
    }

    extern("libjoyecs", "wojeapi_deltatime")
    public func deltatime()=> real;

    extern("libjoyecs", "wojeapi_smooth_deltatime")
    public func smooth_deltatime()=> real;

    extern("libjoyecs", "wojeapi_startup_coroutine")
    public func start_coroutine<FT, ArgTs>(f: FT, args: ArgTs)=> void
        where f(args...) is void;

    namespace unsafe
    {
        extern("libjoyecs", "wojeapi_startup_thread")
        public func start_thread<FT, ArgTs>(f: FT, args: ArgTs)=> void
            where f(args...) is void;

        public func singleton<T>(token: string, f: ()=>T)=> ()=>T
        {
            extern("libjoyecs", "wojeapi_create_singleton")
            func _singleton<T>(token: string, f: ()=>T)=> T;

            return \=_singleton(token, f);;
        }
    }

    extern("libjoyecs", "wojeapi_generate_uid")
        public func uid()=> string;

    extern("libjoyecs", "wojeapi_current_platform_config")
    public func platform()=> string;

    extern("libjoyecs", "wojeapi_load_module")
    public func load_module(name: string, path: string)=> option<handle>;

    extern("libjoyecs", "wojeapi_delay_unload_module")
    public func delay_unload_module(module: handle)=> void;

    extern("libjoyecs", "wojeapi_unload_module")
    public func unload_module(module: handle)=> void;

    namespace file
    {
        extern("libjoyecs", "wojeapi_read_file_all")
        public func readall(path: string)=> option<string>;
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

    public let MAX_MOUSE_GROUP_COUNT = 16;

    public enum mousecode
    {
        LEFT, MID, RIGHT,

        CUSTOM_0 = 32,

        _COUNT = 64,
    };

    public enum keycode
    {
        UNKNOWN = 0,

        A = 65, B, C, D, E, F, G, H, I, J, K, L,
        M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        _1 = 49, _2, _3, _4, _5, _6, _7, _8, _9,
        _0, _ = 32,

        L_SHIFT = 256, L_CTRL, L_ALT, TAB, ENTER,
        ESC, BACKSPACE,

        CUSTOM_0 = 512,

        _COUNT = 1024,
    };

    namespace input
    {
        extern("libjoyecs", "wojeapi_wheel_count")
        public func wheel(group: int)=> (real, real);

        extern("libjoyecs", "wojeapi_input_keydown")
        public func keydown(kcode: keycode)=> bool;

        extern("libjoyecs", "wojeapi_input_mousedown")
        public func mousedown(group: int, kcode: mousecode)=> bool;

        extern("libjoyecs", "wojeapi_input_window_size")
        public func windowsize()=> (int, int);

        extern("libjoyecs", "wojeapi_input_mouse_pos")
        public func mousepos(group: int)=> (int, int);

        extern("libjoyecs", "wojeapi_input_mouse_view_pos")
        public func mouseviewpos(group: int)=> (real, real);
    }

    public using typeinfo = handle;
    namespace typeinfo
    {
        extern("libjoyecs", "wojeapi_type_is_component")
        public func is_component(self: typeinfo)=> bool;

        extern("libjoyecs", "wojeapi_type_is_system")
        public func is_system(self: typeinfo)=> bool;

        extern("libjoyecs", "wojeapi_get_all_registed_types")
        public func get_all_registed_types()=> array<typeinfo>;

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
            INT, INT2, BOOL, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT,
        }
        extern("libjoyecs", "wojeapi_type_basic_type")
        private func get_basic_type(tid: basic_type)=> typeinfo;

        extern("libjoyecs", "wojeapi_type_members")
        public func get_members_info(self: typeinfo)=> array<(string, typeinfo)>;

        public let int = typeinfo::get_basic_type(basic_type::INT);
        public let int2 = typeinfo::get_basic_type(basic_type::INT2);
        public let bool = typeinfo::get_basic_type(basic_type::BOOL);
        public let float = typeinfo::get_basic_type(basic_type::FLOAT);
        public let float2 = typeinfo::get_basic_type(basic_type::FLOAT2);
        public let float3 = typeinfo::get_basic_type(basic_type::FLOAT3);
        public let float4 = typeinfo::get_basic_type(basic_type::FLOAT4);
        public let quat = typeinfo::get_basic_type(basic_type::QUAT);
        public let string = typeinfo::get_basic_type(basic_type::STRING);
    }
    namespace audio
    {
        using buffer = gchandle
        {
            extern("libjoyecs", "wojeapi_audio_buffer_load")
            public func load(path: string)=> option<buffer>;

            extern("libjoyecs", "wojeapi_audio_buffer_byte_size")
            public func get_byte_size(self: buffer)=> int;

            extern("libjoyecs", "wojeapi_audio_buffer_byte_rate")
            public func get_byte_rate(self: buffer)=> int;
        }
        using source = gchandle
        {
            enum state
            {
                STOPPED,
                PLAYING,
                PAUSED,
            }

            extern("libjoyecs", "wojeapi_audio_source_create")
            public func create()=> source;

            extern("libjoyecs", "wojeapi_audio_source_get_state")
            public func get_state(self: source)=> state;

            extern("libjoyecs", "wojeapi_audio_source_set_playing_buffer")
            public func set_playing_buffer(self: source, buf: buffer)=> void;

            extern("libjoyecs", "wojeapi_audio_source_play")
            public func play(self: source)=> void;

            extern("libjoyecs", "wojeapi_audio_source_pause")
            public func pause(self: source)=> void;

            extern("libjoyecs", "wojeapi_audio_source_stop")
            public func stop(self: source)=> void;

            extern("libjoyecs", "wojeapi_audio_source_get_playing_offset")
            public func get_playing_offset(self: source)=> int;

            extern("libjoyecs", "wojeapi_audio_source_set_playing_offset")
            public func set_playing_offset(self: source, offset: int)=> void;

            extern("libjoyecs", "wojeapi_audio_source_set_pitch")
            public func set_pitch(self: source, pitch: real)=> void;

            extern("libjoyecs", "wojeapi_audio_source_set_volume")
            public func set_volume(self: source, volume: real)=> void;

            extern("libjoyecs", "wojeapi_audio_source_set_position")
            public func set_position(self: source, x: real, y: real, z: real)=> void;

            extern("libjoyecs", "wojeapi_audio_source_set_velocity")
            public func set_velocity(self: source, x: real, y: real, z: real)=> void;

            extern("libjoyecs", "wojeapi_audio_source_set_loop")
            public func set_loop(self: source, loop: bool)=> void;
        }
        namespace listener
        {
            extern("libjoyecs", "wojeapi_audio_listener_set_volume")
            public func set_volume(volume: real)=> void;

            extern("libjoyecs", "wojeapi_audio_listener_set_position")
            public func set_position(x: real, y: real, z: real)=> void;

            extern("libjoyecs", "wojeapi_audio_listener_set_direction")
            public func set_direction(x: real, y: real, z: real)=> void;

            extern("libjoyecs", "wojeapi_audio_listener_set_velocity")
            public func set_velocity(x: real, y: real, z: real)=> void;
        }
    }
    namespace graphic
    {
        public using texture = gchandle
        {
            extern("libjoyecs", "wojeapi_texture_open", slow)
            public func load(univ: option<universe>, path: string)=> option<texture>;

            extern("libjoyecs", "wojeapi_texture_create", slow)
            public func create(width: int, height: int)=> texture;

            extern("libjoyecs", "wojeapi_texture_bind_path")
            public func bind_path(self: texture, new_path: string)=> void;

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
        }

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

        public using font = gchandle
        {
            extern("libjoyecs", "wojeapi_font_open", slow)
            public func load(path: string, font_width: int)=> option<font>;

            extern("libjoyecs", "wojeapi_font_load_char", slow)
            public func load_char(self: font, ch: char)=> character;

            extern("libjoyecs", "wojeapi_font_string_texture", slow)
            public func load_string(self: font, str: string)=> texture;
        }
)"
R"(
        public using shader = gchandle
        {
            extern("libjoyecs", "wojeapi_shader_open", slow)
            public func load(univ: option<universe>, path: string)=> option<shader>;
            
            extern("libjoyecs", "wojeapi_shader_create", slow)
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

            extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
            public func get_uniforms(self: shader)=> map<string, uniform_variable>;

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

        extern("libjoyecs", "wojeapi_wait_universe", slow)
        public func wait(self: universe)=> void;

        extern("libjoyecs", "wojeapi_universe_get_frame_deltatime")
        public func get_frame_deltatime(self: universe)=> real;

        extern("libjoyecs", "wojeapi_universe_set_frame_deltatime")
        public func set_frame_deltatime(self: universe, delta: real)=> void;

        extern("libjoyecs", "wojeapi_universe_get_max_deltatime")
        public func get_max_deltatime(self: universe)=> real;

        extern("libjoyecs", "wojeapi_universe_set_max_deltatime")
        public func set_max_deltatime(self: universe, delta: real)=> void;

        extern("libjoyecs", "wojeapi_universe_get_timescale")
        public func get_timescale(self: universe)=> real;

        extern("libjoyecs", "wojeapi_universe_set_timescale")
        public func set_timescale(self: universe, scale: real)=> void;

        extern("libjoyecs", "wojeapi_get_all_worlds_in_universe")
        public func get_all_worlds(self: universe)=> array<world>;
    }

    public using world = handle;
    namespace world
    {
        extern("libjoyecs", "wojeapi_create_world_in_universe")
        public func create(self: universe)=> world;

        extern("libjoyecs", "wojeapi_close_world")
        public func close(self: world) => void;

        extern("libjoyecs", "wojeapi_add_system_to_world", slow)
        public func add_system(self: world, systype: typeinfo)=> bool;

        extern("libjoyecs", "wojeapi_add_entity_to_world_with_components")
        public func add_entity(self: world, components: array<typeinfo>)=> entity;

        extern("libjoyecs", "wojeapi_add_entity_to_world_with_prefab")
        public func instance(self: world, prefab: entity)=> entity;

        extern("libjoyecs", "wojeapi_add_prefab_to_world_with_components")
        public func add_prefab(self: world, components: array<typeinfo>)=> entity;

        extern("libjoyecs", "wojeapi_remove_system_from_world")
        public func remove_system(self: world, sysinfo: typeinfo)=> void;
    
        extern("libjoyecs", "wojeapi_get_universe_from_world")
        public func get_universe(self: world)=> universe;

        extern("libjoyecs", "wojeapi_get_system_from_world")
        public func get_system(self: world, systype: typeinfo)=> option<handle>;

        extern("libjoyecs", "wojeapi_get_world_name")
        public func name(self: world)=> string;

        extern("libjoyecs", "wojeapi_set_world_name")
        public func set_name(self: world, _name: string)=> void;

        extern("libjoyecs", "wojeapi_get_all_entities_from_world")
        public func get_all_entities(self: world)=> array<entity>;

        extern("libjoyecs", "wojeapi_get_all_systems_from_world")
        public func get_all_systems(self: world)=> array<typeinfo>;
    }
    public using entity = gchandle;
    namespace entity
    {
        public func operator == (a: entity, b: entity)
        {
            return a->chunkinfo() == b->chunkinfo();
        }
        public func operator != (a: entity, b: entity)
        {
            return a->chunkinfo() != b->chunkinfo();
        }

        extern("libjoyecs", "wojeapi_close_entity")
        public func close(self: entity)=> void;
        
        extern("libjoyecs", "wojeapi_get_world_from_entity")
        public func get_world(self: entity)=> world;

        public func get_component(self: entity, type: typeinfo)=> option<component>
        {
            extern("libjoyecs", "wojeapi_get_component_from_entity")
            func _get_component(self: entity, type: typeinfo)=> option<handle>;
    
            return _get_component(self, type)
                ->> \addr = component{
                        addr = addr, 
                        type = type,
                    }
                    ;
                ;
        };

        public func add_component(self: entity, type: typeinfo)=> component
        {
            extern("libjoyecs", "wojeapi_add_component_from_entity")
            func _add_component(self: entity, type: typeinfo)=> handle;
    
            return component{addr = _add_component(self, type), type = type,};
        };

        extern("libjoyecs", "wojeapi_remove_component_from_entity")
        public func remove_component(self: entity, type: typeinfo)=> void;

        extern("libjoyecs", "wojeapi_get_entity_chunk_info")
        public func chunkinfo(self: entity)=> string;
)"
R"(
        extern("libjoyecs", "wojeapi_shaders_of_entity")
        public func get_shaders(self: entity)=> array<graphic::shader>;

        extern("libjoyecs", "wojeapi_set_shaders_of_entity")
        public func set_shaders(self: entity, shaders: array<graphic::shader>)=> void;

        extern("libjoyecs", "wojeapi_textures_of_entity")
        public func get_textures(self: entity)=> dict<int, graphic::texture>;

        extern("libjoyecs", "wojeapi_bind_texture_for_entity")
        public func bind_texture(self: entity, id: int, tex: graphic::texture)=> void;

    } // end of namespace entity

    public using native_value = handle;
    namespace native_value
    {
        extern("libjoyecs", "wojeapi_native_value_bool")
        public func bool(self: native_value)=> bool;

        extern("libjoyecs", "wojeapi_native_value_int")
        public func int(self: native_value)=> int;

        extern("libjoyecs", "wojeapi_native_value_int2")
        public func int2(self: native_value)=> (int, int);

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

        extern("libjoyecs", "wojeapi_native_value_set_int2")
        public func set_int2(self: native_value, x: int, y: int)=> void;

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

    using component = struct{addr: handle, type: typeinfo}
    {
        extern("libjoyecs", "wojeapi_get_components_member")
        public func get_member(self: component, name: string)=> option<(typeinfo, native_value)>;

        extern("libjoyecs", "wojeapi_component_get_all_members")
        public func get_members(self: component)=> array<(string, typeinfo, native_value)>;
    }
}

)";
