import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (NEVER);
ZWRITE  (DISABLE);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct
    {
        vertex: float3,
        uv: float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        color: float4,
    };
    
public func vert(v: vin)
{
    return v2f{
        pos = vec4!(v.vertex, 1.),
        uv = v.uv,
    };
}

let nearest_clamp = 
    Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);

WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, nearest_clamp);
    
public func frag(vf: v2f)
{
    return fout{
        color = tex2d(Main, vf.uv),
    };
}
