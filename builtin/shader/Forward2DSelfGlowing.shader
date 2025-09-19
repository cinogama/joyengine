// Forward2DSelfGlowing.shader
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;
SHARED  (false);
ZTEST   (LESS);
ZWRITE  (ENABLE);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,
        uv      : float2,
        normal  : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        vpos    : float3,
        vnorm   : float3,
        uv      : float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        // 漫反射颜色，在光照处理中用于计算颜色
        albedo              : float4,
        // 自发光颜色，最终混合颜色公式中将叠加此颜色
        self_luminescence   : float4,
        // 视空间坐标(xyz)，主要用于与光源坐标进行距离计算，
        // 决定后处理光照的影响系数，w 系数暂时留空，应当设
        // 置为1
        vspace_position     : float4,
        // 视空间法线(xyz)，主要用于光照后处理进行光照计算
        // 的高光等效果，w 系数暂时留空，应当设置为1
        vspace_normalize    : float4,
    };
    
public func vert(v: vin)
{
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    let vnorm = normalize(vec3x3!(JE_MV) * v.normal);
    return v2f{
        pos = JE_P * vpos,
        vpos = vpos->xyz / vpos->w,
        vnorm = vnorm,
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}

let NearestSampler  = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Albedo          = texture2d::uniform(0, NearestSampler);
WOSHADER_UNIFORM!
    let SelfGlowing     = vec1!(1.);
    
public func frag(vf: v2f)
{
    let albedo_color = JE_COLOR * tex2d(Albedo, vf.uv);
    return fout{
        albedo = alphatest(albedo_color),
        self_luminescence = vec4!(albedo_color->xyz * SelfGlowing, 1.),
        vspace_position = vec4!(vf.vpos, 1.),
        vspace_normalize = vec4!(vf.vnorm, 1.),
    };
}
