#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_core_animation_system.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

namespace jeecs::animation
{
    namespace
    {
        constexpr size_t kMaxFrameEvents = 32;

        size_t _value_channels(uint8_t type)
        {
            switch (type)
            {
            case JE_ANIMATION_VALUE_INT:
            case JE_ANIMATION_VALUE_FLOAT:
                return 1;
            case JE_ANIMATION_VALUE_VEC2:
                return 2;
            case JE_ANIMATION_VALUE_VEC3:
                return 3;
            case JE_ANIMATION_VALUE_VEC4:
            case JE_ANIMATION_VALUE_QUAT4:
                return 4;
            }
            return 0;
        }

        const float* _value_channels_of(const je_animation_value_t& v)
        {
            switch (v.m_type)
            {
            case JE_ANIMATION_VALUE_FLOAT: return &v.m_value.f32;
            case JE_ANIMATION_VALUE_VEC2: return v.m_value.v2;
            case JE_ANIMATION_VALUE_VEC3: return v.m_value.v3;
            case JE_ANIMATION_VALUE_VEC4: return v.m_value.v4;
            case JE_ANIMATION_VALUE_QUAT4: return v.m_value.q4;
            }
            return nullptr;
        }

        float* _value_channels_of(je_animation_value_t& v)
        {
            switch (v.m_type)
            {
            case JE_ANIMATION_VALUE_FLOAT: return &v.m_value.f32;
            case JE_ANIMATION_VALUE_VEC2: return v.m_value.v2;
            case JE_ANIMATION_VALUE_VEC3: return v.m_value.v3;
            case JE_ANIMATION_VALUE_VEC4: return v.m_value.v4;
            case JE_ANIMATION_VALUE_QUAT4: return v.m_value.q4;
            }
            return nullptr;
        }

        // 数值插值（QUAT4 为 slerp，INT 为线性后取整）
        je_animation_value_t _mix_values(
            const je_animation_value_t& a,
            const je_animation_value_t& b,
            float t)
        {
            assert(a.m_type == b.m_type);

            je_animation_value_t result{};
            result.m_type = a.m_type;
            switch (a.m_type)
            {
            case JE_ANIMATION_VALUE_INT:
                result.m_value.i32 = (int32_t)std::lround(
                    math::lerp((float)a.m_value.i32, (float)b.m_value.i32, t));
                break;
            case JE_ANIMATION_VALUE_FLOAT:
                result.m_value.f32 = math::lerp(a.m_value.f32, b.m_value.f32, t);
                break;
            case JE_ANIMATION_VALUE_VEC2:
                result.m_value.v2[0] = math::lerp(a.m_value.v2[0], b.m_value.v2[0], t);
                result.m_value.v2[1] = math::lerp(a.m_value.v2[1], b.m_value.v2[1], t);
                break;
            case JE_ANIMATION_VALUE_VEC3:
                for (size_t i = 0; i < 3; ++i)
                    result.m_value.v3[i] =
                        math::lerp(a.m_value.v3[i], b.m_value.v3[i], t);
                break;
            case JE_ANIMATION_VALUE_VEC4:
                for (size_t i = 0; i < 4; ++i)
                    result.m_value.v4[i] =
                        math::lerp(a.m_value.v4[i], b.m_value.v4[i], t);
                break;
            case JE_ANIMATION_VALUE_QUAT4:
            {
                math::quat qa(
                    a.m_value.q4[0], a.m_value.q4[1],
                    a.m_value.q4[2], a.m_value.q4[3]);
                math::quat qb(
                    b.m_value.q4[0], b.m_value.q4[1],
                    b.m_value.q4[2], b.m_value.q4[3]);
                math::quat q = math::quat::slerp(qa, qb, t);
                result.m_value.q4[0] = q.x;
                result.m_value.q4[1] = q.y;
                result.m_value.q4[2] = q.z;
                result.m_value.q4[3] = q.w;
                break;
            }
            default:
                break;
            }
            return result;
        }

