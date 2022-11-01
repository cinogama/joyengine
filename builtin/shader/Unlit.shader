import je.shader;

ZTEST(LESS);
ZWRITE(ENABLE);
BLEND(ONE, ZERO);
CULL(NONE);

VAO_STRUCT vin{
    vertex: float3,
    uv : float2,
};

using v2f = struct {
pos: float4,
uv : float2,
};

using fout = struct {
color: float4
};

public func vert(v: vin)
{
    return v2f{
        pos = je_mvp * float4::create(v.vertex, 1.),
        uv = v.uv,
    };
}

public func frag(vf: v2f)
{
    let tex = uniform_texture:<texture2d>("MainTexture", 0);

    return fout{
        color = je_color_factor * texture(tex, vf.uv),
    };
}