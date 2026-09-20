// GizmoIcon.shader
// 摄像机实体等使用的图标 gizmo 着色器：采样图标纹理的公告板四边形，
// 染色（悬停/选中高亮）由 JE_COLOR 提供（内置 uniform，默认白色），
// 关闭深度测试保证图标始终可见。
import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE);
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
    // 裁剪空间 z 钳制回 [0, w]（同 GizmoSolid）：绕过 GPU 固定功能的
    // 视锥体裁剪，摄像机实体位于游走摄像机 zfar 之外时图标仍作为
    // 视锥体线框的锚点完整绘制；w <= 0（摄像机身后）仍被 x/y 裁剪剔除。
    let clip = JE_MVP * vec4!(v.vertex, 1.);
    let clip_w = max(clip->w, 0.);
    return v2f{
        pos = vec4!(clip->x, clip->y, clamp(clip->z, 0., clip_w), clip->w),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}

let NearestSampler  = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main        = texture2d::uniform(0, NearestSampler);

public func frag(vf: v2f)
{
    let texcolor = tex2d(Main, vf.uv);
    return fout{
        color = texcolor * JE_COLOR,
    };
}
