#define JE_IMPL
#include "jeecs_core_audio_environment.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace jeecs::audio
{
    namespace
    {
        constexpr float ROOM_WEIGHT_EPSILON = 1e-4f;
        constexpr float PORTAL_EDGE_EPSILON = 1e-4f;

        float room_weight(
            const math::vec3& point,
            const audio_environment_room& room,
            bool use_2d)
        {
            const math::vec3 local =
                room.m_rotation.inverse() * (point - room.m_center);

            const float dx =
                std::max(std::fabs(local.x) - room.m_half_extent.x, 0.0f);
            const float dy =
                std::max(std::fabs(local.y) - room.m_half_extent.y, 0.0f);
            const float dz = use_2d
                ? 0.0f
                : std::max(std::fabs(local.z) - room.m_half_extent.z, 0.0f);

            const float outside = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (outside <= 0.0f)
                return 1.0f;

            if (room.m_fade <= 1e-6f)
                return 0.0f;

            const float t = std::min(outside / room.m_fade, 1.0f);
            return 1.0f - t * t * (3.0f - 2.0f * t);
        }

        // 门洞两侧的房间解析必须是“点在房间内”的硬判定：fade 是混响发送
        // 权重在房间外侧的衰减距离，不代表该点属于房间。默认 fade=1 时，
        // 贴在墙上的门洞两侧会同时落进 fade 带，被算成同一个房间而失效。
        // 房间嵌套（小房间整只放进大房间）时多个房间都包含该点：取体积
        // 最小者——门洞属于它实际开在的那个（内层）房间。
        uint32_t find_room_inside(
            const std::vector<audio_environment_room>& rooms,
            const math::vec3& point,
            bool use_2d)
        {
            const audio_environment_room* best = nullptr;
            float best_volume = 0.0f;

            for (const auto& room : rooms)
            {
                // 点在 OBB 内时 room_weight 恰好返回 1.0
                if (room_weight(point, room, use_2d) < 1.0f)
                    continue;

                const float volume = room.m_half_extent.x
                    * room.m_half_extent.y * room.m_half_extent.z;
                if (best == nullptr || volume < best_volume)
                {
                    best = &room;
                    best_volume = volume;
                }
            }

            return best != nullptr ? best->m_id : AUDIO_OUTDOOR_ROOM_ID;
        }

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

        bool room_snapshot_less(
            const audio_environment_room_snapshot& a,
            const audio_environment_room_snapshot& b) noexcept
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

            if (a.m_fade != b.m_fade)
                return a.m_fade < b.m_fade;
            if (a.m_preset != b.m_preset)
                return a.m_preset < b.m_preset;
            if (a.m_decay_time != b.m_decay_time)
                return a.m_decay_time < b.m_decay_time;
            if (a.m_gain != b.m_gain)
                return a.m_gain < b.m_gain;
            if (a.m_gain_hf != b.m_gain_hf)
                return a.m_gain_hf < b.m_gain_hf;
            if (a.m_diffusion != b.m_diffusion)
                return a.m_diffusion < b.m_diffusion;
            if (a.m_send_gain != b.m_send_gain)
                return a.m_send_gain < b.m_send_gain;

            return a.m_source_id < b.m_source_id;
        }

        bool portal_snapshot_less(
            const audio_environment_portal_snapshot& a,
            const audio_environment_portal_snapshot& b) noexcept
        {
            if (vec_less(a.m_center, b.m_center))
                return true;
            if (vec_less(b.m_center, a.m_center))
                return false;

            if (vec_less(a.m_normal, b.m_normal))
                return true;
            if (vec_less(b.m_normal, a.m_normal))
                return false;

            if (a.m_half_thickness != b.m_half_thickness)
                return a.m_half_thickness < b.m_half_thickness;
            if (a.m_transmission != b.m_transmission)
                return a.m_transmission < b.m_transmission;
            if (a.m_muffle != b.m_muffle)
                return a.m_muffle < b.m_muffle;

            return a.m_source_id < b.m_source_id;
        }

        uint64_t hash_mix(uint64_t hash, uint64_t value) noexcept
        {
            hash ^= value;
            hash *= 1099511628211ull;
            return hash;
        }

        uint64_t hash_float(uint64_t hash, float value) noexcept
        {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return hash_mix(hash, bits);
        }

        uint64_t hash_vec(uint64_t hash, const math::vec3& v) noexcept
        {
            hash = hash_float(hash, v.x);
            hash = hash_float(hash, v.y);
            return hash_float(hash, v.z);
        }

        uint64_t hash_quat(uint64_t hash, const math::quat& q) noexcept
        {
            hash = hash_float(hash, q.x);
            hash = hash_float(hash, q.y);
            hash = hash_float(hash, q.z);
            return hash_float(hash, q.w);
        }
    }

    const audio_environment_path& audio_environment::path(
        uint32_t from_node, uint32_t to_node) const noexcept
    {
        const size_t n = node_count();
        const size_t index = (size_t)from_node * n + (size_t)to_node;

        assert(index < m_paths.size());
        return m_paths[index];
    }

    float audio_environment_room_weight(
        const math::vec3& point,
        const audio_environment_room& room,
        bool use_2d)
    {
        return room_weight(point, room, use_2d);
    }

    uint64_t hash_audio_environment_inputs(
        const std::vector<audio_environment_room_snapshot>& rooms,
        const std::vector<audio_environment_portal_snapshot>& portals,
        bool use_2d)
    {
        std::vector<audio_environment_room_snapshot> sorted_rooms = rooms;
        std::vector<audio_environment_portal_snapshot> sorted_portals = portals;

        std::sort(sorted_rooms.begin(), sorted_rooms.end(), room_snapshot_less);
        std::sort(
            sorted_portals.begin(), sorted_portals.end(), portal_snapshot_less);

        uint64_t hash = 14695981039346656037ull;
        hash = hash_mix(hash, use_2d ? 1u : 0u);
        hash = hash_mix(hash, (uint64_t)sorted_rooms.size());
        hash = hash_mix(hash, (uint64_t)sorted_portals.size());

        for (const auto& room : sorted_rooms)
        {
            hash = hash_mix(hash, room.m_source_id);
            hash = hash_vec(hash, room.m_center);
            hash = hash_quat(hash, room.m_rotation);
            hash = hash_vec(hash, room.m_half_extent);
            hash = hash_float(hash, room.m_fade);
            hash = hash_mix(hash, (uint64_t)room.m_preset);
            hash = hash_float(hash, room.m_decay_time);
            hash = hash_float(hash, room.m_gain);
            hash = hash_float(hash, room.m_gain_hf);
            hash = hash_float(hash, room.m_diffusion);
            hash = hash_float(hash, room.m_send_gain);
        }

        for (const auto& portal : sorted_portals)
        {
            hash = hash_mix(hash, portal.m_source_id);
            hash = hash_vec(hash, portal.m_center);
            hash = hash_vec(hash, portal.m_normal);
            hash = hash_float(hash, portal.m_half_thickness);
            hash = hash_float(hash, portal.m_transmission);
            hash = hash_float(hash, portal.m_muffle);
        }

        return hash;
    }

    audio_environment build_audio_environment(
        const std::vector<audio_environment_room_snapshot>& rooms,
        const std::vector<audio_environment_portal_snapshot>& portals,
        bool use_2d)
    {
        audio_environment result;

        std::vector<audio_environment_room_snapshot> sorted_rooms = rooms;
        std::sort(sorted_rooms.begin(), sorted_rooms.end(), room_snapshot_less);

        result.m_rooms.reserve(sorted_rooms.size());
        for (size_t i = 0; i < sorted_rooms.size(); ++i)
        {
            const auto& source = sorted_rooms[i];

            audio_environment_room room;
            room.m_id = (uint32_t)i;
            room.m_center = source.m_center;
            room.m_rotation = source.m_rotation;
            room.m_half_extent = source.m_half_extent;
            room.m_fade = source.m_fade;
            room.m_preset = source.m_preset;
            room.m_decay_time = source.m_decay_time;
            room.m_gain = source.m_gain;
            room.m_gain_hf = source.m_gain_hf;
            room.m_diffusion = source.m_diffusion;
            room.m_send_gain = source.m_send_gain;

            result.m_rooms.push_back(room);
        }

        std::vector<audio_environment_portal_snapshot> sorted_portals = portals;
        std::sort(
            sorted_portals.begin(), sorted_portals.end(), portal_snapshot_less);

        for (const auto& source : sorted_portals)
        {
            math::vec3 normal = source.m_normal;
            if (normal.length() <= 1e-6f)
                continue;
            normal = normal.unit();

            const float offset =
                std::max(source.m_half_thickness, 0.01f) + 0.01f;

            const uint32_t room_a = find_room_inside(
                result.m_rooms, source.m_center + normal * offset, use_2d);
            const uint32_t room_b = find_room_inside(
                result.m_rooms, source.m_center - normal * offset, use_2d);

            if (room_a == room_b)
                continue;
            if (room_a == AUDIO_OUTDOOR_ROOM_ID
                && room_b == AUDIO_OUTDOOR_ROOM_ID)
                continue;
            if (source.m_transmission <= PORTAL_EDGE_EPSILON)
                continue;

            audio_environment_portal portal;
            portal.m_room_a = room_a;
            portal.m_room_b = room_b;
            portal.m_center = source.m_center;
            portal.m_normal = normal;
            portal.m_transmission = source.m_transmission;
            portal.m_muffle = source.m_muffle;

            result.m_portals.push_back(portal);
        }

        // 房间 + 室外节点
        const size_t node_count = result.m_rooms.size() + 1;
        const float inf = std::numeric_limits<float>::infinity();

        struct edge_t
        {
            uint32_t m_to;
            float m_transmission;
            float m_muffle;
        };

        std::vector<std::vector<edge_t>> adjacency(node_count);
        for (const auto& portal : result.m_portals)
        {
            const uint32_t a = result.node_of_room(portal.m_room_a);
            const uint32_t b = result.node_of_room(portal.m_room_b);

            if (a == b)
                continue;

            adjacency[a].push_back(edge_t{
                b, portal.m_transmission, portal.m_muffle });
            adjacency[b].push_back(edge_t{
                a, portal.m_transmission, portal.m_muffle });
        }

        result.m_paths.resize(node_count * node_count);

        std::vector<float> cost(node_count);
        std::vector<uint8_t> visited(node_count);
        std::vector<audio_environment_path> best(node_count);

        for (uint32_t from = 0; from < (uint32_t)node_count; ++from)
        {
            std::fill(cost.begin(), cost.end(), inf);
            std::fill(visited.begin(), visited.end(), (uint8_t)0);
            std::fill(best.begin(), best.end(), audio_environment_path());

            cost[from] = 0.0f;
            best[from].m_transmission = 1.0f;

            for (size_t step = 0; step < node_count; ++step)
            {
                uint32_t current = (uint32_t)node_count;
                float current_cost = inf;

                for (uint32_t i = 0; i < (uint32_t)node_count; ++i)
                {
                    if (!visited[i] && cost[i] < current_cost)
                    {
                        current_cost = cost[i];
                        current = i;
                    }
                }

                if (current == (uint32_t)node_count)
                    break;

                visited[current] = 1;

                for (const auto& edge : adjacency[current])
                {
                    if (visited[edge.m_to])
                        continue;

                    const float next_cost = current_cost
                        - std::log(std::max(
                            edge.m_transmission, 1e-6f));

                    if (next_cost >= cost[edge.m_to])
                        continue;

                    cost[edge.m_to] = next_cost;

                    audio_environment_path& next = best[edge.m_to];
                    next.m_transmission =
                        best[current].m_transmission * edge.m_transmission;
                    next.m_muffle = 1.0f
                        - (1.0f - best[current].m_muffle)
                            * (1.0f - edge.m_muffle);
                }
            }

            for (uint32_t to = 0; to < (uint32_t)node_count; ++to)
                result.m_paths[(size_t)from * node_count + to] = best[to];
        }

        return result;
    }

}
