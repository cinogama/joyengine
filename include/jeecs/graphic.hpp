#pragma once

// ============================================================================
// jeecs/graphic.hpp —— 图形：je_gl_* / je_graphic_* / je_font_* / je_image_texture_* C API
// （数据类型、后端虚表、线程/资源/字体/渲染 API V2/uhost、图片纹理缓存）与 jeecs::graphic
// C++ 封装（纹理/着色器/顶点/帧缓冲/一致缓冲、graphic::animation、character/font、
// BasePipelineInterface、graphic_syncer_host）
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/file.hpp"
#include "jeecs/core.hpp"
#include "jeecs/game.hpp"
#include "jeecs/math.hpp"
#include "jeecs/animation.hpp"
#include "jeecs/basic.hpp"

// ---- C ABI 不透明句柄（opaque handle）前向声明 ----
// 这些类型对外仅暴露指针；完整定义在引擎内部（src/）。C/C++ 通用，二进制兼容。
// 注：je_game_universe_t/je_game_world_t/je_archtype_t/je_chunk_t 的前向声明位于 jeecs/ecs.hpp
// （je_game_entity_t 等早期定义的类型需要引用 je_chunk_t）。
typedef struct je_graphic_uhost_t    je_graphic_uhost_t;      // 图形渲染宿主上下文
typedef struct je_render_branch_t je_render_branch_t;   // 可编程绘制分支

#ifdef __cplusplus
namespace jeecs
{
    // 注：je_render_branch_t / je_graphic_uhost_t 不再在此提供 C++ 别名。
    // C++ 代码直接使用 C ABI 类型 je_render_branch_t / je_graphic_uhost_t（见文件顶部 C ABI 段）。
    /*
    jeecs::graphic [命名空间]
    此处定义引擎图形库相关资源的封装类型和工具
    */
    namespace graphic
    {
        struct character;
        /*
        jeecs::graphic::EDITOR_GIZMO_BRANCH_QUEUE [常量]
        用于指示默认的编辑器Gizmo 绘制集合的优先级，通常情况下没有特别的作用
        */
        inline constexpr int EDITOR_GIZMO_BRANCH_QUEUE = 50000;

        /*
        jeecs::graphic::GRAPHIC_UPDATE_BRANCH_QUEUE [常量]
        资源更新链使用的优先级：先于所有绘制链（render queue）提交，保证本帧的绘制
        看到的是本帧录制的瞬态内容（见 je_graphic_branch_new_update_command）
        */
        inline constexpr int GRAPHIC_UPDATE_BRANCH_QUEUE = INT_MIN;

        /*
        jeecs::graphic::INVALID_UNIFORM_LOCATION [常量]
        图形库的一致变量位置信息无效值，通常用于指示着色器中不存在指定名称的一致变量
        请参见：
            je_gl_shader_t
        */
        constexpr uint32_t INVALID_UNIFORM_LOCATION = (uint32_t)-1;

        /*
        jeecs::graphic::PENDING_UNIFORM_LOCATION [常量]
        图形库的一致变量位置信息初始值，由于此信息需要在对应着色器编译完成后才能确定，
        因此此值暗示当前一致变量位置尚未确定，可通过此值确认shader状态，但不推荐
        请参见：
            je_gl_shader_t
        */
        constexpr uint32_t PENDING_UNIFORM_LOCATION = (uint32_t)-2;
    }
}
#endif // __cplusplus

JE_FORCE_CAPI

/////////////////////////// GRAPHIC //////////////////////////////
// Here to store low-level-graphic-api.

/*
je_gl_display_mode_t [枚举]
图形接口窗口的显示模式
*/
typedef enum je_gl_display_mode_t
{
    JE_GL_DISPLAY_MODE_WINDOWED,
    JE_GL_DISPLAY_MODE_FULLSCREEN,
    JE_GL_DISPLAY_MODE_BOARDLESS,
} je_gl_display_mode_t;

/*
je_gl_interface_config_t [类型]
图形接口的配置类型，创建图形线程时传入并生效
*/
typedef struct je_gl_interface_config_t
{
    // 显示模式，指示图形应该在全屏、窗口或无边框窗口中显示
    // * 在一些平台上无效
    je_gl_display_mode_t m_display_mode;

    // 是否允许用户调整窗口大小
    // * 在一些平台上无效
    bool m_enable_resize;

    // 若MSAA值为0，则说明关闭超采样抗锯齿
    //  * MSAA配置应该是2的整数次幂
    //  * 最终能否使用取决于图形库
    size_t m_msaa;

    // 启动时的窗口尺寸，即绘制区域的大小，如果设置为0，则使用默认可用的绘制空间大小
    // * 如需全屏，通常设置为 0 即可。
    // * 在一些平台上无效
    size_t m_width;
    size_t m_height;

    // 限制帧数，若指定为0，则启用垂直同步
    // 不限制帧率请设置为 SIZE_MAX
    size_t m_fps;

    // 窗口标题，不可为 nullptr
    const char* m_title;

    // 用户数据，针对一些特殊的平台，可能需要通过此参数传递一些特定的参数
    // * 不同图形库可能以不同的方式使用此参数，请根据额外约定使用。
    void* m_userdata;
} je_gl_interface_config_t;

typedef struct je_gl_context_notifier_t je_gl_context_notifier_t;
typedef struct je_gl_graphic_api_t je_gl_graphic_api_t;
typedef struct je_gl_context_promise_t je_gl_context_promise_t;

/*
je_gl_update_action_t [类型]
图形后端的帧更新接口返回的渲染动作，也会作为参数传给帧渲染任务
    * JE_GL_UPDATE_CONTINUE 表示继续正常渲染
    * JE_GL_UPDATE_SKIP 表示跳过本帧的渲染
    * JE_GL_UPDATE_STOP 表示请求关闭渲染
*/
typedef enum je_gl_update_action_t
{
    JE_GL_UPDATE_CONTINUE,
    JE_GL_UPDATE_SKIP,
    JE_GL_UPDATE_STOP,
} je_gl_update_action_t;

typedef struct je_gl_context_t je_gl_context_t;

/*
je_gl_frame_job_func_t [类型]
图形线程每帧调用的帧渲染任务函数类型
*/
typedef void (*je_gl_frame_job_func_t)(je_gl_context_t*, void*, je_gl_update_action_t);

/*
je_gl_context_t [类型]
图形上下文，储存有当前图形线程的各项信息
*/
// 图形实现上下文指针（提升到文件作用域，供 je_gl_graphic_api_t 的函数指针别名直接使用，避免 C++ 的 :: 作用域解析）。
typedef void* je_graphic_impl_context_t;
struct je_gl_context_t
{
    je_gl_frame_job_func_t _m_frame_rend_work;
    void* _m_frame_rend_work_arg;
    void* _m_sync_callback_arg;

    je_gl_context_notifier_t* _m_thread_notifier;
    void* _m_interface_handle;

    je_game_universe_t* m_universe_instance;
    je_version_t m_version;
    je_gl_interface_config_t m_config;
    je_gl_graphic_api_t* m_apis;
    je_graphic_impl_context_t m_graphic_impl_context;
};

typedef void* je_gl_resource_blob_t;
typedef struct je_gl_resource_bind_counter_t je_gl_resource_bind_counter_t;

/*
je_gl_resource_handle_t [类型]
图形资源句柄，储存有资源的路径、底层图形实现指针和引用计数等信息
*/
typedef struct je_gl_resource_handle_t
{
    const char* m_path_may_null_if_builtin;
    je_gl_resource_bind_counter_t* m_raw_ref_count;

    je_gl_context_t* m_graphic_thread;
    je_version_t m_graphic_thread_version;

    bool m_modified;
    void* m_ptr;
} je_gl_resource_handle_t;

/*
je_gl_texture_kind_t [类型]
纹理类别标签，用于在类型被擦除的边界（绘制链重放、脚本语言包装层）区分纹理的实际类别
*/
enum class je_gl_texture_kind_t : uint8_t
{
    TEXTURE_2D,
    TEXTURE_3D,
    TEXTURE_ARRAY,
    TEXTURE_CUBE,
    TEXTURE_MSAA,
};

/*
je_gl_texture_pixel_data_t [类型别名]
纹理像素数据的存储单元类型
*/
typedef uint8_t je_gl_texture_pixel_data_t;

/*
je_gl_texture_format_t [枚举]
纹理像素格式；低位表示通道布局，其余位是附加标志
*/
typedef enum je_gl_texture_format_t : uint16_t
{
    JE_GL_TEXTURE_FORMAT_MONO = 0x0001,
    JE_GL_TEXTURE_FORMAT_RGBA = 0x0004,
    JE_GL_TEXTURE_FORMAT_COLOR_DEPTH_MASK = 0x000F,

    JE_GL_TEXTURE_FORMAT_FLOAT16 = 0x0010,
    JE_GL_TEXTURE_FORMAT_DEPTH = 0x0020,
    JE_GL_TEXTURE_FORMAT_FRAMEBUF = 0x0040,
    // Sample RGB through the sRGB transfer function (hardware decode).
    // Pixel storage is unchanged; alpha is always linear.
    JE_GL_TEXTURE_FORMAT_SRGB = 0x0080,

    // Formats that do not keep a CPU-side pixel buffer.
    JE_GL_TEXTURE_FORMAT_NON_PIXEL_FORMAT_MASK =
        JE_GL_TEXTURE_FORMAT_FLOAT16 | JE_GL_TEXTURE_FORMAT_DEPTH | JE_GL_TEXTURE_FORMAT_FRAMEBUF,
} je_gl_texture_format_t;

/*
je_gl_texture_common_t [类型]
各类纹理的公共前缀布局，仅供其余纹理类型继承，不应被单独使用。
所有纹理类型必须保证 m_handle 位于结构体偏移 0 处，以复用引擎的资源生命周期管理。
*/
typedef struct je_gl_texture_common_t
{
    // NOTE:
    // * Pixel data is storage from LEFT/BOTTOM to RIGHT/TOP
    // * m_initial_pixels 是创建期像素快照：只在后端首次初始化（以及图形线程
    //   重建）时读取，创建完成后不再允许任何改写。运行时修改纹理只能通过
    //   je_graphic_cmd_upload_texture_* 录制区段上传。
    // * If texture's m_initial_pixels is nullptr, only create a texture in pipeline.
    je_gl_resource_handle_t m_handle;
    je_gl_texture_kind_t m_kind;

    je_gl_texture_pixel_data_t* m_initial_pixels;
    size_t m_width;
    size_t m_height;
    je_gl_texture_format_t m_format;
} je_gl_texture_common_t;

/*
je_gl_texture_2d_t [类型]
二维纹理原始数据，储存有纹理的采样方式和像素数据等信息
*/
struct je_gl_texture_2d_t : public je_gl_texture_common_t
{
    static constexpr je_gl_texture_kind_t KIND = je_gl_texture_kind_t::TEXTURE_2D;
};
static_assert(
    std::is_trivial_v<je_gl_texture_2d_t> && 0 == offsetof(je_gl_texture_2d_t, m_handle));

/*
je_gl_texture_3d_t [类型]
三维纹理原始数据，m_depth 指示纹理在深度方向上的纹素数量，像素数据按
宽度 x 高度 x 深度 连续排布
*/
struct je_gl_texture_3d_t : public je_gl_texture_common_t
{
    static constexpr je_gl_texture_kind_t KIND = je_gl_texture_kind_t::TEXTURE_3D;
    size_t m_depth;
};
static_assert(
    std::is_trivial_v<je_gl_texture_3d_t> && 0 == offsetof(je_gl_texture_3d_t, m_handle));

/*
je_gl_texture_array_t [类型]
纹理数组原始数据，m_layers 指示数组中二维纹理层的数量，像素数据按
宽度 x 高度 x 层数 连续排布
*/
struct je_gl_texture_array_t : public je_gl_texture_common_t
{
    static constexpr je_gl_texture_kind_t KIND = je_gl_texture_kind_t::TEXTURE_ARRAY;
    size_t m_layers;
};
static_assert(
    std::is_trivial_v<je_gl_texture_array_t> && 0 == offsetof(je_gl_texture_array_t, m_handle));

/*
je_gl_texture_cube_t [类型]
立方体贴图纹理原始数据，固定包含 6 个宽度高度相同的正方形面，
像素数据按 6 x 宽度 x 高度 连续排布（面的顺序与图形 API 的
+CUBE_MAP_POSITIVE_X .. -Z 顺序一致）
*/
struct je_gl_texture_cube_t : public je_gl_texture_common_t
{
    static constexpr je_gl_texture_kind_t KIND = je_gl_texture_kind_t::TEXTURE_CUBE;
};
static_assert(
    std::is_trivial_v<je_gl_texture_cube_t> && 0 == offsetof(je_gl_texture_cube_t, m_handle));

/*
je_gl_texture_msaa_t [类型]
多重采样二维纹理原始数据，仅存在于 GPU 侧（渲染目标附件），不持有 CPU 像素数据，
m_samples 指示每像素采样数
*/
struct je_gl_texture_msaa_t : public je_gl_texture_common_t
{
    static constexpr je_gl_texture_kind_t KIND = je_gl_texture_kind_t::TEXTURE_MSAA;
    size_t m_samples;
};
static_assert(
    std::is_trivial_v<je_gl_texture_msaa_t> && 0 == offsetof(je_gl_texture_msaa_t, m_handle));

/*
je_gl_vertex_type_t [枚举]
顶点的图元绘制方式
*/
typedef enum je_gl_vertex_type_t
{
    JE_GL_VERTEX_TYPE_LINESTRIP = 0,
    JE_GL_VERTEX_TYPE_TRIANGLES,
    JE_GL_VERTEX_TYPE_TRIANGLESTRIP,

    JE_GL_VERTEX_TYPE_COUNT,
} je_gl_vertex_type_t;

/*
je_gl_vertex_data_type_t [枚举]
顶点属性分量的数据类型
*/
typedef enum je_gl_vertex_data_type_t
{
    JE_GL_VERTEX_DATA_TYPE_INT32 = 0,
    JE_GL_VERTEX_DATA_TYPE_FLOAT32,
} je_gl_vertex_data_type_t;

/*
je_gl_vertex_data_layout_t [类型]
顶点属性布局的一项
*/
typedef struct je_gl_vertex_data_layout_t
{
    je_gl_vertex_data_type_t m_type;
    size_t m_count;
} je_gl_vertex_data_layout_t;

/*
je_gl_vertex_bone_data_t [类型]
顶点骨骼绑定数据
*/
typedef struct je_gl_vertex_bone_data_t
{
    const char* m_name;
    size_t m_index;
    float m_m2b_trans[4][4];
} je_gl_vertex_bone_data_t;

/*
je_gl_vertex_t [类型]
顶点原始数据，储存有顶点的绘制方式、格式、大小和顶点数据等信息
*/
typedef struct je_gl_vertex_t
{
    je_gl_resource_handle_t m_handle;

    float m_x_min, m_x_max,
        m_y_min, m_y_max,
        m_z_min, m_z_max;

    // 创建期顶点/索引快照：只在后端首次初始化（以及图形线程重建）时读取；
    // 运行时修改顶点内容只能通过 je_graphic_cmd_update_vertex 录制本帧内容。
    const void* m_initial_vertexs;
    size_t m_vertex_length;
    const uint32_t* m_initial_indices;
    size_t m_index_count;
    // 索引缓冲的分配容量；m_index_count 是当前激活的绘制数量（不得超过容量）
    size_t m_index_capacity;
    const je_gl_vertex_data_layout_t* m_formats;
    size_t m_format_count;
    size_t m_data_size_per_point;
    je_gl_vertex_type_t m_type;
    const je_gl_vertex_bone_data_t** m_bones;
    size_t m_bone_count;
} je_gl_vertex_t;
static_assert(
    std::is_trivial_v<je_gl_vertex_t> && 0 == offsetof(je_gl_vertex_t, m_handle));

/*
je_gl_shader_filter_mode_t [枚举]
着色器默认采样器的过滤模式
*/
typedef enum je_gl_shader_filter_mode_t
{
    JE_GL_SHADER_FILTER_MODE_NEAREST,
    JE_GL_SHADER_FILTER_MODE_LINEAR,
} je_gl_shader_filter_mode_t;

/*
je_gl_shader_wrap_mode_t [枚举]
着色器默认采样器的寻址模式
*/
typedef enum je_gl_shader_wrap_mode_t
{
    JE_GL_SHADER_WRAP_MODE_CLAMP,
    JE_GL_SHADER_WRAP_MODE_REPEAT,
} je_gl_shader_wrap_mode_t;

/*
je_gl_shader_sampler_method_t [类型]
着色器声明的默认采样器参数
*/
typedef struct je_gl_shader_sampler_method_t
{
    je_gl_shader_filter_mode_t m_min;
    je_gl_shader_filter_mode_t m_mag;
    je_gl_shader_filter_mode_t m_mip;
    je_gl_shader_wrap_mode_t m_uwrap;
    je_gl_shader_wrap_mode_t m_vwrap;
    uint32_t m_sampler_id; // Used for DX11 & HLSL generation
    uint64_t m_pass_id_count;
    uint32_t* m_pass_ids; // Used for GL3 & GLSL generation
} je_gl_shader_sampler_method_t;

/*
je_gl_shader_uniform_type_t [枚举]
自定义 uniform 的存储类型
*/
typedef enum je_gl_shader_uniform_type_t
{
    JE_GL_SHADER_UNIFORM_TYPE_INT,
    JE_GL_SHADER_UNIFORM_TYPE_INT2,
    JE_GL_SHADER_UNIFORM_TYPE_INT3,
    JE_GL_SHADER_UNIFORM_TYPE_INT4,
    JE_GL_SHADER_UNIFORM_TYPE_FLOAT,
    JE_GL_SHADER_UNIFORM_TYPE_FLOAT2,
    JE_GL_SHADER_UNIFORM_TYPE_FLOAT3,
    JE_GL_SHADER_UNIFORM_TYPE_FLOAT4,
    JE_GL_SHADER_UNIFORM_TYPE_TEXTURE,
    JE_GL_SHADER_UNIFORM_TYPE_FLOAT2X2,
    JE_GL_SHADER_UNIFORM_TYPE_FLOAT3X3,
    JE_GL_SHADER_UNIFORM_TYPE_FLOAT4X4,
} je_gl_shader_uniform_type_t;

/*
自定义 uniform 的附加标签（位掩码）。标签不改变存储类型与 ABI：
例如 COLOR 仍然是 FLOAT4，仅告知编辑器/脚本层按颜色呈现。
后续新增的编辑器语义标签继续在这里追加。
*/
typedef enum je_gl_shader_uniform_mask_t : uint32_t
{
    JE_GL_SHADER_UNIFORM_MASK_NONE = 0x0000,
    JE_GL_SHADER_UNIFORM_MASK_COLOR = 0x0001,
} je_gl_shader_uniform_mask_t;

/*
Woolang `shader::uniform_variable` union 的 variant 索引（脚本 ABI）。
必须与 je/graphic.wo 中的 union 声明顺序一致。
*/
typedef enum je_gl_shader_script_uniform_variant_t
{
    JE_GL_SHADER_SCRIPT_UNIFORM_INTEGER = 0,
    JE_GL_SHADER_SCRIPT_UNIFORM_INTEGER2,
    JE_GL_SHADER_SCRIPT_UNIFORM_INTEGER3,
    JE_GL_SHADER_SCRIPT_UNIFORM_INTEGER4,
    JE_GL_SHADER_SCRIPT_UNIFORM_FLOAT,
    JE_GL_SHADER_SCRIPT_UNIFORM_FLOAT2,
    JE_GL_SHADER_SCRIPT_UNIFORM_FLOAT3,
    JE_GL_SHADER_SCRIPT_UNIFORM_FLOAT4,
    JE_GL_SHADER_SCRIPT_UNIFORM_TEXTURE,
    JE_GL_SHADER_SCRIPT_UNIFORM_COLOR,
    JE_GL_SHADER_SCRIPT_UNIFORM_OTHERS,
} je_gl_shader_script_uniform_variant_t;

/*
je_gl_shader_builtin_uniform_location_t [类型]
内建 uniform 的后端位置表
*/
typedef struct je_gl_shader_builtin_uniform_location_t
{
    // NOTE: m_builtin_uniform_ndc_scale 是图形库的保留变量，任何情况下都不要
    //      从外部修改它的值，它会在渲染时被图形库自动更新为正确的NDC缩放值
    uint32_t m_builtin_uniform_ndc_scale;

    uint32_t m_builtin_uniform_m;
    uint32_t m_builtin_uniform_mv;
    uint32_t m_builtin_uniform_mvp;

    uint32_t m_builtin_uniform_local_scale;

    uint32_t m_builtin_uniform_tiling;
    uint32_t m_builtin_uniform_offset;

    uint32_t m_builtin_uniform_color;

    uint32_t m_builtin_uniform_light2d_resolution;
    uint32_t m_builtin_uniform_light2d_decay;

    // SpriteShadow 接触椭圆参数：x=长轴全长，y=短轴全长，
    // z=背光尾迹延伸长度，w=有效消隐距离（视图空间长度）
    uint32_t m_builtin_uniform_shadow_ellipse;
} je_gl_shader_builtin_uniform_location_t;

