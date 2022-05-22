#define JE_IMPL
#include "jeecs.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

            if (thread->_m_thread_notifier->m_reboot_flag)
                break;

            auto* del_res = _destroing_graphic_resources.pick_all();
            while (del_res)
            {
                auto* cur_del_res = del_res;
                del_res = del_res->last;

                if (cur_del_res->m_destroy_resource->m_graphic_thread == thread
                    && cur_del_res->m_destroy_resource->m_graphic_thread_version == thread->m_version)
                {
                    thread->m_apis->close_resource(cur_del_res->m_destroy_resource);
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
            {
                // graphic thread want to exit. mark stop update
                thread->m_stop_update = true;
            }
            else
            {
                frame_rend_work(arg, thread);
            }

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
    // This function is not thread safe.
    if (!_current_graphic_thread)
        return jeecs::debug::log_error("Graphic resource only usable in graphic thread.");

    if (!resource->m_graphic_thread)
        resource->m_graphic_thread = _current_graphic_thread;
    if (_current_graphic_thread != resource->m_graphic_thread)
        return jeecs::debug::log_error("This resource has been used in graphic thread:%p.", resource->m_graphic_thread);

    _current_graphic_thread->m_apis->using_resource(resource);
}

void jegl_close_resource(jegl_resource* resource)
{
    switch (resource->m_type)
    {
    case jegl_resource::TEXTURE:
        // close resource's raw data, then send this resource to closing-queue
        stbi_image_free(resource->m_raw_texture_data->m_pixels);
        jeecs::basic::destroy_free(resource->m_raw_texture_data);
        resource->m_raw_texture_data = nullptr;
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

jegl_resource* jegl_load_texture(const char* path)
{
    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        jegl_resource* texture = jeecs::basic::create_new<jegl_resource>();
        texture->m_graphic_thread = nullptr;
        texture->m_type = jegl_resource::TEXTURE;
        texture->m_raw_texture_data = jeecs::basic::create_new<jegl_texture>();

        unsigned char* fbuf = new unsigned char[texfile->m_file_length];
        int w, h, cdepth;

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

        return texture;
    }

    jeecs::debug::log_error("Fail to open file: '%s'", path);
    return nullptr;
}