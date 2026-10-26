import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (LESS_EQUAL);
ZWRITE  (ENABLE);
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
        factor: float,
        // 锚边外法线：顶点着色器用它判定本边是否背光
        normal: float2,
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
    let light2d_vdir = (JE_V * vec4!(JE_COLOR->xyz, 1.))->xyz - movement(JE_V);
    let shadow_scale_factor = JE_COLOR->w;
    let shadow_dir = normalize(light2d_vdir);

    // 与点光版相同：边法线随模型旋转到视空间，向光边折叠回锚边（零面积）
    let edge_normal = (JE_MV * vec4!(v.normal->x, v.normal->y, 0., 0.))->xy;
    let keep_edge = step(dot(edge_normal, shadow_dir->xy), 0.);

    // WallShadow 用实体级 painter key
    let shadow_depth = clamp(
        JE_SHADOW_KEY->x * JE_SHADOW_KEY->y + JE_SHADOW_KEY->z, 0., 0.999999);

    let vpos = JE_MV * vec4!(v.vertex, 1.);
    let pos = JE_P * vec4!(
        (vpos->xyz / vpos->w)
            + shadow_dir * shadow_scale_factor * v.factor * keep_edge,
        1.);
    return v2f{
        pos = vec4!(pos->x, pos->y, shadow_depth * pos->w, pos->w),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
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