/*
je_gl_shader_uniform_value_t [联合]
单个 uniform 的值
*/
typedef union je_gl_shader_uniform_value_t
{
    int m_int;
    int m_int2[2];
    int m_int3[3];
    int m_int4[4];
    float m_float;
    float m_float2[2];
    float m_float3[3];
    float m_float4[4];
    float m_float2x2[2][2];
    float m_float3x3[3][3];
    float m_float4x4[4][4];
} je_gl_shader_uniform_value_t;
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_int));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_int2));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_int3));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_int4));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_float));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_float2));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_float3));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_float4));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_float2x2));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_float3x3));
static_assert(0 == offsetof(je_gl_shader_uniform_value_t, m_float4x4));

/*
je_gl_shader_uniform_variables_t [类型]
着色器自定义 uniform 的反射信息链表节点
*/
typedef struct je_gl_shader_uniform_variables_t
{
    const char* m_name;
    // Backend-resolved location, kept for the legacy immediate API.
    // PENDING_UNIFORM_LOCATION until the shader blob is initialized.
    uint32_t m_index;
    // Canonical std140 byte offset/size inside the shader uniform block.
    // Computed once when the shader source is generated (or loaded from
    // cache) and shared by every backend and the command-buffer recorder.
    uint32_t m_offset;
    uint32_t m_size;
    je_gl_shader_uniform_type_t m_uniform_type;
    je_gl_shader_uniform_mask_t m_uniform_mask;
    bool m_updated;

    je_gl_shader_uniform_variables_t* m_next;
    je_gl_shader_uniform_value_t m_value;
} je_gl_shader_uniform_variables_t;

/*
je_gl_shader_uniform_block_t [类型]
着色器 uniform block 的反射信息链表节点
*/
typedef struct je_gl_shader_uniform_block_t
{
    const char* m_name;
    uint32_t m_specify_binding_place;

    je_gl_shader_uniform_block_t* m_next;
} je_gl_shader_uniform_block_t;

/*
je_gl_shader_depth_test_method_t [枚举]
深度测试方法
*/
enum class je_gl_shader_depth_test_method_t : int8_t
{
    INVALID = -1,

    NEVER,
    LESS, /* DEFAULT */
    EQUAL,
    LESS_EQUAL,
    GREATER,
    NOT_EQUAL,
    GREATER_EQUAL,
    ALWAYS,
};

/*
je_gl_shader_depth_mask_method_t [枚举]
深度写入方法
*/
enum class je_gl_shader_depth_mask_method_t : int8_t
{
    INVALID = -1,

    DISABLE,
    ENABLE, /* DEFAULT */
};

/*
je_gl_shader_blend_equation_t [枚举]
混合方程
*/
enum class je_gl_shader_blend_equation_t : int8_t
{
    INVALID = -1,

    DISABLED, /* DEFAULT */
    ADD,
    SUBTRACT,
    REVERSE_SUBTRACT,
    MIN,
    MAX,
};

/*
je_gl_shader_blend_method_t [枚举]
混合因子
*/
enum class je_gl_shader_blend_method_t : int8_t
{
    INVALID = -1,

    ZERO, /* DEFAULT SRC = ONE, DST = ZERO */
    ONE,

    SRC_COLOR,
    SRC_ALPHA,

    ONE_MINUS_SRC_ALPHA,
    ONE_MINUS_SRC_COLOR,

    DST_COLOR,
    DST_ALPHA,

    ONE_MINUS_DST_ALPHA,
    ONE_MINUS_DST_COLOR,

    // CONST_COLOR,
    // ONE_MINUS_CONST_COLOR,

    // CONST_ALPHA,
    // ONE_MINUS_CONST_ALPHA,
};

/*
je_gl_shader_cull_mode_t [枚举]
面剔除模式
*/
enum class je_gl_shader_cull_mode_t : int8_t
{
    INVALID = -1,

    NONE, /* DEFAULT */
    FRONT,
    BACK,
};

/*
je_gl_shader_spirv_code_t [类型别名]
SPIR-V 代码字
*/
typedef uint32_t je_gl_shader_spirv_code_t;

/*
je_gl_shader_texture_decl_t [类型]
着色器声明的纹理绑定
*/
typedef struct je_gl_shader_texture_decl_t
{
    const char* m_name;
    uint32_t m_pass;
    uint32_t m_sampler_id;
    je_gl_texture_kind_t m_kind;
} je_gl_shader_texture_decl_t;

/*
je_gl_shader_t [类型]
着色器原始数据，储存有着色器源码、绘制参数和一致变量等信息
*/
typedef struct je_gl_shader_t
{
    je_gl_resource_handle_t m_handle;

    const char* m_vertex_hlsl_src;
    const char* m_fragment_hlsl_src;
    const char* m_vertex_glsl_src;
    const char* m_fragment_glsl_src;
    const char* m_vertex_glsles_src;
    const char* m_fragment_glsles_src;
    const char* m_vertex_msl_mac_src;
    const char* m_fragment_msl_mac_src;

    size_t m_vertex_spirv_count;
    const je_gl_shader_spirv_code_t* m_vertex_spirv_codes;

    size_t m_fragment_spirv_count;
    const je_gl_shader_spirv_code_t* m_fragment_spirv_codes;

    size_t m_vertex_in_count;
    je_gl_shader_uniform_type_t* m_vertex_in;

    size_t m_fragment_out_count;
    je_gl_shader_uniform_type_t* m_fragment_out;

    je_gl_shader_uniform_variables_t* m_custom_uniforms;
    je_gl_shader_uniform_block_t* m_custom_uniform_blocks;

    // Canonical std140-aligned size of the shader's per-draw uniform block.
    uint32_t m_uniform_block_size;

    // Texture bindings declared by the shader (reflection for the render API).
    je_gl_shader_texture_decl_t* m_textures;
    size_t m_texture_count;

    je_gl_shader_builtin_uniform_location_t m_builtin_uniforms;

    bool m_enable_to_shared;
    je_gl_shader_depth_test_method_t m_depth_test;
    je_gl_shader_depth_mask_method_t m_depth_mask;
    je_gl_shader_blend_equation_t m_blend_equation;
    je_gl_shader_blend_method_t m_blend_src_mode, m_blend_dst_mode;
    je_gl_shader_cull_mode_t m_cull_mode;

    je_gl_shader_sampler_method_t* m_sampler_methods;
    size_t m_sampler_count;
} je_gl_shader_t;
static_assert(
    std::is_trivial_v<je_gl_shader_t> && 0 == offsetof(je_gl_shader_t, m_handle));

/*
je_gl_frame_buffer_attachment_t [类型]
帧缓冲区附件；不透明类型，引擎内部以 resource<texture2d> 持有
*/
struct je_gl_frame_buffer_attachment_t;
typedef struct je_gl_frame_buffer_attachment_t je_gl_frame_buffer_attachment_t;

/*
je_gl_frame_buffer_t [类型]
帧缓冲区原始数据，储存有帧大小、附件等信息
*/
typedef struct je_gl_frame_buffer_t
{
    je_gl_resource_handle_t m_handle;

    // In fact, je_gl_frame_buffer_attachment_t is jeecs::basic::resource<jeecs::graphic::texture2d>
    je_gl_frame_buffer_attachment_t* m_output_attachments;
    size_t m_attachment_count;
    size_t m_width;
    size_t m_height;
} je_gl_frame_buffer_t;
static_assert(
    std::is_trivial_v<je_gl_frame_buffer_t> && 0 == offsetof(je_gl_frame_buffer_t, m_handle));

/*
je_gl_uniform_buffer_t [类型]
一致变量缓冲区原始数据，储存有一致变量的数据和容量等信息
*/
typedef struct je_gl_uniform_buffer_t
{
    je_gl_resource_handle_t m_handle;

    size_t m_buffer_binding_place;
    size_t m_buffer_size;
    // 后端首次初始化（以及图形线程重建）时可能读取的内容缓冲。它的内容、以及
    // 后端是否读取它，都不作为接口保证：一致变量缓冲区创建后不保证初始化，
    // 内容必须由 je_graphic_cmd_update_uniform_buffer 按帧录制完整数据。
    uint8_t* m_initial_buffer;
} je_gl_uniform_buffer_t;
static_assert(
    std::is_trivial_v<je_gl_uniform_buffer_t> && 0 == offsetof(je_gl_uniform_buffer_t, m_handle));

/*
je_gl_frame_buffer_clear_type_t [枚举]
帧缓冲区清除目标类别
*/
enum class je_gl_frame_buffer_clear_type_t
{
    COLOR,
    DEPTH,
};

/*
je_gl_frame_buffer_clear_color_attachment_t [类型]
清除指定颜色附件
*/
typedef struct je_gl_frame_buffer_clear_color_attachment_t
{
    size_t m_color_attachment_idx;
    float m_clear_color_rgba[4];
} je_gl_frame_buffer_clear_color_attachment_t;

/*
je_gl_frame_buffer_clear_depth_attachment_t [类型]
清除深度附件
*/
typedef struct je_gl_frame_buffer_clear_depth_attachment_t
{
    float m_clear_depth;
} je_gl_frame_buffer_clear_depth_attachment_t;

/*
je_gl_frame_buffer_clear_operation_t [类型]
帧缓冲区清除操作链表，用于指定清除哪些附件以及清除为何值
*/
typedef struct je_gl_frame_buffer_clear_operation_t
{
    je_gl_frame_buffer_clear_type_t m_type;
    union
    {
        je_gl_frame_buffer_clear_color_attachment_t m_color;
        je_gl_frame_buffer_clear_depth_attachment_t m_depth;
    };
    const je_gl_frame_buffer_clear_operation_t* m_next;
} je_gl_frame_buffer_clear_operation_t;

/*
je_gl_pipeline_t / je_gl_sampler_t / je_gl_command_buffer_t [类型]
Render API (v2) 的不透明对象句柄；具体定义由引擎内部持有，
外部只能通过对应的 `je_graphic_*` 接口创建、使用和关闭
    * je_gl_command_buffer_t 的录制与提交规则见其接口说明
*/
typedef struct je_gl_pipeline_t je_gl_pipeline_t;
typedef struct je_gl_sampler_t je_gl_sampler_t;
typedef struct je_gl_command_buffer_t je_gl_command_buffer_t;

/*
je_gl_viewport_t [类型]
渲染 pass 的视口范围，单位为像素
    * width 与 height 必须大于 0
*/
typedef struct je_gl_viewport_t
{
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
} je_gl_viewport_t;

// 图形基本启动和关闭接口
typedef je_graphic_impl_context_t(*je_gl_graphic_api_startup_func_t)(je_gl_context_t*, const je_gl_interface_config_t*, bool);
typedef void (*je_gl_graphic_api_shutdown_func_t)(je_gl_context_t*, je_graphic_impl_context_t, bool);

typedef je_gl_update_action_t(*je_gl_graphic_api_update_func_t)(je_graphic_impl_context_t);
typedef je_gl_update_action_t(*je_gl_graphic_api_commit_func_t)(je_graphic_impl_context_t, je_gl_update_action_t);

// 资源创建相关接口
typedef je_gl_resource_blob_t(*je_gl_graphic_api_shader_create_blob_func_t)(je_graphic_impl_context_t, je_gl_shader_t*);
typedef void (*je_gl_graphic_api_shader_close_blob_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t);
typedef void (*je_gl_graphic_api_shader_init_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_shader_t*);
typedef void (*je_gl_graphic_api_shader_update_func_t)(je_graphic_impl_context_t, je_gl_shader_t*);
typedef void (*je_gl_graphic_api_shader_close_func_t)(je_graphic_impl_context_t, je_gl_shader_t*);

// 注意：这些函数指针类型位于 extern "C" 区域内，不允许使用模板别名，
// 因此各类纹理的函数指针类型均为显式展开
typedef je_gl_resource_blob_t(*je_gl_graphic_api_texture_2d_create_blob_func_t)(je_graphic_impl_context_t, je_gl_texture_2d_t*);
typedef je_gl_resource_blob_t(*je_gl_graphic_api_texture_3d_create_blob_func_t)(je_graphic_impl_context_t, je_gl_texture_3d_t*);
typedef je_gl_resource_blob_t(*je_gl_graphic_api_texture_array_create_blob_func_t)(je_graphic_impl_context_t, je_gl_texture_array_t*);
typedef je_gl_resource_blob_t(*je_gl_graphic_api_texture_cube_create_blob_func_t)(je_graphic_impl_context_t, je_gl_texture_cube_t*);
typedef je_gl_resource_blob_t(*je_gl_graphic_api_texture_msaa_create_blob_func_t)(je_graphic_impl_context_t, je_gl_texture_msaa_t*);
typedef void (*je_gl_graphic_api_texture_close_blob_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t);
typedef void (*je_gl_graphic_api_texture_2d_init_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_2d_t*);
typedef void (*je_gl_graphic_api_texture_3d_init_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_3d_t*);
typedef void (*je_gl_graphic_api_texture_array_init_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_array_t*);
typedef void (*je_gl_graphic_api_texture_cube_init_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_cube_t*);
typedef void (*je_gl_graphic_api_texture_msaa_init_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_texture_msaa_t*);
typedef void (*je_gl_graphic_api_texture_2d_upload_func_t)(
    je_graphic_impl_context_t, je_gl_texture_2d_t*,
    size_t x, size_t y, size_t width, size_t height,
    const je_gl_texture_pixel_data_t* pixels);
typedef void (*je_gl_graphic_api_texture_3d_upload_func_t)(
    je_graphic_impl_context_t, je_gl_texture_3d_t*,
    size_t x, size_t y, size_t z, size_t width, size_t height, size_t depth,
    const je_gl_texture_pixel_data_t* pixels);
typedef void (*je_gl_graphic_api_texture_array_upload_func_t)(
    je_graphic_impl_context_t, je_gl_texture_array_t*,
    size_t x, size_t y, size_t layer, size_t width, size_t height, size_t layers,
    const je_gl_texture_pixel_data_t* pixels);
typedef void (*je_gl_graphic_api_texture_cube_upload_func_t)(
    je_graphic_impl_context_t, je_gl_texture_cube_t*,
    size_t face, size_t x, size_t y, size_t width, size_t height,
    const je_gl_texture_pixel_data_t* pixels);
typedef void (*je_gl_graphic_api_texture_2d_close_func_t)(je_graphic_impl_context_t, je_gl_texture_2d_t*);
typedef void (*je_gl_graphic_api_texture_3d_close_func_t)(je_graphic_impl_context_t, je_gl_texture_3d_t*);
typedef void (*je_gl_graphic_api_texture_array_close_func_t)(je_graphic_impl_context_t, je_gl_texture_array_t*);
typedef void (*je_gl_graphic_api_texture_cube_close_func_t)(je_graphic_impl_context_t, je_gl_texture_cube_t*);
typedef void (*je_gl_graphic_api_texture_msaa_close_func_t)(je_graphic_impl_context_t, je_gl_texture_msaa_t*);

typedef je_gl_resource_blob_t(*je_gl_graphic_api_vertex_create_blob_func_t)(je_graphic_impl_context_t, je_gl_vertex_t*);
typedef void (*je_gl_graphic_api_vertex_close_blob_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t);
typedef void (*je_gl_graphic_api_vertex_init_func_t)(je_graphic_impl_context_t, je_gl_resource_blob_t, je_gl_vertex_t*);
typedef void (*je_gl_graphic_api_vertex_update_func_t)(
    je_graphic_impl_context_t, je_gl_vertex_t*,
    const void* vertices, size_t vertex_length);
typedef void (*je_gl_graphic_api_vertex_close_func_t)(je_graphic_impl_context_t, je_gl_vertex_t*);

typedef void (*je_gl_graphic_api_framebuffer_init_func_t)(je_graphic_impl_context_t, je_gl_frame_buffer_t*);
typedef void (*je_gl_graphic_api_framebuffer_update_func_t)(je_graphic_impl_context_t, je_gl_frame_buffer_t*);
typedef void (*je_gl_graphic_api_framebuffer_close_func_t)(je_graphic_impl_context_t, je_gl_frame_buffer_t*);

typedef void (*je_gl_graphic_api_ubuffer_init_func_t)(je_graphic_impl_context_t, je_gl_uniform_buffer_t*);
typedef void (*je_gl_graphic_api_ubuffer_update_func_t)(
    je_graphic_impl_context_t, je_gl_uniform_buffer_t*,
    const void* data, size_t data_length);
typedef void (*je_gl_graphic_api_ubuffer_close_func_t)(je_graphic_impl_context_t, je_gl_uniform_buffer_t*);

// 渲染接口（pipeline / sampler / render pass / draw）相关接口
typedef void (*je_gl_graphic_api_pipeline_init_func_t)(je_graphic_impl_context_t, je_gl_pipeline_t*);
typedef void (*je_gl_graphic_api_pipeline_close_func_t)(je_graphic_impl_context_t, je_gl_pipeline_t*);
typedef void (*je_gl_graphic_api_pipeline_bind_func_t)(je_graphic_impl_context_t, je_gl_pipeline_t*);

typedef void (*je_gl_graphic_api_sampler_init_func_t)(je_graphic_impl_context_t, je_gl_sampler_t*);
typedef void (*je_gl_graphic_api_sampler_close_func_t)(je_graphic_impl_context_t, je_gl_sampler_t*);
typedef void (*je_gl_graphic_api_sampler_bind_func_t)(je_graphic_impl_context_t, uint32_t, je_gl_sampler_t*);

typedef void (*je_gl_graphic_api_begin_pass_func_t)(je_graphic_impl_context_t, je_gl_frame_buffer_t*, const je_gl_viewport_t*);
typedef void (*je_gl_graphic_api_clear_color_func_t)(je_graphic_impl_context_t, uint32_t, const float*);
typedef void (*je_gl_graphic_api_clear_depth_func_t)(je_graphic_impl_context_t, float);
typedef void (*je_gl_graphic_api_end_pass_func_t)(je_graphic_impl_context_t);

typedef void (*je_gl_graphic_api_bind_vertex_func_t)(je_graphic_impl_context_t, je_gl_vertex_t*);
typedef void (*je_gl_graphic_api_bind_uniform_buffer_func_t)(je_graphic_impl_context_t, je_gl_uniform_buffer_t*);
typedef void (*je_gl_graphic_api_bind_texture_2d_func_t)(je_graphic_impl_context_t, uint32_t, je_gl_texture_2d_t*);
typedef void (*je_gl_graphic_api_bind_texture_3d_func_t)(je_graphic_impl_context_t, uint32_t, je_gl_texture_3d_t*);
typedef void (*je_gl_graphic_api_bind_texture_array_func_t)(je_graphic_impl_context_t, uint32_t, je_gl_texture_array_t*);
typedef void (*je_gl_graphic_api_bind_texture_cube_func_t)(je_graphic_impl_context_t, uint32_t, je_gl_texture_cube_t*);

typedef void (*je_gl_graphic_api_draw_indexed_func_t)(je_graphic_impl_context_t, uint32_t, uint32_t, uint32_t, const void*, uint32_t);
typedef void (*je_gl_graphic_api_draw_func_t)(je_graphic_impl_context_t, uint32_t, uint32_t, uint32_t, const void*, uint32_t);

typedef void (*je_gl_graphic_api_commit_begin_func_t)(je_graphic_impl_context_t);
typedef void (*je_gl_graphic_api_commit_end_func_t)(je_graphic_impl_context_t);

/*
je_gl_graphic_api_t [类型]
图形接口，包含底层使用的各项基本函数
*/
struct je_gl_graphic_api_t
{
    /*
    je_gl_graphic_api_t::interface_startup [成员]
    图形接口在初始化或者被请求重新启动时调用，按照JoyEngine的约定，图形实现应当在此处创建窗口，并完成渲染所需
    的初始化工作。
        * 请求重新启动通常是因为需要变更图形设置、例如MSAA设置或窗口标题、帧率等，因此建议图形实现在处理重新启动
            时，应当保留可以保留的资源以节约开销；若不提供对此的优化支持，也可以直接忽略，按照默认的启停规则进行操作。
        * 当图形上下文被请求重新启动时，会再次调用此函数，此时第3个参数将被设置为true，否则为 false 。
    */
    je_gl_graphic_api_startup_func_t interface_startup;

