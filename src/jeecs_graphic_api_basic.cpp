#define JE_IMPL
#include "jeecs.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <condition_variable>

struct jegl_thread_notifier
{
    jegl_interface_config m_interface_config;
    std::atomic_flag m_graphic_terminate_flag;

    std::mutex       m_update_mx;
    std::atomic_bool m_update_flag;
    std::condition_variable m_update_waiter;

    std::atomic_bool m_reboot_flag;
};

thread_local jegl_thread* _current_graphic_thread = nullptr;
void* const INVALID_RESOURCE = (void*)(size_t)-1;

jeecs::basic::atomic_list<jegl_resource::jegl_destroy_resouce> _destroing_graphic_resources;
void _graphic_work_thread(jegl_thread* thread, void(*frame_rend_work)(void*, jegl_thread*), void* arg)
{
    _current_graphic_thread = thread;
    do
    {
        auto custom_interface = thread->m_apis->init_interface(thread, &thread->_m_thread_notifier->m_interface_config);
        ++thread->m_version;
        while (thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set())
        {
            do
            {
                std::unique_lock uq1(thread->_m_thread_notifier->m_update_mx);
                thread->_m_thread_notifier->m_update_waiter.wait(uq1, [thread]()->bool {
                    return thread->_m_thread_notifier->m_update_flag;
                    });
            } while (0);
            // Ready for rend..

            if (!thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set()
                || thread->_m_thread_notifier->m_reboot_flag)
                break;

            auto* del_res = _destroing_graphic_resources.pick_all();
            while (del_res)
            {
                auto* cur_del_res = del_res;
                del_res = del_res->last;

                if (cur_del_res->m_destroy_resource->m_graphic_thread == thread
                    && cur_del_res->m_destroy_resource->m_graphic_thread_version == thread->m_version)
                {
                    if (cur_del_res->m_destroy_resource->m_ptr != INVALID_RESOURCE)
                        thread->m_apis->close_resource(thread, cur_del_res->m_destroy_resource);

                    jeecs::basic::destroy_free(cur_del_res->m_destroy_resource);
                    jeecs::basic::destroy_free(cur_del_res);
                }
                else if (--cur_del_res->m_retry_times)
                    // Need re-try
                    _destroing_graphic_resources.add_one(cur_del_res);
                else
                {
                    // Free this
                    jeecs::debug::log_warn("Resource %p cannot free by correct thread, maybe it is out-dated? Free it!"
                        , cur_del_res->m_destroy_resource);
                    jeecs::basic::destroy_free(cur_del_res->m_destroy_resource);
                    jeecs::basic::destroy_free(cur_del_res);
                }
            }

            if (!thread->m_apis->update_interface(thread, custom_interface))
                // graphic thread want to exit. mark stop update
                thread->m_stop_update = true;
            else
                frame_rend_work(arg, thread);

            if (!thread->m_apis->late_update_interface(thread, custom_interface))
                thread->m_stop_update = true;

            std::lock_guard g1(thread->_m_thread_notifier->m_update_mx);
            thread->_m_thread_notifier->m_update_flag = false;
            thread->_m_thread_notifier->m_update_waiter.notify_all();
        }

        thread->m_apis->shutdown_interface(thread, custom_interface);

        if (!thread->_m_thread_notifier->m_reboot_flag)
            break;

        thread->_m_thread_notifier->m_reboot_flag = false;
    } while (true);

}

//////////////////////////////////// API /////////////////////////////////////////

jegl_thread* jegl_start_graphic_thread(
    jegl_interface_config config,
    jeecs_api_register_func_t register_func,
    void(*frame_rend_work)(void*, jegl_thread*),
    void* arg)
{
    jegl_thread* thread_handle = jeecs::basic::create_new<jegl_thread>();

    thread_handle->m_version = 0;
    thread_handle->_m_thread_notifier = jeecs::basic::create_new<jegl_thread_notifier>();
    thread_handle->m_apis = jeecs::basic::create_new<jegl_graphic_api>();

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
            jeecs::debug::log_fatal("GraphicAPI function: %zu is invalid.", (size_t)(reador - (void**)thread_handle->m_apis));
        }
    }
    if (err_api_no)
    {
        jeecs::basic::destroy_free(thread_handle->_m_thread_notifier);
        jeecs::basic::destroy_free(thread_handle->m_apis);
        jeecs::basic::destroy_free(thread_handle);

        jeecs::debug::log_fatal("Fail to start up graphic thread, abort and return nullptr.");
        return nullptr;
    }

    // Take place.
    thread_handle->_m_thread_notifier->m_interface_config = config;
    thread_handle->_m_thread_notifier->m_graphic_terminate_flag.test_and_set();
    thread_handle->_m_thread_notifier->m_update_flag = false;
    thread_handle->_m_thread_notifier->m_reboot_flag = false;

    thread_handle->_m_thread =
        jeecs::basic::create_new<std::thread>(
            _graphic_work_thread,
            thread_handle,
            frame_rend_work,
            arg);

    return thread_handle;
}

