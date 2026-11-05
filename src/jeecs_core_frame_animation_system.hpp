#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

namespace jeecs
{
    using namespace Animation;

    struct FrameAnimationSystem : public game_system
    {
        double _fixed_time = 0.;

        FrameAnimationSystem(game_world w)
            : game_system(w)
        {
        }

        static void _apply_component_and_uniform_frame_data(
            const game_entity& e,
            /* OPTIONAL */ Renderer::Shaders* shaders,
            const je_animation_frame_data& frame)
        {
            for (size_t i = 0; i < frame.m_component_data_count; ++i)
            {
                const je_animation_component_data& cdata = frame.m_component_data[i];
                auto* component_addr = je_ecs_world_entity_get_component(&e._m_raw, cdata.m_component_type->m_id);

                if (component_addr == nullptr)
                    continue;

                auto* member_addr = (void*)(
                    cdata.m_member_info->m_member_offset + (intptr_t)component_addr);

                switch (cdata.m_member_value.m_type)
                {
                case JE_ANIMATION_VALUE_INT:
                    if (cdata.m_offset_mode)
                        *(int*)member_addr += cdata.m_member_value.m_value.i32;
                    else
                        *(int*)member_addr = cdata.m_member_value.m_value.i32;
                    break;
                case JE_ANIMATION_VALUE_FLOAT:
                    if (cdata.m_offset_mode)
                        *(float*)member_addr += cdata.m_member_value.m_value.f32;
                    else
                        *(float*)member_addr = cdata.m_member_value.m_value.f32;
                    break;
                case JE_ANIMATION_VALUE_VEC2:
                {
                    const float* v = cdata.m_member_value.m_value.v2;
                    if (cdata.m_offset_mode)
                        *(math::vec2*)member_addr += math::vec2(v[0], v[1]);
                    else
                        *(math::vec2*)member_addr = math::vec2(v[0], v[1]);
                    break;
                }
                case JE_ANIMATION_VALUE_VEC3:
                {
                    const float* v = cdata.m_member_value.m_value.v3;
                    if (cdata.m_offset_mode)
                        *(math::vec3*)member_addr += math::vec3(v[0], v[1], v[2]);
                    else
                        *(math::vec3*)member_addr = math::vec3(v[0], v[1], v[2]);
                    break;
                }
                case JE_ANIMATION_VALUE_VEC4:
                {
                    const float* v = cdata.m_member_value.m_value.v4;
                    if (cdata.m_member_info->m_member_type
                        == jeecs::typing::of<math::color>())
                    {
                        auto* color_member = (math::color*)member_addr;
                        if (cdata.m_offset_mode)
                            *color_member = math::color(
                                color_member->r + v[0],
                                color_member->g + v[1],
                                color_member->b + v[2],
                                color_member->a + v[3]);
                        else
                            *color_member = math::color(v[0], v[1], v[2], v[3]);
                    }
                    else if (cdata.m_offset_mode)
                        *(math::vec4*)member_addr +=
                        math::vec4(v[0], v[1], v[2], v[3]);
                    else
                        *(math::vec4*)member_addr =
                        math::vec4(v[0], v[1], v[2], v[3]);
                    break;
                }
                case JE_ANIMATION_VALUE_QUAT4:
                {
                    const float* q = cdata.m_member_value.m_value.q4;
                    math::quat value;
                    value.x = q[0];
                    value.y = q[1];
                    value.z = q[2];
                    value.w = q[3];

                    if (cdata.m_offset_mode)
                        *(math::quat*)member_addr =
                        *(math::quat*)member_addr * value;
                    else
                        *(math::quat*)member_addr = value;
                    break;
                }
                default:
                    break;
                }
            }
            if (shaders != nullptr)
            {
                for (size_t i = 0; i < frame.m_uniform_data_count; ++i)
                {
                    const je_animation_uniform_data& udata = frame.m_uniform_data[i];
                    switch (udata.m_uniform_value.m_type)
                    {
                    case JE_ANIMATION_VALUE_INT:
                        shaders->set_uniform(
                            udata.m_uniform_name, udata.m_uniform_value.m_value.i32);
                        break;
                    case JE_ANIMATION_VALUE_FLOAT:
                        shaders->set_uniform(
                            udata.m_uniform_name, udata.m_uniform_value.m_value.f32);
                        break;
                    case JE_ANIMATION_VALUE_VEC2:
                    {
                        const float* v = udata.m_uniform_value.m_value.v2;
                        shaders->set_uniform(
                            udata.m_uniform_name, math::vec2(v[0], v[1]));
                        break;
                    }
                    case JE_ANIMATION_VALUE_VEC3:
                    {
                        const float* v = udata.m_uniform_value.m_value.v3;
                        shaders->set_uniform(
                            udata.m_uniform_name, math::vec3(v[0], v[1], v[2]));
                        break;
                    }
                    case JE_ANIMATION_VALUE_VEC4:
                    {
                        const float* v = udata.m_uniform_value.m_value.v4;
                        shaders->set_uniform(
                            udata.m_uniform_name, math::vec4(v[0], v[1], v[2], v[3]));
                        break;
                    }
                    default:
                        jeecs::debug::logerr(
                            "Bad animation data type(%d) when trying set data of uniform variable '%s', please check.",
                            (int)udata.m_uniform_value.m_type,
                            udata.m_uniform_name);
                        break;
                    }
                }
            }
        }

