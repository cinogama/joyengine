#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_cache_version.hpp"

#include "wo.h"

#include <cstdio>
#include <string>
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
//                 "type" -> INT32         // 对应 frame_data::data_value::type
//                 "value" -> INT32 | FLOAT32 | LIST of FLOAT32
//             "uniforms" -> LIST of DOCUMENT
//                 "name" -> STRING
//                 "type" -> INT32
//                 "value" -> 同上
//   END
// QUAT4 的 value 是四元数 (x, y, z, w)，由生成器从 TOML 中的欧拉角换算。

namespace
{
    using jeecs_frame_animation_data_value =
        jeecs::Animation::FrameAnimation::animation_list::frame_data::data_value;

    constexpr size_t CACHE_FILE_HEADER_SIZE =
        sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t);

    constexpr const char* ANIMATION_CACHE_GENERATOR_PATH =
        "!/builtin/script/cache_generator/animation.wo";
    constexpr const char* ANIMATION_CACHE_GENERATOR_ENTRY = "generate";

    // 将 @/! 前缀展开成脚本侧可直接访问的本地路径；普通路径原样返回。
    std::string _resolve_source_path(const char* filepath)
    {
        if (filepath[0] == '@')
        {
            std::string root = jeecs_file_get_runtime_path();
            if (root.empty())
                root = jeecs_file_get_host_path();

            const char* rest = filepath + 1;
            if (root.empty())
                return rest;

            const bool root_sep = root.back() == '/' || root.back() == '\\';
            const bool rest_sep = rest[0] == '/' || rest[0] == '\\';
            if (root_sep && rest_sep)
                return root + (rest + 1);
            if (!root_sep && !rest_sep)
                return root + "/" + rest;
            return root + rest;
        }

        if (filepath[0] == '!')
        {
            std::string root = jeecs_file_get_host_path();
            const char* rest = filepath + 1;
            if (root.empty())
                return rest;

            const bool root_sep = root.back() == '/' || root.back() == '\\';
            const bool rest_sep = rest[0] == '/' || rest[0] == '\\';
            if (root_sep && rest_sep)
                return root + (rest + 1);
            if (!root_sep && !rest_sep)
                return root + "/" + rest;
            return root + rest;
        }

        return filepath;
    }

    std::string _cache_file_path(const char* filepath)
    {
        return _resolve_source_path(filepath) + ".je4cache";
    }

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
        jeecs_frame_animation_data_value* out)
    {
        int32_t type_tag;
        if (!jeecs_cache_binary_document_index(
            document, "type", JE_CACHE_BINARY_VALUE_INT32, &type_tag))
            return false;

        switch (type_tag)
        {
        case jeecs_frame_animation_data_value::type::INT:
        {
            int32_t value;
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_INT32, &value))
                return false;

            out->m_type = jeecs_frame_animation_data_value::type::INT;
            out->m_value.i32 = value;
            return true;
        }

        case jeecs_frame_animation_data_value::type::FLOAT:
        {
            float value;
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_FLOAT32, &value))
                return false;

            out->m_type = jeecs_frame_animation_data_value::type::FLOAT;
            out->m_value.f32 = value;
            return true;
        }

        case jeecs_frame_animation_data_value::type::VEC2:
        {
            jeecs_cache_binary_list values;
            float value[2];
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                || !_read_cache_floats(&values, value, 2))
                return false;

            out->m_type = jeecs_frame_animation_data_value::type::VEC2;
            out->m_value.v2 = jeecs::math::vec2(value[0], value[1]);
            return true;
        }

        case jeecs_frame_animation_data_value::type::VEC3:
        {
            jeecs_cache_binary_list values;
            float value[3];
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                || !_read_cache_floats(&values, value, 3))
                return false;

            out->m_type = jeecs_frame_animation_data_value::type::VEC3;
            out->m_value.v3 = jeecs::math::vec3(value[0], value[1], value[2]);
            return true;
        }

        case jeecs_frame_animation_data_value::type::VEC4:
        {
            jeecs_cache_binary_list values;
            float value[4];
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                || !_read_cache_floats(&values, value, 4))
                return false;

            out->m_type = jeecs_frame_animation_data_value::type::VEC4;
            out->m_value.v4 = jeecs::math::vec4(value[0], value[1], value[2], value[3]);
            return true;
        }

        case jeecs_frame_animation_data_value::type::QUAT4:
        {
            jeecs_cache_binary_list values;
            float value[4];
            if (!jeecs_cache_binary_document_index(
                document, "value", JE_CACHE_BINARY_VALUE_LIST, &values)
                || !_read_cache_floats(&values, value, 4))
                return false;

            out->m_type = jeecs_frame_animation_data_value::type::QUAT4;
            out->m_value.q4.x = value[0];
            out->m_value.q4.y = value[1];
            out->m_value.q4.z = value[2];
            out->m_value.q4.w = value[3];
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
        jeecs_file* script_file = jeecs_file_open(ANIMATION_CACHE_GENERATOR_PATH);
        if (script_file == nullptr)
        {
            jeecs::debug::logerr(
                "Cannot open animation cache generator '%s'.",
                ANIMATION_CACHE_GENERATOR_PATH);
            return false;
        }

        const size_t script_size = jeecs_file_size(script_file);
        std::vector<char> script_source(script_size + 1);
        if (script_size > 0
            && script_size != jeecs_file_read(
                script_source.data(), sizeof(char), script_size, script_file))
        {
            jeecs_file_close(script_file);
            jeecs::debug::logerr(
                "Failed to read animation cache generator '%s'.",
                ANIMATION_CACHE_GENERATOR_PATH);
            return false;
        }
        script_source[script_size] = '\0';
        jeecs_file_close(script_file);

        wo_CompileErrors* cerror = nullptr;
        woort_CodeEnv* const cenv = wo_load_source(
            ANIMATION_CACHE_GENERATOR_PATH, script_source.data(), &cerror);
        if (cenv == nullptr)
        {
            jeecs::debug::logerr(
                "Failed to compile animation cache generator '%s':\n%s",
                ANIMATION_CACHE_GENERATOR_PATH,
                wo_get_compile_error(cerror, WO_PLAIM));

            wo_compile_errors_free(cerror);
            return false;
        }

        // 生成器第一个参数是缓存写入器，第二个参数是源 TOML 的本地路径。
        // 缓存文件头由 C 侧创建，脚本只负责写二进制文档。
        const std::string source_path = _resolve_source_path(filepath);
        je_writing_cache_file* const cache_file = jeecs_create_cache_file(
            filepath, ANIMATION_CACHE_VERSION, 0);
        if (cache_file == nullptr)
        {
            woort_codeenv_drop(cenv);
            return false;
        }

        bool generated = false;

        woort_vm* const vmm = woort_vm_create();
        if (vmm == nullptr)
            jeecs::debug::logerr(
                "Failed to create vm for animation cache generator '%s'.",
                ANIMATION_CACHE_GENERATOR_PATH);
        else
        {
            woort_vm* const last = woort_vm_swap(vmm);
            {
                if (WOORT_VM_CALL_STATUS_NORMAL
                    != woort_bootup(WOORT_IGNORE, cenv, false))
                {
                    jeecs::debug::logerr(
                        "Failed to init animation cache generator '%s': %s.",
                        ANIMATION_CACHE_GENERATOR_PATH,
                        woort_vm_get_runtime_error(vmm));
                }
                else
                {
                    woort_value stack;
                    if (!woort_push_reserve(3, &stack))
                        jeecs::debug::logerr(
                            "Failed to reserve stack for animation cache generator.");
                    else
                    {
                        const woort_value arg_cache = stack + 0;
                        const woort_value arg_source = stack + 1;
                        const woort_value func = stack + 2;

                        woort_set_pointer(arg_cache, cache_file);
                        woort_set_string(arg_source, source_path.c_str());

                        if (!woort_load_extern_const(
                            func, cenv, ANIMATION_CACHE_GENERATOR_ENTRY))
                        {
                            jeecs::debug::logerr(
                                "Animation cache generator '%s' has no '%s' function.",
                                ANIMATION_CACHE_GENERATOR_PATH,
                                ANIMATION_CACHE_GENERATOR_ENTRY);
                        }
                        else if (WOORT_VM_CALL_STATUS_NORMAL
                            != woort_invoke(arg_cache, func))
                        {
                            jeecs::debug::logerr(
                                "Animation cache generation failed for '%s': %s.",
                                filepath,
                                woort_vm_get_runtime_error(vmm));
                        }
                        else
                            generated = woort_bool(arg_cache);

                        woort_pop(3);
                    }
                }
            }
            (void)woort_vm_swap(last);
            woort_vm_close(vmm);
        }

        jeecs_close_cache_file(cache_file);

        if (generated)
        {
            if (jeecs_file* cache = jeecs_load_cache_file(
                filepath, ANIMATION_CACHE_VERSION, 0))
            {
                jeecs_file_close(cache);
                woort_codeenv_drop(cenv);
                return true;
            }
        }

        const std::string cache_path = _cache_file_path(filepath);
        std::remove(cache_path.c_str());

        woort_codeenv_drop(cenv);
        return false;
    }

    bool _ensure_animation_cache(const char* filepath)
    {
        if (jeecs_file* cache = jeecs_load_cache_file(
            filepath, ANIMATION_CACHE_VERSION, 0))
        {
            jeecs_file_close(cache);
            return true;
        }
        return _generate_animation_cache(filepath);
    }
}