        // 叠加模式下的数值累加：target += value * weight
        void _accumulate_values(
            je_animation_value_t* target,
            const je_animation_value_t& value,
            float weight)
        {
            assert(target->m_type == value.m_type);

            switch (value.m_type)
            {
            case JE_ANIMATION_VALUE_INT:
                target->m_value.i32 += (int32_t)std::lround(value.m_value.i32 * weight);
                break;
            case JE_ANIMATION_VALUE_FLOAT:
                target->m_value.f32 += value.m_value.f32 * weight;
                break;
            default:
            {
                float* dst = _value_channels_of(*target);
                const float* src = _value_channels_of(value);
                const size_t channels = _value_channels(value.m_type);
                for (size_t i = 0; i < channels; ++i)
                    dst[i] += src[i] * weight;
                break;
            }
            }
        }

        // 三次 hermite 基函数（切线为导数 d值/d时间）
        float _hermite(float p0, float p1, float m0, float m1, float dt, float u)
        {
            const float u2 = u * u;
            const float u3 = u2 * u;
            return (2.f * u3 - 3.f * u2 + 1.f) * p0
                + (u3 - 2.f * u2 + u) * (dt * m0)
                + (-2.f * u3 + 3.f * u2) * p1
                + (u3 - u2) * (dt * m1);
        }

        // 采样一条轨道在 time 时刻的值；time 超出关键帧范围时钳制到端点。
        // 要求关键帧按时间升序排列（加载器保证）。
        je_animation_value_t _sample_track(
            const je_animation_track_t& track, float time)
        {
            assert(track.m_key_count != 0);

            const je_animation_key_t* keys = track.m_keys;
            if (time <= keys[0].m_time)
                return keys[0].m_value;
            if (time >= keys[track.m_key_count - 1].m_time)
                return keys[track.m_key_count - 1].m_value;

            // 定位区段：keys[i].m_time <= time < keys[i+1].m_time
            size_t lo = 0, hi = track.m_key_count - 1;
            while (hi - lo > 1)
            {
                const size_t mid = (lo + hi) / 2;
                if (keys[mid].m_time <= time)
                    lo = mid;
                else
                    hi = mid;
            }

            const je_animation_key_t& k0 = keys[lo];
            const je_animation_key_t& k1 = keys[lo + 1];

            const float dt = k1.m_time - k0.m_time;
            const float u = dt > 0.f ? (time - k0.m_time) / dt : 0.f;

            switch (k0.m_interp)
            {
            case JE_ANIMATION_INTERP_CONSTANT:
                return k0.m_value;

            case JE_ANIMATION_INTERP_LINEAR:
                return _mix_values(k0.m_value, k1.m_value, u);

            case JE_ANIMATION_INTERP_HERMITE:
            {
                assert(k0.m_tangents != nullptr && k1.m_tangents != nullptr);

                je_animation_value_t result = k0.m_value;
                const float* p0 = _value_channels_of(k0.m_value);
                const float* p1 = _value_channels_of(k1.m_value);
                float* pr = _value_channels_of(result);
                const size_t channels = _value_channels(k0.m_value.m_type);

                for (size_t c = 0; c < channels; ++c)
                {
                    // 切线布局：每通道 (入导数, 出导数)
                    const float m0 = k0.m_tangents[c * 2 + 1];
                    const float m1 = k1.m_tangents[c * 2];
                    pr[c] = _hermite(p0[c], p1[c], m0, m1, dt, u);
                }
                return result;
            }
            default:
                return k0.m_value;
            }
        }

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
    }
}

// 采样累加缓冲（C API 中的不透明类型，定义须位于全局作用域），
// 见 jeecs.hpp 的 je_animation_sample_buffer_t 注释
struct je_animation_sample_buffer_t
{
    std::vector<jeecs::animation::sample_entry> m_entries;
};