    /*
    je_gl_graphic_api_t::interface_shutdown_before_resource_release [成员]
    图形接口在被关闭或请求重新启动时调用，按照JoyEngine的约定，所有申请而暂未释放的图形资源将在调用此函数完成之后
    执行释放，然后调用 interface_shutdown；因此图形实现应当在此做好释放图形资源的准备，例如让渲染设备进入暂停状态。
        * 请求重新启动通常是因为需要变更图形设置、例如MSAA设置或窗口标题、帧率等，因此建议图形实现在处理重新启动
            时，应当保留可以保留的资源以节约开销；若不提供对此的优化支持，也可以直接忽略，按照默认的启停规则进行操作。
        * 当图形上下文被请求重新启动时，第3个参数将被设置为true，否则为 false。
    请参见：
        je_gl_graphic_api_t::interface_post_shutdown
    */
    je_gl_graphic_api_shutdown_func_t interface_shutdown_before_resource_release;

    /*
    je_gl_graphic_api_t::interface_shutdown [成员]
    图形接口在被关闭或请求重新启动时调用，按照JoyEngine的约定，图形接口应当在此函数中完成所有资源、设备和窗口的释放。
        * 此接口将在图形接口被请求关闭之后，在完成调用interface_shutdown_before_resource_release接口、进而释放全部
            未释放的图形资源，最后调用interface_shutdown。
        * 请求重新启动通常是因为需要变更图形设置、例如MSAA设置或窗口标题、帧率等，因此建议图形实现在处理重新启动
            时，应当保留可以保留的资源以节约开销；若不提供对此的优化支持，也可以直接忽略，按照默认的启停规则进行操作。
        * 当图形上下文被请求重新启动时，第3个参数将被设置为true，否则为 false。
    请参见：
        je_gl_graphic_api_t::interface_pre_shutdown
    */
    je_gl_graphic_api_shutdown_func_t interface_shutdown;

    /*
    je_gl_graphic_api_t::update_frame_ready [成员]
    图形接口在更新一帧画面的起始位置会调用的接口，图形实现应当在此接口的开头完成前一帧画面的交换以呈现在屏幕上（这样可
    以获得最佳的画面流畅度），然后做好其他准备：正式的主要渲染任务会在此接口返回后开始。
        * 接口若返回 je_gl_update_action_t::STOP，则表示图形实现请求关闭渲染
            创建图形线程时指示的绘制任务将被跳过，在帧同步工作完成后进入图形线程的退出流程。
        * 接口若返回 je_gl_update_action_t::SKIP，则表示图形实现请求跳过本帧的渲染，
            通常这种情况是由于窗口被切换至后台、最小化等情况导致的，
            创建图形线程时指示的绘制任务将被跳过，但 **不会** 进入图形线程的退出流程。
        无论如何，update_draw_commit 都会被执行（目的是能正确执行 gui 的逻辑），update_draw_commit将通过参数
        获知渲染任务是否被执行
    */
    je_gl_graphic_api_update_func_t update_frame_ready;

    /*
    je_gl_graphic_api_t::update_draw_commit [成员]
    图形接口在完成指示的渲染操作之后会调用的接口，图形实现应当在此接口中，将既存的提交任务提交到图形设备中（以最大化利用
    设备空闲），如果可以，渲染GUI的任务也应当在此处实现，然后一并提交。
        * 接口若返回 je_gl_update_action_t::STOP，则表示图形实现请求关闭渲染，在帧同步工作完成后进入图形线程的退出流程。
        * 接口若返回 je_gl_update_action_t::SKIP，由于并没有后续的渲染任务可被跳过，因此事实上如同返回 je_gl_update_action_t::CONTINUE。
    */
    je_gl_graphic_api_commit_func_t update_draw_commit;

    /*
    je_gl_graphic_api_t::shader_create_blob [成员]
    图形接口创建着色器资源的底层数据块，je_graphic_shader_source_load 和 je_graphic_shader_load 在调用 shader_init 之前，会调用此接口创建 blob，
    对于具有相同路径的着色器资源，在先前创建的 blob 失效之前，不会重复调用此接口。
    图形实现可以利用 blob 加速着色器的创建流程。
        * 可以返回 nullptr，即便如此，在对应资源失效之前，此接口依然只被调用一次。
        * 如果接口返回 nullptr，shader_init 将收到一个 nullptr 作为 blob 参数。
        * 如果接口返回 nullptr，对应的 shader_close_blob 将不会被调用。
    */
    je_gl_graphic_api_shader_create_blob_func_t shader_create_blob;

    /*
    je_gl_graphic_api_t::texture_2d_create_blob / texture_3d_create_blob / texture_array_create_blob /
    texture_cube_create_blob / texture_msaa_create_blob [成员]
    图形接口创建对应类别纹理资源的底层数据块，je_graphic_texture_2d_load 和 je_graphic_texture_*_create 在调用对应的
    texture_*_init 之前，会调用此接口创建 blob，对于具有相同路径的纹理资源，在先前创建的 blob 失效之前，
    不会重复调用此接口。图形实现可以利用 blob 加速纹理的创建流程。
        * 可以返回 nullptr，即便如此，在对应资源失效之前，此接口依然只被调用一次。
        * 如果接口返回 nullptr，对应的 texture_*_init 将收到一个 nullptr 作为 blob 参数。
        * 如果接口返回 nullptr，对应的 texture_close_blob 将不会被调用。
        * 若图形实现不支持某一纹理类别，可以将对应的函数指针置空，引擎中间层将拒绝创建该类纹理并记录日志。
    */
    je_gl_graphic_api_texture_2d_create_blob_func_t texture_2d_create_blob;
    je_gl_graphic_api_texture_3d_create_blob_func_t texture_3d_create_blob;
    je_gl_graphic_api_texture_array_create_blob_func_t texture_array_create_blob;
    je_gl_graphic_api_texture_cube_create_blob_func_t texture_cube_create_blob;
    je_gl_graphic_api_texture_msaa_create_blob_func_t texture_msaa_create_blob;

    /*
    je_gl_graphic_api_t::vertex_create_blob [成员]
    图形接口创建顶点资源的底层数据块，je_graphic_vertex_load 在调用 vertex_init 之前，会调用此接口创建 blob，
    对于具有相同路径的顶点资源，在先前创建的 blob 失效之前，不会重复调用此接口。
    图形实现可以利用 blob 加速顶点数据的创建流程。
        * 可以返回 nullptr，即便如此，在对应资源失效之前，此接口依然只被调用一次。
        * 如果接口返回 nullptr，vertex_init 将收到一个 nullptr 作为 blob 参数。
        * 如果接口返回 nullptr，对应的 vertex_close_blob 将不会被调用。
    */
    je_gl_graphic_api_vertex_create_blob_func_t vertex_create_blob;

    /*
    je_gl_graphic_api_t::shader_close_blob [成员]
    图形接口关闭着色器资源的底层数据块，当一个资源失效或者图形上下文退出时，由引擎的中间层负责在适当的
    时机调用此接口释放对应的 blob。
        * 仅当所需释放的 blob 不是 nullptr 时，此接口才会被调用。
    */
    je_gl_graphic_api_shader_close_blob_func_t shader_close_blob;

    /*
    je_gl_graphic_api_t::texture_close_blob [成员]
    图形接口关闭纹理资源的底层数据块，当一个资源失效或者图形上下文退出时，由引擎的中间层负责在适当的
    时机调用此接口释放对应的 blob。
        * 仅当所需释放的 blob 不是 nullptr 时，此接口才会被调用。
    */
    je_gl_graphic_api_texture_close_blob_func_t texture_close_blob;

    /*
    je_gl_graphic_api_t::vertex_close_blob [成员]
    图形接口关闭顶点资源的底层数据块，当一个资源失效或者图形上下文退出时，由引擎的中间层负责在适当的
    时机调用此接口释放对应的 blob。
        * 仅当所需释放的 blob 不是 nullptr 时，此接口才会被调用。
    */
    je_gl_graphic_api_vertex_close_blob_func_t vertex_close_blob;

    /*
    je_gl_graphic_api_t::shader_init [成员]
    初始化并创建着色器在图形层对应的资源，此接口将在资源被首次使用时由渲染 API 的
    pipeline 准备阶段调用，图形实现应当在此接口中完成所需的准备工作。
    */
    je_gl_graphic_api_shader_init_func_t shader_init;

    /*
    je_gl_graphic_api_t::texture_2d_init / texture_3d_init / texture_array_init / texture_cube_init /
    texture_msaa_init [成员]
    初始化并创建对应类别纹理在图形层对应的资源，此接口将在资源被首次使用时由渲染 API 的
    纹理绑定准备阶段调用，图形实现应当在此接口中完成所需的准备工作。
    */
    je_gl_graphic_api_texture_2d_init_func_t texture_2d_init;
    je_gl_graphic_api_texture_3d_init_func_t texture_3d_init;
    je_gl_graphic_api_texture_array_init_func_t texture_array_init;
    je_gl_graphic_api_texture_cube_init_func_t texture_cube_init;
    je_gl_graphic_api_texture_msaa_init_func_t texture_msaa_init;

    /*
    je_gl_graphic_api_t::vertex_init [成员]
    初始化并创建顶点在图形层对应的资源，此接口将在资源被首次使用时由渲染 API 的
    绘制准备阶段调用，图形实现应当在此接口中完成所需的准备工作。
    */
    je_gl_graphic_api_vertex_init_func_t vertex_init;

    /*
    je_gl_graphic_api_t::framebuffer_init [成员]
    初始化并创建帧缓冲区在图形层对应的资源，此接口将在资源被首次使用时由渲染 API 的
    pass 准备阶段调用，图形实现应当在此接口中完成所需的准备工作。

    引擎的中间层在创建 frambuffer 实例时，已经创建了对应的附件纹理资源（m_output_attachments），图形实现应
    当通过内部资源准备接口确保这些附件纹理资源已经被正确创建和初始化。
    */
    je_gl_graphic_api_framebuffer_init_func_t framebuffer_init;

    /*
    je_gl_graphic_api_t::ubuffer_init [成员]
    初始化并创建一致变量缓冲区在图形层对应的资源，此接口将在资源被首次使用时由渲染 API 的
    UBO 绑定准备阶段调用，图形实现应当在此接口中完成所需的准备工作。
    */
    je_gl_graphic_api_ubuffer_init_func_t  ubuffer_init;

    /*
    je_gl_graphic_api_t::shader_update [成员]
    当一个 shader 资源的 m_modified 标记被设置为 true 时，引擎中间层将在此资源的下次使用时调用此接口。图形接口
    负责准备好对应的更新工作。
    */
    je_gl_graphic_api_shader_update_func_t shader_update;

    /*
    je_gl_graphic_api_t::texture_2d_upload / texture_3d_upload / texture_array_upload /
    texture_cube_upload [成员]
    提交期落实一个持久纹理区段上传（来自 je_graphic_cmd_upload_texture_* 录制的数据）。
    图形实现负责保证上传不会与仍在执行的帧冲突：允许在必要时等待在飞帧，这是稀有路径。
    */
    je_gl_graphic_api_texture_2d_upload_func_t texture_2d_upload;
    je_gl_graphic_api_texture_3d_upload_func_t texture_3d_upload;
    je_gl_graphic_api_texture_array_upload_func_t texture_array_upload;
    je_gl_graphic_api_texture_cube_upload_func_t texture_cube_upload;

    /*
    je_gl_graphic_api_t::vertex_update [成员]
    提交期落实一个顶点的「本帧内容」（来自 je_graphic_cmd_update_vertex 录制的数据）。
    该内容只对本次提交的绘制生效；图形实现通过每帧瞬态存储（ring / 丢弃重写）保证
    与在飞帧隔离，不得原地覆写仍在执行的帧所读取的持久缓冲。
    vertex_length 为 0（vertices 为 nullptr）表示只调整激活索引数量：图形实现不得
    因此改动或丢弃顶点缓冲的现有内容。
    */
    je_gl_graphic_api_vertex_update_func_t vertex_update;

    /*
    je_gl_graphic_api_t::framebuffer_update [成员]
    当一个 framebuffer 资源的 m_modified 标记被设置为 true 时，引擎中间层将在此资源的下次使用时调用此接口。图形接口
    负责准备好对应的更新工作。
    */
    je_gl_graphic_api_framebuffer_update_func_t framebuffer_update;

    /*
    je_gl_graphic_api_t::ubuffer_update [成员]
    提交期落实一个一致变量缓冲区的「本帧完整内容」（来自
    je_graphic_cmd_update_uniform_buffer 录制的数据）。隔离要求与 vertex_update 相同。
    */
    je_gl_graphic_api_ubuffer_update_func_t  ubuffer_update;

    /*
    je_gl_graphic_api_t::shader_close [成员]
    图形接口关闭着色器资源，当一个着色器资源被释放时，引擎中间层将调用此接口，图形实现
    应当在此接口中完成着色器资源的释放工作。
    */
    je_gl_graphic_api_shader_close_func_t shader_close;

    /*
    je_gl_graphic_api_t::texture_2d_close / texture_3d_close / texture_array_close / texture_cube_close /
    texture_msaa_close [成员]
    图形接口关闭对应类别的纹理资源，当一个纹理资源被释放时，引擎中间层将调用此接口，图形实现
    应当在此接口中完成纹理资源的释放工作。
    */
    je_gl_graphic_api_texture_2d_close_func_t texture_2d_close;
    je_gl_graphic_api_texture_3d_close_func_t texture_3d_close;
    je_gl_graphic_api_texture_array_close_func_t texture_array_close;
    je_gl_graphic_api_texture_cube_close_func_t texture_cube_close;
    je_gl_graphic_api_texture_msaa_close_func_t texture_msaa_close;

    /*
    je_gl_graphic_api_t::vertex_close [成员]
    图形接口关闭顶点资源，当一个顶点资源被释放时，引擎中间层将调用此接口，图形实现
    应当在此接口中完成顶点资源的释放工作。
    */
    je_gl_graphic_api_vertex_close_func_t vertex_close;

    /*
    je_gl_graphic_api_t::framebuffer_close [成员]
    图形接口关闭帧缓冲区资源，当一个帧缓冲区资源被释放时，引擎中间层将调用此接口，图形实现
    应当在此接口中完成帧缓冲区资源的释放工作。
    */
    je_gl_graphic_api_framebuffer_close_func_t framebuffer_close;

    /*
    je_gl_graphic_api_t::ubuffer_close [成员]
    图形接口关闭一致变量缓冲区资源，当一个一致变量缓冲区资源被释放时，引擎中间层将调用此接口，
    图形实现应当在此接口中完成一致变量缓冲区资源的释放工作。
    */
    je_gl_graphic_api_ubuffer_close_func_t ubuffer_close;

    /*
    je_gl_graphic_api_t 的渲染接口（pipeline / sampler / render pass / draw / commit）
        * 与资源接口共用同一张函数表；所有内置后端都必须提供
        * 参数约定与 je_graphic_cmd_* / je_graphic_pipeline_* 公共接口一致
        * 下列接口只在图形线程内、由命令缓冲区的提交过程驱动调用；在进入对应调用前，引擎中间层已经
            完成了相关资源的初始化 / 更新，图形实现无需再次检查资源状态
    */

    /*
    je_gl_graphic_api_t::pipeline_init [成员]
    创建 pipeline 在图形层对应的后端对象（深度 / 混合等固定功能状态，或 PSO），图形实现应当将创建结果
    保存到 pipeline 的 m_backend_ptr 中。此接口在 pipeline 首次参与提交，或图形线程重启导致后端状态失效
    后，于 pipeline_bind 之前调用。
        * 调用前引擎已确保 pipeline 对应的 shader 已经初始化 / 更新完成。
        * 返回后引擎会将 pipeline 标记为已初始化，并在 pipeline 释放时调用 pipeline_close。
    */
    je_gl_graphic_api_pipeline_init_func_t pipeline_init;

    /*
    je_gl_graphic_api_t::pipeline_close [成员]
    释放 pipeline 在图形层对应的后端对象，即 pipeline_init 中创建的内容。pipeline 被释放时由引擎中间层
    在图形线程中调用此接口。
        * 即使 pipeline 从未参与提交（pipeline_init 未被调用、后端指针为空），此接口也可能被调用，
            实现必须能够安全处理这种情况。
    */
    je_gl_graphic_api_pipeline_close_func_t pipeline_close;

    /*
    je_gl_graphic_api_t::pipeline_bind [成员]
    将 pipeline 绑定为当前绘制状态：绑定其 shader，并应用固定功能状态（顶点输入布局、深度测试 / 写入、
    混合、剔除、绕序、颜色写掩码等）。图元拓扑由顶点资源携带，在 draw 时生效。引擎在提交过程中遇到
    绑定 pipeline 的命令时调用此接口，并保证：
        * 若 pipeline 尚未初始化，会先调用 pipeline_init；
        * 连续的重复绑定会被去重，不会重复调用。
    */
    je_gl_graphic_api_pipeline_bind_func_t pipeline_bind;

    /*
    je_gl_graphic_api_t::sampler_init [成员]
    根据 sampler 的描述（je_gl_sampler_desc_t）创建图形层对应的采样器对象，并保存到 sampler 的后端指针中。
    此接口在 sampler 首次参与提交，或图形线程重启导致后端状态失效后，于 sampler_bind 之前调用。
    */
    je_gl_graphic_api_sampler_init_func_t sampler_init;

    /*
    je_gl_graphic_api_t::sampler_close [成员]
    释放 sampler 在图形层对应的采样器对象，即 sampler_init 中创建的内容。sampler 被释放时由引擎中间层
    在图形线程中调用此接口。
        * 即使 sampler 从未参与提交（sampler_init 未被调用、后端指针为空），此接口也可能被调用，
            实现必须能够安全处理这种情况。
    */
    je_gl_graphic_api_sampler_close_func_t sampler_close;

    /*
    je_gl_graphic_api_t::sampler_bind [成员]
    为 shader 声明的采样器 sampler_id 绑定独立采样器，覆盖 shader 声明的默认采样器。
        * sampler_id 取自 shader 反射（je_gl_shader_t::m_sampler_methods），对该采样器声明覆盖的全部
            纹理 pass 生效，图形实现应当将采样器绑定到这些 pass 对应的纹理单元 / 描述符槽位。
        * 引擎保证 sampler 已经初始化，并会对同一 sampler_id 上连续的重复绑定去重。
    */
    je_gl_graphic_api_sampler_bind_func_t sampler_bind;

    /*
    je_gl_graphic_api_t::begin_pass [成员]
    开始一个渲染 pass：将后续绘制定向到 target 对应的渲染目标，并应用 viewport。
        * target 为空表示屏幕后备缓冲区。
        * viewport 不得为空，且 width / height 必须大于 0。
        * 不允许嵌套：上一个 pass 必须以 end_pass 结束，引擎保证调用顺序。
        * 图形实现应当在此重置 pass 级的状态缓存（当前 shader、顶点、纹理等绑定），
            引擎侧的绑定去重缓存也会在 begin_pass 时重置。
    */
    je_gl_graphic_api_begin_pass_func_t begin_pass;

    /*
    je_gl_graphic_api_t::clear_color [成员]
    将当前 pass 的第 attachment 个颜色附件清除为 rgba 指定的颜色。
        * attachment 是颜色附件索引，从 0 开始；屏幕后备缓冲区只有 0 号附件。
        * 只能在 begin_pass 之后、当前 pass 的第一次 draw 之前调用。
    */
    je_gl_graphic_api_clear_color_func_t clear_color;

    /*
    je_gl_graphic_api_t::clear_depth [成员]
    将当前 pass 的深度附件清除为 depth。
        * 只能在 begin_pass 之后、当前 pass 的第一次 draw 之前调用。
        * 若当前渲染目标没有深度附件，图形实现可以忽略此调用。
    */
    je_gl_graphic_api_clear_depth_func_t clear_depth;

    /*
    je_gl_graphic_api_t::end_pass [成员]
    结束当前 pass。图形实现应当在此完成该 pass 的收尾工作（例如结束渲染编码、解析附件、结束命令录制段）；
    屏幕后备缓冲区的呈现仍由 update_draw_commit 负责。
        * 每个 begin_pass 都有且仅有一个配对的 end_pass；若提交时 pass 尚未显式结束，引擎会在
            commit_begin 之前自动补发 end_pass。
    */
    je_gl_graphic_api_end_pass_func_t end_pass;

    /*
    je_gl_graphic_api_t::bind_vertex [成员]
    绑定顶点资源（顶点 / 索引缓冲区与顶点格式），作为后续 draw 的数据来源。
        * 顶点拓扑包含在顶点资源中，实际在 draw 时使用。
        * 引擎保证顶点资源在调用前已经初始化 / 更新完成，并会对连续的重复绑定去重。
    */
    je_gl_graphic_api_bind_vertex_func_t bind_vertex;

