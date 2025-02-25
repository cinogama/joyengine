#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <list>
#include <optional>

wo_integer_t _je_wo_extern_symb_rsfunc(wo_vm vm, wo_string_t name);

std::atomic_flag _jewo_log_buffer_mx = {};
std::list<std::pair<int, std::string>> _jewo_log_buffer;

std::mutex _jewo_all_alive_vm_threads_mx;
std::unordered_set<wo_vm> _jewo_all_alive_vm_threads;

struct _je_thread
{
    std::thread* m_thread;
    std::atomic_bool m_finished;

    JECS_DISABLE_MOVE_AND_COPY(_je_thread);

    _je_thread() = default;
};

WO_API wo_api wojeapi_startup_thread(wo_vm vm, wo_value args)
{
    wo_value arguments = args + 1;
    auto argument_count = wo_lengthof(arguments);

    wo_vm co_vmm = wo_borrow_vm(vm);
    wo_value co_s = wo_reserve_stack(co_vmm, argument_count + 1, nullptr);

    wo_value cofunc = co_s + argument_count;
    wo_set_val(cofunc, args + 0);

    for (size_t i = 0; i < argument_count; --i)
    {
        wo_struct_get(co_s + i, arguments, (uint16_t)i);
    }

    std::lock_guard g1(_jewo_all_alive_vm_threads_mx);
    _jewo_all_alive_vm_threads.insert(co_vmm);

    _je_thread* thread_instance = new _je_thread();
    thread_instance->m_finished.store(false);
    thread_instance->m_thread =
        new std::thread([=]
            {
                wo_invoke_value(co_vmm, cofunc, argument_count, nullptr, nullptr);
                thread_instance->m_finished.store(true);

                do
                {
                    std::lock_guard g2(_jewo_all_alive_vm_threads_mx);
                    _jewo_all_alive_vm_threads.erase(co_vmm);

                    wo_release_vm(co_vmm);
                } while (0);
            });

    return wo_ret_gchandle(vm, thread_instance,
        nullptr,
        [](void* ptr)
        {
            _je_thread* t = std::launder(reinterpret_cast<_je_thread*>(ptr));

            if (t->m_thread->joinable())
                t->m_thread->detach();

            delete t->m_thread;
            delete t;
        });
}

WO_API wo_api wojeapi_wait_thread(wo_vm vm, wo_value args)
{
    _je_thread* t = std::launder(reinterpret_cast<_je_thread*>(wo_pointer(args + 0)));
    if (t->m_thread->joinable())
        t->m_thread->join();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_check_thread(wo_vm vm, wo_value args)
{
    _je_thread* t = std::launder(reinterpret_cast<_je_thread*>(wo_pointer(args + 0)));
    return wo_ret_bool(vm, t->m_finished.load());
}

WO_API wo_api wojeapi_abort_all_thread(wo_vm vm, wo_value args)
{
    for (;;)
    {
        if (true)
        {
            std::lock_guard g1(_jewo_all_alive_vm_threads_mx);
            for (auto* thread_vm : _jewo_all_alive_vm_threads)
                wo_abort_vm(thread_vm);

            if (_jewo_all_alive_vm_threads.empty())
                break;
        }

        je_clock_sleep_for(0.1);
    }

    return wo_ret_void(vm);
}

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
    if (auto* file = jeecs_file_open(wo_string(args + 0)))
    {
        std::vector<char> readed_buf(file->m_file_length);
        auto readed_len = jeecs_file_read(readed_buf.data(), sizeof(char), file->m_file_length, file);
        readed_buf.resize(readed_len);

        jeecs_file_close(file);

        return wo_ret_option_raw_string(vm, readed_buf.data(), readed_buf.size());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_file_cache_write_all(wo_vm vm, wo_value args)
{
    // public func saveall(path: string, content : string) = > bool;
    if (auto* cache = jeecs_create_cache_file(wo_string(args + 0), 0, 1))
    {
        auto data_length = wo_str_bytelen(args + 1);
        auto written_len = jeecs_write_cache_file(wo_string(args + 1), sizeof(char), data_length, cache);

        jeecs_close_cache_file(cache);

        if (data_length == written_len)
            return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}
WO_API wo_api wojeapi_file_cache_read_all(wo_vm vm, wo_value args)
{
    // public func readall(path: string) = > option<string>;
    if (auto* cache = jeecs_load_cache_file(wo_string(args + 0), 0, -1))
    {
        std::vector<char> readed_buf(cache->m_file_length);
        auto readed_len = jeecs_file_read(readed_buf.data(), sizeof(char), cache->m_file_length, cache);
        readed_buf.resize(readed_len);

        jeecs_file_close(cache);

        return wo_ret_option_raw_string(vm, readed_buf.data(), readed_buf.size());
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_mark_shared_glresource_outdated(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, jegl_mark_shared_resources_outdated(wo_string(args + 0)));
}

WO_API wo_api wojeapi_init_graphic_pipeline(wo_vm vm, wo_value args)
{
    jegl_uhost_get_or_create_for_universe(wo_pointer(args + 0), nullptr);
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
        (size_t)wo_int(args + 1)
    ));
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
                _jewo_log_buffer_mx.clear();
            });

    return wo_ret_handle(vm,
        je_log_register_callback([](int level, const char* msg, void* func)
            {
                (*(std::function<void(int, const char*)>*)func)(level, msg);
            }, callbacks));
}

