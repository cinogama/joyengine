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
    let vpos = je_mv * float4::create(v.vertex, 1.);

    return v2f{
        pos = je_p * vpos,
        vpos = vpos,
        uv = v.uv, // No need for uvtrans(...).

        light_vpos = movement(je_mv),
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

    for (let _, (x, y, weight) : bias_weight)
    {
        let shadow_weight = texture(shadow, uv + reso_inv * float2::create(x, y) * bias)->x;
        shadow_factor = max(shadow_factor, shadow_weight * weight);
    }
    return float::one - shadow_factor;
}

SHADER_FUNCTION!
func apply_point_light_effect(
    fragment_vpos: float3,
    fragment_vnorm: float3,
    light_vpos : float3,
    shadow_factor: float,
)
{
    let f2l = fragment_vpos - light_vpos;
    let ldistance = length(f2l);
    let ldirection = normalize(f2l);

    let distance_factor = max(float::one - ldistance, float::zero);

    // Linear decay for better effect.
    let fade_factor = distance_factor / je_light2d_decay;
    let point_light_factor = fragment_vnorm->dot(ldirection->negative) / (ldistance + 1.) * fade_factor;

    return shadow_factor 
        * max(float::zero, point_light_factor) 
        * je_color->w
        * je_color->xyz;
}

public func frag(vf: v2f)
{
    // let albedo_buffer = je_light2d_defer_albedo;
    // let self_lumine = je_light2d_defer_self_luminescence;
    let vspace_position = je_light2d_defer_vspace_position;
    let vspace_normalize = je_light2d_defer_vspace_normalize;
    let shadow_buffer = je_light2d_defer_shadow;
   
    let uv = uvframebuf((vf.pos->xy / vf.pos->w + float2::new(1., 1.)) /2.);

    let pixvpos = vf.vpos->xyz / vf.vpos->w;

    let vposition = texture(vspace_position, uv)->xyz;
    let vnormalize = texture(vspace_normalize, uv)->xyz;
    let uvdistance = clamp(length((vf.uv - float2::new(0.5, 0.5)) * 2.), 0., 1.);
    let fgdistance = distance(vposition, pixvpos);
    let shadow_factor = multi_sampling_for_bias_shadow(shadow_buffer, je_light2d_resolutin, uv);

    let decay = je_light2d_decay;

    let fade_factor = pow(float::one - uvdistance, decay);
    let result = je_color->xyz * je_color->w * shadow_factor * fade_factor
        / (fgdistance + 1.0) * step(pixvpos->z, vposition->z);

    return fout{
        color = float4::create(
            result + apply_point_light_effect(
                vposition,
                vnormalize,
                vf.light_vpos,
                shadow_factor),
            0.),
    };
}
