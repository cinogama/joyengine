// GizmoIcon.shader
// 摄像机实体等使用的图标 gizmo 着色器：采样图标纹理的公告板四边形，
// 染色（悬停/选中高亮）由 JE_COLOR 提供（内置 uniform，默认白色），
// 关闭深度测试保证图标始终可见。
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
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
        pos = JE_MVP * vec4!(v.vertex, 1.),
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
        color = vec4!(
            texcolor->x * JE_COLOR->x,
            texcolor->y * JE_COLOR->y,
            texcolor->z * JE_COLOR->z,
            texcolor->w * JE_COLOR->w),
    };
}
