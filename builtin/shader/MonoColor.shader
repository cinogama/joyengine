// MonoColor.shader
// (C)Cinogama project. 2022. 版权所有

import je.shader;

ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT!vin{
    vertex: float3,
};

using v2f = struct {
    pos: float4,
};

using fout = struct {
    color: float4
};

public func vert(v: vin)
{
    return v2f{
        pos = je_mvp * float4::create(v.vertex, 1.)
    };
}

public func frag(vf: v2f)
{
    return fout{
        color = uniform("Color", float4_one)
    };
}