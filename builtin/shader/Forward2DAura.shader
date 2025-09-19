// Forward2DAura.shader
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (false);
ZTEST   (LESS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE);
CULL    (NONE);

WOSHADER_VERTEX_IN! 
    using vin = struct {
        vertex  : float3,
        uv      : float2,
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        vpos    : float3,
        uv      : float2,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        // 漫反射颜色，在光照处理中用于计算颜色
        albedo              : float4,
        // 自发光颜色，最终混合颜色公式中将叠加此颜色
        self_luminescence   : float4,
        // 视空间坐标(xyz)，主要用于与光源坐标进行距离计算，
        // 决定后处理光照的影响系数，w 系数暂时留空，应当设
        // 置为1
        vspace_position     : float4,
        // 视空间法线(xyz)，主要用于光照后处理进行光照计算
        // 的高光等效果，w 系数暂时留空，应当设置为1
        vspace_normalize    : float4,
    };

public func vert(v: vin)
{
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    return v2f{
        pos  = JE_P * vpos,
        vpos = vpos->xyz / vpos->w,
        uv   = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}

WOSHADER_UNIFORM!
    let Decay   = vec1!(1.);

public func frag(vf: v2f)
{
    let dv = clamp(1. - length((vf.uv - vec2!(0.5, 0.5)) * 2.), 0., 1.);

    return fout{
        albedo = vec4!(0.,0.,0.,0.),
        self_luminescence = vec4!(JE_COLOR->xyz, pow(dv, Decay)) * JE_COLOR->w,
        vspace_position = vec4!(0., 0., 0., 0.),
        vspace_normalize = vec4!(0., 0., 0., 0.),
    };
}
