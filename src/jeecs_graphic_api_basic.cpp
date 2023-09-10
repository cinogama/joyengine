#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_cache_version.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <condition_variable>
#include <list>

struct jegl_thread_notifier
{
    std::atomic_flag m_graphic_terminate_flag;

    std::mutex       m_update_mx;
    std::atomic_bool m_update_flag;
    std::condition_variable m_update_waiter;

    std::atomic_bool m_reboot_flag;

    std::list<jegl_rendchain*> _m_commited_rendchains;
};

thread_local jegl_thread* _current_graphic_thread = nullptr;

jeecs::basic::atomic_list<jegl_resource::jegl_destroy_resouce> _destroing_graphic_resources;
bool _jegl_rchain_resource_used_by_chain(jegl_rendchain* chain, jegl_resource* resource);
void _graphic_work_thread(jegl_thread* thread, void(*frame_rend_work)(void*, jegl_thread*), void* arg)
{
    bool first_bootup = true;
    _current_graphic_thread = thread;
    do
    {
        thread->m_userdata = thread->m_apis->init_interface(thread,
            &thread->m_config, !first_bootup);
        first_bootup = false;
        ++thread->m_version;

        std::unordered_map<jegl_resource::jegl_destroy_resouce*, bool> _waiting_to_free_resource;

        while (thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set())
        {
            do
            {
                std::unique_lock uq1(thread->_m_thread_notifier->m_update_mx);
                thread->_m_thread_notifier->m_update_waiter.wait(uq1, [thread]()->bool {
                    return thread->_m_thread_notifier->m_update_flag;
                    });
            } while (0);

            if (!thread->m_apis->pre_update_interface(thread))
                // graphic thread want to exit. mark stop update
                std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update))->store(true);
            
            thread->_m_thread_notifier->_m_commited_rendchains.clear();

            // Ready for rend..
            if (!thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set()
                || thread->_m_thread_notifier->m_reboot_flag)
                break;

            if (!thread->m_apis->update_interface(thread))
                // graphic thread want to exit. mark stop update
                std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update))->store(true);
            else
                frame_rend_work(arg, thread);

            if (!thread->m_apis->late_update_interface(thread))
                std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update))->store(true);

            auto* del_res = _destroing_graphic_resources.pick_all();
            while (del_res)
            {
                auto* cur_del_res = del_res;
                del_res = del_res->last;

                if (cur_del_res->m_destroy_resource->m_graphic_thread == thread
                    && cur_del_res->m_destroy_resource->m_graphic_thread_version == thread->m_version)
                {
                    bool need_release = true;
                    for (auto* chain : thread->_m_thread_notifier->_m_commited_rendchains)
                    {
                        if (_jegl_rchain_resource_used_by_chain(chain, cur_del_res->m_destroy_resource))
                        {
                            _destroing_graphic_resources.add_one(cur_del_res);
                            need_release = false;
                            break;
                        }
                    }

                    if (need_release)
                    {
                        _waiting_to_free_resource[cur_del_res] = true;
                    }
                }
                else if (--cur_del_res->m_retry_times)
                    // Need re-try
                    _destroing_graphic_resources.add_one(cur_del_res);
                else
                {
                    // Free this
                    if (cur_del_res->m_destroy_resource->m_graphic_thread != nullptr)
                        jeecs::debug::logwarn("Resource %p cannot free by correct thread, maybe it is out-dated? Free it!",
                            cur_del_res->m_destroy_resource);
                    _waiting_to_free_resource[cur_del_res] = false;
                }
            }

            do {
                std::lock_guard g1(thread->_m_thread_notifier->m_update_mx);
                thread->_m_thread_notifier->m_update_flag = false;
                thread->_m_thread_notifier->m_update_waiter.notify_all();
            } while (0);

            for (auto [deleting_resource, need_release] : _waiting_to_free_resource)
            {
                if (need_release)
                {
                    thread->m_apis->close_resource(thread, deleting_resource->m_destroy_resource);
                    delete deleting_resource->m_destroy_resource;
                    delete deleting_resource;
                }
            }
            _waiting_to_free_resource.clear();
        }

        thread->m_apis->shutdown_interface(
            thread, 
            thread->m_userdata,
            thread->_m_thread_notifier->m_reboot_flag);
        thread->m_userdata = nullptr;
        if (!thread->_m_thread_notifier->m_reboot_flag)
            break;

        thread->_m_thread_notifier->m_reboot_flag = false;
    } while (true);

}

void _jegl_commit_rendchain(jegl_thread* glthread, jegl_rendchain* chain)
{
    glthread->_m_thread_notifier->_m_commited_rendchains.push_back(chain);
}

void jegl_shader_generate_glsl(void* shader_generator, jegl_shader* write_to_shader);
void jegl_shader_free_generated_glsl(jegl_shader* write_to_shader);

//////////////////////////////////// API /////////////////////////////////////////

std::vector<jegl_thread*> _jegl_alive_glthread_list;
std::mutex _jegl_alive_glthread_list_mx;

