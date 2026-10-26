import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (LESS_EQUAL);
ZWRITE  (ENABLE);
// MAX 合并：受光格 cover=0 不写入，避免擦除遮挡体已写入的阴影
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

// 顶点属性按网格布局前缀绑定：位置 0/1/2 为 pos/uv/normal，
// 位置 3 为 bottom（块底边局部 Y，PerBlockSelfShadow 契约）。
// normal 声明未用，仅为占位到位置 3
WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
        normal: float3,
        bottom: float,
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        uv: float2,
        cover: float,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };

public func vert(v: vin)
{
    // 块底边（局部）变换到世界取 Y，与高度线比较（JE_COLOR：x=factor，
    // y=高度线——AUTO 为光源世界 Y，ON 恒暗时为 +1e30 使 lit 恒 0）
    let wb = (JE_M * vec4!(v.vertex->x, v.bottom, v.vertex->z, 1.))->y;

    // 逐块 painter key：格顶（bottom + 1）作为地面高度。CPU 侧该 pass
    // 以 mode=3（逐顶点 + TopDown）设置 JE_SHADOW_KEY；.x 作为回退。
    let wtop = (JE_M * vec4!(v.vertex->x, v.bottom + 1., v.vertex->z, 1.))->y;
    let use_vertex = step(JE_SHADOW_KEY->w, 1.5);
    let key_world = JE_SHADOW_KEY->x + (wtop - JE_SHADOW_KEY->x) * use_vertex;
    let shadow_depth = clamp(
        key_world * JE_SHADOW_KEY->y + JE_SHADOW_KEY->z, 0., 0.999999);

    let lit = step(wb, JE_COLOR->y);
    let pos = JE_MVP * vec4!(v.vertex, 1.);
    return v2f{
        pos = vec4!(pos->x, pos->y, shadow_depth * pos->w, pos->w),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
        cover = JE_COLOR->x * (1. - lit),
    };
}

let nearest_clamp = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, nearest_clamp);

public func frag(vf: v2f)
{
    let final_shadow = alphatest(vec4!(vf.cover, vf.cover, vf.cover, tex2d(Main, vf.uv)->w));

    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, 1.),
    };
}
