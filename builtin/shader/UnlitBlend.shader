// UnlitBlend.shader

import je::shader;

SHARED  (true);
ZTEST   (LESS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (NONE);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2,
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
    return v2f{
        pos = je_mvp * float4::create(v.vertex, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}

public func frag(vf: v2f)
{
    let nearest_repeat = sampler2d::create(NEAREST, NEAREST, NEAREST, REPEAT, REPEAT);
    let Main = uniform_texture:<texture2d>("Main", nearest_repeat, 0);
    return fout{
        color = alphatest(texture(Main, vf.uv)),
    };
}
