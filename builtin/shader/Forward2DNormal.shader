// Forward2DNormal.shader
// (C)Cinogama project. 2022. 版权所有

import je::shader;
import je::shader::light2d;

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
    pos     : float4,
    vpos    : float3,
    uv      : float2,
    vtangent_x : float3,
    vtangent_y : float3,
    vtangent_z : float3,
};

using fout = struct {
    albedo              : float4, // 漫反射颜色，在光照处理中用于计算颜色
    self_luminescence   : float4, // 自发光颜色，最终混合颜色公式中将叠加此颜色
    visual_coordinates  : float4, // 视空间坐标(xyz)，主要用于与光源坐标进行距离计算，决定后处理光照的影响系数
                                  // w 系数暂时留空，应当设置为1
};

func invscale_f3_2_f4(v: float3)
{
    return float4::create(v / abs(je_local_scale), 1.);
}

public func vert(v: vin)
{
    let vspace_position = je_mv * float4::create(v.vertex, 1.);
    let m_movement = movement(je_m);
    let v_movement = movement(je_v);
    return v2f{
        pos = je_p * vspace_position,
        vpos = vspace_position->xyz / vspace_position->w,
        uv = uvtrans(v.uv, je_tiling, je_offset),
        vtangent_x = (je_v * float4::create((je_m * invscale_f3_2_f4(
            float3::new(1., 0., 0.)))->xyz - m_movement, 1.))
                ->xyz - v_movement,
        vtangent_y = (je_v * float4::create((je_m * invscale_f3_2_f4(
            float3::new(0., 1., 0.)))->xyz - m_movement, 1.))
                ->xyz - v_movement,
        vtangent_z = (je_v * float4::create((je_m * invscale_f3_2_f4(
            float3::new(0., 0., -1.)))->xyz - m_movement, 1.))
                ->xyz - v_movement,
    };
}

func get_normal_from_map(normal_map: texture2d, uv : float2)
{
    return (float::new(2.) * texture(normal_map, uv)->xyz) - float3::new(1., 1., 1.);
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

public func frag(vf: v2f)
{
    let Albedo = uniform_texture:<texture2d>("Albedo", 0);
    let Normalize = uniform_texture:<texture2d>("Normalize", 1);

    let vnormal = transed_normal_tangent_map(Normalize, vf);

    let mut normal_effect_self_luminescence = float4::zero;
    for (let index, light : je_light2ds)
    {
        // 点光源照射部分
        let lvpos = je_v * float4::create(light->position->xyz, 1.);
        let f2l = vf.vpos - lvpos->xyz / lvpos->w;
        let ldistance = length(f2l);
        let ldir = normalize(f2l);

        let distance_factor = max(float::one - ldistance / light->position->w, float::zero);
        let fade_factor = distance_factor / light->factors->z; // 用线性衰减而不是指数衰减，否则法线效果会变得很弱
        let point_light_factor = vnormal->dot(ldir->negative) / (ldistance + 1.) * fade_factor * light->factors->x;

        // 平行光源照射部分
        let parallel_light_factor = vnormal->dot(light->direction->xyz->negative) * light->factors->y;

        // 最终光照产生的法线效果
        let light_effect_factor = max(float::zero, point_light_factor + parallel_light_factor);

        // 获取阴影, 如果当前像素被此灯光的阴影遮盖，则得到系数 0. 否则得到 1.
        let shadow_factor = float::one - texture(je_shadow2ds[index], (vf.pos->xy / vf.pos->w + float2::one) / 2.)->x;

        normal_effect_self_luminescence =
            shadow_factor
            * light->color->w
            * light_effect_factor
            * float4::create(light->color->xyz, 1.)
            + normal_effect_self_luminescence;
    }

    return fout{
        albedo = alphatest(texture(Albedo, vf.uv)),
        self_luminescence = normal_effect_self_luminescence,
        visual_coordinates = float4::create(vf.vpos, 1.),
    };
}