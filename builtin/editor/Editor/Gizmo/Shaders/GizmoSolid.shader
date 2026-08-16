// GizmoSolid.shader
// 变换游标使用的纯色着色器：颜色由每次绘制的 JE_COLOR uniform 提供，
// 关闭深度测试以保证游标始终可见。
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
    return v2f{
        pos = JE_MVP * vec4!(v.vertex, 1.),
    };
}

public func frag(_: v2f)
{
    return fout{
        color = JE_COLOR,
    };
}
