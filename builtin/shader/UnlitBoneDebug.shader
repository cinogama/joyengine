// Unlit.shader

import je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin {
    vertex      : float3,
    //uv          : float2,
    //normal      : float3,
    //bone_index  : integer4,
    //bone_weight : float4,
};

using v2f = struct {
    pos     : float4,
    //uv      : float2,
};

using fout = struct {
    color   : float4
};

let Index = uniform("Index", 0);
let Offset = uniform("Offset", float3::zero);

public func vert(v: vin)
{
    let origin_vertex_pos = vec4(v.vertex * 10. + Offset, 1.);

    return v2f{
        pos = vec4(0., -0.5, 0., 0.) + origin_vertex_pos * vec4(0.005, 0.005, -0.005, 1.),
        //uv = v.uv, //uvtrans(v.uv, je_tiling, je_offset),
    };
}

public func frag(_: v2f)
{
    return fout{
        color = vec4(float3::const(1.0, 0.0, 1.0) * vec1(Index) / 52., 1.0),
    };
}
