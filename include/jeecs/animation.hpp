#pragma once

// ============================================================================
// jeecs/animation.hpp —— 动画资源：je_animation_* C API 与 jeecs::animation 播放控制 C++ 封装
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"
#include "jeecs/file.hpp"
#include "jeecs/cache.hpp"
#include "jeecs/game.hpp"

JE_FORCE_CAPI

/////////////////////////// ANIMATION RESOURCE /////////////////////////////////

/*
je_animation_value_type_t [类型]
动画轨道中一个数据值的类型标记，与缓存文档中写入的 type 字段一致
*/
typedef enum je_animation_value_type_t
{
    JE_ANIMATION_VALUE_INT = 0,
    JE_ANIMATION_VALUE_FLOAT,
    JE_ANIMATION_VALUE_VEC2,
    JE_ANIMATION_VALUE_VEC3,
    JE_ANIMATION_VALUE_VEC4,
    JE_ANIMATION_VALUE_QUAT4,
} je_animation_value_type_t;

/*
je_animation_interp_mode_t [类型]
关键帧与其下一个关键帧之间的插值方式：
    * CONSTANT —— 保持当前值直到下一个关键帧（阶跃）
    * LINEAR   —— 线性插值；QUAT4 为 slerp，INT 为线性后取整
    * HERMITE  —— 三次 hermite 插值，使用关键帧切线（QUAT4/INT 不支持）
*/
typedef enum je_animation_interp_mode_t
{
    JE_ANIMATION_INTERP_CONSTANT = 0,
    JE_ANIMATION_INTERP_LINEAR,
    JE_ANIMATION_INTERP_HERMITE,
} je_animation_interp_mode_t;

/*
je_animation_tangent_mode_t [类型]
hermite 关键帧切线的来源；生成缓存时 AUTO/LINEAR 被烘焙为具体导数值
*/
typedef enum je_animation_tangent_mode_t
{
    JE_ANIMATION_TANGENT_AUTO = 0,   // Catmull-Rom 自动切线
    JE_ANIMATION_TANGENT_LINEAR,     // 与相邻关键帧线性连接
    JE_ANIMATION_TANGENT_FREE,       // 使用源文件提供的导数值
} je_animation_tangent_mode_t;

/*
je_animation_loop_mode_t [类型]
动作播放到结尾时的循环方式
*/
typedef enum je_animation_loop_mode_t
{
    JE_ANIMATION_LOOP_NONE = 0,   // 停在结尾并结束播放
    JE_ANIMATION_LOOP_REPEAT,     // 回绕到开头继续
    JE_ANIMATION_LOOP_PINGPONG,   // 到达结尾后反向播放
} je_animation_loop_mode_t;

/*
je_animation_track_kind_t [类型]
动画轨道的作用目标类型
*/
typedef enum je_animation_track_kind_t
{
    JE_ANIMATION_TRACK_MEMBER = 0,   // 组件成员轨道
    JE_ANIMATION_TRACK_UNIFORM,      // 着色器 uniform 轨道
} je_animation_track_kind_t;

/*
je_animation_value_t [类型]
动画轨道中的一个数据值。按 m_type 解释 m_value：
    * INT   —— i32
    * FLOAT —— f32
    * VEC2  —— v2[0..1]
    * VEC3  —— v3[0..2]
    * VEC4  —— v4[0..3]
    * QUAT4 —— q4[0..3]（x, y, z, w）
*/
typedef struct je_animation_value_t
{
    uint8_t m_type;
    union
    {
        int32_t i32;
        float f32;
        float v2[2];
        float v3[3];
        float v4[4];
        float q4[4];
    } m_value;
} je_animation_value_t;

/*
je_animation_key_t [类型]
轨道上的一个关键帧：
    * m_interp / m_tangent_mode 描述此关键帧到下一个关键帧的区段
    * m_tangents 为每通道 (入导数, 出导数) 共 通道数*2 个 float，
      仅 HERMITE 区段且切线已烘焙/显式提供时非空
    * m_value 为 QUAT4 时存储四元数 (x, y, z, w)，
      由生成器从源文件中的欧拉角换算
*/
typedef struct je_animation_key_t
{
    float m_time;
    je_animation_value_t m_value;
    uint8_t m_interp;        // je_animation_interp_mode_t
    uint8_t m_tangent_mode;  // je_animation_tangent_mode_t
    float* m_tangents;       // OPTIONAL：仅 HERMITE 区段持有
} je_animation_key_t;