        void StateUpdate()
        {
            _fixed_time += deltatime();
            for (auto&& [e, frame_animation, shaders] : query_entity<
                view typesof(
                    Animation::FrameAnimation&, Renderer::Shaders*
                )
            >())
            {
                if (abs(frame_animation.speed) == 0.0f)
                    continue;

                for (auto& animation : frame_animation.animations.m_animations)
                {
                    if (!animation.m_current_action.has_value())
                        continue;

                    const je_animation_action_data* active_action =
                        animation.m_animation->find_action(
                            animation.m_current_action.value().c_str());

                    if (active_action == nullptr)
                    {
                        // 如果没有找到对应的动画，那么终止动画
                        animation.stop();
                        continue;
                    }
                    if (active_action->m_frame_count == 0)
                        continue;

                    // 当前动画数据找到，如果当前帧是 SIZEMAX，或者已经到了要更新帧的时候，
                    if (animation.m_current_frame_index == SIZE_MAX
                        || animation.m_next_update_time <= _fixed_time)
                    {
                        if (animation.m_current_frame_index == SIZE_MAX
                            || animation.m_last_speed != frame_animation.speed)
                        {
                            animation.m_current_frame_index = 0;
                            animation.m_next_update_time =
                                _fixed_time
                                + (active_action->m_frames[animation.m_current_frame_index].m_frame_time
                                    + math::random(-frame_animation.jitter, frame_animation.jitter))
                                / frame_animation.speed;
                        }
                        else
                        {
                            // 到达下一次更新时间！检查间隔时间，并跳转到对应的帧
                            const size_t frame_count = active_action->m_frame_count;
                            auto delta_time_between_frams =
                                _fixed_time - animation.m_next_update_time;
                            size_t next_frame_index =
                                (animation.m_current_frame_index + 1) % frame_count;

                            for (;;)
                            {
                                const auto& frame = active_action->m_frames[next_frame_index];
                                if (delta_time_between_frams <= frame.m_frame_time / frame_animation.speed)
                                    break;

                                if (animation.m_loop == false
                                    && next_frame_index == frame_count - 1)
                                    break;

                                // 在此应用跳过帧的deltaframe数据
                                _apply_component_and_uniform_frame_data(e, shaders, frame);

                                delta_time_between_frams -= frame.m_frame_time
                                    / frame_animation.speed;
                                next_frame_index = (next_frame_index + 1) % frame_count;
                            }

                            animation.m_current_frame_index = next_frame_index;
                            animation.m_next_update_time =
                                _fixed_time
                                - delta_time_between_frams
                                + (active_action->m_frames[animation.m_current_frame_index].m_frame_time
                                    + math::random(-frame_animation.jitter, frame_animation.jitter))
                                / frame_animation.speed;
                        }

                        bool finish_animation = false;
                        if (animation.m_loop == false)
                        {
                            if (animation.m_current_frame_index
                                == active_action->m_frame_count - 1)
                                finish_animation = true;
                        }

                        const je_animation_frame_data& updating_frame =
                            active_action->m_frames[animation.m_current_frame_index];

                        _apply_component_and_uniform_frame_data(e, shaders, updating_frame);

                        if (finish_animation)
                            // 终止动画
                            animation.stop();

                        animation.m_last_speed = frame_animation.speed;
                    }
                }
            }
        }
    };
}