using namespace jeecs;

namespace jeecs::animation
{
    using namespace slice_requirement;

    AnimatorSystem::AnimatorSystem(game_world w)
        : game_system(w)
    {
    }

    namespace
    {
        void _push_frame_event(
            Animation::Animator& animator, const char* event_name)
        {
            if (animator.m_frame_events.size() >= kMaxFrameEvents)
            {
                debug::logwarn(
                    "Animation events overflow (max %zu), '%s' dropped.",
                    kMaxFrameEvents, event_name);
                return;
            }
            animator.m_frame_events.push_back(event_name);
        }

        // 触发 low/high 之间被越过的事件：
        //     direction > 0 —— (low, high]
        //     direction < 0 —— [low, high)
        void _fire_events_crossed(
            Animation::Animator& animator,
            const je_animation_action_data_t& action,
            double low, double high, int direction)
        {
            for (size_t i = 0; i < action.m_event_count; ++i)
            {
                const je_animation_event_t& e = action.m_events[i];
                const bool crossed = direction > 0
                    ? (e.m_time > low && e.m_time <= high)
                    : (e.m_time >= low && e.m_time < high);
                if (crossed)
                    _push_frame_event(animator, e.m_name);
            }
        }

        // 推进一个动作的播放时间并处理循环与事件；
        // 非循环动作到达任一端点时 out_finished 置位，时间钳制在端点。
        void _advance_time(
            Animation::Animator& animator,
            const je_animation_action_data_t& action,
            double scaled_dt,
            double& io_time,
            int& io_direction,
            bool& out_finished)
        {
            const double duration = action.m_duration;
            if (duration <= 0.0)
            {
                io_time = 0.0;
                out_finished = true;
                return;
            }

            double from = io_time;
            double to = io_time + scaled_dt * io_direction;

            switch (action.m_loop_mode)
            {
            case JE_ANIMATION_LOOP_NONE:
                if (to >= duration && io_direction > 0)
                {
                    _fire_events_crossed(animator, action, from, duration, +1);
                    io_time = duration;
                    out_finished = true;
                }
                else if (to <= 0.0 && io_direction < 0)
                {
                    _fire_events_crossed(animator, action, 0.0, from, -1);
                    io_time = 0.0;
                    out_finished = true;
                }
                else
                {
                    _fire_events_crossed(animator, action, from, to, io_direction);
                    io_time = to;
                }
                break;

            case JE_ANIMATION_LOOP_REPEAT:
                while (to >= duration)
                {
                    _fire_events_crossed(animator, action, from, duration, +1);
                    to -= duration;
                    from = 0.0;
                }
                while (to < 0.0)
                {
                    _fire_events_crossed(animator, action, 0.0, from, -1);
                    to += duration;
                    from = duration;
                }
                _fire_events_crossed(animator, action, from, to, io_direction);
                io_time = to;
                break;

            case JE_ANIMATION_LOOP_PINGPONG:
                while (to >= duration || to < 0.0)
                {
                    if (io_direction > 0)
                    {
                        _fire_events_crossed(animator, action, from, duration, +1);
                        to = 2.0 * duration - to;
                        io_direction = -1;
                        from = duration;
                    }
                    else
                    {
                        _fire_events_crossed(animator, action, 0.0, from, -1);
                        to = -to;
                        io_direction = 1;
                        from = 0.0;
                    }
                }
                _fire_events_crossed(animator, action, from, to, io_direction);
                io_time = to;
                break;

            default:
                io_time = to;
                break;
            }
        }

