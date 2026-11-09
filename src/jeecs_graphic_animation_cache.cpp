#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_basic_cache_version.hpp"

#include "wo.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace jeecs::animation {}

using namespace std;
using namespace jeecs;

namespace jeecs::animation
{
    namespace
    {
        // 动画不再直接读取 .je4animation 源文件，而是读取由
        // builtin/script/internal/cache/animation.wo 生成的二进制缓存：
        //   <动画源路径>.je4cache
        // 缓存文件头由 je_cache_file_create 写入（格式版本 + 源文件 CRC64；
        // 管理器版本留空占位，由 je_cache_file_close(..., true) 回写后缓存才
        // 生效），其后是一个 jeecs_cache_binary_* 文档。文档结构：
        //   DOCUMENT
        //     "actions" -> LIST of DOCUMENT(action)
        //         "name" -> STRING
        //         "duration" -> FLOAT32
        //         "loop" -> INT32              // je_animation_loop_mode_t
        //         "tracks" -> LIST of DOCUMENT(track)
        //             "kind" -> INT32          // je_animation_track_kind_t
        //             "component" -> STRING    // MEMBER 轨道
        //             "member" -> STRING       // MEMBER 轨道
        //             "uniform" -> STRING      // UNIFORM 轨道
        //             "offset" -> BOOL
        //             "keys" -> LIST of DOCUMENT(key)
        //                 "time" -> FLOAT32
        //                 "interp" -> INT32    // je_animation_interp_mode_t
        //                 "tangent" -> INT32   // je_animation_tangent_mode_t（HERMITE）
        //                 "value" -> INT32 | FLOAT32 | LIST of FLOAT32
        //                 "tangents" -> LIST of FLOAT32   // 通道数*2 (入,出)
        //         "events" -> LIST of DOCUMENT
        //             "time" -> FLOAT32
        //             "name" -> STRING
        //   END
        // QUAT4 的 value 是四元数 (x, y, z, w)，由生成器从 TOML 中的欧拉角换算；
        // AUTO/LINEAR 切线由生成器烘焙为具体导数值后写入 tangents。
        //
        // 解析结果写入纯 C 的 je_animation_t 结构（连续数组，无 C++ 容器），
        // 由 je_animation_close 统一释放；组件成员的类型校验在加载时完成。
        bool _read_cache_string(
            const je_cache_binary_document_t* document,
            const char* key,
            string* out)
        {
            je_cache_binary_string_t str;
            if (!je_cache_binary_document_index(
                document, key, JE_CACHE_BINARY_VALUE_STRING, &str))
                return false;

            size_t len = 0;
            if (!je_cache_binary_string_read(nullptr, &len, &str))
                return false;

            out->resize(len);
            if (len > 0)
            {
                size_t capacity = len;
                if (!je_cache_binary_string_read(out->data(), &capacity, &str))
                    return false;
            }
            return true;
        }

        bool _read_cache_floats(
            const je_cache_binary_list_t* list, float* out, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
            {
                if (!je_cache_binary_list_index(
                    list, i, JE_CACHE_BINARY_VALUE_FLOAT32, out + i))
                    return false;
            }
            return true;
        }

