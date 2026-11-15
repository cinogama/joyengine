#define JE_IMPL
#include "jeecs/audio.hpp"
#include "jeecs/basic.hpp"
#include "jeecs/components.hpp"
#include "jeecs/game.hpp"
#include "jeecs/logging.hpp"
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
#include <unordered_set>
#include <utility>
#include <vector>

namespace jeecs::audio
{
    using namespace slice_requirement;

    namespace
    {
        constexpr float OCCLUSION_EPSILON = 1e-4f;
        constexpr float REVERB_WEIGHT_EPSILON = 1e-4f;

        // 声源不在听者房间时，穿过墙/门洞的声音仍会激发听者房间的混响。
        // 该系数是这种二次激发的发送上限。
        constexpr float LISTENER_ROOM_EXCITATION = 0.25f;

        // 两个房间之间没有门洞连接时，跨房间湿声按“墙体泄漏”计算：
        // 少量透射 + 强闷音。不能像以前那样无路径就不做路径衰减——那等于
        // 把墙当成 transmission=1、muffle=0 的敞开门，任何门洞（甚至
        // transmission=0.8）反而比无门洞更安静、更闷，听感上几乎没有区别。
        constexpr float WALL_WET_TRANSMISSION = 0.2f;
        constexpr float WALL_WET_MUFFLE = 0.7f;

        // 遮挡平滑：射线遮挡在遮挡体边缘是硬切换，直接应用会让声音在跨过
        // 边缘的一帧突变。对遮挡增益与闷音做指数平滑——进入遮挡（变轻变闷）
        // 稍快，离开遮挡（恢复明亮）稍慢，听感上像声音自然地从墙后出来。
        constexpr float OCCLUSION_ATTACK_SECONDS = 0.08f;
        constexpr float OCCLUSION_RELEASE_SECONDS = 0.25f;
        constexpr float OCCLUSION_SMOOTH_EPSILON = 0.002f;

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
            float t1 = 0.0f;
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

        struct reverb_candidate
        {
            const audio_environment_room* m_room;
            float m_weight;
            float m_hf;
        };

        // EAX 标准混响预设；顺序与 je_audio_effect_eax_reverb_preset_t
        // 去掉 CUSTOM 后一一对应。数值来自 EFX/EAX 规范。
        const je_audio_effect_eax_reverb_t EAX_REVERB_PRESETS[] = {
#include "eax_reverb_presets.inc"
        };

        static_assert(
            sizeof(EAX_REVERB_PRESETS) / sizeof(EAX_REVERB_PRESETS[0])
                == (size_t)(JE_AUDIO_EAX_REVERB_PRESET_COUNT - 1),
            "EAX_REVERB_PRESETS must cover every non-custom preset.");

        je_audio_effect_eax_reverb_t make_reverb_params(
            const audio_environment_room& zone)
        {
            if (zone.m_preset > JE_AUDIO_EAX_REVERB_PRESET_CUSTOM
                && zone.m_preset < JE_AUDIO_EAX_REVERB_PRESET_COUNT)
            {
                return EAX_REVERB_PRESETS[(size_t)zone.m_preset - 1];
            }

            // CUSTOM：以原通用混响的默认值为基线，只覆盖组件暴露的参数。
            je_audio_effect_eax_reverb_t params{};
            params.m_density = 1.0f;
            params.m_diffusion = zone.m_diffusion;
            params.m_gain = zone.m_gain;
            params.m_gain_hf = zone.m_gain_hf;
            params.m_gain_lf = 1.0f;
            params.m_decay_time = zone.m_decay_time;
            params.m_decay_hf_ratio = 0.83f;
            params.m_decay_lf_ratio = 1.0f;
            params.m_reflections_gain = 0.05f;
            params.m_reflections_delay = 0.007f;
            params.m_late_reverb_gain = 1.26f;
            params.m_late_reverb_delay = 0.011f;
            params.m_echo_time = 0.25f;
            params.m_echo_depth = 0.0f;
            params.m_modulation_time = 0.25f;
            params.m_modulation_depth = 0.0f;
            params.m_air_absorption_gain_hf = 0.994f;
            params.m_hf_reference = 5000.0f;
            params.m_lf_reference = 250.0f;
            params.m_room_rolloff_factor = 0.0f;
            params.m_decay_hf_limit = false;
            return params;
        }

