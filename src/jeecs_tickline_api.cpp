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

WO_API wo_api jeecs_tickline_launch_vm_form_ticklinesys(wo_vm vm, wo_value args, size_t argc)
{
    auto covm = wo_borrow_vm(jeecs::TicklineSystem::ENTRY_TICKLINE_WOOLANG_VIRTUAL_MACHINE);

    wo_value f = args + 1;
    wo_value cargs = args + 2;

    size_t args_len = wo_lengthof(cargs);

    for (size_t i = args_len; i > 0; --i)
        wo_push_val(covm, wo_struct_get(cargs, i - 1));

    if (wo_valuetype(f) == WO_INTEGER_TYPE)
        wo_dispatch_rsfunc(covm, wo_int(f), args_len);
    else
    {
        assert(wo_valuetype(f) == WO_CLOSURE_TYPE);
        wo_dispatch_closure(covm, f, args_len);
    }

    return wo_ret_gchandle(vm, covm, nullptr,
        [](void* vmptr)
        {
            wo_release_vm((wo_vm)vmptr);
        }
    );
}

WO_API wo_api jeecs_tickline_dispatch_vm(wo_vm vm, wo_value args, size_t argc)
{
    auto* covm = (wo_vm)wo_pointer(args + 0);

    auto result = wo_dispatch(covm);
    if (result == WO_CONTINUE)
        return wo_ret_ok_bool(vm, false);
    else {
        wo_api r;
        if (result == nullptr)
            r = wo_ret_err_string(vm, wo_get_runtime_error(covm));
        else
            r = wo_ret_ok_bool(vm, true);
        wo_gchandle_close(args + 0);
        return r;
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