        bool _read_cache_data_value(
            const je_cache_binary_document_t* document,
            je_animation_value_t* out)
        {
            int32_t type_tag;
            if (!je_cache_binary_document_index(
                document, "type", JE_CACHE_BINARY_VALUE_INT32, &type_tag))
                return false;

            switch (type_tag)
            {
            case JE_ANIMATION_VALUE_INT:
            {
                int32_t value;
                if (!je_cache_binary_document_index(
                    document, "value", JE_CACHE_BINARY_VALUE_INT32, &value))
                    return false;

                out->m_type = (uint8_t)JE_ANIMATION_VALUE_INT;
                out->m_value.i32 = value;
                return true;
            }

            case JE_ANIMATION_VALUE_FLOAT:
            {
                float value;
                if (!je_cache_binary_document_index(
                    document, "value", JE_CACHE_BINARY_VALUE_FLOAT32, &value))
                    return false;

                out->m_type = (uint8_t)JE_ANIMATION_VALUE_FLOAT;
                out->m_value.f32 = value;
                return true;
            }

            case JE_ANIMATION_VALUE_VEC2:
            {
                je_cache_binary_list_t values;
                if (!je_cache_binary_document_index(
                    document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                    || !_read_cache_floats(&values, out->m_value.v2, 2))
                    return false;

                out->m_type = (uint8_t)JE_ANIMATION_VALUE_VEC2;
                return true;
            }

            case JE_ANIMATION_VALUE_VEC3:
            {
                je_cache_binary_list_t values;
                if (!je_cache_binary_document_index(
                    document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                    || !_read_cache_floats(&values, out->m_value.v3, 3))
                    return false;

                out->m_type = (uint8_t)JE_ANIMATION_VALUE_VEC3;
                return true;
            }

            case JE_ANIMATION_VALUE_VEC4:
            {
                je_cache_binary_list_t values;
                if (!je_cache_binary_document_index(
                    document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                    || !_read_cache_floats(&values, out->m_value.v4, 4))
                    return false;

                out->m_type = (uint8_t)JE_ANIMATION_VALUE_VEC4;
                return true;
            }

            case JE_ANIMATION_VALUE_QUAT4:
            {
                je_cache_binary_list_t values;
                if (!je_cache_binary_document_index(
                    document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                    || !_read_cache_floats(&values, out->m_value.q4, 4))
                    return false;

                out->m_type = (uint8_t)JE_ANIMATION_VALUE_QUAT4;
                return true;
            }

            default:
                return false;
            }
        }

        // 组件成员类型必须在加载时匹配，运行期不再逐帧校验。
        bool _animation_member_type_matches(
            uint8_t type, const je_type_info_t* member_type)
        {
            switch (type)
            {
            case JE_ANIMATION_VALUE_INT:
                return member_type == typing::of<int>();
            case JE_ANIMATION_VALUE_FLOAT:
                return member_type == typing::of<float>();
            case JE_ANIMATION_VALUE_VEC2:
                return member_type == typing::of<math::vec2>();
            case JE_ANIMATION_VALUE_VEC3:
                return member_type == typing::of<math::vec3>();
            case JE_ANIMATION_VALUE_VEC4:
                return member_type == typing::of<math::vec4>()
                    || member_type == typing::of<math::color>();
            case JE_ANIMATION_VALUE_QUAT4:
                return member_type == typing::of<math::quat>();
            }
            return false;
        }

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

        // HERMITE 依赖切线，只对浮点通道开放；QUAT4 的 LINEAR 为 slerp。
        bool _interp_mode_allowed(uint8_t value_type, uint8_t interp)
        {
            switch (interp)
            {
            case JE_ANIMATION_INTERP_CONSTANT:
            case JE_ANIMATION_INTERP_LINEAR:
                return true;
            case JE_ANIMATION_INTERP_HERMITE:
                return value_type == JE_ANIMATION_VALUE_FLOAT
                    || value_type == JE_ANIMATION_VALUE_VEC2
                    || value_type == JE_ANIMATION_VALUE_VEC3
                    || value_type == JE_ANIMATION_VALUE_VEC4;
            }
            return false;
        }

        struct KeyData
        {
            float m_time = 0.0f;
            je_animation_value_t m_value{};
            uint8_t m_interp = JE_ANIMATION_INTERP_CONSTANT;
            uint8_t m_tangent_mode = JE_ANIMATION_TANGENT_AUTO;
            vector<float> m_tangents;
        };
        struct TrackData
        {
            uint8_t m_kind = JE_ANIMATION_TRACK_MEMBER;
            bool m_offset_mode = false;

            const je_type_info_t* m_component_type = nullptr;
            const je_member_info_t* m_member_info = nullptr;
            string m_uniform_name;

            vector<KeyData> m_keys;
        };
        struct EventData
        {
            float m_time = 0.0f;
            string m_name;
        };
        struct ActionData
        {
            string m_name;
            float m_duration = 0.0f;
            uint8_t m_loop_mode = JE_ANIMATION_LOOP_NONE;
            vector<TrackData> m_tracks;
            vector<EventData> m_events;
        };

        char* _copy_c_string(const string& str)
        {
            char* result = new char[str.size() + 1];
            memcpy(result, str.c_str(), str.size());
            result[str.size()] = '\0';
            return result;
        }

        je_animation_t* _build_animation(const vector<ActionData>& actions)
        {
            auto* result = new je_animation_t;
            result->m_actions = actions.empty()
                ? nullptr
                : new je_animation_action_data_t[actions.size()];
            result->m_action_count = actions.size();

            for (size_t i = 0; i < actions.size(); ++i)
            {
                const ActionData& src_action = actions[i];
                je_animation_action_data_t& dst_action = result->m_actions[i];

                dst_action.m_name = _copy_c_string(src_action.m_name);
                dst_action.m_duration = src_action.m_duration;
                dst_action.m_loop_mode = src_action.m_loop_mode;

                dst_action.m_tracks = src_action.m_tracks.empty()
                    ? nullptr
                    : new je_animation_track_t[src_action.m_tracks.size()];
                dst_action.m_track_count = src_action.m_tracks.size();

                for (size_t j = 0; j < src_action.m_tracks.size(); ++j)
                {
                    const TrackData& src_track = src_action.m_tracks[j];
                    je_animation_track_t& dst_track = dst_action.m_tracks[j];

                    dst_track.m_kind = src_track.m_kind;
                    dst_track.m_offset_mode = src_track.m_offset_mode;
                    dst_track.m_component_type = src_track.m_component_type;
                    dst_track.m_member_info = src_track.m_member_info;
                    dst_track.m_uniform_name = src_track.m_kind == JE_ANIMATION_TRACK_UNIFORM
                        ? _copy_c_string(src_track.m_uniform_name)
                        : nullptr;

                    dst_track.m_keys = src_track.m_keys.empty()
                        ? nullptr
                        : new je_animation_key_t[src_track.m_keys.size()];
                    dst_track.m_key_count = src_track.m_keys.size();

                    for (size_t k = 0; k < src_track.m_keys.size(); ++k)
                    {
                        const KeyData& src_key = src_track.m_keys[k];
                        je_animation_key_t& dst_key = dst_track.m_keys[k];

                        dst_key.m_time = src_key.m_time;
                        dst_key.m_value = src_key.m_value;
                        dst_key.m_interp = src_key.m_interp;
                        dst_key.m_tangent_mode = src_key.m_tangent_mode;

                        if (src_key.m_tangents.empty())
                            dst_key.m_tangents = nullptr;
                        else
                        {
                            dst_key.m_tangents =
                                new float[src_key.m_tangents.size()];
                            memcpy(
                                dst_key.m_tangents,
                                src_key.m_tangents.data(),
                                src_key.m_tangents.size() * sizeof(float));
                        }
                    }

                    // 轨道值类型由其关键帧推得（加载期已保证全部一致）
                    dst_track.m_value_type = src_track.m_keys.empty()
                        ? (uint8_t)JE_ANIMATION_VALUE_FLOAT
                        : src_track.m_keys.front().m_value.m_type;
                }

                dst_action.m_events = src_action.m_events.empty()
                    ? nullptr
                    : new je_animation_event_t[src_action.m_events.size()];
                dst_action.m_event_count = src_action.m_events.size();

                for (size_t j = 0; j < src_action.m_events.size(); ++j)
                {
                    dst_action.m_events[j].m_time = src_action.m_events[j].m_time;
                    dst_action.m_events[j].m_name =
                        _copy_c_string(src_action.m_events[j].m_name);
                }
            }

            return result;
        }

        // 读取一条轨道的关键帧列表；返回 false 表示轨道无效（已记录日志）。
        bool _load_track_keys(
            const char* path,
            const char* action_name,
            size_t track_index,
            const je_cache_binary_list_t* cache_keys,
            TrackData* out_track)
        {
            bool value_type_established = false;
            uint8_t track_value_type = JE_ANIMATION_VALUE_FLOAT;

            for (size_t key_index = 0;; ++key_index)
            {
                je_cache_binary_document_t key;
                if (!je_cache_binary_list_index(
                    cache_keys, key_index, JE_CACHE_BINARY_VALUE_DOCUMENT, &key))
                    break;

                KeyData temp_key;
                if (!je_cache_binary_document_index(
                    &key, "time", JE_CACHE_BINARY_VALUE_FLOAT32, &temp_key.m_time))
                {
                    debug::logerr(
                        "Animation '%s' action '%s' track %zu key %zu has no time.",
                        path, action_name, track_index, key_index);
                    return false;
                }

                int32_t interp_tag;
                if (!je_cache_binary_document_index(
                    &key, "interp", JE_CACHE_BINARY_VALUE_INT32, &interp_tag))
                {
                    debug::logerr(
                        "Animation '%s' action '%s' track %zu key %zu has no interp mode.",
                        path, action_name, track_index, key_index);
                    return false;
                }
                temp_key.m_interp = (uint8_t)interp_tag;

                int32_t tangent_tag;
                if (!je_cache_binary_document_index(
                    &key, "tangent", JE_CACHE_BINARY_VALUE_INT32, &tangent_tag))
                    tangent_tag = JE_ANIMATION_TANGENT_AUTO;
                temp_key.m_tangent_mode = (uint8_t)tangent_tag;

                if (!_read_cache_data_value(&key, &temp_key.m_value))
                {
                    debug::logerr(
                        "Animation '%s' action '%s' track %zu key %zu has invalid value.",
                        path, action_name, track_index, key_index);
                    return false;
                }

                if (!value_type_established)
                {
                    value_type_established = true;
                    track_value_type = temp_key.m_value.m_type;
                }
                else if (track_value_type != temp_key.m_value.m_type)
                {
                    debug::logerr(
                        "Animation '%s' action '%s' track %zu key %zu has a different value type from previous keys.",
                        path, action_name, track_index, key_index);
                    return false;
                }

                if (!_interp_mode_allowed(
                    temp_key.m_value.m_type, temp_key.m_interp))
                {
                    debug::logerr(
                        "Animation '%s' action '%s' track %zu key %zu uses an interp mode not allowed for its value type.",
                        path, action_name, track_index, key_index);
                    return false;
                }

                if (temp_key.m_interp == JE_ANIMATION_INTERP_HERMITE)
                {
                    // 生成器已把 AUTO/LINEAR/FREE 切线烘焙为导数值
                    const size_t tangent_count =
                        _value_channels(temp_key.m_value.m_type) * 2;

                    je_cache_binary_list_t cache_tangents;
                    if (!je_cache_binary_document_index(
                        &key, "tangents", JE_CACHE_BINARY_VALUE_LIST,
                        &cache_tangents))
                    {
                        debug::logerr(
                            "Animation '%s' action '%s' track %zu key %zu is hermite but has no tangents.",
                            path, action_name, track_index, key_index);
                        return false;
                    }

                    temp_key.m_tangents.resize(tangent_count);
                    if (!_read_cache_floats(
                        &cache_tangents,
                        temp_key.m_tangents.data(),
                        tangent_count))
                    {
                        debug::logerr(
                            "Animation '%s' action '%s' track %zu key %zu has invalid tangents.",
                            path, action_name, track_index, key_index);
                        return false;
                    }
                }

                out_track->m_keys.push_back(move(temp_key));
            }

            // 关键帧按时间升序排列，保证采样时可以定位区段
            stable_sort(
                out_track->m_keys.begin(),
                out_track->m_keys.end(),
                [](const KeyData& a, const KeyData& b)
                { return a.m_time < b.m_time; });

            return true;
        }

        bool _load_animation_actions(
            const char* path, vector<ActionData>* out_actions)
        {
            je_file_t* cache_file = je_animation_cache_open_or_create(path);
            if (cache_file == nullptr)
            {
                debug::logerr(
                    "Failed to load animation '%s'.", path);
                return false;
            }

            je_cache_binary_document_t root;
            if (!je_cache_binary_document_open(cache_file, &root))
            {
                debug::logerr(
                    "Invalid animation cache document in '%s'.", path);
                je_file_close(cache_file);
                return false;
            }

            je_cache_binary_list_t cache_actions;
            if (!je_cache_binary_document_index(
                &root, "actions", JE_CACHE_BINARY_VALUE_LIST, &cache_actions))
            {
                debug::logerr(
                    "Animation cache '%s' has no 'actions' list.", path);
                je_file_close(cache_file);
                return false;
            }

            for (size_t action_index = 0;; ++action_index)
            {
                je_cache_binary_document_t action;
                if (!je_cache_binary_list_index(
                    &cache_actions,
                    action_index,
                    JE_CACHE_BINARY_VALUE_DOCUMENT,
                    &action))
                    break;

                ActionData temp_action;
                if (!_read_cache_string(&action, "name", &temp_action.m_name))
                {
                    debug::logerr(
                        "Animation '%s' action %zu has no name.",
                        path, action_index);
                    continue;
                }

                int32_t loop_tag;
                if (!je_cache_binary_document_index(
                    &action, "loop", JE_CACHE_BINARY_VALUE_INT32, &loop_tag))
                {
                    debug::logerr(
                        "Animation '%s' action '%s' has no loop mode.",
                        path, temp_action.m_name.c_str());
                    continue;
                }
                temp_action.m_loop_mode = (uint8_t)loop_tag;

                if (!je_cache_binary_document_index(
                    &action, "duration", JE_CACHE_BINARY_VALUE_FLOAT32,
                    &temp_action.m_duration))
                {
                    debug::logerr(
                        "Animation '%s' action '%s' has no duration.",
                        path, temp_action.m_name.c_str());
                    continue;
                }

                je_cache_binary_list_t cache_tracks;
                if (je_cache_binary_document_index(
                    &action, "tracks", JE_CACHE_BINARY_VALUE_LIST, &cache_tracks))
                {
                    for (size_t track_index = 0;; ++track_index)
                    {
                        je_cache_binary_document_t track;
                        if (!je_cache_binary_list_index(
                            &cache_tracks,
                            track_index,
                            JE_CACHE_BINARY_VALUE_DOCUMENT,
                            &track))
                            break;

                        TrackData temp_track;
                        int32_t kind_tag;
                        if (!je_cache_binary_document_index(
                            &track, "kind", JE_CACHE_BINARY_VALUE_INT32, &kind_tag))
                        {
                            debug::logerr(
                                "Animation '%s' action '%s' track %zu has no kind.",
                                path, temp_action.m_name.c_str(), track_index);
                            continue;
                        }
                        temp_track.m_kind = (uint8_t)kind_tag;

                        if (!je_cache_binary_document_index(
                            &track, "offset", JE_CACHE_BINARY_VALUE_BOOL,
                            &temp_track.m_offset_mode))
                        {
                            debug::logerr(
                                "Animation '%s' action '%s' track %zu has no offset mode.",
                                path, temp_action.m_name.c_str(), track_index);
                            continue;
                        }

                        je_cache_binary_list_t cache_keys;
                        if (!je_cache_binary_document_index(
                            &track, "keys", JE_CACHE_BINARY_VALUE_LIST, &cache_keys))
                        {
                            debug::logerr(
                                "Animation '%s' action '%s' track %zu has no keys.",
                                path, temp_action.m_name.c_str(), track_index);
                            continue;
                        }

                        if (temp_track.m_kind == JE_ANIMATION_TRACK_MEMBER)
                        {
                            string component_name, member_name;
                            if (!_read_cache_string(&track, "component", &component_name)
                                || !_read_cache_string(&track, "member", &member_name))
                            {
                                debug::logerr(
                                    "Animation '%s' action '%s' member track %zu has no component/member.",
                                    path, temp_action.m_name.c_str(), track_index);
                                continue;
                            }

                            const je_type_info_t* component_type =
                                typing::of(component_name.c_str());
                            if (component_type == nullptr)
                            {
                                debug::logerr(
                                    "Failed to find component type named '%s' when reading animation '%s' action '%s' track %zu.",
                                    component_name.c_str(), path,
                                    temp_action.m_name.c_str(), track_index);
                                continue;
                            }

                            const je_member_info_t* member_info =
                                typing::find_member_by_name(
                                    component_type, member_name.c_str());
                            if (member_info == nullptr)
                            {
                                debug::logerr(
                                    "Component '%s' has no member named '%s' when reading animation '%s' action '%s' track %zu.",
                                    component_name.c_str(), member_name.c_str(), path,
                                    temp_action.m_name.c_str(), track_index);
                                continue;
                            }

                            temp_track.m_component_type = component_type;
                            temp_track.m_member_info = member_info;
                        }
                        else if (temp_track.m_kind == JE_ANIMATION_TRACK_UNIFORM)
                        {
                            if (!_read_cache_string(
                                &track, "uniform", &temp_track.m_uniform_name))
                            {
                                debug::logerr(
                                    "Animation '%s' action '%s' uniform track %zu has no uniform name.",
                                    path, temp_action.m_name.c_str(), track_index);
                                continue;
                            }
                        }
                        else
                        {
                            debug::logerr(
                                "Animation '%s' action '%s' track %zu has unknown kind %d.",
                                path, temp_action.m_name.c_str(), track_index,
                                (int)kind_tag);
                            continue;
                        }

                        if (!_load_track_keys(
                            path,
                            temp_action.m_name.c_str(),
                            track_index,
                            &cache_keys,
                            &temp_track))
                            continue;

                        // 成员轨道：值类型必须与成员的实际类型匹配
                        if (temp_track.m_kind == JE_ANIMATION_TRACK_MEMBER
                            && !temp_track.m_keys.empty()
                            && !_animation_member_type_matches(
                                temp_track.m_keys.front().m_value.m_type,
                                temp_track.m_member_info->m_member_type))
                        {
                            debug::logerr(
                                "Member '%s.%s' has type '%s', which does not match the animation value type, when reading animation '%s' action '%s' track %zu.",
                                temp_track.m_component_type->m_typename,
                                temp_track.m_member_info->m_member_name,
                                temp_track.m_member_info->m_member_type->m_typename, path,
                                temp_action.m_name.c_str(), track_index);
                            continue;
                        }

                        temp_action.m_tracks.push_back(move(temp_track));
                    }
                }

                je_cache_binary_list_t cache_events;
                if (je_cache_binary_document_index(
                    &action, "events", JE_CACHE_BINARY_VALUE_LIST, &cache_events))
                {
                    for (size_t i = 0;; ++i)
                    {
                        je_cache_binary_document_t event;
                        if (!je_cache_binary_list_index(
                            &cache_events,
                            i,
                            JE_CACHE_BINARY_VALUE_DOCUMENT,
                            &event))
                            break;

                        EventData temp_event;
                        if (!je_cache_binary_document_index(
                            &event, "time", JE_CACHE_BINARY_VALUE_FLOAT32,
                            &temp_event.m_time)
                            || !_read_cache_string(&event, "name", &temp_event.m_name))
                        {
                            debug::logerr(
                                "Animation '%s' action '%s' event %zu is invalid.",
                                path, temp_action.m_name.c_str(), i);
                            continue;
                        }

                        temp_action.m_events.push_back(move(temp_event));
                    }
                }

                out_actions->push_back(move(temp_action));
            }

            je_file_close(cache_file);
            return true;
        }
    }
}

/* OPTIONAL */ je_animation_t* je_animation_open(const char* path)
{
    assert(path != nullptr);

    // 空路径返回空动画：加载失败的组件用它占位，保证组件状态可用。
    if (path[0] == '\0')
        return jeecs::animation::_build_animation(
            vector<jeecs::animation::ActionData>{});

    vector<jeecs::animation::ActionData> actions;
    if (!jeecs::animation::_load_animation_actions(path, &actions))
        return nullptr;

    return jeecs::animation::_build_animation(actions);
}

void je_animation_close(je_animation_t* animation)
{
    assert(animation != nullptr);

    for (size_t i = 0; i < animation->m_action_count; ++i)
    {
        je_animation_action_data_t& action = animation->m_actions[i];

        for (size_t j = 0; j < action.m_track_count; ++j)
        {
            je_animation_track_t& track = action.m_tracks[j];

            for (size_t k = 0; k < track.m_key_count; ++k)
                delete[] track.m_keys[k].m_tangents;

            delete[] track.m_keys;
            delete[] track.m_uniform_name;
        }
        delete[] action.m_tracks;

        for (size_t j = 0; j < action.m_event_count; ++j)
            delete[] action.m_events[j].m_name;
        delete[] action.m_events;

        delete[] action.m_name;
    }

    delete[] animation->m_actions;
    delete animation;
}

/* OPTIONAL */ const je_animation_action_data_t* je_animation_action_find(
    const je_animation_t* animation, const char* name)
{
    assert(animation != nullptr);
    assert(name != nullptr);

    for (size_t i = 0; i < animation->m_action_count; ++i)
    {
        if (0 == strcmp(animation->m_actions[i].m_name, name))
            return &animation->m_actions[i];
    }
    return nullptr;
}

WOORT_API woort_api wojeapi_animation_cache_ensure(void)
{
    const string path = woort_string(0);

    je_file_t* cache = je_animation_cache_open_or_create(path.c_str());
    if (nullptr != cache)
    {
        je_file_close(cache);
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}