        bool same_reverb_params(
            const je_audio_effect_eax_reverb_t& a,
            const je_audio_effect_eax_reverb_t& b)
        {
            // 两个结构体都是值初始化后逐字段赋值的 POD，padding 为 0。
            return std::memcmp(&a, &b, sizeof(a)) == 0;
        }
    }

    audio_map_grid::audio_map_grid()
        : m_stamp(0)
        , m_width(0)
        , m_height(0)
        , m_occlusion()
        , m_muffle()
    {
    }

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
                if (!cell.has_value() || cell->tileset_idx < 0
                    || cell->tileset_idx
                        >= (int32_t)document.tilesets.size()
                    || !document.tilesets[cell->tileset_idx]
                            .resolved.has_value())
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

    AudioUpdatingSystem::AudioUpdatingSystem(game_world w)
        : game_system(w)
        , m_reverb_rooms()
        , m_next_reverb_epoch(0)
        , m_live_environment()
        , m_live_environment_signature(0)
        , m_has_live_environment(false)
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

        // ==================== 2. 组件快照与音频环境 ====================
        // 实时回退输入：Audio::ReverbZone = 房间，Audio::Portal = 门洞。
        std::vector<audio_environment_room_snapshot> room_snapshots;
        std::vector<audio_environment_portal_snapshot> portal_snapshots;

        for (auto&& [zone_entity, zone, trans] :
            query_entity<view typesof(
                Audio::ReverbZone&, Transform::Translation&)>())
        {
            audio_environment_room_snapshot snapshot;
            snapshot.m_source_id = zone_entity._m_raw._m_id;
            snapshot.m_center = trans.world_position;
            snapshot.m_rotation = trans.world_rotation;
            snapshot.m_half_extent = math::vec3(
                std::fabs(zone.size.x * trans.local_scale.x) * 0.5f,
                std::fabs(zone.size.y * trans.local_scale.y) * 0.5f,
                std::fabs(zone.size.z * trans.local_scale.z) * 0.5f);
            snapshot.m_fade = zone.fade;
            snapshot.m_preset = zone.preset;
            snapshot.m_decay_time = zone.decay_time;
            snapshot.m_gain = zone.gain;
            snapshot.m_gain_hf = zone.gain_hf;
            snapshot.m_diffusion = zone.diffusion;
            snapshot.m_send_gain = zone.send_gain;

            room_snapshots.push_back(snapshot);
        }

        for (auto&& [portal_entity, portal, trans] :
            query_entity<view typesof(
                Audio::Portal&, Transform::Translation&)>())
        {
            if (!portal.enabled)
                continue;

            math::vec3 normal = trans.world_rotation * portal.normal;
            if (normal.length() <= 1e-6f)
                continue;
            normal = normal.unit();

            audio_environment_portal_snapshot snapshot;
            snapshot.m_source_id = portal_entity._m_raw._m_id;
            snapshot.m_center = trans.world_position;
            snapshot.m_normal = normal;
            snapshot.m_half_thickness =
                std::fabs(portal.size.z * trans.local_scale.z) * 0.5f;
            snapshot.m_transmission =
                std::clamp(portal.transmission, 0.0f, 1.0f);
            snapshot.m_muffle = std::clamp(portal.muffle, 0.0f, 1.0f);

            portal_snapshots.push_back(snapshot);
        }

        // 实时环境：组件签名变化时重建
        const uint64_t live_signature = hash_audio_environment_inputs(
            room_snapshots, portal_snapshots, listener_2d);

