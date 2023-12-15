// Light2DCameraBloomPass.shader

import je::shader;
import je::shader::light2d;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

VAO_STRUCT!vin
{
    vertex: float3,
    uv : float2,
};

using v2f = struct {
    pos: float4,
    uv : float2,
};

using fout = struct {
    color: float4
};

public func vert(v: vin)
{
    return v2f{
        pos = float4::create(v.vertex, 1.),
        uv = uvframebuf(v.uv),
    };
}

let bias = 1.;
let bias_weight = [
    (-1., 1., 0.094742),    (0., 1., 0.118318),     (1., 1., 0.094742),
    (-1., 0., 0.118318),    (0., 0., 0.147761),     (1., 0., 0.118318),
    (-1., -1., 0.094742),   (0., -1., 0.118318),    (1., -1., 0.094742),
];

func multi_sampling_for_bias_self_glowing(tex: texture2d, reso: float2, uv: float2, depth: int)=> float3
{
    let mut result = float3::zero;
    let reso_inv = float2::one / reso;
    for (let _, (x, y, weight) : bias_weight)
    {
        if (depth <= 0)
            result = result + texture(
                tex, uv + reso_inv * float2::create(x, y) * bias
            )->xyz * weight;  
        else
            result = result + multi_sampling_for_bias_self_glowing(
                tex, reso, uv + reso_inv * float2::create(x, y) * bias, depth - 1
                ) * weight;
    }
    return result;
}

public func frag(vf: v2f)
{
    let albedo_buffer = je_light2d_defer_albedo;
    let self_lumine = je_light2d_defer_self_luminescence;
    let NearestRepeatSampler = sampler2d::create(NEAREST, NEAREST, NEAREST, REPEAT, REPEAT);
    let light_buffer = uniform_texture:<texture2d>("Light", NearestRepeatSampler, 0);

    let albedo_color_rgb = pow(texture(albedo_buffer, vf.uv)->xyz, float3::new(2.2, 2.2, 2.2));
    let light_color_rgb = texture(light_buffer, vf.uv)->xyz;
    let self_lumine_color_rgb = texture(self_lumine, vf.uv)->xyz;
    let self_luming_color_gos_rgb = multi_sampling_for_bias_self_glowing(self_lumine, je_light2d_resolutin, vf.uv, 1);
    let self_luming_color_gos_rgb_hdr = self_luming_color_gos_rgb / (self_luming_color_gos_rgb + float3::one);
    let self_luming_color_gos_rgb_hdr_brightness = 
        float::new(0.3) * self_luming_color_gos_rgb_hdr->x
        + float::new(0.49) * self_luming_color_gos_rgb_hdr->y
        + float::new(0.11) * self_luming_color_gos_rgb_hdr->z;

    let mixed_color_rgb = max(float3::zero, albedo_color_rgb
        * ( self_luming_color_gos_rgb_hdr_brightness * self_luming_color_gos_rgb 
            + (float::one - self_luming_color_gos_rgb_hdr_brightness) * self_lumine_color_rgb
            + light_color_rgb 
            + float3::new(0.03, 0.03, 0.03)));

    let hdr_color_rgb = mixed_color_rgb / (mixed_color_rgb + float3::one);
    let hdr_ambient_with_gamma = pow(hdr_color_rgb, float3::new(1. / 2.2, 1. / 2.2, 1. / 2.2, ));

    return fout{
        color = float4::create(hdr_ambient_with_gamma, 1.)
    };
}
