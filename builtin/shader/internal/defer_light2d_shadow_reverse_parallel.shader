import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
CULL    (FRONT);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        factor: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    // ATTENTION: We will using JE_COLOR: float4 to pass lwpos.
    let light_vdir = (JE_V * JE_COLOR)->xyz - movement(JE_V);
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    
    let shadow_vdir = vec3!(normalize(light_vdir->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = JE_P * vec4!(vpos->xyz + shadow_vdir, 1.)};
}

public func frag(_: v2f)
{
    // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
    let shadow_factor = JE_LOCAL_SCALE->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