void jegl_terminate_graphic_thread(jegl_thread* thread)
{
    assert(thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set());
    thread->_m_thread_notifier->m_graphic_terminate_flag.clear();

    do
    {
        std::lock_guard g1(thread->_m_thread_notifier->m_update_mx);
        thread->_m_thread_notifier->m_update_flag = true;
        thread->_m_thread_notifier->m_update_waiter.notify_all();
    } while (0);

    thread->_m_thread->join();

    jeecs::basic::destroy_free(thread->_m_thread_notifier);
    jeecs::basic::destroy_free(thread);
}

bool jegl_update(jegl_thread* thread)
{
    if (thread->m_stop_update)
        return false;

    do
    {
        std::lock_guard g1(thread->_m_thread_notifier->m_update_mx);
        thread->_m_thread_notifier->m_update_flag = true;
        thread->_m_thread_notifier->m_update_waiter.notify_all();
    } while (0);

    // Start Frame, then wait frame end...~

    do
    {
        std::unique_lock uq1(thread->_m_thread_notifier->m_update_mx);
        thread->_m_thread_notifier->m_update_waiter.wait(uq1, [thread]()->bool {
            return !thread->_m_thread_notifier->m_update_flag;
            });
    } while (0);

    return true;
}

void jegl_reboot_graphic_thread(jegl_thread* thread_handle, jegl_interface_config config)
{
    thread_handle->_m_thread_notifier->m_interface_config = config;
    thread_handle->_m_thread_notifier->m_reboot_flag = true;
}

void jegl_using_resource(jegl_resource* resource)
{
    bool need_init_resouce = false;
    // This function is not thread safe.
    if (!_current_graphic_thread)
        return jeecs::debug::log_error("Graphic resource only usable in graphic thread.");

    if (!resource->m_graphic_thread)
    {
        need_init_resouce = true;
        resource->m_graphic_thread = _current_graphic_thread;
        resource->m_graphic_thread_version = _current_graphic_thread->m_version;
    }
    if (_current_graphic_thread != resource->m_graphic_thread)
        return jeecs::debug::log_error("This resource has been used in graphic thread:%p.", resource->m_graphic_thread);
    if (resource->m_graphic_thread_version != _current_graphic_thread->m_version)
    {
        need_init_resouce = true;
        resource->m_graphic_thread_version = _current_graphic_thread->m_version;
    }
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

            if (uniform_vars->m_updated)
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
                case jegl_shader::uniform_type::INT:
                    jegl_uniform_int(resource, uniform_vars->m_index, uniform_vars->n);
                    break;
                default:
                    jeecs::debug::log_error("Unsupport uniform variable type."); break;
                    break;
                }
            }

            uniform_vars = uniform_vars->m_next;
        }
    }
}

void jegl_close_resource(jegl_resource* resource)
{
    switch (resource->m_type)
    {
    case jegl_resource::TEXTURE:
        // close resource's raw data, then send this resource to closing-queue
        stbi_image_free(resource->m_raw_texture_data->m_pixels);
        if (resource->m_raw_texture_data->m_path)
            je_mem_free((void*)resource->m_raw_texture_data->m_path);
        jeecs::basic::destroy_free(resource->m_raw_texture_data);
        resource->m_raw_texture_data = nullptr;
        break;
    case jegl_resource::SHADER:
        // close resource's raw data, then send this resource to closing-queue
        jegl_shader_free_generated_glsl(resource->m_raw_shader_data);
        if (resource->m_raw_shader_data->m_path)
            je_mem_free((void*)resource->m_raw_shader_data->m_path);
        jeecs::basic::destroy_free(resource->m_raw_shader_data);
        resource->m_raw_shader_data = nullptr;
        break;
    case jegl_resource::VERTEX:
        // close resource's raw data, then send this resource to closing-queue
        je_mem_free((void*)resource->m_raw_vertex_data->m_vertex_datas);
        je_mem_free((void*)resource->m_raw_vertex_data->m_vertex_formats);
        if (resource->m_raw_vertex_data->m_path)
            je_mem_free((void*)resource->m_raw_vertex_data->m_path);
        jeecs::basic::destroy_free(resource->m_raw_vertex_data);
        resource->m_raw_vertex_data = nullptr;
        break;
    default:
        jeecs::debug::log_error("Unknown resource type to close.");
        return;
    }
    // Send this resource to destroing list;
    auto* del_res = new jegl_resource::jegl_destroy_resouce;
    del_res->m_retry_times = 15;
    del_res->m_destroy_resource = resource;
    _destroing_graphic_resources.add_one(del_res);
}

