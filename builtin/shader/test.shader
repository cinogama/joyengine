// test shader, only for test.
import je.shader;

ZTEST   (OFF);
ZWRITE  (DISABLE);

using VAO_STRUCT vin = struct {
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

let vert = \v: vin = v2f{ pos = trans(v.vertex), uv = v.uv }
    where 
        trans = \v3: float3 = je_mvp *  float4(v3, 1.);;;

let frag = \f: v2f = fout{ color = inverse_col(texture(main_texture, f.uv)) }
    where
        main_texture = uniform:<texture2d>("main_texture"),
        inverse_col = \col: float4 = float4((white - col)->xyz(), col->w());,
        white = float4(1., 1., 1., 1.);;
