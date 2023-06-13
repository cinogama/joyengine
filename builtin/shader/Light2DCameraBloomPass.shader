import je.shader;
import je.shader.light2d;

ZTEST(ALWAYS);
ZWRITE(DISABLE);
BLEND(ONE, ZERO);
CULL(BACK);

VAO_STRUCT!vin
{
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
        pos = float4::create(v.vertex, 1.),
        uv = v.uv,
    };
}

func multi_sampling_for_bias_self_glowing(tex: texture2d, reso: float2, uv: float2)
{
    let mut result = float3::zero;
    let bias = 2.;

    let bias_weight = [
        (-1., 1., 0.094742),    (0., 1., 0.118318),     (1., 1., 0.094742),
        (-1., 0., 0.118318),    (0., 0., 0.147761),     (1., 0., 0.118318),
        (-1., -1., 0.094742),   (0., -1., 0.118318),    (1., -1., 0.094742),
    ];

    let reso_inv = float2::one / reso;

    for (let _, (x, y, weight) : bias_weight)
    {
        result = result + texture(
            tex, uv + reso_inv * float2::create(x, y) * bias
        )->xyz * weight;  
    }
    return result;
}

public func frag(vf: v2f)
{
    let albedo_buffer = je_light2d_defer_albedo;
    let self_lumine = je_light2d_defer_self_luminescence;
    let light_buffer = uniform_texture:<texture2d>("Light", 0);

    let albedo_color_rgb = pow(texture(albedo_buffer, vf.uv)->xyz, float3::new(2.2, 2.2, 2.2));
    let light_color_rgb = texture(light_buffer, vf.uv)->xyz;
    let self_lumine_color_rgb = multi_sampling_for_bias_self_glowing(self_lumine, je_light2d_resolutin, vf.uv);
    let mixed_color_rgb = max(float3::zero, albedo_color_rgb
        * (self_lumine_color_rgb + light_color_rgb + float3::new(0.03, 0.03, 0.03)));

    let hdr_color_rgb = mixed_color_rgb / (mixed_color_rgb + float3::new(1., 1., 1.));
    let hdr_ambient_with_gamma = pow(hdr_color_rgb, float3::new(1. / 2.2, 1. / 2.2, 1. / 2.2, ));

    return fout{
        color = float4::create(hdr_ambient_with_gamma, 1.)
    };
}