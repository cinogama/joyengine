// Mono.shader

import je::shader;

SHARED  (true);
ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin {
    vertex  : float3,
};

using v2f = struct {
    pos     : float4,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    return v2f{
        pos = je_mvp * vec4(v.vertex, 1.)
    };
}

public func frag(_: v2f)
{
    return fout{
        color = je_color,
    };
}
