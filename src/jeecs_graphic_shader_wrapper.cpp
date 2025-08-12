#define JE_IMPL
#include "jeecs.hpp"
#include <functional>
#include <variant>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "jeecs_cache_version.hpp"
#include "jeecs_graphic_shader_wrapper.hpp"
#include "jeecs_graphic_shader_wrapper_methods.hpp"

#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
#define JE_ENABLE_GLSL_CACHE_LOADING
#endif

#if defined(JE_ENABLE_GLSL_CACHE_LOADING) || !defined(JE4_BUILD_FOR_RUNTIME_TARGET_ONLY)
#define JE_ENABLE_GLSL_GENERATION
#include "jeecs_graphic_shader_wrapper_glsl.hpp"
#endif

#if defined(JE_ENABLE_DX11_GAPI)
#define JE_ENABLE_HLSL_CACHE_LOADING
#endif

#if defined(JE_ENABLE_HLSL_CACHE_LOADING) || defined(JE_ENABLE_VK130_GAPI) || !defined(JE4_BUILD_FOR_RUNTIME_TARGET_ONLY)
#define JE_ENABLE_HLSL_GENERATION
#include "jeecs_graphic_shader_wrapper_hlsl.hpp"
#endif

#if defined(JE_ENABLE_VK130_GAPI)
#define JE_ENABLE_SPIRV_CACHE_LOADING
#endif

#if defined(JE_ENABLE_SPIRV_CACHE_LOADING) || !defined(JE4_BUILD_FOR_RUNTIME_TARGET_ONLY)
#define JE_ENABLE_SPIRV_GENERATION
#include <glslang_c_interface.h>
#include <resource_limits_c.h>
// #   include <spirv_cross_c.h>

jegl_shader::spir_v_code_t *_jegl_parse_spir_v_from_hlsl(const char *hlsl_src, bool is_fragment, size_t *out_codelen)
{
    glslang_input_t hlsl_shader_input;
    hlsl_shader_input.language = glslang_source_t::GLSLANG_SOURCE_HLSL;
    hlsl_shader_input.stage = is_fragment
                                  ? glslang_stage_t::GLSLANG_STAGE_FRAGMENT
                                  : glslang_stage_t::GLSLANG_STAGE_VERTEX;
    hlsl_shader_input.client = glslang_client_t::GLSLANG_CLIENT_VULKAN;
    hlsl_shader_input.client_version = glslang_target_client_version_t::GLSLANG_TARGET_VULKAN_1_1;
    hlsl_shader_input.target_language = glslang_target_language_t::GLSLANG_TARGET_SPV;
    hlsl_shader_input.target_language_version = glslang_target_language_version_t::GLSLANG_TARGET_SPV_1_1;

    hlsl_shader_input.code = hlsl_src;
    hlsl_shader_input.default_version = 50; // HLSL VERSION?
    hlsl_shader_input.default_profile = GLSLANG_NO_PROFILE;
    hlsl_shader_input.force_default_version_and_profile = 0; // ?
    hlsl_shader_input.forward_compatible = 0;                // ?
    hlsl_shader_input.messages = glslang_messages_t::GLSLANG_MSG_DEFAULT_BIT;
    hlsl_shader_input.resource = glslang_default_resource();
    hlsl_shader_input.callbacks = {};
    hlsl_shader_input.callbacks_ctx = nullptr;

    glslang_program_t *program = glslang_program_create();
    glslang_shader_t *hlsl_shader = glslang_shader_create(&hlsl_shader_input);

    glslang_shader_set_entry_point(hlsl_shader, is_fragment ? "fragment_main" : "vertex_main");
    glslang_shader_set_preamble(hlsl_shader, "#define GLSLANG_HLSL_TO_SPIRV 1");

    if (!glslang_shader_preprocess(hlsl_shader, &hlsl_shader_input))
    {
        jeecs::debug::logfatal("Failed to preprocess hlsl vertex shader: %s.",
                               glslang_shader_get_info_log(hlsl_shader));
    }
    if (!glslang_shader_parse(hlsl_shader, &hlsl_shader_input))
    {
        jeecs::debug::logfatal("Failed to preprocess hlsl vertex shader: %s.",
                               glslang_shader_get_info_log(hlsl_shader));
    }

    glslang_program_add_shader(program, hlsl_shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT))
    {
        jeecs::debug::logfatal("Failed to preprocess hlsl vertex program: %s.",
                               glslang_program_get_info_log(program));
    }

    glslang_spv_options_t spv_options{};
    spv_options.generate_debug_info = false;
    spv_options.strip_debug_info = false;
    spv_options.disable_optimizer = false;
    spv_options.optimize_size = true;
    spv_options.disassemble = false;
    spv_options.validate = true;
    spv_options.emit_nonsemantic_shader_debug_info = false;
    spv_options.emit_nonsemantic_shader_debug_source = false;
    spv_options.compile_only = false;

    glslang_program_SPIRV_generate_with_options(program, hlsl_shader_input.stage, &spv_options);
    if (glslang_program_SPIRV_get_messages(program))
    {
        jeecs::debug::logfatal("Failed to generate code hlsl vertex program: %s.",
                               glslang_program_SPIRV_get_messages(program));
    }

    auto spir_v_code_len = glslang_program_SPIRV_get_size(program);
    auto spir_v_codes = glslang_program_SPIRV_get_ptr(program);

    *out_codelen = spir_v_code_len;
    jegl_shader::spir_v_code_t *codes =
        (jegl_shader::spir_v_code_t *)je_mem_alloc(
            spir_v_code_len * sizeof(jegl_shader::spir_v_code_t));

    memcpy(codes, spir_v_codes,
           spir_v_code_len * sizeof(jegl_shader::spir_v_code_t));

    /*_debug_jegl_regenerate_hlsl_from_spir_v(spir_v_codes, spir_v_code_len);
    _debug_jegl_regenerate_glsl_from_spir_v(spir_v_codes, spir_v_code_len);*/

    glslang_shader_delete(hlsl_shader);
    glslang_program_delete(program);

    return codes;
}
#endif

