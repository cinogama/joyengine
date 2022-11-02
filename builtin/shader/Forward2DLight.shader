// Forward2DLight.shader
// (C)Cinogama project. 2022.

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
    fragpos : float3,
    uv      : float2,
    rtangent_x : float3, 
    rtangent_y : float3,
    rtangent_z : float3,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    let frag_position = je_m * float4::create(v.vertex, 1.);
    let move_position = movement(je_m);
    return v2f{
        fragpos = frag_position->xyz,
        pos  = je_vp * frag_position,
        uv   = v.uv,
        rtangent_x = (je_m * float4::new(1., 0., 0., 1.))->xyz - move_position,
        rtangent_y = (je_m * float4::new(0., 1., 0., 1.))->xyz - move_position,
        rtangent_z = (je_m * float4::new(0., 0., -1., 1.))->xyz - move_position,
    };
}

public func get_normal_from_map(normal_map: texture2d, uv : float2)
{
    return (float::new(2.) * texture(normal_map, uv)->xyz) - float3::new(1., 1., 1.);
}

public func transed_normal_tangent_map(normal_map: texture2d, vertex_info : v2f)
{
    let normal_from_map = get_normal_from_map(normal_map, vertex_info.uv);
    return normalize(
        vertex_info.rtangent_x * normal_from_map +
        vertex_info.rtangent_y * normal_from_map +
        vertex_info.rtangent_z * normal_from_map
    );
}

public func frag(vf: v2f)
{
    let Albedo = uniform_texture:<texture2d>("Albedo", 0);
    let Normalize = uniform_texture:<texture2d>("Normalize", 1);
    let Metallic = uniform_texture:<texture2d>("Metallic", 2);
    let AO = uniform_texture:<texture2d>("AO", 3);

    let pixel_normal = transed_normal_tangent_map(Normalize, vf);

    let mut out_color = float3::new(0., 0., 0.);
    for (let mut i = 0; i < MAX_SHADOW_LIGHT_COUNT; i += 1)
    {
        let active_light = je_light2ds[i];
        // TODO: Parallel & angle light.
        // TODO: PBR Pipeline?
        let fragment_to_light_direction = normalize(active_light->position->xyz - vf.fragpos);
        let light_effect_factor = fragment_to_light_direction->dot(pixel_normal)->clamp(0., 1.);
        let light_effect_color = active_light->color->w * light_effect_factor * active_light->color->xyz;

        // TODO; Effect distance decrease.
        out_color = out_color + light_effect_color * texture(Albedo, vf.uv)->xyz;
    }

    return fout{
        color = float4::create(out_color, 1.), // TODO;
    };
}