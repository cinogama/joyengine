// GizmoSolid.shader
// 变换游标使用的纯色着色器：颜色由每次绘制的 JE_COLOR uniform 提供，
// 关闭深度测试以保证游标始终可见。
import pkg::std;

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
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };

public func vert(v: vin)
{
    // 关闭深度测试只解决遮挡，图元仍会被 GPU 固定功能的视锥体裁剪：
    // 超出摄像机 znear/zfar 的部分将被硬裁。这里把裁剪空间 z 钳制回
    // [0, w]（本引擎投影矩阵的 NDC z 范围为 [0,1]，各后端 JE_NDC_SCALE
    // 的 z 分量恒为 1）：屏幕投影（x/w, y/w）与属性插值（按 1/w 权重）
    // 均不受影响，仅深度被压进可绘制范围内，从而完整绘制远超编辑器
    // 游走摄像机 zfar 的 gizmo（如场景摄像机的视锥体线框）。
    // w <= 0（顶点在摄像机后方）时钳制为 0，顶点仍会被 x/y 裁剪面剔除，
    // 保持与原先一致的行为。
    let clip = JE_MVP * vec4!(v.vertex, 1.);
    let clip_w = max(clip->w, 0.);
    return v2f{
        pos = vec4!(clip->x, clip->y, clamp(clip->z, 0., clip_w), clip->w),
    };
}

public func frag(_: v2f)
{
    return fout{
        color = JE_COLOR,
    };
}
