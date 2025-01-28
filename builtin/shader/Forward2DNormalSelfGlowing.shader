// Forward2DNormalSelfGlowing.shader

import je::shader;

SHARED  (false);
ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2,
};

using v2f = struct {
    pos         : float4,
    vpos        : float3,
    uv          : float2,
    vtangent_x  : float3,
    vtangent_y  : float3,
    vtangent_z  : float3,
};

using fout = struct {
    albedo              : float4, // 漫反射颜色，在光照处理中用于计算颜色
    self_luminescence   : float4, // 自发光颜色，最终混合颜色公式中将叠加此颜色
    vspace_position     : float4, // 视空间坐标(xyz)，主要用于与光源坐标进行距离计算，
                                  // 决定后处理光照的影响系数，w 系数暂时留空，应当设
                                  // 置为1
    vspace_normalize    : float4, // 视空间法线(xyz)，主要用于光照后处理进行光照计算
                                  // 的高光等效果，w 系数暂时留空，应当设置为1
};

SHADER_FUNCTION!
func vtangent(normal: float3)
{
    return normalize(je_mv->float3x3 * normal / abs(je_local_scale));
}

public func vert(v: vin)
{
    let vspace_position = je_mv * vec4(v.vertex, 1.);
    return v2f{
        pos = je_p * vspace_position,
        vpos = vspace_position->xyz / vspace_position->w,
        uv = uvtrans(v.uv, je_tiling, je_offset),
        vtangent_x = vtangent(float3::const(1., 0., 0.)),
        vtangent_y = vtangent(float3::const(0., 1., 0.)),
        vtangent_z = vtangent(float3::const(0., 0., -1.)),
    };
}

func get_normal_from_map(normal_map: texture2d, uv : float2)
{
    return (float::const(2.) * texture(normal_map, uv)->xyz) - float3::const(1., 1., 1.);
}

func transed_normal_tangent_map(normal_map: texture2d, vertex_info : v2f)
{
    let normal_from_map = get_normal_from_map(normal_map, vertex_info.uv);
    return normalize(
        vertex_info.vtangent_x * normal_from_map->x +
        vertex_info.vtangent_y * normal_from_map->y +
        vertex_info.vtangent_z * normal_from_map->z
    );
}

let NearestSampler  = sampler2d::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
let Albedo          = uniform_texture:<texture2d>("Albedo", NearestSampler, 0);
let Normalize       = uniform_texture:<texture2d>("Normalize", NearestSampler, 1);

let self_glowing    = uniform("SelfGlowing", float::one);

public func frag(vf: v2f)
{
    let albedo_color = je_color * texture(Albedo, vf.uv);
    return fout{
        albedo = alphatest(albedo_color),
        self_luminescence = vec4(albedo_color->xyz * self_glowing, 1.),
        vspace_position = vec4(vf.vpos, 1.),
        vspace_normalize = vec4(transed_normal_tangent_map(Normalize, vf), 1.),
    };
}
