import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

// 自阴影接收面 pass：key 由 CPU 按“实体/图层 key - 半层偏移”给出，
// ZTEST(LESS_EQUAL) + ZWRITE(ENABLE) + 覆盖写入。位于接收面之后
//（key 更大）的遮挡阴影被替换，位于其前（key 更小）的阴影保留；
// 同一图层的 BlockShadow 也因 key 稍后而被清除（自遮挡消除）。
// 受光面 CPU 传 0（清除），未受光/常开面传 factor。
ZTEST   (LESS_EQUAL);
ZWRITE  (ENABLE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
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
    // 接收面 key：CPU 已按“实体/图层 key - 半层偏移”处理
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
    let final_shadow = alphatest(vec4!(JE_COLOR->xyz, tex2d(Main, vf.uv)->w));

    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, 1.),
    };
}
