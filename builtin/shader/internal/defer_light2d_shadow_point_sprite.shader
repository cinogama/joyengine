import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

// SpriteShadow 是纯加法的接触阴影：只往阴影缓冲里叠加强度。
// 它的四边形只是「椭圆 + 背光切线锥」的包围框（片元里才裁出实际覆盖），
// 且一个片元可能覆盖离脚底很远的区域；如果写逐像素 painter key 深度，
// 包围框内本不产生阴影的空洞也会以脚底 key 占据深度，使随后绘制的
// 遮挡阴影 / 自阴影（key 更大，即更靠后）被深度测试丢弃，出现
// “接触阴影把别的阴影擦掉”的瑕疵。因此这里只测试深度（不覆盖更靠前
// 的既有贡献），不写深度。
ZTEST   (LESS_EQUAL);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

// 单位四边形 [-1,1]²：顶点着色器把它映射到
// 「椭圆 + 背光切线锥」的视空间包围盒
WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
        off: float2,
        dir: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    // JE_LOCAL_SCALE.yz：CPU 预计算的世界空间背光方向（xy）
    let dir_raw = JE_LOCAL_SCALE->yz;
    let shadow_dir_world = dir_raw / max(length(dir_raw), 0.000001);

    // JE_COLOR.xy：脚底椭圆中心（世界空间），.z：光源到椭圆的平面距离，
    // .w：中心世界 Z；JE_SHADOW_ELLIPSE：x=长轴全长（视空间 X），
    // y=短轴全长（视空间 Y），z=尾迹延伸长度，w=有效消隐距离
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

    // 椭圆在阴影局部系（s 沿光向、t 垂直光向）中的二次型
    // A s² + 2 B s t + C t² = 1
    let A = dir_view->x * dir_view->x / (half_a * half_a)
        + dir_view->y * dir_view->y / (half_b * half_b);
    let B = dir_view->x * dir_view->y
        * (1. / (half_a * half_a) - 1. / (half_b * half_b));
    let C = dir_view->y * dir_view->y / (half_a * half_a)
        + dir_view->x * dir_view->x / (half_b * half_b);

    let along = sqrt(
        half_a * half_a * dir_view->x * dir_view->x
            + half_b * half_b * dir_view->y * dir_view->y);
    let perp_support = sqrt(
        half_a * half_a * dir_view->y * dir_view->y
            + half_b * half_b * dir_view->x * dir_view->x);

    // 尾迹延伸范围与光源距离
    let tail = max(JE_SHADOW_ELLIPSE->z, 0.);
    let s_end = along + tail;
    let light_dist = max(JE_COLOR->z, 0.0001);

    // 四边形横向包围：取光源到椭圆的切线斜率（阴影锥边界），
    // 法向锥退化时回退到支撑半宽 + 估算张开
    let det = A * C - B * B;
    let K = C - light_dist * light_dist * det;
    let slope_disc = B * B - K * A;
    let has_k = step(abs(K), 0.000001);
    let slope_ok = step(slope_disc, 0.);
    let valid = has_k * slope_ok;
    let denom = K + (1. - has_k);
    let sqm = sqrt(max(slope_disc, 0.));
    let m_hi = max(
        abs((0. - B + sqm) / denom),
        abs((0. - B - sqm) / denom));
    let t_slope = m_hi * max(s_end + light_dist, 0.);

    let spread_fallback = min(
        perp_support / max(
            sqrt(max(light_dist * light_dist - perp_support * perp_support, 0.000001)),
            0.000001),
        8.);
    let t_fallback = perp_support + max(s_end - along, 0.) * spread_fallback;
    let t_bound = clamp(
        lerp(t_fallback, t_slope, valid),
        perp_support,
        perp_support * 64. + tail * 4. + 0.001);

    // 四边形覆盖 s∈[-along, s_end]，t∈[-t_bound, t_bound]
    let frame_s = lerp(0. - along, s_end, (v.vertex->y + 1.) * 0.5);
    let frame_t = v.vertex->x * max(t_bound, 0.0001);

    let off_x = perp_view->x * frame_t + dir_view->x * frame_s;
    let off_y = perp_view->y * frame_t + dir_view->y * frame_s;

    let view_pos = vec3!(
        center_view->x + off_x,
        center_view->y + off_y,
        center_view->z);

    // SpriteShadow 用脚底实体级 painter key：仅用于深度测试（被更靠前的
    // 贡献遮挡），本 pass 不写深度，不参与遮挡仲裁。
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
    let light_dist = max(JE_COLOR->z, 0.0001);

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

    // 椭圆二次型
    let A = dir_view->x * dir_view->x / (half_a * half_a)
        + dir_view->y * dir_view->y / (half_b * half_b);
    let B = dir_view->x * dir_view->y
        * (1. / (half_a * half_a) - 1. / (half_b * half_b));
    let C = dir_view->y * dir_view->y / (half_a * half_a)
        + dir_view->x * dir_view->x / (half_b * half_b);

    // 从光源 P=(-L,0) 沿 (s+L,t) 看，光线与椭圆相切即阴影锥边界。
    // 解 Q(μ)=0：Q= A ws²+2B ws wt+C wt²，ws=s+L，wt=t
    let ws = s + light_dist;
    let wt = t;
    let Qw = A * ws * ws + 2. * B * ws * wt + C * wt * wt;
    let b = (0. - light_dist) * (A * ws + B * wt);
    let c0 = A * light_dist * light_dist - 1.;
    let disc = b * b - Qw * c0;
    let sq = sqrt(max(disc, 0.));

    // 片段在椭圆外且光线在到达片段前穿过椭圆 => 处于本影中
    let outside = step(Qw, 0.000001);
    let mu = (0. - b - sq) / max(Qw, 0.000001);
    let hit =
        step(disc, 0.)
            * step(mu, 0.)
            * step(1., mu)
            * outside;

    // 片段到椭圆命中点的背光距离（沿 s），用于长度截断与渐隐
    let s_b = 0. - light_dist + mu * ws;
    let d_behind = s - s_b;

    // 距离渐隐：椭圆背光侧之后按 smoothstep 消隐
    let u = clamp(d_behind / fade_len, 0., 1.);
    let fade = 1. - u * u * (3. - 2. * u);

    let in_tail =
        hit
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
