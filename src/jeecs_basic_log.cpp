#define JE_IMPL
#define WO_NEED_ANSI_CONTROL
#include "jeecs.hpp"
#include "jeecs_basic_bootup.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
#   include <Windows.h>
#endif

namespace jeecs::log {}

using namespace std;
using namespace jeecs;
using namespace jeecs::log;

namespace jeecs::log
{
    namespace
    {
        // Pending-queue node; basic::atomic_list chains nodes via `last`.
        struct log_message
        {
            je_log_level_t m_level;
            char* m_text; // heap buffer, owned by this node
            log_message* last;
        };

        // One registered log callback; id/callback/userdata are colocated.
        struct callback_entry
        {
            je_log_regid_t m_id;
            je_log_callback_t m_callback;
            void* m_userdata;
        };

        // Always available, so logs emitted before bootup can already queue up.
        basic::atomic_list<log_message> g_pending;

        atomic_size_t g_next_regid = 0;
        shared_mutex g_callbacks_mx;
        vector<callback_entry> g_callbacks; // iterated in registration order

        atomic_bool g_worker_running = false;
        atomic_bool g_worker_shutdown = false;
        mutex g_worker_mx;
        condition_variable g_worker_cv;
        thread g_worker;

        // "[HH:MM:SS.mmm] " local-time prefix, baked into the message text.
        size_t format_timestamp(char* dst, size_t cap)
        {
            using namespace chrono;
            const auto now = system_clock::now();
            const auto msec = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            const time_t ttime = system_clock::to_time_t(now);
            tm tmbuf{};
#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
            localtime_s(&tmbuf, &ttime);
#else
            localtime_r(&ttime, &tmbuf);
#endif
            const int len = snprintf(
                dst, cap, "[%02d:%02d:%02d.%03d] ",
                tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec, (int)msec.count());
            return (size_t)len;
        }

        struct level_style
        {
            const char* m_tag;
            const char* m_ansi;
        };

        level_style level_style_of(je_log_level_t level)
        {
            switch (level)
            {
            case JE_LOG_NORMAL: return { "NORML", ANSI_HIG };
            case JE_LOG_INFO: return { "INFOR", ANSI_HIC };
            case JE_LOG_WARNING: return { "WARNI", ANSI_HIY };
            case JE_LOG_ERROR: return { "ERROR", ANSI_HIR };
            case JE_LOG_FATAL: return { "FATAL", ANSI_HIM };
            }
            assert(!"unreachable: invalid log level");
            return { "UNKNO", ANSI_HIC };
        }

        void print_message(je_log_level_t level, const char* text)
        {
            const level_style style = level_style_of(level);
            fprintf(stdout, "[%s%s%s] ", style.m_ansi, style.m_tag, ANSI_RST);
            fputs(text, stdout);
            fputc('\n', stdout);
            fflush(stdout);

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
            // Plain-text mirror (no ANSI) so logs stay visible in the debugger
            // of WIN32 GUI builds that have no console attached.
            char prefix[16];
            snprintf(prefix, sizeof(prefix), "[%s] ", style.m_tag);
            OutputDebugStringA(prefix);
            OutputDebugStringA(text);
            OutputDebugStringA("\n");
#endif
        }

        // Deliver one message: callbacks in registration order, then console.
        void dispatch_message(log_message* msg)
        {
            {
                shared_lock callbacks_guard(g_callbacks_mx);
                for (const callback_entry& entry : g_callbacks)
                    entry.m_callback(msg->m_level, msg->m_text, entry.m_userdata);
            }
            print_message(msg->m_level, msg->m_text);
            delete[] msg->m_text;
            delete msg;
        }

        // Drain the pending queue in chronological order.
        void dispatch_pending()
        {
            vector<log_message*> pending;
            for (log_message* msg = g_pending.pick_all(); msg != nullptr; msg = msg->last)
                pending.push_back(msg);

            for (auto it = pending.rbegin(); it != pending.rend(); ++it)
                dispatch_message(*it);
        }

        void log_worker()
        {
            do
            {
                {
                    unique_lock worker_guard(g_worker_mx);
                    g_worker_cv.wait(worker_guard, []
                        {
                            return !g_pending.empty() || g_worker_shutdown.load();
                        });
                }

                dispatch_pending();

            } while (!g_worker_shutdown.load() || !g_pending.empty());
        }
    }

    void bootup(int, char**)
    {
        g_worker_shutdown.store(false);
        g_worker_running.store(true);
        g_worker = thread(log_worker);
    }

    void shutdown()
    {
        {
            lock_guard worker_guard(g_worker_mx);
            g_worker_shutdown.store(true);
        }
        g_worker_cv.notify_all();

        if (g_worker.joinable())
            g_worker.join();
        g_worker_running.store(false);

        // Flush messages that raced in while the worker was exiting.
        dispatch_pending();
    }
}

je_log_regid_t je_log_callback_register(je_log_callback_t callback, /* OPTIONAL */ void* userdata)
{
    const je_log_regid_t regid = ++g_next_regid;

    lock_guard callbacks_guard(g_callbacks_mx);
    g_callbacks.push_back(callback_entry{ regid, callback, userdata });

    return regid;
}

/* OPTIONAL: If registered userdata was null */ void* je_log_callback_unregister(je_log_regid_t regid)
{
    lock_guard callbacks_guard(g_callbacks_mx);

    for (auto it = g_callbacks.begin(); it != g_callbacks.end(); ++it)
    {
        if (it->m_id == regid)
        {
            void* const userdata = it->m_userdata;
            g_callbacks.erase(it);
            return userdata;
        }
    }

    assert(!"je_log_callback_unregister: unknown regid");
    return nullptr;
}

void je_log(je_log_level_t level, const char* format, ...)
{
    va_list va, vb;
    va_start(va, format);
    va_copy(vb, va);

    const size_t text_size = (size_t)vsnprintf(nullptr, 0, format, va);
    va_end(va);

    char timestamp[16];
    const size_t timestamp_size = format_timestamp(timestamp, sizeof(timestamp));

    char* const text = new char[timestamp_size + text_size + 1];
    memcpy(text, timestamp, timestamp_size);
    vsnprintf(text + timestamp_size, text_size + 1, format, vb);
    va_end(vb);

    g_pending.add_one(new log_message{ level, text, nullptr });

    if (g_worker_running.load())
    {
        // Take the mutex around the notify so a worker sitting between its
        // predicate check and the sleep can never miss this wakeup.
        {
            lock_guard worker_guard(g_worker_mx);
        }
        g_worker_cv.notify_one();
    }
    else
    {
        // No worker (pre-bootup / post-shutdown): dispatch on the calling
        // thread instead of letting messages stall in the queue.
        dispatch_pending();
    }
}
