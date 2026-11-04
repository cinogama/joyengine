#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_cache_version.hpp"
#include "jeecs_cache_generator.hpp"

#include "wo.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// 帧动画不再直接读取 .je4animation 源文件，而是读取由
// builtin/script/cache_generator/animation.wo 生成的二进制缓存：
//   <动画源路径>.je4cache
// 缓存文件头由 jeecs_create_cache_file 写入（管理器版本 + 格式版本 +
// 源文件 CRC64），其后是一个 jeecs_cache_binary_* 文档。文档结构：
//   DOCUMENT
//     "animations" -> LIST of DOCUMENT(action)
//         "name" -> STRING
//         "frames" -> LIST of DOCUMENT(frame)
//             "time" -> FLOAT32
//             "components" -> LIST of DOCUMENT
//                 "name" -> STRING        // 组件类型名
//                 "member" -> STRING
//                 "offset" -> BOOL
//                 "type" -> INT32         // je_animation_value_type
//                 "value" -> INT32 | FLOAT32 | LIST of FLOAT32
//             "uniforms" -> LIST of DOCUMENT
//                 "name" -> STRING
//                 "type" -> INT32
//                 "value" -> 同上
//   END
// QUAT4 的 value 是四元数 (x, y, z, w)，由生成器从 TOML 中的欧拉角换算。
//
// 解析结果写入纯 C 的 je_animation 结构（连续数组，无 C++ 容器），
// 由 je_animation_close 统一释放；组件成员的类型校验在加载时完成。

namespace
{
    constexpr const char* ANIMATION_CACHE_GENERATOR_PATH =
        "!/builtin/script/cache_generator/animation.wo";
    constexpr const char* ANIMATION_CACHE_GENERATOR_ENTRY = "generate";

    bool _read_cache_string(
        const jeecs_cache_binary_document* document,
        const char* key,
        std::string* out)
    {
        jeecs_cache_binary_string str;
        if (!jeecs_cache_binary_document_index(
            document, key, JE_CACHE_BINARY_VALUE_STRING, &str))
            return false;

        size_t len = 0;
        if (!jeecs_cache_binary_string_read(nullptr, &len, &str))
            return false;

        out->resize(len);
        if (len > 0)
        {
            size_t capacity = len;
            if (!jeecs_cache_binary_string_read(out->data(), &capacity, &str))
                return false;
        }
        return true;
    }

