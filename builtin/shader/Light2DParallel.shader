// Light2DParallel.shader
import woo::std;

import je::shader;
import je::shader::light2d;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (false);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos         : float4,
        light_vdir  : float3,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };
    
public func vert(v: vin)
{
    return v2f{
        pos = vec4!(v.vertex, 0.5) * 2.,
        light_vdir = normalize(
            vec3x3!(JE_MV) * vec3!(0., -1., 1.)),
    };
}

WOSHADER_FUNCTION!
    func multi_sampling_for_bias_shadow(
        shadow  : texture2d,
        uv      : float2)
    {
        let mut shadow_factor = vec1!(0.);
        
        let bias = [
            (0., 1., 0.25),
            (-1., 0., 0.25),
            (0., 0., 0.75),
            (1., 0., 0.25),
            (0., -1., 0.25),
        ];
        
        let bias_step = vec2!(1.5, 1.5) / JE_LIGHT2D_RESOLUTION;
        for (let (xv, yv, f) : bias)
        {
            shadow_factor += f * tex2d(shadow, uv + bias_step * vec2!(xv, yv))->x;
        }
        return min(shadow_factor, 1.);
    }
    
WOSHADER_FUNCTION!
    func apply_light_normal_effect(
        fragment_vnorm  : float3,
        light_vdir      : float3,
    )
    {
        let matched_light_factor = fragment_vnorm->dot(light_vdir->negative);
        return max(0., matched_light_factor) ;
    }
    
// let Albedo          = JE_LIGHT2D_Albedo;
// let SelfLumine      = JE_LIGHT2D_SelfLuminescence;
// let VPosition       = JE_LIGHT2D_VSpacePosition;
let VNormalize      = JE_LIGHT2D_VSpaceNormalize;
let Shadow          = JE_LIGHT2D_Shadow;

WOSHADER_UNIFORM!
    let normal_z_offset = vec1!(1.);
    
public func frag(vf: v2f)
{
    let uv = (vf.pos->xy / vf.pos->w + vec2!(1., 1.)) /2.;
    let shadow_factor = 1. - multi_sampling_for_bias_shadow(Shadow, uv);
    
    let vnormalize = normalize(
        tex2d(VNormalize, uv)->xyz - vec3!(0., 0., normal_z_offset));
        
    let color_intensity =
        JE_COLOR->xyz
            * JE_COLOR->w
            * shadow_factor;
            
    return fout{
        color = vec4!(
            color_intensity * apply_light_normal_effect(
                vnormalize,
                vf.light_vdir),
            0.),
    };
}
