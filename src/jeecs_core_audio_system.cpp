#define JE_IMPL
#include "jeecs/audio.hpp"
#include "jeecs/basic.hpp"
#include "jeecs/components.hpp"
#include "jeecs/game.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_core_audio_system.hpp"
#include "jeecs_core_tilemap_model.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace jeecs::audio
{
    using namespace slice_requirement;

    namespace
    {
        constexpr float OCCLUSION_EPSILON = 1e-4f;

        struct occluder_snapshot
        {
            math::vec3 m_center;
            math::quat m_rotation;
            math::vec3 m_half_extent;
            float m_occlusion;
            float m_muffle;
        };
        struct tilemap_snapshot
        {
            const audio_map_grid* m_grid;
            math::vec3 m_position;
            math::quat m_rotation;
            math::vec3 m_scale;
        };
        struct occlusion_result
        {
            float m_gain;
            float m_hf;
        };

        // 把地图的所有格子在所有图层上的音频系数合并成两张平整化网格
        // （同一格取各层最大值）。只在 map_stamp 变化时调用。
        void rebuild_audio_grid(
            audio_map_grid& grid, Tilemap::MapDocument& document)
        {
            auto& registry = Tilemap::documents::inst();
            registry.ensure_tilesets(document);

            grid.m_stamp = registry.map_stamp(document);
            grid.m_width = document.width;
            grid.m_height = document.height;

            const size_t cell_count =
                (size_t)document.width * (size_t)document.height;

            grid.m_occlusion.assign(cell_count, 0.0f);
            grid.m_muffle.assign(cell_count, 0.0f);

            for (size_t i = 0; i < cell_count; ++i)
            {
                float occlusion = 0.0f;
                float muffle = 0.0f;

                for (int32_t layer = 0;
                    layer < (int32_t)document.layers.size(); ++layer)
                {
                    const int32_t value = document.layers[layer].grid[i];
                    if (value == 0)
                        continue;

                    const auto cell = Tilemap::decode_tile_value(value);
                    if (!cell.has_value()
                        || cell->tileset_idx < 0
                        || cell->tileset_idx >= (int32_t)document.tilesets.size()
                        || !document.tilesets[cell->tileset_idx].resolved.has_value())
                        continue;

                    const auto* tileset =
                        document.tilesets[cell->tileset_idx].resolved->get();
                    const auto* def = tileset->tile_def(cell->local_id);

                    if (def != nullptr)
                    {
                        occlusion = std::max(occlusion, def->audio_occlusion);
                        muffle = std::max(muffle, def->audio_muffle);
                    }
                }

                grid.m_occlusion[i] = std::clamp(occlusion, 0.0f, 1.0f);
                grid.m_muffle[i] = std::clamp(muffle, 0.0f, 1.0f);
            }
        }

        // 世界坐标 -> 地图局部网格坐标（1 格 = 1 单位，y 向下为正）。
        math::vec3 to_grid_space(
            const math::quat& rotation,
            const math::vec3& position,
            const math::vec3& scale,
            const math::vec3& world)
        {
            const math::vec3 local = rotation.inverse() * (world - position);

            return math::vec3(
                local.x / scale.x,
                -local.y / scale.y,
                0.0f);
        }

        // Amanatides-Woo 2D DDA：遍历线段穿过的网格。
        // skip_first 为 true 时跳过起点格（听众自身所在格）。
        template <typename TVisitor>
        void visit_segment_cells(
            float x0, float y0, float x1, float y1,
            bool skip_first, TVisitor&& visitor)
        {
            int32_t cx = (int32_t)std::floor(x0);
            int32_t cy = (int32_t)std::floor(y0);

            const int32_t ex = (int32_t)std::floor(x1);
            const int32_t ey = (int32_t)std::floor(y1);

            const int32_t step_x = x1 > x0 ? 1 : -1;
            const int32_t step_y = y1 > y0 ? 1 : -1;

            const float dx = x1 - x0;
            const float dy = y1 - y0;

            const float t_delta_x = std::fabs(dx) < 1e-9f
                ? std::numeric_limits<float>::infinity()
                : 1.0f / std::fabs(dx);
            const float t_delta_y = std::fabs(dy) < 1e-9f
                ? std::numeric_limits<float>::infinity()
                : 1.0f / std::fabs(dy);

            float t_max_x = std::fabs(dx) < 1e-9f
                ? std::numeric_limits<float>::infinity()
                : ((step_x > 0) ? (cx + 1.0f - x0) : (x0 - cx)) / std::fabs(dx);
            float t_max_y = std::fabs(dy) < 1e-9f
                ? std::numeric_limits<float>::infinity()
                : ((step_y > 0) ? (cy + 1.0f - y0) : (y0 - cy)) / std::fabs(dy);

            const long long max_steps =
                std::llabs((long long)ex - cx)
                + std::llabs((long long)ey - cy)
                + 2;

            bool first = skip_first;
            for (long long i = 0; i < max_steps; ++i)
            {
                if (!first)
                    visitor(cx, cy);
                first = false;

                if (cx == ex && cy == ey)
                    break;

                if (t_max_x < t_max_y)
                {
                    cx += step_x;
                    t_max_x += t_delta_x;
                }
                else
                {
                    cy += step_y;
                    t_max_y += t_delta_y;
                }
            }
        }

        // 把线段裁剪到网格矩形 [0,width] × [0,height]（Liang-Barsky）。
        // 返回 false 表示线段与地图不相交；t0/t1 为裁剪后的参数区间。
        bool clip_segment_to_grid(
            float x0, float y0, float x1, float y1,
            float width, float height, float& t0, float& t1)
        {
            t0 = 0.0f;
            t1 = 1.0f;

            const float dx = x1 - x0;
            const float dy = y1 - y0;

            auto clip_test = [&](float p, float q) -> bool
            {
                if (p == 0.0f)
                    return q >= 0.0f;

                const float r = q / p;

                if (p < 0.0f)
                {
                    if (r > t1)
                        return false;
                    if (r > t0)
                        t0 = r;
                }
                else
                {
                    if (r < t0)
                        return false;
                    if (r < t1)
                        t1 = r;
                }
                return true;
            };

            return clip_test(-dx, x0)
                && clip_test(dx, width - x0)
                && clip_test(-dy, y0)
                && clip_test(dy, height - y0);
        }

        // 线段是否穿过 2D 旋转矩形；起点在矩形内时不算（避免听众自身所在格）。
        bool segment_hits_box_2d(
            const math::vec2& from,
            const math::vec2& to,
            const math::vec3& center,
            const math::quat& rotation,
            const math::vec2& half_extent)
        {
            const math::quat inverse = rotation.inverse();

            const math::vec3 local_from =
                inverse * (math::vec3(from.x, from.y, 0.0f) - center);
            const math::vec3 local_to =
                inverse * (math::vec3(to.x, to.y, 0.0f) - center);

            const float dx = local_to.x - local_from.x;
            const float dy = local_to.y - local_from.y;

            float t_enter = 0.0f;
            float t_exit = 1.0f;

            auto slab = [&](float p, float d, float half) -> bool
            {
                if (std::fabs(d) < 1e-9f)
                    return p >= -half && p <= half;

                float t0 = (-half - p) / d;
                float t1 = (half - p) / d;

                if (t0 > t1)
                    std::swap(t0, t1);

                t_enter = std::max(t_enter, t0);
                t_exit = std::min(t_exit, t1);

                return t_enter <= t_exit;
            };

            if (!slab(local_from.x, dx, half_extent.x)
                || !slab(local_from.y, dy, half_extent.y))
                return false;

            return t_enter > OCCLUSION_EPSILON && t_enter <= 1.0f;
        }

        // 线段是否穿过 3D OBB；起点在盒内时不算。
        bool segment_hits_box_3d(
            const math::vec3& from,
            const math::vec3& to,
            const math::vec3& center,
            const math::quat& rotation,
            const math::vec3& half_extent)
        {
            const math::quat inverse = rotation.inverse();

            const math::vec3 local_from = inverse * (from - center);
            const math::vec3 local_to = inverse * (to - center);

            const math::vec3 d = local_to - local_from;

            float t_enter = 0.0f;
            float t_exit = 1.0f;

            auto slab = [&](float p, float dir, float half) -> bool
            {
                if (std::fabs(dir) < 1e-9f)
                    return p >= -half && p <= half;

                float t0 = (-half - p) / dir;
                float t1 = (half - p) / dir;

                if (t0 > t1)
                    std::swap(t0, t1);

                t_enter = std::max(t_enter, t0);
                t_exit = std::min(t_exit, t1);

                return t_enter <= t_exit;
            };

            if (!slab(local_from.x, d.x, half_extent.x)
                || !slab(local_from.y, d.y, half_extent.y)
                || !slab(local_from.z, d.z, half_extent.z))
                return false;

            return t_enter > OCCLUSION_EPSILON && t_enter <= 1.0f;
        }

        bool point_in_box(
            const math::vec3& point,
            const math::vec3& center,
            const math::quat& rotation,
            const math::vec3& half_extent,
            bool use_2d)
        {
            const math::vec3 local = rotation.inverse() * (point - center);

            if (std::fabs(local.x) > half_extent.x)
                return false;
            if (std::fabs(local.y) > half_extent.y)
                return false;
            if (!use_2d && std::fabs(local.z) > half_extent.z)
                return false;

            return true;
        }

        // 累乘线段穿过的格子的缓存音频系数。起点格由 DDA 跳过。
        // 网格已按地图/图集版本平整化，热路径只做数组索引。
        void accumulate_tilemap_occlusion(
            const tilemap_snapshot& snapshot,
            const math::vec3& listener,
            const math::vec3& source,
            occlusion_result& result)
        {
            const auto& scale = snapshot.m_scale;
            const auto* grid = snapshot.m_grid;

            if (std::fabs(scale.x) < 1e-6f || std::fabs(scale.y) < 1e-6f
                || grid->m_width <= 0 || grid->m_height <= 0)
                return;

            const math::vec3 from = to_grid_space(
                snapshot.m_rotation, snapshot.m_position, scale, listener);
            const math::vec3 to = to_grid_space(
                snapshot.m_rotation, snapshot.m_position, scale, source);

            // 裁剪到地图矩形，避免远处声源让 DDA 遍历地图外的海量空格
            float t0 = 0.0f;
            float t1 = 1.0f;
            if (!clip_segment_to_grid(
                    from.x, from.y, to.x, to.y,
                    (float)grid->m_width, (float)grid->m_height,
                    t0, t1))
                return;

            const float dx = to.x - from.x;
            const float dy = to.y - from.y;

            const float start_x = from.x + dx * t0;
            const float start_y = from.y + dy * t0;
            const float end_x = from.x + dx * t1;
            const float end_y = from.y + dy * t1;

            // 起点本来就在地图内时跳过起点格；从地图外切入时保留入口格
            const bool skip_first = t0 <= 1e-6f;

            visit_segment_cells(start_x, start_y, end_x, end_y, skip_first,
                [&](int32_t cx, int32_t cy)
                {
                    if (cx < 0 || cy < 0
                        || cx >= grid->m_width || cy >= grid->m_height)
                        return;

                    const size_t index =
                        (size_t)cy * (size_t)grid->m_width + (size_t)cx;

                    result.m_gain *= 1.0f - grid->m_occlusion[index];
                    result.m_hf *= 1.0f - grid->m_muffle[index];
                });
        }
    }

    AudioUpdatingSystem::AudioUpdatingSystem(game_world w)
        : game_system(w)
        , m_reverb_active(false)
        , m_reverb_decay_time(0.0f)
        , m_reverb_gain(0.0f)
        , m_reverb_gain_hf(0.0f)
        , m_reverb_diffusion(0.0f)
        , m_reverb_generation(0)
        , m_reverb_effect()
        , m_reverb_slot()
        , m_audio_maps()
    {
    }

    void AudioUpdatingSystem::CommitUpdate()
    {
        // ==================== 1. 听众 ====================
        bool listener_present = false;
        bool listener_2d = false;
        math::vec3 listener_position(0.0f, 0.0f, 0.0f);

        for (auto&& [listener, trans] : query<
            view typesof(Audio::Listener&, Transform::Translation&)
        >())
        {
            listener_present = true;
            listener_2d = listener.use_2d;

            math::vec3 position = trans.world_position;
            math::vec3 velocity(0.0f, 0.0f, 0.0f);

            const float delta = std::max(deltatime(), 0.0001f);

            if (listener.has_last_position)
                velocity = (position - listener.last_position) / delta;

            listener.last_position = position;
            listener.has_last_position = true;

            if (listener.use_2d)
            {
                position.z = 0.0f;
                velocity.z = 0.0f;
            }

            listener_position = position;

            const auto face = trans.world_rotation * listener.face;
            const auto up = trans.world_rotation * listener.up;

            listener::update(
                [&](je_audio_listener_t* data)
                {
                    data->m_location[0] = position.x;
                    data->m_location[1] = position.y;
                    data->m_location[2] = position.z;

                    data->m_velocity[0] = velocity.x;
                    data->m_velocity[1] = velocity.y;
                    data->m_velocity[2] = velocity.z;

                    data->m_forward[0] = face.x;
                    data->m_forward[1] = face.y;
                    data->m_forward[2] = face.z;

                    data->m_upward[0] = up.x;
                    data->m_upward[1] = up.y;
                    data->m_upward[2] = up.z;

                    data->m_gain = listener.volume;
                    data->m_distance_model = listener.distance_model;
                    data->m_doppler_factor = listener.doppler_factor;
                });
        }

        // ==================== 2. 混响区域 ====================
        bool zone_active = false;
        float zone_decay_time = 0.0f;
        float zone_gain = 0.0f;
        float zone_gain_hf = 0.0f;
        float zone_diffusion = 0.0f;

        if (listener_present)
        {
            float best_volume = std::numeric_limits<float>::max();

            for (auto&& [zone, trans] : query<
                view typesof(Audio::ReverbZone&, Transform::Translation&)
            >())
            {
                const math::vec3 half_extent(
                    std::fabs(zone.size.x * trans.local_scale.x) * 0.5f,
                    std::fabs(zone.size.y * trans.local_scale.y) * 0.5f,
                    std::fabs(zone.size.z * trans.local_scale.z) * 0.5f);

                if (!point_in_box(
                        listener_position,
                        trans.world_position,
                        trans.world_rotation,
                        half_extent,
                        listener_2d))
                    continue;

                const float volume =
                    half_extent.x * half_extent.y * half_extent.z;

                if (volume < best_volume)
                {
                    best_volume = volume;
                    zone_active = true;

                    zone_decay_time = zone.decay_time;
                    zone_gain = zone.gain;
                    zone_gain_hf = zone.gain_hf;
                    zone_diffusion = zone.diffusion;
                }
            }
        }

        const bool reverb_changed =
            (zone_active != m_reverb_active)
            || (zone_active
                && (zone_decay_time != m_reverb_decay_time
                    || zone_gain != m_reverb_gain
                    || zone_gain_hf != m_reverb_gain_hf
                    || zone_diffusion != m_reverb_diffusion));

        if (reverb_changed)
        {
            m_reverb_active = zone_active;
            m_reverb_decay_time = zone_decay_time;
            m_reverb_gain = zone_gain;
            m_reverb_gain_hf = zone_gain_hf;
            m_reverb_diffusion = zone_diffusion;

            ++m_reverb_generation;

            if (zone_active)
            {
                if (!m_reverb_effect.has_value())
                    m_reverb_effect.emplace(
                        effect<je_audio_effect_reverb_t>::create());

                if (!m_reverb_slot.has_value())
                    m_reverb_slot.emplace(effect_slot::create());

                m_reverb_effect.value()->update(
                    [&](je_audio_effect_reverb_t* reverb)
                    {
                        reverb->m_decay_time = zone_decay_time;
                        reverb->m_gain = zone_gain;
                        reverb->m_gain_hf = zone_gain_hf;
                        reverb->m_diffusion = zone_diffusion;
                    });

                m_reverb_slot.value()->bind_effect(m_reverb_effect.value());
            }
        }

        // ==================== 3. 遮挡快照 ====================
        std::vector<occluder_snapshot> occluders;
        for (auto&& [occluder, trans] : query<
            view typesof(Audio::Occluder&, Transform::Translation&)
        >())
        {
            occluders.push_back(occluder_snapshot{
                trans.world_position,
                trans.world_rotation,
                math::vec3(
                    std::fabs(occluder.size.x * trans.local_scale.x) * 0.5f,
                    std::fabs(occluder.size.y * trans.local_scale.y) * 0.5f,
                    std::fabs(occluder.size.z * trans.local_scale.z) * 0.5f),
                occluder.occlusion,
                occluder.muffle,
            });
        }

        std::vector<tilemap_snapshot> tilemaps;
        for (auto&& [map, trans] : query<
            view typesof(Tilemap::Map&, Transform::Translation&)
        >())
        {
            if (map.map_path.c_str()[0] == '\0')
                continue;

            const std::string path = map.map_path.cpp_str();

            auto document = Tilemap::documents::inst().observe_map(path);
            if (!document.has_value())
                continue;

            Tilemap::documents::inst().ensure_tilesets(*document.value());

            audio_map_grid& grid = m_audio_maps[path];

            const uint64_t stamp =
                Tilemap::documents::inst().map_stamp(*document.value());

            if (grid.m_stamp != stamp)
                rebuild_audio_grid(grid, *document.value());

            tilemaps.push_back(tilemap_snapshot{
                &grid,
                trans.world_position,
                trans.world_rotation,
                trans.local_scale,
            });
        }

        // ==================== 4. 音源 ====================
        for (auto&& [source, trans] : query<
            view typesof(Audio::Source&, Transform::Translation*)
        >())
        {
            const bool spatial_active = source.spatial && trans != nullptr;

            math::vec3 position(0.0f, 0.0f, 0.0f);
            math::vec3 velocity(0.0f, 0.0f, 0.0f);

            if (spatial_active)
            {
                position = trans->world_position;

                const float delta = std::max(deltatime(), 0.0001f);
                if (source.has_last_position)
                    velocity = (position - source.last_position) / delta;

                source.last_position = position;
                source.has_last_position = true;

                if (listener_present && listener_2d)
                {
                    position.z = 0.0f;
                    velocity.z = 0.0f;
                }
            }
            else
                source.has_last_position = false;

            source.spatial_active = spatial_active;
            source.last_world_position = position;

            occlusion_result occlusion{ 1.0f, 1.0f };

            if (spatial_active && listener_present)
            {
                for (const auto& occluder : occluders)
                {
                    const bool hit = listener_2d
                        ? segment_hits_box_2d(
                            math::vec2(listener_position.x, listener_position.y),
                            math::vec2(position.x, position.y),
                            occluder.m_center,
                            occluder.m_rotation,
                            math::vec2(
                                occluder.m_half_extent.x,
                                occluder.m_half_extent.y))
                        : segment_hits_box_3d(
                            listener_position,
                            position,
                            occluder.m_center,
                            occluder.m_rotation,
                            occluder.m_half_extent);

                    if (hit)
                    {
                        occlusion.m_gain *= 1.0f - std::clamp(
                            occluder.m_occlusion, 0.0f, 1.0f);
                        occlusion.m_hf *= 1.0f - std::clamp(
                            occluder.m_muffle, 0.0f, 1.0f);
                    }
                }

                for (const auto& tilemap : tilemaps)
                    accumulate_tilemap_occlusion(
                        tilemap, listener_position, position, occlusion);
            }

            source.current_occlusion_gain = occlusion.m_gain;
            source.current_occlusion_hf = occlusion.m_hf;

            // 音量总线：变化时提交到声源
            if (source.bus_applied != source.bus)
            {
                source.bus_applied = source.bus;
                source.source->set_bus(source.bus);
            }

            // 遮挡闷音：按需创建低通滤波器并绑定
            if (occlusion.m_hf < 0.999f)
            {
                if (!source.occlusion_filter.has_value())
                    source.occlusion_filter.emplace(filter::create());

                if (source.applied_occlusion_hf != occlusion.m_hf)
                {
                    source.occlusion_filter.value()->update(
                        [&](je_audio_filter_t* filter_data)
                        {
                            filter_data->m_gain_hf = occlusion.m_hf;
                        });

                    source.applied_occlusion_hf = occlusion.m_hf;
                }

                if (!source.occlusion_filter_bound)
                {
                    source.source->set_filter(
                        std::optional<basic::resource<filter>>(
                            source.occlusion_filter.value()));
                    source.occlusion_filter_bound = true;
                }
            }
            else if (source.occlusion_filter_bound)
            {
                source.source->set_filter(std::nullopt);
                source.occlusion_filter_bound = false;
            }

            // 混响区域变化时重绑第 0 路辅助发送
            const bool want_reverb = spatial_active && m_reverb_active;

            if (source.reverb_generation != m_reverb_generation
                || source.reverb_bound != want_reverb)
            {
                source.reverb_generation = m_reverb_generation;
                source.reverb_bound = want_reverb;

                if (want_reverb)
                    source.source->bind_effect_slot(
                        0, m_reverb_slot, std::nullopt);
                else
                    source.source->bind_effect_slot(
                        0, std::nullopt, std::nullopt);
            }

            // 提交空间参数
            source.source->update(
                [&](je_audio_source_t* data)
                {
                    data->m_loop = source.loop;
                    data->m_gain = source.volume * occlusion.m_gain;
                    data->m_pitch = source.pitch;

                    data->m_relative = !spatial_active;

                    if (spatial_active)
                    {
                        data->m_location[0] = position.x;
                        data->m_location[1] = position.y;
                        data->m_location[2] = position.z;

                        data->m_velocity[0] = velocity.x;
                        data->m_velocity[1] = velocity.y;
                        data->m_velocity[2] = velocity.z;
                    }
                    else
                    {
                        data->m_location[0] = 0.0f;
                        data->m_location[1] = 0.0f;
                        data->m_location[2] = 0.0f;

                        data->m_velocity[0] = 0.0f;
                        data->m_velocity[1] = 0.0f;
                        data->m_velocity[2] = 0.0f;
                    }

                    data->m_reference_distance = source.reference_distance;
                    data->m_max_distance = source.max_distance;
                    data->m_rolloff = source.rolloff;
                });

            // 播放状态机
            if (!source.buffer.has_resource())
            {
                source.state = Audio::Source::play_state::STOPPED;

                if (source.source->get_state() != JE_AUDIO_STATE_STOPPED)
                    source.source->stop();
            }
            else
            {
                const bool buffer_updated =
                    source.source->set_playing_buffer(
                        source.buffer.get_resource().value());

                if (source.state == Audio::Source::play_state::STOPPED)
                {
                    if (source.play)
                    {
                        source.source->play();
                        source.state = Audio::Source::play_state::PLAYING;
                    }
                }
                else
                {
                    switch (source.source->get_state())
                    {
                    case JE_AUDIO_STATE_STOPPED:
                        if (!buffer_updated)
                        {
                            // 自然播放结束
                            source.state = Audio::Source::play_state::STOPPED;
                            source.play = false;
                        }
                        else if (source.play)
                            source.source->play();
                        break;

                    case JE_AUDIO_STATE_PAUSED:
                        if (source.play)
                            source.source->play();
                        break;

                    case JE_AUDIO_STATE_PLAYING:
                        if (!source.play)
                            source.source->pause();
                        break;
                    }
                }
            }
        }
    }
}