jegl_thread* jegl_start_graphic_thread(
    jegl_interface_config config,
    void* universe_instance,
    jeecs_api_register_func_t register_func,
    void(*frame_rend_work)(void*, jegl_thread*),
    void* arg)
{
    jegl_thread* thread_handle = new jegl_thread();

    thread_handle->m_version = 0;
    thread_handle->_m_thread_notifier = new jegl_thread_notifier();
    thread_handle->m_apis = new jegl_graphic_api();

    memset(thread_handle->m_apis, 0, sizeof(jegl_graphic_api));
    register_func(thread_handle->m_apis);

    size_t err_api_no = 0;
    for (void** reador = (void**)thread_handle->m_apis;
        reador < (void**)(thread_handle->m_apis + 1);
        ++reador)
    {
        if (!*reador)
        {
            err_api_no++;
            jeecs::debug::logfatal("GraphicAPI function: %zu is invalid.", (size_t)(reador - (void**)thread_handle->m_apis));
        }
    }

    if (err_api_no)
    {
        delete thread_handle->_m_thread_notifier;
        delete thread_handle->m_apis;
        delete thread_handle;

        jeecs::debug::logfatal("Fail to start up graphic thread, abort and return nullptr.");
        return nullptr;
    }

    // Register finish functions
    do
    {
        std::lock_guard g1(_jegl_alive_glthread_list_mx);

        if (_jegl_alive_glthread_list.end() == 
            std::find(_jegl_alive_glthread_list.begin(), _jegl_alive_glthread_list.end(), 
                thread_handle))
        {
            _jegl_alive_glthread_list.push_back(thread_handle);
        }
    } while (0);


    // Take place.
    thread_handle->m_config = config;
    thread_handle->_m_thread_notifier->m_graphic_terminate_flag.test_and_set();
    thread_handle->_m_universe_instance = universe_instance;
    thread_handle->_m_thread_notifier->m_update_flag = false;
    thread_handle->_m_thread_notifier->m_reboot_flag = false;
    thread_handle->m_stop_update = new std::atomic_bool;
    thread_handle->_m_thread =
        new std::thread(
            _graphic_work_thread,
            thread_handle,
            frame_rend_work,
            arg);

    return thread_handle;
}

void jegl_finish()
{
    std::vector<jegl_thread*> shutdown_glthreads;
    do
    {
        std::lock_guard g1(_jegl_alive_glthread_list_mx);
        shutdown_glthreads = _jegl_alive_glthread_list;
    } while (0);
    for (auto alive_glthread : shutdown_glthreads)
    {
        jegl_terminate_graphic_thread(alive_glthread);
    }
}

void jegl_terminate_graphic_thread(jegl_thread* thread)
{
    do
    {
        std::lock_guard g1(_jegl_alive_glthread_list_mx);
        auto fnd = std::find(_jegl_alive_glthread_list.begin(), _jegl_alive_glthread_list.end(), thread);
        if (fnd == _jegl_alive_glthread_list.end())
            return;
        else
            _jegl_alive_glthread_list.erase(fnd);

    } while (0);

    assert(thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set());
    thread->_m_thread_notifier->m_graphic_terminate_flag.clear();

    do
    {
        std::lock_guard g1(thread->_m_thread_notifier->m_update_mx);
        thread->_m_thread_notifier->m_update_flag = true;
        thread->_m_thread_notifier->m_update_waiter.notify_all();
    } while (0);

    assert(std::launder(reinterpret_cast<std::thread*>(thread->_m_thread))->joinable());
    std::launder(reinterpret_cast<std::thread*>(thread->_m_thread))->join();

    delete thread->_m_thread_notifier;
    delete thread;
}

bool jegl_update(jegl_thread* thread)
{
    if (std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update))->load())
        return false;

    do
    {
        std::unique_lock uq1(thread->_m_thread_notifier->m_update_mx);
        thread->_m_thread_notifier->m_update_flag = true;
        thread->_m_thread_notifier->m_update_waiter.notify_all();

        // Start Frame, then wait frame end...~
        thread->_m_thread_notifier->m_update_waiter.wait(uq1, [thread]()->bool {
            return !thread->_m_thread_notifier->m_update_flag;
            });
    } while (0);

    return true;
}

void jegl_reboot_graphic_thread(jegl_thread* thread_handle, jegl_interface_config config)
{
    thread_handle->m_config = config;
    thread_handle->_m_thread_notifier->m_reboot_flag = true;
}

void jegl_using_resource(jegl_resource* resource)
{
    bool need_init_resouce = false;
    // This function is not thread safe.
    if (!_current_graphic_thread)
        return jeecs::debug::logerr("Graphic resource only usable in graphic thread.");

    if (!resource->m_graphic_thread)
    {
        need_init_resouce = true;
        resource->m_graphic_thread = _current_graphic_thread;
        resource->m_graphic_thread_version = _current_graphic_thread->m_version;
    }
    if (_current_graphic_thread != resource->m_graphic_thread)
    {
        jeecs::debug::logerr("This resource has been used in graphic thread: %p.", resource->m_graphic_thread);
        return;
    }
    if (resource->m_graphic_thread_version != _current_graphic_thread->m_version)
    {
        need_init_resouce = true;
        resource->m_graphic_thread_version = _current_graphic_thread->m_version;
    }

    // If resource is died, ignore it.
    if (resource->m_custom_resource == nullptr)
        need_init_resouce = false;

    if (need_init_resouce)
        _current_graphic_thread->m_apis->init_resource(_current_graphic_thread, resource);
    _current_graphic_thread->m_apis->using_resource(_current_graphic_thread, resource);
    if (resource->m_type == jegl_resource::SHADER)
    {
        auto uniform_vars = resource->m_raw_shader_data->m_custom_uniforms;
        while (uniform_vars)
        {
            if (uniform_vars->m_index == jeecs::typing::INVALID_UINT32)
                uniform_vars->m_index = jegl_uniform_location(resource, uniform_vars->m_name);

            if (uniform_vars->m_updated || need_init_resouce)
            {
                uniform_vars->m_updated = false;
                switch (uniform_vars->m_uniform_type)
                {
                case jegl_shader::uniform_type::FLOAT:
                    jegl_uniform_float(resource, uniform_vars->m_index, uniform_vars->x);
                    break;
                case jegl_shader::uniform_type::FLOAT2:
                    jegl_uniform_float2(resource, uniform_vars->m_index, uniform_vars->x, uniform_vars->y);
                    break;
                case jegl_shader::uniform_type::FLOAT3:
                    jegl_uniform_float3(resource, uniform_vars->m_index, uniform_vars->x, uniform_vars->y, uniform_vars->z);
                    break;
                case jegl_shader::uniform_type::FLOAT4:
                    jegl_uniform_float4(resource, uniform_vars->m_index, uniform_vars->x, uniform_vars->y, uniform_vars->z, uniform_vars->w);
                    break;
                case jegl_shader::uniform_type::FLOAT4X4:
                    jegl_uniform_float4x4(resource, uniform_vars->m_index, uniform_vars->mat4x4);
                    break;
                case jegl_shader::uniform_type::INT:
                case jegl_shader::uniform_type::TEXTURE:
                    jegl_uniform_int(resource, uniform_vars->m_index, uniform_vars->n);
                    break;
                default:
                    jeecs::debug::logerr("Unsupport uniform variable type."); break;
                    break;
                }
            }

            uniform_vars = uniform_vars->m_next;
        }
    }
    
}

