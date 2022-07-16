// Default shader, only for test.
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

func vert(vdata : vin)
{
    return v2f{
        pos = je_mvp * float4(float3(0.5, 0.5, 0.5) * vdata.vertex, 1.),
        uv = vdata.uv,
    };
}

using fout = struct {
    color : float4
};

let main_texture = uniform:<texture2d>("main_texture");

func frag(v2f : v2f)
{
    return fout{
        color = texture(main_texture, v2f.uv),
    };
}