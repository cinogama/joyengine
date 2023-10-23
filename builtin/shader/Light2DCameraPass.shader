// Light2DCameraPass.shader
// (C)Cinogama project. 2022. 版权所有

import je::shader;
import je::shader::light2d;

SHARED  (false);
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
        uv = v.uv,
    };
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
    let mixed_color_rgb = max(float3::zero, albedo_color_rgb
        * (self_lumine_color_rgb + light_color_rgb + float3::new(0.03, 0.03, 0.03)));

    let hdr_color_rgb = mixed_color_rgb / (mixed_color_rgb + float3::new(1., 1., 1.));
    let hdr_ambient_with_gamma = pow(hdr_color_rgb, float3::new(1. / 2.2, 1. / 2.2, 1. / 2.2, ));

    return fout{
        color = float4::create(hdr_ambient_with_gamma, 1.)
    };
}