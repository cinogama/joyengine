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
        void StateUpdate()
        {
            _fixed_time += deltatime();
            for (auto&& [e, frame_animation, shaders] : query_entity<
                view typesof(
                    Animation::FrameAnimation&, Shaders*
                )
            >())
            {
                if (abs(frame_animation.speed) == 0.0f)
                    continue;

                for (auto& animation : frame_animation.animations.m_animations)
                {
                    if (!animation.m_current_action.has_value())
                        continue;

                    auto* active_animation_frames =
                        animation.m_animations.find(animation.m_current_action.value());

                    if (active_animation_frames != animation.m_animations.end())
                    {
                        if (active_animation_frames->v.frames.empty() == false)
                        {
                            // 当前动画数据找到，如果当前帧是 SIZEMAX，或者已经到了要更新帧的时候，
                            if (animation.m_current_frame_index == SIZE_MAX || animation.m_next_update_time <= _fixed_time)
                            {
                                bool finish_animation = false;

                                auto update_and_apply_component_frame_data =
                                    [](const game_entity& e, jeecs::Animation::FrameAnimation::animation_list::frame_data& frame)
                                    {
                                        for (auto& cdata : frame.m_component_data)
                                        {
                                            if (cdata.m_entity_cache == e)
                                                continue;

                                            cdata.m_entity_cache = e;

                                            assert(cdata.m_component_type != nullptr && cdata.m_member_info != nullptr);

                                            auto* component_addr = je_ecs_world_entity_get_component(&e, cdata.m_component_type->m_id);
                                            if (component_addr == nullptr)
                                                // 没有这个组件，忽略之
                                                continue;

                                            auto* member_addr = (void*)(cdata.m_member_info->m_member_offset + (intptr_t)component_addr);

                                            // 在这里做好缓存和检查，不要每次都重新获取组件地址和检查类型
                                            cdata.m_member_addr_cache = member_addr;

                                            switch (cdata.m_member_value.m_type)
                                            {
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::INT:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<int>())
                                                {
                                                    jeecs::debug::logerr(
                                                        "Cannot apply animation frame data for component '%s''s member '%s', "
                                                        "type should be 'int', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::FLOAT:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<float>())
                                                {
                                                    jeecs::debug::logerr(
                                                        "Cannot apply animation frame data for component '%s''s member '%s', "
                                                        "type should be 'float', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC2:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec2>())
                                                {
                                                    jeecs::debug::logerr(
                                                        "Cannot apply animation frame data for component '%s''s member '%s', "
                                                        "type should be 'vec2', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC3:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec3>())
                                                {
                                                    jeecs::debug::logerr(
                                                        "Cannot apply animation frame data for component '%s''s member '%s', "
                                                        "type should be 'vec3', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC4:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec4>())
                                                {
                                                    jeecs::debug::logerr(
                                                        "Cannot apply animation frame data for component '%s''s member '%s', "
                                                        "type should be 'vec4', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::QUAT4:
                                                if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::quat>())
                                                {
                                                    jeecs::debug::logerr(
                                                        "Cannot apply animation frame data for component '%s''s member '%s', "
                                                        "type should be 'quat', but member is '%s'.",
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name,
                                                        cdata.m_member_info->m_member_type->m_typename);
                                                    cdata.m_member_addr_cache = nullptr;
                                                }
                                                break;
                                            default:
                                                jeecs::debug::logerr(
                                                    "Bad animation data type(%d) when trying set data of component '%s''s member '%s', "
                                                    "please check.",
                                                    (int)cdata.m_member_value.m_type,
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name);
                                                cdata.m_member_addr_cache = nullptr;
                                                break;
                                            }
                                        }
                                        for (auto& cdata : frame.m_component_data)
                                        {
                                            if (cdata.m_member_addr_cache == nullptr)
                                                continue; // Invalid! skip this component.

                                            switch (cdata.m_member_value.m_type)
                                            {
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::INT:
                                                if (cdata.m_offset_mode)
                                                    *(int*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.i32;
                                                else
                                                    *(int*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.i32;
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::FLOAT:
                                                if (cdata.m_offset_mode)
                                                    *(float*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.f32;
                                                else
                                                    *(float*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.f32;
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC2:
                                                if (cdata.m_offset_mode)
                                                    *(math::vec2*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v2;
                                                else
                                                    *(math::vec2*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v2;
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC3:
                                                if (cdata.m_offset_mode)
                                                    *(math::vec3*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v3;
                                                else
                                                    *(math::vec3*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v3;
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC4:
                                                if (cdata.m_offset_mode)
                                                    *(math::vec4*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v4;
                                                else
                                                    *(math::vec4*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v4;
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::QUAT4:
                                                if (cdata.m_offset_mode)
                                                    *(math::quat*)cdata.m_member_addr_cache =
                                                    *(math::quat*)cdata.m_member_addr_cache
                                                    * cdata.m_member_value.m_value.q4;
                                                else
                                                    *(math::quat*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.q4;
                                                break;
                                            default:
                                                jeecs::debug::logerr(
                                                    "Bad animation data type(%d) when trying set data of component '%s''s member '%s', "
                                                    "please check.",
                                                    (int)cdata.m_member_value.m_type,
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name);
                                                break;
                                            }
                                        }
                                    };

                                auto current_animation_frame_count = active_animation_frames->v.frames.size();

                                if (animation.m_current_frame_index == SIZE_MAX || animation.m_last_speed != frame_animation.speed)
                                {
                                    animation.m_current_frame_index = 0;
                                    animation.m_next_update_time =
                                        _fixed_time
                                        + (active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time
                                            + math::random(-frame_animation.jitter, frame_animation.jitter))
                                        / frame_animation.speed;
                                }
                                else
                                {
                                    // 到达下一次更新时间！检查间隔时间，并跳转到对应的帧
                                    auto delta_time_between_frams = _fixed_time - animation.m_next_update_time;
                                    auto next_frame_index = (animation.m_current_frame_index + 1) % current_animation_frame_count;

                                    while (
                                        delta_time_between_frams > active_animation_frames->v.frames[next_frame_index].m_frame_time / frame_animation.speed)
                                    {
                                        if (animation.m_loop == false && next_frame_index == current_animation_frame_count - 1)
                                            break;

                                        // 在此应用跳过帧的deltaframe数据
                                        update_and_apply_component_frame_data(e, active_animation_frames->v.frames[next_frame_index]);

                                        delta_time_between_frams -=
                                            active_animation_frames->v.frames[next_frame_index].m_frame_time / frame_animation.speed;
                                        next_frame_index = (next_frame_index + 1) % current_animation_frame_count;
                                    }

                                    animation.m_current_frame_index = next_frame_index;
                                    animation.m_next_update_time =
                                        _fixed_time
                                        - delta_time_between_frams
                                        + (active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time
                                            + math::random(-frame_animation.jitter, frame_animation.jitter))
                                        / frame_animation.speed;
                                }

                                if (animation.m_loop == false)
                                {
                                    if (animation.m_current_frame_index == current_animation_frame_count - 1)
                                        finish_animation = true;
                                }

                                auto& updating_frame = active_animation_frames->v.frames[animation.m_current_frame_index];
                                update_and_apply_component_frame_data(e, updating_frame);

                                if (shaders != nullptr)
                                {
                                    for (auto& udata : updating_frame.m_uniform_data)
                                    {
                                        switch (udata.m_uniform_value.m_type)
                                        {
                                        case Animation::FrameAnimation::animation_list::frame_data::data_value::type::INT:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.i32);
                                            break;
                                        case Animation::FrameAnimation::animation_list::frame_data::data_value::type::FLOAT:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.f32);
                                            break;
                                        case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC2:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v2);
                                            break;
                                        case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC3:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v3);
                                            break;
                                        case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC4:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v4);
                                            break;
                                        default:
                                            jeecs::debug::logerr(
                                                "Bad animation data type(%d) when trying set data of uniform variable '%s', "
                                                "please check.",
                                                (int)udata.m_uniform_value.m_type,
                                                udata.m_uniform_name.c_str());
                                            break;
                                        }
                                    }
                                }
                                if (finish_animation)
                                {
                                    // 终止动画
                                    animation.stop();
                                }
                                animation.m_last_speed = frame_animation.speed;
                            }
                        }
                    }
                    else
                    {
                        // 如果没有找到对应的动画，那么终止动画
                        animation.stop();
                    }
                    // 这个注释写在这里单纯是因为花括号写得太难看，稍微避免出现一个大于号
                }
                // End
            }
        }
    };
}
