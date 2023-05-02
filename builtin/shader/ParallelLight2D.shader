// Unlit.shader
// (C)Cinogama project. 2022. 版权所有

import je.shader;
import je.shader.light2d;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

VAO_STRUCT! vin
{
    vertex: float3,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    color: float4
};

public func vert(v: vin)
{
    return v2f{
        pos = float4::create(v.vertex, 0.5) * 2.,
    };
}

public func multi_sampling_for_bias_shadow(shadow: texture2d, reso: float2, uv: float2)
{
    let mut shadow_factor = float_zero;
    let bias = 2.;

    let bias_weight = [
        /*(-1., 1., 0.08),*/    (0., 1., 0.08),     /*(1., 1., 0.08),*/
        (-1., 0., 0.08),    (0., 0., 0.72),     (1., 0., 0.08),
        /*(-1., -1., 0.08),*/   (0., -1., 0.08),    /*(1., -1., 0.08),*/
    ];

    let reso_inv = float2_one / reso;

    for (let _, (x, y, weight) : bias_weight)
    {
        shadow_factor = shadow_factor + texture(
            shadow, uv + reso_inv * float2::create(x, y) * bias
        )->x * weight;  
    }
    return clamp(shadow_factor, 0., 1.);
}

public func frag(vf: v2f)
{
    // let albedo_buffer = je_light2d_defer_albedo;
    // let self_lumine = je_light2d_defer_self_luminescence;
    // let visual_coord = je_light2d_defer_visual_coord;

    let shadow_buffer = je_light2d_defer_shadow;

    let uv = (vf.pos->xy / vf.pos->w + float2::new(1., 1.)) /2.;
    let shadow_factor = multi_sampling_for_bias_shadow(shadow_buffer, je_shadow2d_resolutin, uv);

    let result = je_color->xyz * je_color->w * (float_one - shadow_factor);

    return fout{
        color = float4::create(result, 0.),
    };
}