        if (!m_has_live_environment
            || m_live_environment_signature != live_signature)
        {
            m_live_environment = build_audio_environment(
                room_snapshots, portal_snapshots, listener_2d);
            m_live_environment_signature = live_signature;
            m_has_live_environment = true;
        }

        const audio_environment* environment = &m_live_environment;

        // 注意：环境图变化时不整体清空 m_reverb_rooms。房间 id 由确定性
        // 构建产生，拖动房间（尺寸/位置/淡出）只改变几何、声学参数不变，
        // 复用现有效果槽即可；ensure_reverb_room 会按参数差异更新效果，
        // 本帧不再使用的房间由第 5 步的 m_active 扫描回收。整体清空会让
        // 每帧销毁/重建全部 EFX 效果与效果槽，播放中拖动会明显卡顿。

        // 听者所在房间：权重最高者；权重并列（房间嵌套时内外房间同为
        // 1.0）取体积最小者——听者在最内层房间里，混响按该房间算
        const audio_environment_room* listener_reverb_room = nullptr;
        float listener_reverb_weight = 0.0f;
        float listener_reverb_volume = 0.0f;

        if (listener_present && environment != nullptr)
        {
            for (const auto& room : environment->m_rooms)
            {
                const float weight = audio_environment_room_weight(
                    listener_position, room, listener_2d);

                const float volume = room.m_half_extent.x
                    * room.m_half_extent.y * room.m_half_extent.z;

                if (weight > listener_reverb_weight
                    || (weight == listener_reverb_weight
                        && listener_reverb_room != nullptr
                        && volume < listener_reverb_volume))
                {
                    listener_reverb_weight = weight;
                    listener_reverb_room = &room;
                    listener_reverb_volume = volume;
                }
            }
        }

        for (auto& room : m_reverb_rooms)
            room.second.m_active = false;

        // ==================== 3. 遮挡快照 ====================
        // 遮挡体与瓦片地图全部逐帧实时测试；地图网格按版本平整化缓存，
        // 只有编辑地图/图集时才重建（见 rebuild_audio_grid）。
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

        // 确保区域的效果器/效果槽存在，并按需更新参数。
        auto ensure_reverb_room =
            [this](const audio_environment_room& zone) -> reverb_room_runtime&
        {
            reverb_room_runtime& room = m_reverb_rooms[zone.m_id];

            if (!room.m_slot.has_value())
            {
                room.m_effect.emplace(
                    effect<je_audio_effect_eax_reverb_t>::create());
                room.m_slot.emplace(effect_slot::create());
                room.m_slot_epoch = ++m_next_reverb_epoch;
                room.m_has_params = false;
                room.m_slot_gain = -1.0f;
            }

            room.m_active = true;

            const je_audio_effect_eax_reverb_t params =
                make_reverb_params(zone);

            if (!room.m_has_params
                || !same_reverb_params(room.m_params, params))
            {
                room.m_params = params;
                room.m_has_params = true;

                room.m_effect.value()->update(
                    [&](je_audio_effect_eax_reverb_t* data)
                    {
                        *data = params;
                    });

                // EFX 的效果参数更新后需要重新绑定到效果槽才能生效
                room.m_slot.value()->bind_effect(room.m_effect.value());
            }

            if (room.m_slot_gain != zone.m_send_gain)
            {
                room.m_slot_gain = zone.m_send_gain;
                room.m_slot.value()->update(
                    [&](je_audio_effect_slot_t* data)
                    {
                        data->m_gain = zone.m_send_gain;
                    });
            }

            return room;
        };

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

