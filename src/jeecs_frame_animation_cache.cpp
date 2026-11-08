#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_cache_version.hpp"

#include "wo.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace jeecs::frame_animation {}

using namespace std;
using namespace jeecs;
using namespace jeecs::frame_animation;

namespace jeecs::frame_animation
{
    namespace
    {
        // 帧动画不再直接读取 .je4animation 源文件，而是读取由
        // builtin/script/cache_generator/animation.wo 生成的二进制缓存：
        //   <动画源路径>.je4cache
        // 缓存文件头由 je_cache_file_create 写入（格式版本 + 源文件 CRC64；
        // 管理器版本留空占位，由 je_cache_file_close(..., true) 回写后缓存才
        // 生效），其后是一个 jeecs_cache_binary_* 文档。文档结构：
        //   DOCUMENT
        //     "animations" -> LIST of DOCUMENT(action)
        //         "name" -> STRING
        //         "frames" -> LIST of DOCUMENT(frame)
        //             "time" -> FLOAT32
        //             "components" -> LIST of DOCUMENT
        //                 "name" -> STRING        // 组件类型名
        //                 "member" -> STRING
        //                 "offset" -> BOOL
        //                 "type" -> INT32         // je_animation_value_type_t
        //                 "value" -> INT32 | FLOAT32 | LIST of FLOAT32
        //             "uniforms" -> LIST of DOCUMENT
        //                 "name" -> STRING
        //                 "type" -> INT32
        //                 "value" -> 同上
        //   END
        // QUAT4 的 value 是四元数 (x, y, z, w)，由生成器从 TOML 中的欧拉角换算。
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

        struct ComponentData
        {
            const je_type_info_t* m_component_type;
            const je_member_info_t* m_member_info;
            je_animation_value_t m_member_value;
            bool m_offset_mode;
        };
        struct UniformData
        {
            string m_uniform_name;
            je_animation_value_t m_uniform_value;
        };
        struct FrameData
        {
            float m_frame_time = 0.0f;
            vector<ComponentData> m_component_data;
            vector<UniformData> m_uniform_data;
        };
        struct ActionData
        {
            string m_name;
            vector<FrameData> m_frames;
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
                dst_action.m_frames = src_action.m_frames.empty()
                    ? nullptr
                    : new je_animation_frame_data_t[src_action.m_frames.size()];
                dst_action.m_frame_count = src_action.m_frames.size();

                for (size_t j = 0; j < src_action.m_frames.size(); ++j)
                {
                    const FrameData& src_frame = src_action.m_frames[j];
                    je_animation_frame_data_t& dst_frame = dst_action.m_frames[j];

                    dst_frame.m_frame_time = src_frame.m_frame_time;
                    dst_frame.m_component_data = src_frame.m_component_data.empty()
                        ? nullptr
                        : new je_animation_component_data_t[src_frame.m_component_data.size()];
                    dst_frame.m_component_data_count = src_frame.m_component_data.size();

                    for (size_t k = 0; k < src_frame.m_component_data.size(); ++k)
                    {
                        const ComponentData& src = src_frame.m_component_data[k];
                        je_animation_component_data_t& dst = dst_frame.m_component_data[k];

                        dst.m_component_type = src.m_component_type;
                        dst.m_member_info = src.m_member_info;
                        dst.m_member_value = src.m_member_value;
                        dst.m_offset_mode = src.m_offset_mode;
                    }

                    dst_frame.m_uniform_data = src_frame.m_uniform_data.empty()
                        ? nullptr
                        : new je_animation_uniform_data_t[src_frame.m_uniform_data.size()];
                    dst_frame.m_uniform_data_count = src_frame.m_uniform_data.size();

                    for (size_t k = 0; k < src_frame.m_uniform_data.size(); ++k)
                    {
                        const UniformData& src = src_frame.m_uniform_data[k];
                        je_animation_uniform_data_t& dst = dst_frame.m_uniform_data[k];

                        dst.m_uniform_name = _copy_c_string(src.m_uniform_name);
                        dst.m_uniform_value = src.m_uniform_value;
                    }
                }
            }

            return result;
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

            je_cache_binary_list_t cache_animations;
            if (!je_cache_binary_document_index(
                &root, "animations", JE_CACHE_BINARY_VALUE_LIST, &cache_animations))
            {
                debug::logerr(
                    "Animation cache '%s' has no 'animations' list.", path);
                je_file_close(cache_file);
                return false;
            }

