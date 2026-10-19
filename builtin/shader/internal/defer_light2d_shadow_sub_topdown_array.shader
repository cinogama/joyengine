import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
// MAX 合并：受光格 cover=0 不写入，避免擦除遮挡体已写入的阴影
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

// 同 sub_topdown：位置 3 为 bottom（块底边局部 Y），normal 仅占位；
// uv.z 为数组纹理层号（瓦片地图分片图集）
WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float3,
        normal: float3,
        bottom: float,
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        uv: float3,
        cover: float,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };

public func vert(v: vin)
{
    let wb = (JE_M * vec4!(v.vertex->x, v.bottom, v.vertex->z, 1.))->y;
    let lit = step(wb, JE_COLOR->y);

    return v2f{
        pos = JE_MVP * vec4!(v.vertex, 1.),
        // tiling/offset 只作用于 uv.xy，数组层号分量原样传递
        uv = vec3!(uvtrans(v.uv->xy, JE_UV_TILING, JE_UV_OFFSET), v.uv->z),
        cover = JE_COLOR->x * (1. - lit),
    };
}

let nearest_clamp = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2darray::uniform(0, nearest_clamp);

public func frag(vf: v2f)
{
    let final_shadow = alphatest(vec4!(vf.cover, vf.cover, vf.cover, tex2darray(Main, vf.uv)->w));

    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, 1.),
    };
}
