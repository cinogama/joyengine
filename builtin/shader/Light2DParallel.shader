// Light2DParallel.shader

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
        pos = float4::create(v.vertex, 0.5) * 2.,
        light_vdir = normalize(
            je_mv->float3x3 * float3::new(0., -1., 1.)),
    };
}

SHADER_FUNCTION!
func multi_sampling_for_bias_shadow(shadow: texture2d, reso: float2, uv: float2)
{
    let mut shadow_factor = float::zero;
    let bias = 1.;

    let bias_weight = [
        (-1., 1., 0.5),    (0., 1., 0.95),     (1., 1., 0.5),
        (-1., 0., 0.95),    (0., 0., 1.),     (1., 0., 0.95),
        (-1., -1., 0.5),   (0., -1., 0.95),    (1., -1., 0.5),
    ];

    let reso_inv = float2::one / reso;

    for (let (x, y, weight) : bias_weight)
    {
        let shadow_weight = texture(shadow, uv + reso_inv * float2::create(x, y) * bias)->x;
        shadow_factor = max(shadow_factor, shadow_weight * weight);
    }
    return 1. - shadow_factor;
}

SHADER_FUNCTION!
func apply_parallel_light_effect(
    fragment_vnorm  : float3,
    light_vdir      : float3,
    shadow_factor   : float
)
{
    let parallel_light_factor = fragment_vnorm->dot(light_vdir->negative);

    return shadow_factor 
        * max(float::zero, parallel_light_factor) 
        * je_color->w
        * je_color->xyz;
}

//let Albedo      = je_light2d_defer_albedo;
//let SelfLumine  = je_light2d_defer_self_luminescence;
//let VPosition   = je_light2d_defer_vspace_position;
let VNormalize  = je_light2d_defer_vspace_normalize;
let Shadow      = je_light2d_defer_shadow;

let normal_z_offset = uniform("normal_z_offset", float::one);

public func frag(vf: v2f)
{
    let uv = uvframebuf((vf.pos->xy / vf.pos->w + float2::new(1., 1.)) /2.);
    let shadow_factor = multi_sampling_for_bias_shadow(Shadow, je_light2d_resolutin, uv);

    let vnormalize = normalize(
        texture(VNormalize, uv)->xyz - float3::create(0., 0., normal_z_offset));

    return fout{
        color = float4::create(
            apply_parallel_light_effect(
                vnormalize,
                vf.light_vdir,
                shadow_factor),
            0.),
    };
}
