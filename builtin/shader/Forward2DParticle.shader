// Forward2DParticle.shader
// 延迟渲染管线（DeferLight2D）的粒子着色器：片段输出与 Forward2D.shader
// 相同的 G-buffer 四通道布局（漫反射/自发光/视空间坐标/视空间法线），
// 粒子颜色写入漫反射通道后交由 Light2D* 光照 pass 与 CameraDefer2DPass
// 合成，可正常接受 2D 光照与阴影；顶点阶段沿用 ParticleBlend.shader 的
// 公告板展开逻辑（JE_V 的行向量即世界空间摄像机 right/up），角点已在
// CPU 端完成自转与尺寸缩放。粒子纹理绑定在 pass 0（Renderer::Textures），
// 未绑定时使用默认纹理。
// 注意：G-buffer 各通道共用一份混合状态，半透明粒子会按 alpha 权重渗入
// 视空间坐标/法线通道（近似插值），因此发射器应保持默认的不透明物体之后
// 绘制（rend_queue 1000）；发光类粒子请改用 Forward2DParticleSelfGlowing.shader，
// 延迟管线下不建议使用加色混合（会持续累加坐标/法线通道导致光照错乱）。
import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
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

public func frag(vf: v2f)
{
    // 完全透明的纹素直接丢弃，避免它们参与 G-buffer 混合；
    // JE_COLOR 为发射器实体的整体着色（缺省为白色）
    let albedo_color = alphatest(JE_COLOR * tex2d(Main, vf.uv) * vf.color);

    // 公告板始终面向摄像机：视空间中片元指向相机（原点）的方向即法线，
    // 逐片元 normalize 使贴近摄像机的大粒子呈现球面受光感
    return fout{
        albedo = albedo_color,
        self_luminescence = vec4!(0., 0., 0., 0.),
        vspace_position = vec4!(vf.vpos, 1.),
        vspace_normalize = vec4!(normalize(vf.vpos->negative), 1.),
    };
}
