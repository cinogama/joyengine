// CameraDefer2DBloomPass.shader
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

let bias_weight = [
    (-1., 1., 0.094742),    (0., 1., 0.118318),     (1., 1., 0.094742),
    (-1., 0., 0.118318),    (0., 0., 0.147761),     (1., 0., 0.118318),
    (-1., -1., 0.094742),   (0., -1., 0.118318),    (1., -1., 0.094742),
];

WOSHADER_FUNCTION!
    func multi_sampling_for_bias_glowing_level1(tex: texture2d, uv : float2, bias: float)=> float3
    {
        let mut result = vec3!(0., 0., 0.);
        let reso_inv = vec2!(1., 1.) / JE_LIGHT2D_RESOLUTION;
        for (let (xv, yv, weight) : bias_weight)
        {
            result = result + tex2d(tex, uv + reso_inv * vec2!(xv, yv) * bias)->xyz * weight;
        }
        return result;
    }
    
WOSHADER_FUNCTION!
    func multi_samping_light_glowing_bloom(tex: texture2d, uv : float2, bias : float)
    {
        let rgb = tex2d(tex, uv)->xyz;
        let gos_rgb = multi_sampling_for_bias_glowing_level1(tex, uv, bias);
        let gos_rgb_hdr = gos_rgb / (gos_rgb + vec3!(1., 1., 1.));
        let gos_rgb_hdr_brightness =
            0.3 * gos_rgb_hdr->x
                + 0.49 * gos_rgb_hdr->y
                + 0.11 * gos_rgb_hdr->z;
                
        return gos_rgb_hdr_brightness * gos_rgb + (1. - gos_rgb_hdr_brightness) * rgb;
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
    
    let glowing_color_rgb =
        multi_samping_light_glowing_bloom(Light, vf.uv, vec1!(1.5))
            + multi_samping_light_glowing_bloom(SelfLumine, vf.uv, vec1!(1.5))
            + vec3!(0.03, 0.03, 0.03);
            
    let mixed_color_rgb = max(vec3!(0., 0., 0.), albedo_color_rgb * glowing_color_rgb);
    
    let hdr_color_rgb = mixed_color_rgb / (mixed_color_rgb + vec3!(1., 1., 1.));
    let hdr_ambient_with_gamma = pow(hdr_color_rgb, vec3!(1. / 2.2, 1. / 2.2, 1. / 2.2));
    
    return fout{
        color = JE_COLOR * vec4!(hdr_ambient_with_gamma, 1.),
    };
}