void delete_shader_value(jegl_shader_value *shader_val)
{
    do
    {
        if (shader_val->m_ref_count)
        {
            --shader_val->m_ref_count;
            return;
        }
    } while (0);
    if (shader_val->is_calc_value())
    {
        if (shader_val->is_shader_in_value())
        {
            ;
        }
        else if (shader_val->is_uniform_variable())
        {
            je_mem_free((void *)shader_val->m_unifrom_varname);
            if (shader_val->m_uniform_init_val_may_nil)
                delete_shader_value(shader_val->m_uniform_init_val_may_nil);
        }
        else
        {
            for (size_t i = 0; i < shader_val->m_opnums_count; ++i)
                delete_shader_value(shader_val->m_opnums[i]);
            delete[] shader_val->m_opnums;

            je_mem_free((void *)shader_val->m_opname);
        }
    }

    delete shader_val;
}
void _free_shader_value(void *shader_value)
{
    jegl_shader_value *shader_val = (jegl_shader_value *)shader_value;
    delete_shader_value(shader_val);
}
WO_API wo_api jeecs_shader_float_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (float)wo_real(args + 0)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_integer_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (int)wo_int(args + 0)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_integer2_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (int)wo_int(args + 0), (int)wo_int(args + 1)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_integer3_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (int)wo_int(args + 0), (int)wo_int(args + 1), (int)wo_int(args + 2)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_integer4_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (int)wo_int(args + 0), (int)wo_int(args + 1), (int)wo_int(args + 2), (int)wo_int(args + 3)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float2_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (float)wo_real(args + 0),
                               (float)wo_real(args + 1)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float3_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (float)wo_real(args + 0),
                               (float)wo_real(args + 1),
                               (float)wo_real(args + 2)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float4_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (float)wo_real(args + 0),
                               (float)wo_real(args + 1),
                               (float)wo_real(args + 2),
                               (float)wo_real(args + 3)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float3x3_create(wo_vm vm, wo_value args)
{
    float data[9] = {};
    for (size_t i = 0; i < 9; i++)
        data[i] = (float)wo_real(args + i);
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(data, jegl_shader_value::FLOAT3x3),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float4x4_create(wo_vm vm, wo_value args)
{
    float data[16] = {};
    for (size_t i = 0; i < 16; i++)
        data[i] = (float)wo_real(args + i);
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(data, jegl_shader_value::FLOAT4x4),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_texture2d_set_channel(wo_vm vm, wo_value args)
{
    jegl_shader_value *texture2d_val = (jegl_shader_value *)wo_pointer(args + 0);
    assert(texture2d_val->get_type() == jegl_shader_value::type::TEXTURE2D || texture2d_val->get_type() == jegl_shader_value::type::TEXTURE2D_MS || texture2d_val->get_type() == jegl_shader_value::type::TEXTURE_CUBE);

    texture2d_val->m_uniform_texture_channel = (int)wo_int(args + 1);
    return wo_ret_val(vm, args + 0);
}
WO_API wo_api jeecs_shader_create_rot_mat4x4(wo_vm vm, wo_value args)
{
    float data[16] = {};
    jeecs::math::quat q((float)wo_real(args + 0), (float)wo_real(args + 1), (float)wo_real(args + 2));
    q.create_matrix(data);
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(data, jegl_shader_value::FLOAT4x4),
                           nullptr, _free_shader_value);
}

struct action_node
{
    std::vector<action_node *> m_next_step;

    jegl_shader_value::type_base_t m_acceptable_types;
    calc_func_t m_reduce_const_function;

    ~action_node()
    {
        for (auto *node : m_next_step)
            delete node;
    }
    action_node *&_get_next_step(jegl_shader_value::type_base_t type)
    {
        auto fnd = std::find_if(m_next_step.begin(), m_next_step.end(),
                                [&](action_node *v)
                                {if (v->m_acceptable_types == type)return true; return false; });
        if (fnd == m_next_step.end())
            return m_next_step.emplace_back(nullptr);
        return *fnd;
    }
};
std::unordered_map<std::string, action_node *> _generate_accpetable_tree()
{
    std::unordered_map<std::string, action_node *> tree;
    for (auto &[action, type_lists] : _operation_table)
    {
        auto **act_node = &tree[action];
        if (!*act_node)
            *act_node = new action_node;

        for (auto &type_or_act : type_lists)
        {
            if (auto type = std::get_if<jegl_shader_value::type_base_t>(&type_or_act))
            {
                if (auto &new_act_node = (*act_node)->_get_next_step(*type))
                    act_node = &new_act_node;
                else
                {
                    new_act_node = new action_node;
                    new_act_node->m_acceptable_types = *type;
                    act_node = &new_act_node;
                }
            }
            else
            {
                if ((*act_node)->m_reduce_const_function)
                    jeecs::debug::logfatal("Shader operation map conflict.");
                (*act_node)->m_reduce_const_function = std::get<calc_func_t>(type_or_act);
            }
        }
    }
    return tree;
}
const std::unordered_map<std::string, action_node *> &shader_operation_map()
{
    const static std::unordered_map<std::string, action_node *> &_s = _generate_accpetable_tree();
    return _s;
}
calc_func_t *_get_reduce_func(action_node *cur_node, jegl_shader_value::type *argts, size_t argc, size_t index)
{
    if (index == argc)
    {
        if (cur_node->m_reduce_const_function)
            return &cur_node->m_reduce_const_function;
        else
            return nullptr;
    }

    assert(index < argc && cur_node);

    for (auto *next_step : cur_node->m_next_step)
    {
        if (next_step->m_acceptable_types & argts[index])
        {
            auto *fnd = _get_reduce_func(next_step, argts, argc, index + 1);
            if (fnd)
                return fnd;
        }
    }
    return nullptr;
}
calc_func_t *get_const_reduce_func(const char *op, jegl_shader_value::type *argts, size_t argc)
{
    auto &_shader_operation_map = shader_operation_map();
    auto fnd = _shader_operation_map.find(op);
    if (fnd == _shader_operation_map.end())
        return nullptr;
    auto *cur_node = fnd->second;
    return _get_reduce_func(cur_node, argts, argc, 0);
}
WO_API wo_api jeecs_shader_create_uniform_variable(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
                           new jegl_shader_value(
                               (jegl_shader_value::type)wo_int(args + 0),
                               wo_string(args + 1),
                               (jegl_shader_value *)nullptr,
                               (bool)wo_bool(args + 2)),
                           nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_create_uniform_variable_with_init_value(wo_vm vm, wo_value args)
{
    jegl_shader_value::type type = (jegl_shader_value::type)wo_int(args + 0);
    jegl_shader_value *init_value = (jegl_shader_value *)wo_pointer(args + 2);
    if (!init_value->is_init_value())
        return wo_ret_halt(vm, "Cannot do this operations: uniform variable's init value must be immediately.");
    if (init_value->get_type() != type)
        return wo_ret_halt(vm, "Cannot do this operations: uniform variable's init value must have same type.");

    return wo_ret_gchandle(vm,
                           new jegl_shader_value(type, wo_string(args + 1), init_value, false), nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_create_sampler2d(wo_vm vm, wo_value args)
{
    auto *sampler = new shader_sampler;
    sampler->m_min = (jegl_shader::fliter_mode)wo_int(args + 0);
    sampler->m_mag = (jegl_shader::fliter_mode)wo_int(args + 1);
    sampler->m_mip = (jegl_shader::fliter_mode)wo_int(args + 2);
    sampler->m_uwrap = (jegl_shader::wrap_mode)wo_int(args + 3);
    sampler->m_vwrap = (jegl_shader::wrap_mode)wo_int(args + 4);
    sampler->m_sampler_id = (uint32_t)wo_int(args + 5);
    return wo_ret_gchandle(vm, sampler, nullptr, [](void *p)
                           { delete (shader_sampler *)p; });
}
WO_API wo_api jeecs_shader_sampler2d_bind_texture(wo_vm vm, wo_value args)
{
    shader_sampler *sampler = (shader_sampler *)wo_pointer(args + 0);
    jegl_shader_value *value = (jegl_shader_value *)wo_pointer(args + 1);

    assert(value->m_binded_sampler_id == jeecs::typing::INVALID_UINT32);
    value->m_binded_sampler_id = sampler->m_sampler_id;

    sampler->m_binded_texture_passid.push_back((uint32_t)value->m_uniform_texture_channel);
    return wo_ret_void(vm);
}
WO_API wo_api jeecs_shader_apply_operation(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_argc(vm);

    bool result_is_const = true;
    std::vector<jegl_shader_value *> tmp_svalue;
    struct AutoRelease
    {
        std::function<void(void)> _m_func;
        AutoRelease(const std::function<void(void)> &f) : _m_func(f) {}
        ~AutoRelease() { _m_func(); }
    };
    AutoRelease auto_release([&]()
                             {
        for (auto& tmp : tmp_svalue)
            delete_shader_value(tmp); });

    std::vector<jegl_shader_value::type> _types(argc - 2);
    std::vector<jegl_shader_value *> _args(argc - 2);
    for (size_t i = 2; i < argc; ++i)
    {
        auto value_type = wo_valuetype(args + i);
        if (value_type != WO_INTEGER_TYPE && value_type != WO_REAL_TYPE && value_type != WO_GCHANDLE_TYPE && value_type != WO_HANDLE_TYPE)
            return wo_ret_halt(vm, "Cannot do this operations: argument type should be number or shader_value.");

        jegl_shader_value *sval;
        if (value_type == WO_GCHANDLE_TYPE || value_type == WO_HANDLE_TYPE)
            sval = (jegl_shader_value *)wo_pointer(args + i);
        else if (value_type == WO_INTEGER_TYPE)
        {
            sval = new jegl_shader_value((int)wo_int(args + i));
            tmp_svalue.push_back(sval);
        }
        else
        {
            sval = new jegl_shader_value((float)wo_cast_real(args + i));
            tmp_svalue.push_back(sval);
        }
        _types[i - 2] = sval->get_type();
        _args[i - 2] = sval;

        if (sval->is_calc_value())
            result_is_const = false;
    }
    jegl_shader_value::type result_type = (jegl_shader_value::type)wo_int(args + 0);
    wo_string_t operation = wo_string(args + 1);

    if (operation[0] == '.')
        // Index operation.
        result_type = (jegl_shader_value::type)(result_type | jegl_shader_value::type::FAST_EVAL);
    else
    {
        auto *reduce_func = get_const_reduce_func(operation, _types.data(), _types.size());
        if (operation[0] == '#')
            // Custom method
            ;
        else if (operation[0] == '%')
            // Type casting
            result_type = (jegl_shader_value::type)(result_type | jegl_shader_value::type::FAST_EVAL);
        else
        {
            if (!reduce_func)
                return wo_ret_halt(vm, "Cannot do this operations: no matched operation with these types.");

            if (result_is_const)
            {
                auto *result = (*reduce_func)(_args.size(), _args.data());
                if (result) // if result == nullptr, there is no method for constant, calc it in shader~
                {
                    if (result->get_type() != result_type)
                    {
                        _free_shader_value(result);
                        return wo_ret_halt(vm, "Cannot do this operations: return type dis-matched.");
                    }
                    return wo_ret_gchandle(vm, result, nullptr, _free_shader_value);
                }
            }
        }
    }

    jegl_shader_value *val =
        new jegl_shader_value(result_type, operation, argc - 2);
    for (size_t i = 2; i < argc; ++i)
        val->set_used_val(i - 2, _args[i - 2]);

    return wo_ret_gchandle(vm, val, nullptr, _free_shader_value);
}

struct vertex_in_data_storage
{
    std::unordered_map<int, jegl_shader_value *> m_in_from_vao_guard;
    ~vertex_in_data_storage()
    {
        for (auto &[id, val] : m_in_from_vao_guard)
            delete_shader_value(val);
    }
    jegl_shader_value *get_val_at(size_t pos, jegl_shader_value::type type)
    {
        auto fnd = m_in_from_vao_guard.find((int)pos);
        if (fnd != m_in_from_vao_guard.end())
        {
            if (fnd->second->get_type() != type)
                return nullptr;
            return fnd->second;
        }
        auto &shval = m_in_from_vao_guard[(int)pos];
        shval = new jegl_shader_value(type);
        shval->m_shader_in_index = pos;
        return shval;
    }
};
WO_API wo_api jeecs_shader_create_vertex_in(wo_vm vm, wo_value args)
{
    // This function is used for debug
    return wo_ret_gchandle(vm, new vertex_in_data_storage, nullptr, [](void *ptr)
                           { delete (vertex_in_data_storage *)ptr; });
}
WO_API wo_api jeecs_shader_get_vertex_in(wo_vm vm, wo_value args)
{
    vertex_in_data_storage *storage = (vertex_in_data_storage *)wo_pointer(args + 0);
    jegl_shader_value::type type = (jegl_shader_value::type)wo_int(args + 1);
    size_t pos = (size_t)wo_int(args + 2);

    auto *result = storage->get_val_at(pos, type);
    if (!result)
        return wo_ret_halt(vm, ("vertex_in[" + std::to_string(pos) + "] has been used, but type didn't match.").c_str());

    return wo_ret_pointer(vm, result);
}
WO_API wo_api jeecs_shader_set_vertex_out(wo_vm vm, wo_value args)
{
    jegl_shader_value *vertex_out_pos = (jegl_shader_value *)wo_pointer(args + 0);
    if (vertex_out_pos->get_type() != jegl_shader_value::FLOAT4)
        return wo_ret_halt(vm, "First value of vertex_out must be FLOAT4 for position.");

    return wo_ret_void(vm);
}
WO_API wo_api jeecs_shader_create_struct_define(wo_vm vm, wo_value args)
{
    shader_struct_define *block = new shader_struct_define;
    block->binding_place = jeecs::typing::INVALID_UINT32;
    block->name = wo_string(args + 0);
    return wo_ret_pointer(vm, block);
}
WO_API wo_api jeecs_shader_bind_struct_as_uniform_buffer(wo_vm vm, wo_value args)
{
    shader_struct_define *block = (shader_struct_define *)wo_pointer(args + 0);
    block->binding_place = (uint32_t)wo_int(args + 1);
    return wo_ret_void(vm);
}
WO_API wo_api jeecs_shader_append_struct_member(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    shader_struct_define *block = (shader_struct_define *)wo_pointer(args + 0);

    shader_struct_define::struct_variable variable_member_define;
    variable_member_define.type = (jegl_shader_value::type)wo_int(args + 1);
    variable_member_define.name = wo_string(args + 2);

    wo_value elem = s + 0;
    if (wo_option_get(elem, args + 3))
    {
        assert(variable_member_define.type == jegl_shader_value::type::STRUCT);
        variable_member_define.struct_type_may_nil = (shader_struct_define *)wo_pointer(elem);
    }
    else
    {
        assert(variable_member_define.type != jegl_shader_value::type::STRUCT);
        variable_member_define.struct_type_may_nil = nullptr;
    }

    if (wo_option_get(elem, args + 4))
        variable_member_define.array_size = (size_t)wo_int(elem);
    else
        variable_member_define.array_size = std::nullopt;

    block->variables.push_back(variable_member_define);

    return wo_ret_void(vm);
}
WO_API wo_api jeecs_shader_create_shader_value_out(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_value voutstruct = args + 1;

    if (wo_valuetype(voutstruct) != WO_STRUCT_TYPE)
        return wo_ret_halt(vm, "'type' must struct when return from vext or frag.");

    uint16_t structsz = (uint16_t)wo_struct_len(voutstruct);

    // is vertex out, check it
    wo_value elem = s + 0;

    // If vertext
    if (wo_bool(args + 0))
    {
        jegl_shader_value *val = nullptr;
        if (structsz > 0)
        {
            wo_struct_get(elem, voutstruct, 0);
            val = (jegl_shader_value *)wo_pointer(elem);
        }

        if (val == nullptr || jegl_shader_value::type::FLOAT4 != val->get_type())
            return wo_ret_halt(vm, "'vert' must return a struct with first member of 'float4'.");
    }

    shader_value_outs *values = new shader_value_outs;
    values->out_values.resize(structsz);
    for (uint16_t i = 0; i < structsz; i++)
    {
        wo_struct_get(elem, voutstruct, i);
        values->out_values[i] = (jegl_shader_value *)wo_pointer(elem);
        values->out_values[i]->add_useref_count();
    }
    return wo_ret_pointer(vm, values);
}
WO_API wo_api jeecs_shader_create_fragment_in(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    shader_value_outs *values = (shader_value_outs *)wo_pointer(args + 0);

    uint16_t fragmentin_size = (uint16_t)values->out_values.size();
    wo_value out_struct = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(out_struct, vm, fragmentin_size);

    for (uint16_t i = 0; i < fragmentin_size; i++)
    {
        auto *val = new jegl_shader_value(values->out_values[i]->get_type());
        val->m_shader_in_index = i;
        wo_set_gchandle(elem, vm, val, nullptr, _free_shader_value);
        wo_struct_set(out_struct, i, elem);
    }

    return wo_ret_val(vm, out_struct);
}
WO_API wo_api jeecs_shader_wrap_result_pack(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_value elem = s + 0;
    wo_value val = s + 1;

    shader_wrapper *wrapper = new shader_wrapper(
        (shader_value_outs *)wo_pointer(args + 1),
        (shader_value_outs *)wo_pointer(args + 2));

    wo_integer_t vin_size = wo_struct_len(args + 0);

    for (wo_integer_t i = 0; i < vin_size; ++i)
    {
        wo_struct_get(elem, args + 0, (uint16_t)i);
        auto *shader_val = (jegl_shader_value *)wo_pointer(elem);

        if (shader_val->is_shader_in_value() == false)
            return wo_ret_halt(vm, "Unsupport value source, should be vertex in.");

        wrapper->vertex_in.push_back(shader_val->get_type());
    }

    wo_struct_get(elem, args + 3, 0);
    wrapper->shader_config.m_enable_shared = wo_bool(elem);
    wo_struct_get(elem, args + 3, 1);
    wrapper->shader_config.m_depth_test = (jegl_shader::depth_test_method)wo_int(elem);
    wo_struct_get(elem, args + 3, 2);
    wrapper->shader_config.m_depth_mask = (jegl_shader::depth_mask_method)wo_int(elem);
    wo_struct_get(elem, args + 3, 3);
    wrapper->shader_config.m_blend_equation = (jegl_shader::blend_equation)wo_int(elem);
    wo_struct_get(elem, args + 3, 4);
    wrapper->shader_config.m_blend_src = (jegl_shader::blend_method)wo_int(elem);
    wo_struct_get(elem, args + 3, 5);
    wrapper->shader_config.m_blend_dst = (jegl_shader::blend_method)wo_int(elem);
    wo_struct_get(elem, args + 3, 6);
    wrapper->shader_config.m_cull_mode = (jegl_shader::cull_mode)wo_int(elem);

    size_t ubo_count = (size_t)wo_arr_len(args + 4);
    for (size_t i = 0; i < ubo_count; ++i)
    {
        wo_arr_get(elem, args + 4, i);
        wrapper->shader_struct_define_may_uniform_block.push_back(
            (shader_struct_define *)wo_pointer(elem));
    }

    size_t sampler_count = (size_t)wo_arr_len(args + 5);
    for (size_t i = 0; i < sampler_count; ++i)
    {
        wo_arr_get(elem, args + 5, i);
        wrapper->decleared_samplers.push_back(
            (shader_sampler *)wo_pointer(elem));
    }

    size_t custom_method_count = (size_t)wo_arr_len(args + 6);
    for (size_t i = 0; i < custom_method_count; ++i)
    {
        wo_arr_get(elem, args + 6, i);
        wo_struct_get(val, elem, 0);

        auto &custom_shader_srcs = wrapper->custom_methods[wo_string(val)];

        wo_struct_get(val, elem, 1);

        wo_struct_get(elem, val, 0);
        custom_shader_srcs.m_glsl_impl = wo_string(elem);
        wo_struct_get(elem, val, 1);
        custom_shader_srcs.m_hlsl_impl = wo_string(elem);
    }

    size_t user_function_count = (size_t)wo_arr_len(args + 7);
    for (size_t i = 0; i < user_function_count; ++i)
    {
        wo_arr_get(elem, args + 7, i);
        wo_struct_get(val, elem, 0);

        auto &user_function = wrapper->user_define_functions[wo_string(val)];
        user_function.m_name = wo_string(val);

        wo_struct_get(val, elem, 2);
        user_function.m_result = (shader_value_outs *)wo_pointer(val);

        wo_struct_get(val, elem, 1);
        size_t user_function_arg_count = (size_t)wo_struct_len(val);
        for (size_t i = 0; i < user_function_arg_count; ++i)
        {
            wo_struct_get(elem, val, (uint16_t)i);
            auto *shader_val = (jegl_shader_value *)wo_pointer(elem);

            if (shader_val->is_shader_in_value() == false)
                return wo_ret_halt(vm, "Unsupport value source, should be user function in.");

            user_function.m_args.push_back(shader_val->get_type());
        }

        user_function.m_used_in_fragment = false;
        user_function.m_used_in_vertex = false;
    }

    return wo_ret_gchandle(vm,
                           wrapper, nullptr,
                           [](void *ptr)
                           {
                               delete (shader_wrapper *)ptr;
                           });
}
WO_API wo_api jeecs_shader_real_raw_op_add(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, wo_real(args + 0) + wo_real(args + 1));
}
WO_API wo_api jeecs_shader_real_raw_op_sub(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, wo_real(args + 0) - wo_real(args + 1));
}
WO_API wo_api jeecs_shader_real_raw_op_mul(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, wo_real(args + 0) * wo_real(args + 1));
}
WO_API wo_api jeecs_shader_real_raw_op_div(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, wo_real(args + 0) / wo_real(args + 1));
}

