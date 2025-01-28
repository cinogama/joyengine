// Unlit.shader

import je::shader;

SHARED  (true);
ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
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
        pos = je_mvp * vec4(v.vertex, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}

let NearestSampler  = sampler2d::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
let Main            = uniform_texture:<texture2d>("Main", NearestSampler, 0);

public func frag(vf: v2f)
{
    return fout{
        color = alphatest(je_color * texture(Main, vf.uv)),
    };
}