                // 遮挡值做时间平滑：射线在遮挡体边缘是硬切换，直接应用到增益/
                // 闷音会在跨过边缘的瞬间突变。首帧/重新进入空间化时直接对齐，
                // 之后指数逼近目标——进入遮挡稍快、离开遮挡稍慢。增益与闷音
                // 各自按自己的变化方向选时间常数：只闷不变轻的遮挡按闷音的
                // “进入”速率变闷，不会跟着增益的“离开”速率慢慢拖。
            if (!spatial_active || !listener_present)
            {
                source.current_occlusion_gain = 1.0f;
                source.current_occlusion_hf = 1.0f;
                source.occlusion_smoothed = false;
            }
            else if (!source.occlusion_smoothed)
            {
                source.current_occlusion_gain = occlusion.m_gain;
                source.current_occlusion_hf = occlusion.m_hf;
                source.occlusion_smoothed = true;
            }
            else
            {
                const float dt = std::max(deltatime(), 0.0f);

                auto smooth_occlusion =
                    [dt](float current, float target, float tau)
                {
                    if (current == target)
                        return target;

                    const float alpha = 1.0f - std::exp(-dt / tau);
                    const float next = current + (target - current) * alpha;

                    if (std::fabs(next - target) < OCCLUSION_SMOOTH_EPSILON)
                        return target;
                    return next;
                };

                // 每个通道按自己的变化方向选时间常数：变小（进入遮挡）
                // 用 attack，变大（离开遮挡）用 release
                const float gain_tau =
                    occlusion.m_gain < source.current_occlusion_gain
                    ? OCCLUSION_ATTACK_SECONDS
                    : OCCLUSION_RELEASE_SECONDS;
                const float hf_tau =
                    occlusion.m_hf < source.current_occlusion_hf
                    ? OCCLUSION_ATTACK_SECONDS
                    : OCCLUSION_RELEASE_SECONDS;

                source.current_occlusion_gain = smooth_occlusion(
                    source.current_occlusion_gain, occlusion.m_gain, gain_tau);
                source.current_occlusion_hf = smooth_occlusion(
                    source.current_occlusion_hf, occlusion.m_hf, hf_tau);
            }

            occlusion.m_gain = source.current_occlusion_gain;
            occlusion.m_hf = source.current_occlusion_hf;

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

                const bool occlusion_hf_changed =
                    source.applied_occlusion_hf != occlusion.m_hf;

                if (occlusion_hf_changed)
                {
                    source.occlusion_filter.value()->update(
                        [&](je_audio_filter_t* filter_data)
                        {
                            filter_data->m_gain_hf = occlusion.m_hf;
                        });

                    source.applied_occlusion_hf = occlusion.m_hf;
                }

                // 同发送滤波器：OpenAL 在绑定时拷贝滤波器参数，只改滤波器
                // 对象不会生效，闷音变化时必须重新绑定。
                if (!source.occlusion_filter_bound || occlusion_hf_changed)
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

            // ---- 混响发送：环境房间 + 门洞路径 ----
            reverb_candidate reverb_candidates[3];
            int reverb_candidate_count = 0;

