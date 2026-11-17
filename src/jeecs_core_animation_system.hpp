#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/animation.hpp"
#include "jeecs/game.hpp"

#include <vector>

namespace jeecs::animation
{
    // 打开动画集缓存 <path>.je4cache：缓存有效时直接返回，缺失或失效时
    // 经内置生成器重建（internal_script::regenerate_animation_cache）。
    // 返回的句柄由调用方负责关闭（见 je_file_close）；无法生成时为 nullptr。
    /* OPTIONAL */ je_file_t* open_or_create_cache(const char* path);

    // ===== 采样原语 =====
    //（原 je_animation_sample_buffer_* / je_animation_action_sample C API 的
    // 内部化版本）供动画系统与预览/自定义采样消费者把一个或多个动作的
    // 加权采样结果混合后写入实体。

    // 采样累加缓冲中的一个条目：同一目标的多次加权采样在缓冲内按权重
    // 混合（QUAT4 为 slerp），叠加轨道按 value*weight 累加
    struct sample_entry
    {
        uint8_t m_kind;
        bool m_additive;

        const je_type_info_t* m_component_type;
        const je_member_info_t* m_member_info;
        const char* m_uniform_name;  // 指向动画资源，生命周期随资源

        je_animation_value_t m_value;
        float m_weight;  // 叠加条目不使用
    };

    // 采样累加缓冲：create/destroy 管理生命周期，clear 清空后开始新一轮
    // 累加，sample_action 按权重累加一个动作在 time 时刻的采样（时间超出
    // 首/尾关键帧范围时钳制到端点，weight <= 0 忽略），
    // apply_sample_buffer 把缓冲写入实体（组件成员轨道要求实体持有对应
    // 组件，uniform 轨道要求持有 Renderer::Shaders，否则该轨道无效果）
    struct sample_buffer
    {
        std::vector<sample_entry> m_entries;
    };

    sample_buffer* create_sample_buffer();
    void destroy_sample_buffer(sample_buffer* buffer);
    void clear_sample_buffer(sample_buffer* buffer);
    void sample_action(
        const je_animation_action_data_t* action,
        float time,
        float weight,
        sample_buffer* buffer);
    void apply_sample_buffer(
        const je_game_entity_t* entity,
        sample_buffer* buffer);

    struct AnimatorSystem : public game_system
    {
        AnimatorSystem(game_world w);
        void StateUpdate();
    };
}
