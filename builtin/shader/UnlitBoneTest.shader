// Unlit.shader

import je::shader;

SHARED  (true);
ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);


GRAPHIC_STRUCT! Bone
{
    bones:  float4x4[256],
};

UNIFORM_BUFFER! Bones = 6
{
    data: struct Bone,
};

VAO_STRUCT! vin {
    vertex      : float3,
    uv          : float2,
    normal      : float3,
    bone_index  : integer4,
    bone_weight : float4,
};

using v2f = struct {
    pos     : float4,
    uv      : float2,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    let origin_vertex_pos = vec4(v.vertex, 1.);
    let mut total_postion = origin_vertex_pos;

    total_postion += (data->bones(v.bone_index->x) * origin_vertex_pos) * v.bone_weight->x;
    total_postion += (data->bones(v.bone_index->y) * origin_vertex_pos) * v.bone_weight->y;
    total_postion += (data->bones(v.bone_index->z) * origin_vertex_pos) * v.bone_weight->z;
    total_postion += (data->bones(v.bone_index->w) * origin_vertex_pos) * v.bone_weight->w;

    return v2f{
        pos = vec4(0., -0.5, 0., 0.) + total_postion * vec4(0.005, 0.005, -0.005, 1.),
        uv = v.uv, //uvtrans(v.uv, je_tiling, je_offset),
    };
}

let NearestSampler  = sampler2d::create(NEAREST, NEAREST, NEAREST, REPEAT, REPEAT);
let Main            = uniform_texture:<texture2d>("Main", NearestSampler, 0);

public func frag(vf: v2f)
{
    return fout{
        color = alphatest(je_color * texture(Main, vf.uv)),
    };
}
