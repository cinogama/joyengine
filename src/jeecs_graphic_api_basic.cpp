#define JE_IMPL
#include "jeecs.hpp"

struct jegl_thread_notifier
{
    jegl_interface_config m_interface_config;
    std::atomic_flag m_graphic_terminate_flag;

    std::mutex       m_update_mx;
    std::atomic_bool m_update_flag;
    std::condition_variable m_update_waiter;

    std::atomic_bool m_reboot_flag;
};

void _graphic_work_thread(jegl_thread* thread, void(*frame_rend_work)(void*), void* arg)
{
    do
    {
        auto custom_interface = thread->m_apis->init_interface(thread, &thread->_m_thread_notifier->m_interface_config);

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

            if (!thread->m_apis->update_interface(thread, custom_interface))
            {
                // graphic thread want to exit. mark stop update
                thread->m_stop_update = true;
            }
            else
            {
                frame_rend_work(arg);
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
    void(*frame_rend_work)(void*),
    void* arg)
{
    jegl_thread* thread_handle = jeecs::basic::create_new<jegl_thread>();

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