/*
je_animation_track_t [类型]
动画中的一个轨道：一列作用于同一目标的关键帧
    * MEMBER 轨道的 m_component_type / m_member_info 在加载时解析并校验，
      无效条目不会保留；实体缺少对应组件时该轨道无效果
    * UNIFORM 轨道通过 Renderer::Shaders 写入自定义 uniform，
      注意其修改的是共享的着色器资源
    * m_offset_mode 为 true 时在原值上叠加，否则直接赋值
    * m_keys 按 m_time 升序排列
*/
typedef struct je_animation_track_t
{
    uint8_t m_kind;  // je_animation_track_kind_t
    bool m_offset_mode;

    const je_type_info_t* m_component_type;  // MEMBER 轨道
    const je_member_info_t* m_member_info;   // MEMBER 轨道

    const char* m_uniform_name;  // UNIFORM 轨道

    uint8_t m_value_type;  // je_animation_value_type_t
    je_animation_key_t* m_keys;
    size_t m_key_count;
} je_animation_track_t;

/*
je_animation_event_t [类型]
动作时间轴上的一个命名事件点，播放越过该时刻时触发，
经 je_animation_entity_events_get 获取
*/
typedef struct je_animation_event_t
{
    float m_time;
    const char* m_name;
} je_animation_event_t;

/*
je_animation_action_data_t [类型]
动画集中的一个动作：一列轨道与事件共享同一条时间轴
    * m_duration 等于所有关键帧与事件时间的最大值
*/
typedef struct je_animation_action_data_t
{
    const char* m_name;
    float m_duration;
    uint8_t m_loop_mode;  // je_animation_loop_mode_t
    je_animation_track_t* m_tracks;
    size_t m_track_count;
    je_animation_event_t* m_events;
    size_t m_event_count;
} je_animation_action_data_t;

/*
je_animation_t [类型]
动画集的只读数据，由 je_animation_open 创建、je_animation_close 释放
    * 该结构不包含任何 C++ 容器，动画数据以连续数组持有
    * 数据在加载完成后不再修改，可被多个组件/实体共享
*/
typedef struct je_animation_t
{
    je_animation_action_data_t* m_actions;
    size_t m_action_count;
} je_animation_t;

/*
je_animation_sample_buffer_t [类型]
动画采样的累加缓冲，用于把一个或多个动作的加权采样结果混合后写入实体。
由 je_animation_sample_buffer_create 创建、destroy 释放
*/
typedef struct je_animation_sample_buffer_t je_animation_sample_buffer_t;

/*
je_animation_open [基本接口]
加载 path 指向的动画集：
    * 读取 <path>.je4cache；缓存缺失/失效时先调用
      builtin/script/internal/cache/animation.wo 生成
    * 空路径返回一个不含任何动作的空动画
    * 其他失败情况返回 nullptr 并记录日志
    * 返回的数据使用 je_animation_close 释放
*/
JE_API /* OPTIONAL */ je_animation_t* je_animation_open(const char* path);

/*
je_animation_close [基本接口]
释放 je_animation_open 返回的动画集数据
*/
JE_API void je_animation_close(je_animation_t* animation);

/*
je_animation_action_find [基本接口]
按名称查找动作，找不到返回 nullptr
*/
JE_API /* OPTIONAL */ const je_animation_action_data_t* je_animation_action_find(
    const je_animation_t* animation,
    const char* name);

/*
je_animation_cache_open_or_create [基本接口]
打开指定的动画集缓存文件；缓存缺失或失效时调用内置缓存生成器重建
    * 返回的句柄由调用方负责关闭（见 je_file_close）；无法生成时返回 nullptr
请参见：
    je_cache_file_load
    je_file_close
*/
JE_API /* OPTIONAL */ je_file_t* je_animation_cache_open_or_create(const char* path);

/*
je_animation_sample_buffer_create [基本接口]
创建一个采样累加缓冲，用毕以 je_animation_sample_buffer_destroy 释放
*/
JE_API je_animation_sample_buffer_t* je_animation_sample_buffer_create(void);

/*
je_animation_sample_buffer_destroy [基本接口]
释放 je_animation_sample_buffer_create 创建的缓冲
*/
JE_API void je_animation_sample_buffer_destroy(je_animation_sample_buffer_t* buffer);

/*
je_animation_sample_buffer_clear [基本接口]
清空缓冲中的全部采样结果，开始新一轮累加
*/
JE_API void je_animation_sample_buffer_clear(je_animation_sample_buffer_t* buffer);

/*
je_animation_action_sample [基本接口]
按权重把 action 在 time 时刻的采样结果累加进 buffer：
    * 多次采样的同一目标按权重加权平均（QUAT4 为 slerp），
      叠加轨道按 value*weight 累加
    * weight <= 0 时忽略本次采样
    * time 超出首/尾关键帧范围时钳制到端点值，不做循环回绕
*/
JE_API void je_animation_action_sample(
    const je_animation_action_data_t* action,
    float time,
    float weight,
    je_animation_sample_buffer_t* buffer);

