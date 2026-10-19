// Default shader
import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos : float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color : float4,
    };
    
public let vert =
    \v: vin = v2f{ pos = JE_MVP * vertex_pos }
        where vertex_pos = vec4!(v.vertex, 1.)
        ;
    ;
    
public let frag =
    \_: v2f = fout{ color = vec4!(t, 0., t, 1.) }
        where t = JE_TIME->y
        ;
    ;
