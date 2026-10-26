import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (LESS_EQUAL);
ZWRITE  (ENABLE);
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

// 单位四边形 [-1,1]²：顶点着色器把它映射到
// 「椭圆 + 背光扫掠条带」的视空间包围盒
WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        off: float2,
        dir: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    // JE_LOCAL_SCALE.yz：CPU 预计算的世界空间背光方向（xy）
    let dir_raw = JE_LOCAL_SCALE->yz;
    let shadow_dir_world = dir_raw / max(length(dir_raw), 0.000001);

    // JE_COLOR.xy：脚底椭圆中心（世界空间），.w：中心世界 Z；
    // JE_SHADOW_ELLIPSE：x=长轴全长（视空间 X），y=短轴全长
    //（视空间 Y），z=尾迹延伸长度，w=有效消隐距离
    let half_a = max(JE_SHADOW_ELLIPSE->x, 0.) * 0.5;
    let half_b = max(JE_SHADOW_ELLIPSE->y, 0.) * 0.5;

    // 椭圆中心（世界空间）与阴影方向变换到视空间；中心不走 JE_MV，
    // 避免实体旋转把脚下椭圆带离世界空间底部
    let center_v4 = JE_V * vec4!(
        JE_COLOR->x, JE_COLOR->y, JE_COLOR->w, 1.);
    let center_view = center_v4->xyz / center_v4->w;
    let dir_view_raw = (JE_V * vec4!(
        shadow_dir_world->x, shadow_dir_world->y, 0., 0.))->xy;
    let dir_view = dir_view_raw / max(length(dir_view_raw), 0.000001);
    let perp_view = vec2!(dir_view->y, 0. - dir_view->x);

    // 椭圆沿光向、垂直光向的支撑半宽
    let along = sqrt(
        half_a * half_a * dir_view->x * dir_view->x
            + half_b * half_b * dir_view->y * dir_view->y);
    let perp_support = sqrt(
        half_a * half_a * dir_view->y * dir_view->y
            + half_b * half_b * dir_view->x * dir_view->x);

    // 平行光阴影 = 椭圆沿光向平移扫过的条带：
    // 4 边形覆盖 s∈[-along, along+tail]，t∈[-perp_support, perp_support]
    let tail = max(JE_SHADOW_ELLIPSE->z, 0.);
    let s_end = along + tail;
    let frame_s = lerp(0. - along, s_end, (v.vertex->y + 1.) * 0.5);
    let frame_t = v.vertex->x * max(perp_support, 0.0001);

    let off_x = perp_view->x * frame_t + dir_view->x * frame_s;
    let off_y = perp_view->y * frame_t + dir_view->y * frame_s;

    let view_pos = vec3!(
        center_view->x + off_x,
        center_view->y + off_y,
        center_view->z);

    // SpriteShadow 用脚底实体级 painter key
    let shadow_depth = clamp(
        JE_SHADOW_KEY->x * JE_SHADOW_KEY->y + JE_SHADOW_KEY->z, 0., 0.999999);

    let pos = JE_P * vec4!(view_pos, 1.);
    return v2f{
        pos = vec4!(pos->x, pos->y, shadow_depth * pos->w, pos->w),
        off = vec2!(off_x, off_y),
        dir = dir_view,
    };
}
    
public func frag(vf: v2f)
{
    let half_a = max(max(JE_SHADOW_ELLIPSE->x, 0.) * 0.5, 0.0001);
    let half_b = max(max(JE_SHADOW_ELLIPSE->y, 0.) * 0.5, 0.0001);
    let dir_view = vf.dir;
    let perp_view = vec2!(dir_view->y, 0. - dir_view->x);
    let tail = max(JE_SHADOW_ELLIPSE->z, 0.);
    let fade_len = max(JE_SHADOW_ELLIPSE->w, 0.0001);

    // 椭圆（固定在视空间 X/Y，不随光向旋转）
    let ex = vf.off->x / half_a;
    let ey = vf.off->y / half_b;
    let ellipse_enabled =
        step(max(JE_SHADOW_ELLIPSE->x, 0.), 0.0001)
            * step(max(JE_SHADOW_ELLIPSE->y, 0.), 0.0001);
    let ellipse_cover =
        clamp((1. - (ex * ex + ey * ey)) * 4., 0., 1.) * ellipse_enabled;

    // 阴影局部坐标
    let s = dot(vf.off, dir_view);
    let t = dot(vf.off, perp_view);

    // 椭圆二次型 A s² + 2 B s t + C t² = 1
    let A = dir_view->x * dir_view->x / (half_a * half_a)
        + dir_view->y * dir_view->y / (half_b * half_b);
    let B = dir_view->x * dir_view->y
        * (1. / (half_a * half_a) - 1. / (half_b * half_b));
    let C = dir_view->y * dir_view->y / (half_a * half_a)
        + dir_view->x * dir_view->x / (half_b * half_b);

    let perp_support = sqrt(
        half_a * half_a * dir_view->y * dir_view->y
            + half_b * half_b * dir_view->x * dir_view->x);

    // 背光扫掠：|t| 在椭圆支撑范围内时，从椭圆背光侧边界
    //（大根 s_plus）起沿光向延伸；边界与椭圆严格相切，无接缝
    let disc_t = B * B * t * t - A * (C * t * t - 1.);
    let s_plus = (0. - B * t + sqrt(max(disc_t, 0.))) / max(A, 0.0001);
    let inside_t = step(perp_support, abs(t));

    let d_behind = s - s_plus;
    let u = clamp(d_behind / fade_len, 0., 1.);
    let fade = 1. - u * u * (3. - 2. * u);

    let in_tail =
        inside_t
            * step(d_behind, 0.)
            * step(tail, d_behind)
            * step(tail, 0.0001);

    let cover = max(ellipse_cover, in_tail * fade);
    let shadow_factor = JE_LOCAL_SCALE->x * cover;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
