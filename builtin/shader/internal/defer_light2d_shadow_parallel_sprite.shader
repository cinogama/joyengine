import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

// SpriteShadow 是纯加法的接触阴影：只往阴影缓冲里叠加强度。
// 它的四边形只是「椭圆 + 尾迹」的包围框（片元里才裁出实际覆盖），
// 且一个片元可能覆盖离脚底很远的区域；如果写逐像素 painter key 深度，
// 包围框内本不产生阴影的空洞也会以脚底 key 占据深度，使随后绘制的
// 遮挡阴影 / 自阴影（key 更大，即更靠后）被深度测试丢弃，出现
// “接触阴影把别的阴影擦掉”的瑕疵。因此这里只测试深度（不覆盖更靠前
// 的既有贡献），不写深度。CPU 侧保证本 pass 在该光源其余阴影贡献
// 之后绘制，后方自阴影接收面的覆盖写入不会擦掉接触阴影。
ZTEST   (LESS_EQUAL);
ZWRITE  (DISABLE);
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

    let ellipse_enabled =
        step(max(JE_SHADOW_ELLIPSE->x, 0.), 0.0001)
            * step(max(JE_SHADOW_ELLIPSE->y, 0.), 0.0001);

    // 阴影局部坐标
    let s = dot(vf.off, dir_view);
    let t = dot(vf.off, perp_view);

    // 椭圆二次型 A s² + 2 B s t + C t² = 1
    //（即固定在视空间 X/Y、不随光向旋转的接触椭圆）
    let A = dir_view->x * dir_view->x / (half_a * half_a)
        + dir_view->y * dir_view->y / (half_b * half_b);
    let B = dir_view->x * dir_view->y
        * (1. / (half_a * half_a) - 1. / (half_b * half_b));
    let C = dir_view->y * dir_view->y / (half_a * half_a)
        + dir_view->x * dir_view->x / (half_b * half_b);

    // 接触椭圆：核心区满强度，外缘约 13% 半径渐隐（软边）
    let q = A * s * s + 2. * B * s * t + C * t * t;
    let ellipse_cover = clamp((1. - q) * 4., 0., 1.) * ellipse_enabled;

    // 尾迹 = 软椭圆沿背光方向的扫掠：对片段及其迎光侧取二次型的最小值
    //（椭圆最厚处 u_thick / q_thick）。扫掠在椭圆最厚处即达到满覆盖，
    // 所以椭圆软边之外的背光侧不会留下一条没被尾迹覆盖的亮缝；迎光侧
    // 仍取椭圆自身的软边。
    let u_thick = (0. - B * t) / max(A, 0.0001);
    let q_thick = (C - B * B / max(A, 0.0001)) * t * t;
    let swept = clamp((1. - lerp(q, q_thick, step(s, u_thick))) * 4., 0., 1.);

    // 尾迹从椭圆背光侧边界（大根 s_plus）起算，按消隐距离渐隐
    let disc_t = B * B * t * t - A * (C * t * t - 1.);
    let s_plus = (0. - B * t + sqrt(max(disc_t, 0.))) / max(A, 0.0001);

    let d_behind = s - s_plus;
    let u = clamp(d_behind / fade_len, 0., 1.);
    let fade = 1. - u * u * (3. - 2. * u);

    let in_tail =
        step(tail, d_behind)
            * step(tail, 0.0001)
            * fade;

    let cover = max(ellipse_cover, in_tail * swept);
    let shadow_factor = JE_LOCAL_SCALE->x * cover;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
