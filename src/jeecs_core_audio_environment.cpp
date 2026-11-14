#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs_core_audio_environment.hpp"

#include "jeecs/cache.hpp"
#include "jeecs/components.hpp"
#include "jeecs/debug.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/typing.hpp"
#include "jeecs_basic_cache_version.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace jeecs::audio
{
    static_assert(std::is_trivially_copyable_v<audio_environment_path>);
    static_assert(
        sizeof(audio_environment_path) == sizeof(float) * 2 + sizeof(int32_t),
        "audio_environment_path is written to the cache as a flat buffer.");

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

        uint32_t find_room(
            const std::vector<audio_environment_room>& rooms,
            const math::vec3& point,
            bool use_2d)
        {
            uint32_t result = AUDIO_OUTDOOR_ROOM_ID;
            float best_weight = ROOM_WEIGHT_EPSILON;

            for (const auto& room : rooms)
            {
                const float weight = room_weight(point, room, use_2d);
                if (weight > best_weight)
                {
                    best_weight = weight;
                    result = room.m_id;
                }
            }

            return result;
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

    uint32_t audio_environment_find_room(
        const std::vector<audio_environment_room>& rooms,
        const math::vec3& point,
        bool use_2d)
    {
        return find_room(rooms, point, use_2d);
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
        result.m_use_2d = use_2d;
        result.m_signature = hash_audio_environment_inputs(rooms, portals, use_2d);

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

            const uint32_t room_a = find_room(
                result.m_rooms, source.m_center + normal * offset, use_2d);
            const uint32_t room_b = find_room(
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
                    next.m_portal_count =
                        best[current].m_portal_count + 1;
                }
            }

            for (uint32_t to = 0; to < (uint32_t)node_count; ++to)
                result.m_paths[(size_t)from * node_count + to] = best[to];
        }

        return result;
    }

    bool write_audio_environment_cache(
        const char* world_path, const audio_environment& environment)
    {
        je_writing_cache_file_t* file =
            je_cache_file_create(world_path, WORLD_CACHE_VERSION, 0);

        if (file == nullptr)
        {
            debug::logerr(
                "Failed to create audio environment cache for world: %s.",
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
        auto append_bool = [&](const char* key, bool value)
        {
            append_key(key);
            ok = ok && je_cache_binary_append_bool(file, value);
        };
        auto append_float = [&](const char* key, float value)
        {
            append_key(key);
            ok = ok && je_cache_binary_append_float32(file, value);
        };
        auto append_vec3 = [&](const char* prefix, const math::vec3& value)
        {
            const std::string x = std::string(prefix) + "_x";
            const std::string y = std::string(prefix) + "_y";
            const std::string z = std::string(prefix) + "_z";
            append_float(x.c_str(), value.x);
            append_float(y.c_str(), value.y);
            append_float(z.c_str(), value.z);
        };
        auto append_quat = [&](const char* prefix, const math::quat& value)
        {
            const std::string x = std::string(prefix) + "_x";
            const std::string y = std::string(prefix) + "_y";
            const std::string z = std::string(prefix) + "_z";
            const std::string w = std::string(prefix) + "_w";
            append_float(x.c_str(), value.x);
            append_float(y.c_str(), value.y);
            append_float(z.c_str(), value.z);
            append_float(w.c_str(), value.w);
        };

        ok = ok && je_cache_binary_append_document(file);

        append_key("audio_environment");
        ok = ok && je_cache_binary_append_document(file);

        append_int32("format", AUDIO_ENVIRONMENT_FORMAT_VERSION);
        append_bool("use_2d", environment.m_use_2d);

        append_key("rooms");
        ok = ok && je_cache_binary_append_list(file);
        for (const auto& room : environment.m_rooms)
        {
            ok = ok && je_cache_binary_append_document(file);
            append_int32("id", (int32_t)room.m_id);
            append_vec3("center", room.m_center);
            append_quat("rotation", room.m_rotation);
            append_vec3("half_extent", room.m_half_extent);
            append_float("fade", room.m_fade);
            append_int32("preset", (int32_t)room.m_preset);
            append_float("decay_time", room.m_decay_time);
            append_float("gain", room.m_gain);
            append_float("gain_hf", room.m_gain_hf);
            append_float("diffusion", room.m_diffusion);
            append_float("send_gain", room.m_send_gain);
            ok = ok && je_cache_binary_append_end(file);
        }
        ok = ok && je_cache_binary_append_end(file);

        append_key("portals");
        ok = ok && je_cache_binary_append_list(file);
        for (const auto& portal : environment.m_portals)
        {
            ok = ok && je_cache_binary_append_document(file);
            append_int32("room_a", (int32_t)portal.m_room_a);
            append_int32("room_b", (int32_t)portal.m_room_b);
            append_vec3("center", portal.m_center);
            append_vec3("normal", portal.m_normal);
            append_float("transmission", portal.m_transmission);
            append_float("muffle", portal.m_muffle);
            ok = ok && je_cache_binary_append_end(file);
        }
        ok = ok && je_cache_binary_append_end(file);

        append_key("paths");
        ok = ok && je_cache_binary_append_buffer(
            file,
            environment.m_paths.data(),
            environment.m_paths.size() * sizeof(audio_environment_path));

        ok = ok && je_cache_binary_append_end(file); // audio_environment
        ok = ok && je_cache_binary_append_end(file); // root document

        je_cache_file_close(file, ok);

        if (!ok)
        {
            debug::logerr(
                "Failed to write audio environment cache for world: %s.",
                world_path);
            return false;
        }

        debug::loginfo(
            "Baked audio environment: %s (%zu rooms, %zu portals).",
            world_path,
            environment.m_rooms.size(),
            environment.m_portals.size());

        return true;
    }

    std::optional<audio_environment> load_audio_environment_cache(
        const char* world_path)
    {
        je_file_t* file =
            je_cache_file_load(world_path, WORLD_CACHE_VERSION, 0);

        if (file == nullptr)
            return std::nullopt;

        auto fail = [&]() -> std::optional<audio_environment>
        {
            debug::logerr(
                "Failed to parse audio environment cache for world: %s.",
                world_path);
            je_file_close(file);
            return std::nullopt;
        };

        je_cache_binary_document_t root_doc;
        if (!je_cache_binary_document_open(file, &root_doc))
            return fail();

        je_cache_binary_document_t environment_doc;
        if (!je_cache_binary_document_index(
                &root_doc,
                "audio_environment",
                JE_CACHE_BINARY_VALUE_DOCUMENT,
                &environment_doc))
            return fail();

        int32_t format = 0;
        if (!je_cache_binary_document_index(
                &environment_doc,
                "format",
                JE_CACHE_BINARY_VALUE_INT32,
                &format)
            || format != AUDIO_ENVIRONMENT_FORMAT_VERSION)
            return fail();

        audio_environment result;

        if (!je_cache_binary_document_index(
                &environment_doc,
                "use_2d",
                JE_CACHE_BINARY_VALUE_BOOL,
                &result.m_use_2d))
            return fail();

        auto read_float = [&](const je_cache_binary_document_t& doc,
                              const char* key, float& out) -> bool
        {
            return je_cache_binary_document_index(
                &doc, key, JE_CACHE_BINARY_VALUE_FLOAT32, &out);
        };
        auto read_vec3 = [&](const je_cache_binary_document_t& doc,
                             const char* prefix, math::vec3& out) -> bool
        {
            const std::string x = std::string(prefix) + "_x";
            const std::string y = std::string(prefix) + "_y";
            const std::string z = std::string(prefix) + "_z";
            return read_float(doc, x.c_str(), out.x)
                && read_float(doc, y.c_str(), out.y)
                && read_float(doc, z.c_str(), out.z);
        };
        auto read_quat = [&](const je_cache_binary_document_t& doc,
                             const char* prefix, math::quat& out) -> bool
        {
            const std::string x = std::string(prefix) + "_x";
            const std::string y = std::string(prefix) + "_y";
            const std::string z = std::string(prefix) + "_z";
            const std::string w = std::string(prefix) + "_w";
            return read_float(doc, x.c_str(), out.x)
                && read_float(doc, y.c_str(), out.y)
                && read_float(doc, z.c_str(), out.z)
                && read_float(doc, w.c_str(), out.w);
        };

        je_cache_binary_list_t rooms;
        if (!je_cache_binary_document_index(
                &environment_doc,
                "rooms",
                JE_CACHE_BINARY_VALUE_LIST,
                &rooms))
            return fail();

        je_cache_binary_list_iterator_t room_it;
        if (!je_cache_binary_list_iterator_open(&rooms, &room_it))
            return fail();

        for (;;)
        {
            if (je_cache_binary_list_iterator_is_end(&room_it))
                break;

            je_cache_binary_document_t room_doc;
            if (!je_cache_binary_list_iterator_read(
                    &room_it,
                    JE_CACHE_BINARY_VALUE_DOCUMENT,
                    &room_doc))
                return fail();

            audio_environment_room room;

            int32_t id = 0;
            int32_t preset = 0;

            if (!je_cache_binary_document_index(
                    &room_doc,
                    "id",
                    JE_CACHE_BINARY_VALUE_INT32,
                    &id)
                || !read_vec3(room_doc, "center", room.m_center)
                || !read_quat(room_doc, "rotation", room.m_rotation)
                || !read_vec3(room_doc, "half_extent", room.m_half_extent)
                || !read_float(room_doc, "fade", room.m_fade)
                || !je_cache_binary_document_index(
                    &room_doc,
                    "preset",
                    JE_CACHE_BINARY_VALUE_INT32,
                    &preset)
                || !read_float(room_doc, "decay_time", room.m_decay_time)
                || !read_float(room_doc, "gain", room.m_gain)
                || !read_float(room_doc, "gain_hf", room.m_gain_hf)
                || !read_float(room_doc, "diffusion", room.m_diffusion)
                || !read_float(room_doc, "send_gain", room.m_send_gain))
                return fail();

            if (id < 0 || (uint32_t)id != (uint32_t)result.m_rooms.size())
                return fail();

            room.m_id = (uint32_t)id;
            room.m_preset = (je_audio_effect_eax_reverb_preset_t)preset;

            result.m_rooms.push_back(room);

            if (!je_cache_binary_list_iterator_advance(&room_it))
            {
                if (!je_cache_binary_list_iterator_is_end(&room_it))
                    return fail();
                break;
            }
        }

        je_cache_binary_list_t portals;
        if (!je_cache_binary_document_index(
                &environment_doc,
                "portals",
                JE_CACHE_BINARY_VALUE_LIST,
                &portals))
            return fail();

        je_cache_binary_list_iterator_t portal_it;
        if (!je_cache_binary_list_iterator_open(&portals, &portal_it))
            return fail();

        for (;;)
        {
            if (je_cache_binary_list_iterator_is_end(&portal_it))
                break;

            je_cache_binary_document_t portal_doc;
            if (!je_cache_binary_list_iterator_read(
                    &portal_it,
                    JE_CACHE_BINARY_VALUE_DOCUMENT,
                    &portal_doc))
                return fail();

            audio_environment_portal portal;

            int32_t room_a = 0;
            int32_t room_b = 0;

            if (!je_cache_binary_document_index(
                    &portal_doc,
                    "room_a",
                    JE_CACHE_BINARY_VALUE_INT32,
                    &room_a)
                || !je_cache_binary_document_index(
                    &portal_doc,
                    "room_b",
                    JE_CACHE_BINARY_VALUE_INT32,
                    &room_b)
                || !read_vec3(portal_doc, "center", portal.m_center)
                || !read_vec3(portal_doc, "normal", portal.m_normal)
                || !read_float(portal_doc, "transmission", portal.m_transmission)
                || !read_float(portal_doc, "muffle", portal.m_muffle))
                return fail();

            portal.m_room_a = (uint32_t)room_a;
            portal.m_room_b = (uint32_t)room_b;

            const uint32_t room_count = (uint32_t)result.m_rooms.size();
            auto valid_room_id = [room_count](uint32_t id)
            {
                return id == AUDIO_OUTDOOR_ROOM_ID || id < room_count;
            };

            if (!valid_room_id(portal.m_room_a)
                || !valid_room_id(portal.m_room_b))
                return fail();

            result.m_portals.push_back(portal);

            if (!je_cache_binary_list_iterator_advance(&portal_it))
            {
                if (!je_cache_binary_list_iterator_is_end(&portal_it))
                    return fail();
                break;
            }
        }

        je_cache_binary_buffer_t paths;
        if (!je_cache_binary_document_index(
                &environment_doc,
                "paths",
                JE_CACHE_BINARY_VALUE_BUFFER,
                &paths))
            return fail();

        size_t paths_size = 0;
        if (!je_cache_binary_buffer_read(nullptr, &paths_size, &paths))
            return fail();

        const size_t node_count = result.m_rooms.size() + 1;
        const size_t expected_size =
            node_count * node_count * sizeof(audio_environment_path);

        if (paths_size != expected_size)
            return fail();

        result.m_paths.resize(node_count * node_count);

        size_t paths_capacity = paths_size;
        if (!je_cache_binary_buffer_read(
                result.m_paths.data(), &paths_capacity, &paths)
            || paths_capacity != paths_size)
            return fail();

        je_file_close(file);

        debug::loginfo(
            "Loaded baked audio environment: %s (%zu rooms, %zu portals).",
            world_path,
            result.m_rooms.size(),
            result.m_portals.size());

        return result;
    }

    namespace
    {
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

        template <typename T>
        T* bake_get_component(je_game_entity_t* entity)
        {
            return static_cast<T*>(je_ecs_world_entity_component_get(
                entity, typing::id<T>()));
        }
    }

    bool bake_audio_environment_world(
        je_game_world_t* world, const char* world_path)
    {
        je_game_entity_t** entity_list =
            je_debug_world_entities_get_all(world);
        assert(entity_list != nullptr);

        // 1. 解析所有 Anchor 实体的世界变换；只读组件，不要求世界已启用/
        //    已 update（导出时会临时 restore 未打开的世界）。
        std::unordered_map<typing::uuid, bake_resolved_transform> resolved;
        std::vector<bake_pending_transform> pending;

        for (je_game_entity_t** it = entity_list; *it != nullptr; ++it)
        {
            je_game_entity_t* entity = *it;

            if (bake_get_component<Transform::Translation>(entity) == nullptr)
                continue;

            auto* anchor = bake_get_component<Transform::Anchor>(entity);
            auto* l2w = bake_get_component<Transform::LocalToWorld>(entity);
            auto* l2p = bake_get_component<Transform::LocalToParent>(entity);
            auto* position =
                bake_get_component<Transform::LocalPosition>(entity);
            auto* rotation =
                bake_get_component<Transform::LocalRotation>(entity);
            auto* scale = bake_get_component<Transform::LocalScale>(entity);

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
            else if (l2p != nullptr && l2w == nullptr && anchor != nullptr)
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

        auto resolve_entity = [&](Transform::Anchor* anchor,
                                  Transform::LocalToParent* l2p,
                                  Transform::LocalPosition* position,
                                  Transform::LocalRotation* rotation,
                                  Transform::LocalScale* scale)
        {
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
        };

        // 2. 房间/门洞快照
        std::vector<audio_environment_room_snapshot> rooms;
        std::vector<audio_environment_portal_snapshot> portals;
        bool use_2d = false;

        for (je_game_entity_t** it = entity_list; *it != nullptr; ++it)
        {
            je_game_entity_t* entity = *it;

            auto* translation =
                bake_get_component<Transform::Translation>(entity);
            if (translation == nullptr)
                continue;

            auto* anchor = bake_get_component<Transform::Anchor>(entity);
            auto* l2p = bake_get_component<Transform::LocalToParent>(entity);
            auto* position =
                bake_get_component<Transform::LocalPosition>(entity);
            auto* rotation =
                bake_get_component<Transform::LocalRotation>(entity);
            auto* scale = bake_get_component<Transform::LocalScale>(entity);

            if (auto* listener =
                    bake_get_component<Audio::Listener>(entity);
                listener != nullptr)
            {
                use_2d = listener->use_2d;
            }

            if (auto* zone = bake_get_component<Audio::ReverbZone>(entity);
                zone != nullptr)
            {
                const bake_resolved_transform transform = resolve_entity(
                    anchor, l2p, position, rotation, scale);

                audio_environment_room_snapshot snapshot;
                snapshot.m_source_id = entity->_m_id;
                snapshot.m_center = transform.m_position;
                snapshot.m_rotation = transform.m_rotation;
                snapshot.m_half_extent = math::vec3(
                    std::fabs(zone->size.x * transform.m_local_scale.x) * 0.5f,
                    std::fabs(zone->size.y * transform.m_local_scale.y) * 0.5f,
                    std::fabs(zone->size.z * transform.m_local_scale.z) * 0.5f);
                snapshot.m_fade = zone->fade;
                snapshot.m_preset = zone->preset;
                snapshot.m_decay_time = zone->decay_time;
                snapshot.m_gain = zone->gain;
                snapshot.m_gain_hf = zone->gain_hf;
                snapshot.m_diffusion = zone->diffusion;
                snapshot.m_send_gain = zone->send_gain;

                rooms.push_back(snapshot);
            }

            if (auto* portal = bake_get_component<Audio::Portal>(entity);
                portal != nullptr && portal->enabled)
            {
                const bake_resolved_transform transform = resolve_entity(
                    anchor, l2p, position, rotation, scale);

                math::vec3 normal = transform.m_rotation * portal->normal;
                if (normal.length() <= 1e-6f)
                    continue;
                normal = normal.unit();

                audio_environment_portal_snapshot snapshot;
                snapshot.m_source_id = entity->_m_id;
                snapshot.m_center = transform.m_position;
                snapshot.m_normal = normal;
                snapshot.m_half_thickness =
                    std::fabs(
                        portal->size.z * transform.m_local_scale.z) * 0.5f;
                snapshot.m_transmission =
                    std::clamp(portal->transmission, 0.0f, 1.0f);
                snapshot.m_muffle =
                    std::clamp(portal->muffle, 0.0f, 1.0f);

                portals.push_back(snapshot);
            }
        }

        for (je_game_entity_t** it = entity_list; *it != nullptr; ++it)
            je_debug_entity_free(*it);
        je_mem_free(entity_list);

        // 3. 构建并写 sidecar
        const audio_environment environment =
            build_audio_environment(rooms, portals, use_2d);

        if (!write_audio_environment_cache(world_path, environment))
            return false;

        je_ecs_world_source_path_set(world, world_path);
        je_audio_environment_reload(world);

        return true;
    }
}

bool je_audio_environment_bake(
    je_game_world_t* world, const char* world_path)
{
    return jeecs::audio::bake_audio_environment_world(world, world_path);
}