JE_API void jegl_get_windows_size(size_t* x, size_t* y)
{
    _current_graphic_thread->m_apis->get_windows_size(_current_graphic_thread, x, y);
}

jegl_resource* jegl_create_texture(size_t width, size_t height, jegl_texture::texture_format format)
{
    jegl_resource* texture = jeecs::basic::create_new<jegl_resource>();
    texture->m_graphic_thread = nullptr;
    texture->m_type = jegl_resource::TEXTURE;
    texture->m_raw_texture_data = jeecs::basic::create_new<jegl_texture>();
    texture->m_raw_texture_data->m_path = nullptr;
    texture->m_ptr = INVALID_RESOURCE;

    texture->m_raw_texture_data->m_pixels = (jegl_texture::pixel_data_t*)stbi__malloc(width * height * format);
    assert(texture->m_raw_texture_data->m_pixels);

    texture->m_raw_texture_data->m_width = width;
    texture->m_raw_texture_data->m_height = height;
    texture->m_raw_texture_data->m_format = format;
    texture->m_raw_texture_data->m_sampling = jegl_texture::texture_sampling::LINEAR;

    return texture;
}

jegl_resource* jegl_load_texture(const char* path)
{
    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        jegl_resource* texture = jeecs::basic::create_new<jegl_resource>();
        texture->m_graphic_thread = nullptr;
        texture->m_type = jegl_resource::TEXTURE;
        texture->m_raw_texture_data = jeecs::basic::create_new<jegl_texture>();
        texture->m_raw_texture_data->m_path = jeecs::basic::make_new_string(path);
        texture->m_ptr = INVALID_RESOURCE;

        unsigned char* fbuf = new unsigned char[texfile->m_file_length];
        jeecs_file_read(fbuf, sizeof(unsigned char), texfile->m_file_length, texfile);
        int w, h, cdepth;

        stbi_set_flip_vertically_on_load(false);
        texture->m_raw_texture_data->m_pixels = stbi_load_from_memory(
            fbuf,
            texfile->m_file_length,
            &w, &h, &cdepth,
            STBI_rgb_alpha
        );

        delete[] fbuf;

        if (texture->m_raw_texture_data->m_pixels == nullptr)
        {
            jeecs::debug::log_error("Fail to load texture form file: '%s'", path);
            jeecs::basic::destroy_free(texture->m_raw_texture_data);
            jeecs::basic::destroy_free(texture);
            return nullptr;
        }

        texture->m_raw_texture_data->m_width = (size_t)w;
        texture->m_raw_texture_data->m_height = (size_t)h;
        texture->m_raw_texture_data->m_format = jegl_texture::RGBA;
        texture->m_raw_texture_data->m_sampling = jegl_texture::texture_sampling::LINEAR;

        return texture;
    }

    jeecs::debug::log_error("Fail to open file: '%s'", path);
    return nullptr;
}

jegl_resource* jegl_load_vertex(const char* path)
{
    // TODO: Not support now!
    abort();
}

jegl_resource* jegl_create_vertex(
    jegl_vertex::vertex_type type,
    const float* datas,
    const size_t* format,
    size_t data_length,
    size_t format_length)
{
    jegl_resource* vertex = jeecs::basic::create_new<jegl_resource>();
    vertex->m_graphic_thread = nullptr;
    vertex->m_type = jegl_resource::VERTEX;
    vertex->m_raw_vertex_data = jeecs::basic::create_new<jegl_vertex>();
    vertex->m_raw_vertex_data->m_path = nullptr;
    vertex->m_ptr = INVALID_RESOURCE;

    size_t datacount_per_point = 0;
    for (size_t i = 0; i < format_length; ++i)
        datacount_per_point += format[i];

    auto point_count = data_length / datacount_per_point;

    if (data_length % datacount_per_point)
        jeecs::debug::log_warn("Vertex data & format not matched, please check.");

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
        jeecs::debug::log_warn("Position data of vertex(%p) mismatch, first data should be position and with length '3'", vertex);
        vertex->m_raw_vertex_data->m_size_x = 1.f;
        vertex->m_raw_vertex_data->m_size_y = 1.f;
        vertex->m_raw_vertex_data->m_size_z = 1.f;
    }
    

    return vertex;
}

