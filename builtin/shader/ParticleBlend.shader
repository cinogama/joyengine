// ParticleBlend.shader
// 内置粒子着色器（常规 alpha 混合）：CPU 端逐帧把粒子构建为公告板
// 四边形网格（顶点属性见 jeecs_core_particle_system.hpp），本着色器在
// 顶点阶段用视图矩阵 JE_V 的行向量（世界空间摄像机 right/up）展开角点，
// 使四边形始终面向摄像机；角点已在 CPU 端完成自转与尺寸缩放。
// 粒子纹理绑定在 pass 0（Renderer::Textures），未绑定时使用默认纹理。
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
        uv      : float2,
        color   : float4,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
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

    return v2f{
        pos = JE_VP * vec4!(world, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
        color = v.color,
    };
}

let LinearSampler  = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main        = texture2d::uniform(0, LinearSampler);

public func frag(vf: v2f)
{
    return fout{
        color = tex2d(Main, vf.uv) * vf.color,
    };
}