/*
je_animation_sample_buffer_apply [基本接口]
把缓冲中的采样结果写入实体：
    * 组件成员轨道要求实体持有对应组件，否则该轨道无效果
    * uniform 轨道要求实体持有 Renderer::Shaders 组件
*/
JE_API void je_animation_sample_buffer_apply(
    const je_game_entity_t* entity,
    je_animation_sample_buffer_t* buffer);

/*
je_animation_entity_play [基本接口]
立即播放实体 Animation::Animator 组件上的指定动作，重置进度并清除淡入淡出
*/
JE_API void je_animation_entity_play(const je_game_entity_t* entity, const char* action_name);

/*
je_animation_entity_crossfade [基本接口]
在 fade_duration 秒内从当前动作平滑过渡到指定动作；
当前无动作或时长为 0 时等价于 je_animation_entity_play
*/
JE_API void je_animation_entity_crossfade(
    const je_game_entity_t* entity,
    const char* action_name,
    float fade_duration);

/*
je_animation_entity_stop / pause / resume [基本接口]
停止（清除动作与进度）/ 暂停 / 恢复实体当前的动画播放
*/
JE_API void je_animation_entity_stop(const je_game_entity_t* entity);
JE_API void je_animation_entity_pause(const je_game_entity_t* entity);
JE_API void je_animation_entity_resume(const je_game_entity_t* entity);

/*
je_animation_entity_is_playing [基本接口]
实体是否正在播放（已指定动作、未暂停且未结束）
*/
JE_API bool je_animation_entity_is_playing(const je_game_entity_t* entity);

/*
je_animation_entity_action_get [基本接口]
获取当前动作名，未播放时返回 nullptr
*/
JE_API /* OPTIONAL */ const char* je_animation_entity_action_get(const je_game_entity_t* entity);

/*
je_animation_entity_duration_get / progress_get [基本接口]
当前动作的总时长与播放进度 (0..1)，未播放时为 0
*/
JE_API float je_animation_entity_duration_get(const je_game_entity_t* entity);
JE_API float je_animation_entity_progress_get(const je_game_entity_t* entity);

/*
je_animation_entity_events_get [基本接口]
获取本帧内动画播放越过事件点而触发的事件名：
    * 复制至多 out_event_names_capacity 个事件名到 out_event_names，
      返回本帧事件总数；返回值大于容量时说明缓冲不足
    * 读取不会消费事件，同一帧内任意多个系统读取都得到相同结果
    * 事件名在该实体所在世界的下一次 AnimatorSystem 更新前有效
*/
JE_API size_t je_animation_entity_events_get(
    const je_game_entity_t* entity,
    /* OPTIONAL */ const char** out_event_names, // 仅当容量为 0（只查询数量）时可为空
    size_t out_event_names_capacity);
typedef je_animation_value_type_t je_AnimationValueType;
typedef je_animation_interp_mode_t je_AnimationInterpMode;
typedef je_animation_tangent_mode_t je_AnimationTangentMode;
typedef je_animation_loop_mode_t je_AnimationLoopMode;
typedef je_animation_track_kind_t je_AnimationTrackKind;
typedef je_animation_value_t je_AnimationValue;
typedef je_animation_key_t je_AnimationKey;
typedef je_animation_track_t je_AnimationTrack;
typedef je_animation_event_t je_AnimationEvent;
typedef je_animation_action_data_t je_AnimationActionData;
typedef je_animation_t je_Animation;
typedef je_animation_sample_buffer_t je_AnimationSampleBuffer;

JE_FORCE_CAPI_END
#ifdef __cplusplus
namespace jeecs
{
    /*
    jeecs::animation [命名空间]
    动画播放控制与采样的 C++ 封装，全部转发到 je_animation_* C API。
    play/crossfade 等播放控制函数要求实体已持有 Animation::Animator 组件。
    */
    namespace animation
    {
        inline void play(const game_entity& e, const char* action_name)
        {
            je_animation_entity_play(&e._m_raw, action_name);
        }
        inline void crossfade(const game_entity& e, const char* action_name, float fade_duration)
        {
            je_animation_entity_crossfade(&e._m_raw, action_name, fade_duration);
        }
        inline void stop(const game_entity& e)
        {
            je_animation_entity_stop(&e._m_raw);
        }
        inline void pause(const game_entity& e)
        {
            je_animation_entity_pause(&e._m_raw);
        }
        inline void resume(const game_entity& e)
        {
            je_animation_entity_resume(&e._m_raw);
        }
        inline bool is_playing(const game_entity& e)
        {
            return je_animation_entity_is_playing(&e._m_raw);
        }
        inline /* OPTIONAL */ const char* current_action(const game_entity& e)
        {
            return je_animation_entity_action_get(&e._m_raw);
        }
        inline float duration(const game_entity& e)
        {
            return je_animation_entity_duration_get(&e._m_raw);
        }
        inline float progress(const game_entity& e)
        {
            return je_animation_entity_progress_get(&e._m_raw);
        }
    }
}
#endif // __cplusplus
