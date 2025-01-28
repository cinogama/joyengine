// CameraDefer2DBloomPass.shader

import je::shader;
import je::shader::light2d;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2,
};

using v2f = struct {
    pos     : float4,
    uv      : float2,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    return v2f{
        pos = vec4(v.vertex, 1.),
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
    let reso_inv = float2::one / je_light2d_resolution;
    for (let (x, y, weight) : bias_weight)
    {
        result = result + texture(tex, uv + reso_inv * vec2(x, y) * bias)->xyz * weight;
    }
    return result;
}

SHADER_FUNCTION!
func multi_samping_light_glowing_bloom(tex: texture2d, uv : float2, bias : float)
{
    let rgb = texture(tex, uv)->xyz;
    let gos_rgb = multi_sampling_for_bias_glowing_level1(tex, uv, bias);
    let gos_rgb_hdr = gos_rgb / (gos_rgb + float3::one);
    let gos_rgb_hdr_brightness =
        0.3 * gos_rgb_hdr->x
        + 0.49 * gos_rgb_hdr->y
        + 0.11 * gos_rgb_hdr->z;

    return gos_rgb_hdr_brightness * gos_rgb + (1. - gos_rgb_hdr_brightness) * rgb;
}

let LinearSampler   = sampler2d::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
let Light           = uniform_texture:<texture2d>("Light", LinearSampler, 0);
let Albedo          = je_light2d_defer_albedo;
let SelfLumine      = je_light2d_defer_self_luminescence;
// let VPosition       = je_light2d_defer_vspace_position;
// let VNormalize      = je_light2d_defer_vspace_normalize;
// let Shadow          = je_light2d_defer_shadow;

public func frag(vf: v2f)
{
    let albedo_color_rgb = pow(texture(Albedo, vf.uv)->xyz, float3::const(2.2, 2.2, 2.2));

    let glowing_color_rgb = 
        multi_samping_light_glowing_bloom(Light, vf.uv, float::const(1.5))
        + multi_samping_light_glowing_bloom(SelfLumine, vf.uv, float::const(1.5))
        + float3::const(0.03, 0.03, 0.03);

    let mixed_color_rgb = max(float3::zero, albedo_color_rgb * glowing_color_rgb);

    let hdr_color_rgb = mixed_color_rgb / (mixed_color_rgb + float3::one);
    let hdr_ambient_with_gamma = pow(hdr_color_rgb, float3::const(1. / 2.2, 1. / 2.2, 1. / 2.2));

    return fout{
        color = je_color * vec4(hdr_ambient_with_gamma, 1.)
    };
}
