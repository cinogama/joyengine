// TilemapForward2D.shader
// 瓦片地图受光照前向着色器：Forward2D 的光照输出（视空间坐标/法线，
// 供光照后处理使用）+ TilemapUnlitBlend 的数组图集采样（每层一个瓦片，
// layer = iy*xcount + ix）。顶点 uv 为 float3 —— (u, v, 数组层号)；
// tiling/offset 只作用于 uv.xy，层号分量原样传递。瓦片网格顶点自带
// normal 属性（图集按瓦片指定，默认 -Z 指向屏幕外侧），参与光照计算。
import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (LESS);
ZWRITE  (ENABLE);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,
        uv      : float3,
        normal  : float3,
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        vpos    : float3,
        vnorm   : float3,
        uv      : float3,
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
    let vnorm = normalize(vec3x3!(JE_MV) * v.normal);
    return v2f{
        pos  = JE_P * vpos,
        vpos = vpos->xyz / vpos->w,
        vnorm = vnorm,
        uv = vec3!(
            uvtrans(v.uv->xy, JE_UV_TILING, JE_UV_OFFSET),
            v.uv->z),
    };
}

let NearestSampler  = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main        = texture2darray::uniform(0, NearestSampler);

public func frag(vf: v2f)
{
    return fout{
        albedo = alphatest(JE_COLOR * tex2darray(Main, vf.uv)),
        self_luminescence = vec4!(0., 0., 0., 0.),
        vspace_position = vec4!(vf.vpos, 1.),
        vspace_normalize = vec4!(vf.vnorm, 1.),
    };
}
