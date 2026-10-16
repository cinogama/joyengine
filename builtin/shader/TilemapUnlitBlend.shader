// TilemapUnlitBlend.shader
// 瓦片地图默认层着色器：数组图集采样（每层一个瓦片，layer = iy*xcount + ix）。
// 顶点 uv 为 float3 —— (u, v, 数组层号)；tile-local UV + 层内 CLAMP 使
// 相邻瓦片渗色在结构上不可能发生。tiling/offset 只作用于 uv.xy，
// 层号分量原样传递。
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
        vertex  : float3,
        uv      : float3,
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        uv      : float3,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };

public func vert(v: vin)
{
    return v2f{
        pos = JE_MVP * vec4!(v.vertex, 1.),
        uv = vec3!(
            uvtrans(v.uv->xy, JE_UV_TILING, JE_UV_OFFSET),
            v.uv->z),
    };
}

let NearestSampler  = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main        = texture2darray::uniform(0, NearestSampler);

public func frag(vf: v2f)
{
    return fout{
        color = alphatest(JE_COLOR * tex2darray(Main, vf.uv)),
    };
}