void jeecs::Animation::FrameAnimation::animation_list::animation_data_set::load_animation(
    const std::string& str)
{
    m_animations.clear();
    m_path = str;

    if (str.empty())
        return;

    jeecs_file* cache_file =
        jeecs_load_cache_file(str.c_str(), ANIMATION_CACHE_VERSION, 0);
    if (cache_file == nullptr)
    {
        if (!_generate_animation_cache(str.c_str()))
        {
            jeecs::debug::logerr(
                "Failed to load animation '%s': cannot generate the binary cache.",
                str.c_str());
            return;
        }

        cache_file =
            jeecs_load_cache_file(str.c_str(), ANIMATION_CACHE_VERSION, 0);
        if (cache_file == nullptr)
        {
            jeecs::debug::logerr(
                "Failed to load animation '%s': generated cache is invalid.",
                str.c_str());
            return;
        }
    }

    if (!jeecs_file_seek(
        cache_file, (int64_t)CACHE_FILE_HEADER_SIZE, JE_READ_FILE_SEEK_SET))
    {
        jeecs::debug::logerr(
            "Failed to seek animation cache for '%s'.", str.c_str());
        jeecs_file_close(cache_file);
        return;
    }

    jeecs_cache_binary_document root;
    if (!jeecs_cache_binary_document_open(cache_file, &root))
    {
        jeecs::debug::logerr(
            "Invalid animation cache document in '%s'.", str.c_str());
        jeecs_file_close(cache_file);
        return;
    }

    jeecs_cache_binary_list cache_animations;
    if (!jeecs_cache_binary_document_index(
        &root, "animations", JE_CACHE_BINARY_VALUE_LIST, &cache_animations))
    {
        jeecs::debug::logerr(
            "Animation cache '%s' has no 'animations' list.", str.c_str());
        jeecs_file_close(cache_file);
        return;
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

        std::string action_name;
        if (!_read_cache_string(&action, "name", &action_name))
        {
            jeecs::debug::logerr(
                "Animation '%s' action %zu has no name.", str.c_str(), action_index);
            continue;
        }

        jeecs_cache_binary_list cache_frames;
        if (!jeecs_cache_binary_document_index(
            &action, "frames", JE_CACHE_BINARY_VALUE_LIST, &cache_frames))
        {
            jeecs::debug::logerr(
                "Animation '%s' action '%s' has no frames.",
                str.c_str(),
                action_name.c_str());
            continue;
        }

        auto& animation = m_animations[action_name.c_str()];

        for (size_t frame_index = 0;; ++frame_index)
        {
            jeecs_cache_binary_document frame;
            if (!jeecs_cache_binary_list_index(
                &cache_frames,
                frame_index,
                JE_CACHE_BINARY_VALUE_DOCUMENT,
                &frame))
                break;

            frame_data frame_dat;
            if (!jeecs_cache_binary_document_index(
                &frame, "time", JE_CACHE_BINARY_VALUE_FLOAT32, &frame_dat.m_frame_time))
            {
                jeecs::debug::logerr(
                    "Animation '%s' action '%s' frame %zu has no time.",
                    str.c_str(),
                    action_name.c_str(),
                    frame_index);
                continue;
            }

            jeecs_cache_binary_list cache_components;
            if (jeecs_cache_binary_document_index(
                &frame, "components", JE_CACHE_BINARY_VALUE_LIST, &cache_components))
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
                            str.c_str(),
                            action_name.c_str(),
                            frame_index,
                            i);
                        continue;
                    }

                    bool offset_mode = false;
                    if (!jeecs_cache_binary_document_index(
                        &component, "offset", JE_CACHE_BINARY_VALUE_BOOL, &offset_mode))
                    {
                        jeecs::debug::logerr(
                            "Animation '%s' action '%s' frame %zu component '%s.%s' has no offset mode.",
                            str.c_str(),
                            action_name.c_str(),
                            frame_index,
                            component_name.c_str(),
                            member_name.c_str());
                        continue;
                    }

                    frame_data::data_value value;
                    if (!_read_cache_data_value(&component, &value))
                    {
                        jeecs::debug::logerr(
                            "Animation '%s' action '%s' frame %zu component '%s.%s' has invalid value.",
                            str.c_str(),
                            action_name.c_str(),
                            frame_index,
                            component_name.c_str(),
                            member_name.c_str());
                        continue;
                    }

                    const je_TypeInfo* component_type =
                        jeecs::typing::of(component_name.c_str());
                    if (component_type == nullptr)
                    {
                        jeecs::debug::logerr(
                            "Failed to find component type named '%s' when reading animation '%s' action '%s' frame %zu.",
                            component_name.c_str(),
                            str.c_str(),
                            action_name.c_str(),
                            frame_index);
                        continue;
                    }

                    const je_MemberInfo* member_info =
                        jeecs::typing::find_member_by_name(
                            component_type, member_name.c_str());
                    if (member_info == nullptr)
                    {
                        jeecs::debug::logerr(
                            "Component '%s' has no member named '%s' when reading animation '%s' action '%s' frame %zu.",
                            component_name.c_str(),
                            member_name.c_str(),
                            str.c_str(),
                            action_name.c_str(),
                            frame_index);
                        continue;
                    }

                    frame_data::component_data cdata;
                    cdata.m_component_type = component_type;
                    cdata.m_member_info = member_info;
                    cdata.m_member_value = value;
                    cdata.m_offset_mode = offset_mode;
                    cdata.m_member_addr_cache = nullptr;
                    cdata.m_entity_cache = {};

                    frame_dat.m_component_data.push_back(cdata);
                }
            }

            jeecs_cache_binary_list cache_uniforms;
            if (jeecs_cache_binary_document_index(
                &frame, "uniforms", JE_CACHE_BINARY_VALUE_LIST, &cache_uniforms))
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

                    std::string uniform_name;
                    if (!_read_cache_string(&uniform, "name", &uniform_name))
                    {
                        jeecs::debug::logerr(
                            "Animation '%s' action '%s' frame %zu uniform %zu has no name.",
                            str.c_str(),
                            action_name.c_str(),
                            frame_index,
                            i);
                        continue;
                    }

                    frame_data::data_value value;
                    if (!_read_cache_data_value(&uniform, &value))
                    {
                        jeecs::debug::logerr(
                            "Animation '%s' action '%s' frame %zu uniform '%s' has invalid value.",
                            str.c_str(),
                            action_name.c_str(),
                            frame_index,
                            uniform_name.c_str());
                        continue;
                    }

                    frame_data::uniform_data udata;
                    udata.m_uniform_name = uniform_name;
                    udata.m_uniform_value = value;

                    frame_dat.m_uniform_data.push_back(udata);
                }
            }

            animation.frames.push_back(frame_dat);
        }
    }

    jeecs_file_close(cache_file);
}

WOORT_API woort_api wojeapi_animation_cache_ensure(void)
{
    const std::string path = woort_string(0);
    return woort_ret_bool(_ensure_animation_cache(path.c_str()));
}
