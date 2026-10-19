import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    let light2d_vpos = JE_V * vec4!(JE_COLOR->xyz, 1.);
    let shadow_scale_factor = JE_COLOR->w;
    
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    let vpos_light_diff = vpos->xyz / vpos->w - light2d_vpos->xyz / light2d_vpos->w;
    let vpos_light_diff_z_abs =  abs(vpos_light_diff->z);
    let shadow_offset_xy_z_abs = vec3!(
        vpos_light_diff->xy / (1. + vpos_light_diff_z_abs) * shadow_scale_factor,
        vpos_light_diff_z_abs);
        
    let shadow_tiling_scale = JE_LOCAL_SCALE->yz;
    let shadow_tiling_scale_apply_offset =
        (vec2!(0.5, 0.5) - v.uv) * 2. * (vec2!(1., 1.) - shadow_tiling_scale);
        
    return v2f{
        pos = JE_P * vec4!((vpos->xyz / vpos->w) + shadow_offset_xy_z_abs, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET) + shadow_tiling_scale_apply_offset,
    };
}

let linear_clamp = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, linear_clamp);
    
public func frag(vf: v2f)
{
    let final_shadow = alphatest(
        vec4!(
            JE_LOCAL_SCALE,     // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
            tex2d(Main, vf.uv)->w));
            
    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