    /*
    je_gl_graphic_api_t::bind_uniform_buffer [成员]
    绑定 uniform buffer 资源，绑定点使用 uniform buffer 创建时指定的位置，
    避免绘制方与 shader 对 binding 编号的理解不一致。
        * 引擎保证 uniform buffer 在调用前已经初始化 / 更新完成，并会对连续的重复绑定去重。
    */
    je_gl_graphic_api_bind_uniform_buffer_func_t bind_uniform_buffer;

    /*
    je_gl_graphic_api_t::bind_texture_2d / bind_texture_3d / bind_texture_array / bind_texture_cube [成员]
    将对应类别的纹理绑定到纹理 pass（即 shader 中声明的纹理槽位）。
        * pass 是 shader 声明的纹理 pass 索引（见 shader 反射的 m_pass_ids），不同类别纹理共用同一套
            pass 编号。
        * 引擎保证纹理资源在调用前已经初始化 / 更新完成，并会按 (pass, 纹理对象) 对连续重复绑定去重。
    */
    je_gl_graphic_api_bind_texture_2d_func_t bind_texture_2d;
    je_gl_graphic_api_bind_texture_3d_func_t bind_texture_3d;
    je_gl_graphic_api_bind_texture_array_func_t bind_texture_array;
    je_gl_graphic_api_bind_texture_cube_func_t bind_texture_cube;

    /*
    je_gl_graphic_api_t::draw_indexed [成员]
    使用当前绑定的 pipeline / 顶点 / 纹理执行一次索引绘制。
        * index_count、instance_count 均大于 0；索引类型固定为 uint32_t，first_index 是索引元素的
            偏移（不是字节偏移）。
        * uniform_data 是本 draw 的 per-draw uniform 数据块快照，uniform_size 等于当前 shader 的
            m_uniform_block_size；shader 未声明 uniform 块时，二者分别为 nullptr 和 0。图形实现应当
            在绘制前将该数据上传到设备（例如写入每帧 UBO ring）并绑定。
        * 调用前引擎保证 pipeline 与顶点资源已经绑定，且均已初始化。
    */
    je_gl_graphic_api_draw_indexed_func_t draw_indexed;

    /*
    je_gl_graphic_api_t::draw [成员]
    使用当前绑定的 pipeline / 顶点 / 纹理执行一次非索引绘制。
        * vertex_count、instance_count 均大于 0；first_vertex 是顶点偏移。
        * uniform_data / uniform_size 的约定与 draw_indexed 相同。
    */
    je_gl_graphic_api_draw_func_t draw;

    /*
    je_gl_graphic_api_t::commit_begin / commit_end [成员]
    提交单个命令缓冲区时，引擎在回放其全部命令的前后各调用一次：
    commit_begin -> [begin_pass / clear / bind / draw ...] -> commit_end。
        * 图形实现可以用它开启 / 结束一次命令录制或提交作用域（例如获取交换链图像、开始 / 结束命令
            列表）；当前内置后端均为空实现。
        * 二者都只在图形线程中调用，且与同一次提交中的其它渲染接口串行执行。
    */
    je_gl_graphic_api_commit_begin_func_t commit_begin;
    je_gl_graphic_api_commit_end_func_t commit_end;
};
static_assert(sizeof(je_gl_graphic_api_t) % sizeof(void*) == 0);
typedef je_graphic_uhost_t je_GraphicUhost;
typedef je_render_branch_t je_RenderBranch;
typedef je_gl_interface_config_t je_GlInterfaceConfig;
typedef je_gl_context_notifier_t je_GlContextNotifier;
typedef je_gl_graphic_api_t je_GlGraphicApi;
typedef je_gl_context_promise_t je_GlContextPromise;
typedef je_gl_update_action_t je_GlUpdateAction;
typedef je_graphic_impl_context_t je_GraphicImplContext;
typedef je_gl_context_t je_GlContext;
typedef je_gl_resource_blob_t je_GlResourceBlob;
typedef je_gl_resource_bind_counter_t je_GlResourceBindCounter;
typedef je_gl_resource_handle_t je_GlResourceHandle;
typedef je_gl_texture_kind_t je_GlTextureKind;
typedef je_gl_texture_common_t je_GlTextureCommon;
typedef je_gl_texture_2d_t je_GlTexture2D;
typedef je_gl_texture_3d_t je_GlTexture3D;
typedef je_gl_texture_array_t je_GlTextureArray;
typedef je_gl_texture_cube_t je_GlTextureCube;
typedef je_gl_texture_msaa_t je_GlTextureMsaa;
typedef je_gl_vertex_t je_GlVertex;
typedef je_gl_shader_t je_GlShader;
typedef je_gl_frame_buffer_t je_GlFrameBuffer;
typedef je_gl_uniform_buffer_t je_GlUniformBuffer;
typedef je_gl_frame_buffer_clear_operation_t je_GlFrameBufferClearOperation;
typedef je_gl_pipeline_t je_GlPipeline;
typedef je_gl_sampler_t je_GlSampler;
typedef je_gl_command_buffer_t je_GlCommandBuffer;
typedef je_gl_viewport_t je_GlViewport;
typedef je_gl_display_mode_t je_GlDisplayMode;
typedef je_gl_frame_job_func_t je_GlFrameJobFunc;
typedef je_gl_texture_pixel_data_t je_GlTexturePixelData;
typedef je_gl_texture_format_t je_GlTextureFormat;
typedef je_gl_vertex_type_t je_GlVertexType;
typedef je_gl_vertex_data_type_t je_GlVertexDataType;
typedef je_gl_vertex_data_layout_t je_GlVertexDataLayout;
typedef je_gl_vertex_bone_data_t je_GlVertexBoneData;
typedef je_gl_shader_filter_mode_t je_GlShaderFilterMode;
typedef je_gl_shader_wrap_mode_t je_GlShaderWrapMode;
typedef je_gl_shader_sampler_method_t je_GlShaderSamplerMethod;
typedef je_gl_shader_uniform_type_t je_GlShaderUniformType;
typedef je_gl_shader_uniform_mask_t je_GlShaderUniformMask;
typedef je_gl_shader_script_uniform_variant_t je_GlShaderScriptUniformVariant;
typedef je_gl_shader_builtin_uniform_location_t je_GlShaderBuiltinUniformLocation;
typedef je_gl_shader_uniform_value_t je_GlShaderUniformValue;
typedef je_gl_shader_uniform_variables_t je_GlShaderUniformVariables;
typedef je_gl_shader_uniform_block_t je_GlShaderUniformBlock;
typedef je_gl_shader_depth_test_method_t je_GlShaderDepthTestMethod;
typedef je_gl_shader_depth_mask_method_t je_GlShaderDepthMaskMethod;
typedef je_gl_shader_blend_equation_t je_GlShaderBlendEquation;
typedef je_gl_shader_blend_method_t je_GlShaderBlendMethod;
typedef je_gl_shader_cull_mode_t je_GlShaderCullMode;
typedef je_gl_shader_spirv_code_t je_GlShaderSpirvCode;
typedef je_gl_shader_texture_decl_t je_GlShaderTextureDecl;
typedef je_gl_frame_buffer_attachment_t je_GlFrameBufferAttachment;
typedef je_gl_frame_buffer_clear_type_t je_GlFrameBufferClearType;
typedef je_gl_frame_buffer_clear_color_attachment_t je_GlFrameBufferClearColorAttachment;
typedef je_gl_frame_buffer_clear_depth_attachment_t je_GlFrameBufferClearDepthAttachment;
typedef je_gl_graphic_api_startup_func_t je_GlGraphicApiStartupFunc;
typedef je_gl_graphic_api_shutdown_func_t je_GlGraphicApiShutdownFunc;
typedef je_gl_graphic_api_update_func_t je_GlGraphicApiUpdateFunc;
typedef je_gl_graphic_api_commit_func_t je_GlGraphicApiCommitFunc;
typedef je_gl_graphic_api_shader_create_blob_func_t je_GlGraphicApiShaderCreateBlobFunc;
typedef je_gl_graphic_api_shader_close_blob_func_t je_GlGraphicApiShaderCloseBlobFunc;
typedef je_gl_graphic_api_shader_init_func_t je_GlGraphicApiShaderInitFunc;
typedef je_gl_graphic_api_shader_update_func_t je_GlGraphicApiShaderUpdateFunc;
typedef je_gl_graphic_api_shader_close_func_t je_GlGraphicApiShaderCloseFunc;
typedef je_gl_graphic_api_texture_2d_create_blob_func_t je_GlGraphicApiTexture2DCreateBlobFunc;
typedef je_gl_graphic_api_texture_3d_create_blob_func_t je_GlGraphicApiTexture3DCreateBlobFunc;
typedef je_gl_graphic_api_texture_array_create_blob_func_t je_GlGraphicApiTextureArrayCreateBlobFunc;
typedef je_gl_graphic_api_texture_cube_create_blob_func_t je_GlGraphicApiTextureCubeCreateBlobFunc;
typedef je_gl_graphic_api_texture_msaa_create_blob_func_t je_GlGraphicApiTextureMsaaCreateBlobFunc;
typedef je_gl_graphic_api_texture_close_blob_func_t je_GlGraphicApiTextureCloseBlobFunc;
typedef je_gl_graphic_api_texture_2d_init_func_t je_GlGraphicApiTexture2DInitFunc;
typedef je_gl_graphic_api_texture_3d_init_func_t je_GlGraphicApiTexture3DInitFunc;
typedef je_gl_graphic_api_texture_array_init_func_t je_GlGraphicApiTextureArrayInitFunc;
typedef je_gl_graphic_api_texture_cube_init_func_t je_GlGraphicApiTextureCubeInitFunc;
typedef je_gl_graphic_api_texture_msaa_init_func_t je_GlGraphicApiTextureMsaaInitFunc;
typedef je_gl_graphic_api_texture_2d_upload_func_t je_GlGraphicApiTexture2DUploadFunc;
typedef je_gl_graphic_api_texture_3d_upload_func_t je_GlGraphicApiTexture3DUploadFunc;
typedef je_gl_graphic_api_texture_array_upload_func_t je_GlGraphicApiTextureArrayUploadFunc;
typedef je_gl_graphic_api_texture_cube_upload_func_t je_GlGraphicApiTextureCubeUploadFunc;
typedef je_gl_graphic_api_texture_2d_close_func_t je_GlGraphicApiTexture2DCloseFunc;
typedef je_gl_graphic_api_texture_3d_close_func_t je_GlGraphicApiTexture3DCloseFunc;
typedef je_gl_graphic_api_texture_array_close_func_t je_GlGraphicApiTextureArrayCloseFunc;
typedef je_gl_graphic_api_texture_cube_close_func_t je_GlGraphicApiTextureCubeCloseFunc;
typedef je_gl_graphic_api_texture_msaa_close_func_t je_GlGraphicApiTextureMsaaCloseFunc;
typedef je_gl_graphic_api_vertex_create_blob_func_t je_GlGraphicApiVertexCreateBlobFunc;
typedef je_gl_graphic_api_vertex_close_blob_func_t je_GlGraphicApiVertexCloseBlobFunc;
typedef je_gl_graphic_api_vertex_init_func_t je_GlGraphicApiVertexInitFunc;
typedef je_gl_graphic_api_vertex_update_func_t je_GlGraphicApiVertexUpdateFunc;
typedef je_gl_graphic_api_vertex_close_func_t je_GlGraphicApiVertexCloseFunc;
typedef je_gl_graphic_api_framebuffer_init_func_t je_GlGraphicApiFrameBufferInitFunc;
typedef je_gl_graphic_api_framebuffer_update_func_t je_GlGraphicApiFrameBufferUpdateFunc;
typedef je_gl_graphic_api_framebuffer_close_func_t je_GlGraphicApiFrameBufferCloseFunc;
typedef je_gl_graphic_api_ubuffer_init_func_t je_GlGraphicApiUBufferInitFunc;
typedef je_gl_graphic_api_ubuffer_update_func_t je_GlGraphicApiUBufferUpdateFunc;
typedef je_gl_graphic_api_ubuffer_close_func_t je_GlGraphicApiUBufferCloseFunc;
typedef je_gl_graphic_api_pipeline_init_func_t je_GlGraphicApiPipelineInitFunc;
typedef je_gl_graphic_api_pipeline_close_func_t je_GlGraphicApiPipelineCloseFunc;
typedef je_gl_graphic_api_pipeline_bind_func_t je_GlGraphicApiPipelineBindFunc;
typedef je_gl_graphic_api_sampler_init_func_t je_GlGraphicApiSamplerInitFunc;
typedef je_gl_graphic_api_sampler_close_func_t je_GlGraphicApiSamplerCloseFunc;
typedef je_gl_graphic_api_sampler_bind_func_t je_GlGraphicApiSamplerBindFunc;
typedef je_gl_graphic_api_begin_pass_func_t je_GlGraphicApiBeginPassFunc;
typedef je_gl_graphic_api_clear_color_func_t je_GlGraphicApiClearColorFunc;
typedef je_gl_graphic_api_clear_depth_func_t je_GlGraphicApiClearDepthFunc;
typedef je_gl_graphic_api_end_pass_func_t je_GlGraphicApiEndPassFunc;
typedef je_gl_graphic_api_bind_vertex_func_t je_GlGraphicApiBindVertexFunc;
typedef je_gl_graphic_api_bind_uniform_buffer_func_t je_GlGraphicApiBindUniformBufferFunc;
typedef je_gl_graphic_api_bind_texture_2d_func_t je_GlGraphicApiBindTexture2DFunc;
typedef je_gl_graphic_api_bind_texture_3d_func_t je_GlGraphicApiBindTexture3DFunc;
typedef je_gl_graphic_api_bind_texture_array_func_t je_GlGraphicApiBindTextureArrayFunc;
typedef je_gl_graphic_api_bind_texture_cube_func_t je_GlGraphicApiBindTextureCubeFunc;
typedef je_gl_graphic_api_draw_indexed_func_t je_GlGraphicApiDrawIndexedFunc;
typedef je_gl_graphic_api_draw_func_t je_GlGraphicApiDrawFunc;
typedef je_gl_graphic_api_commit_begin_func_t je_GlGraphicApiCommitBeginFunc;
typedef je_gl_graphic_api_commit_end_func_t je_GlGraphicApiCommitEndFunc;


typedef void  (*je_graphic_api_register_func_t)(je_gl_graphic_api_t*);
typedef void  (*je_graphic_sync_callback_func_t)(je_gl_context_t*, void*);

/*
je_graphic_sync_thread_callback_register [基本接口]
一些平台下，由于上下文等限制，不能创建独立的渲染线程，
对于这种情况，可以使用此函数注册一个回调函数，在本应
创建图形线程的地方调用此回调，用于获取对应的universe
和图形上下文，之后则需要手动调用同步图形更新接口。
    * 必须在 je_core_init 之后注册，引擎初始化会重置回调
请参见：
    je_core_init
*/
JE_API void je_graphic_sync_thread_callback_register(
    je_graphic_sync_callback_func_t callback,
    /* OPTIONAL */ void* arg);

/*
je_gl_sync_state_t [类型]
手动同步图形更新时，je_graphic_sync_update 返回的状态
    * JE_GL_SYNC_COMPLETE 表示本帧正常结束
    * JE_GL_SYNC_SHUTDOWN 表示绘制将结束
    * JE_GL_SYNC_REBOOT 表示需要重新初始化图形上下文
*/
enum je_gl_sync_state_t
{
    JE_GL_SYNC_COMPLETE,
    JE_GL_SYNC_SHUTDOWN,
    JE_GL_SYNC_REBOOT,
};

/*
je_graphic_sync_init [基本接口]
一些平台下，由于上下文等限制，不能创建独立的渲染线程，
对于这种情况，可以使用je_graphic_sync_thread_callback_register
注册回调函数，并手动执行 je_graphic_sync_init、je_graphic_sync_update
和 je_graphic_sync_shutdown。
    * je_graphic_sync_init 用于图形在首次启动或重启时执行图形
        任务的初始化操作，初始化完成后，则循环执行
        je_graphic_sync_update 直到其返回 JE_GL_SYNC_SHUTDOWN
        或 JE_GL_SYNC_REBOOT 之前，都不需要重新初始化
请参见：
    je_graphic_sync_thread_callback_register
    je_graphic_sync_update
    je_graphic_sync_shutdown
*/
JE_API void je_graphic_sync_init(
    je_gl_context_t* thread,
    bool isreboot);

/*
je_graphic_sync_update [基本接口]
一些平台下，由于上下文等限制，不能创建独立的渲染线程，
对于这种情况，可以使用je_graphic_sync_thread_callback_register
注册回调函数，并手动执行 je_graphic_sync_init、je_graphic_sync_update
和 je_graphic_sync_shutdown。
    * je_graphic_sync_update 用于更新图形图形的一帧
    * 函数返回 JE_GL_SYNC_COMPLETE 表示正常结束
    * 返回 JE_GL_SYNC_SHUTDOWN 表示绘制将结束，请以 isreboot = false
        调用 je_graphic_sync_shutdown，结束全部绘制工作
    * 返回 JE_GL_SYNC_REBOOT 表示绘制需要重新初始化，请以
        isreboot = true 调用 je_graphic_sync_shutdown 释放当前
        上下文，然后以 isreboot = true 调用je_graphic_sync_init
        重新执行初始化
请参见：
    je_graphic_sync_thread_callback_register
    je_graphic_sync_init
    je_graphic_sync_shutdown
*/
JE_API je_gl_sync_state_t je_graphic_sync_update(je_gl_context_t* thread);

/*
je_graphic_sync_shutdown [基本接口]
一些平台下，由于上下文等限制，不能创建独立的渲染线程，
对于这种情况，可以使用je_graphic_sync_thread_callback_register
注册回调函数，并手动执行 je_graphic_sync_init、je_graphic_sync_update
和 je_graphic_sync_shutdown。
    * je_graphic_sync_shutdown 用于释放上下文，根据je_graphic_sync_update
        的返回值，以适当的参数调用此函数以正确完成渲染逻辑
    * 返回值事实上只与 isreboot 有关：返回 true 表示确实退出，
        返回 false 表示仍需重新初始化
请参见：
    je_graphic_sync_thread_callback_register
    je_graphic_sync_init
    je_graphic_sync_shutdown
*/
JE_API bool je_graphic_sync_shutdown(je_gl_context_t* thread, bool isreboot);

/*
je_graphic_thread_start [基本接口]
创建图形线程
指定配置、图形库接口和帧更新函数创建一个图形绘制线程。
图形线程的帧更新操作 je_graphic_update 将调用指定的帧更新函数，
实际的绘制任务应该放在帧更新函数中
创建出来的图形线程需要使用je_graphic_thread_terminate释放
请参见：
    je_graphic_update
    je_graphic_thread_terminate
*/
JE_API /* OPTIONAL: If failed to start up */ je_gl_context_t* je_graphic_thread_start(
    je_gl_interface_config_t config,
    /* OPTIONAL: If no universe attached */ je_game_universe_t* universe_instance,
    je_graphic_api_register_func_t register_func,
    je_gl_frame_job_func_t frame_rend_work,
    /* OPTIONAL */ void* arg);

/*
je_graphic_thread_terminate [基本接口]
终止图形线程，将会阻塞直到图形线程完全退出
创建图形线程之后，无论图形绘制工作是否终止，都需要使用此接口释放图形线程
请参见：
    je_graphic_thread_start
*/
JE_API void je_graphic_thread_terminate(je_gl_context_t* thread_handle);

/*
je_gl_update_sync_mode_t [类型]
je_graphic_update 的同步模式
    * JE_GL_WAIT_THIS_FRAME_END 表示阻塞直到当前帧渲染完毕
    * JE_GL_WAIT_LAST_FRAME_END 表示阻塞直到上一帧渲染完毕
*/
enum je_gl_update_sync_mode_t
{
    JE_GL_WAIT_THIS_FRAME_END,
    JE_GL_WAIT_LAST_FRAME_END,
};

/*
je_gl_update_sync_callback_t [类型]
je_graphic_update 完成等待后调用的回调函数类型，参数为注册时传入的 callback_param
*/
typedef void (*je_gl_update_sync_callback_t)(void*);

