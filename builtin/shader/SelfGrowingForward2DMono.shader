// SelfGrowingForward2DMono.shader
// (C)Cinogama project. 2022. 版权所有

import je::shader;

SHARED  (false);
ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
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
    visual_coordinates  : float4, // 视空间坐标(xyz)，主要用于与光源坐标进行距离计算，决定后处理光照的影响系数
                                  // w 系数暂时留空，应当设置为1
};

public func vert(v: vin)
{
    let vpos = je_mv * float4::create(v.vertex, 1.);
    return v2f{
        pos  = je_p * vpos,
        vpos = vpos->xyz / vpos->w,
        uv   = uvtrans(v.uv, je_tiling, je_offset),
    };
}

public func frag(vf: v2f)
{
    let color = uniform("Color", float4::one);
    let self_growing = uniform("SelfGrowing", float::one);

    return fout{
        albedo = color,
        self_luminescence = float4::create(color->xyz * self_growing, 1.),
        visual_coordinates = float4::create(vf.vpos, 1.),
    };
}