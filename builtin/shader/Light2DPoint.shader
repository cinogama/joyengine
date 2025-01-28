// Light2DPoint.shader

import je::shader;
import je::shader::light2d;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2, // We will use uv to decided light fade.
};

using v2f = struct {
    pos         : float4,
    vpos        : float4,
    uv          : float2,
    light_vpos  : float3,
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
        uv = v.uv, // No need for uvtrans(...).

        light_vpos = movement(je_mv),
    };
}

SHADER_FUNCTION!
func multi_sampling_for_bias_shadow(
    shadow  : texture2d, 
    uv      : float2)
{
    let mut shadow_factor = float::zero;

    let bias = [
        (0., 1.),
        (-1., 0.),    
        (0., 0.),     
        (1., 0.),
        (0., -1.),
    ];

    let bias_step = float2::const(0.5, 0.5) / je_light2d_resolution;
    for (let (x, y) : bias)
    {
        shadow_factor = max(
            shadow_factor,
            texture(shadow, uv + bias_step * vec2(x, y))->x);
    }
    return shadow_factor;
}

SHADER_FUNCTION!
func apply_point_light_effect(
    fragment_vpos   : float3,
    fragment_vnorm  : float3,
    light_vpos      : float3,
    light_fade      : float,
    shadow_factor   : float
)
{
    let f2l = fragment_vpos - light_vpos;
    let ldistance = length(f2l);
    let ldirection = normalize(f2l);

    let point_light_factor = 
        fragment_vnorm->dot(ldirection->negative) / (ldistance + 1.);

    return shadow_factor 
        * light_fade
        * max(float::zero, point_light_factor) 
        * je_color->w
        * je_color->xyz;
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
    let uvdistance = clamp(length((vf.uv - float2::const(0.5, 0.5)) * 2.), 0., 1.);
    let fgdistance = distance(vposition, pixvpos);

    let shadow_factor = 1. - multi_sampling_for_bias_shadow(Shadow, uv);
    let decay = je_light2d_decay;

    let fade_factor = pow(1. - uvdistance, decay);

    let color_intensity = je_color->xyz * je_color->w * shadow_factor;

    let result = color_intensity 
        * fade_factor / (fgdistance + 1.0)
        * step(pixvpos->z, vposition->z);

    return fout{
        color = vec4(
            result + apply_point_light_effect(
                vposition,
                vnormalize,
                vf.light_vpos,
                fade_factor,
                shadow_factor),
            0.),
    };
}
