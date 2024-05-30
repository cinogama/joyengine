// CameraDefer2DPass.shader

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

    let light_color_rgb = texture(Light, vf.uv)->xyz;
    let self_lumine_color_rgb = texture(SelfLumine, vf.uv)->xyz;
    let glowing_color_rgb = self_lumine_color_rgb + light_color_rgb + float3::const(0.03, 0.03, 0.03);

    let mixed_color_rgb = max(float3::zero, albedo_color_rgb * glowing_color_rgb);

    let hdr_color_rgb = mixed_color_rgb / (mixed_color_rgb + float3::const(1., 1., 1.));
    let hdr_ambient_with_gamma = pow(hdr_color_rgb, float3::const(1. / 2.2, 1. / 2.2, 1. / 2.2));

    return fout{
        color = je_color * vec4(hdr_ambient_with_gamma, 1.)
    };
}
