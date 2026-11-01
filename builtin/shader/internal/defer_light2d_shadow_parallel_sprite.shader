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
        // s（沿背光方向）、t（垂直背光方向）的视空间坐标
        st: float2,
        // 视空间偏移除以椭圆半径：椭圆二次型 q = dot(ell, ell)
        ell: float2,
        // 逐 draw 常量：A、-B/A、1/A、det/A²（见 frag 的二次型）
        coef: float4,
        // tail_on（尾迹是否启用）、1/fade_len
        misc: float2,
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

    // 椭圆二次型 A s² + 2 B s t + C t² = 1 的系数只由 draw 参数决定，
    // 在顶点算一次传给片元，省掉每像素的除法和重复展开；
    // 半轴下限与片元一致，避免零半轴除零。
    let ha = max(half_a, 0.0001);
    let hb = max(half_b, 0.0001);
    let ia2 = 1. / (ha * ha);
    let ib2 = 1. / (hb * hb);
    let A = dir_view->x * dir_view->x * ia2
        + dir_view->y * dir_view->y * ib2;
    let B = dir_view->x * dir_view->y * (ia2 - ib2);
    let C = dir_view->y * dir_view->y * ia2
        + dir_view->x * dir_view->x * ib2;
    let inv_a = 1. / A;
    // 椭圆最厚处 u_thick = b_a * t；背光侧边界
    // s_plus = b_a * t + sqrt(1/A - k_a * t²)
    let b_a = (0. - B) * inv_a;
    let k_a = C * inv_a - b_a * b_a;

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
    let tail_on = step(tail, 0.0001);
    let fade_len = max(JE_SHADOW_ELLIPSE->w, 0.0001);
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
        st = vec2!(frame_s, frame_t),
        ell = vec2!(off_x / ha, off_y / hb),
        coef = vec4!(A, b_a, inv_a, k_a),
        misc = vec2!(tail_on, 1. / fade_len),
    };
}
    
public func frag(vf: v2f)
{
    let s = vf.st->x;
    let t = vf.st->y;

    // 接触椭圆（固定在视空间 X/Y、不随光向旋转）：核心区满强度，
    // 外缘约 13% 半径渐隐（软边）
    let q = dot(vf.ell, vf.ell);
    let ellipse_cover = clamp((1. - q) * 4., 0., 1.);

    // 尾迹 = 软椭圆沿背光方向的扫掠：把片段沿迎光方向回推到椭圆最厚处
    //（u_thick = b_a * t），二次型相应减去 A·ds²。扫掠在椭圆最厚处即
    // 达到满覆盖，所以椭圆软边之外的背光侧不会留下一条没被尾迹覆盖的
    // 亮缝；迎光侧不再回推，仍取椭圆自身的软边。
    let ds = max(s - vf.coef->y * t, 0.);
    let swept = clamp((1. - (q - vf.coef->x * ds * ds)) * 4., 0., 1.);

    // 尾迹从椭圆背光侧边界（大根 s_plus）起算，按消隐距离渐隐；
    // CPU 保证消隐距离不超过尾迹长度，因此无需再按长度截断。
    let s_plus = vf.coef->y * t
        + sqrt(max(vf.coef->z - vf.coef->w * t * t, 0.));
    let u = clamp((s - s_plus) * vf.misc->y, 0., 1.);
    let fade = 1. - u * u * (3. - 2. * u);

    let cover = max(ellipse_cover, (vf.misc->x * fade) * swept);
    let shadow_factor = JE_LOCAL_SCALE->x * cover;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