void _scan_used_uniforms_in_vals(
    shader_wrapper *wrap,
    jegl_shader_value *val,
    bool in_vertex,
    std::unordered_set<jegl_shader_value *> *sanned)
{
    if (val->is_calc_value())
    {
        if (val->is_uniform_variable())
        {
            if (!val->is_block_uniform_variable())
            {
                if (wrap->uniform_variables.find(val->m_unifrom_varname) == wrap->uniform_variables.end())
                {
                    wrap->uniform_variables[val->m_unifrom_varname] =
                        _shader_wrapper_contex::get_uniform_info(val);
                }
                auto &uinfo = wrap->uniform_variables.at(val->m_unifrom_varname);
                if (in_vertex)
                    uinfo.m_used_in_vertex = true;
                else
                    uinfo.m_used_in_fragment = true;
            }
        }
        else if (!val->is_shader_in_value())
        {
            if (sanned->find(val) != sanned->end())
                return;
            sanned->insert(val);

            if (val->m_opname[0] == '#')
            {
                auto fnd = wrap->user_define_functions.find(val->m_opname + 1);
                if (fnd != wrap->user_define_functions.end())
                {
                    auto &func_info = fnd->second;
                    assert(func_info.m_result != nullptr && func_info.m_result->out_values.size() == 1);

                    if (in_vertex && !func_info.m_used_in_vertex)
                    {
                        func_info.m_used_in_vertex = true;
                        _scan_used_uniforms_in_vals(
                            wrap, func_info.m_result->out_values[0], true, sanned);
                    }
                    else if (!func_info.m_used_in_fragment)
                    {
                        func_info.m_used_in_fragment = true;
                        _scan_used_uniforms_in_vals(
                            wrap, func_info.m_result->out_values[0], false, sanned);
                    }
                }
            }

            for (size_t i = 0; i < val->m_opnums_count; ++i)
                _scan_used_uniforms_in_vals(
                    wrap, val->m_opnums[i], in_vertex, sanned);
        }
    }
}