/*
je_graphic_update [基本接口]
调度图形线程更新一帧，此函数将阻塞直到一帧的绘制操作完成
    * mode 用于指示更新的同步模式，其中：
        WAIT_THIS_FRAME_END：阻塞直到当前帧渲染完毕
        * 同步更简单，但是绘制时间无法和其他逻辑同步执行
        WAIT_LAST_FRAME_END：阻塞直到上一帧渲染完毕（绘制信号发出后立即返回）
        * 更适合CPU密集操作，但需要更复杂的机制以保证数据完整性
*/
JE_API bool je_graphic_update(
    je_gl_context_t* thread_handle,
    je_gl_update_sync_mode_t mode,
    /* OPTIONAL */ je_gl_update_sync_callback_t callback_after_wait_may_null,
    /* OPTIONAL */ void* callback_param);

/*
je_graphic_thread_reboot [基本接口]
以指定的配置请求重启一个图形线程
    * 若不需要改变图形配置，请使用nullptr
*/
JE_API void je_graphic_thread_reboot(
    je_gl_context_t* thread_handle,
    /* OPTIONAL: nullptr to keep current config */ const je_gl_interface_config_t* config_may_null);

/*
je_graphic_resource_handle_share [基本接口]
增加指定资源句柄的引用计数，使得对应的图形资源需要额外执行一次 close 操作才能被释放
    * 此函数通常用于实现资源的共享引用
*/
JE_API void je_graphic_resource_handle_share(je_gl_resource_handle_t* resource_handle);

/*
jegl_share_resource [宏]
对 je_graphic_resource_handle_share 的封装宏，用于增加指定资源的引用计数
    * 此宏通常用于实现资源的共享引用
请参见：
    je_graphic_resource_handle_share
*/
#define jegl_share_resource(resource) je_graphic_resource_handle_share(&(resource)->m_handle)

///////////////////////////// SHADER /////////////////////////////
/*
je_graphic_shader_source_load [基本接口]
从源码加载一个着色器实例，可创建或使用缓存文件以加速着色器的加载
    * 实际上je_graphic_shader_load会读取文件内容之后，调用此函数进行实际上的着色器加载
        若不需要创建缓存文件，请将 is_virtual_file 指定为 false
    * 图形资源被创建时，其自动获取一个引用计数；使用 jegl_share_resource可增加引
        用计数，使用 je_graphic_shader_close 可减少引用计数；当资源的引用计数变为 0 时，
        资源将被释放
请参见：
    je_graphic_shader_load
    jegl_share_resource
    je_graphic_shader_close
*/
JE_API /* OPTIONAL */ je_gl_shader_t* je_graphic_shader_source_load(
    /* OPTIONAL */ je_gl_context_t* context,
    const char* path,
    const char* src,
    bool is_virtual_file);

/*
je_graphic_shader_load [基本接口]
从源码文件加载一个着色器实例，会创建或使用缓存文件以加速着色器的加载
    * 图形资源被创建时，其自动获取一个引用计数；使用 jegl_share_resource可增加引
        用计数，使用 je_graphic_shader_close 可减少引用计数；当资源的引用计数变为 0 时，
        资源将被释放
请参见：
    jegl_share_resource
    je_graphic_shader_close
*/
JE_API /* OPTIONAL */ je_gl_shader_t* je_graphic_shader_load(
    /* OPTIONAL */ je_gl_context_t* context,
    const char* path);

/*
je_graphic_shader_close [基本接口]
关闭一个着色器资源（使得其引用计数减少），如果资源被 jegl_share_resource 引用过多次，
那么需要对应调用多次才能真正释放资源；
    * 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用；对应的底层图形资
        源将由资源对应的图形线程上下文在适当的时机释放。
请参见：
    jegl_share_resource
*/
JE_API void je_graphic_shader_close(je_gl_shader_t* shader);
///////////////////////////// TEXTURE /////////////////////////////
/*
je_graphic_texture_2d_load [基本接口]
从指定路径加载一个二维纹理资源，加载的路径规则与 je_file_open 相同
    * 从图片纹理缓存 <path>.je4cache 读取颜色空间与像素数据；缓存缺失或
        失效时先按图片与属性源文件 <path>.je4texture 重建缓存（导出的
        运行时只携带缓存、没有生成器，缓存不可用时返回 nullptr）
    * 若无法取得可用的缓存，则返回nullptr
    * 图形资源被创建时，其自动获取一个引用计数；使用 jegl_share_resource可增加引
        用计数，使用 je_graphic_texture_2d_close 可减少引用计数；当资源的引用计数变为 0 时，
        资源将被释放
请参见：
    je_file_open
    je_image_texture_cache_open_or_create
    jegl_share_resource
    je_graphic_texture_2d_close
*/
JE_API /* OPTIONAL */ je_gl_texture_2d_t* je_graphic_texture_2d_load(
    /* OPTIONAL */ je_gl_context_t* context,
    const char* path);

/*
je_graphic_texture_2d_create [基本接口]
创建一个指定大小和格式的二维纹理资源
    * initial_pixels 是一份完整像素快照（行序与 je_gl_texture_common_t 的
        m_initial_pixels 一致，自底向上），创建时拷贝保存；传 nullptr 表示不创建
        像素快照（纹理在 GPU 侧未初始化）
    * 若指定的格式包含 FLOAT16、DEPTH 或 FRAMEBUF，则 initial_pixels 必须为 nullptr
    * 创建期快照只在后端首次初始化（以及图形线程重建）时读取，创建后不再允许改写；
        运行时修改纹理请使用 je_graphic_cmd_upload_texture_2d
    * 考虑到部分图形库的实现，如果指定的宽度或高度为 0，je_graphic_texture_2d_create将视其为 1
    * 图形资源被创建时，其自动获取一个引用计数；使用 jegl_share_resource可增加引
        用计数，使用 je_graphic_texture_2d_close 可减少引用计数；当资源的引用计数变为 0 时，
        资源将被释放
    * 使用此接口创建的纹理，其对应的资源路径为 nullptr
请参见：
    jegl_share_resource
    je_graphic_texture_2d_close
    je_graphic_cmd_upload_texture_2d
*/
JE_API je_gl_texture_2d_t* /* NOT NULL */ je_graphic_texture_2d_create(
    size_t width,
    size_t height,
    je_gl_texture_format_t format,
    /* OPTIONAL: nullptr 表示不创建像素快照 */ const je_gl_texture_pixel_data_t* initial_pixels);

/*
je_graphic_texture_3d_create [基本接口]
创建一个指定大小、深度和格式的三维纹理资源
    * initial_pixels 语义同 je_graphic_texture_2d_create，数据按 宽度 x 高度 x 深度 连续排布
    * 若指定的格式包含 FLOAT16，则 initial_pixels 必须为 nullptr
    * 若指定的宽度、高度或深度为 0，将视其为 1
请参见：
    jegl_share_resource
    je_graphic_texture_3d_close
    je_graphic_cmd_upload_texture_3d
*/
JE_API je_gl_texture_3d_t* /* NOT NULL */ je_graphic_texture_3d_create(
    size_t width,
    size_t height,
    size_t depth,
    je_gl_texture_format_t format,
    /* OPTIONAL: nullptr 表示不创建像素快照 */ const je_gl_texture_pixel_data_t* initial_pixels);

/*
je_graphic_texture_array_create [基本接口]
创建一个指定大小、层数和格式的纹理数组资源
    * initial_pixels 语义同 je_graphic_texture_2d_create，数据按 宽度 x 高度 x 层数 连续排布
    * 若指定的格式包含 FLOAT16，则 initial_pixels 必须为 nullptr
    * 若指定的宽度、高度或层数为 0，将视其为 1
请参见：
    jegl_share_resource
    je_graphic_texture_array_close
    je_graphic_cmd_upload_texture_array
*/
JE_API je_gl_texture_array_t* /* NOT NULL */ je_graphic_texture_array_create(
    size_t width,
    size_t height,
    size_t layers,
    je_gl_texture_format_t format,
    /* OPTIONAL: nullptr 表示不创建像素快照 */ const je_gl_texture_pixel_data_t* initial_pixels);

/*
je_graphic_texture_cube_create [基本接口]
创建一个指定大小和格式的立方体贴图纹理资源，固定包含 6 个面
    * initial_pixels 语义同 je_graphic_texture_2d_create，数据按 6 x 宽度 x 高度 连续排布
    * 若指定的格式包含 FLOAT16，则 initial_pixels 必须为 nullptr
    * 若指定的宽度或高度为 0，将视其为 1
请参见：
    jegl_share_resource
    je_graphic_texture_cube_close
    je_graphic_cmd_upload_texture_cube
*/
JE_API je_gl_texture_cube_t* /* NOT NULL */ je_graphic_texture_cube_create(
    size_t width,
    size_t height,
    je_gl_texture_format_t format,
    /* OPTIONAL: nullptr 表示不创建像素快照 */ const je_gl_texture_pixel_data_t* initial_pixels);

/*
je_graphic_texture_msaa_create [基本接口]
创建一个指定大小、采样数和格式的多重采样二维纹理资源，此类纹理仅存在于 GPU 侧
（用作渲染目标附件），不持有 CPU 像素数据
    * 若指定的采样数为 0，将视其为 1
请参见：
    jegl_share_resource
    je_graphic_texture_msaa_close
*/
JE_API je_gl_texture_msaa_t* /* NOT NULL */ je_graphic_texture_msaa_create(
    size_t width,
    size_t height,
    size_t samples,
    je_gl_texture_format_t format);

/*
je_graphic_texture_2d_close / je_graphic_texture_3d_close / je_graphic_texture_array_close /
je_graphic_texture_cube_close / je_graphic_texture_msaa_close [基本接口]
关闭一个对应类别的纹理资源（使得其引用计数减少），如果资源被 jegl_share_resource 引用过多次，
那么需要对应调用多次才能真正释放资源；
    * 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用；对应的底层图形资
        源将由资源对应的图形线程上下文在适当的时机释放。
请参见：
    jegl_share_resource
*/
JE_API void je_graphic_texture_2d_close(je_gl_texture_2d_t* texture);
JE_API void je_graphic_texture_3d_close(je_gl_texture_3d_t* texture);
JE_API void je_graphic_texture_array_close(je_gl_texture_array_t* texture);
JE_API void je_graphic_texture_cube_close(je_gl_texture_cube_t* texture);
JE_API void je_graphic_texture_msaa_close(je_gl_texture_msaa_t* texture);

///////////////////////////// VERTEX /////////////////////////////
/*
je_graphic_vertex_load [基本接口]
从指定路径加载一个顶点（模型）资源，加载的路径规则与 je_file_open 相同
    * 若指定的文件不存在或不是一个合法的模型，则返回nullptr
    * 使用此方法加载的模型，始终按照三角面排布，其顶点数据格式如下所示：
        顶点坐标        3 * f32
        UV映射坐标      2 * f32
        法线方向        3 * f32
        切线方向        3 * f32（由引擎按面累计计算，不使用模型自带切线）
        骨骼索引        4 * i32
        骨骼权重        4 * f32
        （每顶点共 76 字节）
    * 对于绑定骨骼数量小于4的顶点，保证空余位置的骨骼索引被 0 填充，权重为 0.f
    * 图形资源被创建时，其自动获取一个引用计数；使用 jegl_share_resource可增加引
        用计数，使用 je_graphic_vertex_close 可减少引用计数；当资源的引用计数变为 0 时，
        资源将被释放
请参见：
    je_file_open
    jegl_share_resource
    je_graphic_vertex_close
*/
JE_API /* OPTIONAL */ je_gl_vertex_t* je_graphic_vertex_load(
    /* OPTIONAL */ je_gl_context_t* context,
    const char* path);

/*
je_graphic_vertex_create [基本接口]
用指定的顶点数据创建一个顶点（模型）资源
    * 图形资源被创建时，其自动获取一个引用计数；使用 jegl_share_resource可增加引
        用计数，使用 je_graphic_vertex_close 可减少引用计数；当资源的引用计数变为 0 时，
        资源将被释放
    * 使用此接口创建的顶点，其对应的资源路径为 nullptr
请参见：
    jegl_share_resource
    je_graphic_vertex_close
*/
JE_API /* OPTIONAL: If bad format or no completed vertex point */ je_gl_vertex_t* je_graphic_vertex_create(
    je_gl_vertex_type_t type,
    const void* datas,
    size_t data_length,
    const uint32_t* indices,
    size_t index_count,
    const je_gl_vertex_data_layout_t* format,
    size_t format_count);

/*
je_graphic_vertex_close [基本接口]
关闭一个顶点（模型）资源（使得其引用计数减少），如果资源被 jegl_share_resource 引用过多次，
那么需要对应调用多次才能真正释放资源；
    * 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用；对应的底层图形资
        源将由资源对应的图形线程上下文在适当的时机释放。
请参见：
    jegl_share_resource
    je_graphic_cmd_update_vertex
*/
JE_API void je_graphic_vertex_close(je_gl_vertex_t* vertex);

///////////////////////////// FRAMEBUFFER /////////////////////////////

/*
je_graphic_framebuf_create [基本接口]
使用指定的附件配置创建一个纹理缓冲区资源
    * 图形资源被创建时，其自动获取一个引用计数；使用 jegl_share_resource可增加引
        用计数，使用 je_graphic_framebuf_close 可减少引用计数；当资源的引用计数变为 0 时，
        资源将被释放
    * 使用此接口创建的顶点，其对应的资源路径为 nullptr
请参见：
    jegl_share_resource
    je_graphic_framebuf_close
*/
JE_API /* OPTIONAL: If size or color_attachment_count is zero */ je_gl_frame_buffer_t* je_graphic_framebuf_create(
    size_t width,
    size_t height,
    const je_gl_texture_format_t* color_attachment_formats,
    size_t color_attachment_count,
    bool contain_depth_attachment);

/*
je_graphic_framebuf_close [基本接口]
关闭一个纹理缓冲区资源（使得其引用计数减少），如果资源被 jegl_share_resource 引用过多次，
那么需要对应调用多次才能真正释放资源；
    * 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用；对应的底层图形资
        源将由资源对应的图形线程上下文在适当的时机释放。
请参见：
    jegl_share_resource
*/
JE_API void je_graphic_framebuf_close(je_gl_frame_buffer_t* framebuffer);

///////////////////////////// UNIFORM BUFFER /////////////////////////////

/*
je_graphic_uniformbuf_create [基本接口]
创建一个指定大小和绑定位置的一致变量缓冲区资源
    * 创建后的缓冲区内容不保证初始化（各图形后端表现不一致，不得假定为全 0）；
        必须通过 je_graphic_cmd_update_uniform_buffer 录制完整数据后再使用，并且
        在绑定它的每一帧都录制一次
    * 图形资源被创建时，其自动获取一个引用计数；使用 jegl_share_resource可增加引
        用计数，使用 je_graphic_uniformbuf_close 可减少引用计数；当资源的引用计数变为 0 时，
        资源将被释放
    * 使用此接口创建的顶点，其对应的资源路径为 nullptr
请参见：
    jegl_share_resource
    je_graphic_uniformbuf_close
    je_graphic_cmd_update_uniform_buffer
*/
JE_API je_gl_uniform_buffer_t* je_graphic_uniformbuf_create(
    size_t binding_place,
    size_t length);

/*
je_graphic_uniformbuf_close [基本接口]
关闭一个一致变量缓冲区资源（使得其引用计数减少），如果资源被 jegl_share_resource 引用过多次，
那么需要对应调用多次才能真正释放资源；
    * 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用；对应的底层图形资
        源将由资源对应的图形线程上下文在适当的时机释放。
请参见：
    jegl_share_resource
*/
JE_API void je_graphic_uniformbuf_close(je_gl_uniform_buffer_t* uniformbuf);

//////////////////////////////////////////////////////////

typedef struct je_stb_font_data_t je_stb_font_data_t;
typedef void (*je_font_char_updater_t)(je_gl_texture_pixel_data_t*, size_t, size_t);

typedef struct je_font_t
{
    const char* m_path;
    uint8_t* m_font_file_buf;

    size_t m_board_size_x;
    size_t m_board_size_y;
    je_font_char_updater_t m_updater;

    int32_t m_ascent;
    int32_t m_descent;
    int32_t m_line_gap;
    int32_t m_line_space;

    je_stb_font_data_t* m_stb_font_data;
} je_font_t;

/*
je_font_load [基本接口]
加载一个字体
    * 字体对象与字号解耦：加载时不需要指定字号，字号在 je_font_char_get 调用时给出
board_blank_size_x、board_blank_size_y用于指示文字纹理的横线和纵向预留空间
char_texture_updater 用于指示文字纹理创建后所需的预处理方法，不需要可以指定为nullptr
使用完毕之后的字体需要使用je_font_free关闭
请参见：
    je_font_free
    je_font_char_get
*/
JE_API /* OPTIONAL */ je_font_t* je_font_load(
    const char* font_path,
    size_t board_blank_size_x,
    size_t board_blank_size_y,
    /* OPTIONAL */ je_font_char_updater_t char_texture_updater);

/*
je_font_free [基本接口]
关闭一个字体
*/
JE_API void je_font_free(je_font_t* font);

/*
je_font_char_get [基本接口]
以指定的字号（单位像素）从字体中加载指定的一个字符的纹理及其他信息
    * 同一字号与字符的字形会被缓存，重复获取直接返回缓存结果
请参见：
    jeecs::graphic::character
*/
JE_API const jeecs::graphic::character* je_font_char_get(
    je_font_t* font,
    float size,
    char32_t unicode32_char);

/*
je_graphic_shared_resources_mark_outdated [基本接口]
将指定路径对应的共享资源缓存标记为过时的，若成功标记，返回true
    * 标记成功：指的是之前此资源存在，并非过时的，在此次操作之后被标记
    * 这将使得下次加载此资源时更新资源，对于已经加载的资源没有影响
请参见：
    je_graphic_shared_resources_mark_outdated
*/
JE_API bool je_graphic_shared_resources_mark_outdated(
    je_gl_context_t* context, const char* path);

/*
je_gl_graphic_api_entry_t [类型]
基础图形库的入口类型
指向一个用于初始化基础图形接口的入口函数
*/
typedef void (*je_gl_graphic_api_entry_t)(je_gl_graphic_api_t*);

/*
je_graphic_host_api_set [基本接口]
设置使用的图形库
    * 在图形线程启动前或结束后设置生效
    * 若不设置或设置为 nullptr，引擎将使用默认的图形接口、
    * 请在je_core_init之后设置，引擎的初始化操作将重置此设置
*/
JE_API void je_graphic_host_api_set(/* OPTIONAL: nullptr to use platform default */ je_gl_graphic_api_entry_t api);

/*
je_graphic_host_api_get [基本接口]
获取使用的图形库
    * 若尚未使用 je_graphic_host_api_set 设置图形库接口，
        则返回引擎根据平台决定使用的默认使用的图形库
    * 否则返回 je_graphic_host_api_set 设置的接口
*/
JE_API je_gl_graphic_api_entry_t je_graphic_host_api_get(void);

/*
je_graphic_using_none_apis [基本接口]
加载JoyEngine基础图形接口的空实现，通常与je_graphic_thread_start一起使用用于指定图形线程使
用的基本图形库。
请参见：
    je_graphic_thread_start
*/
JE_API void je_graphic_using_none_apis(je_gl_graphic_api_t* write_to_apis);

/*
je_graphic_using_opengl3_apis [基本接口]
加载OpenGL 3.3 或 OpenGLES 3.0 API集合，通常与je_graphic_thread_start一起使用以指定图形线程
使用的基本图形实现。
请参见：
    je_graphic_thread_start
*/
JE_API void je_graphic_using_opengl3_apis(je_gl_graphic_api_t* write_to_apis);

/*
je_graphic_using_vk120_apis [基本接口]
加载Vulkan API v1.2集合，通常与je_graphic_thread_start一起使用以指定图形线程使用的基本图形
实现。
请参见：
    je_graphic_thread_start
*/
JE_API void je_graphic_using_vk120_apis(je_gl_graphic_api_t* write_to_apis);

/*
je_graphic_using_metal_apis [基本接口]
加载Metal API集合，通常与je_graphic_thread_start一起使用以指定图形线程使用的基本图形
实现。
请参见：
    je_graphic_thread_start
*/
JE_API void je_graphic_using_metal_apis(je_gl_graphic_api_t* write_to_apis);

/*
je_graphic_using_dx11_apis [基本接口]
加载directx 11 API集合，通常与je_graphic_thread_start一起使用以指定图形线程使用的基本图形
实现。
请参见：
    je_graphic_thread_start
*/
JE_API void je_graphic_using_dx11_apis(je_gl_graphic_api_t* write_to_apis);

///////////////////////////// RENDER API (V2) /////////////////////////////

