// TilemapForward2DNormal.shader
// 瓦片地图受光照前向着色器（法线图集版）：TilemapForward2D 的 MRT 光照
// 输出 + 附加层级法线采样。通道 0 为主纹理（反照率）数组图集，通道 1
// 为法线数组图集（图集源纹理的附加层级，编辑器中把法线贴图按通道 1
// 添加到源纹理即可）。顶点 uv 为 float3 —— (u, v, 数组层号)；两张数组
// 的层号一致（层号 = iy*xcount + ix），同一 uv 可同时采样两通道。
// 瓦片网格顶点轴对齐（uv.x/y 对齐局部 x/y 轴，法线 +Z），局部切线基
// 恒为单位基（T=(1,0,0), B=(0,1,0), N=+Z），法线图采样后按 Forward2DNormal
// 的视空间切线基（含 JE_LOCAL_SCALE 去斜）重建视空间法线。
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
        vtangent_x  : float3,
        vtangent_y  : float3,
        vtangent_z  : float3,
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

WOSHADER_FUNCTION!
    func vtangent(normal: float3)
    {
        return normalize(vec3x3!(JE_MV) * normal / abs(JE_LOCAL_SCALE));
    }

public func vert(v: vin)
{
    let vpos = JE_MV * vec4!(v.vertex, 1.);

    // 瓦片四边形轴对齐：局部切线基为单位基（T=+X, B=+Y, N=+Z），
    // 与 uv.xy 同向，故切线基取模型旋转（含缩放去斜）后的固定基
    let N = vtangent(v.normal);
    let T = vtangent(vec3!(1., 0., 0.));
    let B = normalize(cross(T, N));

    return v2f{
        pos = JE_P * vpos,
        vpos = vpos->xyz / vpos->w,
        vtangent_x = T,
        vtangent_y = B,
        vtangent_z = N,
        uv = vec3!(
            uvtrans(v.uv->xy, JE_UV_TILING, JE_UV_OFFSET),
            v.uv->z),
    };
}

let NearestSampler  = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main        = texture2darray::uniform(0, NearestSampler);
WOSHADER_UNIFORM!
    let Normalize   = texture2darray::uniform(1, NearestSampler);

func transed_normal_tangent_map(vertex_info : v2f)
{
    // 法线图切线空间重建（与 Forward2DNormal 同式，采样换为数组图集）
    let normal_from_map = 2. * tex2darray(Normalize, vertex_info.uv)->xyz - vec3!(1., 1., 1.);
    return normalize(
        vertex_info.vtangent_x * normal_from_map->x +
            vertex_info.vtangent_y * normal_from_map->y +
                vertex_info.vtangent_z * normal_from_map->z);
}

public func frag(vf: v2f)
{
    return fout{
        albedo = alphatest(JE_COLOR * tex2darray(Main, vf.uv)),
        self_luminescence = vec4!(0., 0., 0., 0.),
        vspace_position = vec4!(vf.vpos, 1.),
        vspace_normalize = vec4!(transed_normal_tangent_map(vf), 1.),
    };
}
