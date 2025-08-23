// Light2DParallel.shader
import woo::std;

import je::shader;
import je::shader::light2d;

SHARED  (false);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

VAO_STRUCT! vin {
    vertex  : float3,
};

using v2f = struct {
    pos         : float4,
    light_vdir  : float3,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    return v2f{
        pos = vec4(v.vertex, 0.5) * 2.,
        light_vdir = normalize(
            je_mv->float3x3 * float3::const(0., -1., 1.)),
    };
}

SHADER_FUNCTION!
func multi_sampling_for_bias_shadow(
    shadow  : texture2d, 
    uv      : float2)
{
    let mut shadow_factor = float::zero;

    let bias = [
        (0., 1., 0.25),
        (-1., 0., 0.25),
        (0., 0., 0.75),
        (1., 0., 0.25),
        (0., -1., 0.25),
    ];

    let bias_step = float2::const(1.5, 1.5) / je_light2d_resolution;
    for (let (x, y, f) : bias)
    {
        shadow_factor += f * texture(shadow, uv + bias_step * vec2(x, y))->x;
    }
    return min(shadow_factor, float::one);
}

SHADER_FUNCTION!
func apply_light_normal_effect(
    fragment_vnorm  : float3,
    light_vdir      : float3,
)
{
    let matched_light_factor = fragment_vnorm->dot(light_vdir->negative);
    return max(float::zero, matched_light_factor) ;
}

//let Albedo      = je_light2d_defer_albedo;
//let SelfLumine  = je_light2d_defer_self_luminescence;
//let VPosition   = je_light2d_defer_vspace_position;
let VNormalize  = je_light2d_defer_vspace_normalize;
let Shadow      = je_light2d_defer_shadow;

let normal_z_offset = uniform("normal_z_offset", float::one);

public func frag(vf: v2f)
{
    let uv = (vf.pos->xy / vf.pos->w + float2::const(1., 1.)) /2.;
    let shadow_factor = 1. - multi_sampling_for_bias_shadow(Shadow, uv);

    let vnormalize = normalize(
        texture(VNormalize, uv)->xyz - vec3(0., 0., normal_z_offset));

    let color_intensity = 
        je_color->xyz 
        * je_color->w 
        * shadow_factor;

    return fout{
        color = vec4(
            color_intensity * apply_light_normal_effect(
                vnormalize,
                vf.light_vdir),
            0.),
    };
}
