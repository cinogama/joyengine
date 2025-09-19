// CameraDefer2DPass.shader
import woo::std;

import je::shader;
import je::shader::light2d;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
CULL    (BACK);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,
        uv      : float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        uv      : float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };
    
public func vert(v: vin)
{
    return v2f{
        pos = vec4!(v.vertex, 1.),
        uv = v.uv,
    };
}

let LinearSampler   = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Light           = texture2d::uniform(0, LinearSampler);
    
let Albedo          = JE_LIGHT2D_Albedo;
let SelfLumine      = JE_LIGHT2D_SelfLuminescence;
// let VPosition       = JE_LIGHT2D_VSpacePosition;
// let VNormalize      = JE_LIGHT2D_VSpaceNormalize;
// let Shadow          = JE_LIGHT2D_Shadow;

public func frag(vf: v2f)
{
    let albedo_color_rgb = pow(tex2d(Albedo, vf.uv)->xyz, vec3!(2.2, 2.2, 2.2));
    
    let light_color_rgb = tex2d(Light, vf.uv)->xyz;
    let self_lumine_color_rgb = tex2d(SelfLumine, vf.uv)->xyz;
    let glowing_color_rgb = self_lumine_color_rgb + light_color_rgb + vec3!(0.03, 0.03, 0.03);
    
    let mixed_color_rgb = max(vec3!(0., 0., 0.), albedo_color_rgb * glowing_color_rgb);
    
    let hdr_color_rgb = mixed_color_rgb / (mixed_color_rgb + vec3!(1., 1., 1.));
    let hdr_ambient_with_gamma = pow(hdr_color_rgb, vec3!(1. / 2.2, 1. / 2.2, 1. / 2.2));
    
    return fout{
        color = JE_COLOR * vec4!(hdr_ambient_with_gamma, 1.)
        };
}
