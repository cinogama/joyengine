// Defer2D.shader
// (C)Cinogama project. 2022. 版权所有

import je.shader;
import je.shader.light2d;

ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT vin{
    vertex  : float3,
    uv      : float2,
};

using v2f = struct {
    pos     : float4,
    vpos    : float3,
    uv      : float2,
    vtangent_x : float3, 
    vtangent_y : float3,
    vtangent_z : float3,
};

using fout = struct {
    // 漫反射颜色
    albedo   : float4,

    // 视空间坐标及金属度
    //  xyz: 视空间坐标
    //  w: 金属度
    vposition_m: float4,

    // 视空间法线及糙度
    //  xyz: 视空间法线
    //  w: 粗糙度
    vnormalize_r: float4,
};

public func vert(v: vin)
{
    let vspace_position = je_mv * float4::create(v.vertex, 1.);
    let m_movement = movement(je_m);
    let v_movement = movement(je_v);
    return v2f{
        pos  = je_p * vspace_position,
        vpos = vspace_position->xyz,
        uv   = uvtrans(v.uv, je_tiling, je_offset),
        vtangent_x = (je_v * float4::create((je_m * float4::new(1., 0., 0., 1.))->xyz - m_movement, 1.))
            ->xyz + v_movement,
        vtangent_y = (je_v * float4::create((je_m * float4::new(0., 1., 0., 1.))->xyz - m_movement, 1.))
            ->xyz + v_movement,
        vtangent_z = (je_v * float4::create((je_m * float4::new(0., 0., -1., 1.))->xyz - m_movement, 1.))
            ->xyz + v_movement,
    };
}

func get_normal_from_map(normal_map: texture2d, uv : float2)
{
    return (float::new(2.) * texture(normal_map, uv)->xyz) - float3::new(1., 1., 1.);
}

func transed_normal_tangent_map(normal_map: texture2d, vertex_info : v2f)
{
    let normal_from_map = get_normal_from_map(normal_map, vertex_info.uv);
    return normalize(
        vertex_info.vtangent_x * normal_from_map +
        vertex_info.vtangent_y * normal_from_map +
        vertex_info.vtangent_z * normal_from_map
    );
}

public func frag(vf: v2f)
{
    let Albedo = uniform_texture:<texture2d>("Albedo", 0);
    let Normalize = uniform_texture:<texture2d>("Normalize", 1);
    let Metallic = uniform_texture:<texture2d>("Metallic", 2);
    let Roughness = uniform_texture:<texture2d>("Roughness", 3);

    let vnormal = transed_normal_tangent_map(Normalize, vf);

    return fout{
        albedo = texture(Albedo, vf.uv),
        vposition_m = float4::create(vf.vpos, texture(Metallic, vf.uv)->x),
        vnormalize_r = float4::create(vnormal, texture(Roughness, vf.uv)->x),
    };
}