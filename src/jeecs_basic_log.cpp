#define JE_IMPL
#define WO_NEED_ANSI_CONTROL
#include "jeecs.hpp"

#include <cstdio>
#include <cstdarg>

#ifdef _WIN32
#   include <Windows.h>
#endif

#include <cstdarg>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>

#include <iostream>

struct GBarLogmsg
{
    int m_level;
    const char* m_msg;

    GBarLogmsg* last;
};

jeecs::basic::atomic_list<GBarLogmsg> _gbar_log_list;
std::thread _gbar_log_thread;
std::mutex _gbar_log_thread_mx;
std::condition_variable _gbar_log_thread_cv;
std::atomic_bool _gbar_log_shutdown_flag = false;

void _je_log_work()
{
    do
    {
        do
        {
            std::unique_lock ug1(_gbar_log_thread_mx);
            _gbar_log_thread_cv.wait(ug1,
                []()->bool {return !_gbar_log_list.empty() || _gbar_log_shutdown_flag; });
        } while (0);

        GBarLogmsg* msg = _gbar_log_list.pick_all();
        std::list<GBarLogmsg*> reverse_msgs;

        while (msg)
        {
            reverse_msgs.push_front(msg);
            msg = msg->last;
        }

        for (auto* rmsg : reverse_msgs)
        {
            switch (rmsg->m_level)
            {
            case JE_LOG_NORMAL:
                printf("[" ANSI_HIG "NORML" ANSI_RST "] "); break;
            case JE_LOG_INFO:
                printf("[" ANSI_HIC "INFOR" ANSI_RST "] "); break;
            case JE_LOG_WARNING:
                printf("[" ANSI_HIY "WARNI" ANSI_RST "] "); break;
            case JE_LOG_ERROR:
                printf("[" ANSI_HIR "ERROR" ANSI_RST "] "); break;
            case JE_LOG_FATAL:
                printf("[" ANSI_HIM "FATAL" ANSI_RST "] "); break;
            default:
                printf("[" ANSI_HIC "UNKNO" ANSI_RST "] "); break;
            }
            puts(rmsg->m_msg);

            free((void*)(rmsg->m_msg));
            delete rmsg;
        }

    } while (!_gbar_log_shutdown_flag || !_gbar_log_list.empty());

}

void je_log_strat()
{
    _gbar_log_thread = std::move(
        std::thread(_je_log_work)
    );
}

void je_log_shutdown()
{
    do
    {
        std::lock_guard g1(_gbar_log_thread_mx);
        _gbar_log_shutdown_flag = true;
        _gbar_log_thread_cv.notify_one();
    } while (0);

    _gbar_log_thread.join();
}

std::atomic_size_t registered_id = 0;
std::shared_mutex registered_callbacks_mx;
std::unordered_map<size_t, std::pair<void(*)(int, const char*, void*), void*>> registered_callbacks;

size_t je_log_register_callback(void(*func)(int level, const char* msg, void* custom), void* custom)
{
    size_t id = registered_id++;

    std::lock_guard lg(registered_callbacks_mx);
    assert(registered_callbacks.find(id) == registered_callbacks.end());
    registered_callbacks[id] = std::make_pair(func, custom);

    return id;
}

void* je_log_unregister_callback(size_t regid)
{
    std::lock_guard lg(registered_callbacks_mx);
    assert(registered_callbacks.find(regid) != registered_callbacks.end());

    auto * cus = registered_callbacks[regid].second;
    registered_callbacks.erase(regid);

    return cus;
}

void je_log(int level, const char* format, ...)
{
    va_list va, vb;
    va_start(va, format);
    va_copy(vb, va);

    size_t total_buffer_sz = vsnprintf(nullptr, 0, format, va);
    va_end(va);

    char* buf = (char*)malloc(total_buffer_sz + 2);
    vsnprintf(buf, total_buffer_sz + 1, format, vb);
    va_end(vb);

    bool need_notify = false;

    do
    {
        // Invoke callbacks
        std::shared_lock sg(registered_callbacks_mx);
        for (auto& [_, f] : registered_callbacks)
        {
            f.first(level, buf, f.second);
        }
    } while (0);

    _gbar_log_list.add_one(new GBarLogmsg{ level, buf });
    _gbar_log_thread_cv.notify_one();
}