        void _sample_action_into_buffer(
            const je_animation_action_data_t* action,
            float time,
            float weight,
            je_animation_sample_buffer_t* buffer)
        {
            for (size_t i = 0; i < action->m_track_count; ++i)
            {
                const je_animation_track_t& track = action->m_tracks[i];
                if (track.m_key_count == 0)
                    continue;

                const je_animation_value_t value = _sample_track(track, time);

                sample_entry* existed = nullptr;
                for (sample_entry& entry : buffer->m_entries)
                {
                    if (entry.m_kind != track.m_kind
                        || entry.m_additive != track.m_offset_mode)
                        continue;

                    if (track.m_kind == JE_ANIMATION_TRACK_MEMBER)
                    {
                        if (entry.m_component_type == track.m_component_type
                            && entry.m_member_info == track.m_member_info)
                            existed = &entry;
                    }
                    else if (0 == strcmp(
                        entry.m_uniform_name, track.m_uniform_name))
                        existed = &entry;

                    if (existed != nullptr)
                        break;
                }

                if (existed != nullptr)
                {
                    if (existed->m_additive)
                        _accumulate_values(&existed->m_value, value, weight);
                    else
                    {
                        const float t = weight / (existed->m_weight + weight);
                        existed->m_value =
                            _mix_values(existed->m_value, value, t);
                        existed->m_weight += weight;
                    }
                }
                else
                {
                    sample_entry entry{};
                    entry.m_kind = track.m_kind;
                    entry.m_additive = track.m_offset_mode;
                    entry.m_component_type = track.m_component_type;
                    entry.m_member_info = track.m_member_info;
                    entry.m_uniform_name = track.m_uniform_name;
                    entry.m_value = value;
                    entry.m_weight = weight;

                    if (entry.m_additive)
                    {
                        // 叠加条目以 value*weight 起始，后续采样继续累加
                        je_animation_value_t zero{};
                        zero.m_type = value.m_type;
                        entry.m_value = zero;
                        _accumulate_values(&entry.m_value, value, weight);
                        entry.m_weight = 0.f;
                    }
                    buffer->m_entries.push_back(entry);
                }
            }
        }

        void _apply_value_to_member(
            void* member_addr,
            const je_member_info_t* member_info,
            const je_animation_value_t& value,
            bool additive)
        {
            switch (value.m_type)
            {
            case JE_ANIMATION_VALUE_INT:
                if (additive)
                    *(int*)member_addr += value.m_value.i32;
                else
                    *(int*)member_addr = value.m_value.i32;
                break;
            case JE_ANIMATION_VALUE_FLOAT:
                if (additive)
                    *(float*)member_addr += value.m_value.f32;
                else
                    *(float*)member_addr = value.m_value.f32;
                break;
            case JE_ANIMATION_VALUE_VEC2:
            {
                const math::vec2 v(
                    value.m_value.v2[0], value.m_value.v2[1]);
                if (additive)
                    *(math::vec2*)member_addr += v;
                else
                    *(math::vec2*)member_addr = v;
                break;
            }
            case JE_ANIMATION_VALUE_VEC3:
            {
                const math::vec3 v(
                    value.m_value.v3[0], value.m_value.v3[1], value.m_value.v3[2]);
                if (additive)
                    *(math::vec3*)member_addr += v;
                else
                    *(math::vec3*)member_addr = v;
                break;
            }
            case JE_ANIMATION_VALUE_VEC4:
            {
                if (member_info->m_member_type
                    == jeecs::typing::of<math::color>())
                {
                    auto* color_member = (math::color*)member_addr;
                    if (additive)
                        *color_member = math::color(
                            color_member->r + value.m_value.v4[0],
                            color_member->g + value.m_value.v4[1],
                            color_member->b + value.m_value.v4[2],
                            color_member->a + value.m_value.v4[3]);
                    else
                        *color_member = math::color(
                            value.m_value.v4[0], value.m_value.v4[1],
                            value.m_value.v4[2], value.m_value.v4[3]);
                }
                else
                {
                    const math::vec4 v(
                        value.m_value.v4[0], value.m_value.v4[1],
                        value.m_value.v4[2], value.m_value.v4[3]);
                    if (additive)
                        *(math::vec4*)member_addr += v;
                    else
                        *(math::vec4*)member_addr = v;
                }
                break;
            }
            case JE_ANIMATION_VALUE_QUAT4:
            {
                const math::quat q(
                    value.m_value.q4[0], value.m_value.q4[1],
                    value.m_value.q4[2], value.m_value.q4[3]);
                if (additive)
                    *(math::quat*)member_addr = *(math::quat*)member_addr * q;
                else
                    *(math::quat*)member_addr = q;
                break;
            }
            default:
                break;
            }
        }

