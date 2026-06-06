#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <list>
#include <optional>

static std::atomic_flag _jewo_log_buffer_mx = {};
static std::list<std::pair<int, std::string>> _jewo_log_buffer;

WOORT_API woort_api wojeapi_get_current_platform(void)
{
    return woort_ret_int(JE4_CURRENT_PLATFORM);
}

WOORT_API woort_api wojeapi_generate_uid(void)
{
    jeecs::typing::uid_t::generate().JEParseToScriptType(WOORT_RETURN_SLOT);
    return woort_ret();
}

WOORT_API woort_api wojeapi_build_version(void)
{
#define JE_VERSION_WRAP(A, B, C) #A "." #B "." #C
    return woort_ret_string(JE_CORE_VERSION);
#undef JE_VERSION_WRAP
}

WOORT_API woort_api wojeapi_build_version_info(void)
{
    return woort_ret_string(je_build_version());
}

WOORT_API woort_api wojeapi_build_commit(void)
{
    return woort_ret_string(je_build_commit());
}

WOORT_API woort_api wojeapi_get_sleep_suppression(void)
{
    return woort_ret_real(je_clock_get_sleep_suppression());
}

WOORT_API woort_api wojeapi_read_file_all(void)
{
    const woort_U8CString filepath = woort_string(0);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        if (auto* file = jeecs_file_open(filepath))
        {
            std::vector<char> readed_buf(file->m_file_length);
            auto readed_len = jeecs_file_read(readed_buf.data(), sizeof(char), file->m_file_length, file);
            readed_buf.resize(readed_len);

            jeecs_file_close(file);

            (void)woort_vm_swap(last);
            return woort_ret_option_buffer(readed_buf.data(), readed_buf.size());
        }
    }
    (void)woort_vm_swap(last);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_file_cache_write_all(void)
{
    bool ok = false;

    const woort_U8CString filepath = woort_string(0);
    size_t buflen;
    const void* buf = woort_buffer(1, &buflen);

    woort_vm* const last = woort_vm_swap(nullptr);
    if (auto* cache = jeecs_create_cache_file(filepath, 0, 1))
    {
        auto written_len = jeecs_write_cache_file(buf, sizeof(char), buflen, cache);

        jeecs_close_cache_file(cache);

        if (buflen == written_len)
            ok = true;
    }
    (void)woort_vm_swap(last);

    return woort_ret_bool(ok);
}
WOORT_API woort_api wojeapi_file_cache_read_all(void)
{
    const woort_U8CString filepath = woort_string(0);

    woort_vm* const last = woort_vm_swap(nullptr);
    if (auto* cache = jeecs_load_cache_file(filepath, 0, -1))
    {
        std::vector<char> readed_buf(cache->m_file_length);
        auto readed_len = jeecs_file_read(readed_buf.data(), sizeof(char), cache->m_file_length, cache);
        readed_buf.resize(readed_len);

        jeecs_file_close(cache);

        (void)woort_vm_swap(last);

        return woort_ret_option_buffer(readed_buf.data(), readed_buf.size());
    }
    (void)woort_vm_swap(last);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_mark_shared_glresource_outdated(void)
{
    auto* uhost = jegl_uhost_get_or_create_for_universe(woort_pointer(0), nullptr);
    return woort_ret_bool(
        jegl_mark_shared_resources_outdated(
            jegl_uhost_get_context(uhost),
            woort_string(1)));
}

WOORT_API woort_api wojeapi_init_graphic_pipeline_for_editor(void)
{
    auto* uhost = jegl_uhost_get_or_create_for_universe(woort_pointer(0), nullptr);
    jegl_uhost_set_skip_behavior(uhost, false);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_runtime_path(void)
{
    jeecs_file_set_runtime_path(woort_string(0));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_create_fimg_packer(void)
{
    return woort_ret_pointer(jeecs_file_image_begin(
        woort_string(0),
        (size_t)woort_int(1)));
}

WOORT_API woort_api wojeapi_pack_file_to_fimg_packer(void)
{
    auto* ctx = static_cast<fimg_creating_context*>(woort_pointer(0));
    return woort_ret_bool(jeecs_file_image_pack_file(ctx, woort_string(1), woort_string(2)));
}

WOORT_API woort_api wojeapi_pack_buffer_to_fimg_packer(void)
{
    auto* ctx = static_cast<fimg_creating_context*>(woort_pointer(0));
    return woort_ret_bool(
        jeecs_file_image_pack_buffer(
            ctx,
            woort_pointer(1),
            (size_t)woort_int(2),
            woort_string(3)));
}

WOORT_API woort_api wojeapi_finish_fimg_packer(void)
{
    auto* ctx = static_cast<fimg_creating_context*>(woort_pointer(0));
    jeecs_file_image_finish(ctx);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_sleep_suppression(void)
{
    je_clock_set_sleep_suppression(woort_real(0));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_woolang_version(void)
{
    std::string woolang_version_info = "Woolang ";
    return woort_ret_string((woolang_version_info + wo_version() + " " + wo_compile_date()).c_str());
}

WOORT_API woort_api wojeapi_crc64_file(void)
{
    const uint64_t result = wo_crc64_file_from_path(woort_string(0));
    if (result)
        return woort_ret_option_int(result);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_crc64_string(void)
{
    const uint64_t result = wo_crc64_str(woort_string(0));
    return woort_ret_int(result);
}

WOORT_API woort_api wojeapi_register_log_callback(void)
{
    std::function<void(int, const char*)>* callbacks =
        new std::function<void(int, const char*)>([&](int level, const char* msg)
            {
                while (_jewo_log_buffer_mx.test_and_set());
                _jewo_log_buffer.push_back({ level, msg });
                _jewo_log_buffer_mx.clear(); });

    return woort_ret_int(
        static_cast<woort_Int>(
            je_log_register_callback(
                [](int level, const char* msg, void* func)
                {
                    (*(std::function<void(int, const char*)> *)func)(level, msg);
                },
                callbacks)));
}

WOORT_API woort_api wojeapi_unregister_log_callback(void)
{
    auto func = static_cast<std::function<void(int, const char*)> *>(
        je_log_unregister_callback(
            static_cast<je_log_regid_t>(woort_int(0))));
    delete func;

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_all_logs(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(result);

    std::list<std::pair<int, std::string>> logs;

    while (_jewo_log_buffer_mx.test_and_set())
        ;
    logs.swap(_jewo_log_buffer);
    assert(_jewo_log_buffer.empty());
    _jewo_log_buffer_mx.clear();

    for (auto& [i, s] : logs)
    {
        woort_set_struct(elem, 2);

        woort_struct_set_int(elem, 0, i);
        woort_struct_set_string(elem, 1, s.c_str());

        woort_vec_push(result, elem);
    }
    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_load_module(void)
{
    auto* const dylib_handle = je_module_load(woort_string(0), woort_string(1));

    if (dylib_handle == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(dylib_handle);
}

WOORT_API woort_api wojeapi_unload_module(void)
{
    je_module_unload(static_cast<woort_Dylib*>(woort_pointer(0)));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_apply_camera_framebuf_setting(void)
{
    jeecs::game_entity* const entity = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    if (jeecs::Camera::RendToFramebuffer* rbf = entity->get_component<jeecs::Camera::RendToFramebuffer>())
    {
        rbf->framebuffer = jeecs::graphic::framebuffer::create(
            (size_t)woort_int(1),
            (size_t)woort_int(2),
            {
                jegl_texture::format::RGBA,
            },
            true);
    }
    else
        jeecs::debug::logfatal("No RendToFramebuffer in specify entity when 'wojeapi_apply_camera_framebuf_setting'.");
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_framebuf_texture(void)
{
    jeecs::game_entity* const entity = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    if (jeecs::Camera::RendToFramebuffer* rbf = entity->get_component<jeecs::Camera::RendToFramebuffer>())
    {
        if (!rbf->framebuffer.has_value())
            return woort_ret_option_none();

        auto tex = rbf->framebuffer.value()->get_attachment((size_t)woort_int(1));
        if (tex.has_value())
            return woort_ret_option_gchandle(
                new jeecs::basic::resource<jeecs::graphic::texture>(tex.value()),
                WOORT_IGNORE,
                [](void* ptr)
                {
                    delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
                },
                nullptr);

        jeecs::debug::logerr("RendToFramebuffer(%p).framebuffer not contain attach(%zu) in entity when 'wojeapi_get_framebuf_texture'.",
            rbf, (size_t)woort_int(1));
        return woort_ret_option_none();
    }
    else
        jeecs::debug::logerr("No RendToFramebuffer in specify entity when 'wojeapi_get_framebuf_texture'.");
    return woort_ret_option_none();
}

// ECS UNIVERSE
WOORT_API woort_api wojeapi_create_universe(void)
{
    return woort_ret_pointer(je_ecs_universe_create());
}

WOORT_API woort_api wojeapi_close_universe(void)
{
    je_ecs_universe_destroy(woort_pointer(0));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_universe_from_world(void)
{
    void* universe = je_ecs_world_in_universe(woort_pointer(0));
    return woort_ret_pointer(universe);
}

WOORT_API woort_api wojeapi_universe_extend_life(void)
{
    jeecs::game_universe(woort_pointer(0)).grow();
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_universe_reduce_life(void)
{
    jeecs::game_universe(woort_pointer(0)).trim();
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_universe_wait(void)
{
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        jeecs::game_universe(woort_pointer(0)).wait();
    }
    (void)woort_vm_swap(last);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_universe_get_frame_deltatime(void)
{
    return woort_ret_real(je_ecs_universe_get_frame_deltatime(woort_pointer(0)));
}

WOORT_API woort_api wojeapi_universe_set_frame_deltatime(void)
{
    je_ecs_universe_set_frame_deltatime(woort_pointer(0), woort_real(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_universe_get_max_deltatime(void)
{
    return woort_ret_real(je_ecs_universe_get_max_deltatime(woort_pointer(0)));
}
WOORT_API woort_api wojeapi_universe_set_max_deltatime(void)
{
    je_ecs_universe_set_max_deltatime(woort_pointer(0), woort_real(1));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_universe_get_timescale(void)
{
    return woort_ret_real(je_ecs_universe_get_time_scale(woort_pointer(0)));
}
WOORT_API woort_api wojeapi_universe_set_timescale(void)
{
    je_ecs_universe_set_time_scale(woort_pointer(0), woort_real(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_create_world_in_universe(void)
{
    return woort_ret_pointer(
        jeecs::game_universe(woort_pointer(0)).create_world().handle());
}

WOORT_API woort_api wojeapi_get_all_worlds_in_universe(void)
{
    void* universe = woort_pointer(0);

    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);

    auto result = jedbg_get_all_worlds_in_universe(universe);
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
    jeecs::game_world(woort_pointer(0)).close();
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_able_world(void)
{
    jeecs::game_world(woort_pointer(0)).set_able(woort_bool(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_world_name(void)
{
    return woort_ret_string(jedbg_get_world_name(woort_pointer(0)));
}

WOORT_API woort_api wojeapi_set_world_name(void)
{
    jedbg_set_world_name(woort_pointer(0), woort_string(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_add_system_to_world(void)
{
    /*
    extern("libjoyecs", "wojeapi_add_system_to_world")
    func add_system(self: world, systype : typeinfo) = > bool;
    */
    jeecs::game_world gworld(woort_pointer(0));
    const jeecs::typing::type_info* system_type =
        static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    bool added;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        added = gworld.add_system(system_type->m_id);
    }
    (void)woort_vm_swap(last);

    return woort_ret_bool(added);
}

WOORT_API woort_api wojeapi_get_system_from_world(void)
{
    jeecs::game_world gworld(woort_pointer(0));
    const jeecs::typing::type_info* system_type =
        static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    auto* const system_addr = gworld.get_system(system_type->m_id);

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
    jeecs::game_world gworld(woort_pointer(0));
    const jeecs::typing::type_info* system_type =
        static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    gworld.remove_system(system_type->m_id);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_all_systems_from_world(void)
{
    /*
    extern("libjoyecs", "wojeapi_get_all_systems_from_world")
    private func _get_systems_from_world(self: world, out_result: array<typeinfo>)=> array<typeinfo>;
    */
    const jeecs::typing::type_info** types =
        jedbg_get_all_system_attached_in_world(woort_pointer(0));

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
    jeecs::game_world gworld = woort_pointer(0);
    woort_value components_list = 1;

    std::vector<jeecs::typing::typeid_t> components;

    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value elem = s + 0;

    const size_t component_count = woort_vec_len(components_list);
    for (size_t i = 0; i < component_count; ++i)
    {
        (void)woort_vec_get(elem, components_list, i);
        components.push_back(
            static_cast<const jeecs::typing::type_info*>(woort_unbox_pointer(elem))->m_id);
    }

    return woort_ret_gchandle(
        new jeecs::game_entity(gworld._add_entity(components)),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::game_entity*)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_add_entity_to_world_with_prefab(void)
{
    jeecs::game_world gworld(woort_pointer(0));
    jeecs::game_entity* const prefab_entity = static_cast<jeecs::game_entity*>(woort_gcpointer(1));

    return woort_ret_gchandle(
        new jeecs::game_entity(gworld.add_entity(*prefab_entity)),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::game_entity*)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_add_prefab_to_world_with_components(void)
{
    jeecs::game_world gworld(woort_pointer(0));
    woort_value components_list = 1;

    std::vector<jeecs::typing::typeid_t> components;

    woort_value s;

    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value elem = s + 0;
    const size_t component_count = woort_vec_len(components_list);
    for (size_t i = 0; i < component_count; ++i)
    {
        (void)woort_vec_get(elem, components_list, i);
        components.push_back(
            static_cast<const jeecs::typing::type_info*>(woort_unbox_pointer(elem))->m_id);
    }

    return woort_ret_gchandle(
        new jeecs::game_entity(gworld._add_prefab(components)),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::game_entity*)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_get_all_entities_from_world(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_arr = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_arr);

    void* world_instance = woort_pointer(0);

    auto required_components_len = woort_vec_len(1);
    std::vector<jeecs::typing::typeid_t> required_components(required_components_len);

    for (size_t i = 0; i < required_components_len; ++i)
    {
        (void)woort_vec_get(elem, 1, i);

        required_components[i] =
            static_cast<const jeecs::typing::type_info*>(woort_unbox_pointer(elem))->m_id;
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
            woort_set_gchandle(
                elem,
                current_e,
                WOORT_IGNORE,
                [](void* entity_ptr)
                {
                    jedbg_free_entity((jeecs::game_entity*)entity_ptr);
                },
                nullptr);
            woort_vec_push(out_arr, elem);
        }
    }
    je_mem_free(entities);

    return woort_ret_value(out_arr);
}

// ECS ENTITY
WOORT_API woort_api wojeapi_close_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    entity->close();
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_world_from_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    void* world = je_ecs_world_of_entity(entity);
    return woort_ret_pointer(world);
}

WOORT_API woort_api wojeapi_set_editing_entity_uid(void)
{
    jedbg_set_editing_entity_uid((jeecs::typing::debug_eid_t)woort_int(0));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_reset_editing_entity_uid(void)
{
    jedbg_set_editing_entity_uid(0);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_editing_entity_uid(void)
{
    jeecs::typing::debug_eid_t uid = jedbg_get_editing_entity_uid();

    if (uid != 0)
        return woort_ret_option_int((woort_Int)uid);
    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_get_entity_uid(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    jeecs::typing::debug_eid_t uid = jedbg_get_entity_uid(entity);

    if (uid != 0)
        return woort_ret_option_int((woort_Int)uid);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_get_entity_anchor_uuid(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    if (auto* anc = entity->get_component<jeecs::Transform::Anchor>())
    {
        anc->uid.JEParseToScriptType(WOORT_RETURN_SLOT);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }

    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_get_parent_anchor_uid(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    if (auto* l2p = entity->get_component<jeecs::Transform::LocalToParent>())
    {
        l2p->parent_uid.JEParseToScriptType(WOORT_RETURN_SLOT);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_set_parent(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    jeecs::game_entity* parent = (jeecs::game_entity*)woort_gcpointer(1);
    bool force = woort_bool(2);

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
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}

WOORT_API woort_api wojeapi_set_parent_with_uid(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    bool force = woort_bool(2);

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

        l2p->parent_uid.JEParseFromScriptType(1);
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}

WOORT_API woort_api wojeapi_get_entity_name(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    return woort_ret_string(je_ecs_get_name_of_entity(entity));
}

WOORT_API woort_api wojeapi_set_entity_name(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    je_ecs_set_name_of_entity(entity, woort_string(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_entity_chunk_info(void)
{
    char buf[64];
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    int result = snprintf(buf, sizeof(buf), "[%p:%uv%u]", entity->_m_in_chunk, entity->_m_id, entity->_m_version);
    assert(result > 0 && result < (int)sizeof(buf));
    (void)result;

    return woort_ret_string(buf);
}

WOORT_API woort_api wojeapi_find_entity_with_chunk_info(void)
{
    jeecs::game_entity* entity = new jeecs::game_entity();
    ((void)sscanf(woort_string(0), "[%p:%uv%u]", &entity->_m_in_chunk, &entity->_m_id, &entity->_m_version));

    return woort_ret_gchandle(
        entity,
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::game_entity*)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_get_all_components_types_from_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_arr = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_arr);

    auto types = jedbg_get_all_components_from_entity(entity);
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
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    auto* const component_addr =
        je_ecs_world_entity_get_component(entity,
            ((const jeecs::typing::type_info*)woort_pointer(1))->m_id);

    if (component_addr == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(component_addr);
}

WOORT_API woort_api wojeapi_add_component_from_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    return woort_ret_pointer(je_ecs_world_entity_add_component(entity,
        ((const jeecs::typing::type_info*)woort_pointer(1))->m_id));
}

WOORT_API woort_api wojeapi_remove_component_from_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    je_ecs_world_entity_remove_component(
        entity, ((const jeecs::typing::type_info*)woort_pointer(1))->m_id);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_is_top_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    return woort_ret_bool(nullptr == entity->get_component<jeecs::Transform::LocalToParent>());
}

WOORT_API woort_api wojeapi_is_child_of_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    jeecs::game_entity* parent = (jeecs::game_entity*)woort_gcpointer(1);

    jeecs::Transform::LocalToParent* l2p = entity->get_component<jeecs::Transform::LocalToParent>();
    jeecs::Transform::Anchor* archor = parent->get_component<jeecs::Transform::Anchor>();

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
    const jeecs::typing::type_info* component_type =
        (const jeecs::typing::type_info*)woort_pointer(elem);

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
    const jeecs::typing::type_info* component_type =
        (const jeecs::typing::type_info*)woort_pointer(elem);

    const woort_U8CString member_name = woort_string(1);

    if (auto* member_info = component_type->find_member_by_name(member_name))
    {
        woort_set_struct(result, 2);

        woort_set_pointer(elem, member_info->m_member_type);
        woort_struct_set(result, 0, elem);
        woort_set_pointer(elem, (member_info->m_member_offset + (intptr_t)component_addr));
        woort_struct_set(result, 1, elem);

        return woort_ret_option_value(result);
    }
    else
        return woort_ret_option_none();
}

// INPUTS
WOORT_API woort_api wojeapi_input_keydown(void)
{
    return woort_ret_bool(jeecs::input::keydown((jeecs::input::keycode)woort_int(0)));
}

WOORT_API woort_api wojeapi_input_mousedown(void)
{
    return woort_ret_bool(jeecs::input::mousedown(
        (size_t)woort_int(0), (jeecs::input::mousecode)woort_int(1)));
}

WOORT_API woort_api wojeapi_wheel_count(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto wheel = jeecs::input::wheel((size_t)woort_int(0));

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_struct(result, 2);

    woort_struct_set_float(result, 0, wheel.x);
    woort_struct_set_float(result, 1, wheel.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_window_size(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto winsz = jeecs::input::windowsize();

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_struct(result, 2);

    woort_struct_set_int(result, 0, (woort_Int)winsz.x);
    woort_struct_set_int(result, 1, (woort_Int)winsz.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_window_pos(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto winpos = jeecs::input::windowpos();

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_struct(result, 2);

    woort_struct_set_int(result, 0, (woort_Int)winpos.x);
    woort_struct_set_int(result, 1, (woort_Int)winpos.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_mouse_pos(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto mousepos = jeecs::input::mousepos((size_t)woort_int(0));

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_struct(result, 2);

    woort_struct_set_int(result, 0, (woort_Int)mousepos.x);
    woort_struct_set_int(result, 1, (woort_Int)mousepos.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_mouse_view_pos(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto mouseview = jeecs::input::mouseviewpos((size_t)woort_int(0));

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_struct(result, 2);

    woort_struct_set_float(result, 0, mouseview.x);
    woort_struct_set_float(result, 1, mouseview.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_gamepad_button(void)
{
    jeecs::input::gamepad* gamepad =
        (jeecs::input::gamepad*)woort_gcpointer(0);
    jeecs::input::gamepadcode kcode =
        (jeecs::input::gamepadcode)woort_int(1);

    return woort_ret_bool(gamepad->button(kcode));
}
WOORT_API woort_api wojeapi_input_gamepad_axis(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    jeecs::input::gamepad* gamepad =
        (jeecs::input::gamepad*)woort_gcpointer(0);
    jeecs::input::joystickcode kcode =
        (jeecs::input::joystickcode)woort_int(1);

    auto axis = gamepad->stick(kcode);

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_struct(result, 2);

    woort_struct_set_float(result, 0, axis.x);
    woort_struct_set_float(result, 1, axis.y);

    return woort_ret_value(result);
}
WOORT_API woort_api wojeapi_input_gamepad_actived(void)
{
    jeecs::input::gamepad* gamepad =
        (jeecs::input::gamepad*)woort_gcpointer(0);

    jeecs::typing::timestamp_ms_t actived;
    if (gamepad->actived(&actived))
        return woort_ret_option_int(actived);

    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_input_gamepad_get_all(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto gamepads = jeecs::input::gamepad::all();

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(result);
    for (auto& gamepad : gamepads)
    {
        woort_set_gchandle(
            elem,
            new jeecs::input::gamepad(gamepad),
            WOORT_IGNORE,
            [](void* p)
            {
                delete (jeecs::input::gamepad*)p;
            },
            nullptr);
        woort_vec_push(result, elem);
    }

    return woort_ret_value(result);
}
WOORT_API woort_api wojeapi_input_gamepad_last(void)
{
    auto gamepad = jeecs::input::gamepad::last();
    if (gamepad.has_value())
    {
        return woort_ret_option_gchandle(
            new jeecs::input::gamepad(gamepad.value()),
            WOORT_IGNORE,
            [](void* p)
            {
                delete (jeecs::input::gamepad*)p;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_input_update_window_size(void)
{
    je_io_set_window_size((int)woort_int(0), (int)woort_int(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_input_update_window_title(void)
{
    je_io_set_window_title(woort_string(0));
    return woort_ret_void();
}

// ECS OTHER
WOORT_API woort_api wojeapi_log(void)
{
    size_t argc = (size_t)woort_int(0);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
    {
        char* str = woort_serialize_dynbox(i + 1, WOORT_SERIALIZE_FLAG_NONE);
        disp += str;
        woort_free(str);
    }

    jeecs::debug::log("%s", disp.c_str());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_loginfo(void)
{
    size_t argc = (size_t)woort_int(0);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
    {
        char* str = woort_serialize_dynbox(i + 1, WOORT_SERIALIZE_FLAG_NONE);
        disp += str;
        woort_free(str);
    }

    jeecs::debug::loginfo("%s", disp.c_str());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_logwarn(void)
{
    size_t argc = (size_t)woort_int(0);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
    {
        char* str = woort_serialize_dynbox(i + 1, WOORT_SERIALIZE_FLAG_NONE);
        disp += str;
        woort_free(str);
    }

    jeecs::debug::logwarn("%s", disp.c_str());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_logerr(void)
{
    size_t argc = (size_t)woort_int(0);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
    {
        char* str = woort_serialize_dynbox(i + 1, WOORT_SERIALIZE_FLAG_NONE);
        disp += str;
        woort_free(str);
    }

    jeecs::debug::logerr("%s", disp.c_str());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_logfatal(void)
{
    size_t argc = (size_t)woort_int(0);
    std::string disp;

    for (size_t i = 0; i < argc; i++)
    {
        char* str = woort_serialize_dynbox(i + 1, WOORT_SERIALIZE_FLAG_NONE);
        disp += str;
        woort_free(str);
    }

    jeecs::debug::logfatal("%s", disp.c_str());
    return woort_ret_void();
}

// ECS TYPEINFO
WOORT_API woort_api wojeapi_type_of_name(void)
{
    auto* const type_info =
        jeecs::typing::type_info::of(woort_string(0));

    if (type_info == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(type_info);
}

WOORT_API woort_api wojeapi_type_of_id(void)
{
    auto* const type_info =
        jeecs::typing::type_info::of((jeecs::typing::typeid_t)woort_int(0));

    if (type_info == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(type_info);
}


WOORT_API woort_api wojeapi_get_all_registed_types(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);

    auto** types = jedbg_get_all_registed_types();

    auto** cur_type = types;
    while (*cur_type)
    {
        woort_set_box_pointer(elem, (void*)*(cur_type++));
        woort_vec_push(out_array, elem);
    }

    je_mem_free(types);
    return woort_ret_value(out_array);
}

WOORT_API woort_api wojeapi_type_is_component(void)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)woort_pointer(0);
    return woort_ret_bool(type->m_type_class == je_typing_class::JE_COMPONENT);
}

WOORT_API woort_api wojeapi_type_is_system(void)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)woort_pointer(0);
    return woort_ret_bool(type->is_system());
}

WOORT_API woort_api wojeapi_type_id(void)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)woort_pointer(0);
    return woort_ret_int(type->m_id);
}

WOORT_API woort_api wojeapi_type_name(void)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)woort_pointer(0);
    return woort_ret_string(type->m_typename);
}
WOORT_API woort_api wojeapi_script_type_name(void)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)woort_pointer(0);
    auto* parser = type->get_script_parser();
    if (parser != nullptr)
        return woort_ret_option_string(parser->m_woolang_typename);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_type_members(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)woort_pointer(0);

    const woort_value result = s + 0;
    const woort_value elem = s + 1;
    const woort_value elem2 = s + 2;

    woort_set_vec(result);

    if (type->m_member_types != nullptr)
    {
        auto* member_iter = type->m_member_types->m_members;
        while (member_iter != nullptr)
        {
            woort_set_struct(elem, 2);

            woort_set_string(elem2, member_iter->m_member_name);
            woort_struct_set(elem, 0, elem2);
            woort_set_pointer(elem2, (void*)member_iter->m_member_type);
            woort_struct_set(elem, 1, elem2);

            woort_vec_push(result, elem);

            member_iter = member_iter->m_next_member;
        }
    }

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_type_basic_type(void)
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
    basic_type type = (basic_type)woort_int(0);

    switch (type)
    {
    case INT:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<int>());
    case INT2:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<jeecs::math::ivec2>());
    case BOOL:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<bool>());
    case FLOAT:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<float>());
    case FLOAT2:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<jeecs::math::vec2>());
    case FLOAT3:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<jeecs::math::vec3>());
    case FLOAT4:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<jeecs::math::vec4>());
    case STRING:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<jeecs::basic::string>());
    case QUAT:
        return woort_ret_pointer((void*)jeecs::typing::type_info::of<jeecs::math::quat>());
    default:
        return woort_ret_panic("Unknown basic type.");
    }
}

///////////////////////////////////////////////////////////////////////

WOORT_API woort_api wojeapi_texture_open(void)
{
    const woort_value universe_ptr = WOORT_RETURN_SLOT;

    void* universe_addr = nullptr;
    if (woort_option_get(universe_ptr, 0))
        universe_addr = woort_pointer(universe_ptr);

    std::optional<jeecs::basic::resource<jeecs::graphic::texture>> loaded_texture;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        jegl_context* gcontext = nullptr;
        if (universe_addr != nullptr)
        {
            gcontext = jegl_uhost_get_context(
                jegl_uhost_get_or_create_for_universe(universe_addr, nullptr));
        }
        loaded_texture = jeecs::graphic::texture::load(gcontext, woort_string(1));
    }
    (void)woort_vm_swap(last);

    if (loaded_texture.has_value())
        return woort_ret_option_gchandle(
            new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
            },
            nullptr);

    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_texture_create(void)
{
    const size_t w = (size_t)woort_int(0);
    const size_t h = (size_t)woort_int(1);

    auto loaded_texture = jeecs::graphic::texture::create(
        w, h, jegl_texture::format::RGBA);

    return woort_ret_gchandle(
        new jeecs::basic::resource<jeecs::graphic::texture>(loaded_texture),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
        },
        nullptr);
}
WOORT_API woort_api wojeapi_texture_clip(void)
{
    auto* loaded_texture =
        (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    return woort_ret_gchandle(
        new jeecs::basic::resource<jeecs::graphic::texture>(
            jeecs::graphic::texture::clip(
                *loaded_texture,
                (size_t)woort_int(1),
                (size_t)woort_int(2),
                (size_t)woort_int(3),
                (size_t)woort_int(4))),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_texture_bind_path(void)
{
    auto* loaded_texture =
        (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    auto& path = (*loaded_texture)->resource()->m_handle.m_path_may_null_if_builtin;
    if (path != nullptr)
        je_mem_free((void*)path);

    path = jeecs::basic::make_new_string(woort_string(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_texture_get_pixel(void)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    const woort_value elem = WOORT_RETURN_SLOT;

    const size_t x = woort_struct_get_int(1, 0);
    const size_t y = woort_struct_get_int(1, 1);

    auto* pix = new jeecs::graphic::texture::pixel((*loaded_texture)->resource(), x, y);

    return woort_ret_gchandle(
        pix,
        0,
        [](void* ptr)
        {
            delete (jeecs::graphic::texture::pixel*)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_texture_take_snapshot(void)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    auto tex_raw = loaded_texture->get()->resource();
    if (tex_raw->m_pixels)
    {
        auto memsz = tex_raw->m_width * tex_raw->m_height *
            (tex_raw->m_format & jegl_texture::format::COLOR_DEPTH_MASK);
        if (memsz > 0)
        {
            auto* membuf = malloc(memsz);
            memcpy(membuf, tex_raw->m_pixels, memsz);
            return woort_ret_result_ok_gchandle(
                membuf,
                WOORT_IGNORE,
                [](void* buf)
                {
                    free(buf);
                },
                nullptr);
        }
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_texture_restore_snapshot(void)
{
    auto* loaded_texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);
    auto* texture_buf = woort_gcpointer(1);

    auto tex_raw = loaded_texture->get()->resource();
    if (tex_raw->m_pixels)
    {
        auto memsz = tex_raw->m_width * tex_raw->m_height *
            (tex_raw->m_format & jegl_texture::format::COLOR_DEPTH_MASK);
        memcpy(tex_raw->m_pixels, texture_buf, memsz);

        return woort_ret_bool(true);
    }
    return woort_ret_bool(false);
}

WOORT_API woort_api wojeapi_texture_pixel_color(void)
{
    auto* pix = (jeecs::graphic::texture::pixel*)woort_gcpointer(0);
    auto color = pix->get();

    woort_set_struct(WOORT_RETURN_SLOT, 4);

    woort_struct_set_float(WOORT_RETURN_SLOT, 0, color.x);
    woort_struct_set_float(WOORT_RETURN_SLOT, 1, color.y);
    woort_struct_set_float(WOORT_RETURN_SLOT, 2, color.z);
    woort_struct_set_float(WOORT_RETURN_SLOT, 3, color.w);

    return woort_ret();
}
WOORT_API woort_api wojeapi_texture_set_pixel_color(void)
{
    auto* pix = (jeecs::graphic::texture::pixel*)woort_gcpointer(0);
    auto color = jeecs::math::vec4();

    color.x = woort_struct_get_float(1, 0);
    color.y = woort_struct_get_float(1, 1);
    color.z = woort_struct_get_float(1, 2);
    color.w = woort_struct_get_float(1, 3);

    pix->set(color);

    return woort_ret_void();
}
/////////////////////////////////////////////////////////////
WOORT_API woort_api wojeapi_font_open(void)
{
    std::optional<jeecs::basic::resource<jeecs::graphic::font>> loaded_font;

    const woort_U8CString font_path = woort_string(0);
    const size_t font_size = (size_t)woort_int(1);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_font = jeecs::graphic::font::load(
            font_path,
            font_size);
    }
    (void)woort_vm_swap(last);

    if (loaded_font.has_value())
    {
        return woort_ret_option_gchandle(
            new jeecs::basic::resource<jeecs::graphic::font>(loaded_font.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::font> *)ptr;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_font_load_char(void)
{
    auto* loaded_font =
        reinterpret_cast<jeecs::basic::resource<jeecs::graphic::font> *>(
            woort_gcpointer(0));
    assert(loaded_font != nullptr);

    const jeecs::graphic::character* ch =
        loaded_font->get()->get_character(
            static_cast<woort_Char>(woort_int(1)));

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

    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_struct(result, 8);

    woort_set_gchandle(
        elem,
        new jeecs::basic::resource<jeecs::graphic::texture>(ch->m_texture),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
        },
        nullptr);

    woort_struct_set(result, 0, elem);

    woort_struct_set_int(result, 1, (woort_Int)ch->m_char);

    woort_struct_set_int(result, 2, ch->m_width);
    woort_struct_set_int(result, 3, ch->m_height);

    woort_struct_set_int(result, 4, ch->m_advance_x);
    woort_struct_set_int(result, 5, ch->m_advance_y);

    woort_struct_set_int(result, 6, ch->m_baseline_offset_x);
    woort_struct_set_int(result, 7, ch->m_baseline_offset_y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_font_string_texture(void)
{
    auto* loaded_font =
        static_cast<jeecs::basic::resource<jeecs::graphic::font> *>(
            woort_gcpointer(0));

    jeecs::basic::resource<jeecs::graphic::texture>* text_texture;

    const woort_U8CString text = woort_string(1);

    std::u32string wstr;
    wstr.resize(woort_str_to_u32str(text, nullptr, 0));
    (void)woort_str_to_u32str(text, wstr.data(), wstr.size());

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        text_texture =
            new jeecs::basic::resource<jeecs::graphic::texture>(
                loaded_font->get()->u32text_texture(wstr));
    }
    (void)woort_vm_swap(last);

    return woort_ret_gchandle(
        text_texture,
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::texture> *)ptr;
        },
        nullptr);
}

/////////////////////////////////////////////////////////////
WOORT_API woort_api wojeapi_shader_open(void)
{
    jegl_context* gcontext = nullptr;

    const woort_value universe_ptr = WOORT_RETURN_SLOT;
    if (woort_option_get(WOORT_RETURN_SLOT, 0))
    {
        gcontext = jegl_uhost_get_context(
            jegl_uhost_get_or_create_for_universe(
                woort_pointer(universe_ptr), nullptr));
    }

    std::optional<jeecs::basic::resource<jeecs::graphic::shader>> loaded_shader;

    const woort_U8CString path = woort_string(1);
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_shader = jeecs::graphic::shader::load(gcontext, path);
    }
    (void)woort_vm_swap(last);

    if (loaded_shader.has_value())
    {
        return woort_ret_option_gchandle(
            new jeecs::basic::resource<jeecs::graphic::shader>(loaded_shader.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_shader_create(void)
{
    jegl_context* gcontext = nullptr;

    const woort_value universe_ptr = WOORT_RETURN_SLOT;
    if (woort_option_get(universe_ptr, 0))
    {
        gcontext = jegl_uhost_get_context(
            jegl_uhost_get_or_create_for_universe(
                woort_pointer(universe_ptr), nullptr));
    }

    std::optional<jeecs::basic::resource<jeecs::graphic::shader>> loaded_shader;
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_shader = jeecs::graphic::shader::create(
            gcontext, woort_string(1), woort_string(2));
    }
    (void)woort_vm_swap(last);

    if (loaded_shader.has_value())
    {
        return woort_ret_gchandle(
            new jeecs::basic::resource<jeecs::graphic::shader>(loaded_shader.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_textures_of_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_map = s + 0;
    const woort_value key = s + 1;
    const woort_value val = s + 2;

    woort_set_map(out_map);

    if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
    {
        for (auto& texture : textures->textures)
        {
            woort_set_gchandle(val,
                new jeecs::basic::resource<jeecs::graphic::texture>(texture.m_texture),
                WOORT_IGNORE,
                [](void* ptr)
                {
                    delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
                },
                nullptr);
            woort_map_set_by_int(out_map, (woort_Int)texture.m_pass_id, val);
        }
    }

    return woort_ret_value(out_map);
}

WOORT_API woort_api wojeapi_bind_texture_for_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    if (jeecs::Renderer::Textures* textures = entity->get_component<jeecs::Renderer::Textures>())
    {
        if (woort_option_get(WOORT_RETURN_SLOT, 2))
        {
            textures->bind_texture(
                (size_t)woort_int(1),
                *(jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(WOORT_RETURN_SLOT));
        }
        else
        {
            textures->remove_texture((size_t)woort_int(1));
        }
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Textures组件，在此panic?

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_shape_for_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    if (jeecs::Renderer::Shape* shape = entity->get_component<jeecs::Renderer::Shape>())
    {
        if (woort_option_get(WOORT_RETURN_SLOT, 1))
            shape->vertex.emplace(*(jeecs::basic::resource<jeecs::graphic::vertex> *)woort_pointer(WOORT_RETURN_SLOT));
        else
            shape->vertex.reset();
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Textures组件，在此panic?

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_shape_of_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    if (jeecs::Renderer::Shape* shape = entity->get_component<jeecs::Renderer::Shape>())
    {
        if (shape->vertex.has_value())
            return woort_ret_option_pointer(
                new jeecs::basic::resource<jeecs::graphic::vertex>(shape->vertex.value()));
    }

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_vertex_load(void)
{
    std::optional<jeecs::basic::resource<jeecs::graphic::vertex>> loaded_vertex;

    const woort_value universe_ptr = WOORT_RETURN_SLOT;
    jegl_context* gcontext = nullptr;
    if (woort_option_get(universe_ptr, 0))
    {
        gcontext = jegl_uhost_get_context(
            jegl_uhost_get_or_create_for_universe(
                woort_pointer(universe_ptr), nullptr));
    }

    const woort_U8CString path = woort_string(1);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_vertex = jeecs::graphic::vertex::load(gcontext, path);
    }
    (void)woort_vm_swap(last);

    if (loaded_vertex.has_value())
        return woort_ret_option_gchandle(
            new jeecs::basic::resource<jeecs::graphic::vertex>(loaded_vertex.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::vertex> *)ptr;
            },
            nullptr);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_vertex_create(void)
{
    // vertices: array<real>, indices: array<int>
    jegl_vertex::type vertex_type = (jegl_vertex::type)woort_int(0);
    std::vector<float> vertices(woort_vec_len(1));
    std::vector<uint32_t> indices(woort_vec_len(2));
    std::vector<jegl_vertex::data_layout> formats(woort_vec_len(3));

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 1, i);
        vertices[i] = woort_unbox_float(WOORT_RETURN_SLOT);
    }
    for (size_t i = 0; i < indices.size(); ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 2, i);
        indices[i] = (uint32_t)woort_unbox_int(WOORT_RETURN_SLOT);
    }
    for (size_t i = 0; i < formats.size(); ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 3, i);
        formats[i] = {
            jegl_vertex::data_type::FLOAT32,
            (size_t)woort_unbox_int(WOORT_RETURN_SLOT)
        };
    }

    std::optional<jeecs::basic::resource<jeecs::graphic::vertex>> loaded_vertex;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_vertex = jeecs::graphic::vertex::create(
            vertex_type,
            vertices.data(),
            vertices.size() * sizeof(float),
            indices,
            formats);
    }
    (void)woort_vm_swap(last);

    if (loaded_vertex.has_value())
        return woort_ret_gchandle(
            new jeecs::basic::resource<jeecs::graphic::vertex>(loaded_vertex.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::vertex> *)ptr;
            },
            nullptr);

    return woort_ret_panic("Bad vertex format.");
}

WOORT_API woort_api wojeapi_vertex_path(void)
{
    auto* loaded_vertex =
        (jeecs::basic::resource<jeecs::graphic::vertex> *)woort_gcpointer(0);

    if (auto path = (*loaded_vertex)->resource()->m_handle.m_path_may_null_if_builtin)
        return woort_ret_option_string(path);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_shaders_of_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);

    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);

    if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
    {
        for (auto& shader : shaders->shaders)
        {
            woort_set_gchandle(elem,
                new jeecs::basic::resource<jeecs::graphic::shader>(shader),
                WOORT_IGNORE,
                [](void* ptr)
                {
                    delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
                },
                nullptr);

            woort_vec_push(out_array, elem);
        }
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Shaders组件，在此panic?
    return woort_ret_value(out_array);
}

WOORT_API woort_api wojeapi_reload_shader_of_entity(void);
WOORT_API woort_api wojeapi_reload_texture_of_entity(void);

WOORT_API woort_api wojeapi_set_shaders_of_entity(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    const woort_value shader_array = 1;

    if (jeecs::Renderer::Shaders* shaders = entity->get_component<jeecs::Renderer::Shaders>())
    {
        shaders->shaders.clear();
        const size_t arrsize = woort_vec_len(shader_array);
        for (size_t i = 0; i < arrsize; ++i)
        {
            (void)woort_vec_get(WOORT_RETURN_SLOT, shader_array, i);

            jeecs::basic::resource<jeecs::graphic::shader>* shader =
                (jeecs::basic::resource<jeecs::graphic::shader> *)woort_pointer(
                    WOORT_RETURN_SLOT);
            shaders->shaders.push_back(*shader);
        }
    }

    // TODO: 如果当前实体不包含jeecs::Renderer::Shaders组件，在此panic?
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_uniforms_from_shader(void)
{
    /*
    extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
                func _get_uniforms_from_shader(
                    shad: shader
                )=> map<string, uniform_variable>;
    */
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);

    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_map = s + 0;
    const woort_value val = s + 1;
    const woort_value elem = s + 2;

    woort_set_map(out_map);

    auto* uniforms = (*shader)->resource()->m_custom_uniforms;

    while (uniforms)
    {
        woort_set_struct(val, 2);
        woort_map_set_by_string(out_map, uniforms->m_name, val);

        if (uniforms->m_uniform_type >= jegl_shader::uniform_type::INT
            && uniforms->m_uniform_type <= jegl_shader::uniform_type::FLOAT4)
        {
            woort_set_int(elem, uniforms->m_uniform_type);
        }
        else if (uniforms->m_uniform_type == jegl_shader::uniform_type::TEXTURE)
        {
            woort_set_int(elem, jegl_shader::uniform_type::TEXTURE);
        }
        else
            // Others
            woort_set_int(elem, 1 + jegl_shader::uniform_type::TEXTURE);

        woort_struct_set(val, 0, elem);

        switch (uniforms->m_uniform_type)
        {
        case jegl_shader::uniform_type::INT:
        case jegl_shader::uniform_type::TEXTURE:
            woort_set_int(elem, uniforms->m_value.m_int);
            break;
        case jegl_shader::uniform_type::INT2:
            woort_set_struct(elem, 2);
            woort_struct_set_int(elem, 0, uniforms->m_value.m_int2[0]);
            woort_struct_set_int(elem, 1, uniforms->m_value.m_int2[1]);
            break;
        case jegl_shader::uniform_type::INT3:
            woort_set_struct(elem, 3);
            woort_struct_set_int(elem, 0, uniforms->m_value.m_int3[0]);
            woort_struct_set_int(elem, 1, uniforms->m_value.m_int3[1]);
            woort_struct_set_int(elem, 2, uniforms->m_value.m_int3[2]);
            break;
        case jegl_shader::uniform_type::INT4:
            woort_set_struct(elem, 4);
            woort_struct_set_int(elem, 0, uniforms->m_value.m_int4[0]);
            woort_struct_set_int(elem, 1, uniforms->m_value.m_int4[1]);
            woort_struct_set_int(elem, 2, uniforms->m_value.m_int4[2]);
            woort_struct_set_int(elem, 3, uniforms->m_value.m_int4[3]);
            break;
        case jegl_shader::uniform_type::FLOAT:
            woort_set_float(elem, uniforms->m_value.m_float);
            break;
        case jegl_shader::uniform_type::FLOAT2:
            woort_set_struct(elem, 2);
            woort_struct_set_float(elem, 0, uniforms->m_value.m_float2[0]);
            woort_struct_set_float(elem, 1, uniforms->m_value.m_float2[1]);
            break;
        case jegl_shader::uniform_type::FLOAT3:
            woort_set_struct(elem, 3);
            woort_struct_set_float(elem, 0, uniforms->m_value.m_float3[0]);
            woort_struct_set_float(elem, 1, uniforms->m_value.m_float3[1]);
            woort_struct_set_float(elem, 2, uniforms->m_value.m_float3[2]);
            break;
        case jegl_shader::uniform_type::FLOAT4:
            woort_set_struct(elem, 4);
            woort_struct_set_float(elem, 0, uniforms->m_value.m_float4[0]);
            woort_struct_set_float(elem, 1, uniforms->m_value.m_float4[1]);
            woort_struct_set_float(elem, 2, uniforms->m_value.m_float4[2]);
            woort_struct_set_float(elem, 3, uniforms->m_value.m_float4[3]);
            break;
        default:
            // Unsupport type
            woort_set_nil(elem);
        }

        woort_struct_set(val, 1, elem);

        uniforms = uniforms->m_next;
    }

    return woort_ret_value(out_map);
}

WOORT_API woort_api wojeapi_set_uniforms_int(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), (int)woort_int(2));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_set_uniforms_int2(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), (int)woort_int(2), (int)woort_int(3));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_set_uniforms_int3(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), (int)woort_int(2), (int)woort_int(3), (int)woort_int(4));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_set_uniforms_int4(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), (int)woort_int(2), (int)woort_int(3), (int)woort_int(4), (int)woort_int(5));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_set_uniforms_float(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), woort_float(2));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_set_uniforms_float2(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1),
        jeecs::math::vec2(woort_float(2), woort_float(3)));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_set_uniforms_float3(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1),
        jeecs::math::vec3(woort_float(2), woort_float(3), woort_float(4)));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_set_uniforms_float4(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1),
        jeecs::math::vec4(woort_float(2), woort_float(3), woort_float(4), woort_float(5)));

    return woort_ret_void();
}

// defined in 'jeecs_core_editor_system.hpp'
WOORT_API woort_api wojeapi_get_bad_shader_list_of_entity(void);
WOORT_API woort_api wojeapi_store_bad_shader_name(void);
WOORT_API woort_api wojeapi_remove_bad_shader_name(void);
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_int(void);
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_int2(void);
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_int3(void);
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_int4(void);
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_float(void);
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_float2(void);
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_float3(void);
WOORT_API woort_api wojeapi_store_bad_shader_uniforms_float4(void);

WOORT_API woort_api wojeapi_shader_path(void)
{
    auto* shader = (jeecs::basic::resource<jeecs::graphic::shader> *)woort_gcpointer(0);

    if (auto str = (*shader)->resource()->m_handle.m_path_may_null_if_builtin)
        return woort_ret_string(str);
    return woort_ret_string("< Built-in shader >");
}

WOORT_API woort_api wojeapi_texture_get_size(void)
{
    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);
    auto sz = texture->get()->size();

    woort_set_struct(WOORT_RETURN_SLOT, 2);

    woort_struct_set_int(WOORT_RETURN_SLOT, 0, (woort_Int)sz.x);
    woort_struct_set_int(WOORT_RETURN_SLOT, 1, (woort_Int)sz.y);

    return woort_ret();
}

WOORT_API woort_api wojeapi_texture_path(void)
{
    auto* texture = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(0);

    if (auto str = (*texture)->resource()->m_handle.m_path_may_null_if_builtin)
        return woort_ret_option_string(str);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_get_entity_arch_information(void)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)woort_gcpointer(0);
    size_t chunk_size = 0, entity_size = 0, entity_count = 0;

    jedbg_get_entity_arch_information(entity, &chunk_size, &entity_size, &entity_count);

    woort_set_struct(WOORT_RETURN_SLOT, 3);

    woort_struct_set_int(WOORT_RETURN_SLOT, 0, (woort_Int)chunk_size);
    woort_struct_set_int(WOORT_RETURN_SLOT, 1, (woort_Int)entity_size);
    woort_struct_set_int(WOORT_RETURN_SLOT, 2, (woort_Int)entity_count);

    return woort_ret();
}

WOORT_API woort_api wojeapi_towoo_register_system(void)
{
    const jeecs::typing::type_info* result;

    const woort_U8CString name = woort_string(0);
    const woort_U8CString path = woort_string(1);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        result = je_towoo_register_system(name, path);
    }
    (void)woort_vm_swap(last);

    if (result != nullptr)
        return woort_ret_option_pointer((void*)result);

    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_towoo_unregister_system(void)
{
    je_towoo_unregister_system((const jeecs::typing::type_info*)woort_pointer(0));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_update_component(void)
{
    const woort_U8CString component_name = woort_string(0);
    const woort_U8CString component_path = woort_string(1);

    woort_vm* const last = woort_vm_swap(nullptr);

    jeecs_file* const texfile = jeecs_file_open(component_path);
    if (texfile == nullptr)
    {
        jeecs::debug::logerr("Failed to register: '%s', unable to open file '%s'.",
            component_name, component_path);
    }
    else
    {
        char* const src = (char*)malloc(texfile->m_file_length);
        if (src == nullptr)
        {
            jeecs::debug::logerr("Failed to register: '%s', out of memory.", component_name);
        }
        else
        {
            (void)jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);

            wo_CompileErrors* cerror;
            woort_CodeEnv* const cenv =
                wo_load_binary(component_path, src, texfile->m_file_length, &cerror);

            jeecs_file_close(texfile);
            free(src);

            if (cenv == nullptr)
            {
                jeecs::debug::logerr("Failed to register: '%s' failed to compile:\n%s",
                    component_name, wo_get_compile_error(cerror, WO_COLORFUL));
                wo_compile_errors_free(cerror);
            }
            else
            {
                woort_vm* const vmm = woort_vm_create();
                if (vmm == nullptr)
                {
                    jeecs::debug::logerr(
                        "Failed to register: '%s' failed to create vm.", component_name);
                }
                else
                {
                    woort_vm* const last2 = woort_vm_swap(vmm);
                    {
                        woort_value s2;
                        if (!woort_push_reserve(2, &s2))
                        {
                            jeecs::debug::logerr(
                                "Failed to register: '%s', failed to reserve stack.", component_name);
                        }
                        else if (!woort_load_extern_const(s2 + 1, cenv, "_init_towoo_component"))
                        {
                            jeecs::debug::logerr("Failed to register: '%s' cannot find '_init_towoo_component' in '%s', "
                                "forget to import je/towoo/component.wo ?",
                                component_name, component_path);
                        }
                        else if (WOORT_VM_CALL_STATUS_NORMAL != woort_bootup_codeenv(WOORT_IGNORE, cenv))
                        {
                            jeecs::debug::logerr("Failed to register: '%s', init failed: '%s'.",
                                component_name, woort_vm_get_runtime_error(vmm));
                        }
                        else
                        {
                            woort_set_string(s2 + 0, component_name);
                            if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(s2 + 1, s2 + 1))
                            {
                                jeecs::debug::logerr("Failed to register: '%s', '_init_towoo_component' failed: '%s'.",
                                    component_name, woort_vm_get_runtime_error(vmm));
                            }
                            else
                            {
                                (void*)woort_vm_swap(last2);
                                woort_codeenv_drop(cenv);
                                (void)woort_vm_swap(last);

                                woort_import_value(WOORT_RETURN_SLOT, vmm, s2 + 1);
                                woort_vm_close(vmm);

                                return woort_ret_option_value(WOORT_RETURN_SLOT);
                            }
                        }
                    }
                    (void*)woort_vm_swap(last2);
                    woort_vm_close(vmm);
                }
                woort_codeenv_drop(cenv);
            }
        }
    }
    (void)woort_vm_swap(last);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_towoo_unregister_component(void)
{
    const jeecs::typing::type_info* t =
        (const jeecs::typing::type_info*)woort_pointer(0);

    je_typing_unregister(t);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_update_api(void)
{
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        je_towoo_update_api();
    }
    (void)woort_vm_swap(last);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_typemgr_get_unregister_count(void)
{
    return woort_ret_int((woort_Int)jedbg_get_unregister_type_count());
}
WOORT_API woort_api wojeapi_get_woolang_commit_sha(void)
{
    return woort_ret_string(wo_commit_sha());
}
WOORT_API woort_api wojeapi_get_all_internal_scripts(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;
    const woort_value val = s + 1;

    woort_set_map(result);

    char** paths = nullptr;
    size_t count = woort_vfs_get_all_paths(&paths);

    for (size_t i = 0; i < count; ++i)
    {
        const char* vpath = paths[i];
        char* data = nullptr;
        size_t len = 0;
        if (woort_vfs_read(vpath, &data, &len))
        {
            woort_set_buffer(val, data, len);
            woort_map_set_by_string(result, vpath, val);

            woort_free(data);
        }
    }

    for (size_t i = 0; i < count; ++i)
        woort_free(paths[i]);
    woort_free(paths);

    return woort_ret_value(result);
}

struct dynamic_parser_impl_t
{
    woort_Value m_saving;
    woort_Value m_restoring;
    woort_Value m_edit;

    const jeecs::typing::typeinfo_script_parser*
        m_script_parser;
};

struct dynamic_parser_global_context_t
{
    using parser_table_t =
        std::unordered_map<jeecs::typing::typeid_t, std::unique_ptr<dynamic_parser_impl_t>>;

    std::mutex _je_dynamic_parser_mx;

    parser_table_t _je_dynamic_parser_impls;
    woort_CodeEnv* _je_dynamic_parser_cenv = nullptr;
};
static dynamic_parser_global_context_t _je_dynamic_parser_global_context;

void _je_dynamic_parser_clear()
{
    if (_je_dynamic_parser_global_context._je_dynamic_parser_cenv != nullptr)
    {
        _je_dynamic_parser_global_context._je_dynamic_parser_impls.clear();

        woort_codeenv_drop(_je_dynamic_parser_global_context._je_dynamic_parser_cenv);
        _je_dynamic_parser_global_context._je_dynamic_parser_cenv = nullptr;
    }

    assert(_je_dynamic_parser_global_context._je_dynamic_parser_impls.empty());
}
void _je_dynamic_parser_update_types(woort_value tmp)
{
    if (_je_dynamic_parser_global_context._je_dynamic_parser_cenv != nullptr)
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

                woort_Value* const tmp_internal_storage = woort_internal_value(tmp);

                auto p = std::make_unique<dynamic_parser_impl_t>();

                bool succ = true;
                if (woort_load_extern_const(
                    tmp,
                    _je_dynamic_parser_global_context._je_dynamic_parser_cenv,
                    (script_woolang_typename + "::parser::saving").c_str()))
                {
                    p->m_saving = *tmp_internal_storage;
                }
                else
                    succ = false;

                if (woort_load_extern_const(
                    tmp,
                    _je_dynamic_parser_global_context._je_dynamic_parser_cenv,
                    (script_woolang_typename + "::parser::restoring").c_str()))
                {
                    p->m_restoring = *tmp_internal_storage;
                }
                else
                    succ = false;

                if (woort_load_extern_const(
                    tmp,
                    _je_dynamic_parser_global_context._je_dynamic_parser_cenv,
                    (script_woolang_typename + "::parser::edit").c_str()))
                {
                    p->m_edit = *tmp_internal_storage;
                }
                else
                    succ = false;

                if (succ)
                {
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

    char* const content = (char*)malloc(filelen + 1);
    if (content == nullptr)
    {
        return std::optional("Out of memory.");
    }

    (void)jeecs_file_read(content, sizeof(char), filelen, file);

    wo_CompileErrors* cerror;
    woort_CodeEnv* const cenv = wo_load_binary(path, content, filelen, &cerror);
    free(content);

    if (cenv == nullptr)
    {
        std::optional<std::string> r;
        r.emplace(wo_get_compile_error(cerror, WO_PLAIM));

        wo_compile_errors_free(cerror);

        return r;
    }

    woort_vm* const vmm = woort_vm_create();
    if (vmm == nullptr)
    {
        woort_codeenv_drop(cenv);
        return std::optional("Out of memory.");
    }

    woort_vm* const last = woort_vm_swap(vmm);
    {
        woort_value s;
        if (!woort_push_reserve(1, &s))
        {
            (void)woort_vm_swap(last);
            woort_codeenv_drop(cenv);
            woort_vm_close(vmm);

            return std::optional("Stack overflow.");
        }

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_bootup_codeenv(WOORT_IGNORE, cenv))
        {
            (void)woort_vm_swap(last);

            std::optional<std::string> r;
            r.emplace(woort_vm_get_runtime_error(vmm));

            woort_codeenv_drop(cenv);
            woort_vm_close(vmm);
            return r;
        }

        _je_dynamic_parser_clear();
        _je_dynamic_parser_global_context._je_dynamic_parser_cenv = cenv;
        _je_dynamic_parser_update_types(s);
    }
    (void)woort_vm_swap(last);
    woort_vm_close(vmm);

    return std::nullopt;
}

////////////////////////////////////////////////////////////////

WOORT_API woort_api wojeapi_dynamic_parser_update_script(void)
{
    std::optional<std::string> result;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
        result = _je_dynamic_parser_update_all(woort_string(0));
    }
    (void)woort_vm_swap(last);

    if (result.has_value())
        return woort_ret_result_err_string(result.value().c_str());
    return woort_ret_result_ok_void();
}

WOORT_API woort_api wojeapi_dynamic_parser_update_type(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_vm* const last = woort_vm_swap(nullptr);
    std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
    (void)woort_vm_swap(last);

    _je_dynamic_parser_update_types(s);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_dynamic_parser_clear(void)
{
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
        _je_dynamic_parser_clear();
    }
    (void)woort_vm_swap(last);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_dynamic_parser_saving(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);

    auto* type = (const jeecs::typing::type_info*)woort_pointer(0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_global_context._je_dynamic_parser_cenv != nullptr);

        auto* val = woort_pointer(1);
        auto& parser = fnd->second;

        const woort_value value = s + 0;
        const woort_value func = s + 1;

        parser->m_script_parser->m_script_parse_c2w(val, value);

        *woort_internal_value(func) = parser->m_saving;

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(value, func))
            return woort_ret_panic("Failed to invoke `save` callback.");

        return woort_ret_option_value(value);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_dynamic_parser_restoring(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);

    auto* type = (const jeecs::typing::type_info*)woort_pointer(0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_global_context._je_dynamic_parser_cenv != nullptr);

        void* val = woort_pointer(1);
        const char* dat = woort_string(2);
        auto& parser = fnd->second;

        const woort_value result = s + 0;
        const woort_value func = s + 1;

        woort_set_string(result, dat);

        *woort_internal_value(func) = parser->m_restoring;

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(result, func))
            return woort_ret_panic("Failed to invoke `restore` callback.");

        parser->m_script_parser->m_script_parse_w2c(val, result);
        return woort_ret_bool(true);
    }
    return woort_ret_bool(false);
}

WOORT_API woort_api wojeapi_dynamic_parser_edit(void)
{
    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    std::lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);

    auto* type = (const jeecs::typing::type_info*)woort_pointer(0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        assert(_je_dynamic_parser_global_context._je_dynamic_parser_cenv != nullptr);

        auto* val = woort_pointer(1);
        const char* tag = woort_string(2);
        auto& parser = fnd->second;

        const woort_value value = s + 0;
        const woort_value tag_slot = s + 1;
        const woort_value func = s + 2;

        parser->m_script_parser->m_script_parse_c2w(val, value);
        woort_set_string(tag_slot, tag);

        *woort_internal_value(func) = parser->m_edit;

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(value, func))
            return woort_ret_panic("Failed to invoke `edit` callback.");

        if (woort_option_get(value, value))
            parser->m_script_parser->m_script_parse_w2c(val, value);

        return woort_ret_bool(true);
    }
    return woort_ret_bool(false);
}

WOORT_API woort_api wojeapi_audio_buffer_load(void)
{
    auto buffer = jeecs::audio::buffer::load(woort_string(0));
    if (buffer)
    {
        return woort_ret_option_gchandle(
            new jeecs::basic::resource<jeecs::audio::buffer>(buffer.value()),
            WOORT_IGNORE,
            [](void* p)
            {
                delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::buffer>*>(p);
            },
            nullptr);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_audio_buffer_info(void)
{
    jeecs::basic::resource<jeecs::audio::buffer>* buffer =
        static_cast<jeecs::basic::resource<jeecs::audio::buffer> *>(woort_gcpointer(0));

    woort_set_struct(WOORT_RETURN_SLOT, 5);
    auto* buffer_instance = (*buffer)->handle();

    woort_struct_set(WOORT_RETURN_SLOT, 0, (woort_Int)buffer_instance->m_size);
    woort_struct_set(WOORT_RETURN_SLOT, 1, (woort_Int)buffer_instance->m_sample_rate);
    woort_struct_set(WOORT_RETURN_SLOT, 2, (woort_Int)buffer_instance->m_sample_size);
    woort_struct_set(WOORT_RETURN_SLOT, 3, (woort_Int)buffer_instance->m_byte_rate);
    woort_struct_set(WOORT_RETURN_SLOT, 4, (woort_Int)buffer_instance->m_format);

    return woort_ret();
}
/*
extern("libjoyecs", "wojeapi_audio_filter_create")
    public func create()=> filter;

extern("libjoyecs", "wojeapi_audio_filter_info")
    public func info(self: filter)=> filter_data_t;

extern("libjoyecs", "wojeapi_audio_filter_update")
    func _update(self: filter, info: filter_data_t)=> void;
*/
WOORT_API woort_api wojeapi_audio_filter_create(void)
{
    return woort_ret_gchandle(
        new jeecs::basic::resource<jeecs::audio::filter>(jeecs::audio::filter::create()),
        WOORT_IGNORE,
        [](void* p)
        {
            delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::filter>*>(p);
        },
        nullptr);
}

WOORT_API woort_api wojeapi_audio_filter_info(void)
{
    jeecs::basic::resource<jeecs::audio::filter>* filter =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::filter> *>(woort_gcpointer(0));

    woort_set_struct(WOORT_RETURN_SLOT, 4);

    auto* filter_instance = (*filter)->handle();

    woort_struct_set_int(WOORT_RETURN_SLOT, 0, (woort_Int)filter_instance->m_type);
    woort_struct_set_float(WOORT_RETURN_SLOT, 1, filter_instance->m_gain);
    woort_struct_set_float(WOORT_RETURN_SLOT, 2, filter_instance->m_gain_lf);
    woort_struct_set_float(WOORT_RETURN_SLOT, 3, filter_instance->m_gain_hf);

    return woort_ret();
}

WOORT_API woort_api wojeapi_audio_filter_update(void)
{
    jeecs::basic::resource<jeecs::audio::filter>* filter =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::filter> *>(woort_gcpointer(0));
    const woort_value updated_info = 1;

    (*filter)->update([&](jeal_filter* flt)
        {
            flt->m_type = (jeal_filter_type)woort_struct_get_int(updated_info, 0);
            flt->m_gain = woort_struct_get_float(updated_info, 1);
            flt->m_gain_lf = woort_struct_get_float(updated_info, 2);
            flt->m_gain_hf = woort_struct_get_float(updated_info, 3);
        });

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_create(void)
{
    return woort_ret_gchandle(
        new jeecs::basic::resource<jeecs::audio::source>(jeecs::audio::source::create()),
        WOORT_IGNORE,
        [](void* p)
        {
            delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::source>*>(p);
        },
        nullptr);
}
WOORT_API woort_api wojeapi_audio_source_info(void)
{
    woort_value elem;
    if (!woort_push_reserve(1, &elem))
        return woort_ret_panic("Stack overflow.");

    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(woort_gcpointer(0));

    woort_set_struct(WOORT_RETURN_SLOT, 5);

    auto* source_instance = (*source)->handle();

    woort_struct_set_bool(WOORT_RETURN_SLOT, 0, source_instance->m_loop);
    woort_struct_set_float(WOORT_RETURN_SLOT, 1, source_instance->m_gain);
    woort_struct_set_float(WOORT_RETURN_SLOT, 2, source_instance->m_pitch);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, source_instance->m_location[i]);
    }
    woort_struct_set(WOORT_RETURN_SLOT, 3, elem);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, source_instance->m_velocity[i]);
    }
    woort_struct_set(WOORT_RETURN_SLOT, 4, elem);

    return woort_ret();
}
WOORT_API woort_api wojeapi_audio_source_update(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(woort_gcpointer(0));
    const woort_value updated_info = 1;

    (*source)->update(
        [&](jeal_source* src)
        {
            src->m_loop = woort_struct_get_bool(updated_info, 0);
            src->m_gain = woort_struct_get_float(updated_info, 1);
            src->m_pitch = woort_struct_get_float(updated_info, 2);

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 3);
            for (uint16_t i = 0; i < 3; ++i)
            {
                src->m_location[i] = 
                    woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 4);
            for (uint16_t i = 0; i < 3; ++i)
            {
                src->m_velocity[i] = 
                    woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }
        });

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_audio_source_play(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(woort_gcpointer(0));

    (*source)->play();

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_audio_source_stop(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(woort_gcpointer(0));

    (*source)->stop();

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_audio_source_pause(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(woort_gcpointer(0));

    (*source)->pause();

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_audio_source_set_buffer(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(woort_gcpointer(0));
    jeecs::basic::resource<jeecs::audio::buffer>* buffer =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::buffer> *>(woort_gcpointer(1));

    (*source)->set_playing_buffer(*buffer);

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_audio_source_set_filter(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(woort_gcpointer(0));

    std::optional<jeecs::basic::resource<jeecs::audio::filter>> filter;
    if (woort_option_get(WOORT_RETURN_SLOT, 1))
    {
        filter.emplace(
            *static_cast<jeecs::basic::resource<jeecs::audio::filter> *>(
                woort_gcpointer(WOORT_RETURN_SLOT)));
    }

    (*source)->set_filter(filter);

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_audio_source_bind_effect_slot_and_filter(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::source> *>(woort_gcpointer(0));

    std::optional<jeecs::basic::resource<jeecs::audio::effect_slot>> effect_slot;
    std::optional<jeecs::basic::resource<jeecs::audio::filter>> filter;

    if (woort_option_get(WOORT_RETURN_SLOT, 1))
    {
        effect_slot.emplace(
            *static_cast<jeecs::basic::resource<jeecs::audio::effect_slot> *>(
                woort_gcpointer(WOORT_RETURN_SLOT)));
    }
    if (woort_option_get(WOORT_RETURN_SLOT, 2))
    {
        filter.emplace(
            *static_cast<jeecs::basic::resource<jeecs::audio::filter> *>(
                woort_gcpointer(WOORT_RETURN_SLOT)));
    }

    (*source)->bind_effect_slot((size_t)woort_int(3), effect_slot, filter);

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_audio_source_get_state(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        static_cast<jeecs::basic::resource<jeecs::audio::source> *>(
            woort_gcpointer(0));

    return woort_ret_int((woort_Int)(*source)->get_state());
}
WOORT_API woort_api wojeapi_audio_source_get_offset(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        static_cast<jeecs::basic::resource<jeecs::audio::source> *>(
            woort_gcpointer(0));

    return woort_ret_int((woort_Int)(*source)->get_playing_offset());
}
WOORT_API woort_api wojeapi_audio_source_set_offset(void)
{
    jeecs::basic::resource<jeecs::audio::source>* source =
        static_cast<jeecs::basic::resource<jeecs::audio::source> *>(
            woort_gcpointer(0));

    (*source)->set_playing_offset((size_t)woort_int(1));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_listener_info(void)
{
    wo_value stacks = wo_reserve_stack(3, &args);
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

    return woort_ret_value(result);
}
WOORT_API woort_api wojeapi_audio_listener_update(void)
{
    wo_value stacks = wo_reserve_stack(2, &args);
    wo_value elem = stacks + 0;
    wo_value elem2 = stacks + 1;

    wo_value updated_info = 0;

    jeecs::audio::listener::update(
        [&](jeal_listener* lstn)
        {
            wo_struct_get(elem, updated_info, 0);
            lstn->m_gain = woort_float(elem);

            wo_struct_get(elem, updated_info, 1);
            lstn->m_global_gain = woort_float(elem);

            wo_struct_get(elem, updated_info, 2);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                lstn->m_location[i] = woort_float(elem2);
            }

            wo_struct_get(elem, updated_info, 3);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                lstn->m_velocity[i] = woort_float(elem2);
            }

            wo_struct_get(elem, updated_info, 4);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                lstn->m_forward[i] = woort_float(elem2);
            }

            wo_struct_get(elem, updated_info, 5);
            for (uint16_t i = 0; i < 3; ++i)
            {
                wo_struct_get(elem2, elem, i);
                lstn->m_upward[i] = woort_float(elem2);
            }
        });

    return woort_ret_void();
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

WOORT_API woort_api wojeapi_audio_effect_create(void)
{
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)woort_int(0);
    switch (kind)
    {
#define woort_ret_jeal_new_effect(effect_type_name)                                                       \
    woort_ret_gchandle(                                                                                   \
        new wo_je_effect_res_t(effect_type_name)(                                                         \
            jeecs::audio::effect<effect_type_name>::create()),                                            \
        WOORT_IGNORE,                                                                                     \
        [](void *p)                                                                                       \
        {                                                                                                 \
            delete static_cast<jeecs::basic::resource<jeecs::audio::effect<effect_type_name>> *>(p);      \
        },                                                                                                \
        nullptr)

    case woolang_je_audio_effect_kind::REVERB:
        return woort_ret_jeal_new_effect(jeal_effect_reverb);
    case woolang_je_audio_effect_kind::CHORUS:
        return woort_ret_jeal_new_effect(jeal_effect_chorus);
    case woolang_je_audio_effect_kind::DISTORTION:
        return woort_ret_jeal_new_effect(jeal_effect_distortion);
    case woolang_je_audio_effect_kind::ECHO:
        return woort_ret_jeal_new_effect(jeal_effect_echo);
    case woolang_je_audio_effect_kind::FLANGER:
        return woort_ret_jeal_new_effect(jeal_effect_flanger);
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
        return woort_ret_jeal_new_effect(jeal_effect_frequency_shifter);
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
        return woort_ret_jeal_new_effect(jeal_effect_vocal_morpher);
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
        return woort_ret_jeal_new_effect(jeal_effect_pitch_shifter);
    case woolang_je_audio_effect_kind::RING_MODULATOR:
        return woort_ret_jeal_new_effect(jeal_effect_ring_modulator);
    case woolang_je_audio_effect_kind::AUTOWAH:
        return woort_ret_jeal_new_effect(jeal_effect_autowah);
    case woolang_je_audio_effect_kind::COMPRESSOR:
        return woort_ret_jeal_new_effect(jeal_effect_compressor);
    case woolang_je_audio_effect_kind::EQUALIZER:
        return woort_ret_jeal_new_effect(jeal_effect_equalizer);
    case woolang_je_audio_effect_kind::EAXREVERB:
        return woort_ret_jeal_new_effect(jeal_effect_eaxreverb);
    default:
        return woort_ret_panic("Unknown audio effect type: %d", (int)kind);

#undef woort_ret_jeal_new_effect
    }
}
WOORT_API woort_api wojeapi_audio_effect_info(void)
{
    wo_value stacks = wo_reserve_stack(3, &args);
    wo_value result = stacks + 0;
    wo_value elem = stacks + 1;
    wo_value elem2 = stacks + 2;

    void* effect_res_ptr = woort_gcpointer(0);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)woort_int(1);

    switch (kind)
    {
    case woolang_je_audio_effect_kind::REVERB:
    {
        auto* effect = static_cast<wo_je_effect_res_t(jeal_effect_reverb)*>(
            effect_res_ptr);

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

        wo_set_int(elem, (woort_integer_t)info->m_waveform);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (woort_integer_t)info->m_phase);
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

        wo_set_int(elem, (woort_integer_t)info->m_waveform);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (woort_integer_t)info->m_phase);
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

        wo_set_int(elem, (woort_integer_t)info->m_left_direction);
        wo_struct_set(result, 1, elem);

        wo_set_int(elem, (woort_integer_t)info->m_right_direction);
        wo_struct_set(result, 2, elem);

        break;
    }
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
    {
        auto* effect =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_vocal_morpher)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        wo_set_struct(result, 6);

        wo_set_int(elem, (woort_integer_t)info->m_phoneme_a);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (woort_integer_t)info->m_phoneme_a_coarse_tuning);
        wo_struct_set(result, 1, elem);

        wo_set_int(elem, (woort_integer_t)info->m_phoneme_b);
        wo_struct_set(result, 2, elem);

        wo_set_int(elem, (woort_integer_t)info->m_phoneme_b_coarse_tuning);
        wo_struct_set(result, 3, elem);

        wo_set_int(elem, (woort_integer_t)info->m_waveform);
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

        wo_set_int(elem, (woort_integer_t)info->m_coarse_tune);
        wo_struct_set(result, 0, elem);

        wo_set_int(elem, (woort_integer_t)info->m_fine_tune);
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

        wo_set_int(elem, (woort_integer_t)info->m_waveform);
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
        return woort_ret_panic("Unknown audio effect type: %d", (int)kind);
    }

    return woort_ret_value(result);
}
WOORT_API woort_api wojeapi_audio_effect_update(void)
{
    wo_value stacks = wo_reserve_stack(2, &args);
    wo_value elem = stacks + 0;
    wo_value elem2 = stacks + 1;

    void* effect = wo_pointer(0);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)woort_int(1);
    wo_value updated_info = 2;

    switch (kind)
    {
    case woolang_je_audio_effect_kind::REVERB:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_reverb)*>(effect);

        (*effect_instance)->update([&](jeal_effect_reverb* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_density = woort_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_diffusion = woort_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_gain_hf = woort_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_decay_time = woort_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_decay_hf_ratio = woort_float(elem);

                wo_struct_get(elem, updated_info, 6);
                eff->m_reflections_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 7);
                eff->m_reflections_delay = woort_float(elem);

                wo_struct_get(elem, updated_info, 8);
                eff->m_late_reverb_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 9);
                eff->m_late_reverb_delay = woort_float(elem);

                wo_struct_get(elem, updated_info, 10);
                eff->m_air_absorption_gain_hf = woort_float(elem);

                wo_struct_get(elem, updated_info, 11);
                eff->m_room_rolloff_factor = woort_float(elem);

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
                eff->m_waveform = (jeal_effect_chorus::waveform)woort_int(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_phase = (int)woort_int(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_rate = woort_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_depth = woort_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_feedback = woort_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_delay = woort_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::DISTORTION:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_distortion)*>(effect);

        (*effect_instance)->update([&](jeal_effect_distortion* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_edge = woort_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_lowpass_cutoff = woort_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_equalizer_center_freq = woort_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_equalizer_bandwidth = woort_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::ECHO:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_echo)*>(effect);

        (*effect_instance)->update([&](jeal_effect_echo* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_delay = woort_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_lr_delay = woort_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_damping = woort_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_feedback = woort_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_spread = woort_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::FLANGER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_flanger)*>(effect);

        (*effect_instance)->update([&](jeal_effect_flanger* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_waveform = (jeal_effect_flanger::waveform)woort_int(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_phase = (int)woort_int(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_rate = woort_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_depth = woort_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_feedback = woort_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_delay = woort_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_frequency_shifter)*>(effect);

        (*effect_instance)->update([&](jeal_effect_frequency_shifter* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_frequency = woort_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_left_direction = (jeal_effect_frequency_shifter::direction)woort_int(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_right_direction = (jeal_effect_frequency_shifter::direction)woort_int(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_vocal_morpher)*>(effect);

        (*effect_instance)->update([&](jeal_effect_vocal_morpher* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_phoneme_a = (jeal_effect_vocal_morpher::phoneme)woort_int(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_phoneme_a_coarse_tuning = (int)woort_int(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_phoneme_b = (jeal_effect_vocal_morpher::phoneme)woort_int(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_phoneme_b_coarse_tuning = (int)woort_int(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_waveform = (jeal_effect_vocal_morpher::waveform)woort_int(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_rate = woort_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_pitch_shifter)*>(effect);

        (*effect_instance)->update([&](jeal_effect_pitch_shifter* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_coarse_tune = (int)woort_int(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_fine_tune = (int)woort_int(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::RING_MODULATOR:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_ring_modulator)*>(effect);

        (*effect_instance)->update([&](jeal_effect_ring_modulator* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_frequency = woort_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_highpass_cutoff = woort_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_waveform = (jeal_effect_ring_modulator::waveform)woort_int(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::AUTOWAH:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_autowah)*>(effect);

        (*effect_instance)->update([&](jeal_effect_autowah* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_attack_time = woort_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_release_time = woort_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_resonance = woort_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_peak_gain = woort_float(elem); });
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
                eff->m_low_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_low_cutoff = woort_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_mid1_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_mid1_center = woort_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_mid1_width = woort_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_mid2_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 6);
                eff->m_mid2_center = woort_float(elem);

                wo_struct_get(elem, updated_info, 7);
                eff->m_mid2_width = woort_float(elem);

                wo_struct_get(elem, updated_info, 8);
                eff->m_high_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 9);
                eff->m_high_cutoff = woort_float(elem); });
        break;
    }
    case woolang_je_audio_effect_kind::EAXREVERB:
    {
        auto* effect_instance =
            reinterpret_cast<wo_je_effect_res_t(jeal_effect_eaxreverb)*>(effect);

        (*effect_instance)->update([&](jeal_effect_eaxreverb* eff)
            {
                wo_struct_get(elem, updated_info, 0);
                eff->m_density = woort_float(elem);

                wo_struct_get(elem, updated_info, 1);
                eff->m_diffusion = woort_float(elem);

                wo_struct_get(elem, updated_info, 2);
                eff->m_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 3);
                eff->m_gain_hf = woort_float(elem);

                wo_struct_get(elem, updated_info, 4);
                eff->m_gain_lf = woort_float(elem);

                wo_struct_get(elem, updated_info, 5);
                eff->m_decay_time = woort_float(elem);

                wo_struct_get(elem, updated_info, 6);
                eff->m_decay_hf_ratio = woort_float(elem);

                wo_struct_get(elem, updated_info, 7);
                eff->m_decay_lf_ratio = woort_float(elem);

                wo_struct_get(elem, updated_info, 8);
                eff->m_reflections_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 9);
                eff->m_reflections_delay = woort_float(elem);

                wo_struct_get(elem, updated_info, 10);
                for (uint16_t i = 0; i < 3; ++i)
                {
                    wo_struct_get(elem2, elem, i);
                    eff->m_reflections_pan_xyz[i] = woort_float(elem2);
                }

                wo_struct_get(elem, updated_info, 11);
                eff->m_late_reverb_gain = woort_float(elem);

                wo_struct_get(elem, updated_info, 12);
                eff->m_late_reverb_delay = woort_float(elem);

                wo_struct_get(elem, updated_info, 13);
                for (uint16_t i = 0; i < 3; ++i)
                {
                    wo_struct_get(elem2, elem, i);
                    eff->m_late_reverb_pan_xyz[i] = woort_float(elem2);
                }

                wo_struct_get(elem, updated_info, 14);
                eff->m_echo_time = woort_float(elem);

                wo_struct_get(elem, updated_info, 15);
                eff->m_echo_depth = woort_float(elem);

                wo_struct_get(elem, updated_info, 16);
                eff->m_modulation_time = woort_float(elem);

                wo_struct_get(elem, updated_info, 17);
                eff->m_modulation_depth = woort_float(elem);

                wo_struct_get(elem, updated_info, 18);
                eff->m_air_absorption_gain_hf = woort_float(elem);

                wo_struct_get(elem, updated_info, 19);
                eff->m_hf_reference = woort_float(elem);

                wo_struct_get(elem, updated_info, 20);
                eff->m_lf_reference = woort_float(elem);

                wo_struct_get(elem, updated_info, 21);
                eff->m_room_rolloff_factor = woort_float(elem);

                wo_struct_get(elem, updated_info, 22);
                eff->m_decay_hf_limit = (wo_bool_t)wo_bool(elem); });
        break;
    }
    default:
        return woort_ret_panic("Unknown audio effect type: %d", (int)kind);
    }

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_effect_slot_create(void)
{
    return woort_ret_gchandle(
        vm,
        new jeecs::basic::resource<jeecs::audio::effect_slot>(jeecs::audio::effect_slot::create()),
        nullptr,
        [](void* p)
        { delete reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot>*>(p); });
}

WOORT_API woort_api wojeapi_audio_effect_slot_bind_effect(void)
{
    auto* effect_slot =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot> *>(wo_pointer(0));
    auto* effect = wo_pointer(1);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)woort_int(2);

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
        return woort_ret_panic("Unknown audio effect type: %d", (int)kind);
    }
    return woort_ret_void();
}

#undef wo_je_effect_res_t

WOORT_API woort_api wojeapi_audio_effect_slot_info(void)
{
    wo_value stacks = wo_reserve_stack(2, &args);
    wo_value result = stacks + 0;
    wo_value elem = stacks + 1;

    auto* effect_slot =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot> *>(wo_pointer(0));

    auto effect_slot_info = (*effect_slot)->handle();

    wo_set_struct(result, 1);

    wo_set_float(elem, effect_slot_info->m_gain);
    wo_struct_set(result, 0, elem);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_audio_effect_slot_update(void)
{
    wo_value stacks = wo_reserve_stack(1, &args);
    wo_value elem = stacks + 0;

    auto* effect_slot =
        reinterpret_cast<jeecs::basic::resource<jeecs::audio::effect_slot> *>(wo_pointer(0));
    wo_value info = 1;

    (*effect_slot)->update([&](jeal_effect_slot* eff)
        {
            wo_struct_get(elem, info, 0);
            eff->m_gain = woort_float(elem); });

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_input_update_mouse_state(void)
{
    auto group = (size_t)woort_int(0);
    auto key = (jeecs::input::mousecode)woort_int(1);
    auto down = wo_bool(2);

    je_io_update_mouse_state(group, key, down);

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_input_update_wheel(void)
{
    auto group = (size_t)woort_int(0);
    je_io_update_wheel(group, woort_float(1), woort_float(2));

    return woort_ret_void();
}
WOORT_API woort_api wojeapi_input_update_key_state(void)
{
    auto key = (jeecs::input::keycode)woort_int(0);
    auto down = wo_bool(1);

    je_io_update_key_state(key, down);

    return woort_ret_void();
}
