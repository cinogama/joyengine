import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

// 同层自遮挡清除 pass：只在阴影缓冲深度与本层 BlockShadow 完全同 key
// （ZTEST EQUAL）的像素上写入 0。其它图层投到本层图块上的遮挡阴影
// 深度不同，EQUAL 失败而保持不动；更靠前图层的阴影随后照常覆盖。
// 不写深度：清除 pass 不参与后续 painter key 仲裁。
ZTEST   (EQUAL);
ZWRITE  (DISABLE);
// 默认混合（替换）：受光格把同层遮挡阴影清为 0
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        uv: float2,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };

public func vert(v: vin)
{
    // 与同层 BlockShadow 使用完全相同的实体/图层 key，保证 EQUAL 命中
    let shadow_depth = clamp(
        JE_SHADOW_KEY->x * JE_SHADOW_KEY->y + JE_SHADOW_KEY->z, 0., 0.999999);
    let pos = JE_MVP * vec4!(v.vertex, 1.);
    return v2f{
        pos = vec4!(pos->x, pos->y, shadow_depth * pos->w, pos->w),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}

let nearest_clamp = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, nearest_clamp);

public func frag(vf: v2f)
{
    // 只做 alpha 裁剪；颜色恒为 0（受光）
    let final_shadow = alphatest(vec4!(0., 0., 0., tex2d(Main, vf.uv)->w));

    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, 1.),
    };
}