            if (spatial_active && environment != nullptr)
            {
                // 声源最多取权重最高的两个房间；重叠房间按权重同时发送。
                // 权重并列（房间嵌套：内外房间同为 1.0）时小房间排前——
                // source_rooms[0] 是声源的主房间（二次激发的路径查询用它），
                // 应为声源实际所在的最内层房间。
                const audio_environment_room* source_rooms[2] = {
                    nullptr, nullptr };
                float source_weights[2] = { 0.0f, 0.0f };
                int source_room_count = 0;

                auto room_ranks_higher =
                    [](const audio_environment_room* room_a, float weight_a,
                        const audio_environment_room* room_b, float weight_b)
                {
                    if (weight_a != weight_b)
                        return weight_a > weight_b;
                    const float volume_a = room_a->m_half_extent.x
                        * room_a->m_half_extent.y * room_a->m_half_extent.z;
                    const float volume_b = room_b->m_half_extent.x
                        * room_b->m_half_extent.y * room_b->m_half_extent.z;
                    return volume_a < volume_b;
                };

                for (const auto& room : environment->m_rooms)
                {
                    const float weight = audio_environment_room_weight(
                        position, room, listener_2d);

                    if (weight <= REVERB_WEIGHT_EPSILON)
                        continue;

                    int insert_at = source_room_count;
                    while (insert_at > 0
                        && room_ranks_higher(&room, weight,
                            source_rooms[insert_at - 1],
                            source_weights[insert_at - 1]))
                        --insert_at;

                    if (insert_at >= 2)
                        continue;

                    const int move_end =
                        std::min(source_room_count, 1);

                    for (int i = move_end; i > insert_at; --i)
                    {
                        source_rooms[i] = source_rooms[i - 1];
                        source_weights[i] = source_weights[i - 1];
                    }

                    source_rooms[insert_at] = &room;
                    source_weights[insert_at] = weight;

                    if (source_room_count < 2)
                        ++source_room_count;
                }

                const uint32_t listener_room_id =
                    listener_reverb_room != nullptr
                    ? listener_reverb_room->m_id
                    : AUDIO_OUTDOOR_ROOM_ID;

                for (int i = 0; i < source_room_count; ++i)
                {
                    const audio_environment_room* source_room =
                        source_rooms[i];

                    const audio_environment_path& path = environment->path(
                        environment->node_of_room(source_room->m_id),
                        environment->node_of_room(listener_room_id));

                    float weight = source_weights[i];
                    float hf = occlusion.m_hf;

                    if (source_room->m_id != listener_room_id)
                    {
                        if (path.m_transmission > REVERB_WEIGHT_EPSILON)
                        {
                            weight *= path.m_transmission;
                            hf *= 1.0f - path.m_muffle;
                        }
                        else
                        {
                            // 无门洞：按墙体泄漏衰减。声源整体增益（含直接
                            // 遮挡增益）已经作用在这路发送上，这里再叠加
                            // 墙体的透射与闷音，保证封闭房间仍有隔墙混响，
                            // 同时让门洞明显更亮更响。
                            weight *= WALL_WET_TRANSMISSION;
                            hf *= 1.0f - WALL_WET_MUFFLE;
                        }
                    }
                    reverb_candidates[reverb_candidate_count++] =
                        reverb_candidate{ source_room, weight, hf };
                }

                // 听者房间的二次激发：听者有房间且不是声源所在房间时，
                // 让穿墙/门洞的声音把听者房间的混响激起来。
                if (listener_reverb_room != nullptr
                    && listener_reverb_weight > REVERB_WEIGHT_EPSILON)
                {
                    bool listener_is_source_room = false;
                    for (int i = 0; i < source_room_count; ++i)
                    {
                        if (source_rooms[i]->m_id
                            == listener_reverb_room->m_id)
                        {
                            listener_is_source_room = true;
                            break;
                        }
                    }

                    if (!listener_is_source_room)
                    {
                        const uint32_t from_room_id = source_room_count > 0
                            ? source_rooms[0]->m_id
                            : AUDIO_OUTDOOR_ROOM_ID;

                        const audio_environment_path& path =
                            environment->path(
                                environment->node_of_room(from_room_id),
                                environment->node_of_room(
                                    listener_reverb_room->m_id));

                        float weight = LISTENER_ROOM_EXCITATION
                            * listener_reverb_weight;
                        float hf = occlusion.m_hf;

                        if (path.m_transmission > REVERB_WEIGHT_EPSILON)
                        {
                            weight *= path.m_transmission;
                            hf *= 1.0f - path.m_muffle;
                        }
                        else
                        {
                            // 无门洞：墙体泄漏，听者房间只被很轻很闷地激发；
                            // 有门洞时才会明显“透进来”。
                            weight *= WALL_WET_TRANSMISSION;
                            hf *= 1.0f - WALL_WET_MUFFLE;
                        }

                        reverb_candidates[reverb_candidate_count++] =
                            reverb_candidate{
                                listener_reverb_room, weight, hf };
                    }
                }
            }

