// WorldGrid.shader
// 编辑器视口的世界坐标网格（XZ 平面，y=0，半透明）：
// 输入为覆盖整个屏幕的 NDC 四边形（见 Gizmo/Shapes.wo 的 FULLSCREEN_NDC_QUAD），
// 片元内由 NDC 重建像素视线（与 ray::from 的重建方式一致：逆投影矩阵反变换到
// 眼空间、w 除法后由摄像机位姿转到世界空间），再与地面求交得到世界坐标，
// 按导数抗锯齿绘制 1/10 单位两级网格并随距离衰减。
// 摄像机位姿与投影矩阵由每次绘制通过 JE_GRID_* uniform 提供。
import woo::std;

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

    // 与 y=0 平面求交；视线近乎水平（或交点在身后）时整片失效
    // 注意：woshader 的 step(value, edge) 语义为 value >= edge
    let slope_valid = step(abs(ray_dir->y), 0.00001);
    let dy = ray_dir->y + (1. - ray_dir->y) * (1. - slope_valid);
    let t = (0. - ray_origin->y) / dy;
    let front = step(t, 0.) * slope_valid;
    let hit = ray_origin + ray_dir * t;

    // 三级网格线：1 单位细线 / 10 单位粗线 / 坐标轴（轴色与变换游标一致）
    let minor = max(grid_line(hit->x, vec1!(1.)), grid_line(hit->z, vec1!(1.)));
    let major = max(grid_line(hit->x * 0.1, vec1!(1.4)), grid_line(hit->z * 0.1, vec1!(1.4)));
    let axis_x = grid_line(hit->z, vec1!(1.8));
    let axis_z = grid_line(hit->x, vec1!(1.8));

    // 随摄像机距离衰减（细线先消失）
    let dist = distance(hit, JE_GRID_CAM_POS);
    let fade_near = clamp(1. - dist / 150., 0., 1.);
    let fade_far = clamp(1. - dist / 600., 0., 1.);
    let smooth_near = fade_near * fade_near * (3. - 2. * fade_near);
    let smooth_far = fade_far * fade_far * (3. - 2. * fade_far);

    let MINOR_LINE_COLOR = vec3!(0.42, 0.45, 0.50);
    let MAJOR_LINE_COLOR = vec3!(0.62, 0.65, 0.70);
    let AXIS_X_COLOR = vec3!(0.92, 0.32, 0.32);
    let AXIS_Z_COLOR = vec3!(0.38, 0.56, 0.98);

    let rgb_minor = MINOR_LINE_COLOR
        + (MAJOR_LINE_COLOR - MINOR_LINE_COLOR) * major;
    let rgb_major = rgb_minor + (AXIS_X_COLOR - rgb_minor) * axis_x;
    let rgb_final = rgb_major + (AXIS_Z_COLOR - rgb_major) * axis_z;

    let alpha = max(
        minor * 0.22 * smooth_near,
        max(
            major * 0.35 * smooth_far,
            max(axis_x, axis_z) * 0.65 * smooth_far))
        * front;

    return fout{
        color = vec4!(rgb_final, alpha),
    };
}