/*
新渲染 API 的设计约定：
    * 强类型：pipeline/sampler/command_buffer 都是独立的 opaque 类型
    * 空安全：必填参数在实现中以断言约束；确实可为空的参数在类型前标注 OPTIONAL
    * 不做防御式回退：违反前置条件属于未定义行为（Debug 构建断言终止），
      不会静默跳过或自动钳制
    * 可移植地板：OpenGL 3.3 / OpenGL ES 3.0 / WebGL 2.0
      （不使用 SSBO、compute、geometry、indirect/multi-draw、base-vertex、
       persistent mapping、无绑定纹理）
*/

/*
je_gl_topology_t [类型]
图元拓扑；与 pipeline 绑定，绘制时使用 pipeline 的拓扑
*/
enum class je_gl_topology_t : uint8_t
{
    POINTS,
    LINES,
    LINE_STRIP,
    TRIANGLES,
    TRIANGLE_STRIP,
};

/*
je_gl_front_face_t [类型]
正面绕序。后端会根据渲染目标（屏幕/离屏）自行处理 NDC 翻转。
*/
enum class je_gl_front_face_t : uint8_t
{
    COUNTER_CLOCKWISE,
    CLOCKWISE,
};

/*
je_gl_pipeline_state_t [类型]
pipeline 的固定功能状态。所有字段都必须显式初始化；
如需基于 shader 的 ZTEST/ZWRITE/BLEND/CULL 配置，请使用
je_graphic_pipeline_state_from_shader 或向 je_graphic_pipeline_create 传入空的 state。
*/
typedef struct je_gl_pipeline_state_t
{
    je_gl_topology_t m_topology;

    bool m_depth_test;
    je_gl_shader_depth_test_method_t m_depth_func;
    bool m_depth_write;

    bool m_blend;
    je_gl_shader_blend_equation_t m_blend_equation;
    je_gl_shader_blend_method_t m_blend_src_color;
    je_gl_shader_blend_method_t m_blend_dst_color;
    je_gl_shader_blend_method_t m_blend_src_alpha;
    je_gl_shader_blend_method_t m_blend_dst_alpha;

    je_gl_shader_cull_mode_t m_cull;
    je_gl_front_face_t m_front_face;

    // bit0..3 = R,G,B,A；0xF 表示全部写入
    uint8_t m_color_write_mask;
} je_gl_pipeline_state_t;

/*
je_gl_pipeline_desc_t [类型]
pipeline 的创建描述
    * m_shader 不得为空，pipeline 会持有其引用
    * m_state 为空时，使用 shader 的 ZTEST/ZWRITE/BLEND/CULL 配置
请参见：
    je_graphic_pipeline_create
*/
typedef struct je_gl_pipeline_desc_t
{
    je_gl_shader_t* m_shader;
    /* OPTIONAL: nullptr 表示使用 shader 的 ZTEST/ZWRITE/BLEND/CULL 配置 */
    const je_gl_pipeline_state_t* m_state;
} je_gl_pipeline_desc_t;

/*
je_gl_uniform_slot_t [类型]
单次绘制的 uniform 在 shader uniform block 内的规范化位置。
    * m_offset == JE_GL_UNIFORM_SLOT_NONE 表示 shader 未声明该 uniform
    * 通过 je_graphic_pipeline_uniform_slot / je_graphic_pipeline_builtin_uniform_slots 获取一次后缓存
*/
#define JE_GL_UNIFORM_SLOT_NONE ((uint32_t)0xFFFFFFFFu)

typedef struct je_gl_uniform_slot_t
{
    uint32_t m_offset;
    uint32_t m_size;
} je_gl_uniform_slot_t;

/*
je_gl_builtin_uniform_slots_t [类型]
pipeline 在创建时缓存的内建 uniform 位置表
    * 未在 shader 中声明的项，其 m_offset 为 JE_GL_UNIFORM_SLOT_NONE
*/
typedef struct je_gl_builtin_uniform_slots_t
{
    je_gl_uniform_slot_t m_model;
    je_gl_uniform_slot_t m_model_view;
    je_gl_uniform_slot_t m_model_view_projection;

    je_gl_uniform_slot_t m_local_scale;
    je_gl_uniform_slot_t m_uv_tiling;
    je_gl_uniform_slot_t m_uv_offset;
    je_gl_uniform_slot_t m_color;

    je_gl_uniform_slot_t m_light2d_resolution;
    je_gl_uniform_slot_t m_light2d_decay;
    je_gl_uniform_slot_t m_shadow_ellipse;

    // DeferLight2D 阴影逐像素排序：x=key，y=缩放，z=偏移，w=模式位。
    // 仅 shadow pass 的 shader 声明该 uniform，其余 shader 解析为 NONE。
    je_gl_uniform_slot_t m_shadow_key;
} je_gl_builtin_uniform_slots_t;

/*
je_graphic_pipeline_create [基本接口]
创建一个 pipeline：shader + 固定功能状态 + 顶点布局兼容性。
    * 返回的 pipeline 持有 shader 的引用
    * 必须在对应图形线程的 context 生命周期内使用
*/
JE_API je_gl_pipeline_t* /* NOT NULL */ je_graphic_pipeline_create(
    je_gl_context_t* context, const je_gl_pipeline_desc_t* desc);

/*
je_graphic_pipeline_close [基本接口]
关闭一个 pipeline 资源（使得其引用计数减少），如果资源被 jegl_share_resource
引用过多次，那么需要对应调用多次才能真正释放资源
    * 若此操作使得引用计数变为 0，pipeline 持有的 shader 引用随之释放，
        对应的后端对象将由资源所属图形线程在适当的时机销毁
请参见：
    je_graphic_pipeline_create
    jegl_share_resource
*/
JE_API void je_graphic_pipeline_close(je_gl_pipeline_t* pipeline);

/*
je_graphic_pipeline_state_from_shader [基本接口]
用 shader 的 ZTEST/ZWRITE/BLEND/CULL 配置填充一份 pipeline 状态。
*/
JE_API je_gl_pipeline_state_t je_graphic_pipeline_state_from_shader(
    je_gl_shader_t* shader);

/*
je_graphic_pipeline_uniform_slot [基本接口]
按名字查询 per-draw uniform 的规范化位置。
    * 返回值的 m_offset == JE_GL_UNIFORM_SLOT_NONE 表示未声明
    * name 不得为空
*/
JE_API /* OPTIONAL: m_offset == JE_GL_UNIFORM_SLOT_NONE if absent */
je_gl_uniform_slot_t je_graphic_pipeline_uniform_slot(
    je_gl_pipeline_t* pipeline, const char* name);

/*
je_graphic_pipeline_builtin_uniform_slots [基本接口]
返回 pipeline 缓存的内建 uniform 位置表；指针由 pipeline 持有，生命周期同 pipeline。
*/
JE_API const je_gl_builtin_uniform_slots_t* /* NOT NULL */
je_graphic_pipeline_builtin_uniform_slots(je_gl_pipeline_t* pipeline);

/*
je_gl_filter_t / je_gl_mip_filter_t [类型]
采样器的缩小/放大过滤与 mipmap 过滤模式
*/
enum class je_gl_filter_t : uint8_t
{
    NEAREST,
    LINEAR,
};

enum class je_gl_mip_filter_t : uint8_t
{
    NEAREST,
    LINEAR,
};

/*
je_gl_wrap_mode_t [类型]
采样器在 U/V/W 方向上的寻址模式
*/
enum class je_gl_wrap_mode_t : uint8_t
{
    CLAMP_TO_EDGE,
    REPEAT,
    MIRRORED_REPEAT,
};

/*
je_gl_sampler_desc_t [类型]
独立采样器的创建描述，所有字段都必须显式初始化
    * 未通过 je_graphic_cmd_bind_sampler 显式绑定时，使用 shader 声明的默认采样器
请参见：
    je_graphic_sampler_create
*/
typedef struct je_gl_sampler_desc_t
{
    je_gl_filter_t m_min_filter;
    je_gl_filter_t m_mag_filter;
    je_gl_mip_filter_t m_mip_filter;
    je_gl_wrap_mode_t m_u_wrap;
    je_gl_wrap_mode_t m_v_wrap;
    je_gl_wrap_mode_t m_w_wrap;
} je_gl_sampler_desc_t;

/*
je_graphic_sampler_create [基本接口]
根据描述创建一个独立采样器
    * 返回的采样器持有引用计数，需要使用 je_graphic_sampler_close 减少引用计数
    * 未通过 je_graphic_cmd_bind_sampler 显式绑定时，使用 shader 声明的默认采样器
请参见：
    je_gl_sampler_desc_t
    je_graphic_sampler_close
    je_graphic_cmd_bind_sampler
*/
JE_API je_gl_sampler_t* /* NOT NULL */ je_graphic_sampler_create(
    je_gl_context_t* context, const je_gl_sampler_desc_t* desc);

/*
je_graphic_sampler_close [基本接口]
关闭一个采样器（使得其引用计数减少），如果资源被 jegl_share_resource 引用过多次，
那么需要对应调用多次才能真正释放资源
    * 若此操作使得引用计数变为 0，对应的后端对象将由资源所属图形线程在适当的
        时机销毁
请参见：
    je_graphic_sampler_create
    jegl_share_resource
*/
JE_API void je_graphic_sampler_close(je_gl_sampler_t* sampler);

/*
je_gl_command_buffer_t [类型]
在任意线程录制、在图形线程提交的绘制命令缓冲区。
    * reset 后可以重复使用；begin_pass 之前不需要额外的 begin
    * commit 只允许在创建时指定的图形线程中调用
*/

/*
je_graphic_cmd_create [基本接口]
创建一个命令缓冲区，并绑定到指定的图形上下文
    * 命令录制可以在任意线程进行；je_graphic_cmd_commit 只允许在 context 所属的
        图形线程调用
    * 创建出的命令缓冲区需要使用 je_graphic_cmd_close 关闭
请参见：
    je_graphic_cmd_close
    je_graphic_cmd_reset
    je_graphic_cmd_commit
*/
JE_API je_gl_command_buffer_t* /* NOT NULL */ je_graphic_cmd_create(
    je_gl_context_t* context);

/*
je_graphic_cmd_close [基本接口]
关闭一个命令缓冲区，并释放其录制期持有的全部资源引用
    * 尚未提交的录制内容会被直接丢弃
请参见：
    je_graphic_cmd_create
*/
JE_API void je_graphic_cmd_close(je_gl_command_buffer_t* cmd);

/*
je_graphic_cmd_reset [基本接口]
重置命令缓冲区以便复用：清空已录制的命令，并释放录制期持有的全部资源引用
    * 重置会直接丢弃仍未提交的内容（包括尚未结束的 pass），不会报错
请参见：
    je_graphic_cmd_create
    je_graphic_cmd_commit
*/
JE_API void je_graphic_cmd_reset(je_gl_command_buffer_t* cmd);

/*
je_graphic_cmd_get_context [基本接口]
返回创建命令缓冲区时绑定的图形上下文
    * 该上下文也是唯一允许调用 je_graphic_cmd_commit 的图形线程
请参见：
    je_graphic_cmd_create
    je_graphic_cmd_commit
*/
JE_API je_gl_context_t* /* NOT NULL */ je_graphic_cmd_get_context(
    je_gl_command_buffer_t* cmd);

/*
je_graphic_cmd_begin_pass [基本接口]
开始一个渲染 pass。target 为空表示屏幕后备缓冲区。
    * 不允许嵌套 pass
    * viewport 的 width/height 必须大于 0
*/
JE_API void je_graphic_cmd_begin_pass(
    je_gl_command_buffer_t* cmd,
    /* OPTIONAL: nullptr 表示屏幕后备缓冲区 */ je_gl_frame_buffer_t* target,
    const je_gl_viewport_t* viewport);

/*
je_graphic_cmd_clear_color / je_graphic_cmd_clear_depth [基本接口]
只能在 begin_pass 之后、该 pass 的第一次 draw 之前调用。
*/
JE_API void je_graphic_cmd_clear_color(
    je_gl_command_buffer_t* cmd, uint32_t attachment, const float rgba[4]);
JE_API void je_graphic_cmd_clear_depth(
    je_gl_command_buffer_t* cmd, float depth);

/*
je_graphic_cmd_end_pass [基本接口]
结束当前渲染 pass
    * 必须与 je_graphic_cmd_begin_pass 配对；未显式调用时 je_graphic_cmd_commit 会自动结束
请参见：
    je_graphic_cmd_begin_pass
    je_graphic_cmd_commit
*/
JE_API void je_graphic_cmd_end_pass(je_gl_command_buffer_t* cmd);

/*
je_graphic_cmd_bind_pipeline [基本接口]
为后续绘制绑定 pipeline
    * 必须在 pass 内调用
    * pipeline 必须与命令缓冲区属于同一个图形上下文
    * 绑定 pipeline 会按 shader 的默认值重新初始化 per-draw uniform 块
请参见：
    je_graphic_pipeline_create
    je_graphic_cmd_set_uniform
*/
JE_API void je_graphic_cmd_bind_pipeline(
    je_gl_command_buffer_t* cmd, je_gl_pipeline_t* pipeline);

/*
je_graphic_cmd_bind_vertex [基本接口]
为后续绘制绑定顶点（模型）资源
    * 必须在 pass 内调用；draw 之前必须同时绑定 pipeline 和 vertex
请参见：
    je_graphic_cmd_bind_pipeline
    je_graphic_cmd_draw
    je_graphic_cmd_draw_indexed
*/
JE_API void je_graphic_cmd_bind_vertex(
    je_gl_command_buffer_t* cmd, je_gl_vertex_t* vertex);

/*
je_graphic_cmd_bind_uniform_buffer [基本接口]
为后续绘制绑定一致变量缓冲区
    * 必须在 pass 内调用
    * 使用 buffer 创建时指定的 binding place，避免绑定位置不一致
请参见：
    je_graphic_uniformbuf_create
    je_graphic_cmd_update_uniform_buffer
*/
JE_API void je_graphic_cmd_bind_uniform_buffer(
    je_gl_command_buffer_t* cmd, je_gl_uniform_buffer_t* buffer);

/*
je_graphic_cmd_bind_texture_2d / je_graphic_cmd_bind_texture_3d / je_graphic_cmd_bind_texture_array /
je_graphic_cmd_bind_texture_cube [基本接口]
将指定类别的纹理绑定到 shader 的纹理声明（pass）上
    * 必须在 pass 内调用
    * pass 取自 shader 反射（je_gl_shader_t::m_sampler_methods）
    * 同一 pass 重复绑定会覆盖此前的纹理
请参见：
    je_graphic_cmd_bind_sampler
    je_gl_shader_t
*/
JE_API void je_graphic_cmd_bind_texture_2d(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_2d_t* texture);
JE_API void je_graphic_cmd_bind_texture_3d(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_3d_t* texture);
JE_API void je_graphic_cmd_bind_texture_array(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_array_t* texture);
JE_API void je_graphic_cmd_bind_texture_cube(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_cube_t* texture);

/*
je_graphic_cmd_bind_sampler [基本接口]
为指定的采样器声明绑定独立采样器
    * 必须在 pass 内且已绑定 pipeline 后调用
    * sampler_id 取自 shader 反射（je_gl_shader_t::m_sampler_methods）
    * 未绑定采样器的声明使用 shader 的默认采样器
请参见：
    je_graphic_sampler_create
    je_graphic_cmd_bind_texture_2d
*/
JE_API void je_graphic_cmd_bind_sampler(
    je_gl_command_buffer_t* cmd, uint32_t sampler_id, je_gl_sampler_t* sampler);

/*
je_graphic_cmd_update_vertex [基本接口]
录制一个顶点的「本帧内容」；提交命令时由图形后端落实，只对本次提交的绘制生效
    * vertices 与 vertex_length 描述一份完整的顶点前缀数据（不得超过资源创建时的
        顶点数据容量）；本命令只定义该前缀，前缀之外的顶点内容未定义，
        active_index_count 范围内的索引不得引用它们
    * vertices 传入 nullptr（此时 vertex_length 必须为 0）时只调整激活的索引数量，
        不算作一次内容录制
    * active_index_count 为本次更新后实际参与绘制的索引数量（不能超过资源创建时
        的索引总数）；传入 0 时会被钳制为 1——0 长度的绘制调用在部分后端上非法，
        而 1 个索引的 TRIANGLES 绘制不产生任何图元
    * 本命令不需要 pass，也不要求先绑定该资源；录制时同步资源的激活索引数量
        （绘制数量属于 CPU 侧状态，仅录制线程读取）
    * 带动态内容的缓冲必须在绘制它的每一帧都录制一次内容：没有录制内容的帧，
        绑定到的顶点内容未定义（取决于图形后端，可能是创建期内容，也可能是此前
        某次更新的内容），不得依赖。从未录制过内容的缓冲始终绑定创建期内容
请参见：
    je_graphic_vertex_create
    je_graphic_branch_new_update_command
*/
JE_API void je_graphic_cmd_update_vertex(
    je_gl_command_buffer_t* cmd,
    je_gl_vertex_t* vertex,
    /* OPTIONAL: nullptr 时只调整激活索引数量 */ const void* vertices,
    size_t vertex_length,
    size_t active_index_count);

/*
je_graphic_cmd_update_uniform_buffer [基本接口]
录制一个一致变量缓冲区的「本帧完整内容」；提交命令时由图形后端落实
    * size 必须等于创建时指定的一致变量缓冲区长度
    * 本命令不需要 pass，也不要求先绑定该资源
    * 必须在绑定它的每一帧都录制一次内容：没有录制更新的帧，缓冲区内容未定义
        （取决于图形后端，可能是此前某次更新的内容，也可能是未初始化的数据），
        不得依赖
请参见：
    je_graphic_uniformbuf_create
    je_graphic_branch_new_update_command
*/
JE_API void je_graphic_cmd_update_uniform_buffer(
    je_gl_command_buffer_t* cmd,
    je_gl_uniform_buffer_t* buffer,
    const void* data,
    size_t size);

/*
je_graphic_cmd_upload_texture_2d / je_graphic_cmd_upload_texture_3d /
je_graphic_cmd_upload_texture_array / je_graphic_cmd_upload_texture_cube [基本接口]
录制一个持久纹理区段上传；提交命令时由图形后端落实
    * 写入的是纹理自身的持久存储，后续帧持续可见（资产语义，不同于顶点/UBO 的
        本帧内容）；图形后端负责与仍在执行的帧同步，可能因此等待一次在飞帧
    * 区段越界、或对不含 CPU 像素的格式（FLOAT16 / DEPTH / FRAMEBUF）上传均属
        调用错误，由断言检查
    * 像素数据行序与创建期快照一致（自底向上）；数据按区段行连续排布：
        - 2D：height 行
        - 3D：depth 个 2D 区段
        - array：layer 起连续 layers 个 2D 区段
        - cube：face 指定面的 height 行
    * MSAA 纹理仅存在于 GPU 侧，不支持上传
请参见：
    je_graphic_texture_2d_create
    je_graphic_branch_new_update_command
*/
JE_API void je_graphic_cmd_upload_texture_2d(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_2d_t* texture,
    size_t x, size_t y,
    size_t width, size_t height,
    const je_gl_texture_pixel_data_t* pixels);
JE_API void je_graphic_cmd_upload_texture_3d(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_3d_t* texture,
    size_t x, size_t y, size_t z,
    size_t width, size_t height, size_t depth,
    const je_gl_texture_pixel_data_t* pixels);
JE_API void je_graphic_cmd_upload_texture_array(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_array_t* texture,
    size_t x, size_t y, size_t layer,
    size_t width, size_t height, size_t layers,
    const je_gl_texture_pixel_data_t* pixels);
JE_API void je_graphic_cmd_upload_texture_cube(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_cube_t* texture,
    size_t face,
    size_t x, size_t y,
    size_t width, size_t height,
    const je_gl_texture_pixel_data_t* pixels);

/*
je_graphic_cmd_set_uniform [基本接口]
设置当前 pipeline 的 per-draw uniform。
    * 必须先绑定 pipeline
    * slot.m_offset 不得为 JE_GL_UNIFORM_SLOT_NONE
    * size 必须等于 slot.m_size；原始接口要求 data 非空
*/
JE_API void je_graphic_cmd_set_uniform(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot,
    const void* data, uint32_t size);

/*
je_graphic_cmd_set_uniform_float / je_graphic_cmd_set_uniform_float2 /
je_graphic_cmd_set_uniform_float3 / je_graphic_cmd_set_uniform_float4 [基本接口]
以标量或浮点向量的形式设置当前 pipeline 的 per-draw uniform
    * 参数约定与 je_graphic_cmd_set_uniform 一致：必须先绑定 pipeline，
        slot 必须有效且大小匹配
请参见：
    je_graphic_cmd_set_uniform
*/
JE_API void je_graphic_cmd_set_uniform_float(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float value);
JE_API void je_graphic_cmd_set_uniform_float2(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float x, float y);
JE_API void je_graphic_cmd_set_uniform_float3(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float x, float y, float z);
JE_API void je_graphic_cmd_set_uniform_float4(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float x, float y, float z, float w);

