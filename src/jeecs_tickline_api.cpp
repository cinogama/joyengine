#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "jeecs_core_tickline_system.hpp"

WO_API wo_api jeecs_tickline_register_global_vm(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_panic(vm, "");

    jeecs::TicklineSystem::ENTRY_TICKLINE_WOOLANG_VIRTUAL_MACHINE = vm;
    return wo_ret_void(vm);
}

struct _je_redispathable_vm
{
    wo_vm m_vm;
    wo_value m_func;
    size_t m_argc;
};

WO_API wo_api jeecs_tickline_launch_vm_form_ticklinesys(wo_vm vm, wo_value args, size_t argc)
{
    auto covm = wo_borrow_vm(jeecs::TicklineSystem::ENTRY_TICKLINE_WOOLANG_VIRTUAL_MACHINE);

    wo_value f = args + 1;
    wo_value cargs = args + 2;

    size_t args_len = wo_lengthof(cargs);

    // 将目标调用函数放进去存起来，未来重新launch时可以取出
    f = wo_push_val(covm, f);

    for (size_t i = args_len; i > 0; --i)
        wo_push_val(covm, wo_struct_get(cargs, i - 1));

    if (wo_valuetype(f) == WO_INTEGER_TYPE)
        wo_dispatch_rsfunc(covm, wo_int(f), args_len);
    else
    {
        assert(wo_valuetype(f) == WO_CLOSURE_TYPE);
        wo_dispatch_closure(covm, f, args_len);
    }

    _je_redispathable_vm* rvmm = new _je_redispathable_vm;
    rvmm->m_vm = covm;
    rvmm->m_func = f;
    rvmm->m_argc = args_len;

    return wo_ret_gchandle(vm, rvmm, nullptr,
        [](void* vmptr)
        {
            wo_release_vm(((_je_redispathable_vm*)vmptr)->m_vm);
            delete (_je_redispathable_vm*)vmptr;
        }
    );
}

WO_API wo_api jeecs_tickline_dispatch_vm(wo_vm vm, wo_value args, size_t argc)
{
    auto* rvmm = (_je_redispathable_vm*)wo_pointer(args + 0);

    auto result = wo_dispatch(rvmm->m_vm);
    if (result == WO_CONTINUE)
        return wo_ret_ok_bool(vm, false);
    else {
        if (result == nullptr)
        {
            wo_api r = wo_ret_err_string(vm, wo_get_runtime_error(rvmm->m_vm));
            wo_gchandle_close(args + 0);
            return r;
        }
        else
        {
            // 重新launch
            if (wo_valuetype(rvmm->m_func) == WO_INTEGER_TYPE)
                wo_dispatch_rsfunc(rvmm->m_vm, wo_int(rvmm->m_func), rvmm->m_argc);
            else
            {
                assert(wo_valuetype(rvmm->m_func) == WO_CLOSURE_TYPE);
                wo_dispatch_closure(rvmm->m_vm, rvmm->m_func, rvmm->m_argc);
            }
            return wo_ret_ok_bool(vm, true);
        }
    }
}

WO_API wo_api jeecs_tickline_get_current_systems_by_uid(wo_vm vm, wo_value args, size_t argc)
{
    wo_value result = wo_push_map(vm);
    wo_value key = wo_push_empty(vm);
    wo_value val = wo_push_empty(vm);

    for (auto& [uid, es] : jeecs::TicklineSystem::CURRENT_TICKLINE_SYSTEM_INSTANCE->m_anchored_entities)
    {
        wo_set_string(key, uid);
    }
}

const char* jeecs_tickline_api_path = "je/tickline.wo";
const char* jeecs_tickline_api_src = R"(
import je;

namespace je::tickline
{
    public using VM = gchandle
    {
        extern("libjoyecs", "jeecs_tickline_dispatch_vm")
        public func dispatch(self: VM)=> result<bool, string>;
    }

    extern("libjoyecs", "jeecs_tickline_launch_vm_form_ticklinesys")
    public func launch<FT, ArgTs>(f: FT, args: ArgTs)=> VM
        where f(args...) is void;

    namespace engine
    {
        extern("libjoyecs", "jeecs_tickline_register_global_vm")
        public func init()=> void;
    }
}
)";