void _jegl_free_resource_instance(jegl_resource* resource)
{
    assert(resource != nullptr);
    assert(resource->m_custom_resource == nullptr);
    assert(resource->m_raw_ref_count == nullptr);

    // Send this resource to destroing list;
    auto* del_res = new jegl_resource::jegl_destroy_resouce;
    del_res->m_retry_times = 15;
    del_res->m_destroy_resource = resource;
    _destroing_graphic_resources.add_one(del_res);
}

void jegl_close_resource(jegl_resource* resource)
{
    assert(resource != nullptr);
    assert(resource->m_custom_resource != nullptr);

    if (resource->m_raw_ref_count == nullptr ||
        1 == std::launder(reinterpret_cast<std::atomic_uint32_t*>(resource->m_raw_ref_count))->fetch_sub(1))
    {
        switch (resource->m_type)
        {
        case jegl_resource::TEXTURE:
            // close resource's raw data, then send this resource to closing-queue
            if (resource->m_raw_texture_data->m_pixels)
            {
                assert(0 == (resource->m_raw_texture_data->m_format & jegl_texture::format::FORMAT_MASK));
                stbi_image_free(resource->m_raw_texture_data->m_pixels);
                delete resource->m_raw_texture_data;
            }
            else
                assert(0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::FORMAT_MASK));
            break;
        case jegl_resource::SHADER:
            // close resource's raw data, then send this resource to closing-queue
            jegl_shader_free_generated_glsl(resource->m_raw_shader_data);
            delete resource->m_raw_shader_data;
            break;
        case jegl_resource::VERTEX:
            // close resource's raw data, then send this resource to closing-queue
            je_mem_free((void*)resource->m_raw_vertex_data->m_vertex_datas);
            je_mem_free((void*)resource->m_raw_vertex_data->m_vertex_formats);
            delete resource->m_raw_vertex_data;
            break;
        case jegl_resource::FRAMEBUF:
            // Close all frame rend out texture
            if (!resource->m_raw_framebuf_data)
                assert(resource->m_raw_framebuf_data->m_attachment_count == 0);
            else
            {
                assert(resource->m_raw_framebuf_data->m_attachment_count > 0);
                jeecs::basic::resource<jeecs::graphic::texture>* attachments =
                    (jeecs::basic::resource<jeecs::graphic::texture>*)resource->m_raw_framebuf_data->m_output_attachments;

                delete[]attachments;
            }
            delete resource->m_raw_framebuf_data;
            break;
        case jegl_resource::UNIFORMBUF:
            je_mem_free(resource->m_raw_uniformbuf_data->m_buffer);
            delete resource->m_raw_uniformbuf_data;
            break;
        default:
            jeecs::debug::logerr("Unknown resource type to close.");
            return;
        }

        if (resource->m_path)
            je_mem_free((void*)resource->m_path);

        resource->m_custom_resource = nullptr;

        if (resource->m_raw_ref_count != nullptr)
        {
            delete std::launder(reinterpret_cast<std::atomic_uint32_t*>(resource->m_raw_ref_count));
            resource->m_raw_ref_count = nullptr;
        }
        _jegl_free_resource_instance(resource);
    }
}

jegl_resource* _create_resource()
{
    jegl_resource* res = new jegl_resource();
    memset(res, 0, sizeof(res));
    return res;
}

jegl_resource* jegl_create_texture(size_t width, size_t height, jegl_texture::format format, jegl_texture::sampling sampling)
{
    jegl_resource* texture = _create_resource();
    texture->m_type = jegl_resource::TEXTURE;
    texture->m_raw_texture_data = new jegl_texture();
    texture->m_path = nullptr;

    if ((format & jegl_texture::format::FORMAT_MASK) == 0)
    {
        texture->m_raw_texture_data->m_pixels = (jegl_texture::pixel_data_t*)stbi__malloc(width * height * format);
        assert(texture->m_raw_texture_data->m_pixels);

        memset(texture->m_raw_texture_data->m_pixels, 0, width * height * format);
    }
    else
        // For special format texture such as depth, do not alloc for pixel.
        texture->m_raw_texture_data->m_pixels = nullptr;

    texture->m_raw_texture_data->m_width = width;
    texture->m_raw_texture_data->m_height = height;
    texture->m_raw_texture_data->m_format = format;
    texture->m_raw_texture_data->m_sampling = sampling;
    texture->m_raw_texture_data->m_modified = false;

    return texture;
}