        void _apply_value_to_uniform(
            Renderer::Shaders* shaders,
            const char* uniform_name,
            const je_animation_value_t& value)
        {
            switch (value.m_type)
            {
            case JE_ANIMATION_VALUE_INT:
                shaders->set_uniform(uniform_name, value.m_value.i32);
                break;
            case JE_ANIMATION_VALUE_FLOAT:
                shaders->set_uniform(uniform_name, value.m_value.f32);
                break;
            case JE_ANIMATION_VALUE_VEC2:
                shaders->set_uniform(
                    uniform_name, math::vec2(value.m_value.v2[0], value.m_value.v2[1]));
                break;
            case JE_ANIMATION_VALUE_VEC3:
                shaders->set_uniform(uniform_name, math::vec3(
                    value.m_value.v3[0], value.m_value.v3[1], value.m_value.v3[2]));
                break;
            case JE_ANIMATION_VALUE_VEC4:
                shaders->set_uniform(uniform_name, math::vec4(
                    value.m_value.v4[0], value.m_value.v4[1],
                    value.m_value.v4[2], value.m_value.v4[3]));
                break;
            default:
                debug::logerr(
                    "Bad animation data type(%d) when trying set data of uniform variable '%s', please check.",
                    (int)value.m_type, uniform_name);
                break;
            }
        }

        void _apply_buffer_to_entity(
            const je_game_entity_t& raw_entity,
            /* OPTIONAL */ Renderer::Shaders* shaders,
            const je_animation_sample_buffer_t& buffer)
        {
            for (const sample_entry& entry : buffer.m_entries)
            {
                if (entry.m_kind == JE_ANIMATION_TRACK_MEMBER)
                {
                    auto* component_addr = je_ecs_world_entity_component_get(
                        &raw_entity, entry.m_component_type->m_id);
                    if (component_addr == nullptr)
                        continue;

                    auto* member_addr = (void*)(
                        entry.m_member_info->m_member_offset
                        + (intptr_t)component_addr);

                    _apply_value_to_member(
                        member_addr, entry.m_member_info,
                        entry.m_value, entry.m_additive);
                }
                else if (shaders != nullptr)
                {
                    _apply_value_to_uniform(
                        shaders, entry.m_uniform_name, entry.m_value);
                }
            }
        }

