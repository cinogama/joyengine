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

    do
    {
        /*jeecs::typing::type_unregister_guard guard;
        entry::module_entry(&guard);
        {
            je_main_script_entry();
        }
        entry::module_leave(&guard);*/

        basic::resource<graphic::shader> r = graphic::shader::create("!/demo.shader", R"(
import je::shader;

SHARED  (true);
ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2,
};

using v2f = struct {
    pos     : float4,
    uv      : float2,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    return v2f{
        pos = je_mvp * vec4(v.vertex, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}

let NearestSampler  = sampler2d::create(NEAREST, NEAREST, NEAREST, REPEAT, REPEAT);
let Main            = uniform_texture:<texture2d>("Main", NearestSampler, 0);

public func frag(vf: v2f)
{
    return fout{
        color = alphatest(je_color * vec4(texture(Main, vf.uv)->xyz, 1.0)),
    };
}
)");
        printf("%s\n\n%s\n", 
            r ? r->resouce()->m_raw_shader_data->m_vertex_hlsl_src : "", 
            r ? r->resouce()->m_raw_shader_data->m_fragment_hlsl_src : "");
        je_clock_sleep_for(1.0);
        system("pause");

    } while (0);

    je_finish();
}
