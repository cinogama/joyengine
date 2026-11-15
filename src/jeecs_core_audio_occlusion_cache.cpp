#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs_core_audio_occlusion_cache.hpp"

#include "jeecs/cache.hpp"
#include "jeecs/components.hpp"
#include "jeecs/core.hpp"
#include "jeecs/debug.hpp"
#include "jeecs/file.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/typing.hpp"
#include "jeecs_basic_cache_version.hpp"
#include "jeecs_core_tilemap_model.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jeecs::audio
{
    namespace
    {
        // 每个格存“单位长度光学密度”，用 uint16 归一化保存；上限覆盖
        // 几乎不透明的薄墙（-ln(0.005) / 0.05 ≈ 106）。
        constexpr float AUDIO_OCCLUSION_DENSITY_MAX = 128.0f;
        constexpr float AUDIO_OCCLUSION_CELL_SIZE = 0.5f;
        constexpr int32_t AUDIO_OCCLUSION_MAX_DIM = 1024;
        constexpr float AUDIO_OCCLUSION_MIN_THICKNESS = 0.05f;
        constexpr float AUDIO_OCCLUSION_EPSILON = 1e-4f;
        constexpr int32_t AUDIO_OCCLUSION_SUPERSAMPLE = 4;

        uint64_t hash_mix(uint64_t hash, uint64_t value) noexcept
        {
            hash ^= value + 0x9E3779B97F4A7C15ull + (hash << 6) + (hash >> 2);
            return hash;
        }
        uint64_t hash_float(uint64_t hash, float value) noexcept
        {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return hash_mix(hash, bits);
        }
        uint64_t hash_vec(uint64_t hash, const math::vec3& value) noexcept
        {
            hash = hash_float(hash, value.x);
            hash = hash_float(hash, value.y);
            return hash_float(hash, value.z);
        }
        uint64_t hash_quat(uint64_t hash, const math::quat& value) noexcept
        {
            hash = hash_float(hash, value.x);
            hash = hash_float(hash, value.y);
            hash = hash_float(hash, value.z);
            return hash_float(hash, value.w);
        }
        uint64_t hash_string(uint64_t hash, const std::string& value) noexcept
        {
            for (unsigned char c : value)
                hash = hash_mix(hash, c);
            return hash_mix(hash, value.size());
        }

        template <typename T>
        T* bake_get_component(je_game_entity_t* entity)
        {
            return static_cast<T*>(je_ecs_world_entity_component_get(
                entity, typing::id<T>()));
        }

        struct world_entity_list
        {
            je_game_entity_t** m_list;

            explicit world_entity_list(je_game_world_t* world)
                : m_list(je_debug_world_entities_get_all(world))
            {
                assert(m_list != nullptr);
            }
            ~world_entity_list()
            {
                for (je_game_entity_t** it = m_list; *it != nullptr; ++it)
                    je_debug_entity_free(*it);

                je_mem_free(m_list);
            }

            world_entity_list(const world_entity_list&) = delete;
            world_entity_list& operator=(const world_entity_list&) = delete;
        };

        // ---- 世界变换解析：只读组件，不要求世界已启用/已 update ----
        struct bake_resolved_transform
        {
            math::vec3 m_position;
            math::quat m_rotation;
            math::vec3 m_local_scale;

            bake_resolved_transform() noexcept
                : m_position()
                , m_rotation()
                , m_local_scale(1.0f, 1.0f, 1.0f)
            {
            }
        };

        struct bake_pending_transform
        {
            typing::uuid m_uid;
            typing::uuid m_parent_uid;
            math::vec3 m_position;
            math::quat m_rotation;
            math::vec3 m_local_scale;
        };

        using resolved_transform_map =
            std::unordered_map<typing::uuid, bake_resolved_transform>;

        resolved_transform_map build_resolved_transforms(
            je_game_entity_t** entity_list)
        {
            resolved_transform_map resolved;
            std::vector<bake_pending_transform> pending;

            for (je_game_entity_t** it = entity_list; *it != nullptr; ++it)
            {
                je_game_entity_t* entity = *it;

                if (bake_get_component<Transform::Translation>(entity)
                    == nullptr)
                    continue;

                auto* anchor = bake_get_component<Transform::Anchor>(entity);
                auto* l2w =
                    bake_get_component<Transform::LocalToWorld>(entity);
                auto* l2p =
                    bake_get_component<Transform::LocalToParent>(entity);
                auto* position =
                    bake_get_component<Transform::LocalPosition>(entity);
                auto* rotation =
                    bake_get_component<Transform::LocalRotation>(entity);
                auto* scale =
                    bake_get_component<Transform::LocalScale>(entity);

                if (l2w != nullptr && l2p == nullptr)
                {
                    bake_resolved_transform transform;
                    transform.m_position =
                        position != nullptr ? position->pos : math::vec3();
                    transform.m_rotation =
                        rotation != nullptr ? rotation->rot : math::quat();
                    transform.m_local_scale =
                        scale != nullptr
                        ? scale->scale
                        : math::vec3(1.0f, 1.0f, 1.0f);

                    if (anchor != nullptr)
                        resolved[anchor->uid] = transform;
                }
                else if (l2p != nullptr && l2w == nullptr
                    && anchor != nullptr)
                {
                    bake_pending_transform transform;
                    transform.m_uid = anchor->uid;
                    transform.m_parent_uid = l2p->parent_uid;
                    transform.m_position =
                        position != nullptr ? position->pos : math::vec3();
                    transform.m_rotation =
                        rotation != nullptr ? rotation->rot : math::quat();
                    transform.m_local_scale =
                        scale != nullptr
                        ? scale->scale
                        : math::vec3(1.0f, 1.0f, 1.0f);

                    pending.push_back(transform);
                }
            }

            for (;;)
            {
                bool progressed = false;

                for (auto it = pending.begin(); it != pending.end();)
                {
                    const auto parent = resolved.find(it->m_parent_uid);
                    if (parent == resolved.end())
                    {
                        ++it;
                        continue;
                    }

                    bake_resolved_transform transform;
                    transform.m_rotation =
                        parent->second.m_rotation * it->m_rotation;
                    transform.m_position =
                        parent->second.m_rotation * it->m_position
                        + parent->second.m_position;
                    transform.m_local_scale = it->m_local_scale;

                    resolved[it->m_uid] = transform;
                    it = pending.erase(it);
                    progressed = true;
                }

                if (!progressed)
                    break;
            }

            return resolved;
        }

        bake_resolved_transform resolve_entity_transform(
            je_game_entity_t* entity,
            const resolved_transform_map& resolved)
        {
            auto* anchor = bake_get_component<Transform::Anchor>(entity);
            auto* l2p = bake_get_component<Transform::LocalToParent>(entity);
            auto* position =
                bake_get_component<Transform::LocalPosition>(entity);
            auto* rotation =
                bake_get_component<Transform::LocalRotation>(entity);
            auto* scale = bake_get_component<Transform::LocalScale>(entity);

            if (anchor != nullptr)
            {
                const auto fnd = resolved.find(anchor->uid);
                if (fnd != resolved.end())
                    return fnd->second;
            }

            if (l2p != nullptr)
            {
                const auto fnd = resolved.find(l2p->parent_uid);
                if (fnd != resolved.end())
                {
                    bake_resolved_transform transform;
                    transform.m_rotation =
                        fnd->second.m_rotation * l2p->rot;
                    transform.m_position =
                        fnd->second.m_rotation * l2p->pos
                        + fnd->second.m_position;
                    transform.m_local_scale = l2p->scale;
                    return transform;
                }
            }

            bake_resolved_transform transform;
            transform.m_position =
                position != nullptr ? position->pos : math::vec3();
            transform.m_rotation =
                rotation != nullptr ? rotation->rot : math::quat();
            transform.m_local_scale =
                scale != nullptr
                ? scale->scale
                : math::vec3(1.0f, 1.0f, 1.0f);
            return transform;
        }

        // ---- 静态输入采集 ----
        struct occlusion_box
        {
            math::vec3 m_center;
            math::quat m_rotation;
            math::vec3 m_half_extent;
            float m_occlusion;
            float m_muffle;

            occlusion_box() noexcept
                : m_center()
                , m_rotation()
                , m_half_extent()
                , m_occlusion(0.0f)
                , m_muffle(0.0f)
            {
            }
        };

        struct occlusion_map
        {
            std::string m_path;
            math::vec3 m_position;
            math::quat m_rotation;
            math::vec3 m_scale;
            // 地图与其图集缓存文件的源 CRC64 哈希（跨进程稳定）
            uint64_t m_stamp;

            occlusion_map()
                : m_path()
                , m_position()
                , m_rotation()
                , m_scale(1.0f, 1.0f, 1.0f)
                , m_stamp(0)
            {
            }
        };

        struct occlusion_inputs
        {
            std::vector<occlusion_box> m_boxes;
            std::vector<occlusion_map> m_maps;
        };

        bool vec_less(const math::vec3& a, const math::vec3& b) noexcept
        {
            if (a.x != b.x)
                return a.x < b.x;
            if (a.y != b.y)
                return a.y < b.y;
            return a.z < b.z;
        }
        bool quat_less(const math::quat& a, const math::quat& b) noexcept
        {
            if (a.x != b.x)
                return a.x < b.x;
            if (a.y != b.y)
                return a.y < b.y;
            if (a.z != b.z)
                return a.z < b.z;
            return a.w < b.w;
        }
        bool box_less(
            const occlusion_box& a, const occlusion_box& b) noexcept
        {
            if (vec_less(a.m_center, b.m_center))
                return true;
            if (vec_less(b.m_center, a.m_center))
                return false;
            if (quat_less(a.m_rotation, b.m_rotation))
                return true;
            if (quat_less(b.m_rotation, a.m_rotation))
                return false;
            if (vec_less(a.m_half_extent, b.m_half_extent))
                return true;
            if (vec_less(b.m_half_extent, a.m_half_extent))
                return false;
            if (a.m_occlusion != b.m_occlusion)
                return a.m_occlusion < b.m_occlusion;
            return a.m_muffle < b.m_muffle;
        }
        bool map_less(const occlusion_map& a, const occlusion_map& b) noexcept
        {
            if (a.m_path != b.m_path)
                return a.m_path < b.m_path;
            if (vec_less(a.m_position, b.m_position))
                return true;
            if (vec_less(b.m_position, a.m_position))
                return false;
            if (quat_less(a.m_rotation, b.m_rotation))
                return true;
            if (quat_less(b.m_rotation, a.m_rotation))
                return false;
            if (vec_less(a.m_scale, b.m_scale))
                return true;
            if (vec_less(b.m_scale, a.m_scale))
                return false;
            return a.m_stamp < b.m_stamp;
        }

        // 读缓存文件头里的源 CRC64（布局：[u32 main][u32 format][u64 crc]）。
        // 文件不存在/损坏时返回 0；不打开缓存管理器，避免校验开销。
        uint64_t read_cache_source_crc(const std::string& source_path)
        {
            const std::string cache_path = source_path + ".je4cache";
            je_file_t* file = je_file_open(cache_path.c_str());
            if (file == nullptr)
                return 0;

            uint32_t main_version = 0;
            uint32_t format_version = 0;
            uint64_t crc = 0;

            const bool ok =
                je_file_read(&main_version, sizeof(main_version), 1, file) == 1
                && je_file_read(&format_version, sizeof(format_version), 1, file)
                    == 1
                && je_file_read(&crc, sizeof(crc), 1, file) == 1;

            je_file_close(file);
            return ok ? crc : 0;
        }

        // 地图的输入指纹：地图缓存源 CRC + 每个图集缓存源 CRC（含路径）。
        uint64_t map_input_stamp(const std::string& map_path)
        {
            // 先让 observe_map/ensure_tilesets 把地图与图集缓存落实，再读
            // 缓存头。首次烘焙时这些缓存可能尚不存在：若先读后建，烘焙与
            // 运行时（缓存已生成）会读到不同的源 CRC，导致缓存被判过期。
            auto document = Tilemap::documents::inst().observe_map(map_path);
            if (document.has_value())
                Tilemap::documents::inst().ensure_tilesets(**document);

            uint64_t hash = hash_mix(0, read_cache_source_crc(map_path));
            if (!document.has_value())
                return hash_string(hash, map_path);

            for (const auto& tileset : (*document)->tilesets)
            {
                hash = hash_string(hash, tileset.path);
                hash = hash_mix(hash, read_cache_source_crc(tileset.path));
            }
            return hash;
        }

        occlusion_inputs collect_occlusion_inputs(je_game_world_t* world)
        {
            world_entity_list entities(world);
            const resolved_transform_map resolved =
                build_resolved_transforms(entities.m_list);

            occlusion_inputs inputs;

            for (je_game_entity_t** it = entities.m_list; *it != nullptr; ++it)
            {
                je_game_entity_t* entity = *it;

                if (bake_get_component<Transform::Translation>(entity)
                    == nullptr)
                    continue;

                auto* occluder = bake_get_component<Audio::Occluder>(entity);
                if (occluder != nullptr && occluder->is_static)
                {
                    const bake_resolved_transform transform =
                        resolve_entity_transform(entity, resolved);

                    occlusion_box box;
                    box.m_center = transform.m_position;
                    box.m_rotation = transform.m_rotation;
                    box.m_half_extent = math::vec3(
                        std::fabs(occluder->size.x * transform.m_local_scale.x)
                            * 0.5f,
                        std::fabs(occluder->size.y * transform.m_local_scale.y)
                            * 0.5f,
                        std::fabs(occluder->size.z * transform.m_local_scale.z)
                            * 0.5f);
                    box.m_occlusion = occluder->occlusion;
                    box.m_muffle = occluder->muffle;

                    inputs.m_boxes.push_back(box);
                }

                auto* map = bake_get_component<Tilemap::Map>(entity);
                if (map != nullptr)
                {
                    const std::string map_path = map->map_path.cpp_str();
                    if (map_path.empty())
                        continue;

                    const bake_resolved_transform transform =
                        resolve_entity_transform(entity, resolved);

                    occlusion_map input;
                    input.m_path = map_path;
                    input.m_position = transform.m_position;
                    input.m_rotation = transform.m_rotation;
                    input.m_scale = transform.m_local_scale;
                    input.m_stamp = map_input_stamp(map_path);

                    inputs.m_maps.push_back(input);
                }
            }

            std::sort(
                inputs.m_boxes.begin(), inputs.m_boxes.end(), box_less);
            std::sort(inputs.m_maps.begin(), inputs.m_maps.end(), map_less);

            return inputs;
        }

        uint64_t hash_occlusion_inputs(const occlusion_inputs& inputs)
        {
            uint64_t hash = 0xCBF29CE484222325ull;
            hash = hash_mix(hash, (uint64_t)AUDIO_OCCLUSION_FORMAT_VERSION);

            hash = hash_mix(hash, inputs.m_boxes.size());
            for (const auto& box : inputs.m_boxes)
            {
                hash = hash_vec(hash, box.m_center);
                hash = hash_quat(hash, box.m_rotation);
                hash = hash_vec(hash, box.m_half_extent);
                hash = hash_float(hash, box.m_occlusion);
                hash = hash_float(hash, box.m_muffle);
            }

            hash = hash_mix(hash, inputs.m_maps.size());
            for (const auto& map : inputs.m_maps)
            {
                hash = hash_string(hash, map.m_path);
                hash = hash_vec(hash, map.m_position);
                hash = hash_quat(hash, map.m_rotation);
                hash = hash_vec(hash, map.m_scale);
                hash = hash_mix(hash, map.m_stamp);
            }

            return hash;
        }

        // ---- 遮挡场栅格化 ----
        struct float_bounds
        {
            float m_min_x;
            float m_min_y;
            float m_max_x;
            float m_max_y;
            bool m_has_value;

            float_bounds() noexcept
                : m_min_x(0.0f)
                , m_min_y(0.0f)
                , m_max_x(0.0f)
                , m_max_y(0.0f)
                , m_has_value(false)
            {
            }

            void include(const math::vec3& point) noexcept
            {
                if (!m_has_value)
                {
                    m_min_x = m_max_x = point.x;
                    m_min_y = m_max_y = point.y;
                    m_has_value = true;
                    return;
                }

                m_min_x = std::min(m_min_x, point.x);
                m_min_y = std::min(m_min_y, point.y);
                m_max_x = std::max(m_max_x, point.x);
                m_max_y = std::max(m_max_y, point.y);
            }
        };

        math::vec3 map_world_corner(
            const occlusion_map& map, float local_x, float local_y) noexcept
        {
            // 地图局部网格 y 向下为正：grid_y 对应的局部 y 是负方向。
            return map.m_position
                + map.m_rotation
                    * math::vec3(
                        local_x * map.m_scale.x,
                        -local_y * map.m_scale.y,
                        0.0f);
        }

        void include_box_bounds(float_bounds& bounds, const occlusion_box& box)
        {
            for (int sx = -1; sx <= 1; sx += 2)
            {
                for (int sy = -1; sy <= 1; sy += 2)
                {
                    bounds.include(
                        box.m_center
                        + box.m_rotation
                            * math::vec3(
                                sx * box.m_half_extent.x,
                                sy * box.m_half_extent.y,
                                0.0f));
                }
            }
        }

        // 一维光学密度：把“穿过整个遮挡体一次衰减 (1 - o)”换算成
        // 每单位长度的密度；垂直于遮挡体穿越时与旧行为一致。
        float optical_density(float value, float thickness) noexcept
        {
            value = std::clamp(value, 0.0f, 0.995f);
            if (value <= AUDIO_OCCLUSION_EPSILON)
                return 0.0f;

            return -std::log(1.0f - value)
                / std::max(thickness, AUDIO_OCCLUSION_MIN_THICKNESS);
        }

        void rasterize_box(
            std::vector<float>& occlusion_density,
            std::vector<float>& muffle_density,
            float origin_x,
            float origin_y,
            float cell_size,
            int32_t width,
            int32_t height,
            const occlusion_box& box)
        {
            const float box_thickness = std::max(
                2.0f * std::min(box.m_half_extent.x, box.m_half_extent.y),
                AUDIO_OCCLUSION_MIN_THICKNESS);
            const float occlusion = optical_density(
                box.m_occlusion, box_thickness);
            const float muffle = optical_density(
                box.m_muffle, box_thickness);

            if (occlusion <= 0.0f && muffle <= 0.0f)
                return;

            float_bounds bounds;
            include_box_bounds(bounds, box);
            if (!bounds.m_has_value)
                return;

            const int32_t x0 = std::clamp(
                (int32_t)std::floor((bounds.m_min_x - origin_x) / cell_size),
                0,
                width - 1);
            const int32_t x1 = std::clamp(
                (int32_t)std::floor((bounds.m_max_x - origin_x) / cell_size),
                0,
                width - 1);
            const int32_t y0 = std::clamp(
                (int32_t)std::floor((bounds.m_min_y - origin_y) / cell_size),
                0,
                height - 1);
            const int32_t y1 = std::clamp(
                (int32_t)std::floor((bounds.m_max_y - origin_y) / cell_size),
                0,
                height - 1);

            const math::quat inverse_rotation = box.m_rotation.inverse();
            constexpr int32_t samples = AUDIO_OCCLUSION_SUPERSAMPLE;
            constexpr float sample_scale =
                1.0f / (samples * samples);

            for (int32_t y = y0; y <= y1; ++y)
            {
                for (int32_t x = x0; x <= x1; ++x)
                {
                    int32_t inside = 0;
                    for (int32_t sy = 0; sy < samples; ++sy)
                    {
                        for (int32_t sx = 0; sx < samples; ++sx)
                        {
                            const math::vec3 point(
                                origin_x
                                    + (x + (sx + 0.5f) / samples) * cell_size,
                                origin_y
                                    + (y + (sy + 0.5f) / samples) * cell_size,
                                0.0f);

                            const math::vec3 local =
                                inverse_rotation * (point - box.m_center);

                            if (std::fabs(local.x) <= box.m_half_extent.x
                                && std::fabs(local.y)
                                    <= box.m_half_extent.y)
                                ++inside;
                        }
                    }

                    if (inside == 0)
                        continue;

                    const float weight = inside * sample_scale;
                    const size_t index = (size_t)y * width + x;
                    occlusion_density[index] += occlusion * weight;
                    muffle_density[index] += muffle * weight;
                }
            }
        }

        // 地图的音频系数按局部格采样：支持地图根实体的旋转/缩放。
        void rasterize_map(
            std::vector<float>& occlusion_density,
            std::vector<float>& muffle_density,
            float origin_x,
            float origin_y,
            float cell_size,
            int32_t width,
            int32_t height,
            const occlusion_map& map,
            const audio_map_grid& grid)
        {
            if (grid.m_width <= 0 || grid.m_height <= 0
                || std::fabs(map.m_scale.x) <= 1e-6f
                || std::fabs(map.m_scale.y) <= 1e-6f)
                return;

            float_bounds bounds;
            bounds.include(map_world_corner(map, 0.0f, 0.0f));
            bounds.include(
                map_world_corner(map, (float)grid.m_width, 0.0f));
            bounds.include(
                map_world_corner(map, 0.0f, (float)grid.m_height));
            bounds.include(
                map_world_corner(map, (float)grid.m_width, (float)grid.m_height));

            // 一格地图在世界中的名义长度：把“每格一次”的系数换算成
            // 每单位长度的密度。
            const float world_cell_size = std::max(
                0.5f
                    * (std::fabs(map.m_scale.x)
                        + std::fabs(map.m_scale.y)),
                AUDIO_OCCLUSION_MIN_THICKNESS);

            const int32_t x0 = std::clamp(
                (int32_t)std::floor((bounds.m_min_x - origin_x) / cell_size),
                0,
                width - 1);
            const int32_t x1 = std::clamp(
                (int32_t)std::floor((bounds.m_max_x - origin_x) / cell_size),
                0,
                width - 1);
            const int32_t y0 = std::clamp(
                (int32_t)std::floor((bounds.m_min_y - origin_y) / cell_size),
                0,
                height - 1);
            const int32_t y1 = std::clamp(
                (int32_t)std::floor((bounds.m_max_y - origin_y) / cell_size),
                0,
                height - 1);

            const math::quat inverse_rotation = map.m_rotation.inverse();
            constexpr int32_t samples = AUDIO_OCCLUSION_SUPERSAMPLE;
            constexpr float sample_scale = 1.0f / (samples * samples);

            for (int32_t y = y0; y <= y1; ++y)
            {
                for (int32_t x = x0; x <= x1; ++x)
                {
                    float occlusion = 0.0f;
                    float muffle = 0.0f;

                    for (int32_t sy = 0; sy < samples; ++sy)
                    {
                        for (int32_t sx = 0; sx < samples; ++sx)
                        {
                            const math::vec3 point(
                                origin_x
                                    + (x + (sx + 0.5f) / samples) * cell_size,
                                origin_y
                                    + (y + (sy + 0.5f) / samples) * cell_size,
                                0.0f);

                            const math::vec3 local =
                                inverse_rotation * (point - map.m_position);

                            // 地图局部网格：1 格 = 1 单位，y 向下为正。
                            const float grid_x = local.x / map.m_scale.x;
                            const float grid_y = -local.y / map.m_scale.y;

                            const int32_t map_x = (int32_t)std::floor(grid_x);
                            const int32_t map_y = (int32_t)std::floor(grid_y);

                            if (map_x < 0 || map_x >= grid.m_width
                                || map_y < 0 || map_y >= grid.m_height)
                                continue;

                            const size_t index =
                                (size_t)map_y * grid.m_width + map_x;

                            occlusion += optical_density(
                                grid.m_occlusion[index], world_cell_size);
                            muffle += optical_density(
                                grid.m_muffle[index], world_cell_size);
                        }
                    }

                    if (occlusion <= 0.0f && muffle <= 0.0f)
                        continue;

                    // optical_density 已归一化为“每世界单位”，这里只做
                    // 子采样平均（边缘覆盖率抗锯齿）。
                    const size_t index = (size_t)y * width + x;
                    occlusion_density[index] += occlusion * sample_scale;
                    muffle_density[index] += muffle * sample_scale;
                }
            }
        }

        uint16_t encode_density(float density) noexcept
        {
            const float normalized =
                std::clamp(density / AUDIO_OCCLUSION_DENSITY_MAX, 0.0f, 1.0f);
            return (uint16_t)std::lround(normalized * 65535.0f);
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

    audio_occlusion_field::audio_occlusion_field() noexcept
        : m_inputs(0)
        , m_cell_size(AUDIO_OCCLUSION_CELL_SIZE)
        , m_origin_x(0.0f)
        , m_origin_y(0.0f)
        , m_width(0)
        , m_height(0)
        , m_density_scale(AUDIO_OCCLUSION_DENSITY_MAX)
        , m_occlusion()
        , m_muffle()
    {
    }

    uint64_t hash_static_occlusion_inputs(je_game_world_t* world)
    {
        return hash_occlusion_inputs(collect_occlusion_inputs(world));
    }

    bool bake_audio_occlusion_world(
        je_game_world_t* world, const char* world_path)
    {
        occlusion_inputs inputs = collect_occlusion_inputs(world);
        const uint64_t inputs_hash = hash_occlusion_inputs(inputs);

        // 哪些输入真正产生遮挡；只有它们参与场的 AABB。
        std::vector<std::pair<occlusion_map, audio_map_grid>> effective_maps;
        float_bounds bounds;

        for (const auto& box : inputs.m_boxes)
        {
            if (box.m_occlusion <= AUDIO_OCCLUSION_EPSILON
                && box.m_muffle <= AUDIO_OCCLUSION_EPSILON)
                continue;

            include_box_bounds(bounds, box);
        }

        for (const auto& map : inputs.m_maps)
        {
            auto document =
                Tilemap::documents::inst().observe_map(map.m_path);
            if (!document.has_value())
            {
                debug::logwarn(
                    "Audio occlusion bake: failed to load tilemap '%s'.",
                    map.m_path.c_str());
                continue;
            }

            audio_map_grid grid;
            rebuild_audio_grid(grid, *document.value());

            bool has_occlusion = false;
            for (float value : grid.m_occlusion)
            {
                if (value > AUDIO_OCCLUSION_EPSILON)
                {
                    has_occlusion = true;
                    break;
                }
            }
            if (!has_occlusion)
            {
                for (float value : grid.m_muffle)
                {
                    if (value > AUDIO_OCCLUSION_EPSILON)
                    {
                        has_occlusion = true;
                        break;
                    }
                }
            }

            if (!has_occlusion)
                continue;

            bounds.include(map_world_corner(map, 0.0f, 0.0f));
            bounds.include(
                map_world_corner(map, (float)grid.m_width, 0.0f));
            bounds.include(
                map_world_corner(map, 0.0f, (float)grid.m_height));
            bounds.include(map_world_corner(
                map, (float)grid.m_width, (float)grid.m_height));

            effective_maps.push_back(std::make_pair(map, std::move(grid)));
        }

        audio_occlusion_field field;
        field.m_inputs = inputs_hash;

        if (bounds.m_has_value)
        {
            const float span_x = bounds.m_max_x - bounds.m_min_x;
            const float span_y = bounds.m_max_y - bounds.m_min_y;

            float cell_size = AUDIO_OCCLUSION_CELL_SIZE;
            const float max_span = std::max(span_x, span_y);
            if (max_span > 0.0f)
                cell_size = std::max(
                    cell_size,
                    max_span / (float)(AUDIO_OCCLUSION_MAX_DIM - 2));

            const int32_t width = std::clamp(
                (int32_t)std::ceil(span_x / cell_size) + 2,
                1,
                AUDIO_OCCLUSION_MAX_DIM);
            const int32_t height = std::clamp(
                (int32_t)std::ceil(span_y / cell_size) + 2,
                1,
                AUDIO_OCCLUSION_MAX_DIM);

            field.m_cell_size = cell_size;
            field.m_origin_x = bounds.m_min_x - cell_size;
            field.m_origin_y = bounds.m_min_y - cell_size;
            field.m_width = width;
            field.m_height = height;

            std::vector<float> occlusion_density(
                (size_t)width * height, 0.0f);
            std::vector<float> muffle_density(
                (size_t)width * height, 0.0f);

            for (const auto& box : inputs.m_boxes)
            {
                rasterize_box(
                    occlusion_density,
                    muffle_density,
                    field.m_origin_x,
                    field.m_origin_y,
                    field.m_cell_size,
                    width,
                    height,
                    box);
            }
            for (const auto& entry : effective_maps)
            {
                rasterize_map(
                    occlusion_density,
                    muffle_density,
                    field.m_origin_x,
                    field.m_origin_y,
                    field.m_cell_size,
                    width,
                    height,
                    entry.first,
                    entry.second);
            }

            field.m_occlusion.resize((size_t)width * height);
            field.m_muffle.resize((size_t)width * height);

            for (size_t i = 0; i < field.m_occlusion.size(); ++i)
            {
                field.m_occlusion[i] = encode_density(occlusion_density[i]);
                field.m_muffle[i] = encode_density(muffle_density[i]);
            }
        }

        // ---- 写 <world>.je4cache 的 audio_occlusion 字段 ----
        je_writing_cache_file_t* file =
            je_cache_file_create(world_path, WORLD_CACHE_VERSION, 0);
        if (file == nullptr)
        {
            debug::logerr(
                "Failed to create audio occlusion cache for world: %s.",
                world_path);
            return false;
        }

        bool ok = true;
        auto append_key = [&](const char* key)
        {
            ok = ok && je_cache_binary_append_string(file, key);
        };
        auto append_int32 = [&](const char* key, int32_t value)
        {
            append_key(key);
            ok = ok && je_cache_binary_append_int32(file, value);
        };
        auto append_uint64 = [&](const char* key, uint64_t value)
        {
            append_key(key);
            ok = ok && je_cache_binary_append_uint64(file, value);
        };
        auto append_float = [&](const char* key, float value)
        {
            append_key(key);
            ok = ok && je_cache_binary_append_float32(file, value);
        };
        auto append_buffer = [&](const char* key, const void* data, size_t size)
        {
            append_key(key);
            static const uint8_t empty = 0;
            ok = ok && je_cache_binary_append_buffer(
                file, size > 0 ? data : (const void*)&empty, size);
        };

        ok = ok && je_cache_binary_append_document(file);

        append_key("audio_occlusion");
        ok = ok && je_cache_binary_append_document(file);

        append_int32("format", AUDIO_OCCLUSION_FORMAT_VERSION);
        append_uint64("inputs", field.m_inputs);
        append_float("cell_size", field.m_cell_size);
        append_float("origin_x", field.m_origin_x);
        append_float("origin_y", field.m_origin_y);
        append_int32("width", field.m_width);
        append_int32("height", field.m_height);
        append_float("dmax", field.m_density_scale);
        append_buffer(
            "occlusion",
            field.m_occlusion.data(),
            field.m_occlusion.size() * sizeof(uint16_t));
        append_buffer(
            "muffle",
            field.m_muffle.data(),
            field.m_muffle.size() * sizeof(uint16_t));

        ok = ok && je_cache_binary_append_end(file); // audio_occlusion
        ok = ok && je_cache_binary_append_end(file); // root document

        je_cache_file_close(file, ok);

        if (!ok)
        {
            debug::logerr(
                "Failed to write audio occlusion cache for world: %s.",
                world_path);
            return false;
        }

        debug::loginfo(
            "Baked audio occlusion: %s (%dx%d cells, cell=%.3f).",
            world_path,
            (int)field.m_width,
            (int)field.m_height,
            field.m_cell_size);

        return true;
    }

    std::optional<audio_occlusion_field> load_audio_occlusion_cache(
        const char* cache_path)
    {
        // 组件持有的是缓存文件路径（<world>.je4cache），cache 管理器按
        // 源文件路径定位并校验 CRC，所以这里去掉缓存后缀。
        std::string source_path = cache_path;
        constexpr const char* CACHE_SUFFIX = ".je4cache";
        const size_t suffix_length = std::strlen(CACHE_SUFFIX);
        if (source_path.size() > suffix_length
            && source_path.compare(source_path.size() - suffix_length,
                   suffix_length, CACHE_SUFFIX)
                == 0)
            source_path.resize(source_path.size() - suffix_length);

        je_file_t* file =
            je_cache_file_load(source_path.c_str(), WORLD_CACHE_VERSION, 0);
        if (file == nullptr)
            return std::nullopt;

        auto fail = [&]() -> std::optional<audio_occlusion_field>
        {
            debug::logerr(
                "Failed to parse audio occlusion cache for world: %s.",
                cache_path);
            je_file_close(file);
            return std::nullopt;
        };

        je_cache_binary_document_t root_doc;
        if (!je_cache_binary_document_open(file, &root_doc))
            return fail();

        je_cache_binary_document_t occlusion_doc;
        if (!je_cache_binary_document_index(
                &root_doc,
                "audio_occlusion",
                JE_CACHE_BINARY_VALUE_DOCUMENT,
                &occlusion_doc))
            return fail();

        int32_t format = 0;
        if (!je_cache_binary_document_index(
                &occlusion_doc,
                "format",
                JE_CACHE_BINARY_VALUE_INT32,
                &format)
            || format != AUDIO_OCCLUSION_FORMAT_VERSION)
            return fail();

        audio_occlusion_field result;

        if (!je_cache_binary_document_index(
                &occlusion_doc,
                "inputs",
                JE_CACHE_BINARY_VALUE_UINT64,
                &result.m_inputs)
            || !je_cache_binary_document_index(
                &occlusion_doc,
                "cell_size",
                JE_CACHE_BINARY_VALUE_FLOAT32,
                &result.m_cell_size)
            || !je_cache_binary_document_index(
                &occlusion_doc,
                "origin_x",
                JE_CACHE_BINARY_VALUE_FLOAT32,
                &result.m_origin_x)
            || !je_cache_binary_document_index(
                &occlusion_doc,
                "origin_y",
                JE_CACHE_BINARY_VALUE_FLOAT32,
                &result.m_origin_y)
            || !je_cache_binary_document_index(
                &occlusion_doc,
                "width",
                JE_CACHE_BINARY_VALUE_INT32,
                &result.m_width)
            || !je_cache_binary_document_index(
                &occlusion_doc,
                "height",
                JE_CACHE_BINARY_VALUE_INT32,
                &result.m_height)
            || !je_cache_binary_document_index(
                &occlusion_doc,
                "dmax",
                JE_CACHE_BINARY_VALUE_FLOAT32,
                &result.m_density_scale))
            return fail();

        if (result.m_width < 0 || result.m_height < 0
            || result.m_width > AUDIO_OCCLUSION_MAX_DIM
            || result.m_height > AUDIO_OCCLUSION_MAX_DIM
            || result.m_cell_size <= 0.0f
            || result.m_density_scale <= 0.0f)
            return fail();

        const size_t cell_count =
            (size_t)result.m_width * (size_t)result.m_height;

        auto read_density_buffer =
            [&](const char* key, std::vector<uint16_t>& out) -> bool
        {
            je_cache_binary_buffer_t buffer;
            if (!je_cache_binary_document_index(
                    &occlusion_doc,
                    key,
                    JE_CACHE_BINARY_VALUE_BUFFER,
                    &buffer))
                return false;

            size_t size = 0;
            if (!je_cache_binary_buffer_read(nullptr, &size, &buffer)
                || size != cell_count * sizeof(uint16_t))
                return false;

            out.resize(cell_count);
            if (size > 0)
            {
                size_t capacity = size;
                if (!je_cache_binary_buffer_read(
                        out.data(), &capacity, &buffer)
                    || capacity != size)
                    return false;
            }
            return true;
        };

        if (!read_density_buffer("occlusion", result.m_occlusion)
            || !read_density_buffer("muffle", result.m_muffle))
            return fail();

        je_file_close(file);
        return result;
    }

    audio_occlusion_result integrate_occlusion_field(
        const audio_occlusion_field& field,
        const math::vec3& from,
        const math::vec3& to)
    {
        audio_occlusion_result result;

        if (field.m_width <= 0 || field.m_height <= 0
            || field.m_occlusion.empty())
            return result;

        const float inv_cell = 1.0f / field.m_cell_size;
        float x0 = (from.x - field.m_origin_x) * inv_cell;
        float y0 = (from.y - field.m_origin_y) * inv_cell;
        float x1 = (to.x - field.m_origin_x) * inv_cell;
        float y1 = (to.y - field.m_origin_y) * inv_cell;

        // Liang-Barsky 把线段裁到 [0, width] x [0, height]。
        float t0 = 0.0f;
        float t1 = 1.0f;
        const float dx = x1 - x0;
        const float dy = y1 - y0;

        auto clip = [&](float p, float q) -> bool
        {
            if (std::fabs(p) <= 1e-9f)
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

        if (!clip(-dx, x0)
            || !clip(dx, (float)field.m_width - x0)
            || !clip(-dy, y0)
            || !clip(dy, (float)field.m_height - y0)
            || t1 <= t0)
            return result;

        const float cx0 = x0 + dx * t0;
        const float cy0 = y0 + dy * t0;

        // 起点恰好落在格边界且方向为负时，floor 得到的是即将离开的格子；
        // 退一格再裁剪，否则 next == cx0 会让 DDA 第一步就得到零长度而
        // 直接退出（整条射线被漏掉）。
        int32_t raw_cell_x = (int32_t)std::floor(cx0);
        int32_t raw_cell_y = (int32_t)std::floor(cy0);
        if (dx < 0.0f && cx0 == (float)raw_cell_x)
            --raw_cell_x;
        if (dy < 0.0f && cy0 == (float)raw_cell_y)
            --raw_cell_y;

        int32_t cell_x = std::clamp(raw_cell_x, 0, field.m_width - 1);
        int32_t cell_y = std::clamp(raw_cell_y, 0, field.m_height - 1);

        const float step_x = dx > 0.0f ? 1.0f : (dx < 0.0f ? -1.0f : 0.0f);
        const float step_y = dy > 0.0f ? 1.0f : (dy < 0.0f ? -1.0f : 0.0f);

        const float next_x = dx > 0.0f
            ? (float)(cell_x + 1)
            : (float)cell_x;
        const float next_y = dy > 0.0f
            ? (float)(cell_y + 1)
            : (float)cell_y;

        float t_max_x = std::fabs(dx) > 1e-9f
            ? t0 + (next_x - cx0) / dx
            : std::numeric_limits<float>::infinity();
        float t_max_y = std::fabs(dy) > 1e-9f
            ? t0 + (next_y - cy0) / dy
            : std::numeric_limits<float>::infinity();

        const float t_delta_x = std::fabs(dx) > 1e-9f
            ? 1.0f / std::fabs(dx)
            : std::numeric_limits<float>::infinity();
        const float t_delta_y = std::fabs(dy) > 1e-9f
            ? 1.0f / std::fabs(dy)
            : std::numeric_limits<float>::infinity();

        const float segment_length =
            std::sqrt(dx * dx + dy * dy) * field.m_cell_size;

        float tau_occlusion = 0.0f;
        float tau_muffle = 0.0f;

        float t = t0;
        const int32_t max_steps =
            field.m_width + field.m_height + AUDIO_OCCLUSION_SUPERSAMPLE;

        for (int32_t step = 0; step < max_steps && t < t1; ++step)
        {
            const float next = std::min(
                std::min(t_max_x, t_max_y), t1);
            if (next <= t)
                break;

            const float length = segment_length * (next - t);

            const size_t index =
                (size_t)cell_y * field.m_width + cell_x;

            tau_occlusion +=
                ((float)field.m_occlusion[index] / 65535.0f)
                * field.m_density_scale * length;
            tau_muffle +=
                ((float)field.m_muffle[index] / 65535.0f)
                * field.m_density_scale * length;

            t = next;

            if (t_max_x < t_max_y)
            {
                t_max_x += t_delta_x;
                cell_x += (int32_t)step_x;
            }
            else if (t_max_y < t_max_x)
            {
                t_max_y += t_delta_y;
                cell_y += (int32_t)step_y;
            }
            else
            {
                // 正好穿过格角：两个方向同时前进，避免下一轮 next == t。
                t_max_x += t_delta_x;
                t_max_y += t_delta_y;
                cell_x += (int32_t)step_x;
                cell_y += (int32_t)step_y;
            }

            if (cell_x < 0 || cell_x >= field.m_width || cell_y < 0
                || cell_y >= field.m_height)
                break;
        }

        result.m_gain = std::exp(-tau_occlusion);
        result.m_hf = std::exp(-tau_muffle);
        return result;
    }
}

bool je_audio_occlusion_bake(
    je_game_world_t* world, const char* world_path)
{
    if (!jeecs::audio::bake_audio_occlusion_world(world, world_path))
        return false;

    je_audio_occlusion_reload(world);
    return true;
}