            for (size_t send_index = 0;
                send_index < Audio::Source::REVERB_SEND_COUNT;
                ++send_index)
            {
                Audio::Source::reverb_send_state& send =
                    source.reverb_sends[send_index];

                if ((int)send_index >= reverb_candidate_count)
                {
                    if (send.filter_bound)
                    {
                        source.source->bind_effect_slot(
                            send_index, std::nullopt, std::nullopt);
                        send.filter_bound = false;
                    }
                    send.room_id = 0;
                    continue;
                }

                const reverb_candidate& candidate =
                    reverb_candidates[send_index];

                reverb_room_runtime& room =
                    ensure_reverb_room(*candidate.m_room);

                if (!send.filter.has_value())
                    send.filter.emplace(filter::create());

                // EFX 没有 per-send gain：用该路发送的低通滤波器 gain 代替。
                // 声源整体增益已经包含遮挡增益，这里只补房间权重、门洞路径
                // 衰减与高频闷音。
                const bool send_gain_changed =
                    send.applied_gain != candidate.m_weight
                    || send.applied_hf != candidate.m_hf;

                if (send_gain_changed)
                {
                    send.applied_gain = candidate.m_weight;
                    send.applied_hf = candidate.m_hf;

                    send.filter.value()->update(
                        [&](je_audio_filter_t* filter_data)
                        {
                            filter_data->m_type =
                                JE_AUDIO_FILTER_LOWPASS;
                            filter_data->m_gain = candidate.m_weight;
                            filter_data->m_gain_hf = candidate.m_hf;
                        });
                }

                // OpenAL 在 alSource3i(AL_AUXILIARY_SEND_FILTER) 时把滤波器的
                // gain/gain_hf 拷贝进 voice；之后单独改滤波器不会生效。所以
                // 发送权重变化时也必须重新绑定发送。
                if (!send.filter_bound
                    || send_gain_changed
                    || send.room_id != candidate.m_room->m_id
                    || send.room_epoch != room.m_slot_epoch)
                {
                    source.source->bind_effect_slot(
                        send_index,
                        std::optional<basic::resource<effect_slot>>(
                            room.m_slot.value()),
                        std::optional<basic::resource<filter>>(
                            send.filter.value()));

                    send.room_id = candidate.m_room->m_id;
                    send.room_epoch = room.m_slot_epoch;
                    send.filter_bound = true;
                }
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

                        math::vec3 direction =
                            trans->world_rotation * source.direction;

                        if (direction.length() > 1e-6f)
                            direction = direction.unit();
                        else
                            direction = math::vec3(0.0f, 0.0f, 1.0f);

                        data->m_direction[0] = direction.x;
                        data->m_direction[1] = direction.y;
                        data->m_direction[2] = direction.z;
                    }
                    else
                    {
                        data->m_location[0] = 0.0f;
                        data->m_location[1] = 0.0f;
                        data->m_location[2] = 0.0f;

                        data->m_velocity[0] = 0.0f;
                        data->m_velocity[1] = 0.0f;
                        data->m_velocity[2] = 0.0f;

                        data->m_direction[0] = 0.0f;
                        data->m_direction[1] = 0.0f;
                        data->m_direction[2] = 1.0f;
                    }

                    data->m_reference_distance = source.reference_distance;
                    data->m_max_distance = source.max_distance;
                    data->m_rolloff = source.rolloff;

                    data->m_min_gain = source.min_gain;
                    data->m_max_gain = source.max_gain;

                    data->m_cone_inner_angle = source.cone_inner_angle;
                    data->m_cone_outer_angle = source.cone_outer_angle;
                    data->m_cone_outer_gain = source.cone_outer_gain;

                    data->m_air_absorption = source.air_absorption;
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

        // ==================== 5. 回收不再使用的房间效果 ====================
        for (auto& room : m_reverb_rooms)
        {
            if (!room.second.m_active)
            {
                room.second.m_slot.reset();
                room.second.m_effect.reset();
                room.second.m_has_params = false;
                room.second.m_slot_gain = -1.0f;
            }
        }
    }

}
