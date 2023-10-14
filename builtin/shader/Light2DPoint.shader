// Light2DPoint.shader
// (C)Cinogama project. 2022. 版权所有

import je::shader;
import je::shader::light2d;

SHARED  (false);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2, // We will use uv to decided light fade.
};

using v2f = struct {
    pos     : float4,
    vpos    : float4,
    uv      : float2
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
    };
}

func multi_sampling_for_bias_shadow(shadow: texture2d, reso: float2, uv: float2)
{
    let mut shadow_factor = float::zero;
    let bias = 1.;

    let bias_weight = [
        (-1., 1., 0.094742),    (0., 1., 0.118318),     (1., 1., 0.094742),
        (-1., 0., 0.118318),    (0., 0., 0.147761),     (1., 0., 0.118318),
        (-1., -1., 0.094742),   (0., -1., 0.118318),    (1., -1., 0.094742),
    ];

    let reso_inv = float2::one / reso;

    for (let _, (x, y, weight) : bias_weight)
    {
        shadow_factor = shadow_factor + texture(
            shadow, uv + reso_inv * float2::create(x, y) * bias
        )->x * weight;  
    }
    return float::one - clamp(float::new(1.5) * shadow_factor, 0., 1.);
}

public func frag(vf: v2f)
{
    // let albedo_buffer = je_light2d_defer_albedo;
    // let self_lumine = je_light2d_defer_self_luminescence;
    let visual_coord = je_light2d_defer_visual_coord;

    let shadow_buffer = je_light2d_defer_shadow;
   
    let uv = (vf.pos->xy / vf.pos->w + float2::new(1., 1.)) /2.;

    let pixvpos = vf.vpos->xyz / vf.vpos->w;

    let vposition = texture(visual_coord, uv);
    let uvdistance = clamp(length((vf.uv - float2::new(0.5, 0.5)) * 2.), 0., 1.);
    let fgdistance = distance(vposition->xyz, pixvpos);
    let shadow_factor = multi_sampling_for_bias_shadow(shadow_buffer, je_light2d_resolutin, uv);

    let decay = je_light2d_decay;

    let fade_factor = pow(float::one - uvdistance, decay);
    let result = je_color->xyz * je_color->w * shadow_factor * fade_factor;

    return fout{
        color = float4::create(result / (fgdistance + 1.0) * step(pixvpos->z, vposition->z), 0.),
    };
}