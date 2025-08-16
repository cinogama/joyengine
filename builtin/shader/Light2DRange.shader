// Light2DRange.shader
import woo::std;

import je::shader;
import je::shader::light2d;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

VAO_STRUCT! vin {
    vertex  : float3,
    strength: float,
};

using v2f = struct {
    pos         : float4,
    vpos        : float4,
    light_vpos  : float3,
    strength    : float,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    let vpos = je_mv * vec4(v.vertex, 1.);
    return v2f{
        pos = je_p * vpos,
        vpos = vpos,
        light_vpos = movement(je_mv),
        strength = v.strength,
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
    light_vdir      : float3
)
{
    let matched_light_factor = fragment_vnorm->dot(light_vdir->negative);
    return max(float::zero, matched_light_factor) ;
}

// let Albedo       = je_light2d_defer_albedo;
// let SelfLumine   = je_light2d_defer_self_luminescence;
let VPosition   = je_light2d_defer_vspace_position;
let VNormalize  = je_light2d_defer_vspace_normalize;
let Shadow      = je_light2d_defer_shadow;

public func frag(vf: v2f)
{  
    let uv = uvframebuf((vf.pos->xy / vf.pos->w + float2::const(1., 1.)) /2.);

    let pixvpos = vf.vpos->xyz / vf.vpos->w;

    let vposition = texture(VPosition, uv)->xyz;
    let vnormalize = texture(VNormalize, uv)->xyz;
    let fgdistance = distance(vposition, pixvpos);

    let shadow_factor = 1. - multi_sampling_for_bias_shadow(Shadow, uv);
    let decay = je_light2d_decay;

    let fade_factor = pow(vf.strength, decay);

    let color_intensity = 
        je_color->xyz 
        * je_color->w 
        * shadow_factor
        * fade_factor / (fgdistance + 1.0);

    return fout{
        color = vec4(
            color_intensity * apply_light_normal_effect(
                vnormalize,
                normalize(vposition - vf.light_vpos)),
            0.),
    };
}