WO_API wo_api wojeapi_unregister_log_callback(wo_vm vm, wo_value args)
{
    auto func = (std::function<void(int, const char*)>*) je_log_unregister_callback((size_t)wo_handle(args + 0));
    delete func;

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_all_logs(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);

    wo_value result = s + 0;
    wo_set_arr(result, vm, 0);

    std::list<std::pair<int, std::string>> logs;

    while (_jewo_log_buffer_mx.test_and_set());
    logs.swap(_jewo_log_buffer);
    assert(_jewo_log_buffer.empty());
    _jewo_log_buffer_mx.clear();

    wo_value elem = s + 1;
    wo_value val = s + 2;
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

WO_API wo_api wojeapi_get_framebuf_texture(wo_vm vm, wo_value args)
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
WO_API wo_api wojeapi_create_universe(wo_vm vm, wo_value args)
{
    void* universe = je_ecs_universe_create();
    return wo_ret_gchandle(vm, universe, nullptr, [](void* universe) {
        jeecs::game_universe::destroy_universe(jeecs::game_universe(universe));
        });
}

WO_API wo_api wojeapi_get_universe_from_world(wo_vm vm, wo_value args)
{
    void* universe = je_ecs_world_in_universe(wo_pointer(args + 0));
    return wo_ret_pointer(vm, universe);
}

WO_API wo_api wojeapi_stop_universe(wo_vm vm, wo_value args)
{
    jeecs::game_universe(wo_pointer(args + 0)).stop();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_wait_universe(wo_vm vm, wo_value args)
{
    jeecs::game_universe(wo_pointer(args + 0)).wait();
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
    wo_set_arr(out_array, vm, 0);

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

    if (gworld.add_system(system_type->m_id))
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
    wo_set_arr(result, vm, 0);

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
    for (wo_integer_t i = 0; i < wo_lengthof(components_list); ++i)
    {
        wo_arr_get(elem, components_list, i);
        components.push_back(((const jeecs::typing::type_info*)wo_pointer(elem))->m_id);
    }

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld._add_entity(components)),
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_add_entity_to_world_with_prefab(wo_vm vm, wo_value args)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    jeecs::game_entity* prefab_entity = (jeecs::game_entity*)wo_pointer(args + 1);

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld.add_entity(*prefab_entity)),
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_add_prefab_to_world_with_components(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::game_world gworld = wo_pointer(args + 0);
    wo_value components_list = args + 1;

    std::vector<jeecs::typing::typeid_t> components;

    wo_value elem = s + 0;
    for (wo_integer_t i = 0; i < wo_lengthof(components_list); ++i)
    {
        wo_arr_get(elem, components_list, i);
        components.push_back(((const jeecs::typing::type_info*)wo_pointer(elem))->m_id);
    }

    return wo_ret_gchandle(vm, new jeecs::game_entity(gworld._add_prefab(components)),
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_get_all_entities_from_world(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_value out_arr = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(s, vm, 0);

    void* world_instance = wo_pointer(args + 0);

    auto required_components_len = wo_lengthof(args + 1);
    std::vector<jeecs::typing::typeid_t> required_components(required_components_len);
    
    for (size_t i = 0; i < required_components_len; ++i)
    {
        wo_arr_get(elem, args + 1, i);

        const jeecs::typing::type_info* t = (const jeecs::typing::type_info*)wo_pointer(elem);
        required_components[i] = t->m_id;
    }

    if (je_ecs_world_is_valid(world_instance))
    {
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
                    [](void* entity_ptr) {
                        jedbg_free_entity((jeecs::game_entity*)entity_ptr);
                    });
                wo_arr_add(out_arr, elem);
            }
        }
        je_mem_free(entities);
    }

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

WO_API wo_api wojeapi_world_is_valid(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, je_ecs_world_is_valid(wo_pointer(args + 0)));
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
    sprintf(buf, "[%p:%zuv%zu]", entity->_m_in_chunk, entity->_m_id, entity->_m_version);
    return wo_ret_string(vm, buf);
}