jegl_resource* jegl_load_shader_source(const char* path, const char* src)
{
    wo_vm vmm = wo_create_vm();
    if (!wo_load_source(vmm, path, src))
    {
        // Compile error
        jeecs::debug::log_error(wo_get_compile_error(vmm, WO_NEED_COLOR));
        jeecs::debug::log_error("Fail to load shader: %s.", path);
        wo_close_vm(vmm);
        return nullptr;
    }

    wo_run(vmm);

    auto generate_shader_func = wo_extern_symb(vmm, "shader::generate");
    if (!generate_shader_func)
    {
        jeecs::debug::log_error("Fail to load shader: %s. you should import je.shader.", path);
        wo_close_vm(vmm);
        return nullptr;
    }
    if (wo_value retval = wo_invoke_rsfunc(vmm, generate_shader_func, 0))
    {
        void* shader_graph = wo_pointer(retval);

        jegl_shader* _shader = jeecs::basic::create_new<jegl_shader>();
        jegl_shader_generate_glsl(shader_graph, _shader);

        jegl_resource* shader = jeecs::basic::create_new<jegl_resource>();
        shader->m_graphic_thread = nullptr;
        shader->m_type = jegl_resource::SHADER;
        shader->m_raw_shader_data = _shader;
        shader->m_raw_shader_data->m_path = jeecs::basic::make_new_string(path);
        shader->m_ptr = INVALID_RESOURCE;

        wo_close_vm(vmm);
        return shader;
    }
    else
    {
        jeecs::debug::log_error("Fail to load shader: %s: %s.", path, wo_get_runtime_error(vmm));
        wo_close_vm(vmm);
        return nullptr;
    }

}
jegl_resource* jegl_load_shader(const char* path)
{
    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        char* src = (char*)je_mem_alloc(texfile->m_file_length + 1);
        jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
        src[texfile->m_file_length] = 0;

        jeecs_file_close(texfile);
        

        return jegl_load_shader_source(path, src);
    }
    jeecs::debug::log_error("Fail to open file: '%s'", path);
    return nullptr;
}

jegl_thread* jegl_current_thread()
{
    return _current_graphic_thread;
}

void jegl_draw_vertex(jegl_resource* vert)
{
    _current_graphic_thread->m_apis->draw_vertex(vert);
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
    _current_graphic_thread->m_apis->bind_texture(texture, pass);
}

void jegl_update_shared_uniform(size_t offset, size_t datalen, const void* data)
{
    _current_graphic_thread->m_apis->update_shared_uniform(_current_graphic_thread, offset, datalen, data);
}

int jegl_uniform_location(jegl_resource* shader, const char* name)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    return _current_graphic_thread->m_apis->get_uniform_location(shader, name);
}

void jegl_uniform_int(jegl_resource* shader, int location, int value)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(shader, location, jegl_shader::INT, &value);
}

void jegl_uniform_float(jegl_resource* shader, int location, float value)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(shader, location, jegl_shader::FLOAT, &value);
}

void jegl_uniform_float2(jegl_resource* shader, int location, float x, float y)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    jeecs::math::vec2 value = { x,y };
    _current_graphic_thread->m_apis->set_uniform(shader, location, jegl_shader::FLOAT2, &value);
}

void jegl_uniform_float3(jegl_resource* shader, int location, float x, float y, float z)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    jeecs::math::vec3 value = { x,y,z };
    _current_graphic_thread->m_apis->set_uniform(shader, location, jegl_shader::FLOAT3, &value);
}

void jegl_uniform_float4(jegl_resource* shader, int location, float x, float y, float z, float w)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    jeecs::math::vec4 value = { x,y,z,w };
    _current_graphic_thread->m_apis->set_uniform(shader, location, jegl_shader::FLOAT4, &value);
}

void jegl_uniform_float4x4(jegl_resource* shader, int location, const float(*mat)[4])
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(shader, location, jegl_shader::FLOAT4X4, mat);
}