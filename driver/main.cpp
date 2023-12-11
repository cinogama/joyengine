/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/

#include "jeecs.hpp"

extern "C"
{
    JE_EXPORT int NvOptimusEnablement = 0x00000001;
    JE_EXPORT int AmdPowerXpressRequestHighPerformance = 0x00000001;
}

int main(int argc, char** argv)
{
    using namespace jeecs;

    je_init(argc, argv);

    auto* guard = new jeecs::typing::type_unregister_guard();
    {
        entry::module_entry(guard);
        {
            graphic::shader::create("*/aaa.shader", R"(
// Default shader
import je::shader;

VAO_STRUCT! vin {
    vertex : float3,
};
using v2f = struct {
    pos : float4,
};
using fout = struct {
    color : float4
};

func fff_impl(in: float, in2: float2)
{
    return (in2 * in)->x;
}
let fff_vin = vertex_in::create();
let fff_args = (fff_vin->in:<float>(0), fff_vin->in:<float2>(1));
let fff = function::register("fff", fff_args, fff_impl(fff_args...));

#macro SHADER_FUNCTION
{
    /*
    SHADER_FUNCTION!
    public func add(a: float, b: float)
    */
}


public let vert = 
\v: vin = v2f{ pos = je_mvp * vertex_pos }
    where vertex_pos = float4::create(v.vertex, fff(1., float2::one));;

public let frag = 
\_: v2f = fout{ color = float4::create(t, 0., t, 1.) }
    where t = je_time->y();;

)");

            je_main_script_entry();
        }
        entry::module_leave(guard);
    }
    delete guard;
    je_finish();
}
