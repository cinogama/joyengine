// WorldGrid2D.shader
// 2D（正交）模式下编辑器视口的世界坐标网格（xOy 平面，z=0，半透明）：
// 输入为覆盖整个屏幕的 NDC 四边形（见 Gizmo/Shapes.wo 的 FULLSCREEN_NDC_QUAD），
// 片元内由 NDC 重建像素视线（与 ray::from 的重建方式一致：逆投影矩阵反变换到
// 眼空间、w 除法后由摄像机位姿转到世界空间），再与 z=0 平面求交得到世界坐标，
// 按导数抗锯齿绘制 1/10/100/1000 单位四级网格：各级随屏幕密度逐级消解
//（远处细网格先行溶解，只留下更粗的层级，避免密集网格产生摩尔纹），
// 整体仅保留远距雾化。坐标轴线色与变换游标一致（x 红 / y 绿）。
// 摄像机位姿与投影矩阵由每次绘制通过 JE_GRID_* uniform 提供。
import pkg::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,   // NDC 坐标（z 不使用）
    };

WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        ndc     : float2,   // 片元对应的 NDC 坐标（+y 为屏幕上方）
    };

WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };

// 摄像机世界空间位姿
WOSHADER_UNIFORM!
    public let JE_GRID_CAM_POS     = vec3!(0., 0., 0.);
WOSHADER_UNIFORM!
    public let JE_GRID_CAM_RIGHT   = vec3!(1., 0., 0.);
WOSHADER_UNIFORM!
    public let JE_GRID_CAM_UP      = vec3!(0., 1., 0.);
WOSHADER_UNIFORM!
    public let JE_GRID_CAM_FORWARD = vec3!(0., 0., 1.);
// Camera::Projection 的逆投影矩阵与正交标记（视线重建与 ray::from 一致）
WOSHADER_UNIFORM!
    public let JE_GRID_INV_PROJ    = float4x4::unit;
WOSHADER_UNIFORM!
    public let JE_GRID_IS_ORTHO    = vec1!(0.);

public func vert(v: vin)
{
    return v2f{
        pos = vec4!(v.vertex, 1.),
        ndc = vec2!(v.vertex->x, v.vertex->y),
    };
}

// 抗锯齿网格线因子：p 为世界坐标（整数线），返回 [0,1]，
// width 越大线条越粗（1 约为 2 像素宽）。
WOSHADER_FUNCTION!
    func grid_line(
        p       : float,
        width   : float)
    {
        // fwidth 近似：p 每像素的跨度（防除零）
        let fw = max(abs(ddx(p)) + abs(ddy(p)), 0.0000001);
        // 到最近整数线的像素距离（+fw/2 偏移补偿导数量化的不对称）
        let d = abs(fract(p + fw * 0.5 - 0.5) - 0.5) / fw;
        return 1. - clamp(d * width, 0., 1.);
    }

// 单个层级的网格线因子：p/q 为世界坐标（取两轴），spacing 为该级间距。
// 除间距缩放外还按屏幕密度消解——每格不足约 4 像素时该级完全溶解
//（多于约 20 像素完整显示）：远处细网格先行消失，只留下更粗的层级，
// 避免亚像素密集的重复线产生摩尔纹。
WOSHADER_FUNCTION!
    func grid_level(
        p       : float,
        q       : float,
        spacing : float,
        width   : float)
    {
        // 该像素处世界坐标的每像素跨度（取两轴较大者，防除零）
        let px = max(abs(ddx(p)), abs(ddx(q)));
        let py = max(abs(ddy(p)), abs(ddy(q)));
        let per_pixel = max(px + py, 0.0000001);

        // 每格占据的像素数：4px 以下完全溶解，20px 以上完整显示
        let cell_px = spacing / per_pixel;
        let dissolve = clamp((cell_px - 4.) / 16., 0., 1.);
        let smooth_dissolve = dissolve * dissolve * (3. - 2. * dissolve);

        return max(
            grid_line(p / spacing, width),
            grid_line(q / spacing, width)) * smooth_dissolve;
    }

// 坐标轴线因子：p 为世界坐标，轴线位于 p == 0 处。
// 与 grid_line 不同，这里按“到零轴的像素距离”计算（不用 fract 的
// 最近整数距离）：视线掠射、该坐标每像素跨度爆炸时，轴线因子自然
// 趋向 0 而不是满幅为 1，避免地平线附近出现整片轴色的色带。
WOSHADER_FUNCTION!
    func axis_line(
        p       : float,
        width   : float)
    {
        // fwidth 近似：p 每像素的跨度（防除零）
        let fw = max(abs(ddx(p)) + abs(ddy(p)), 0.0000001);
        // 到零轴的像素距离
        let d = abs(p) / fw;
        return 1. - clamp(d * width, 0., 1.);
    }

