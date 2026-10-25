// 全屏 sRGB 编码 pass。
// 当默认帧缓冲不支持硬件 sRGB 写入控制时（GLES/WebGL，或 Vulkan 仅有 UNORM
// surface），本该绘制到屏幕的内容先渲染到线性离屏 FBO，再用本 shader 做一次
// 线性到 sRGB 的重映射，最后绘制到真正的默认帧缓冲。
// 顶点为全屏四边形；FlipV 用于适配 Vulkan 屏幕 viewport 的纵向翻转。

import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,
        uv      : float2,
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        uv      : float2,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };

public func vert(v: vin)
{
    return v2f{
        pos = vec4!(v.vertex->xy, 0., 1.),
        uv = v.uv,
    };
}

let nearest_clamp = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);

WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, nearest_clamp);

// 0 = 正常采样；1 = 采样时翻转 V（Vulkan 屏幕 viewport 为负高度）。
WOSHADER_UNIFORM!
    let FlipV = vec1!(0.);

func linear_to_srgb_channel(c: float3)=> float3
{
    let clamped = max(c, vec3!(0., 0., 0.));
    let lo = clamped * 12.92;

    let srgb_factor = 1. / 2.4;
    let hi = 1.055 * pow(clamped, vec3!(srgb_factor, srgb_factor, srgb_factor)) - vec3!(0.055, 0.055, 0.055);
    let use_hi = step(clamped, vec3!(0.0031308, 0.0031308, 0.0031308));
    return lerp(lo, hi, use_hi);
}

public func frag(vf: v2f)
{
    let uv = vec2!(
        vf.uv->x,
        lerp(vf.uv->y, 1. - vf.uv->y, FlipV));
    let c = tex2d(Main, uv);
    return fout{
        color = vec4!(
            linear_to_srgb_channel(c->xyz),
            c->w),
    };
}