        void _update_animator(
            const game_entity& e,
            Animation::Animator& animator,
            /* OPTIONAL */ Renderer::Shaders* shaders,
            float raw_deltatime)
        {
            auto& state = animator.m_playing;
            if (!state.has_action())
                return;

            auto* animation_set = animator.m_animation_resource.get();

            const je_animation_action_data_t* current_action =
                animation_set->find_action(state.m_action.c_str());
            if (current_action == nullptr)
            {
                debug::logwarn(
                    "Animation action '%s' not found in '%s', stop playing.",
                    state.m_action.c_str(),
                    animator.m_animation.m_path.c_str());
                state.stop();
                return;
            }

            if (state.m_paused)
            {
                // 暂停时冻结在当前姿态
                je_animation_sample_buffer_t buffer;
                _sample_action_into_buffer(
                    current_action, (float)state.m_time, 1.f, &buffer);
                _apply_buffer_to_entity(e._m_raw, shaders, buffer);
                return;
            }

            const float scaled_dt = raw_deltatime * animator.speed;

            _advance_time(
                animator, *current_action, scaled_dt,
                state.m_time, state.m_direction, state.m_finished);

            const je_animation_action_data_t* fading_action = nullptr;
            float fade_weight = 0.f;
            if (state.m_fade_duration > 0.f
                && state.m_fading_action.size() != 0)
            {
                fading_action =
                    animation_set->find_action(state.m_fading_action.c_str());
                if (fading_action == nullptr)
                {
                    // 过渡目标丢失，放弃过渡（保持当前动作）
                    debug::logwarn(
                        "Animation action '%s' (crossfade target) not found in '%s'.",
                        state.m_fading_action.c_str(),
                        animator.m_animation.m_path.c_str());
                    animator.m_playing.clear_fade();
                }
                else
                {
                    bool fading_finished = false;
                    _advance_time(
                        animator, *fading_action, scaled_dt,
                        state.m_fading_time, state.m_fading_direction,
                        fading_finished);

                    state.m_fade_elapsed += std::fabs(scaled_dt);
                    fade_weight = state.m_fade_elapsed / state.m_fade_duration;
                    if (fade_weight >= 1.f)
                    {
                        fade_weight = 1.f;
                        animator.m_playing.finish_fade();
                    }
                }
            }

            je_animation_sample_buffer_t buffer;
            _sample_action_into_buffer(
                current_action, (float)state.m_time, 1.f - fade_weight, &buffer);
            if (fading_action != nullptr && fade_weight > 0.f)
                _sample_action_into_buffer(
                    fading_action, (float)state.m_fading_time,
                    fade_weight, &buffer);

            _apply_buffer_to_entity(e._m_raw, shaders, buffer);
        }
    }

    void AnimatorSystem::StateUpdate()
    {
        const float dt = deltatime();
        for (auto&& [e, animator, shaders] : query_entity<
            view typesof(
                Animation::Animator&, Renderer::Shaders*
            )
        >())
        {
            animator.m_frame_events.clear();

            // 资源加载/换源：路径变化时重新加载并复位播放状态。
            // m_animation_resource 由组件构造保证始终有效（初始为空动画）
            if (0 != strcmp(
                animator.m_loaded_path.c_str(),
                animator.m_animation.m_path.c_str()))
            {
                animator.m_animation_resource =
                    graphic::animation::load(animator.m_animation.m_path.cpp_str());
                animator.m_loaded_path = animator.m_animation.m_path;
                animator.m_playing.stop();
                animator.m_autoplay_played.reset();
            }

            // autoplay：每个动作值只触发一次（重新换源后可再次触发）；
            // 清空 autoplay 只重置记录，不打断当前播放
            const bool autoplay_changed =
                animator.m_animation.m_autoplay_action.has_value()
                != animator.m_autoplay_played.has_value()
                || (animator.m_animation.m_autoplay_action.has_value()
                    && animator.m_autoplay_played.has_value()
                    && 0 != strcmp(
                        animator.m_animation.m_autoplay_action.value().c_str(),
                        animator.m_autoplay_played.value().c_str()));
            if (autoplay_changed)
            {
                if (animator.m_animation.m_autoplay_action.has_value())
                {
                    animator.m_autoplay_played.emplace() =
                        animator.m_animation.m_autoplay_action.value();
                    animator.m_playing.play(
                        animator.m_animation.m_autoplay_action.value().c_str());
                }
                else
                    animator.m_autoplay_played.reset();
            }

            _update_animator(e, animator, shaders, dt);
        }
    }
}

// ===== C API：采样 =====

je_animation_sample_buffer_t* je_animation_sample_buffer_create(void)
{
    return new je_animation_sample_buffer_t();
}

void je_animation_sample_buffer_destroy(je_animation_sample_buffer_t* buffer)
{
    assert(buffer != nullptr);
    delete buffer;
}

