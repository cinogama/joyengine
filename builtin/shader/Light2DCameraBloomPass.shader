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

let bias_weight = [
    (-1., 1., 0.094742),    (0., 1., 0.118318),     (1., 1., 0.094742),
    (-1., 0., 0.118318),    (0., 0., 0.147761),     (1., 0., 0.118318),
    (-1., -1., 0.094742),   (0., -1., 0.118318),    (1., -1., 0.094742),
];

SHADER_FUNCTION!
func multi_sampling_for_bias_glowing_level1(tex: texture2d, uv : float2, bias: float)=> float3
{
    let mut result = float3::zero;
    let reso_inv = float2::one / je_light2d_resolutin;
    for (let _, (x, y, weight) : bias_weight)
    {
        result = result + texture(tex, uv + reso_inv * float2::create(x, y) * bias)->xyz * weight;
    }
    return result;
}

SHADER_FUNCTION!
func multi_sampling_for_bias_glowing_level2(tex: texture2d, uv: float2, bias: float)=> float3
{
    let mut result = float3::zero;
    let reso_inv = float2::one / je_light2d_resolutin;
    for (let _, (x, y, weight) : bias_weight)
    {
        result = result + multi_sampling_for_bias_glowing_level1(
            tex, uv + reso_inv * float2::create(x, y) * bias, bias) * weight;
    }
    return result;
}

SHADER_FUNCTION!
func multi_samping_glowing_bloom(tex: texture2d, uv : float2, bias : float)
{
    let rgb = texture(tex, uv)->xyz;
    let gos_rgb = multi_sampling_for_bias_glowing_level2(tex, uv, bias);
    let gos_rgb_hdr = gos_rgb / (gos_rgb + float3::one);
    let gos_rgb_hdr_brightness =
        float::new(0.3) * gos_rgb_hdr->x
        + float::new(0.49) * gos_rgb_hdr->y
        + float::new(0.11) * gos_rgb_hdr->z;

    return gos_rgb_hdr_brightness * gos_rgb + (1. - gos_rgb_hdr_brightness) * rgb;
}

public func frag(vf: v2f)
{
    let NearestRepeatSampler = sampler2d::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);

    let light_buffer = uniform_texture:<texture2d>("Light", NearestRepeatSampler, 0);
    let albedo_buffer = je_light2d_defer_albedo;
    let self_lumine = je_light2d_defer_self_luminescence;

    let albedo_color_rgb = pow(texture(albedo_buffer, vf.uv)->xyz, float3::new(2.2, 2.2, 2.2));

    let glowing_color_rgb = 
        multi_samping_glowing_bloom(light_buffer, vf.uv, float::new(5.))
        + multi_samping_glowing_bloom(self_lumine, vf.uv, float::new(1.5))
        + float3::new(0.03, 0.03, 0.03);

    let mixed_color_rgb = max(float3::zero, albedo_color_rgb * glowing_color_rgb);

    let hdr_color_rgb = mixed_color_rgb / (mixed_color_rgb + float3::one);
    let hdr_ambient_with_gamma = pow(hdr_color_rgb, float3::new(1. / 2.2, 1. / 2.2, 1. / 2.2, ));

    return fout{
        color = float4::create(hdr_ambient_with_gamma, 1.)
    };
}