void scan_used_uniforms_in_wrap(shader_wrapper *wrap)
{
    std::unordered_set<jegl_shader_value *> _scanned_val;
    for (auto *vout : wrap->vertex_out->out_values)
        _scan_used_uniforms_in_vals(wrap, vout, true, &_scanned_val);
    for (auto *vout : wrap->fragment_out->out_values)
        _scan_used_uniforms_in_vals(wrap, vout, false, &_scanned_val);
}

#if 0
void _debug_jegl_regenerate_hlsl_from_spir_v(uint32_t* spir_v_code, size_t spir_v_ir_count)
{
    spvc_context spir_v_cross_context = nullptr;
    spvc_context_create(&spir_v_cross_context);

    spvc_parsed_ir ir = nullptr;
    spvc_context_parse_spirv(spir_v_cross_context, spir_v_code, spir_v_ir_count, &ir);

    spvc_compiler compiler = nullptr;
    spvc_context_create_compiler(spir_v_cross_context, SPVC_BACKEND_HLSL, ir, SPVC_CAPTURE_MODE_COPY, &compiler);

    spvc_compiler_options options = nullptr;
    spvc_compiler_create_compiler_options(compiler, &options);

    spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, 50);

    spvc_compiler_install_compiler_options(compiler, options);

    // 转换成hlsl
    const char* src = nullptr;
    spvc_compiler_compile(compiler, &src);

    spvc_context_destroy(spir_v_cross_context);
}
void _debug_jegl_regenerate_glsl_from_spir_v(uint32_t* spir_v_code, size_t spir_v_ir_count)
{
    spvc_context spir_v_cross_context = nullptr;
    spvc_context_create(&spir_v_cross_context);

    spvc_parsed_ir ir = nullptr;
    spvc_context_parse_spirv(spir_v_cross_context, spir_v_code, spir_v_ir_count, &ir);

    spvc_compiler compiler = nullptr;
    spvc_context_create_compiler(spir_v_cross_context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_COPY, &compiler);

    spvc_compiler_options options = nullptr;
    spvc_compiler_create_compiler_options(compiler, &options);

    spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 450);

    spvc_compiler_install_compiler_options(compiler, options);

    // 转换成glsl
    const char* src = nullptr;
    spvc_compiler_compile(compiler, &src);

    spvc_context_destroy(spir_v_cross_context);
}
#endif

