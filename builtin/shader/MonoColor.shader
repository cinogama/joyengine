import je.shader;

ZTEST(LESS);
ZWRITE(ENABLE);
BLEND(ONE, ZERO);
CULL(NONE);

VAO_STRUCT vin{
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
        color = je_color_factor * float4::new(1., 1., 1., 1.)
    };
}