// test shader, only for test.
import je.shader;

ZTEST   (OFF);
ZWRITE  (DISABLE);

VAO_STRUCT vin {
    vertex  : float3,
    uv      : float2,
};

using v2f = struct {
    pos     : float4,
    uv      : float2,
};

using fout = struct {
    color: float4
};

public let vert = 
\v: vin = v2f{ pos = trans(v.vertex), uv = v.uv }
    where 
        trans = \v3: float3 = je_mvp *  float4(v3, 1.);;;

public let frag = 
\f: v2f = fout{ color = texture(main_texture, f.uv) }
    where
        main_texture = uniform:<texture2d>("main_texture");;
