#define JE_IMPL
#include "jeecs.hpp"

struct jegl_shader_value
{
    enum type
    {
        INIT_VALUE,
        CALC_VALUE,
    };

    type m_type;
};

struct jegl_shader_float : jegl_shader_value
{

};

RS_API rs_api jeecs_shader_float_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm, nullptr, nullptr, [](void*) {});
}