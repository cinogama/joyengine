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

public func calculate_2dlight_effect(light_id: int)
{
    let current_light2d = je_light2ds[light_id];
}

public func vert(v: vin)
{
    let frag_position = je_m * float4::create(v.vertex, 1.);
    return v2f{
        fragpos = frag_position->xyz,
        pos  = je_vp * frag_position,
        uv   = v.uv,
        rtangent_x = (je_m * float4::new(1., 0., 0., 1.))->xyz,
        rtangent_y = (je_m * float4::new(0., 1., 0., 1.))->xyz,
        rtangent_z = (je_m * float4::new(0., 0., -1., 1.))->xyz,
    };
}

public func frag(vf: v2f)
{
    let Albedo = uniform_texture:<texture2d>("Albedo", 0);
    let Normalize = uniform_texture:<texture2d>("Normalize", 1);
    let Metallic = uniform_texture:<texture2d>("Metallic", 2);
    let AO = uniform_texture:<texture2d>("AO", 3);

    for (let mut i = 0; i < MAX_SHADOW_LIGHT_COUNT; i += 1)
    {
        
    }

    return fout{
        color = je_color_factor * texture(Albedo, vf.uv),
    };
}