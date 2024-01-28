// Forward2DMono.shader

import je::shader;

SHARED  (true);
ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2,
    normal  : float3,
};

using v2f = struct {
    pos     : float4,
    vpos    : float3,
    vnorm   : float3,
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
    let vpos = je_mv * float4::create(v.vertex, 1.);
    let vnorm = v.normal * je_mv->float3x3;
    return v2f{
        pos = je_p * vpos,
        vpos = vpos->xyz / vpos->w,
        vnorm = vnorm,
    };
}

public func frag(vf: v2f)
{
    return fout{
        albedo = je_color,
        self_luminescence = float4::zero,
        vspace_position = float4::create(vf.vpos, 1.),
        vspace_normalize = float4::create(vf.vnorm, 1.),
    };
}
