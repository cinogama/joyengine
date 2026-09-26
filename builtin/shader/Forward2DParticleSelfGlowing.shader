// Forward2DParticleSelfGlowing.shader
// 延迟渲染管线（DeferLight2D）的自发光粒子着色器：与
// Forward2DParticle.shader 相同的公告板顶点逻辑与 G-buffer 四通道
// 输出，参照 Forward2DSelfGlowing.shader 把粒子颜色按 SelfGlowing 系数
// 写入自发光通道，适合火焰、火花、魔法特效等不受光照衰减影响、直接
// 发光的粒子；SelfGlowing 为可写 uniform（缺省 1.0），可在脚本中按
// 材质调节强度。粒子纹理绑定在 pass 0（Renderer::Textures）。
// 注意：G-buffer 各通道共用一份混合状态，半透明粒子会按 alpha 权重
// 渗入视空间坐标/法线通道（近似插值），因此发射器应保持默认的不透明
// 物体之后绘制（rend_queue 1000）；普通受光粒子请改用
// Forward2DParticle.shader。
import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (false);
ZTEST   (LESS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,    // 粒子中心（发射器局部坐标）
        color   : float4,    // 粒子颜色 RGBA（初始随机色 × 生命周期渐变）
        corner  : float2,    // 已自转、已缩放的角点偏移
        uv      : float2,    // UV
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        vpos    : float3,
        uv      : float2,
        color   : float4,
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
    // JE_V 的行向量即世界空间摄像机 right/up（列主序存储，col(n)
    // 生成 HLSL 的 JE_V[n]，取第 n 行）
    let right = JE_V->col(0)->xyz;
    let up    = JE_V->col(1)->xyz;

    // 中心转到世界空间后沿摄像机平面展开公告板
    let center_world = (JE_M * vec4!(v.vertex, 1.))->xyz;
    let world = center_world + right * v.corner->x + up * v.corner->y;

    let vpos = JE_V * vec4!(world, 1.);
    return v2f{
        pos = JE_P * vpos,
        vpos = vpos->xyz / vpos->w,
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
        color = v.color,
    };
}

let LinearSampler  = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main        = texture2d::uniform(0, LinearSampler);

WOSHADER_UNIFORM!
    let SelfGlowing = vec1!(1.);

public func frag(vf: v2f)
{
    // 完全透明的纹素直接丢弃，避免它们参与 G-buffer 混合；
    // JE_COLOR 为发射器实体的整体着色（缺省为白色）
    let albedo_color = alphatest(JE_COLOR * tex2d(Main, vf.uv) * vf.color);

    // 公告板始终面向摄像机：视空间中片元指向相机（原点）的方向即法线，
    // 逐片元 normalize 使贴近摄像机的大粒子呈现球面受光感
    return fout{
        albedo = albedo_color,
        self_luminescence = vec4!(albedo_color->xyz * SelfGlowing, 1.),
        vspace_position = vec4!(vf.vpos, 1.),
        vspace_normalize = vec4!(normalize(vf.vpos->negative), 1.),
    };
}