WO_API wo_api wojeapi_find_entity_with_chunk_info(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = new jeecs::game_entity();
    ((void)sscanf(wo_string(args + 0), "[%p:%zuv%zu]", &entity->_m_in_chunk, &entity->_m_id, &entity->_m_version));

    return wo_ret_gchandle(vm, entity,
        nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
}

WO_API wo_api wojeapi_get_all_components_types_from_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_arr = s + 0;
    wo_set_arr(out_arr, vm, 0);

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

    return wo_ret_option_ptr_may_null(vm, je_ecs_world_entity_get_component(entity,
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
    wo_set_arr(result, vm, 0);

    if (component_type->m_member_types != nullptr)
    {
        auto* member_type = component_type->m_member_types->m_members;
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
        wo_set_struct(result, vm, 2);

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

    wo_set_struct(result, vm, 2);

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

    wo_set_struct(result, vm, 2);

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

    wo_set_struct(result, vm, 2);

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

    wo_set_struct(result, vm, 2);

    wo_set_float(elem, winsz.x);
    wo_struct_set(result, 0, elem);
    wo_set_float(elem, winsz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_input_update_window_size(wo_vm vm, wo_value args)
{
    je_io_set_windowsize((int)wo_int(args + 0), (int)wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_input_update_window_title(wo_vm vm, wo_value args)
{
    je_io_set_windowtitle(wo_string(args + 0));
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
    else //if (wo_valuetype(args + 0) == WO_STRING_TYPE)
        return wo_ret_option_ptr_may_null(vm, (void*)jeecs::typing::type_info::of(wo_string(args + 0)));
}

WO_API wo_api wojeapi_get_all_registed_types(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_value out_array = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(out_array, vm, 0);

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

    wo_set_arr(result, vm, 0);

    if (type->m_member_types != nullptr)
    {
        auto* member_iter = type->m_member_types->m_members;
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
    }

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_type_basic_type(wo_vm vm, wo_value args)
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

///////////////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_graphic_shrink_cache(wo_vm vm, wo_value args)
{
    jegl_context* gcontext = jegl_uhost_get_context(
        jegl_uhost_get_or_create_for_universe(
            wo_pointer(args + 0), nullptr));

    jegl_shrink_shared_resource_cache(gcontext, (size_t)wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_texture_open(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jegl_context* gcontext = nullptr;

    wo_value universe_ptr = s + 0;
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
WO_API wo_api wojeapi_texture_create(wo_vm vm, wo_value args)
{
    auto loaded_texture = jeecs::graphic::texture::create(
        (size_t)wo_int(args + 0), (size_t)wo_int(args + 1), jegl_texture::format::RGBA);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture), nullptr,
        [](void* ptr) {
            delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}
WO_API wo_api wojeapi_texture_clip(wo_vm vm, wo_value args)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::texture>(
            jeecs::graphic::texture::clip(
                *loaded_texture, 
                (size_t)wo_int(args + 1), 
                (size_t)wo_int(args + 2), 
                (size_t)wo_int(args + 3), 
                (size_t)wo_int(args + 4))
        ), nullptr,
        [](void* ptr) {
            delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}

WO_API wo_api wojeapi_texture_bind_path(wo_vm vm, wo_value args)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    auto& path = (*loaded_texture)->resouce()->m_path;
    if (path != nullptr)
        je_mem_free((void*)path);
    path = jeecs::basic::make_new_string(wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_texture_get_pixel(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    wo_value elem = s + 0;
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

WO_API wo_api wojeapi_texture_take_snapshot(wo_vm vm, wo_value args)
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

WO_API wo_api wojeapi_texture_restore_snapshot(wo_vm vm, wo_value args)
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

WO_API wo_api wojeapi_texture_pixel_color(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto* pix = (jeecs::graphic::texture::pixel*)wo_pointer(args + 0);
    auto color = pix->get();

    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, vm, 4);

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

WO_API wo_api wojeapi_font_load_char(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

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
    wo_value result = s + 0; 
    wo_value elem = s + 1;

    wo_set_struct(result, vm, 8);

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
    auto* loaded_font = (jeecs::basic::resource<jeecs::graphic::font>*)wo_pointer(args + 0);
    auto* text_texture = new jeecs::basic::resource<jeecs::graphic::texture>(loaded_font->get()->text_texture(wo_string(args + 1)));

    return wo_ret_gchandle(vm, text_texture, nullptr, [](void* ptr) {
        delete (jeecs::basic::resource<jeecs::graphic::texture>*)ptr;
        });
}

/////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_shader_open(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jegl_context* gcontext = nullptr;

    wo_value universe_ptr = s + 0;
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

WO_API wo_api wojeapi_shader_create(wo_vm vm, wo_value args)
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

WO_API wo_api wojeapi_textures_of_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_map = s + 0;
    wo_set_map(out_map, vm, 0);

    if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
    {
        wo_value key = s + 1;
        wo_value val = s + 2;

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
                *(jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(elem));
        }
        else
        {
            textures->bind_texture((size_t)wo_int(args + 1), nullptr);
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
            shape->vertex = *(jeecs::basic::resource<jeecs::graphic::vertex>*)wo_pointer(elem);
        else
            shape->vertex = nullptr;
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Textures组件，在此panic?

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_shape_of_entity(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    if (jeecs::Renderer::Shape* shape = entity->get_component<jeecs::Renderer::Shape>())
    {
        if (shape->vertex != nullptr)
            return wo_ret_option_pointer(vm, 
                new jeecs::basic::resource<jeecs::graphic::vertex>(shape->vertex));
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_vertex_load(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jegl_context* gcontext = nullptr;

    wo_value universe_ptr = s + 0;
    if (wo_option_get(universe_ptr, args + 0))
    {
        gcontext = jegl_uhost_get_context(
            jegl_uhost_get_or_create_for_universe(
                wo_pointer(universe_ptr), nullptr));
    }

    auto loaded_vertex = jeecs::graphic::vertex::load(gcontext, wo_string(args + 1));

    if (loaded_vertex != nullptr)
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::vertex>(loaded_vertex), nullptr,
            [](void* ptr) {
                delete (jeecs::basic::resource<jeecs::graphic::vertex>*)ptr;
            });

    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_vertex_create(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    // vertices: array<real>, indices: array<int>
    jegl_vertex::type vertex_type = (jegl_vertex::type)wo_int(args + 0);
    std::vector<float> vertices(wo_lengthof(args + 1));
    std::vector<uint32_t> indices(wo_lengthof(args + 2));
    std::vector<jegl_vertex::data_layout> formats(wo_lengthof(args + 3));

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

    auto loaded_vertex = jeecs::graphic::vertex::create(
        vertex_type, 
        vertices.data(), 
        vertices.size() * sizeof(float), 
        indices,
        formats);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::graphic::vertex>(loaded_vertex), nullptr,
        [](void* ptr) {
            delete (jeecs::basic::resource<jeecs::graphic::vertex>*)ptr;
        });
}

WO_API wo_api wojeapi_vertex_path(wo_vm vm, wo_value args)
{
    auto* loaded_vertex = (jeecs::basic::resource<jeecs::graphic::vertex>*)wo_pointer(args + 0);

    if (auto path = (*loaded_vertex)->resouce()->m_path)
        return wo_ret_option_string(vm, path);
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_shaders_of_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_array = s + 0;
    wo_set_arr(out_array, vm, 0);

    if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
    {
        wo_value elem = s + 1;

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

WO_API wo_api wojeapi_get_uniforms_from_shader(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 4, &args);

    /*
    extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
                func _get_uniforms_from_shader(
                    shad: shader
                )=> map<string, uniform_variable>;
    */
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    wo_value out_map = s + 0;
    wo_set_map(out_map, vm, 0);

    auto* uniforms = (*shader)->resouce()->m_raw_shader_data->m_custom_uniforms;
    wo_value key = s + 1;
    wo_value val = s + 2;
    wo_value elem = s + 3;
    while (uniforms)
    {
        wo_set_string(key, vm, uniforms->m_name);
        wo_set_struct(val, vm, 2);
        wo_map_set(out_map, key, val);

        if (uniforms->m_uniform_type >= jegl_shader::uniform_type::INT
            && uniforms->m_uniform_type <= jegl_shader::uniform_type::TEXTURE)
            wo_set_int(elem, uniforms->m_uniform_type);
        else
            // Others
            wo_set_int(elem, 1 + jegl_shader::uniform_type::TEXTURE);

        wo_struct_set(val, 0, elem);

        switch (uniforms->m_uniform_type)
        {
        case jegl_shader::uniform_type::INT:
        case jegl_shader::uniform_type::TEXTURE:
            wo_set_int(elem, uniforms->ix); break;
        case jegl_shader::uniform_type::INT2:
            wo_set_struct(elem, vm, 2);
            wo_set_int(key, uniforms->ix);
            wo_struct_set(elem, 0, key);
            wo_set_int(key, uniforms->iy);
            wo_struct_set(elem, 1, key);
            break;
        case jegl_shader::uniform_type::INT3:
            wo_set_struct(elem, vm, 3);
            wo_set_int(key, uniforms->ix);
            wo_struct_set(elem, 0, key);
            wo_set_int(key, uniforms->iy);
            wo_struct_set(elem, 1, key);
            wo_set_int(key, uniforms->iz);
            wo_struct_set(elem, 2, key);
            break;
        case jegl_shader::uniform_type::INT4:
            wo_set_struct(elem, vm, 4);
            wo_set_int(key, uniforms->ix);
            wo_struct_set(elem, 0, key);
            wo_set_int(key, uniforms->iy);
            wo_struct_set(elem, 1, key);
            wo_set_int(key, uniforms->iz);
            wo_struct_set(elem, 2, key);
            wo_set_int(key, uniforms->iw);
            wo_struct_set(elem, 3, key);
            break;
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

WO_API wo_api wojeapi_set_uniforms_int(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_int2(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_int3(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3), (int)wo_int(args + 4));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_int4(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3), (int)wo_int(args + 4), (int)wo_int(args + 5));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1), wo_float(args + 2));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float2(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1),
        jeecs::math::vec2(wo_float(args + 2), wo_float(args + 3)));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float3(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
    (*shader)->set_uniform(wo_string(args + 1),
        jeecs::math::vec3(wo_float(args + 2), wo_float(args + 3), wo_float(args + 4)));

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_set_uniforms_float4(wo_vm vm, wo_value args)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);
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
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader>*)wo_pointer(args + 0);

    if (auto str = (*shader)->resouce()->m_path)
        return wo_ret_string(vm, str);
    return wo_ret_string(vm, "< Built-in shader >");
}

WO_API wo_api wojeapi_texture_get_size(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);
    auto sz = texture->get()->size();

    wo_value result = s + 0; 
    wo_value elem = s + 1;

    wo_set_struct(result, vm, 2);

    wo_set_int(elem, (wo_int_t)sz.x);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)sz.y);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_texture_path(wo_vm vm, wo_value args)
{
    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 0);

    if (auto str = (*texture)->resouce()->m_path)
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

    wo_set_struct(result, vm, 3);

    wo_set_int(elem, (wo_int_t)chunk_size);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_int_t)entity_size);
    wo_struct_set(result, 1, elem);
    wo_set_int(elem, (wo_int_t)entity_count);
    wo_struct_set(result, 2, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api wojeapi_audio_buffer_load(wo_vm vm, wo_value args)
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
WO_API wo_api wojeapi_audio_buffer_byte_size(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::buffer>* buf = (jeecs::basic::resource<jeecs::audio::buffer>*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)buf->get()->get_byte_size());
}
WO_API wo_api wojeapi_audio_buffer_byte_rate(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::buffer>* buf = (jeecs::basic::resource<jeecs::audio::buffer>*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)buf->get()->get_byte_rate());
}

WO_API wo_api wojeapi_audio_source_create(wo_vm vm, wo_value args)
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
WO_API wo_api wojeapi_audio_source_get_state(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)src->get()->get_state());
}
WO_API wo_api wojeapi_audio_source_set_playing_buffer(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    jeecs::basic::resource<jeecs::audio::buffer>* buf = (jeecs::basic::resource<jeecs::audio::buffer>*)wo_pointer(args + 1);
    src->get()->set_playing_buffer(*buf);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_play(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->play();
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_pause(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->pause();
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_stop(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->stop();
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_get_playing_offset(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)src->get()->get_playing_offset());
}
WO_API wo_api wojeapi_audio_source_set_playing_offset(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_playing_offset((size_t)wo_int(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_pitch(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_pitch(wo_float(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_volume(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_volume(wo_float(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_position(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_position(jeecs::math::vec3(wo_float(args + 1), wo_float(args + 2), wo_float(args + 3)));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_velocity(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_velocity(jeecs::math::vec3(wo_float(args + 1), wo_float(args + 2), wo_float(args + 3)));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_source_set_loop(wo_vm vm, wo_value args)
{
    jeecs::basic::resource<jeecs::audio::source>* src = (jeecs::basic::resource<jeecs::audio::source>*)wo_pointer(args + 0);
    src->get()->set_loop(wo_bool(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_audio_listener_set_volume(wo_vm vm, wo_value args)
{
    jeecs::audio::listener::set_volume(wo_float(args + 0));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_listener_set_position(wo_vm vm, wo_value args)
{
    jeecs::audio::listener::set_position(jeecs::math::vec3(wo_float(args + 0), wo_float(args + 1), wo_float(args + 2)));
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_listener_set_direction(wo_vm vm, wo_value args)
{
    jeecs::math::quat rot(wo_float(args + 0), wo_float(args + 1), wo_float(args + 2));
    jeecs::audio::listener::set_direction(rot);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_audio_listener_set_velocity(wo_vm vm, wo_value args)
{
    jeecs::audio::listener::set_velocity(jeecs::math::vec3(wo_float(args + 0), wo_float(args + 1), wo_float(args + 2)));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_register_system(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* result =
        je_towoo_register_system(wo_string(args + 0), wo_string(args + 1));
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
            wo_integer_t initfunc = _je_wo_extern_symb_rsfunc(cvm, "_init_towoo_component");
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
                    wo_value cvm_s = wo_reserve_stack(cvm, 1, nullptr);
                    wo_set_string(cvm_s + 0, cvm, component_name);
                    auto* retval = wo_invoke_rsfunc(cvm, initfunc, 1, nullptr, &cvm_s);
                    wo_pop_stack(cvm, 1);

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

WO_API wo_api wojeapi_towoo_unregister_component(wo_vm vm, wo_value args)
{
    const jeecs::typing::type_info* t = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    je_typing_unregister(t);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_update_api(wo_vm vm, wo_value args)
{
    je_towoo_update_api();

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_typemgr_get_unregister_count(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, (wo_integer_t)jedbg_get_unregister_type_count());
}

WO_API wo_api  wojeapi_get_all_internal_scripts(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);

    wo_value result = s + 0;
    wo_value key = s + 1;
    wo_value val = s + 2;

    wo_set_map(result, vm, 0);

    auto* iter = wo_open_virtual_file_iter();
    while (wo_string_t vpath = wo_next_virtual_file_iter(iter))
    {
        if (auto* vfhandle = wo_open_virtual_file(vpath))
        {
            size_t len;
            auto* dat = wo_virtual_file_data(vfhandle, &len);

            wo_set_string(key, vm, vpath);
            wo_set_buffer(val, vm, dat, len);
            wo_map_set(result, key, val);
        }
    }
    wo_close_virtual_file_iter(iter);
    return wo_ret_val(vm, result);
}

std::recursive_mutex _jewo_singleton_list_mx;
struct _jewo_singleton
{
    wo_pin_value m_value;
    std::recursive_mutex* m_mutex;

    JECS_DISABLE_MOVE_AND_COPY(_jewo_singleton);

    _jewo_singleton(wo_value value)
        : m_mutex(new std::recursive_mutex())
    {
        m_value = wo_create_pin_value();
        wo_pin_value_set(m_value, value);
    }
    ~_jewo_singleton()
    {
        wo_close_pin_value(m_value);
        delete m_mutex;
    }
};
std::unordered_map<std::string, jeecs::basic::resource<_jewo_singleton>> _jewo_singleton_list;

jeecs::basic::resource<_jewo_singleton> _jewo_create_singleton(
    wo_vm vm, const char* token, wo_value func, wo_value* inout_args)
{
    std::lock_guard g1(_jewo_singleton_list_mx);

    auto fnd = _jewo_singleton_list.find(token);
    if (fnd != _jewo_singleton_list.end())
        return fnd->second;
    else
    {
        wo_value ret = wo_invoke_value(vm, func, 0, inout_args, nullptr);
        if (ret != nullptr)
        {
            return _jewo_singleton_list[token] =
                new _jewo_singleton(ret);
        }
        return nullptr;
    }
}
void _jewo_clear_singletons()
{
    std::lock_guard g1(_jewo_singleton_list_mx);
    _jewo_singleton_list.clear();
}

WO_API wo_api wojeapi_create_singleton(wo_vm vm, wo_value args)
{
    auto singleton = _jewo_create_singleton(vm, wo_string(args + 0), args + 1, &args);
    if (singleton != nullptr)
    {
        return wo_ret_gchandle(vm,
            new jeecs::basic::resource<_jewo_singleton>(singleton),
            nullptr,
            [](void* p)
            {
                delete std::launder(
                    reinterpret_cast<
                    jeecs::basic::resource<_jewo_singleton>*>(
                        p));
            });
    }
    return wo_ret_panic(vm, "Failed to create singleton: '%s': %s.",
        wo_string(args + 0),
        wo_get_runtime_error(vm));
}
WO_API wo_api wojeapi_apply_singleton(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    jeecs::basic::resource<_jewo_singleton> singleton =
        *std::launder(
            reinterpret_cast<
            jeecs::basic::resource<_jewo_singleton>*>(
                wo_pointer(args + 0)));
    wo_value f = args + 1;

    std::lock_guard g1(*(singleton->m_mutex));

    auto val = s + 0;
    wo_pin_value_get(val, singleton->m_value);

    wo_value result = wo_invoke_value(vm, f, 1, &args, &s);
    if (result == nullptr)
        return wo_ret_panic(vm, "Failed to apply singleton: %s.",
            wo_get_runtime_error(vm));

    return wo_ret_val(vm, result);
}
WO_API wo_api wojeapi_clear_singletons(wo_vm vm, wo_value args)
{
    _jewo_clear_singletons();
    return wo_ret_void(vm);
}

struct dynamic_parser_impl_t
{
    wo_integer_t m_saving;
    wo_integer_t m_restoring;
    wo_integer_t m_edit;

    const jeecs::typing::typeinfo_script_parser*
        m_script_parser;
};

std::mutex  _je_dynamic_parser_mx;
wo_vm       _je_dynamic_parser_vm = nullptr;
std::unordered_map<jeecs::typing::typeid_t, std::unique_ptr<dynamic_parser_impl_t>>
_je_dynamic_parser_impls;

void _je_dynamic_parser_clear()
{
    if (_je_dynamic_parser_vm != nullptr)
    {
        _je_dynamic_parser_impls.clear();

        wo_close_vm(_je_dynamic_parser_vm);
        _je_dynamic_parser_vm = nullptr;
    }

    assert(_je_dynamic_parser_impls.empty());
}
void _je_dynamic_parser_update_types()
{
    if (_je_dynamic_parser_vm != nullptr)
    {
        _je_dynamic_parser_impls.clear();

        auto** types = jedbg_get_all_registed_types();

        auto** cur_type = types;
        while (*cur_type)
        {
            auto* script_parser = (*cur_type)->get_script_parser();
            if (script_parser != nullptr)
            {
                std::string script_woolang_typename = script_parser->m_woolang_typename;

                wo_integer_t saving_func = _je_wo_extern_symb_rsfunc(_je_dynamic_parser_vm, (script_woolang_typename + "::parser::saving").c_str());
                wo_integer_t restoring_func = _je_wo_extern_symb_rsfunc(_je_dynamic_parser_vm, (script_woolang_typename + "::parser::restoring").c_str());
                wo_integer_t edit_func = _je_wo_extern_symb_rsfunc(_je_dynamic_parser_vm, (script_woolang_typename + "::parser::edit").c_str());

                if (saving_func != 0 || restoring_func != 0 || edit_func != 0)
                {
                    _je_dynamic_parser_impls.insert(
                        std::make_pair(
                            (*cur_type)->m_id, std::make_unique<dynamic_parser_impl_t>(
                                dynamic_parser_impl_t
                                {
                                    saving_func,
                                    restoring_func,
                                    edit_func,
                                    script_parser
                                })));
                }
            }

            ++cur_type;
        }

        je_mem_free(types);
    }
    else
    {
        assert(_je_dynamic_parser_impls.empty());
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

        std::string result = wo_get_compile_error(newvm, wo_inform_style::WO_DEFAULT);
        wo_close_vm(newvm);

        return std::optional(result);
    }

    wo_jit(newvm);
    wo_run(newvm);

    free(content);

    _je_dynamic_parser_clear();
    _je_dynamic_parser_vm = newvm;
    _je_dynamic_parser_update_types();

    return std::nullopt;
}

////////////////////////////////////////////////////////////////

WO_API wo_api wojeapi_dynamic_parser_update_script(wo_vm vm, wo_value args)
{
    std::lock_guard g1(_je_dynamic_parser_mx);

    if (auto result = _je_dynamic_parser_update_all(wo_string(args + 0)))
        return wo_ret_err_string(vm, result.value().c_str());
    return wo_ret_ok_void(vm);
}

WO_API wo_api wojeapi_dynamic_parser_update_type(wo_vm vm, wo_value args)
{
    std::lock_guard g1(_je_dynamic_parser_mx);

    _je_dynamic_parser_update_types();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_dynamic_parser_clear(wo_vm vm, wo_value args)
{
    std::lock_guard g1(_je_dynamic_parser_mx);

    _je_dynamic_parser_clear();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_dynamic_parser_saving(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    std::lock_guard g1(_je_dynamic_parser_mx);

    auto* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    auto fnd = _je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_vm != nullptr);

        auto* val = wo_pointer(args + 1);
        auto& parser = fnd->second;

        assert(parser->m_saving != 0);

        wo_value value = s + 0;
        parser->m_script_parser->m_script_parse_c2w(val, vm, value);

        wo_value _je_dynamic_parser_vm_s = wo_reserve_stack(_je_dynamic_parser_vm, 1, nullptr);
        wo_set_val(_je_dynamic_parser_vm_s + 0, value);
        wo_value result = wo_invoke_rsfunc(
            _je_dynamic_parser_vm, parser->m_saving, 1, nullptr, &_je_dynamic_parser_vm_s);

        wo_pop_stack(_je_dynamic_parser_vm, 1);

        if (result != nullptr)
            return wo_ret_option_val(vm, result);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_dynamic_parser_restoring(wo_vm vm, wo_value args)
{
    std::lock_guard g1(_je_dynamic_parser_mx);

    auto* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    auto fnd = _je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_vm != nullptr);

        void* val = wo_pointer(args + 1);
        auto& parser = fnd->second;

        assert(parser->m_restoring != 0);

        wo_value _je_dynamic_parser_vm_s = wo_reserve_stack(_je_dynamic_parser_vm, 1, nullptr);
        wo_set_val(_je_dynamic_parser_vm_s + 0, args + 2);

        wo_value result = wo_invoke_rsfunc(
            _je_dynamic_parser_vm, parser->m_restoring, 1, nullptr, &_je_dynamic_parser_vm_s);

        wo_pop_stack(_je_dynamic_parser_vm, 1);

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

    std::lock_guard g1(_je_dynamic_parser_mx);

    auto* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    auto fnd = _je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_vm != nullptr);

        auto* val = wo_pointer(args + 1);
        auto* tag = wo_string(args + 2);

        auto& parser = fnd->second;

        assert(parser->m_edit != 0);

        wo_value value = s + 0;
        parser->m_script_parser->m_script_parse_c2w(val, vm, value);

        wo_value _je_dynamic_parser_vm_s = wo_reserve_stack(
            _je_dynamic_parser_vm, 2, nullptr);

        wo_set_val(_je_dynamic_parser_vm_s + 0, value);
        wo_set_string(_je_dynamic_parser_vm_s + 1, _je_dynamic_parser_vm, tag);
        
        wo_value result = wo_invoke_rsfunc(
            _je_dynamic_parser_vm, parser->m_edit, 2, nullptr, &_je_dynamic_parser_vm_s);

        wo_pop_stack(_je_dynamic_parser_vm, 2);

        if (result != nullptr)
        {
            if (wo_option_get(value, result))
                parser->m_script_parser->m_script_parse_w2c(val, vm, value);

            return wo_ret_bool(vm, WO_TRUE);
        }
    }
    return wo_ret_bool(vm, WO_FALSE);
}

const char* jeecs_woolang_editor_api_path = "je/editor.wo";
const char* jeecs_woolang_editor_api_src = R"(
import woo::std;
import je;

namespace je
{
    namespace woolang
    {
        extern("libjoyecs", "wojeapi_get_all_internal_scripts")
        public func get_all_internal_scripts()=> dict<string, string>;
    }
    namespace typeinfo
    {
        extern("libjoyecs", "wojeapi_typemgr_get_unregister_count")
        public func get_unregister_count()=> ::int;
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

            extern("libjoyecs", "wojeapi_get_entity_uid")
            public func get_entity_uid(self: entity)=> option<euid_t>;

            extern("libjoyecs", "wojeapi_get_editing_entity_uid")
            public func get_editing_uid()=> option<euid_t>;

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
                    =>> \tid = comp->is_value ? [(tid, comp->unwrap)] | []
                        where comp = self->get_component(tid);
                    ->  mapping;
            }

            extern("libjoyecs", "wojeapi_get_all_components_types_from_entity")
            public func get_components_types(self: entity)=> array<typeinfo>;

            extern("libjoyecs", "wojeapi_is_top_entity")
            public func is_top(self: entity)=> bool;

            extern("libjoyecs", "wojeapi_is_child_of_entity")
            public func is_child_of(self: entity, parent: entity)=> bool;

            namespace graphic
            {
                // Used for reload specify shader; called when shader updated or moved;
                // RETURN TRUE MEANS OK
                extern("libjoyecs", "wojeapi_reload_shader_of_entity", slow)
                public func try_reload_shaders(self: entity, old_shad: string, new_shad: string)=> bool;

                extern("libjoyecs", "wojeapi_reload_texture_of_entity", slow)
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
                extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_int")
                func _set_uniform_int(shadhandle: bad_shader_handle_t, name: string, val: int)=> void;
                extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_int2")
                func _set_uniform_int2(shadhandle: bad_shader_handle_t, name: string, x: int, y: int)=> void;
                extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_int3")
                func _set_uniform_int3(shadhandle: bad_shader_handle_t, name: string, x: int, y: int, z: int)=> void;
                extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_int4")
                func _set_uniform_int4(shadhandle: bad_shader_handle_t, name: string, x: int, y: int, z: int, w: int)=> void;
                extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float")
                func _set_uniform_float(shadhandle: bad_shader_handle_t, name: string, val: real)=> void;
                extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float2")
                func _set_uniform_float2(shadhandle: bad_shader_handle_t, name: string, x: real, y: real)=> void;
                extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float3")
                func _set_uniform_float3(shadhandle: bad_shader_handle_t, name: string, x: real, y: real, z: real)=> void;
                extern("libjoyecs", "wojeapi_store_bad_shader_uniforms_float4")
                func _set_uniform_float4(shadhandle: bad_shader_handle_t, name: string, x: real, y: real, z: real, w: real)=> void;

                public func store_uniform_dat_for_bad_shader_update<T>(shadhandle: bad_shader_handle_t, name: string, val: T)
                    where val is int
                        || val is (int, int)
                        || val is (int, int, int)
                        || val is (int, int, int, int)
                        || val is real
                        || val is (real, real)
                        || val is (real, real, real)
                        || val is (real, real, real, real);
                {
                    if (val is int)
                        _set_uniform_int(shadhandle, name, val);
                    else if (val is (int, int))
                        _set_uniform_int2(shadhandle, name, val...);
                    else if (val is (int, int, int))
                        _set_uniform_int3(shadhandle, name, val...);
                    else if (val is (int, int, int, int))
                        _set_uniform_int4(shadhandle, name, val...);
                    else if (val is real)
                        _set_uniform_float(shadhandle, name, val);
                    else if (val is (real, real))
                        _set_uniform_float2(shadhandle, name, val...);
                    else if (val is (real, real, real))
                        _set_uniform_float3(shadhandle, name, val...);
                    else if (val is (real, real, real, real))
                        _set_uniform_float4(shadhandle, name, val...);
                    else
                        std::panic("Here should not been exec.");
                }
            }
            public func set_editing_uid(id: option<euid_t>)
            {
                extern("libjoyecs", "wojeapi_set_editing_entity_uid")
                    public func _set_editing_entity(eid: euid_t)=> void;

                extern("libjoyecs", "wojeapi_reset_editing_entity_uid")
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
    public enum platform
    {
        WINDOWS = 1,
        LINUX = 2,
        ANDROID = 3,
        WEBGL = 4,
    }
    extern("libjoyecs", "wojeapi_get_current_platform")
    public func get_current_platform()=> platform;

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

        namespace unsafe
        {
            namespace dynamic_parser
            {
                extern("libjoyecs", "wojeapi_dynamic_parser_update_script", slow)
                public func update_all(path: string)=> result<void, string>;

                extern("libjoyecs", "wojeapi_dynamic_parser_update_type", slow)
                public func update_type()=> void;

                extern("libjoyecs", "wojeapi_dynamic_parser_clear", slow)
                public func clear()=> void;

                extern("libjoyecs", "wojeapi_dynamic_parser_saving")
                public func saving(type: typeinfo, val: handle)=> option<string>;

                extern("libjoyecs", "wojeapi_dynamic_parser_restoring")
                public func restoring(type: je::typeinfo, val: handle, dat: string)=> bool;

                extern("libjoyecs", "wojeapi_dynamic_parser_edit")
                public func edit(type: je::typeinfo, val: handle, tag: string)=> bool;
            }
        }
    }

    extern("libjoyecs", "wojeapi_deltatime")
    public func deltatime()=> real;

    extern("libjoyecs", "wojeapi_smooth_deltatime")
    public func smooth_deltatime()=> real;

    extern("libjoyecs", "wojeapi_startup_coroutine")
    public func start_coroutine<FT, ArgTs>(f: FT, args: ArgTs)=> void
        where f(args...) is void;

    using singleton<T> = struct{
        instantiate: ()=>_singleton_instantiate<T>,
    }
    {
        using _singleton_instantiate<T> = gchandle
        {
            extern("libjoyecs", "wojeapi_create_singleton")
            private func create<T>(token: string, f: ()=>T)=> _singleton_instantiate<T>;

            extern("libjoyecs", "wojeapi_apply_singleton")
            private func apply<T, R>(self: _singleton_instantiate<T>, f: (T)=> R)=> R;
        }
        public func create<T>(token: string, f: ()=>T)
        {
            return singleton:<T>{   
                instantiate = \ = _singleton_instantiate::create(token, f);,
            };
        }
        public func apply<T, R>(self: singleton<T>, f: (T)=> R)=> R
        {
            return self.instantiate()->apply(f);
        }

        namespace unsafe
        {
            extern("libjoyecs", "wojeapi_clear_singletons")
            public func clear_all()=> void;
        }
    }
    
    using thread = gchandle
    {
        namespace unsafe
        {
            extern("libjoyecs", "wojeapi_startup_thread")
            public func create<FT, ArgTs>(f: FT, args: ArgTs)=> thread
                where f(args...) is void;

            extern("libjoyecs", "wojeapi_abort_all_thread", slow)
            public func abort_all()=> void;
        }
        extern("libjoyecs", "wojeapi_wait_thread", slow)
        public func wait(self: thread)=> void;

        extern("libjoyecs", "wojeapi_check_thread", slow)
        public func finished(self: thread)=> bool;
    }

    extern("libjoyecs", "wojeapi_generate_uid")
        public func uid()=> string;

    extern("libjoyecs", "wojeapi_load_module")
    public func load_module(name: string, path: string)=> option<handle>;

    extern("libjoyecs", "wojeapi_unload_module")
    public func unload_module(module: handle)=> void;

    namespace file
    {
        extern("libjoyecs", "wojeapi_read_file_all", slow)
        public func readall(path: string)=> option<string>;
        namespace cache
        {
            extern("libjoyecs", "wojeapi_file_cache_write_all", slow)
            public func saveall(path: string, content: string)=> bool;
            extern("libjoyecs", "wojeapi_file_cache_read_all", slow)
            public func readall(path: string)=> option<string>;
        }
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

    public using typeinfo = handle
    {
        public func operator == (self: typeinfo, another: typeinfo)
        {
            return self->id() == another->id();
        }
        public func operator != (self: typeinfo, another: typeinfo)
        {
            return self->id() != another->id();
        }

        extern("libjoyecs", "wojeapi_type_is_component")
        public func is_component(self: typeinfo)=> ::bool;

        extern("libjoyecs", "wojeapi_type_is_system")
        public func is_system(self: typeinfo)=> ::bool;

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

        extern("libjoyecs", "wojeapi_type_of", repeat)
        public func load(name: ::string)=> option<typeinfo>;

        extern("libjoyecs", "wojeapi_type_of", repeat)
        public func loadid(id: ::int)=> option<typeinfo>;

        extern("libjoyecs", "wojeapi_type_id")
        public func id(self: typeinfo)=> ::int;

        extern("libjoyecs", "wojeapi_type_name")
        public func name(self: typeinfo)=> ::string;

        extern("libjoyecs", "wojeapi_script_type_name")
        public func script_name(self: typeinfo)=> option<::string>;

        enum basic_type
        {
            INT, INT2, BOOL, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT,
        }
        extern("libjoyecs", "wojeapi_type_basic_type")
        private func get_basic_type(tid: basic_type)=> typeinfo;

        extern("libjoyecs", "wojeapi_type_members")
        public func get_members_info(self: typeinfo)=> array<(::string, typeinfo)>;

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
        extern("libjoyecs", "wojeapi_graphic_shrink_cache", slow)
        public func shrink_cache(u: universe, target_count: int)=> void;

        public using vertex = gchandle
        {
            public enum type
            {
                LINES = 0,
                LINESTRIP,
                TRIANGLES,
                TRIANGLESTRIP,
            }

            extern("libjoyecs", "wojeapi_vertex_load", slow)
            public func load(univ: option<universe>, path: string)=> option<vertex>;

            extern("libjoyecs", "wojeapi_vertex_create", slow)
            public func create(vtype: type, vertices: array<real>, indexs: array<int>, indices: array<int>)=> vertex;

            extern("libjoyecs", "wojeapi_vertex_path")
            public func path(self: vertex)=> option<string>;
        }

        public using texture = gchandle
        {
            extern("libjoyecs", "wojeapi_texture_open", slow)
            public func load(univ: option<universe>, path: string)=> option<texture>;

            extern("libjoyecs", "wojeapi_texture_create", slow)
            public func create(width: int, height: int)=> texture;

             extern("libjoyecs", "wojeapi_texture_clip", slow)
            public func clip(src: texture, x: int, y: int, w: int, h: int)=> texture;

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
                integer2((int, int)),
                integer3((int, int, int)),  
                integer4((int, int, int, int)),
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

            extern("libjoyecs", "wojeapi_set_uniforms_int")
            func _set_uniform_int(shad: shader, name: string, val: int)=> void;
            extern("libjoyecs", "wojeapi_set_uniforms_int2")
            func _set_uniform_int2(shad: shader, name: string, x: int, y: int)=> void;
            extern("libjoyecs", "wojeapi_set_uniforms_int3")    
            func _set_uniform_int3(shad: shader, name: string, x: int, y: int, z: int)=> void;
            extern("libjoyecs", "wojeapi_set_uniforms_int4")
            func _set_uniform_int4(shad: shader, name: string, x: int, y: int, z: int, w: int)=> void;

            extern("libjoyecs", "wojeapi_set_uniforms_float")
            func _set_uniform_float(shad: shader, name: string, val: real)=> void;
            extern("libjoyecs", "wojeapi_set_uniforms_float2")
            func _set_uniform_float2(shad: shader, name: string, x: real, y: real)=> void;
            extern("libjoyecs", "wojeapi_set_uniforms_float3")
            func _set_uniform_float3(shad: shader, name: string, x: real, y: real, z: real)=> void;
            extern("libjoyecs", "wojeapi_set_uniforms_float4")
            func _set_uniform_float4(shad: shader, name: string, x: real, y: real, z: real, w: real)=> void;

            public func set_uniform<T>(self: shader, name: string, val: T)
                where val is int
                    || val is (int, int)
                    || val is (int, int, int)  
                    || val is (int, int, int, int)
                    || val is real
                    || val is (real, real)
                    || val is (real, real, real)
                    || val is (real, real, real, real);
            {
                if (val is int)
                    _set_uniform_int(self, name, val);
                if (val is (int, int))
                    _set_uniform_int2(self, name, val...);
                else if (val is (int, int, int))
                    _set_uniform_int3(self, name, val...);
                else if (val is (int, int, int, int))
                    _set_uniform_int4(self, name, val...);
                else if (val is real)
                    _set_uniform_float(self, name, val);
                else if (val is (real, real))
                    _set_uniform_float2(self, name, val...);
                else if (val is (real, real, real))
                    _set_uniform_float3(self, name, val...);
                else if (val is (real, real, real, real))
                    _set_uniform_float4(self, name, val...);
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
        public func create(u: universe)=> world;

        extern("libjoyecs", "wojeapi_close_world")
        public func close(self: world) => void;

        extern("libjoyecs", "wojeapi_set_able_world")
        public func set_able(self: world, able: bool)=> void;

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
        public func get_all_entities(self: world, components_require: array<typeinfo>)=> array<entity>;

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
        public func bind_texture(self: entity, id: int, tex: option<graphic::texture>)=> void;

        extern("libjoyecs", "wojeapi_set_shape_for_entity")
        public func set_shape(self: entity, shape: option<graphic::vertex>)=> void;

        extern("libjoyecs", "wojeapi_get_shape_of_entity")
        public func get_shape(self: entity)=> option<graphic::vertex>;

    } // end of namespace entity

    using component = struct{addr: handle, type: typeinfo}
    {
        extern("libjoyecs", "wojeapi_get_components_member")
        public func get_member(self: component, name: string)=> option<(typeinfo, handle)>;

        extern("libjoyecs", "wojeapi_component_get_all_members")
        public func get_members(self: component)=> array<(string, typeinfo, handle)>;
    }
}
)";
