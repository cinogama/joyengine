// Forward2DAura.shader

import je::shader;

SHARED  (false);
ZTEST   (LESS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE);
CULL    (NONE);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2,
};

using v2f = struct {
    pos     : float4,
    vpos    : float3,
    uv      : float2,
};

using fout = struct {
    albedo              : float4, // 漫反射颜色，在光照处理中用于计算颜色
    self_luminescence   : float4, // 自发光颜色，最终混合颜色公式中将叠加此颜色
    vspace_position     : float4, // 视空间坐标(xyz)，主要用于与光源坐标进行距离计算，
                                  // 决定后处理光照的影响系数，w 系数暂时留空，应当设
                                  // 置为1
    vspace_normalize    : float4, // 视空间法线(xyz)，主要用于光照后处理进行光照计算
                                  // 的高光等效果，w 系数暂时留空，应当设置为1
};

public func vert(v: vin)
{
    let vpos = je_mv * vec4(v.vertex, 1.);
    return v2f{
        pos  = je_p * vpos,
        vpos = vpos->xyz / vpos->w,
        uv   = uvtrans(v.uv, je_tiling, je_offset),
    };
}

let color   = uniform("Color", float4::one);
let decay   = uniform("Decay", float::one);

public func frag(vf: v2f)
{
    let dv = clamp(1. - length((vf.uv - float2::const(0.5, 0.5)) * 2.), 0., 1.);

    return fout{
        albedo = float4::const(0.,0.,0.,0.),
        self_luminescence = vec4(color->xyz, pow(dv, decay)) * color->w,
        vspace_position = float4::zero,
        vspace_normalize = float4::zero,
    };
}
