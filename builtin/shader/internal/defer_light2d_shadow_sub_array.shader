import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (LESS_EQUAL);
ZWRITE  (ENABLE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        // 瓦片地图分片顶点：uv.z 为数组纹理层号
        uv: float3,
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        uv: float3,
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };

public func vert(v: vin)
{
    // 普通 SelfShadow：实体级 key（CPU 已按半层偏移处理）
    let shadow_depth = clamp(
        JE_SHADOW_KEY->x * JE_SHADOW_KEY->y + JE_SHADOW_KEY->z, 0., 0.999999);
    let pos = JE_MVP * vec4!(v.vertex, 1.);
    // tiling/offset 只作用于 uv.xy，数组层号分量原样传递
    return v2f{
        pos = vec4!(pos->x, pos->y, shadow_depth * pos->w, pos->w),
        uv = vec3!(uvtrans(v.uv->xy, JE_UV_TILING, JE_UV_OFFSET), v.uv->z),
    };
}

let nearest_clamp = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2darray::uniform(0, nearest_clamp);

public func frag(vf: v2f)
{
    let final_shadow = alphatest(vec4!(JE_COLOR->xyz, tex2darray(Main, vf.uv)->w));

    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, 1.),
    };
}