/*
je_graphic_cmd_set_uniform_int / je_graphic_cmd_set_uniform_int2 /
je_graphic_cmd_set_uniform_int3 / je_graphic_cmd_set_uniform_int4 [基本接口]
以标量或整数向量的形式设置当前 pipeline 的 per-draw uniform
    * 参数约定与 je_graphic_cmd_set_uniform 一致：必须先绑定 pipeline，
        slot 必须有效且大小匹配
请参见：
    je_graphic_cmd_set_uniform
*/
JE_API void je_graphic_cmd_set_uniform_int(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, int value);
JE_API void je_graphic_cmd_set_uniform_int2(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, int x, int y);
JE_API void je_graphic_cmd_set_uniform_int3(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, int x, int y, int z);
JE_API void je_graphic_cmd_set_uniform_int4(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, int x, int y, int z, int w);

/*
je_graphic_cmd_set_uniform_float2x2 / je_graphic_cmd_set_uniform_float3x3 /
je_graphic_cmd_set_uniform_float4x4 [基本接口]
以矩阵的形式设置当前 pipeline 的 per-draw uniform
    * 矩阵按连续内存写入 uniform 块；float3x3 的每个 3 分量向量按 std140
        规则占用 16 字节
    * 参数约定与 je_graphic_cmd_set_uniform 一致：必须先绑定 pipeline，
        slot 必须有效且大小匹配
请参见：
    je_graphic_cmd_set_uniform
*/
JE_API void je_graphic_cmd_set_uniform_float2x2(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, const float (*mat)[2]);
JE_API void je_graphic_cmd_set_uniform_float3x3(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, const float (*mat)[3]);
JE_API void je_graphic_cmd_set_uniform_float4x4(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, const float (*mat)[4]);

/*
je_graphic_cmd_draw_indexed / je_graphic_cmd_draw [基本接口]
在当前 pipeline/vertex/绑定状态下发出绘制。
    * index_count/vertex_count/instance_count 必须大于 0
    * 索引类型固定为 uint32_t
*/
JE_API void je_graphic_cmd_draw_indexed(
    je_gl_command_buffer_t* cmd,
    uint32_t index_count, uint32_t first_index, uint32_t instance_count);
JE_API void je_graphic_cmd_draw(
    je_gl_command_buffer_t* cmd,
    uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count);

/*
je_graphic_cmd_commit [基本接口]
在图形线程提交命令缓冲区。
    * 若仍有未结束的 pass，commit 会自动结束它
    * 没有任何命令的命令缓冲区会被直接忽略（不触发后端提交）
    * 提交完成后命令缓冲区保留已录制的命令，需要复用请先调用 je_graphic_cmd_reset
*/
JE_API void je_graphic_cmd_commit(je_gl_command_buffer_t* cmd);

/*
je_graphic_branch_new_command_buffer [基本接口]
从绘制组申请一个已经 reset 的命令缓冲区；屏幕目标传 nullptr。
*/
JE_API je_gl_command_buffer_t* /* NOT NULL */ je_graphic_branch_new_command_buffer(
    je_render_branch_t* branch,
    /* OPTIONAL: nullptr 表示屏幕 */ je_gl_frame_buffer_t* framebuffer,
    const je_gl_viewport_t* viewport);

/*
je_graphic_branch_new_update_command [基本接口]
从绘制组申请一个已经 reset、且不开启任何 pass 的命令缓冲区，用于录制资源更新
（je_graphic_cmd_update_* / je_graphic_cmd_upload_texture_*）
    * 更新命令缓冲只承载资源更新，不会开始/结束绘制 pass；宿主的帧任务会把这类
        空闲 pass 的命令缓冲提交，即使这一帧窗口被跳过（更新不丢失）
    * 建议用宿主支持的最低优先级（INT32_MIN）调用 je_graphic_branch_new_frame，
        使更新先于本帧所有绘制链提交
请参见：
    je_graphic_cmd_update_vertex
    je_graphic_cmd_update_uniform_buffer
    je_graphic_cmd_upload_texture_2d
    je_graphic_branch_new_frame
*/
JE_API je_gl_command_buffer_t* /* NOT NULL */ je_graphic_branch_new_update_command(
    je_render_branch_t* branch);

/*
je_graphic_uhost_get_or_create_for_universe [基本接口]
获取或创建指定Universe的可编程图形上下文接口 uhost
    * config 被用于指示图形配置，若首次创建图形接口则使用此设置，
    若图形接口已经创建，则调用je_graphic_thread_reboot以应用图形配置
    * 若需要使用默认配置或不改变图形设置，请传入 nullptr
    * 创建出的 uhost 将通过 je_ecs_universe_exit_callback_register 接口注册
    Universe 的退出回调，以便在 Universe 退出时释放 uhost 实例
请参见：
    je_graphic_thread_reboot
    je_ecs_universe_exit_callback_register
*/
JE_API je_graphic_uhost_t* je_graphic_uhost_get_or_create_for_universe(
    je_game_universe_t* universe,
    /* OPTIONAL: nullptr to use default config */ const je_gl_interface_config_t* config);

/*
je_graphic_uhost_get_context [基本接口]
从指定的可编程图形上下文接口获取图形线程的正式描述符
*/
JE_API /* OPTIONAL: If graphic thread failed to start up */ je_gl_context_t* je_graphic_uhost_get_context(
    je_graphic_uhost_t* host);

/*
je_graphic_uhost_set_skip_behavior [基本接口]
设置图形实现请求跳过这一帧时，uhost的绘制动作行为
    * skip_all_draw 为真时，跳过全部绘制动作；反之只跳过以屏幕缓冲区为目标的动作
    * uhost 实例创建时默认为真
*/
JE_API void je_graphic_uhost_set_skip_behavior(
    je_graphic_uhost_t* host, bool skip_all_draw);

/*
je_graphic_uhost_alloc_branch [基本接口]
从指定的可编程图形上下文接口申请一个绘制组
    * 所有申请出的绘制组都需要在对应 uhost 关闭之前，通过 je_graphic_uhost_free_branch 释放
请参见：
    je_graphic_uhost_free_branch
*/
JE_API je_render_branch_t* je_graphic_uhost_alloc_branch(
    je_graphic_uhost_t* host);

/*
je_graphic_uhost_free_branch [基本接口]
从指定的可编程图形上下文接口释放一个绘制组
*/
JE_API void je_graphic_uhost_free_branch(
    je_graphic_uhost_t* host,
    je_render_branch_t* free_branch);

/*
je_graphic_branch_new_frame [基本接口]
在绘制开始之前，指示绘制组开始新的一帧，并指定优先级
*/
JE_API void je_graphic_branch_new_frame(
    je_render_branch_t* branch,
    int priority);
/*
je_image_texture_color_space_t [类型]
图片纹理的颜色空间。图片加载时按此决定 GPU 纹理格式：
    * SRGB   —— 8 位 RGB 数据按 sRGB 编码存储，采样时由硬件转换为线性；
                这是未提供属性源文件时的默认值
    * LINEAR —— 8 位 RGB 数据按线性数值直接采样（法线图、遮罩等非颜色数据）
取值来自属性源文件 <图片路径>.je4texture（TOML，例如 color_space = "linear"），
随像素一同写入图片纹理缓存
请参见：
    je_image_texture_cache_open_or_create
*/
enum je_image_texture_color_space_t
{
    JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB = 0,
    JE_IMAGE_TEXTURE_COLOR_SPACE_LINEAR = 1,
};

/*
je_image_texture_cache_open_or_create [基本接口]
打开指定图片的纹理缓存 <path>.je4cache；缓存缺失或失效时调用内置缓存生成器重建
    * path 为图片路径；缓存包含纹理的颜色空间和解码后的 RGBA8 像素数据，
      加载纹理时可直接使用而无需再解码图片
    * 校验码为图片文件与属性源文件 <path>.je4texture 内容连续计算的 CRC64，
      两者任一改变都会使缓存失效；属性源文件是可选的
    * 图片与属性源文件都不存在（如导出的运行时只携带缓存）时跳过校验
    * 返回的句柄由调用方负责关闭（见 je_file_close）；无法生成时返回 nullptr
请参见：
    je_cache_file_load
    je_file_close
    je_graphic_texture_2d_load
*/
JE_API /* OPTIONAL */ je_file_t* je_image_texture_cache_open_or_create(
    const char* path);
typedef je_image_texture_color_space_t je_ImageTextureColorSpace;
typedef je_graphic_api_register_func_t je_GraphicApiRegisterFunc;
typedef je_graphic_sync_callback_func_t je_GraphicSyncCallbackFunc;
typedef je_gl_sync_state_t je_GlSyncState;
typedef je_gl_update_sync_mode_t je_GlUpdateSyncMode;
typedef je_gl_update_sync_callback_t je_GlUpdateSyncCallback;
typedef je_stb_font_data_t je_StbFontData;
typedef je_font_char_updater_t je_FontCharUpdater;
typedef je_font_t je_Font;
typedef je_gl_graphic_api_entry_t je_GlGraphicApiEntry;
typedef je_gl_topology_t je_GlTopology;
typedef je_gl_front_face_t je_GlFrontFace;
typedef je_gl_pipeline_state_t je_GlPipelineState;
typedef je_gl_pipeline_desc_t je_GlPipelineDesc;
typedef je_gl_uniform_slot_t je_GlUniformSlot;
typedef je_gl_builtin_uniform_slots_t je_GlBuiltinUniformSlots;
typedef je_gl_filter_t je_GlFilter;
typedef je_gl_mip_filter_t je_GlMipFilter;
typedef je_gl_wrap_mode_t je_GlWrapMode;
typedef je_gl_sampler_desc_t je_GlSamplerDesc;
JE_FORCE_CAPI_END

#ifdef __cplusplus
namespace jeecs
{
    namespace graphic
    {
        namespace requirements
        {
            template<typename T>
            concept basic_graphic_resource =
                std::same_as<T, je_gl_shader_t> ||
                std::same_as<T, je_gl_texture_2d_t> ||
                std::same_as<T, je_gl_texture_3d_t> ||
                std::same_as<T, je_gl_texture_array_t> ||
                std::same_as<T, je_gl_texture_cube_t> ||
                std::same_as<T, je_gl_texture_msaa_t> ||
                std::same_as<T, je_gl_vertex_t> ||
                std::same_as<T, je_gl_frame_buffer_t> ||
                std::same_as<T, je_gl_uniform_buffer_t> ||
                std::same_as<T, je_gl_pipeline_t> ||
                std::same_as<T, je_gl_sampler_t> ||
                std::same_as<T, je_animation_t>;
        }

        template<requirements::basic_graphic_resource T>
        class resource_basic
        {
            JECS_DISABLE_MOVE_AND_COPY(resource_basic);

            T* _m_resource;

