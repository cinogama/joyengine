// Light2DRange.shader
import woo::std;

import je::shader;
import je::shader::light2d;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,
        strength: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos         : float4,
        vpos        : float4,
        light_vpos  : float3,
        strength    : float,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };
    
public func vert(v: vin)
{
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    return v2f{
        pos = JE_P * vpos,
        vpos = vpos,
        light_vpos = movement(JE_MV),
        strength = v.strength,
    };
}

WOSHADER_FUNCTION!
    func multi_sampling_for_bias_shadow(
        shadow  : texture2d,
        uv      : float2)
    {
        let mut shadow_factor = vec1!(0.);
        
        let bias = [
            (-1., -1.),
            (-1., 1.),
            (0., 0.),
            (1., -1.),
            (1., 1.),
        ];
        
        let bias_step = vec2!(1.5, 1.5) / JE_LIGHT2D_RESOLUTION;
        for (let (xv, yv) : bias)
        {
            shadow_factor = max(
                shadow_factor, 
                tex2d(shadow, uv + bias_step * vec2!(xv, yv))->x);
        }
        return shadow_factor;
    }
    
WOSHADER_FUNCTION!
    func apply_light_normal_effect(
        fragment_vnorm  : float3,
        light_vdir      : float3
        )
    {
        let matched_light_factor = fragment_vnorm->dot(light_vdir->negative);
        return max(0., matched_light_factor) ;
    }
    
// let Albedo          = JE_LIGHT2D_Albedo;
// let SelfLumine      = JE_LIGHT2D_SelfLuminescence;
let VPosition       = JE_LIGHT2D_VSpacePosition;
let VNormalize      = JE_LIGHT2D_VSpaceNormalize;
let Shadow          = JE_LIGHT2D_Shadow;

public func frag(vf: v2f)
{
    let uv = (vf.pos->xy / vf.pos->w + vec2!(1., 1.)) /2.;
    
    let pixvpos = vf.vpos->xyz / vf.vpos->w;
    
    let vposition = tex2d(VPosition, uv)->xyz;
    let vnormalize = tex2d(VNormalize, uv)->xyz;
    let fgdistance = distance(vposition, pixvpos);
    
    let shadow_factor = 1. - multi_sampling_for_bias_shadow(Shadow, uv);  
    let fade_factor = pow(vf.strength, JE_LIGHT2D_DECAY);
    
    let color_intensity =
        JE_COLOR->xyz
            * JE_COLOR->w
            * shadow_factor
            * fade_factor / (fgdistance + 1.0);
            
    return fout{
        color = vec4!(
            color_intensity * apply_light_normal_effect(
                vnormalize,
                normalize(vposition - vf.light_vpos)),
            0.),
    };
}