void je_animation_sample_buffer_clear(je_animation_sample_buffer_t* buffer)
{
    assert(buffer != nullptr);
    buffer->m_entries.clear();
}

void je_animation_action_sample(
    const je_animation_action_data_t* action,
    float time,
    float weight,
    je_animation_sample_buffer_t* buffer)
{
    assert(action != nullptr);
    assert(buffer != nullptr);

    if (weight <= 0.f)
        return;

    jeecs::animation::_sample_action_into_buffer(action, time, weight, buffer);
}

void je_animation_sample_buffer_apply(
    const je_game_entity_t* entity,
    je_animation_sample_buffer_t* buffer)
{
    assert(entity != nullptr);
    assert(buffer != nullptr);

    auto* shaders = (Renderer::Shaders*)je_ecs_world_entity_component_get(
        entity, jeecs::typing::id<Renderer::Shaders>());

    jeecs::animation::_apply_buffer_to_entity(*entity, shaders, *buffer);
}

// ===== C API：播放控制 =====

namespace
{
    Animation::Animator* _animator_of(const je_game_entity_t* entity)
    {
        assert(entity != nullptr);
        auto* animator = (Animation::Animator*)je_ecs_world_entity_component_get(
            entity, jeecs::typing::id<Animation::Animator>());
        assert(animator != nullptr); // 实体必须持有 Animation::Animator 组件
        return animator;
    }
}

void je_animation_entity_play(const je_game_entity_t* entity, const char* action_name)
{
    assert(action_name != nullptr);
    _animator_of(entity)->m_playing.play(action_name);
}

void je_animation_entity_crossfade(
    const je_game_entity_t* entity,
    const char* action_name,
    float fade_duration)
{
    assert(action_name != nullptr);
    _animator_of(entity)->m_playing.crossfade(action_name, fade_duration);
}

void je_animation_entity_stop(const je_game_entity_t* entity)
{
    _animator_of(entity)->m_playing.stop();
}

void je_animation_entity_pause(const je_game_entity_t* entity)
{
    _animator_of(entity)->m_playing.pause();
}

void je_animation_entity_resume(const je_game_entity_t* entity)
{
    _animator_of(entity)->m_playing.resume();
}

bool je_animation_entity_is_playing(const je_game_entity_t* entity)
{
    return _animator_of(entity)->m_playing.is_playing();
}

/* OPTIONAL */ const char* je_animation_entity_action_get(const je_game_entity_t* entity)
{
    auto& state = _animator_of(entity)->m_playing;
    return state.has_action() ? state.m_action.c_str() : nullptr;
}

float je_animation_entity_duration_get(const je_game_entity_t* entity)
{
    auto* animator = _animator_of(entity);
    auto& state = animator->m_playing;
    if (!state.has_action())
        return 0.f;

    const je_animation_action_data_t* action =
        animator->m_animation_resource->find_action(state.m_action.c_str());
    return action != nullptr ? action->m_duration : 0.f;
}

float je_animation_entity_progress_get(const je_game_entity_t* entity)
{
    auto* animator = _animator_of(entity);
    auto& state = animator->m_playing;
    if (!state.has_action())
        return 0.f;

    const je_animation_action_data_t* action =
        animator->m_animation_resource->find_action(state.m_action.c_str());
    if (action == nullptr || action->m_duration <= 0.f)
        return 0.f;

    return (float)std::clamp(
        state.m_time / action->m_duration, 0.0, 1.0);
}

size_t je_animation_entity_events_get(
    const je_game_entity_t* entity,
    /* OPTIONAL */ const char** out_event_names,
    size_t out_event_names_capacity)
{
    auto& events = _animator_of(entity)->m_frame_events;
    if (out_event_names != nullptr)
    {
        const size_t copy_count =
            std::min(events.size(), out_event_names_capacity);
        for (size_t i = 0; i < copy_count; ++i)
            out_event_names[i] = events[i].c_str();
    }
    return events.size();
}