        protected:
            resource_basic(T* res) noexcept
                : _m_resource(res)
            {
                assert(_m_resource != nullptr);
            }
            ~resource_basic()noexcept
            {
                if constexpr (std::is_same_v<T, je_gl_shader_t>)
                    je_graphic_shader_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_texture_2d_t>)
                    je_graphic_texture_2d_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_texture_3d_t>)
                    je_graphic_texture_3d_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_texture_array_t>)
                    je_graphic_texture_array_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_texture_cube_t>)
                    je_graphic_texture_cube_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_texture_msaa_t>)
                    je_graphic_texture_msaa_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_vertex_t>)
                    je_graphic_vertex_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_frame_buffer_t>)
                    je_graphic_framebuf_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_uniform_buffer_t>)
                    je_graphic_uniformbuf_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_pipeline_t>)
                    je_graphic_pipeline_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_gl_sampler_t>)
                    je_graphic_sampler_close(_m_resource);
                else if constexpr (std::is_same_v<T, je_animation_t>)
                    je_animation_close(_m_resource);
                else
                {
                    static_assert(sizeof(T) == 0,
                        "Unsupported graphic resource type.");
                }
            }

        public:
            inline T* resource() const noexcept
            {
                return _m_resource;
            }
        };
        class texture2d : public resource_basic<je_gl_texture_2d_t>
        {
            explicit texture2d(je_gl_texture_2d_t* res)
                : resource_basic(res)
            {
            }

        public:
            static std::optional<basic::resource<texture2d>> load(je_gl_context_t* context, const std::string& str)
            {
                je_gl_texture_2d_t* res = je_graphic_texture_2d_load(context, str.c_str());
                if (res != nullptr)
                    return basic::resource<texture2d>(new texture2d(res));
                return std::nullopt;
            }
            static basic::resource<texture2d> create(size_t width, size_t height, je_gl_texture_format_t format)
            {
                return create(width, height, format, nullptr);
            }
            // initial_pixels 是创建期像素快照（自底向上行序）；传 nullptr 表示不创建快照
            static basic::resource<texture2d> create(
                size_t width, size_t height, je_gl_texture_format_t format,
                /* OPTIONAL */ const void* initial_pixels)
            {
                je_gl_texture_2d_t* res = je_graphic_texture_2d_create(
                    width, height, format,
                    static_cast<const je_gl_texture_pixel_data_t*>(initial_pixels));

                // Create texture must be successfully.
                assert(res != nullptr);
                return basic::resource<texture2d>(new texture2d(res));
            }

            // 持久区段上传（见 je_graphic_cmd_upload_texture_2d）
            void upload(je_gl_command_buffer_t* cmd,
                size_t x, size_t y, size_t w, size_t h, const void* pixels) const
            {
                je_graphic_cmd_upload_texture_2d(
                    cmd, resource(), x, y, w, h,
                    static_cast<const je_gl_texture_pixel_data_t*>(pixels));
            }

            inline size_t height() const noexcept
            {
                return resource()->m_height;
            }
            inline size_t width() const noexcept
            {
                return resource()->m_width;
            }
            inline math::ivec2 size() const noexcept
            {
                return math::ivec2(
                    (int)resource()->m_width,
                    (int)resource()->m_height);
            }
        };

        class texture3d : public resource_basic<je_gl_texture_3d_t>
        {
            explicit texture3d(je_gl_texture_3d_t* res)
                : resource_basic(res)
            {
            }

        public:
            static basic::resource<texture3d> create(
                size_t width, size_t height, size_t depth, je_gl_texture_format_t format)
            {
                return create(width, height, depth, format, nullptr);
            }
            // initial_pixels 是创建期像素快照（按 宽度 x 高度 x 深度 连续排布）
            static basic::resource<texture3d> create(
                size_t width, size_t height, size_t depth, je_gl_texture_format_t format,
                /* OPTIONAL */ const void* initial_pixels)
            {
                je_gl_texture_3d_t* res = je_graphic_texture_3d_create(
                    width, height, depth, format,
                    static_cast<const je_gl_texture_pixel_data_t*>(initial_pixels));

                // Create texture must be successfully.
                assert(res != nullptr);
                return basic::resource<texture3d>(new texture3d(res));
            }

            // 持久区段上传（见 je_graphic_cmd_upload_texture_3d）
            void upload(je_gl_command_buffer_t* cmd,
                size_t x, size_t y, size_t z,
                size_t w, size_t h, size_t d, const void* pixels) const
            {
                je_graphic_cmd_upload_texture_3d(
                    cmd, resource(), x, y, z, w, h, d,
                    static_cast<const je_gl_texture_pixel_data_t*>(pixels));
            }

            inline size_t height() const noexcept { return resource()->m_height; }
            inline size_t width() const noexcept { return resource()->m_width; }
            inline size_t depth() const noexcept { return resource()->m_depth; }
        };

        class texture_array : public resource_basic<je_gl_texture_array_t>
        {
            explicit texture_array(je_gl_texture_array_t* res)
                : resource_basic(res)
            {
            }

        public:
            static basic::resource<texture_array> create(
                size_t width, size_t height, size_t layers, je_gl_texture_format_t format)
            {
                return create(width, height, layers, format, nullptr);
            }
            // initial_pixels 是创建期像素快照（按 宽度 x 高度 x 层数 连续排布）
            static basic::resource<texture_array> create(
                size_t width, size_t height, size_t layers, je_gl_texture_format_t format,
                /* OPTIONAL */ const void* initial_pixels)
            {
                je_gl_texture_array_t* res = je_graphic_texture_array_create(
                    width, height, layers, format,
                    static_cast<const je_gl_texture_pixel_data_t*>(initial_pixels));

                // Create texture must be successfully.
                assert(res != nullptr);
                return basic::resource<texture_array>(new texture_array(res));
            }

            // 持久区段上传（见 je_graphic_cmd_upload_texture_array）
            void upload(je_gl_command_buffer_t* cmd,
                size_t x, size_t y, size_t layer,
                size_t w, size_t h, size_t layers, const void* pixels) const
            {
                je_graphic_cmd_upload_texture_array(
                    cmd, resource(), x, y, layer, w, h, layers,
                    static_cast<const je_gl_texture_pixel_data_t*>(pixels));
            }

            inline size_t height() const noexcept { return resource()->m_height; }
            inline size_t width() const noexcept { return resource()->m_width; }
            inline size_t layers() const noexcept { return resource()->m_layers; }
        };

        class texture_cube : public resource_basic<je_gl_texture_cube_t>
        {
            explicit texture_cube(je_gl_texture_cube_t* res)
                : resource_basic(res)
            {
            }

        public:
            static basic::resource<texture_cube> create(
                size_t width, size_t height, je_gl_texture_format_t format)
            {
                return create(width, height, format, nullptr);
            }
            // initial_pixels 是创建期像素快照（按 6 x 宽度 x 高度 连续排布）
            static basic::resource<texture_cube> create(
                size_t width, size_t height, je_gl_texture_format_t format,
                /* OPTIONAL */ const void* initial_pixels)
            {
                je_gl_texture_cube_t* res = je_graphic_texture_cube_create(
                    width, height, format,
                    static_cast<const je_gl_texture_pixel_data_t*>(initial_pixels));

                // Create texture must be successfully.
                assert(res != nullptr);
                return basic::resource<texture_cube>(new texture_cube(res));
            }

            // 持久区段上传（见 je_graphic_cmd_upload_texture_cube）
            void upload(je_gl_command_buffer_t* cmd, size_t face,
                size_t x, size_t y, size_t w, size_t h, const void* pixels) const
            {
                je_graphic_cmd_upload_texture_cube(
                    cmd, resource(), face, x, y, w, h,
                    static_cast<const je_gl_texture_pixel_data_t*>(pixels));
            }

            inline size_t height() const noexcept { return resource()->m_height; }
            inline size_t width() const noexcept { return resource()->m_width; }
        };

        class texture_msaa : public resource_basic<je_gl_texture_msaa_t>
        {
            explicit texture_msaa(je_gl_texture_msaa_t* res)
                : resource_basic(res)
            {
            }

        public:
            static basic::resource<texture_msaa> create(
                size_t width, size_t height, size_t samples, je_gl_texture_format_t format)
            {
                je_gl_texture_msaa_t* res = je_graphic_texture_msaa_create(width, height, samples, format);

                // Create texture must be successfully.
                assert(res != nullptr);
                return basic::resource<texture_msaa>(new texture_msaa(res));
            }

            inline size_t height() const noexcept { return resource()->m_height; }
            inline size_t width() const noexcept { return resource()->m_width; }
            inline size_t samples() const noexcept { return resource()->m_samples; }
        };
        class shader : public resource_basic<je_gl_shader_t>
        {
        private:
            explicit shader(je_gl_shader_t* res)
                : resource_basic(res), m_builtin(nullptr)
            {
                m_builtin = &this->resource()->m_builtin_uniforms;
            }

        public:
            je_gl_shader_builtin_uniform_location_t* m_builtin;

            static std::optional<basic::resource<shader>> create(
                je_gl_context_t* context_may_null, const std::string& name_path, const std::string& src)
            {
                je_gl_shader_t* res = je_graphic_shader_source_load(context_may_null, name_path.c_str(), src.c_str(), true);
                if (res != nullptr)
                    return basic::resource<shader>(new shader(res));
                return std::nullopt;
            }
            static std::optional<basic::resource<shader>> load(je_gl_context_t* context, const std::string& src_path)
            {
                je_gl_shader_t* res = je_graphic_shader_load(context, src_path.c_str());
                if (res != nullptr)
                    return basic::resource<shader>(new shader(res));
                return std::nullopt;
            }

            void set_uniform(const std::string& name, int val) noexcept
            {
                auto* shader_instance = resource();

                auto* jegl_shad_uniforms = shader_instance->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            JE_GL_SHADER_UNIFORM_TYPE_INT)
                            debug::logerr(
                                "Trying set uniform('%s' = %d) to shader(%p), but current uniform type is not 'INT'.",
                                name.c_str(), val, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.m_int = val;

                            shader_instance->m_handle.m_modified =
                                jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, int x, int y) noexcept
            {
                auto* shader_instance = resource();

                auto* jegl_shad_uniforms = shader_instance->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            JE_GL_SHADER_UNIFORM_TYPE_INT2)
                            debug::logerr(
                                "Trying set uniform('%s' = %d, %d) to shader(%p), but current uniform type is not 'INT2'.",
                                name.c_str(), x, y, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.m_int2[0] = x;
                            jegl_shad_uniforms->m_value.m_int2[1] = y;

                            shader_instance->m_handle.m_modified =
                                jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, int x, int y, int z) noexcept
            {
                auto* shader_instance = resource();

                auto* jegl_shad_uniforms = shader_instance->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            JE_GL_SHADER_UNIFORM_TYPE_INT3)
                            debug::logerr(
                                "Trying set uniform('%s' = %d, %d, %d) to shader(%p), but current uniform type is not 'INT3'.",
                                name.c_str(), x, y, z, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.m_int3[0] = x;
                            jegl_shad_uniforms->m_value.m_int3[1] = y;
                            jegl_shad_uniforms->m_value.m_int3[2] = z;

                            shader_instance->m_handle.m_modified =
                                jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, int x, int y, int z, int w) noexcept
            {
                auto* shader_instance = resource();

                auto* jegl_shad_uniforms = shader_instance->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            JE_GL_SHADER_UNIFORM_TYPE_INT4)
                            debug::logerr(
                                "Trying set uniform('%s' = %d, %d, %d, %d) to shader(%p), but current uniform type is not 'INT4'.",
                                name.c_str(), x, y, z, w, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.m_int4[0] = x;
                            jegl_shad_uniforms->m_value.m_int4[1] = y;
                            jegl_shad_uniforms->m_value.m_int4[2] = z;
                            jegl_shad_uniforms->m_value.m_int4[3] = w;

                            shader_instance->m_handle.m_modified =
                                jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, float val) noexcept
            {
                auto* shader_instance = resource();

                auto* jegl_shad_uniforms = shader_instance->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            JE_GL_SHADER_UNIFORM_TYPE_FLOAT)
                            debug::logerr(
                                "Trying set uniform('%s' = %f) to shader(%p), but current uniform type is not 'FLOAT'.",
                                name.c_str(), val, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.m_float = val;

                            shader_instance->m_handle.m_modified =
                                jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec2& val) noexcept
            {
                auto* shader_instance = resource();

                auto* jegl_shad_uniforms = shader_instance->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            JE_GL_SHADER_UNIFORM_TYPE_FLOAT2)
                            debug::logerr(
                                "Trying set uniform('%s' = (%f, %f)) to shader(%p), but current uniform type is not 'FLOAT2'.",
                                name.c_str(), val.x, val.y, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.m_float2[0] = val.x;
                            jegl_shad_uniforms->m_value.m_float2[1] = val.y;

                            shader_instance->m_handle.m_modified =
                                jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec3& val) noexcept
            {
                auto* shader_instance = resource();

                auto* jegl_shad_uniforms = shader_instance->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            JE_GL_SHADER_UNIFORM_TYPE_FLOAT3)
                            debug::logerr(
                                "Trying set uniform('%s' = (%f, %f, %f)) to shader(%p), but current uniform type is not 'FLOAT3'.",
                                name.c_str(), val.x, val.y, val.z, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.m_float3[0] = val.x;
                            jegl_shad_uniforms->m_value.m_float3[1] = val.y;
                            jegl_shad_uniforms->m_value.m_float3[2] = val.z;

                            shader_instance->m_handle.m_modified =
                                jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec4& val) noexcept
            {
                auto* shader_instance = resource();

                auto* jegl_shad_uniforms = shader_instance->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            JE_GL_SHADER_UNIFORM_TYPE_FLOAT4)
                            debug::logerr(
                                "Trying set uniform('%s' = (%f, %f, %f, %f)) to shader(%p), but current uniform type is not 'FLOAT4'.",
                                name.c_str(), val.x, val.y, val.z, val.w, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.m_float4[0] = val.x;
                            jegl_shad_uniforms->m_value.m_float4[1] = val.y;
                            jegl_shad_uniforms->m_value.m_float4[2] = val.z;
                            jegl_shad_uniforms->m_value.m_float4[3] = val.w;

                            shader_instance->m_handle.m_modified =
                                jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::color& val) noexcept
            {
                set_uniform(name, val.to_linear());
            }
            const uint32_t* get_uniform_location(const std::string& name)
            {
                auto* jegl_shad_uniforms = resource()->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        return &jegl_shad_uniforms->m_index;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
                return nullptr;
            }
        };
        class animation : public resource_basic<je_animation_t>
        {
            explicit animation(je_animation_t* res)
                : resource_basic(res)
            {
            }

        public:
            // 加载 .je4animation 对应的缓存；失败时记录日志并返回空动画，
            // 保证持有该资源的组件始终能得到有效实例。
            static basic::resource<animation> load(const std::string& path)
            {
                je_animation_t* res = je_animation_open(path.c_str());
                if (res == nullptr)
                    res = je_animation_open("");

                assert(res != nullptr);
                return basic::resource<animation>(new animation(res));
            }
            const je_animation_action_data_t* find_action(const char* name) const
            {
                return je_animation_action_find(resource(), name);
            }
        };
        class vertex : public resource_basic<je_gl_vertex_t>
        {
            explicit vertex(je_gl_vertex_t* res)
                : resource_basic(res)
            {
            }

        public:
            static std::optional<basic::resource<vertex>> load(
                je_gl_context_t* context,
                const std::string& str)
            {
                auto* res = je_graphic_vertex_load(context, str.c_str());
                if (res != nullptr)
                    return basic::resource<vertex>(new vertex(res));
                return std::nullopt;
            }
            static std::optional<basic::resource<vertex>> create(
                je_gl_vertex_type_t type,
                const void* pdatas,
                size_t pdatalen,
                const std::vector<uint32_t>& idatas, // EBO indices
                const std::vector<je_gl_vertex_data_layout_t>& fdatas)
            {
                auto* res = je_graphic_vertex_create(
                    type,
                    pdatas,
                    pdatalen,
                    idatas.data(),
                    idatas.size(),
                    fdatas.data(),
                    fdatas.size());

                if (res != nullptr)
                    return basic::resource<vertex>(new vertex(res));
                return std::nullopt;
            }
            // 录制本帧顶点内容并收缩激活索引数量（见 je_graphic_cmd_update_vertex），
            // 供逐帧改写的动态网格使用；索引缓冲保持创建时的内容不变
            void update_buffer(je_gl_command_buffer_t* cmd,
                const void* pdatas, size_t pdatalen, size_t active_index_count) const
            {
                je_graphic_cmd_update_vertex(
                    cmd, resource(), pdatas, pdatalen, active_index_count);
            }
        };
        class framebuffer : public resource_basic<je_gl_frame_buffer_t>
        {
            explicit framebuffer(je_gl_frame_buffer_t* res)
                : resource_basic(res)
            {
            }

        public:
            static std::optional<basic::resource<framebuffer>> create(
                size_t reso_w,
                size_t reso_h,
                const std::vector<je_gl_texture_format_t>& color_attachment_formats,
                bool contain_depth_attachment)
            {
                auto* res = je_graphic_framebuf_create(
                    reso_w,
                    reso_h,
                    color_attachment_formats.data(),
                    color_attachment_formats.size(),
                    contain_depth_attachment);

                if (res != nullptr)
                    return basic::resource<framebuffer>(new framebuffer(res));
                return std::nullopt;
            }
            std::optional<basic::resource<texture2d>> get_attachment(size_t index) const
            {
                if (index < resource()->m_attachment_count)
                {
                    auto* attachments =
                        reinterpret_cast<basic::resource<graphic::texture2d> *>(
                            resource()->m_output_attachments);
                    return attachments[index];
                }
                return std::nullopt;
            }

            inline size_t height() const noexcept
            {
                return resource()->m_height;
            }
            inline size_t width() const noexcept
            {
                return resource()->m_width;
            }
            inline math::ivec2 size() const noexcept
            {
                return math::ivec2(
                    (int)resource()->m_width,
                    (int)resource()->m_height);
            }
        };
        class uniformbuffer : public resource_basic<je_gl_uniform_buffer_t>
        {
            explicit uniformbuffer(je_gl_uniform_buffer_t* res)
                : resource_basic(res)
            {
            }

        public:
            static std::optional<basic::resource<uniformbuffer>> create(
                size_t binding_place, size_t buffersize)
            {
                je_gl_uniform_buffer_t* res = je_graphic_uniformbuf_create(binding_place, buffersize);
                if (res != nullptr)
                    return basic::resource<uniformbuffer>(new uniformbuffer(res));
                return std::nullopt;
            }
            // 录制本帧的完整一致变量内容（见 je_graphic_cmd_update_uniform_buffer）
            void update_buffer(je_gl_command_buffer_t* cmd, const void* datafrom, size_t size) const noexcept
            {
                je_graphic_cmd_update_uniform_buffer(cmd, resource(), datafrom, size);
            }
        };

        inline constexpr float ORTHO_PROJECTION_RATIO = 5.0f;

        inline void ortho_projection(
            float (*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float scale,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;

            const float R = WIDTH_HEIGHT_RATIO * ORTHO_PROJECTION_RATIO / scale;
            const float L = -R;
            const float T = ORTHO_PROJECTION_RATIO / scale;
            const float B = -T;

            auto m = out_proj_mat;
            m[0][0] = 2.0f / (R - L);
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = 2.0f / (T - B);
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = 1.0f / (zfar - znear);
            m[2][3] = 0;

            m[3][0] = -((R + L) / (R - L));
            m[3][1] = -((T + B) / (T - B));
            m[3][2] = 0;
            m[3][3] = 1;
        }

        inline void ortho_inv_projection(
            float (*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float scale,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;

            const float R = WIDTH_HEIGHT_RATIO * ORTHO_PROJECTION_RATIO / scale;
            const float L = -R;
            const float T = ORTHO_PROJECTION_RATIO / scale;
            const float B = -T;

            auto m = out_proj_mat;
            m[0][0] = (R - L) / 2.0f;
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = (T - B) / 2.0f;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = zfar - znear;
            m[2][3] = 0;

            m[3][0] = (R + L) / 2.0f;
            m[3][1] = (T + B) / 2.0f;
            m[3][2] = 0;
            m[3][3] = 1;
        }

        inline void perspective_projection(
            float (*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float angle,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;
            const float ZRANGE = znear - zfar;
            const float TAN_HALF_FOV = tanf(angle / math::RAD2DEG / 2.0f);

            auto m = out_proj_mat;
            m[0][0] = 1.0f / (TAN_HALF_FOV * WIDTH_HEIGHT_RATIO);
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = 1.0f / TAN_HALF_FOV;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = -zfar / ZRANGE;
            m[2][3] = 1.0f;

            m[3][0] = 0;
            m[3][1] = 0;
            m[3][2] = zfar * znear / ZRANGE;
            m[3][3] = 0;
        }

        inline void perspective_inv_projection(
            float (*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float angle,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;
            const float ZRANGE = znear - zfar;
            const float TAN_HALF_FOV = tanf(angle / math::RAD2DEG / 2.0f);

            auto m = out_proj_mat;
            m[0][0] = TAN_HALF_FOV * WIDTH_HEIGHT_RATIO;
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = TAN_HALF_FOV;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = 0;
            m[2][3] = ZRANGE / (zfar * znear);

            m[3][0] = 0;
            m[3][1] = 0;
            m[3][2] = 1;
            m[3][3] = 1.f / znear;
        }

        struct character
        {
            // 字符位图（RGBA：RGB 恒为白，A 为覆盖度），行序自底向上，
            // 尺寸为 m_width x m_height（含边框）；由字体对象持有，
            // 生命周期与所在字符缓存一致
            const je_gl_texture_pixel_data_t* m_bitmap;

            // 字符本身
            char32_t m_char;
            // 字符本身的宽度和高度, 单位是像素
            // * 包含边框的影响
            int m_width;
            int m_height;

            // 建议的字符横向进展，通常用于计算下一列字符的起始位置，单位是像素，正数表示向右方延伸
            int m_advance_x;
            // 建议的字符纵向进展，即行间距，通常用于计算下一行字符的起始位置，单位是像素，正数表示向上方延伸
            int m_advance_y;

            // 当前字符基线横向偏移量，单位是像素，正数表示向右方偏移
            // * 包含边框的影响
            int m_baseline_offset_x;
            // 当前字符基线纵向偏移量，单位是像素，正数表示向上方偏移
            // * 包含边框的影响
            int m_baseline_offset_y;
        };
        class font
        {
            JECS_DISABLE_MOVE_AND_COPY(font);

            je_font_t* m_font;

        private:
            font(je_font_t* font_resource) noexcept
                : m_font(font_resource)
            {
            }

        public:
            static std::optional<basic::resource<font>> load(
                const std::string& fontfile,
                size_t board_size = 0,
                je_font_char_updater_t char_texture_updater = nullptr)
            {
                auto* font_res = je_font_load(
                    fontfile.c_str(),
                    board_size,
                    board_size,
                    char_texture_updater);

                if (font_res != nullptr)
                    return basic::resource<font>(new font(font_res));
                return std::nullopt;
            }
            ~font()
            {
                je_font_free(m_font);
            }
            const character* get_character(float size, char32_t wcharacter) const noexcept
            {
                return je_font_char_get(m_font, size, wcharacter);
            }
            je_font_t* resource() const noexcept
            {
                return m_font;
            }
        };

        /*
        jeecs::graphic::BasePipelineInterface [`系统(System)`接口]
        可编程图形管线系统的接口类，用于简化图形管线的创建，包装了一些基本函数
        （实际上开发者可以直接通过je_graphic_uhost_get_or_create_for_universe获取uhost进行绘制）
        请参见：
            je_graphic_uhost_get_or_create_for_universe
        */
        struct BasePipelineInterface : game_system
        {
            // Locate uniform buffer 0.
            struct default_uniform_buffer_data_t
            {
                float m_v_float4x4[4][4];
                float m_p_float4x4[4][4];
                float m_vp_float4x4[4][4];
                float m_time[4];
            };

            je_graphic_uhost_t* _m_graphic_host;
            std::vector<je_render_branch_t*> _m_render_branches;
            je_render_branch_t* _m_update_branch;
            je_gl_command_buffer_t* _m_update_command;
            size_t _m_this_frame_allocate_render_branch_count;

            BasePipelineInterface(game_world w, const je_gl_interface_config_t* config)
                : game_system(w)
                , _m_graphic_host(je_graphic_uhost_get_or_create_for_universe(w.get_universe().handle(), config))
                , _m_update_branch(nullptr)
                , _m_update_command(nullptr)
                , _m_this_frame_allocate_render_branch_count(0)
            {
            }
            ~BasePipelineInterface()
            {
                OnDisable();
            }

            void OnDisable()
            {
                // Free all branch if disabled.
                for (auto* branch : _m_render_branches)
                    je_graphic_uhost_free_branch(_m_graphic_host, branch);

                if (_m_update_branch != nullptr)
                {
                    je_graphic_uhost_free_branch(_m_graphic_host, _m_update_branch);
                    _m_update_branch = nullptr;
                }

                _m_update_command = nullptr;
                _m_render_branches.clear();
                _m_this_frame_allocate_render_branch_count = 0;
            }

            void branch_allocate_begin()
            {
                _m_this_frame_allocate_render_branch_count = 0;

                // 资源更新链：最低优先级，保证先于本帧所有绘制链提交；
                // 不开启 pass，窗口跳过帧也会被宿主提交。
                if (_m_update_branch == nullptr)
                    _m_update_branch = je_graphic_uhost_alloc_branch(_m_graphic_host);

                je_graphic_branch_new_frame(
                    _m_update_branch, GRAPHIC_UPDATE_BRANCH_QUEUE);
                _m_update_command =
                    je_graphic_branch_new_update_command(_m_update_branch);
            }
            je_gl_command_buffer_t* update_command() const
            {
                assert(_m_update_command != nullptr);
                return _m_update_command;
            }
            je_render_branch_t* allocate_branch(int priority)
            {
                if (_m_this_frame_allocate_render_branch_count >= _m_render_branches.size())
                {
                    assert(_m_this_frame_allocate_render_branch_count == _m_render_branches.size());
                    _m_render_branches.push_back(je_graphic_uhost_alloc_branch(_m_graphic_host));
                }
                auto* result = _m_render_branches[_m_this_frame_allocate_render_branch_count++];
                je_graphic_branch_new_frame(result, priority);
                return result;
            }
            void branch_allocate_end()
            {
                for (size_t i = _m_this_frame_allocate_render_branch_count; i < _m_render_branches.size(); ++i)
                    je_graphic_uhost_free_branch(_m_graphic_host, _m_render_branches[i]);

                _m_render_branches.resize(_m_this_frame_allocate_render_branch_count);
            }
        };

        class graphic_syncer_host
        {
            JECS_DISABLE_MOVE_AND_COPY(graphic_syncer_host);

            std::mutex mx;
            std::condition_variable cv;
            std::optional<std::thread> entry_script_thread;

            bool entry_script_ended;
            std::optional<je_gl_context_t*> graphic_context;

            je_gl_context_t* in_frame_current_context;

            static void callback(je_gl_context_t* context, void* p)
            {
                graphic_syncer_host* self =
                    static_cast<graphic_syncer_host*>(p);

                std::lock_guard g(self->mx);
                self->graphic_context = context;
                self->cv.notify_one();
            }
            bool check_context_ready_no_lock(bool do_sync_init)
            {
                if (graphic_context.has_value())
                {
                    in_frame_current_context = graphic_context.value();
                    graphic_context.reset();

                    if (do_sync_init)
                        je_graphic_sync_init(in_frame_current_context, false);

                    return true;
                }
                return false;
            }
        public:
            // 被设计用于自定义的图形同步上下文，用于获取引擎提供的图形配置信息
            // 
            // NOTE: 必须在 check_context_ready_block 或 check_context_ready_noblock 确认上下文已
            //      就绪后才能调用
            je_gl_context_t* get_graphic_context_after_context_ready() const
            {
                assert(in_frame_current_context != nullptr);
                return in_frame_current_context;
            }
            bool check_context_ready_block(bool do_sync_init)
            {
                for (;;)
                {
                    std::unique_lock ug(mx);
                    cv.wait(ug, [this]() {return entry_script_ended || graphic_context.has_value(); });

                    if (entry_script_ended)
                        return false;

                    if (!check_context_ready_no_lock(do_sync_init))
                    {
                        jeecs::debug::logerr(
                            "Failed to get graphic context, it should not happend.");
                        continue;
                    }
                    break;
                }
                return true;
            }
            bool check_context_ready_noblock(bool do_sync_init)
            {
                std::lock_guard g(mx);
                return check_context_ready_no_lock(do_sync_init);
            }
            bool frame()
            {
                assert(in_frame_current_context != nullptr);
                switch (je_graphic_sync_update(in_frame_current_context))
                {
                case je_gl_sync_state_t::JE_GL_SYNC_COMPLETE:
                    // Normal frame end, do nothing.
                    break;
                case je_gl_sync_state_t::JE_GL_SYNC_REBOOT:
                    // Require to reboot graphic thread.
                    je_graphic_sync_shutdown(in_frame_current_context, true);
                    je_graphic_sync_init(in_frame_current_context, true);
                    break;
                case je_gl_sync_state_t::JE_GL_SYNC_SHUTDOWN:
                    // Graphic thread want to shutdown, exit the loop.
                    je_graphic_sync_shutdown(in_frame_current_context, false);
                    in_frame_current_context = nullptr;
                    return false;
                }
                return true;
            }
            void loop()
            {
                for (;;)
                {
                    if (!check_context_ready_block(true))
                        break; // If the entry script ended, exit the loop.

                    while (frame())
                        ; // Update frames until the graphic context request to shutdown.
                }
            }
            graphic_syncer_host(bool execute_entry_script)
                : entry_script_ended(false)
                , graphic_context(std::nullopt)
                , in_frame_current_context(nullptr)
            {
                je_graphic_sync_thread_callback_register(
                    graphic_syncer_host::callback, this);

                if (execute_entry_script)
                    entry_script_thread = std::thread(
                        [this]()
                        {
                            je_core_main_script_entry();

                            std::lock_guard g(mx);
                            entry_script_ended = true;
                            cv.notify_one();
                        });
                else
                    entry_script_thread = std::nullopt;
            }
            ~graphic_syncer_host()
            {
                if (entry_script_thread.has_value())
                    entry_script_thread.value().join();
            }
        };
    }
}
#endif // __cplusplus
