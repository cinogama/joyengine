import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (LESS_EQUAL);
ZWRITE  (ENABLE);
BLEND   (MAX, ONE, ONE);
CULL    (FRONT);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        factor: float,
        // 局部墙底 Y：本段及附近跨层墙段的较低基座，整段统一排序
        key_y: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    // ATTENTION: We will using JE_COLOR: float4 to pass lwpos.
    let light_vdir = (JE_V * JE_COLOR)->xyz - movement(JE_V);
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    
    // 逐像素 painter key：瓦片轮廓取本段及附近跨层墙段的局部墙底，
    // 普通实体回退到实体 key。
    let wp = JE_M * vec4!(v.vertex, 1.);
    let use_vertex = step(JE_SHADOW_KEY->w, 1.5);
    let vertex_topdown = step(JE_SHADOW_KEY->w, 2.5);
    let key_point = JE_M * vec4!(v.vertex->x, v.key_y, v.vertex->z, 1.);
    let vertex_key = wp->z + (key_point->y - wp->z) * vertex_topdown;
    let key_world = JE_SHADOW_KEY->x + (vertex_key - JE_SHADOW_KEY->x) * use_vertex;
    let shadow_depth = clamp(
        key_world * JE_SHADOW_KEY->y + JE_SHADOW_KEY->z, 0., 0.999999);

    let shadow_vdir = vec3!(normalize(light_vdir->xy), 0.) * 2000. * v.factor;

    let pos = JE_P * vec4!(vpos->xyz + shadow_vdir, 1.);
    return v2f{
        pos = vec4!(pos->x, pos->y, shadow_depth * pos->w, pos->w),
    };
}

public func frag(_: v2f)
{
    // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
    let shadow_factor = JE_LOCAL_SCALE->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