public func frag(vf: v2f)
{
    // 重建像素视线（与 ray::from 一致）：
    // 逆投影矩阵把 NDC 反变换回眼空间（w 除法），再经摄像机位姿转到世界空间
    let pe = JE_GRID_INV_PROJ * vec4!(vf.ndc, 1., 1.);
    let eye = pe->xyz / pe->w;

    // 透视：过摄像机沿视线；正交：起点随像素平移、方向恒定
    let persp_dir =
        JE_GRID_CAM_RIGHT * eye->x
        + JE_GRID_CAM_UP * eye->y
        + JE_GRID_CAM_FORWARD * eye->z;
    let ray_dir = normalize(
        persp_dir + (JE_GRID_CAM_FORWARD - persp_dir) * JE_GRID_IS_ORTHO);
    let ray_origin = JE_GRID_CAM_POS
        + (JE_GRID_CAM_RIGHT * eye->x + JE_GRID_CAM_UP * eye->y) * JE_GRID_IS_ORTHO;

    // 与 z=0 平面求交；视线近乎平行该平面（或交点在身后）时整片失效
    // 注意：woshader 的 step(value, edge) 语义为 value >= edge
    let slope_valid = step(abs(ray_dir->z), 0.00001);
    let dz = ray_dir->z + (1. - ray_dir->z) * (1. - slope_valid);
    let t = (0. - ray_origin->z) / dz;
    let front = step(t, 0.) * slope_valid;
    let hit = ray_origin + ray_dir * t;

    // 四级网格线：1 / 10 / 100 / 1000 单位间距
    //（各级随屏幕密度消解，见 grid_level；拉远时细层级先行溶解，
    //  依次只留下更粗的层级）+ 坐标轴（轴色与变换游标一致）
    let minor  = grid_level(hit->x, hit->y, vec1!(1.),    vec1!(1.0));
    let major  = grid_level(hit->x, hit->y, vec1!(10.),   vec1!(1.4));
    let grand  = grid_level(hit->x, hit->y, vec1!(100.),  vec1!(1.8));
    let coarse = grid_level(hit->x, hit->y, vec1!(1000.), vec1!(2.2));
    let axis_x = axis_line(hit->y, vec1!(1.8));
    let axis_y = axis_line(hit->x, vec1!(1.8));

    // 整体只保留远距雾化：层级交接已完全交给密度消解，
    // 半径需远大于编辑常用距离，避免拉远后粗层级被整体抹掉
    let dist = distance(hit, JE_GRID_CAM_POS);
    let fade = clamp(1. - dist / 5000., 0., 1.);
    let smooth_fade = fade * fade * (3. - 2. * fade);

    let MINOR_LINE_COLOR  = vec3!(0.42, 0.45, 0.50);
    let MAJOR_LINE_COLOR  = vec3!(0.62, 0.65, 0.70);
    let GRAND_LINE_COLOR  = vec3!(0.72, 0.75, 0.80);
    let COARSE_LINE_COLOR = vec3!(0.80, 0.82, 0.86);
    let AXIS_X_COLOR = vec3!(0.92, 0.32, 0.32);
    let AXIS_Y_COLOR = vec3!(0.38, 0.85, 0.38);

    let rgb_minor = MINOR_LINE_COLOR
        + (MAJOR_LINE_COLOR - MINOR_LINE_COLOR) * major;
    let rgb_major = rgb_minor + (GRAND_LINE_COLOR - rgb_minor) * grand;
    let rgb_grand = rgb_major + (COARSE_LINE_COLOR - rgb_major) * coarse;
    let rgb_axisx = rgb_grand + (AXIS_X_COLOR - rgb_grand) * axis_x;
    let rgb_final = rgb_axisx + (AXIS_Y_COLOR - rgb_axisx) * axis_y;

    let alpha = max(
        minor * 0.22,
        max(
            major * 0.35,
            max(
                grand * 0.45,
                max(
                    coarse * 0.55,
                    max(axis_x, axis_y) * 0.65))))
        * smooth_fade
        * front;

    return fout{
        color = vec4!(rgb_final, alpha),
    };
}