jegl_resource *_create_resource();

jegl_resource *_jegl_load_shader_cache(jeecs_file *cache_file, const char *path)
{
    assert(cache_file != nullptr);

    jegl_shader *_shader = new jegl_shader();

    jegl_resource *shader = _create_resource();
    shader->m_type = jegl_resource::SHADER;
    shader->m_raw_shader_data = _shader;

    uint64_t
        vertex_glsl_src_len,
        fragment_glsl_src_len,
        vertex_hlsl_src_len,
        fragment_hlsl_src_len,
        vertex_spirv_src_len,
        fragment_spirv_src_len;

    // 1. Read generated source
#ifdef JE_ENABLE_GLSL_CACHE_LOADING
    jeecs_file_read(&vertex_glsl_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_vertex_glsl_src = (const char *)je_mem_alloc((size_t)vertex_glsl_src_len + 1);
    jeecs_file_read(const_cast<char *>(_shader->m_vertex_glsl_src), sizeof(char), (size_t)vertex_glsl_src_len, cache_file);
    const_cast<char *>(_shader->m_vertex_glsl_src)[(size_t)vertex_glsl_src_len] = 0;

    jeecs_file_read(&fragment_glsl_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_fragment_glsl_src = (const char *)je_mem_alloc((size_t)fragment_glsl_src_len + 1);
    jeecs_file_read(const_cast<char *>(_shader->m_fragment_glsl_src), sizeof(char), (size_t)fragment_glsl_src_len, cache_file);
    const_cast<char *>(_shader->m_fragment_glsl_src)[(size_t)fragment_glsl_src_len] = 0;
#else
    _shader->m_vertex_glsl_src = nullptr;
    _shader->m_fragment_glsl_src = nullptr;

    jeecs_file_read(&vertex_glsl_src_len, sizeof(uint64_t), 1, cache_file);
    jeecs_file_seek(cache_file, vertex_glsl_src_len, JE_READ_FILE_CURRENT);

    jeecs_file_read(&fragment_glsl_src_len, sizeof(uint64_t), 1, cache_file);
    jeecs_file_seek(cache_file, fragment_glsl_src_len, JE_READ_FILE_CURRENT);
#endif

#ifdef JE_ENABLE_HLSL_CACHE_LOADING
    jeecs_file_read(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_vertex_hlsl_src = (const char *)je_mem_alloc((size_t)vertex_hlsl_src_len + 1);
    jeecs_file_read(const_cast<char *>(_shader->m_vertex_hlsl_src), sizeof(char), (size_t)vertex_hlsl_src_len, cache_file);
    const_cast<char *>(_shader->m_vertex_hlsl_src)[(size_t)vertex_hlsl_src_len] = 0;

    jeecs_file_read(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_fragment_hlsl_src = (const char *)je_mem_alloc((size_t)fragment_hlsl_src_len + 1);
    jeecs_file_read(const_cast<char *>(_shader->m_fragment_hlsl_src), sizeof(char), (size_t)fragment_hlsl_src_len, cache_file);
    const_cast<char *>(_shader->m_fragment_hlsl_src)[(size_t)fragment_hlsl_src_len] = 0;
#else
    _shader->m_vertex_hlsl_src = nullptr;
    _shader->m_fragment_hlsl_src = nullptr;

    jeecs_file_read(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cache_file);
    jeecs_file_seek(cache_file, vertex_hlsl_src_len, JE_READ_FILE_CURRENT);

    jeecs_file_read(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cache_file);
    jeecs_file_seek(cache_file, fragment_hlsl_src_len, JE_READ_FILE_CURRENT);
#endif

#ifdef JE_ENABLE_SPIRV_CACHE_LOADING
    jeecs_file_read(&vertex_spirv_src_len, sizeof(uint64_t), 1, cache_file);
    assert((size_t)vertex_spirv_src_len % sizeof(jegl_shader::spir_v_code_t) == 0);

    _shader->m_vertex_spirv_count = (size_t)vertex_spirv_src_len / sizeof(jegl_shader::spir_v_code_t);
    _shader->m_vertex_spirv_codes = (const jegl_shader::spir_v_code_t *)je_mem_alloc((size_t)vertex_spirv_src_len);
    jeecs_file_read(const_cast<jegl_shader::spir_v_code_t *>(_shader->m_vertex_spirv_codes), sizeof(char), (size_t)vertex_spirv_src_len, cache_file);

    jeecs_file_read(&fragment_spirv_src_len, sizeof(uint64_t), 1, cache_file);
    assert((size_t)fragment_spirv_src_len % sizeof(jegl_shader::spir_v_code_t) == 0);

    _shader->m_fragment_spirv_count = (size_t)fragment_spirv_src_len / sizeof(jegl_shader::spir_v_code_t);
    _shader->m_fragment_spirv_codes = (const jegl_shader::spir_v_code_t *)je_mem_alloc((size_t)fragment_spirv_src_len);
    jeecs_file_read(const_cast<jegl_shader::spir_v_code_t *>(_shader->m_fragment_spirv_codes), sizeof(char), (size_t)fragment_spirv_src_len, cache_file);
#else
    _shader->m_vertex_spirv_codes = nullptr;
    _shader->m_fragment_spirv_codes = nullptr;
    _shader->m_vertex_spirv_count = 0;
    _shader->m_fragment_spirv_count = 0;

    jeecs_file_read(&vertex_spirv_src_len, sizeof(uint64_t), 1, cache_file);
    jeecs_file_seek(cache_file, vertex_spirv_src_len, JE_READ_FILE_CURRENT);

    jeecs_file_read(&fragment_spirv_src_len, sizeof(uint64_t), 1, cache_file);
    jeecs_file_seek(cache_file, fragment_spirv_src_len, JE_READ_FILE_CURRENT);
#endif

    // 2. read shader config
    jeecs_file_read(&_shader->m_depth_test, sizeof(jegl_shader::depth_test_method), 1, cache_file);
    jeecs_file_read(&_shader->m_depth_mask, sizeof(jegl_shader::depth_mask_method), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_equation, sizeof(jegl_shader::blend_equation), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_src_mode, sizeof(jegl_shader::blend_method), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_dst_mode, sizeof(jegl_shader::blend_method), 1, cache_file);
    jeecs_file_read(&_shader->m_cull_mode, sizeof(jegl_shader::cull_mode), 1, cache_file);

    // 3. read if shader is enable to shared?
    jeecs_file_read(&_shader->m_enable_to_shared, sizeof(bool), 1, cache_file);

    // 4. read and generate custom variable & uniform block informs

    // 4.1 read and generate custom variable
    uint64_t custom_uniform_count;
    jeecs_file_read(&custom_uniform_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_custom_uniforms = nullptr;

    jegl_shader::unifrom_variables *last_create_variable = nullptr;
    for (uint64_t i = 0; i < custom_uniform_count; ++i)
    {
        jegl_shader::unifrom_variables *current_variable = new jegl_shader::unifrom_variables();
        if (_shader->m_custom_uniforms == nullptr)
            _shader->m_custom_uniforms = current_variable;

        if (last_create_variable != nullptr)
            last_create_variable->m_next = current_variable;

        // 4.1.1 read name
        uint64_t uniform_name_len;
        jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
        current_variable->m_name = (const char *)je_mem_alloc((size_t)uniform_name_len + 1);
        jeecs_file_read(const_cast<char *>(current_variable->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
        const_cast<char *>(current_variable->m_name)[(size_t)uniform_name_len] = 0;

        // 4.1.2 read type
        jeecs_file_read(&current_variable->m_uniform_type, sizeof(jegl_shader::uniform_type), 1, cache_file);

        // 4.1.3 read data
        static_assert(sizeof(current_variable->mat4x4) == sizeof(float[4][4]));
        jeecs_file_read(&current_variable->mat4x4, sizeof(float[4][4]), 1, cache_file);

        current_variable->m_index = jeecs::typing::INVALID_UINT32;
        current_variable->m_updated = false;

        last_create_variable = current_variable;
        current_variable->m_next = nullptr;
    }

    // 4.2 read uniform block informs
    uint64_t custom_uniform_block_count;
    jeecs_file_read(&custom_uniform_block_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_custom_uniform_blocks = nullptr;

    jegl_shader::uniform_blocks *last_create_block = nullptr;
    for (uint64_t i = 0; i < custom_uniform_block_count; ++i)
    {
        jegl_shader::uniform_blocks *current_block = new jegl_shader::uniform_blocks();
        if (_shader->m_custom_uniform_blocks == nullptr)
            _shader->m_custom_uniform_blocks = current_block;

        if (last_create_block != nullptr)
            last_create_block->m_next = current_block;

        // 4.2.1 read name
        uint64_t uniform_name_len;
        jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
        current_block->m_name = (const char *)je_mem_alloc((size_t)uniform_name_len + 1);
        jeecs_file_read(const_cast<char *>(current_block->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
        const_cast<char *>(current_block->m_name)[(size_t)uniform_name_len] = 0;

        // 4.2.2 read binding place
        static_assert(sizeof(current_block->m_specify_binding_place) == sizeof(uint32_t));
        jeecs_file_read(&current_block->m_specify_binding_place, sizeof(uint32_t), 1, cache_file);

        last_create_block = current_block;
        current_block->m_next = nullptr;
    }

    // 4.3 read shader vertex layout
    uint64_t vertex_in_count;
    jeecs_file_read(&vertex_in_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_vertex_in_count = (size_t)vertex_in_count;
    _shader->m_vertex_in = new jegl_shader::vertex_in_variables[_shader->m_vertex_in_count];

    jeecs_file_read(_shader->m_vertex_in, sizeof(jegl_shader::vertex_in_variables),
                    _shader->m_vertex_in_count, cache_file);

    // 4.4 read sampler informations;
    uint64_t sampler_count;
    jeecs_file_read(&sampler_count, sizeof(uint64_t), 1, cache_file);
    _shader->m_sampler_count = (size_t)sampler_count;
    _shader->m_sampler_methods = new jegl_shader::sampler_method[(size_t)sampler_count];
    for (uint64_t i = 0; i < sampler_count; ++i)
    {
        auto &sampler = _shader->m_sampler_methods[i];
        jeecs_file_read(&sampler.m_min, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_mag, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_mip, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_uwrap, sizeof(jegl_shader::wrap_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_vwrap, sizeof(jegl_shader::wrap_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_sampler_id, sizeof(uint32_t), 1, cache_file);
        jeecs_file_read(&sampler.m_pass_id_count, sizeof(uint64_t), 1, cache_file);

        sampler.m_pass_ids = new uint32_t[(size_t)sampler.m_pass_id_count];
        jeecs_file_read(sampler.m_pass_ids, sizeof(uint32_t), (size_t)sampler.m_pass_id_count, cache_file);
    }

    jeecs_file_close(cache_file);

    shader->m_path = jeecs::basic::make_new_string(path);
    return shader;
}
void _jegl_create_shader_cache(jegl_resource *shader_resource, wo_integer_t virtual_file_crc64)
{
    assert(shader_resource->m_path != nullptr && shader_resource->m_raw_shader_data && shader_resource->m_type == jegl_resource::type::SHADER);

    if (auto *cachefile = jeecs_create_cache_file(
            shader_resource->m_path,
            SHADER_CACHE_VERSION,
            virtual_file_crc64))
    {
        auto *raw_shader_data = shader_resource->m_raw_shader_data;

        // 1. write shader generated source to cache
#ifdef JE_ENABLE_GLSL_GENERATION
        const uint64_t vertex_glsl_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_glsl_src);
        const uint64_t fragment_glsl_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_glsl_src);

        jeecs_write_cache_file(&vertex_glsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_glsl_src, sizeof(char), (size_t)vertex_glsl_src_len, cachefile);
        jeecs_write_cache_file(&fragment_glsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_glsl_src, sizeof(char), (size_t)fragment_glsl_src_len, cachefile);
#else
        const uint64_t glsl_zero = 0;
        jeecs_write_cache_file(&glsl_zero, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(&glsl_zero, sizeof(uint64_t), 1, cachefile);
#endif

#ifdef JE_ENABLE_HLSL_GENERATION
        const uint64_t vertex_hlsl_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_hlsl_src);
        const uint64_t fragment_hlsl_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_hlsl_src);

        jeecs_write_cache_file(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_hlsl_src, sizeof(char), (size_t)vertex_hlsl_src_len, cachefile);
        jeecs_write_cache_file(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_hlsl_src, sizeof(char), (size_t)fragment_hlsl_src_len, cachefile);
#else
        const uint64_t hlsl_zero = 0;
        jeecs_write_cache_file(&hlsl_zero, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(&hlsl_zero, sizeof(uint64_t), 1, cachefile);
#endif

#ifdef JE_ENABLE_SPIRV_GENERATION
        const uint64_t vertex_spirv_src_len = (uint64_t)raw_shader_data->m_vertex_spirv_count * sizeof(jegl_shader::spir_v_code_t);
        const uint64_t fragment_spirv_src_len = (uint64_t)raw_shader_data->m_fragment_spirv_count * sizeof(jegl_shader::spir_v_code_t);

        jeecs_write_cache_file(&vertex_spirv_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_spirv_codes, sizeof(char), (size_t)vertex_spirv_src_len, cachefile);
        jeecs_write_cache_file(&fragment_spirv_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_spirv_codes, sizeof(char), (size_t)fragment_spirv_src_len, cachefile);
#else
        const uint64_t spirv_zero = 0;
        jeecs_write_cache_file(&spirv_zero, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(&spirv_zero, sizeof(uint64_t), 1, cachefile);
#endif
        // 2. write shader config to cache
        /*
            depth_test_method   m_depth_test;
            depth_mask_method   m_depth_mask;
            blend_method        m_blend_src_mode, m_blend_dst_mode;
            cull_mode           m_cull_mode;
        */
        jeecs_write_cache_file(&raw_shader_data->m_depth_test, sizeof(jegl_shader::depth_test_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_depth_mask, sizeof(jegl_shader::depth_mask_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_equation, sizeof(jegl_shader::blend_equation), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_src_mode, sizeof(jegl_shader::blend_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_dst_mode, sizeof(jegl_shader::blend_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_cull_mode, sizeof(jegl_shader::cull_mode), 1, cachefile);

        // 3. write if shader is enable to shared?
        jeecs_write_cache_file(&raw_shader_data->m_enable_to_shared, sizeof(bool), 1, cachefile);

        // 4. write shader custom variable & uniform block informs.
        uint64_t count_for_uniform = 0;
        uint64_t count_for_uniform_block = 0;

        auto *custom_uniform = raw_shader_data->m_custom_uniforms;
        while (custom_uniform)
        {
            ++count_for_uniform;
            custom_uniform = custom_uniform->m_next;
        }
        auto *custom_uniform_block = raw_shader_data->m_custom_uniform_blocks;
        while (custom_uniform_block)
        {
            ++count_for_uniform_block;
            custom_uniform_block = custom_uniform_block->m_next;
        }

        // 4.1 write shader custom variable
        jeecs_write_cache_file(&count_for_uniform, sizeof(uint64_t), 1, cachefile);
        custom_uniform = raw_shader_data->m_custom_uniforms;
        while (custom_uniform)
        {
            // 4.1.1 write name
            uint64_t uniform_name_len = (uint64_t)strlen(custom_uniform->m_name);
            jeecs_write_cache_file(&uniform_name_len, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(custom_uniform->m_name, sizeof(char), (size_t)uniform_name_len, cachefile);

            // 4.1.2 write type
            jeecs_write_cache_file(&custom_uniform->m_uniform_type, sizeof(jegl_shader::uniform_type), 1, cachefile);

            // 4.1.3 write data
            static_assert(sizeof(custom_uniform->mat4x4) == sizeof(float[4][4]));
            jeecs_write_cache_file(&custom_uniform->mat4x4, sizeof(float[4][4]), 1, cachefile);

            custom_uniform = custom_uniform->m_next;
        }

        // 4.2 write shader custom uniform block informs
        jeecs_write_cache_file(&count_for_uniform_block, sizeof(uint64_t), 1, cachefile);
        custom_uniform_block = raw_shader_data->m_custom_uniform_blocks;
        while (custom_uniform_block)
        {
            // 4.2.1 write name
            uint64_t uniform_block_name_len = (uint64_t)strlen(custom_uniform_block->m_name);
            jeecs_write_cache_file(&uniform_block_name_len, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(custom_uniform_block->m_name, sizeof(char), (size_t)uniform_block_name_len, cachefile);

            // 4.2.2 write place
            static_assert(sizeof(custom_uniform_block->m_specify_binding_place) == sizeof(uint32_t));
            jeecs_write_cache_file(&custom_uniform_block->m_specify_binding_place, sizeof(uint32_t), 1, cachefile);

            custom_uniform_block = custom_uniform_block->m_next;
        }

        // 4.3 write shader vertex layout
        uint64_t vertex_in_count = (uint64_t)raw_shader_data->m_vertex_in_count;
        jeecs_write_cache_file(&vertex_in_count, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_in, sizeof(jegl_shader::vertex_in_variables),
                               raw_shader_data->m_vertex_in_count, cachefile);

        // 4.4 write sampler informations;
        uint64_t sampler_count = (uint64_t)raw_shader_data->m_sampler_count;
        jeecs_write_cache_file(&sampler_count, sizeof(uint64_t), 1, cachefile);
        for (uint64_t i = 0; i < sampler_count; ++i)
        {
            auto &sampler = raw_shader_data->m_sampler_methods[i];
            jeecs_write_cache_file(&sampler.m_min, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_mag, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_mip, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_uwrap, sizeof(jegl_shader::wrap_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_vwrap, sizeof(jegl_shader::wrap_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_sampler_id, sizeof(uint32_t), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_pass_id_count, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(sampler.m_pass_ids, sizeof(uint32_t), (size_t)sampler.m_pass_id_count, cachefile);
        }

        jeecs_close_cache_file(cachefile);
    }
}

void jegl_shader_generate_glsl(void *shader_generator, jegl_shader *write_to_shader)
{
    shader_wrapper *shader_wrapper_ptr = (shader_wrapper *)shader_generator;

    write_to_shader->m_enable_to_shared = shader_wrapper_ptr->shader_config.m_enable_shared;
    write_to_shader->m_depth_test = shader_wrapper_ptr->shader_config.m_depth_test;
    write_to_shader->m_depth_mask = shader_wrapper_ptr->shader_config.m_depth_mask;
    write_to_shader->m_blend_equation = shader_wrapper_ptr->shader_config.m_blend_equation;
    write_to_shader->m_blend_src_mode = shader_wrapper_ptr->shader_config.m_blend_src;
    write_to_shader->m_blend_dst_mode = shader_wrapper_ptr->shader_config.m_blend_dst;
    write_to_shader->m_cull_mode = shader_wrapper_ptr->shader_config.m_cull_mode;

    scan_used_uniforms_in_wrap(shader_wrapper_ptr);

#ifdef JE_ENABLE_GLSL_GENERATION
    jeecs::shader_generator::glsl_generator _glsl_generator;
    write_to_shader->m_vertex_glsl_src = jeecs::basic::make_new_string(
        _glsl_generator.generate_vertex(shader_wrapper_ptr).c_str());
    write_to_shader->m_fragment_glsl_src = jeecs::basic::make_new_string(
        _glsl_generator.generate_fragment(shader_wrapper_ptr).c_str());
#else
    write_to_shader->m_vertex_glsl_src = nullptr;
    write_to_shader->m_fragment_glsl_src = nullptr;
#endif

#ifdef JE_ENABLE_HLSL_GENERATION
    jeecs::shader_generator::hlsl_generator _hlsl_generator;
    write_to_shader->m_vertex_hlsl_src = jeecs::basic::make_new_string(
        _hlsl_generator.generate_vertex(shader_wrapper_ptr).c_str());
    write_to_shader->m_fragment_hlsl_src = jeecs::basic::make_new_string(
        _hlsl_generator.generate_fragment(shader_wrapper_ptr).c_str());
#else
    write_to_shader->m_vertex_hlsl_src = nullptr;
    write_to_shader->m_fragment_hlsl_src = nullptr;
#endif

#ifdef JE_ENABLE_SPIRV_GENERATION
#if !defined(JE_ENABLE_HLSL_GENERATION)
#error HLSL must be enabled when SPIRV is enabled.
#endif
    write_to_shader->m_vertex_spirv_codes =
        _jegl_parse_spir_v_from_hlsl(
            write_to_shader->m_vertex_hlsl_src, false,
            &write_to_shader->m_vertex_spirv_count);
    write_to_shader->m_fragment_spirv_codes =
        _jegl_parse_spir_v_from_hlsl(
            write_to_shader->m_fragment_hlsl_src, true,
            &write_to_shader->m_fragment_spirv_count);
#else
    write_to_shader->m_vertex_spirv_codes = nullptr;
    write_to_shader->m_fragment_spirv_codes = nullptr;
    write_to_shader->m_vertex_spirv_count = 0;
    write_to_shader->m_fragment_spirv_count = 0;
#endif

    write_to_shader->m_vertex_in_count = shader_wrapper_ptr->vertex_in.size();
    write_to_shader->m_vertex_in = new jegl_shader::vertex_in_variables[write_to_shader->m_vertex_in_count];
    for (size_t i = 0; i < write_to_shader->m_vertex_in_count; ++i)
        write_to_shader->m_vertex_in[i].m_type =
            _shader_wrapper_contex::get_outside_type(shader_wrapper_ptr->vertex_in[i]);

    std::unordered_map<std::string, shader_struct_define *> _uniform_blocks;
    for (auto &struct_def : shader_wrapper_ptr->shader_struct_define_may_uniform_block)
    {
        assert(struct_def != nullptr);

        if (struct_def->binding_place != jeecs::typing::INVALID_UINT32)
        {
            assert(_uniform_blocks.find(struct_def->name) == _uniform_blocks.end());
            _uniform_blocks[struct_def->name] = struct_def;
        }
    }
    do
    {
        jegl_shader::unifrom_variables **last = &write_to_shader->m_custom_uniforms;
        for (auto &[name, uniform_info] : shader_wrapper_ptr->uniform_variables)
        {
            jegl_shader::unifrom_variables *variable = new jegl_shader::unifrom_variables();
            variable->m_next = nullptr;

            variable->m_name = jeecs::basic::make_new_string(name.c_str());
            variable->m_uniform_type = uniform_info.m_type;

            variable->m_index = jeecs::typing::INVALID_UINT32;

            auto utype = uniform_info.m_value->get_type();
            auto *init_val = (utype == jegl_shader_value::type::TEXTURE2D ||
                              utype == jegl_shader_value::type::TEXTURE2D_MS ||
                              utype == jegl_shader_value::type::TEXTURE_CUBE)
                                 ? uniform_info.m_value
                                 : uniform_info.m_value->m_uniform_init_val_may_nil;

            if (init_val != nullptr)
            {
                switch (utype)
                {
                case jegl_shader_value::type::FLOAT:
                    variable->x = init_val->m_float;
                    break;
                case jegl_shader_value::type::FLOAT2:
                    variable->x = init_val->m_float2[0];
                    variable->y = init_val->m_float2[1];
                    break;
                case jegl_shader_value::type::FLOAT3:
                    variable->x = init_val->m_float3[0];
                    variable->y = init_val->m_float3[1];
                    variable->z = init_val->m_float3[2];
                    break;
                case jegl_shader_value::type::FLOAT4:
                    variable->x = init_val->m_float4[0];
                    variable->y = init_val->m_float4[1];
                    variable->z = init_val->m_float4[2];
                    variable->w = init_val->m_float4[3];
                    break;
                case jegl_shader_value::type::FLOAT4x4:
                    memcpy(variable->mat4x4, init_val->m_float4x4, 4 * 4 * sizeof(float));
                    break;
                case jegl_shader_value::type::INTEGER:
                    variable->ix = init_val->m_integer;
                    break;
                case jegl_shader_value::type::INTEGER2:
                {
                    variable->ix = init_val->m_integer2[0];
                    variable->iy = init_val->m_integer2[1];
                    break;
                }
                case jegl_shader_value::type::INTEGER3:
                {
                    variable->ix = init_val->m_integer3[0];
                    variable->iy = init_val->m_integer3[1];
                    variable->iz = init_val->m_integer3[2];
                    break;
                }
                case jegl_shader_value::type::INTEGER4:
                {
                    variable->ix = init_val->m_integer4[0];
                    variable->iy = init_val->m_integer4[1];
                    variable->iz = init_val->m_integer4[2];
                    variable->iw = init_val->m_integer4[3];
                    break;
                }
                case jegl_shader_value::type::TEXTURE2D:
                case jegl_shader_value::type::TEXTURE2D_MS:
                case jegl_shader_value::type::TEXTURE_CUBE:
                    variable->ix = uniform_info.m_value->m_uniform_texture_channel;
                    break;
                default:
                    jeecs::debug::logerr("Unsupport uniform variable type.");
                    break;
                }
                variable->m_updated = true;
            }
            else
            {
                static_assert(sizeof(variable->mat4x4) == 16 * sizeof(float));
                memset(variable->mat4x4, 0, sizeof(variable->mat4x4));
                variable->m_updated = false;
            }

            *last = variable;
            last = &variable->m_next;
        }
    } while (false);

    do
    {
        jegl_shader::uniform_blocks **last = &write_to_shader->m_custom_uniform_blocks;
        for (auto &[_ /*useless*/, uniform_block_info] : _uniform_blocks)
        {
            jegl_shader::uniform_blocks *block = new jegl_shader::uniform_blocks();
            block->m_next = nullptr;

            assert(uniform_block_info->binding_place != jeecs::typing::INVALID_UINT32);

            block->m_name = jeecs::basic::make_new_string(uniform_block_info->name);
            block->m_specify_binding_place = uniform_block_info->binding_place;

            *last = block;
            last = &block->m_next;
        }
    } while (false);

    write_to_shader->m_sampler_count = shader_wrapper_ptr->decleared_samplers.size();
    auto *sampler_methods = new jegl_shader::sampler_method[write_to_shader->m_sampler_count];
    for (size_t i = 0; i < write_to_shader->m_sampler_count; ++i)
    {
        auto *sampler = shader_wrapper_ptr->decleared_samplers[i];
        sampler_methods[i].m_min = sampler->m_min;
        sampler_methods[i].m_mag = sampler->m_mag;
        sampler_methods[i].m_mip = sampler->m_mip;
        sampler_methods[i].m_uwrap = sampler->m_uwrap;
        sampler_methods[i].m_vwrap = sampler->m_vwrap;

        sampler_methods[i].m_sampler_id = sampler->m_sampler_id;
        sampler_methods[i].m_pass_id_count = (uint64_t)sampler->m_binded_texture_passid.size();
        auto *passids = new uint32_t[sampler->m_binded_texture_passid.size()];

        static_assert(std::is_same<decltype(sampler_methods[i].m_pass_ids), uint32_t *>::value);

        memcpy(
            passids,
            sampler->m_binded_texture_passid.data(),
            (size_t)sampler_methods[i].m_pass_id_count * sizeof(uint32_t));

        sampler_methods[i].m_pass_ids = passids;
    }
    write_to_shader->m_sampler_methods = sampler_methods;
}

void jegl_shader_free_generated_glsl(jegl_shader *write_to_shader)
{
    if (write_to_shader->m_vertex_glsl_src != nullptr)
    {
        assert(write_to_shader->m_fragment_glsl_src != nullptr);
        je_mem_free((void *)write_to_shader->m_vertex_glsl_src);
        je_mem_free((void *)write_to_shader->m_fragment_glsl_src);
    }

    if (write_to_shader->m_vertex_hlsl_src != nullptr)
    {
        assert(write_to_shader->m_fragment_hlsl_src != nullptr);
        je_mem_free((void *)write_to_shader->m_vertex_hlsl_src);
        je_mem_free((void *)write_to_shader->m_fragment_hlsl_src);
    }

    if (write_to_shader->m_vertex_spirv_codes != nullptr)
    {
        assert(write_to_shader->m_fragment_spirv_codes != nullptr);
        je_mem_free((void *)write_to_shader->m_vertex_spirv_codes);
        je_mem_free((void *)write_to_shader->m_fragment_spirv_codes);
    }

    delete[] write_to_shader->m_vertex_in;

    auto *uniform_variable_info = write_to_shader->m_custom_uniforms;
    while (uniform_variable_info)
    {
        auto *current_uniform_variable = uniform_variable_info;
        uniform_variable_info = uniform_variable_info->m_next;

        je_mem_free((void *)current_uniform_variable->m_name);

        delete current_uniform_variable;
    }
    auto *uniform_block_info = write_to_shader->m_custom_uniform_blocks;
    while (uniform_block_info)
    {
        auto *cur_uniform_block = uniform_block_info;
        uniform_block_info = uniform_block_info->m_next;

        je_mem_free((void *)cur_uniform_block->m_name);

        delete cur_uniform_block;
    }

    for (size_t i = 0; i < write_to_shader->m_sampler_count; ++i)
        delete[] write_to_shader->m_sampler_methods[i].m_pass_ids;
    delete[] write_to_shader->m_sampler_methods;
}

void jegl_shader_generator_init()
{
#if defined(JE_ENABLE_SPIRV_GENERATION)
    glslang_initialize_process();
#endif
}
void jegl_shader_generator_shutdown()
{
#if defined(JE_ENABLE_SPIRV_GENERATION)
    glslang_finalize_process();
#endif
}
