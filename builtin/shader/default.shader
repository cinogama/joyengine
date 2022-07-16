// Default shader, only for test.
import je.shader;

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
        pos = je_mvp * float4(vdata.vertex, 1.),
        uv = vdata.uv;
    };
}

using fout = struct {
    color : float4
};

let main_tex = uniform:<texture2d>("main");

func frag(v2f : v2f)
{
    return fout{
        color = texture(main_tex, v2f.uv),    
    };
}