#define JE_IMPL
#include "jeecs.hpp"

struct jegl_shader_value
{
    enum type
    {
        INIT_VALUE = 0x0000,
        CALC_VALUE = 0x0001,
        //
        FLOAT = 0x0010,
        FLOAT2 = 0x0020,
        FLOAT3 = 0x0040,
        FLOAT4 = 0x0080,
        
        FLOAT2x2 = 0x0100,
        FLOAT3x3 = 0x0200,
        FLOAT4x4 = 0x0400,
    };

    type m_type;
    union
    {
        float m_float;
        float m_float2[2];
        float m_float3[3];
        float m_float4[4];
    };
};

RS_API rs_api jeecs_shader_float_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm, nullptr, nullptr, [](void*) {});
}

const char* shader_wrapper_path = "je/shader.rsn";
const char* shader_wrapper_src = R"(
// JoyEngineECS RScene shader wrapper

using float = gchandle
namespace float
{
    extern("libjoyecs", "jeecs_shader_float_create");
    func create(var init_val:real) : float;

    extern("libjoyecs", "jeecs_shader_float_create");
    func create(var init_val:float) : float;

    extern("libjoyecs", "jeecs_shader_float_add_float");
    func operator + (var a:float, var b:float) : float;

    extern("libjoyecs", "jeecs_shader_float_sub_float");
    func operator - (var a:float, var b:float) : float;

    extern("libjoyecs", "jeecs_shader_float_mul_float");
    func operator * (var a:float, var b:float) : float;

    extern("libjoyecs", "jeecs_shader_float_div_float");
    func operator / (var a:float, var b:float) : float;
}


)";