    bool _read_cache_floats(
        const jeecs_cache_binary_list* list, float* out, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (!jeecs_cache_binary_list_index(
                list, i, JE_CACHE_BINARY_VALUE_FLOAT32, out + i))
                return false;
        }
        return true;
    }

    bool _read_cache_data_value(
        const jeecs_cache_binary_document* document,
        je_animation_value* out)
    {
        int32_t type_tag;
        if (!jeecs_cache_binary_document_index(
            document, "type", JE_CACHE_BINARY_VALUE_INT32, &type_tag))
            return false;

        switch (type_tag)
        {
        case JE_ANIMATION_VALUE_INT:
        {
            int32_t value;
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_INT32, &value))
                return false;

            out->m_type = (uint8_t)JE_ANIMATION_VALUE_INT;
            out->m_value.i32 = value;
            return true;
        }

        case JE_ANIMATION_VALUE_FLOAT:
        {
            float value;
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_FLOAT32, &value))
                return false;

            out->m_type = (uint8_t)JE_ANIMATION_VALUE_FLOAT;
            out->m_value.f32 = value;
            return true;
        }

        case JE_ANIMATION_VALUE_VEC2:
        {
            jeecs_cache_binary_list values;
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                || !_read_cache_floats(&values, out->m_value.v2, 2))
                return false;

            out->m_type = (uint8_t)JE_ANIMATION_VALUE_VEC2;
            return true;
        }

        case JE_ANIMATION_VALUE_VEC3:
        {
            jeecs_cache_binary_list values;
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                || !_read_cache_floats(&values, out->m_value.v3, 3))
                return false;

            out->m_type = (uint8_t)JE_ANIMATION_VALUE_VEC3;
            return true;
        }

        case JE_ANIMATION_VALUE_VEC4:
        {
            jeecs_cache_binary_list values;
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                || !_read_cache_floats(&values, out->m_value.v4, 4))
                return false;

            out->m_type = (uint8_t)JE_ANIMATION_VALUE_VEC4;
            return true;
        }

        case JE_ANIMATION_VALUE_QUAT4:
        {
            jeecs_cache_binary_list values;
            if (!jeecs_cache_binary_document_index(
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

    // 调用生成器脚本，把 TOML 源转换为引擎可读的 .je4cache。
    // 返回 true 表示缓存已写出并通过 jeecs_load_cache_file 校验。
    bool _generate_animation_cache(const char* filepath)
    {
        return jeecs::cache::generate_from_script(filepath,
            ANIMATION_CACHE_VERSION,
            ANIMATION_CACHE_GENERATOR_PATH,
            ANIMATION_CACHE_GENERATOR_ENTRY);
    }

    // 组件成员类型必须在加载时匹配，运行期不再逐帧校验。
    bool _animation_member_type_matches(
        uint8_t type, const je_TypeInfo* member_type)
    {
        switch (type)
        {
        case JE_ANIMATION_VALUE_INT:
            return member_type == jeecs::typing::of<int>();
        case JE_ANIMATION_VALUE_FLOAT:
            return member_type == jeecs::typing::of<float>();
        case JE_ANIMATION_VALUE_VEC2:
            return member_type == jeecs::typing::of<jeecs::math::vec2>();
        case JE_ANIMATION_VALUE_VEC3:
            return member_type == jeecs::typing::of<jeecs::math::vec3>();
        case JE_ANIMATION_VALUE_VEC4:
            return member_type == jeecs::typing::of<jeecs::math::vec4>()
                || member_type == jeecs::typing::of<jeecs::math::color>();
        case JE_ANIMATION_VALUE_QUAT4:
            return member_type == jeecs::typing::of<jeecs::math::quat>();
        }
        return false;
    }

    struct _temp_frame_component
    {
        const je_TypeInfo* m_component_type;
        const je_MemberInfo* m_member_info;
        je_animation_value m_member_value;
        bool m_offset_mode;
    };
    struct _temp_frame_uniform
    {
        std::string m_uniform_name;
        je_animation_value m_uniform_value;
    };
    struct _temp_frame
    {
        float m_frame_time = 0.0f;
        std::vector<_temp_frame_component> m_component_data;
        std::vector<_temp_frame_uniform> m_uniform_data;
    };
    struct _temp_action
    {
        std::string m_name;
        std::vector<_temp_frame> m_frames;
    };

    char* _copy_c_string(const std::string& str)
    {
        char* result = new char[str.size() + 1];
        memcpy(result, str.c_str(), str.size());
        result[str.size()] = '\0';
        return result;
    }

    je_animation* _build_animation(const std::vector<_temp_action>& actions)
    {
        auto* result = new je_animation;
        result->m_actions = actions.empty()
            ? nullptr
            : new je_animation_action_data[actions.size()];
        result->m_action_count = actions.size();

        for (size_t i = 0; i < actions.size(); ++i)
        {
            const _temp_action& src_action = actions[i];
            je_animation_action_data& dst_action = result->m_actions[i];

            dst_action.m_name = _copy_c_string(src_action.m_name);
            dst_action.m_frames = src_action.m_frames.empty()
                ? nullptr
                : new je_animation_frame_data[src_action.m_frames.size()];
            dst_action.m_frame_count = src_action.m_frames.size();

            for (size_t j = 0; j < src_action.m_frames.size(); ++j)
            {
                const _temp_frame& src_frame = src_action.m_frames[j];
                je_animation_frame_data& dst_frame = dst_action.m_frames[j];

                dst_frame.m_frame_time = src_frame.m_frame_time;
                dst_frame.m_component_data = src_frame.m_component_data.empty()
                    ? nullptr
                    : new je_animation_component_data[src_frame.m_component_data.size()];
                dst_frame.m_component_data_count = src_frame.m_component_data.size();

                for (size_t k = 0; k < src_frame.m_component_data.size(); ++k)
                {
                    const _temp_frame_component& src = src_frame.m_component_data[k];
                    je_animation_component_data& dst = dst_frame.m_component_data[k];

                    dst.m_component_type = src.m_component_type;
                    dst.m_member_info = src.m_member_info;
                    dst.m_member_value = src.m_member_value;
                    dst.m_offset_mode = src.m_offset_mode;
                }

                dst_frame.m_uniform_data = src_frame.m_uniform_data.empty()
                    ? nullptr
                    : new je_animation_uniform_data[src_frame.m_uniform_data.size()];
                dst_frame.m_uniform_data_count = src_frame.m_uniform_data.size();

                for (size_t k = 0; k < src_frame.m_uniform_data.size(); ++k)
                {
                    const _temp_frame_uniform& src = src_frame.m_uniform_data[k];
                    je_animation_uniform_data& dst = dst_frame.m_uniform_data[k];

                    dst.m_uniform_name = _copy_c_string(src.m_uniform_name);
                    dst.m_uniform_value = src.m_uniform_value;
                }
            }
        }

        return result;
    }

    bool _load_animation_actions(
        const char* path, std::vector<_temp_action>* out_actions)
    {
        jeecs_file* cache_file =
            jeecs_load_cache_file(path, ANIMATION_CACHE_VERSION, 0);
        if (cache_file == nullptr)
        {
            if (!_generate_animation_cache(path))
            {
                jeecs::debug::logerr(
                    "Failed to load animation '%s': cannot generate the binary cache.",
                    path);
                return false;
            }

            cache_file =
                jeecs_load_cache_file(path, ANIMATION_CACHE_VERSION, 0);
            if (cache_file == nullptr)
            {
                jeecs::debug::logerr(
                    "Failed to load animation '%s': generated cache is invalid.",
                    path);
                return false;
            }
        }

        if (!jeecs_file_seek(
            cache_file, (int64_t)jeecs::cache::CACHE_FILE_HEADER_SIZE, JE_READ_FILE_SEEK_SET))
        {
            jeecs::debug::logerr(
                "Failed to seek animation cache for '%s'.", path);
            jeecs_file_close(cache_file);
            return false;
        }

        jeecs_cache_binary_document root;
        if (!jeecs_cache_binary_document_open(cache_file, &root))
        {
            jeecs::debug::logerr(
                "Invalid animation cache document in '%s'.", path);
            jeecs_file_close(cache_file);
            return false;
        }

        jeecs_cache_binary_list cache_animations;
        if (!jeecs_cache_binary_document_index(
            &root, "animations", JE_CACHE_BINARY_VALUE_LIST, &cache_animations))
        {
            jeecs::debug::logerr(
                "Animation cache '%s' has no 'animations' list.", path);
            jeecs_file_close(cache_file);
            return false;
        }

        for (size_t action_index = 0;; ++action_index)
        {
            jeecs_cache_binary_document action;
            if (!jeecs_cache_binary_list_index(
                &cache_animations,
                action_index,
                JE_CACHE_BINARY_VALUE_DOCUMENT,
                &action))
                break;

            _temp_action temp_action;
            if (!_read_cache_string(&action, "name", &temp_action.m_name))
            {
                jeecs::debug::logerr(
                    "Animation '%s' action %zu has no name.",
                    path, action_index);
                continue;
            }

            jeecs_cache_binary_list cache_frames;
            if (!jeecs_cache_binary_document_index(
                &action, "frames", JE_CACHE_BINARY_VALUE_LIST, &cache_frames))
            {
                jeecs::debug::logerr(
                    "Animation '%s' action '%s' has no frames.",
                    path, temp_action.m_name.c_str());
                continue;
            }

            for (size_t frame_index = 0;; ++frame_index)
            {
                jeecs_cache_binary_document frame;
                if (!jeecs_cache_binary_list_index(
                    &cache_frames,
                    frame_index,
                    JE_CACHE_BINARY_VALUE_DOCUMENT,
                    &frame))
                    break;

                _temp_frame temp_frame;
                if (!jeecs_cache_binary_document_index(
                    &frame, "time", JE_CACHE_BINARY_VALUE_FLOAT32,
                    &temp_frame.m_frame_time))
                {
                    jeecs::debug::logerr(
                        "Animation '%s' action '%s' frame %zu has no time.",
                        path, temp_action.m_name.c_str(), frame_index);
                    continue;
                }

                jeecs_cache_binary_list cache_components;
                if (jeecs_cache_binary_document_index(
                    &frame, "components", JE_CACHE_BINARY_VALUE_LIST,
                    &cache_components))
                {
                    for (size_t i = 0;; ++i)
                    {
                        jeecs_cache_binary_document component;
                        if (!jeecs_cache_binary_list_index(
                            &cache_components,
                            i,
                            JE_CACHE_BINARY_VALUE_DOCUMENT,
                            &component))
                            break;

                        std::string component_name, member_name;
                        if (!_read_cache_string(&component, "name", &component_name)
                            || !_read_cache_string(&component, "member", &member_name))
                        {
                            jeecs::debug::logerr(
                                "Animation '%s' action '%s' frame %zu component %zu is invalid.",
                                path, temp_action.m_name.c_str(), frame_index, i);
                            continue;
                        }

                        bool offset_mode = false;
                        if (!jeecs_cache_binary_document_index(
                            &component, "offset", JE_CACHE_BINARY_VALUE_BOOL,
                            &offset_mode))
                        {
                            jeecs::debug::logerr(
                                "Animation '%s' action '%s' frame %zu component '%s.%s' has no offset mode.",
                                path, temp_action.m_name.c_str(), frame_index,
                                component_name.c_str(), member_name.c_str());
                            continue;
                        }

                        je_animation_value value;
                        if (!_read_cache_data_value(&component, &value))
                        {
                            jeecs::debug::logerr(
                                "Animation '%s' action '%s' frame %zu component '%s.%s' has invalid value.",
                                path, temp_action.m_name.c_str(), frame_index,
                                component_name.c_str(), member_name.c_str());
                            continue;
                        }

                        const je_TypeInfo* component_type =
                            jeecs::typing::of(component_name.c_str());
                        if (component_type == nullptr)
                        {
                            jeecs::debug::logerr(
                                "Failed to find component type named '%s' when reading animation '%s' action '%s' frame %zu.",
                                component_name.c_str(), path,
                                temp_action.m_name.c_str(), frame_index);
                            continue;
                        }

                        const je_MemberInfo* member_info =
                            jeecs::typing::find_member_by_name(
                                component_type, member_name.c_str());
                        if (member_info == nullptr)
                        {
                            jeecs::debug::logerr(
                                "Component '%s' has no member named '%s' when reading animation '%s' action '%s' frame %zu.",
                                component_name.c_str(), member_name.c_str(), path,
                                temp_action.m_name.c_str(), frame_index);
                            continue;
                        }

                        if (!_animation_member_type_matches(
                            value.m_type, member_info->m_member_type))
                        {
                            jeecs::debug::logerr(
                                "Member '%s.%s' has type '%s', which does not match the animation value type, when reading animation '%s' action '%s' frame %zu.",
                                component_name.c_str(), member_name.c_str(),
                                member_info->m_member_type->m_typename, path,
                                temp_action.m_name.c_str(), frame_index);
                            continue;
                        }

                        _temp_frame_component cdata;
                        cdata.m_component_type = component_type;
                        cdata.m_member_info = member_info;
                        cdata.m_member_value = value;
                        cdata.m_offset_mode = offset_mode;
                        temp_frame.m_component_data.push_back(cdata);
                    }
                }

                jeecs_cache_binary_list cache_uniforms;
                if (jeecs_cache_binary_document_index(
                    &frame, "uniforms", JE_CACHE_BINARY_VALUE_LIST,
                    &cache_uniforms))
                {
                    for (size_t i = 0;; ++i)
                    {
                        jeecs_cache_binary_document uniform;
                        if (!jeecs_cache_binary_list_index(
                            &cache_uniforms,
                            i,
                            JE_CACHE_BINARY_VALUE_DOCUMENT,
                            &uniform))
                            break;

                        _temp_frame_uniform udata;
                        if (!_read_cache_string(
                            &uniform, "name", &udata.m_uniform_name))
                        {
                            jeecs::debug::logerr(
                                "Animation '%s' action '%s' frame %zu uniform %zu has no name.",
                                path, temp_action.m_name.c_str(), frame_index, i);
                            continue;
                        }

                        if (!_read_cache_data_value(
                            &uniform, &udata.m_uniform_value))
                        {
                            jeecs::debug::logerr(
                                "Animation '%s' action '%s' frame %zu uniform '%s' has invalid value.",
                                path, temp_action.m_name.c_str(), frame_index,
                                udata.m_uniform_name.c_str());
                            continue;
                        }

                        temp_frame.m_uniform_data.push_back(std::move(udata));
                    }
                }

                temp_action.m_frames.push_back(std::move(temp_frame));
            }

            out_actions->push_back(std::move(temp_action));
        }

        jeecs_file_close(cache_file);
        return true;
    }
}

JE_API /* OPTIONAL */ je_animation* je_animation_open(const char* path)
{
    assert(path != nullptr);

    // 空路径返回空动画：加载失败的条目用它占位，保持动画条目下标对齐。
    if (path[0] == '\0')
        return _build_animation(std::vector<_temp_action>{});

    std::vector<_temp_action> actions;
    if (!_load_animation_actions(path, &actions))
        return nullptr;

    return _build_animation(actions);
}

JE_API void je_animation_close(je_animation* animation)
{
    assert(animation != nullptr);

    for (size_t i = 0; i < animation->m_action_count; ++i)
    {
        je_animation_action_data& action = animation->m_actions[i];

        for (size_t j = 0; j < action.m_frame_count; ++j)
        {
            je_animation_frame_data& frame = action.m_frames[j];

            for (size_t k = 0; k < frame.m_uniform_data_count; ++k)
                delete[] frame.m_uniform_data[k].m_uniform_name;

            delete[] frame.m_uniform_data;
            delete[] frame.m_component_data;
        }

        delete[] action.m_frames;
        delete[] action.m_name;
    }

    delete[] animation->m_actions;
    delete animation;
}

JE_API /* OPTIONAL */ const je_animation_action_data* je_animation_find_action(
    const je_animation* animation, const char* name)
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
    const std::string path = woort_string(0);

    if (jeecs_file* cache =
        jeecs_load_cache_file(path.c_str(), ANIMATION_CACHE_VERSION, 0))
    {
        jeecs_file_close(cache);
        return woort_ret_bool(true);
    }

    return woort_ret_bool(_generate_animation_cache(path.c_str()));
}