bool _jegl_read_texture_sampling_cache(const char* path, jegl_texture::sampling* samp)
{
    if (jeecs_file* image_cache = jeecs_load_cache_file(path, IMAGE_CACHE_VERSION, -1))
    {
        jeecs_file_read(samp, sizeof(jegl_texture::sampling), 1, image_cache);
        jeecs_file_close(image_cache);
        return true;
    }
    return false;
}

bool _jegl_write_texture_sampling_cache(const char* path, jegl_texture::sampling samp)
{
    if (void* image_cache = jeecs_create_cache_file(path, IMAGE_CACHE_VERSION, 0))
    {
        jeecs_write_cache_file(&samp, sizeof(jegl_texture::sampling), 1, image_cache);
        jeecs_close_cache_file(image_cache);
        return true;
    }
    return false;
}

jegl_resource* _jegl_load_shader_cache(jeecs_file* cache_file, const char* path)
{
    assert(cache_file != nullptr);

    jegl_shader* _shader = new jegl_shader();

    jegl_resource* shader = _create_resource();
    shader->m_type = jegl_resource::SHADER;
    shader->m_raw_shader_data = _shader;

    uint64_t vertex_glsl_src_len, fragment_glsl_src_len;

    // 1. Read generated source
    jeecs_file_read(&vertex_glsl_src_len, sizeof(uint64_t), 1, cache_file);
    _shader->m_vertex_glsl_src = (const char*)je_mem_alloc((size_t)vertex_glsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_vertex_glsl_src), sizeof(char), (size_t)vertex_glsl_src_len, cache_file);
    const_cast<char*>(_shader->m_vertex_glsl_src)[(size_t)vertex_glsl_src_len] = 0;

    jeecs_file_read(&fragment_glsl_src_len, sizeof(uint64_t), 1, cache_file);
    _shader->m_fragment_glsl_src = (const char*)je_mem_alloc((size_t)fragment_glsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_fragment_glsl_src), sizeof(char), (size_t)fragment_glsl_src_len, cache_file);
    const_cast<char*>(_shader->m_fragment_glsl_src)[(size_t)fragment_glsl_src_len] = 0;

    // 2. read shader config
    jeecs_file_read(&_shader->m_depth_test, sizeof(jegl_shader::depth_test_method), 1, cache_file);
    jeecs_file_read(&_shader->m_depth_mask, sizeof(jegl_shader::depth_mask_method), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_src_mode, sizeof(jegl_shader::blend_method), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_dst_mode, sizeof(jegl_shader::blend_method), 1, cache_file);
    jeecs_file_read(&_shader->m_cull_mode, sizeof(jegl_shader::cull_mode), 1, cache_file);

    // 3. read if shader is enable to shared?
    jeecs_file_read(&_shader->m_enable_to_shared, sizeof(bool), 1, cache_file);

    // 4. read and generate custom variable & uniform block informs
    
    // 4.1 read and generate custom variable
    uint64_t custom_uniform_count;
    jeecs_file_read(&custom_uniform_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_custom_uniforms = nullptr;

    jegl_shader::unifrom_variables* last_create_variable = nullptr;
    for (uint64_t i = 0; i < custom_uniform_count; ++i)
    {
        jegl_shader::unifrom_variables* current_variable = new jegl_shader::unifrom_variables();
        if (_shader->m_custom_uniforms == nullptr)
            _shader->m_custom_uniforms = current_variable;

        if (last_create_variable != nullptr)
            last_create_variable->m_next = current_variable;

        // 4.1.1 read name
        uint64_t uniform_name_len;
        jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
        current_variable->m_name = (const char*)je_mem_alloc((size_t)uniform_name_len + 1);
        jeecs_file_read(const_cast<char*>(current_variable->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
        const_cast<char*>(current_variable->m_name)[(size_t)uniform_name_len] = 0;

        // 4.1.2 read type
        jeecs_file_read(&current_variable->m_uniform_type, sizeof(jegl_shader::uniform_type), 1, cache_file);

        // 4.1.3 read data
        static_assert(sizeof(current_variable->mat4x4) == sizeof(float[4][4]));
        jeecs_file_read(&current_variable->mat4x4, sizeof(float[4][4]), 1, cache_file);

        current_variable->m_index = jeecs::typing::INVALID_UINT32;
        current_variable->m_updated = false;

        last_create_variable = current_variable;
        current_variable->m_next = nullptr;
    }

    // 4.2 read uniform block informs
    uint64_t custom_uniform_block_count;
    jeecs_file_read(&custom_uniform_block_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_custom_uniform_blocks = nullptr;

    jegl_shader::uniform_blocks* last_create_block = nullptr;
    for (uint64_t i = 0; i < custom_uniform_block_count; ++i)
    {
        jegl_shader::uniform_blocks* current_block = new jegl_shader::uniform_blocks();
        if (_shader->m_custom_uniform_blocks == nullptr)
            _shader->m_custom_uniform_blocks = current_block;

        if (last_create_block != nullptr)
            last_create_block->m_next = current_block;

        // 4.2.1 read name
        uint64_t uniform_name_len;
        jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
        current_block->m_name = (const char*)je_mem_alloc((size_t)uniform_name_len + 1);
        jeecs_file_read(const_cast<char*>(current_block->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
        const_cast<char*>(current_block->m_name)[(size_t)uniform_name_len] = 0;

        // 4.2.2 read binding place
        static_assert(sizeof(current_block->m_specify_binding_place) == sizeof(uint32_t));
        jeecs_file_read(&current_block->m_specify_binding_place, sizeof(uint32_t), 1, cache_file);

        last_create_block = current_block;
        current_block->m_next = nullptr;
    }

    jeecs_file_close(cache_file);

    shader->m_path = jeecs::basic::make_new_string(path);
    return shader;
}

void _jegl_create_shader_cache(jegl_resource* shader_resource, wo_integer_t virtual_file_crc64)
{
    assert(shader_resource->m_path != nullptr
        && shader_resource->m_raw_shader_data
        && shader_resource->m_type == jegl_resource::type::SHADER);

    if (auto* cachefile = jeecs_create_cache_file(
        shader_resource->m_path,
        SHADER_CACHE_VERSION,
        virtual_file_crc64))
    {
        auto* raw_shader_data = shader_resource->m_raw_shader_data;

        uint64_t vertex_glsl_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_glsl_src);
        uint64_t fragment_glsl_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_glsl_src);

        // 1. write shader generated source to cache
        jeecs_write_cache_file(&vertex_glsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_glsl_src, sizeof(char), (size_t)vertex_glsl_src_len, cachefile);

        jeecs_write_cache_file(&fragment_glsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_glsl_src, sizeof(char), (size_t)fragment_glsl_src_len, cachefile);

        // 2. write shader config to cache
        /*
            depth_test_method   m_depth_test;
            depth_mask_method   m_depth_mask;
            blend_method        m_blend_src_mode, m_blend_dst_mode;
            cull_mode           m_cull_mode;
        */
        jeecs_write_cache_file(&raw_shader_data->m_depth_test, sizeof(jegl_shader::depth_test_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_depth_mask, sizeof(jegl_shader::depth_mask_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_src_mode, sizeof(jegl_shader::blend_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_dst_mode, sizeof(jegl_shader::blend_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_cull_mode, sizeof(jegl_shader::cull_mode), 1, cachefile);

        // 3. write if shader is enable to shared?
        jeecs_write_cache_file(&raw_shader_data->m_enable_to_shared, sizeof(bool), 1, cachefile);

        // 4. write shader custom variable & uniform block informs.
        uint64_t count_for_uniform = 0;
        uint64_t count_for_uniform_block = 0;

        auto* custom_uniform = raw_shader_data->m_custom_uniforms;
        while (custom_uniform)
        {
            ++count_for_uniform;
            custom_uniform = custom_uniform->m_next;
        }
        auto* custom_uniform_block = raw_shader_data->m_custom_uniform_blocks;
        while (custom_uniform_block)
        {
            ++count_for_uniform_block;
            custom_uniform_block = custom_uniform_block->m_next;
        }

        // 4.1 write shader custom variable
        jeecs_write_cache_file(&count_for_uniform, sizeof(uint64_t), 1, cachefile);
        custom_uniform = raw_shader_data->m_custom_uniforms;
        while (custom_uniform)
        {
            // 4.1.1 write name
            uint64_t uniform_name_len = (uint64_t)strlen(custom_uniform->m_name);
            jeecs_write_cache_file(&uniform_name_len, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(custom_uniform->m_name, sizeof(char), (size_t)uniform_name_len, cachefile);

            // 4.1.2 write type
            jeecs_write_cache_file(&custom_uniform->m_uniform_type, sizeof(jegl_shader::uniform_type), 1, cachefile);

            // 4.1.3 write data
            static_assert(sizeof(custom_uniform->mat4x4) == sizeof(float[4][4]));
            jeecs_write_cache_file(&custom_uniform->mat4x4, sizeof(float[4][4]), 1, cachefile);

            custom_uniform = custom_uniform->m_next;
        }

        // 4.2 write shader custom uniform block informs
        jeecs_write_cache_file(&count_for_uniform_block, sizeof(uint64_t), 1, cachefile);
        custom_uniform_block = raw_shader_data->m_custom_uniform_blocks;
        while (custom_uniform_block)
        {
            // 4.2.1 write name
            uint64_t uniform_block_name_len = (uint64_t)strlen(custom_uniform_block->m_name);
            jeecs_write_cache_file(&uniform_block_name_len, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(custom_uniform_block->m_name, sizeof(char), (size_t)uniform_block_name_len, cachefile);

            // 4.2.2 write place
            static_assert(sizeof(custom_uniform_block->m_specify_binding_place) == sizeof(uint32_t));
            jeecs_write_cache_file(&custom_uniform_block->m_specify_binding_place, sizeof(uint32_t), 1, cachefile);

            custom_uniform_block = custom_uniform_block->m_next;
        }

        jeecs_close_cache_file(cachefile);
    }
}

jegl_resource* jegl_load_shader_source(const char* path, const char* src, bool is_virtual_file)
{
    if (is_virtual_file)
    {
        if (jeecs_file* shader_cache = jeecs_load_cache_file(path, SHADER_CACHE_VERSION, wo_crc64_str(src)))
            return _jegl_load_shader_cache(shader_cache, path);
    }

    wo_vm vmm = wo_create_vm();
    if (!wo_load_source(vmm, path, src))
    {
        // Compile error
        jeecs::debug::logerr("Fail to load shader: %s.\n%s", path, wo_get_compile_error(vmm, WO_DEFAULT));
        wo_close_vm(vmm);
        return nullptr;
    }

    wo_run(vmm);

    auto generate_shader_func = wo_extern_symb(vmm, "shader::generate");
    if (!generate_shader_func)
    {
        jeecs::debug::logerr("Fail to load shader: %s. you should import je::shader.", path);
        wo_close_vm(vmm);
        return nullptr;
    }
    if (wo_value retval = wo_invoke_rsfunc(vmm, generate_shader_func, 0))
    {
        void* shader_graph = wo_pointer(retval);

        jegl_shader* _shader = new jegl_shader();
        jegl_shader_generate_glsl(shader_graph, _shader);

        jegl_resource* shader = _create_resource();
        shader->m_type = jegl_resource::SHADER;
        shader->m_raw_shader_data = _shader;
        shader->m_path = jeecs::basic::make_new_string(path);

        _jegl_create_shader_cache(shader, is_virtual_file ? wo_crc64_str(src) : 0);

        wo_close_vm(vmm);
        return shader;
    }
    else
    {
        jeecs::debug::logerr("Fail to load shader: %s: %s.", path, wo_get_runtime_error(vmm));
        wo_close_vm(vmm);
        return nullptr;
    }

}

struct shared_resource_instance
{
    jegl_resource* m_resource;
    bool m_tobe_replace;

    JECS_DISABLE_MOVE_AND_COPY(shared_resource_instance);
    shared_resource_instance()
        : m_resource(nullptr)
        , m_tobe_replace(false)
    {

    }
    ~shared_resource_instance()
    {
        jegl_close_resource(m_resource);
    }
};
std::unordered_map<std::string, std::unique_ptr<shared_resource_instance>> shared_resource_list;
std::shared_mutex shared_resource_list_smx;
bool enable_shared_resource_list = false;

void jegl_set_able_shared_resources(bool able)
{
    std::lock_guard g1(shared_resource_list_smx);
    enable_shared_resource_list = able;
    if (able)
    {
        assert(shared_resource_list.empty());
    }
    else
        shared_resource_list.clear();
}
bool jegl_mark_shared_resources_outdated(const char* path)
{
    std::lock_guard g1(shared_resource_list_smx);
    if (enable_shared_resource_list)
    {
        auto fnd = shared_resource_list.find(path);
        if (fnd != shared_resource_list.end()
            && fnd->second->m_tobe_replace == false)
        {
            fnd->second->m_tobe_replace = true;
            return true;
        }
    }
    return false;
}

jegl_resource* _jegl_share_resource(jegl_resource* resource)
{
    assert(resource != nullptr);
    if (resource->m_raw_ref_count == nullptr)
    {
        static_assert(sizeof(uint32_t) == sizeof(std::atomic_uint32_t));
        static_assert(std::atomic_uint32_t::is_always_lock_free);

        resource->m_raw_ref_count = std::launder(reinterpret_cast<uint32_t*>(new std::atomic_uint32_t(1)));
        assert(std::launder(reinterpret_cast<std::atomic_uint32_t*>(resource->m_raw_ref_count))->load() == 1);
    }
    std::launder(reinterpret_cast<std::atomic_uint32_t*>(resource->m_raw_ref_count))->fetch_add(1);

    return resource;
}

jegl_resource* jegl_try_update_shared_resource(jegl_resource* resource, jegl_resource::type aimtype)
{
    assert(resource != nullptr && resource->m_path != nullptr);
    do
    {
        std::lock_guard g1(shared_resource_list_smx);
        if (!enable_shared_resource_list)
            return resource;

        auto fnd = shared_resource_list.find(resource->m_path);
        if (fnd != shared_resource_list.end() 
            && fnd->second->m_tobe_replace == false
            && fnd->second->m_resource->m_type == aimtype)
        {
            jegl_close_resource(resource);
            return _jegl_share_resource(fnd->second->m_resource);
        }

        // Create new fxxking one!
        auto shared_info = std::make_unique<shared_resource_instance>();
        shared_info->m_resource = resource;
        shared_resource_list[resource->m_path] = std::move(shared_info);
        return _jegl_share_resource(resource);

    } while (0);

    // Cannot be here
    abort();
}
jegl_resource* jegl_try_load_shared_resource(const char* path)
{
    do
    {
        std::shared_lock sg1(shared_resource_list_smx);

        if (!enable_shared_resource_list)
            return nullptr;

        auto fnd = shared_resource_list.find(path);
        if (fnd != shared_resource_list.end() && fnd->second->m_tobe_replace == false)
        {
            return _jegl_share_resource(fnd->second->m_resource);
        }

    } while (0);
   
    return nullptr;
}

jegl_resource* jegl_load_shader(const char* path)
{
    auto* shared_shader = jegl_try_load_shared_resource(path);
    if (shared_shader != nullptr && shared_shader->m_type == jegl_resource::type::SHADER)
        return shared_shader;

    if (jeecs_file* shader_cache = jeecs_load_cache_file(path, SHADER_CACHE_VERSION, 0))
    {
        auto* shader_resource = _jegl_load_shader_cache(shader_cache, path);

        assert(shader_resource != nullptr && shader_resource->m_raw_shader_data != nullptr);

        if (shader_resource->m_raw_shader_data->m_enable_to_shared)
            return jegl_try_update_shared_resource(shader_resource, jegl_resource::type::SHADER);
        return shader_resource;
    }
    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        char* src = (char*)je_mem_alloc(texfile->m_file_length + 1);
        jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
        src[texfile->m_file_length] = 0;

        jeecs_file_close(texfile);

        auto* shader_resource = jegl_load_shader_source(path, src, false);
        if (shader_resource != nullptr)
        {
            assert(shader_resource != nullptr && shader_resource->m_raw_shader_data != nullptr);
            if (shader_resource->m_raw_shader_data->m_enable_to_shared)
                return jegl_try_update_shared_resource(shader_resource, jegl_resource::type::SHADER);
            return shader_resource;
        }
        return nullptr;
        
    }
    jeecs::debug::logerr("Fail to open file: '%s'", path);
    return nullptr;
}

jegl_resource* jegl_load_texture(const char* path)
{
    auto* shared_texture = jegl_try_load_shared_resource(path);
    if (shared_texture != nullptr && shared_texture->m_type == jegl_resource::type::TEXTURE)
        return shared_texture;

    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        jegl_resource* texture = _create_resource();
        texture->m_type = jegl_resource::TEXTURE;
        texture->m_raw_texture_data = new jegl_texture();
        texture->m_path = jeecs::basic::make_new_string(path);

        unsigned char* fbuf = new unsigned char[texfile->m_file_length];
        jeecs_file_read(fbuf, sizeof(unsigned char), texfile->m_file_length, texfile);
        int w, h, cdepth;

        stbi_set_flip_vertically_on_load(true);
        texture->m_raw_texture_data->m_pixels = stbi_load_from_memory(
            fbuf,
            (int)texfile->m_file_length,
            &w, &h, &cdepth,
            STBI_rgb_alpha
        );

        delete[] fbuf;
        jeecs_file_close(texfile);

        if (texture->m_raw_texture_data->m_pixels == nullptr)
        {
            jeecs::debug::logerr("Cannot load texture: Invalid image format of file: '%s'", path);
            delete texture->m_raw_texture_data;
            delete texture;
            return nullptr;
        }

        // Read samping config from cache file...
        jegl_texture::sampling sampling_method;
        if (!_jegl_read_texture_sampling_cache(path, &sampling_method))
            sampling_method = jegl_texture::sampling::DEFAULT;

        texture->m_raw_texture_data->m_width = (size_t)w;
        texture->m_raw_texture_data->m_height = (size_t)h;
        texture->m_raw_texture_data->m_format = jegl_texture::RGBA;
        texture->m_raw_texture_data->m_sampling = sampling_method;
        texture->m_raw_texture_data->m_modified = false;

        return jegl_try_update_shared_resource(texture, jegl_resource::type::TEXTURE);
    }

    jeecs::debug::logerr("Cannot load texture: Failed to open file: '%s'", path);
    return nullptr;
}

jegl_resource* jegl_load_vertex(const char* path)
{
    // TODO: Not support now!
    abort();
}

jegl_resource* jegl_create_vertex(
    jegl_vertex::type type,
    const float* datas,
    const size_t* format,
    size_t data_length,
    size_t format_length)
{
    jegl_resource* vertex = _create_resource();
    vertex->m_type = jegl_resource::VERTEX;
    vertex->m_raw_vertex_data = new jegl_vertex();

    size_t datacount_per_point = 0;
    for (size_t i = 0; i < format_length; ++i)
        datacount_per_point += format[i];

    auto point_count = data_length / datacount_per_point;

    if (data_length % datacount_per_point)
        jeecs::debug::logwarn("Vertex data & format not matched, please check.");

    vertex->m_raw_vertex_data->m_type = type;
    vertex->m_raw_vertex_data->m_format_count = format_length;
    vertex->m_raw_vertex_data->m_point_count = point_count;
    vertex->m_raw_vertex_data->m_data_count_per_point = datacount_per_point;

    vertex->m_raw_vertex_data->m_vertex_datas
        = (float*)je_mem_alloc(point_count * datacount_per_point * sizeof(float));
    vertex->m_raw_vertex_data->m_vertex_formats
        = (size_t*)je_mem_alloc(format_length * sizeof(size_t));

    memcpy(vertex->m_raw_vertex_data->m_vertex_datas, datas,
        point_count * datacount_per_point * sizeof(float));

    memcpy(vertex->m_raw_vertex_data->m_vertex_formats, format,
        format_length * sizeof(size_t));

    // Calc size by default:
    if (format[0] == 3)
    {
        float
            x_min = INFINITY, x_max = -INFINITY,
            y_min = INFINITY, y_max = -INFINITY,
            z_min = INFINITY, z_max = -INFINITY;
        // First data group is position(by default).
        for (size_t i = 0; i < point_count; ++i)
        {
            float x = datas[0 + i * datacount_per_point];
            float y = datas[1 + i * datacount_per_point];
            float z = datas[2 + i * datacount_per_point];

            x_min = std::min(x, x_min);
            x_max = std::max(x, x_max);
            y_min = std::min(y, y_min);
            y_max = std::max(y, y_max);
            z_min = std::min(z, z_min);
            z_max = std::max(z, z_max);
        }

        vertex->m_raw_vertex_data->m_size_x = x_max - x_min;
        vertex->m_raw_vertex_data->m_size_y = y_max - y_min;
        vertex->m_raw_vertex_data->m_size_z = z_max - z_min;
    }
    else
    {
        jeecs::debug::logwarn("Position data of vertex(%p) mismatch, first data should be position and with length '3'", vertex);
        vertex->m_raw_vertex_data->m_size_x = 1.f;
        vertex->m_raw_vertex_data->m_size_y = 1.f;
        vertex->m_raw_vertex_data->m_size_z = 1.f;
    }

    return vertex;
}


jegl_resource* jegl_create_framebuf(
    size_t width, size_t height, 
    const jegl_texture::format* attachment_formats, 
    const jegl_texture::sampling* attachment_samlings, 
    size_t attachment_count)
{
    if (width == 0 || height == 0 || attachment_count == 0)
    {
        jeecs::debug::logwarn("Failed to create invalid framebuffer: size is zero or no attachment.");
        return nullptr;
    }

    jegl_resource* framebuf = _create_resource();
    framebuf->m_type = jegl_resource::FRAMEBUF;
    framebuf->m_raw_framebuf_data = new jegl_frame_buffer();
    framebuf->m_path = nullptr;

    framebuf->m_raw_framebuf_data->m_attachment_count = attachment_count;
    framebuf->m_raw_framebuf_data->m_width = width;
    framebuf->m_raw_framebuf_data->m_height = height;

    jeecs::basic::resource<jeecs::graphic::texture>* attachments = nullptr;
    if (attachment_count > 0)
    {
        attachments = new jeecs::basic::resource<jeecs::graphic::texture>[attachment_count];

        for (size_t i = 0; i < attachment_count; ++i)
            attachments[i] = jeecs::graphic::texture::create(width, height,
                jegl_texture::format(attachment_formats[i] | jegl_texture::format::FRAMEBUF), jegl_texture::sampling::DEFAULT);
    }

    framebuf->m_raw_framebuf_data->m_output_attachments = (jegl_frame_buffer::attachment_t*)attachments;

    return framebuf;
}

jegl_resource* jegl_create_uniformbuf(
    size_t binding_place,
    size_t length)
{
    jegl_resource* uniformbuf = _create_resource();
    uniformbuf->m_type = jegl_resource::UNIFORMBUF;
    uniformbuf->m_raw_uniformbuf_data = new jegl_uniform_buffer();
    uniformbuf->m_path = nullptr;

    uniformbuf->m_raw_uniformbuf_data->m_buffer = (uint8_t*)je_mem_alloc(length);
    uniformbuf->m_raw_uniformbuf_data->m_buffer_size = length;
    uniformbuf->m_raw_uniformbuf_data->m_buffer_binding_place = binding_place;

    uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset = 0;
    uniformbuf->m_raw_uniformbuf_data->m_update_length = 0;

    return uniformbuf;
}

void jegl_update_uniformbuf(jegl_resource* uniformbuf, const void* buf, size_t update_offset, size_t update_length)
{
    assert(uniformbuf->m_type == jegl_resource::UNIFORMBUF && update_length != 0);

    if (update_length != 0)
    {
        memcpy(uniformbuf->m_raw_uniformbuf_data->m_buffer + update_offset, buf, update_length);
        if (uniformbuf->m_raw_uniformbuf_data->m_update_length != 0)
        {
            size_t new_begin = std::min(uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset, update_offset);
            size_t new_end = std::max(
                uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset
                + uniformbuf->m_raw_uniformbuf_data->m_update_length,
                update_offset + update_length);

            uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset = new_begin;
            uniformbuf->m_raw_uniformbuf_data->m_update_length = new_end - new_begin;
        }
        else
        {
            uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset = update_offset;
            uniformbuf->m_raw_uniformbuf_data->m_update_length = update_length;
        }
    }
}

void jegl_draw_vertex(jegl_resource* vert)
{
    _current_graphic_thread->m_apis->draw_vertex(_current_graphic_thread, vert);
}

void jegl_clear_framebuffer(jegl_resource* framebuffer)
{
    _current_graphic_thread->m_apis->clear_rend_buffer(_current_graphic_thread, framebuffer);
}
void jegl_clear_framebuffer_color(jegl_resource* framebuffer)
{
    _current_graphic_thread->m_apis->clear_rend_buffer_color(_current_graphic_thread, framebuffer);
}
void jegl_clear_framebuffer_depth(jegl_resource* framebuffer)
{
    _current_graphic_thread->m_apis->clear_rend_buffer_depth(_current_graphic_thread, framebuffer);
}

void jegl_rend_to_framebuffer(jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
{
    _current_graphic_thread->m_apis->set_rend_buffer(_current_graphic_thread, framebuffer, x, y, w, h);
}

void jegl_using_texture(jegl_resource* texture, size_t pass)
{
    _current_graphic_thread->m_apis->bind_texture(_current_graphic_thread, texture, pass);
}

uint32_t jegl_uniform_location(jegl_resource* shader, const char* name)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    return _current_graphic_thread->m_apis->get_uniform_location(_current_graphic_thread, shader, name);
}

void jegl_uniform_int(jegl_resource* shader, uint32_t location, int value)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(_current_graphic_thread, shader, location, jegl_shader::INT, &value);
}

void jegl_uniform_float(jegl_resource* shader, uint32_t location, float value)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(_current_graphic_thread, shader, location, jegl_shader::FLOAT, &value);
}

void jegl_uniform_float2(jegl_resource* shader, uint32_t location, float x, float y)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    jeecs::math::vec2 value = { x,y };
    _current_graphic_thread->m_apis->set_uniform(_current_graphic_thread, shader, location, jegl_shader::FLOAT2, &value);
}

void jegl_uniform_float3(jegl_resource* shader, uint32_t location, float x, float y, float z)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    jeecs::math::vec3 value = { x,y,z };
    _current_graphic_thread->m_apis->set_uniform(_current_graphic_thread, shader, location, jegl_shader::FLOAT3, &value);
}

void jegl_uniform_float4(jegl_resource* shader, uint32_t location, float x, float y, float z, float w)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    jeecs::math::vec4 value = { x,y,z,w };
    _current_graphic_thread->m_apis->set_uniform(_current_graphic_thread, shader, location, jegl_shader::FLOAT4, &value);
}

void jegl_uniform_float4x4(jegl_resource* shader, uint32_t location, const float(*mat)[4])
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(_current_graphic_thread, shader, location, jegl_shader::FLOAT4X4, mat);
}