            for (size_t action_index = 0;; ++action_index)
            {
                je_cache_binary_document_t action;
                if (!je_cache_binary_list_index(
                    &cache_animations,
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

                je_cache_binary_list_t cache_frames;
                if (!je_cache_binary_document_index(
                    &action, "frames", JE_CACHE_BINARY_VALUE_LIST, &cache_frames))
                {
                    debug::logerr(
                        "Animation '%s' action '%s' has no frames.",
                        path, temp_action.m_name.c_str());
                    continue;
                }

                for (size_t frame_index = 0;; ++frame_index)
                {
                    je_cache_binary_document_t frame;
                    if (!je_cache_binary_list_index(
                        &cache_frames,
                        frame_index,
                        JE_CACHE_BINARY_VALUE_DOCUMENT,
                        &frame))
                        break;

                    FrameData temp_frame;
                    if (!je_cache_binary_document_index(
                        &frame, "time", JE_CACHE_BINARY_VALUE_FLOAT32,
                        &temp_frame.m_frame_time))
                    {
                        debug::logerr(
                            "Animation '%s' action '%s' frame %zu has no time.",
                            path, temp_action.m_name.c_str(), frame_index);
                        continue;
                    }

                    je_cache_binary_list_t cache_components;
                    if (je_cache_binary_document_index(
                        &frame, "components", JE_CACHE_BINARY_VALUE_LIST,
                        &cache_components))
                    {
                        for (size_t i = 0;; ++i)
                        {
                            je_cache_binary_document_t component;
                            if (!je_cache_binary_list_index(
                                &cache_components,
                                i,
                                JE_CACHE_BINARY_VALUE_DOCUMENT,
                                &component))
                                break;

                            string component_name, member_name;
                            if (!_read_cache_string(&component, "name", &component_name)
                                || !_read_cache_string(&component, "member", &member_name))
                            {
                                debug::logerr(
                                    "Animation '%s' action '%s' frame %zu component %zu is invalid.",
                                    path, temp_action.m_name.c_str(), frame_index, i);
                                continue;
                            }

                            bool offset_mode = false;
                            if (!je_cache_binary_document_index(
                                &component, "offset", JE_CACHE_BINARY_VALUE_BOOL,
                                &offset_mode))
                            {
                                debug::logerr(
                                    "Animation '%s' action '%s' frame %zu component '%s.%s' has no offset mode.",
                                    path, temp_action.m_name.c_str(), frame_index,
                                    component_name.c_str(), member_name.c_str());
                                continue;
                            }

                            je_animation_value_t value;
                            if (!_read_cache_data_value(&component, &value))
                            {
                                debug::logerr(
                                    "Animation '%s' action '%s' frame %zu component '%s.%s' has invalid value.",
                                    path, temp_action.m_name.c_str(), frame_index,
                                    component_name.c_str(), member_name.c_str());
                                continue;
                            }

                            const je_type_info_t* component_type =
                                typing::of(component_name.c_str());
                            if (component_type == nullptr)
                            {
                                debug::logerr(
                                    "Failed to find component type named '%s' when reading animation '%s' action '%s' frame %zu.",
                                    component_name.c_str(), path,
                                    temp_action.m_name.c_str(), frame_index);
                                continue;
                            }

                            const je_member_info_t* member_info =
                                typing::find_member_by_name(
                                    component_type, member_name.c_str());
                            if (member_info == nullptr)
                            {
                                debug::logerr(
                                    "Component '%s' has no member named '%s' when reading animation '%s' action '%s' frame %zu.",
                                    component_name.c_str(), member_name.c_str(), path,
                                    temp_action.m_name.c_str(), frame_index);
                                continue;
                            }

                            if (!_animation_member_type_matches(
                                value.m_type, member_info->m_member_type))
                            {
                                debug::logerr(
                                    "Member '%s.%s' has type '%s', which does not match the animation value type, when reading animation '%s' action '%s' frame %zu.",
                                    component_name.c_str(), member_name.c_str(),
                                    member_info->m_member_type->m_typename, path,
                                    temp_action.m_name.c_str(), frame_index);
                                continue;
                            }

                            ComponentData cdata;
                            cdata.m_component_type = component_type;
                            cdata.m_member_info = member_info;
                            cdata.m_member_value = value;
                            cdata.m_offset_mode = offset_mode;
                            temp_frame.m_component_data.push_back(cdata);
                        }
                    }

                    je_cache_binary_list_t cache_uniforms;
                    if (je_cache_binary_document_index(
                        &frame, "uniforms", JE_CACHE_BINARY_VALUE_LIST,
                        &cache_uniforms))
                    {
                        for (size_t i = 0;; ++i)
                        {
                            je_cache_binary_document_t uniform;
                            if (!je_cache_binary_list_index(
                                &cache_uniforms,
                                i,
                                JE_CACHE_BINARY_VALUE_DOCUMENT,
                                &uniform))
                                break;

                            UniformData udata;
                            if (!_read_cache_string(
                                &uniform, "name", &udata.m_uniform_name))
                            {
                                debug::logerr(
                                    "Animation '%s' action '%s' frame %zu uniform %zu has no name.",
                                    path, temp_action.m_name.c_str(), frame_index, i);
                                continue;
                            }

                            if (!_read_cache_data_value(
                                &uniform, &udata.m_uniform_value))
                            {
                                debug::logerr(
                                    "Animation '%s' action '%s' frame %zu uniform '%s' has invalid value.",
                                    path, temp_action.m_name.c_str(), frame_index,
                                    udata.m_uniform_name.c_str());
                                continue;
                            }

                            temp_frame.m_uniform_data.push_back(move(udata));
                        }
                    }

                    temp_action.m_frames.push_back(move(temp_frame));
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

    // 空路径返回空动画：加载失败的条目用它占位，保持动画条目下标对齐。
    if (path[0] == '\0')
        return _build_animation(vector<ActionData>{});

    vector<ActionData> actions;
    if (!_load_animation_actions(path, &actions))
        return nullptr;

    return _build_animation(actions);
}

void je_animation_close(je_animation_t* animation)
{
    assert(animation != nullptr);

    for (size_t i = 0; i < animation->m_action_count; ++i)
    {
        je_animation_action_data_t& action = animation->m_actions[i];

        for (size_t j = 0; j < action.m_frame_count; ++j)
        {
            je_animation_frame_data_t& frame = action.m_frames[j];

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
