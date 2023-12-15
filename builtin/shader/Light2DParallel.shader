// Light2DParallel.shader

import je::shader;
import je::shader::light2d;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

VAO_STRUCT! vin {
    vertex  : float3,
};

using v2f = struct {
    pos     : float4,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    return v2f{
        pos = float4::create(v.vertex, 0.5) * 2.,
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

public func frag(vf: v2f)
{
    // let albedo_buffer = je_light2d_defer_albedo;
    // let self_lumine = je_light2d_defer_self_luminescence;
    // let visual_coord = je_light2d_defer_visual_coord;

    let shadow_buffer = je_light2d_defer_shadow;

    let uv = uvframebuf((vf.pos->xy / vf.pos->w + float2::new(1., 1.)) /2.);
    let shadow_factor = multi_sampling_for_bias_shadow(shadow_buffer, je_light2d_resolutin, uv);

    let result = je_color->xyz * je_color->w * shadow_factor;

    return fout{
        color = float4::create(result, 0.),
    };
}
