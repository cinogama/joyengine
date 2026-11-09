#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"
#include "jeecs_basic_bootup.hpp"

#include "wo.h"
#include "woort.h"

#include <optional>

namespace jeecs::woolang_runtime {}

using namespace std;
using namespace jeecs;
using namespace jeecs::woolang_runtime;

namespace jeecs::woolang_runtime
{
    namespace
    {
        struct PanicHook
        {
            woort_vm* m_vm;
            woort_GCPin* m_func;
            woort_PanicHandlerFunction m_last_hook;
        };
        optional<PanicHook> panic_hook;

        static woort_PanicHandler_Action _jedbg_hook_woolang_panic(
            woort_vm* vm,
            const char* functionname,
            const char* src_file,
            int lineno,
            int rterrcode,
            const char* reason)
        {
            std::string trace;

            if (vm == nullptr)
                trace = "<No vm running>";
            else
            {
                woort_VMRuntime_TraceCallstack_Iter trace_iter;
                woort_VMRuntime_trace_begin(vm, &trace_iter);

                woort_VMRuntime_TraceCallstack frame;
                while (woort_VMRuntime_trace_next(&trace_iter, &frame))
                {
                    const char* const func = frame.m_function_name;
                    const char* const file = frame.m_file_or_lib_name;
                    const size_t line = frame.m_location_begin[0];
                    const size_t col = frame.m_location_begin[1];

                    char buf[512];
                    if (func != nullptr && file != nullptr)
                    {
                        if (frame.m_has_location)
                            snprintf(buf, sizeof(buf), "    at %s (%s:%zu:%zu)\n",
                                func, file, line + 1, col + 1);
                        else
                            snprintf(buf, sizeof(buf), "    at %s (%s)\n", func, file);
                    }
                    else if (func != nullptr)
                        snprintf(buf, sizeof(buf), "    at %s\n", func);
                    else if (file != nullptr)
                    {
                        if (frame.m_has_location && line != 0)
                            snprintf(buf, sizeof(buf), "    at <unknown> (%s:%zu:%zu)\n",
                                file, line + 1, col + 1);
                        else
                            snprintf(buf, sizeof(buf), "    at <unknown> (%s)\n", file);
                    }
                    else
                        snprintf(buf, sizeof(buf), "    at <unknown>\n");

                    trace += buf;
                }
            }

            jeecs::debug::logerr("Woolang Panic(%x):%s (%s in %s: %u):\n%s",
                rterrcode, reason, functionname, src_file, lineno, trace.c_str());

            auto& hook = panic_hook.value();

            woort_vm* const last_vm = woort_vm_swap(hook.m_vm);
            {
                woort_value _je_global_panic_hooker_s;
                if (!woort_push_reserve(7, &_je_global_panic_hooker_s))
                    // Stack overflow.
                    return WOORT_PANIC_HANDLER_ACTION_USE_DEFAULT_HANDLER;

                woort_set_string(_je_global_panic_hooker_s + 0, src_file);
                woort_set_int(_je_global_panic_hooker_s + 1, (woort_Int)lineno);
                woort_set_string(_je_global_panic_hooker_s + 2, functionname);
                woort_set_int(_je_global_panic_hooker_s + 3, (woort_Int)rterrcode);
                woort_set_string(_je_global_panic_hooker_s + 4, reason);
                woort_set_string(_je_global_panic_hooker_s + 5, trace.c_str());

                woort_GCPin_get(
                    _je_global_panic_hooker_s + 6,
                    hook.m_func,
                    0);

                const auto invoke_result =
                    woort_invoke(WOORT_IGNORE, _je_global_panic_hooker_s + 6);

                woort_pop(7);

                if (invoke_result != WOORT_VM_CALL_STATUS_NORMAL)
                {
                    jeecs::debug::logwarn("Engine's woolang panic hook failed, try default.");
                    return WOORT_PANIC_HANDLER_ACTION_USE_DEFAULT_HANDLER;
                }
            }
            (void)woort_vm_swap(last_vm);

            return WOORT_PANIC_HANDLER_ACTION_ABORT;
        }
    }

    void bootup(int argc, char** argv)
    {
        const wo_WooDyn_Functions funcs = {
            &woort_init,
            &woort_shutdown,
            &woort_dylib_load,
            &woort_dylib_load_func,
            &woort_dylib_unload,
        };
        wo_init(argc, argv, &funcs);
    }
    void shutdown()
    {
        if (panic_hook.has_value())
        {
            auto& hook = panic_hook.value();

            woort_vm_close(hook.m_vm);

            const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
            {
                woort_GCPin_destroy(hook.m_func);
            }
            if (entry_tmp_gc_guard)
                woort_GC_sync_marking_unlock();

            (void)woort_set_panic_callback(hook.m_last_hook);

            panic_hook.reset();
        }
        wo_finish(nullptr, nullptr);
    }
}

WOORT_API woort_api wojeapi_editor_register_panic_hook(void)
{
    PanicHook hook;
    hook.m_vm = woort_vm_create();
    if (hook.m_vm == nullptr)
        return woort_ret_panic("Unable to register panic hook: Failed to create vm.");

    if (panic_hook.has_value())
    {
        // ATTENTION: Unsafe for multi thread.
        woort_vm_close(panic_hook.value().m_vm);
        panic_hook.reset();
    }

    hook.m_func = woort_GCPin_create(1);
    woort_GCPin_set(hook.m_func, 0, 0);

    hook.m_last_hook =
        woort_set_panic_callback(&_jedbg_hook_woolang_panic);

    panic_hook.emplace(hook);
 
    return woort_ret_void();
}
