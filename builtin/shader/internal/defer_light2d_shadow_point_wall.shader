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
        factor: float,
        // 锚边外法线：顶点着色器用它判定本边是否背光
        normal: float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    let light2d_vpos = JE_V * vec4!(JE_COLOR->xyz, 1.);
    let shadow_scale_factor = JE_COLOR->w;

    let vpos = JE_MV * vec4!(v.vertex, 1.);
    // 逐顶点径向：本顶点背向光源的方向（视空间，透视除后），
    // 近光源时各边自然发散，远光源时退化为整体平移
    let far_dir = (vpos->xyz / vpos->w) - (light2d_vpos->xyz / light2d_vpos->w);
    let shadow_dir = far_dir / max(length(far_dir), 0.000001);

    // 边法线随模型旋转/翻转到视空间（w=0 跳过平移），
    // 向光边（dot<0）整体折叠回锚边成为零面积，不产生片元
    let edge_normal = (JE_MV * vec4!(v.normal->x, v.normal->y, 0., 0.))->xy;
    let keep_edge = step(dot(edge_normal, shadow_dir->xy), 0.);

    return v2f{
        pos = JE_P * vec4!(
            (vpos->xyz / vpos->w)
                + shadow_dir * shadow_scale_factor * v.factor * keep_edge,
            1.),
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
