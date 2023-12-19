#define JE_IMPL
#include "jeecs.hpp"
#include <functional>
#include <variant>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "jeecs_graphic_shader_wrapper.hpp"
#include "jeecs_graphic_shader_wrapper_methods.hpp"
#include "jeecs_graphic_shader_wrapper_glsl.hpp"
#include "jeecs_graphic_shader_wrapper_hlsl.hpp"

void delete_shader_value(jegl_shader_value* shader_val)
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
            je_mem_free((void*)shader_val->m_unifrom_varname);
            if (shader_val->m_uniform_init_val_may_nil)
                delete_shader_value(shader_val->m_uniform_init_val_may_nil);
        }
        else
        {
            for (size_t i = 0; i < shader_val->m_opnums_count; ++i)
                delete_shader_value(shader_val->m_opnums[i]);
            delete[]shader_val->m_opnums;

            je_mem_free((void*)shader_val->m_opname);
        }
    }

    delete shader_val;
}

void _free_shader_value(void* shader_value)
{
    jegl_shader_value* shader_val = (jegl_shader_value*)shader_value;
    delete_shader_value(shader_val);
}

WO_API wo_api jeecs_shader_float_create(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        new jegl_shader_value(
            (float)wo_real(args + 0)), nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float2_create(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        new jegl_shader_value(
            (float)wo_real(args + 0),
            (float)wo_real(args + 1)), nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float3_create(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        new jegl_shader_value(
            (float)wo_real(args + 0),
            (float)wo_real(args + 1),
            (float)wo_real(args + 2)), nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float4_create(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        new jegl_shader_value(
            (float)wo_real(args + 0),
            (float)wo_real(args + 1),
            (float)wo_real(args + 2),
            (float)wo_real(args + 3)), nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float3x3_create(wo_vm vm, wo_value args, size_t argc)
{
    float data[9] = {};
    for (size_t i = 0; i < 9; i++)
        data[i] = (float)wo_real(args + i);
    return wo_ret_gchandle(vm,
        new jegl_shader_value(data, jegl_shader_value::FLOAT3x3),
        nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_float4x4_create(wo_vm vm, wo_value args, size_t argc)
{
    float data[16] = {};
    for (size_t i = 0; i < 16; i++)
        data[i] = (float)wo_real(args + i);
    return wo_ret_gchandle(vm,
        new jegl_shader_value(data, jegl_shader_value::FLOAT4x4),
        nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_texture2d_set_channel(wo_vm vm, wo_value args, size_t argc)
{
    jegl_shader_value* texture2d_val = (jegl_shader_value*)wo_pointer(args + 0);
    assert(texture2d_val->get_type() == jegl_shader_value::type::TEXTURE2D
        || texture2d_val->get_type() == jegl_shader_value::type::TEXTURE2D_MS
        || texture2d_val->get_type() == jegl_shader_value::type::TEXTURE_CUBE);

    texture2d_val->m_uniform_texture_channel = (int)wo_int(args + 1);
    return wo_ret_val(vm, args + 0);
}
WO_API wo_api jeecs_shader_create_rot_mat4x4(wo_vm vm, wo_value args, size_t argc)
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
    std::vector<action_node*> m_next_step;

    jegl_shader_value::type_base_t m_acceptable_types;
    calc_func_t m_reduce_const_function;

    ~action_node()
    {
        for (auto* node : m_next_step)
            delete node;
    }
    action_node*& _get_next_step(jegl_shader_value::type_base_t type)
    {
        auto fnd = std::find_if(m_next_step.begin(), m_next_step.end(),
            [&](action_node* v) {if (v->m_acceptable_types == type)return true; return false; });
        if (fnd == m_next_step.end())
            return m_next_step.emplace_back(nullptr);
        return *fnd;
    }
};

std::unordered_map<std::string, action_node*> _generate_accpetable_tree()
{
    std::unordered_map<std::string, action_node*> tree;
    for (auto& [action, type_lists] : _operation_table)
    {
        auto** act_node = &tree[action];
        if (!*act_node) *act_node = new action_node;

        for (auto& type_or_act : type_lists)
        {
            if (auto type = std::get_if<jegl_shader_value::type_base_t>(&type_or_act))
            {
                if (auto& new_act_node = (*act_node)->_get_next_step(*type))
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

const std::unordered_map<std::string, action_node*>& shader_operation_map() {
    const static std::unordered_map<std::string, action_node*>& _s = _generate_accpetable_tree();
    return _s;
}

calc_func_t* _get_reduce_func(action_node* cur_node, jegl_shader_value::type* argts, size_t argc, size_t index)
{
    if (index == argc)
    {
        if (cur_node->m_reduce_const_function)
            return &cur_node->m_reduce_const_function;
        else
            return nullptr;
    }

    assert(index < argc && cur_node);

    for (auto* next_step : cur_node->m_next_step)
    {
        if (next_step->m_acceptable_types & argts[index])
        {
            auto* fnd = _get_reduce_func(next_step, argts, argc, index + 1);
            if (fnd)
                return fnd;
        }
    }
    return nullptr;
}
calc_func_t* get_const_reduce_func(const char* op, jegl_shader_value::type* argts, size_t argc)
{
    auto& _shader_operation_map = shader_operation_map();
    auto fnd = _shader_operation_map.find(op);
    if (fnd == _shader_operation_map.end())
        return nullptr;
    auto* cur_node = fnd->second;
    return _get_reduce_func(cur_node, argts, argc, 0);
}
WO_API wo_api jeecs_shader_create_uniform_variable(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        new jegl_shader_value(
            (jegl_shader_value::type)wo_int(args + 0),
            wo_string(args + 1),
            (jegl_shader_value*)nullptr,
            (bool)wo_bool(args + 2))
        , nullptr, _free_shader_value);
}

WO_API wo_api jeecs_shader_create_uniform_variable_with_init_value(wo_vm vm, wo_value args, size_t argc)
{
    jegl_shader_value::type type = (jegl_shader_value::type)wo_int(args + 0);
    jegl_shader_value* init_value = (jegl_shader_value*)wo_pointer(args + 2);
    if (!init_value->is_init_value())
        return wo_ret_halt(vm, "Cannot do this operations: uniform variable's init value must be immediately.");
    if (init_value->get_type() != type)
        return wo_ret_halt(vm, "Cannot do this operations: uniform variable's init value must have same type.");

    return wo_ret_gchandle(vm,
        new jegl_shader_value(type, wo_string(args + 1), init_value, false)
        , nullptr, _free_shader_value);
}
WO_API wo_api jeecs_shader_create_sampler2d(wo_vm vm, wo_value args, size_t argc)
{
    auto* sampler = new shader_sampler;
    sampler->m_min = (jegl_shader::fliter_mode)wo_int(args + 0);
    sampler->m_mag = (jegl_shader::fliter_mode)wo_int(args + 1);
    sampler->m_mip = (jegl_shader::fliter_mode)wo_int(args + 2);
    sampler->m_uwrap = (jegl_shader::wrap_mode)wo_int(args + 3);
    sampler->m_vwrap = (jegl_shader::wrap_mode)wo_int(args + 4);
    sampler->m_sampler_id = (uint32_t)wo_int(args + 5);
    return wo_ret_gchandle(vm, sampler, nullptr, [](void* p) {delete(shader_sampler*)p; });
}
WO_API wo_api jeecs_shader_sampler2d_bind_texture(wo_vm vm, wo_value args, size_t argc)
{
    shader_sampler* sampler = (shader_sampler*)wo_pointer(args + 0);
    jegl_shader_value* value = (jegl_shader_value*)wo_pointer(args + 1);

    assert(value->m_binded_sampler_id == jeecs::typing::INVALID_UINT32);
    value->m_binded_sampler_id = sampler->m_sampler_id;

    sampler->m_binded_texture_passid.push_back((uint32_t)value->m_uniform_texture_channel);
    return wo_ret_void(vm);
}
WO_API wo_api jeecs_shader_apply_operation(wo_vm vm, wo_value args, size_t argc)
{
    bool result_is_const = true;
    std::vector<jegl_shader_value*> tmp_svalue;
    struct AutoRelease
    {
        std::function<void(void)> _m_func;
        AutoRelease(const std::function<void(void)>& f) :_m_func(f) {}
        ~AutoRelease() { _m_func(); }
    };
    AutoRelease auto_release([&]() {
        for (auto& tmp : tmp_svalue)
            delete_shader_value(tmp);
        });

    std::vector<jegl_shader_value::type> _types(argc - 2);
    std::vector<jegl_shader_value*> _args(argc - 2);
    for (size_t i = 2; i < argc; ++i)
    {
        auto value_type = wo_valuetype(args + i);
        if (value_type != WO_INTEGER_TYPE && value_type != WO_REAL_TYPE && value_type != WO_GCHANDLE_TYPE && value_type != WO_HANDLE_TYPE)
            return wo_ret_halt(vm, "Cannot do this operations: argument type should be number or shader_value.");

        jegl_shader_value* sval;
        if (value_type == WO_GCHANDLE_TYPE || value_type == WO_HANDLE_TYPE)
            sval = (jegl_shader_value*)wo_pointer(args + i);
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
        ;
    else
    {
        auto* reduce_func = get_const_reduce_func(operation, _types.data(), _types.size());
        if (operation[0] == '#')
        {
            // Custom method 
        }
        else if (operation[0] == '%')
        {
            // Type casting 
        }
        else
        {
            if (!reduce_func)
                return wo_ret_halt(vm, "Cannot do this operations: no matched operation with these types.");

            if (result_is_const)
            {
                auto* result = (*reduce_func)(_args.size(), _args.data());
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

    jegl_shader_value* val =
        new jegl_shader_value(result_type, operation, argc - 2);
    for (size_t i = 2; i < argc; ++i)
        val->set_used_val(i - 2, _args[i - 2]);

    return wo_ret_gchandle(vm, val, nullptr, _free_shader_value);
}

struct vertex_in_data_storage
{
    std::unordered_map<int, jegl_shader_value*> m_in_from_vao_guard;
    ~vertex_in_data_storage()
    {
        for (auto& [id, val] : m_in_from_vao_guard)
            delete_shader_value(val);

    }
    jegl_shader_value* get_val_at(size_t pos, jegl_shader_value::type type)
    {
        auto fnd = m_in_from_vao_guard.find((int)pos);
        if (fnd != m_in_from_vao_guard.end())
        {
            if (fnd->second->get_type() != type)
                return nullptr;
            return fnd->second;
        }
        auto& shval = m_in_from_vao_guard[(int)pos];
        shval = new jegl_shader_value(type);
        shval->m_shader_in_index = pos;
        return shval;
    }
};

WO_API wo_api jeecs_shader_create_vertex_in(wo_vm vm, wo_value args, size_t argc)
{
    // This function is used for debug
    return wo_ret_gchandle(vm, new vertex_in_data_storage, nullptr, [](void* ptr) {
        delete (vertex_in_data_storage*)ptr;
        });
}

WO_API wo_api jeecs_shader_get_vertex_in(wo_vm vm, wo_value args, size_t argc)
{
    vertex_in_data_storage* storage = (vertex_in_data_storage*)wo_pointer(args + 0);
    jegl_shader_value::type type = (jegl_shader_value::type)wo_int(args + 1);
    size_t pos = (size_t)wo_int(args + 2);

    auto* result = storage->get_val_at(pos, type);
    if (!result)
        return wo_ret_halt(vm, ("vertex_in[" + std::to_string(pos) + "] has been used, but type didn't match.").c_str());

    return wo_ret_pointer(vm, result);
}

WO_API wo_api jeecs_shader_set_vertex_out(wo_vm vm, wo_value args, size_t argc)
{
    jegl_shader_value* vertex_out_pos = (jegl_shader_value*)wo_pointer(args + 0);
    if (vertex_out_pos->get_type() != jegl_shader_value::FLOAT4)
        return wo_ret_halt(vm, "First value of vertex_out must be FLOAT4 for position.");

    return wo_ret_void(vm);
}

WO_API wo_api jeecs_shader_create_struct_define(wo_vm vm, wo_value args, size_t argc)
{
    shader_struct_define* block = new shader_struct_define;
    block->binding_place = jeecs::typing::INVALID_UINT32;
    block->name = wo_string(args + 0);
    return wo_ret_pointer(vm, block);
}

WO_API wo_api jeecs_shader_bind_struct_as_uniform_buffer(wo_vm vm, wo_value args, size_t argc)
{
    shader_struct_define* block = (shader_struct_define*)wo_pointer(args + 0);
    block->binding_place = (uint32_t)wo_int(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api jeecs_shader_append_struct_member(wo_vm vm, wo_value args, size_t argc)
{
    shader_struct_define* block = (shader_struct_define*)wo_pointer(args + 0);

    shader_struct_define::struct_variable variable_member_define;
    variable_member_define.type = (jegl_shader_value::type)wo_int(args + 1);
    variable_member_define.name = wo_string(args + 2);

    if (variable_member_define.type == jegl_shader_value::type::STRUCT)
    {
        wo_value elem = wo_push_empty(vm);

        wo_struct_get(elem, args + 3, 1);
        variable_member_define.struct_type_may_nil = (shader_struct_define*)wo_pointer(elem);
    }
    else
        variable_member_define.struct_type_may_nil = nullptr;

    block->variables.push_back(variable_member_define);

    return wo_ret_void(vm);
}

WO_API wo_api jeecs_shader_create_shader_value_out(wo_vm vm, wo_value args, size_t argc)
{
    wo_value voutstruct = args + 1;

    if (wo_valuetype(voutstruct) != WO_STRUCT_TYPE)
        return wo_ret_halt(vm, "'type' must struct when return from vext or frag.");

    uint16_t structsz = (uint16_t)wo_lengthof(voutstruct);

    // is vertex out, check it
    wo_value elem = wo_push_empty(vm);

    // If vertext
    if (wo_bool(args + 0))
    {
        jegl_shader_value* val = nullptr;
        if (structsz > 0)
        {
            wo_struct_get(elem, voutstruct, 0);
            val = (jegl_shader_value*)wo_pointer(elem);
        }

        if (val == nullptr || jegl_shader_value::type::FLOAT4 != val->get_type())
            return wo_ret_halt(vm, "'vert' must return a struct with first member of 'float4'.");
    }

    shader_value_outs* values = new shader_value_outs;
    values->out_values.resize(structsz);
    for (uint16_t i = 0; i < structsz; i++)
    {
        wo_struct_get(elem, voutstruct, i);
        values->out_values[i] = (jegl_shader_value*)wo_pointer(elem);
        values->out_values[i]->add_useref_count();
    }
    return wo_ret_pointer(vm, values);
}
WO_API wo_api jeecs_shader_create_fragment_in(wo_vm vm, wo_value args, size_t argc)
{
    shader_value_outs* values = (shader_value_outs*)wo_pointer(args + 0);

    uint16_t fragmentin_size = (uint16_t)values->out_values.size();
    wo_value out_struct = wo_push_struct(vm, fragmentin_size);
    wo_value elem = wo_push_empty(vm);

    for (uint16_t i = 0; i < fragmentin_size; i++)
    {
        auto* val = new jegl_shader_value(values->out_values[i]->get_type());
        val->m_shader_in_index = i;
        wo_set_gchandle(elem, vm, val, nullptr, _free_shader_value);
        wo_struct_set(out_struct, i, elem);
    }

    return wo_ret_val(vm, out_struct);
}

WO_API wo_api jeecs_shader_wrap_result_pack(wo_vm vm, wo_value args, size_t argc)
{
    wo_value elem = wo_push_empty(vm);
    wo_value val = wo_push_empty(vm);

    shader_wrapper* wrapper = new shader_wrapper(
        (shader_value_outs*)wo_pointer(args + 1),
        (shader_value_outs*)wo_pointer(args + 2));

    wo_integer_t vin_size = wo_lengthof(args + 0);

    for (wo_integer_t i = 0; i < vin_size; ++i)
    {
        wo_struct_get(elem, args + 0, (uint16_t)i);
        auto* shader_val = (jegl_shader_value*)wo_pointer(elem);

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
    wrapper->shader_config.m_blend_src = (jegl_shader::blend_method)wo_int(elem);
    wo_struct_get(elem, args + 3, 4);
    wrapper->shader_config.m_blend_dst = (jegl_shader::blend_method)wo_int(elem);
    wo_struct_get(elem, args + 3, 5);
    wrapper->shader_config.m_cull_mode = (jegl_shader::cull_mode)wo_int(elem);

    size_t ubo_count = (size_t)wo_lengthof(args + 4);
    for (size_t i = 0; i < ubo_count; ++i)
    {
        wo_arr_get(elem, args + 4, i);
        wrapper->shader_struct_define_may_uniform_block.push_back(
            (shader_struct_define*)wo_pointer(elem));
    }

    size_t sampler_count = (size_t)wo_lengthof(args + 5);
    for (size_t i = 0; i < sampler_count; ++i)
    {
        wo_arr_get(elem, args + 5, i);
        wrapper->decleared_samplers.push_back(
            (shader_sampler*)wo_pointer(elem));
    }

    size_t custom_method_count = (size_t)wo_lengthof(args + 6);
    for (size_t i = 0; i < custom_method_count; ++i)
    {
        wo_arr_get(elem, args + 6, i);
        wo_struct_get(val, elem, 0);

        auto& custom_shader_srcs = wrapper->custom_methods[wo_string(val)];

        wo_struct_get(val, elem, 1);

        wo_struct_get(elem, val, 0);
        custom_shader_srcs.m_glsl_impl = wo_string(elem);
        wo_struct_get(elem, val, 1);
        custom_shader_srcs.m_hlsl_impl = wo_string(elem);
    }

    size_t user_function_count = (size_t)wo_lengthof(args + 7);
    for (size_t i = 0; i < user_function_count; ++i)
    {
        wo_arr_get(elem, args + 7, i);
        wo_struct_get(val, elem, 0);

        auto& user_function = wrapper->user_define_functions[wo_string(val)];
        user_function.m_name = wo_string(val);

        wo_struct_get(val, elem, 2);
        user_function.m_result = (shader_value_outs*)wo_pointer(val);

        wo_struct_get(val, elem, 1);
        size_t user_function_arg_count = (size_t)wo_lengthof(val);
        for (size_t i = 0; i < user_function_arg_count; ++i)
        {
            wo_struct_get(elem, val, (uint16_t)i);
            auto* shader_val = (jegl_shader_value*)wo_pointer(elem);

            if (shader_val->is_shader_in_value() == false)
                return wo_ret_halt(vm, "Unsupport value source, should be user function in.");

            user_function.m_args.push_back(shader_val->get_type());
        }

        user_function.m_used_in_fragment = false;
        user_function.m_used_in_vertex = false;
    }

    return wo_ret_gchandle(vm,
        wrapper, nullptr,
        [](void* ptr) {
            delete (shader_wrapper*)ptr;
        });
}

const char* shader_wrapper_path = "je/shader.wo";
const char* shader_wrapper_src = R"(
// JoyEngineECS RScene shader wrapper

import woo::std;

public enum shader_value_type
{
    INIT_VALUE = 0x0000,
    CALC_VALUE = 0x0001,
    SHADER_IN_VALUE = 0x0002,
    UNIFORM_VARIABLE = 0x0004,
    UNIFORM_BLOCK_VARIABLE = 0x0008,
    //
    TYPE_MASK = 0x00000FFF0,

    FLOAT = 0x0010,
    FLOAT2 = 0x0020,
    FLOAT3 = 0x0040,
    FLOAT4 = 0x0080,

    FLOAT2x2 = 0x0100,
    FLOAT3x3 = 0x0200,
    FLOAT4x4 = 0x0400,

    TEXTURE2D = 0x0800,
    TEXTURE_CUBE = 0x1000,
    TEXTURE2D_MS = 0x2000,

    INTEGER = 0x4000,

    STRUCT = 0x8000,
}

public using float = gchandle;
public using float2 = gchandle;
public using float3 = gchandle;
public using float4 = gchandle;

public using float2x2 = gchandle;
public using float3x3 = gchandle;
public using float4x4 = gchandle;

public using texture2d = gchandle;
public using texture2dms = gchandle;
public using texturecube = gchandle;
public using integer = gchandle;

public using structure = gchandle;

public enum fliter
{
    NEAREST,
    LINEAR,
}
public enum wrap
{
    CLAMP,
    REPEAT,
}
public let NEAREST = fliter::NEAREST;
public let LINEAR = fliter::LINEAR;
public let CLAMP = wrap::CLAMP;
public let REPEAT = wrap::REPEAT;

let registered_custom_methods = {}mut: map<string, (string, string)>;

public func custom_method<ShaderResultT>(name: string, glsl_src: string, hlsl_src: string)
{
    if (registered_custom_methods->contain(name))
        return std::halt(F"Custom method '{name}' has been registered.");

    registered_custom_methods->set(name, (glsl_src, hlsl_src));
    return func(...)
    {
        return apply_operation:<ShaderResultT>("#" + name, ......);
    };
}

using sampler2d = gchandle
{
    extern("libjoyecs", "jeecs_shader_create_sampler2d")
    func _create(
        min: fliter, 
        mag: fliter, 
        mip: fliter, 
        uwarp: wrap, 
        vwarp: wrap,
        samplerid: int)=> sampler2d;

    let mut alloc_sampler_count = 0;
    let created_sampler2ds = []mut: vec<sampler2d>;

    public func create(
        min: fliter, 
        mag: fliter, 
        mip: fliter, 
        uwarp: wrap, 
        vwarp: wrap)
    {
        let s = _create(min, mag, mip, uwarp, vwarp, alloc_sampler_count);
        
        created_sampler2ds->add(s);
        alloc_sampler_count += 1;
        
        return s;
    }
    
    extern("libjoyecs", "jeecs_shader_sampler2d_bind_texture")
    func append_bind<T>(self: sampler2d, tex: T)=> void
        where tex is texture2d
        || tex is texture2dms
        || tex is texturecube;
}

private func _get_type_enum<ShaderValueT>()=> shader_value_type
{
    if (std::is_same_type:<ShaderValueT, float>)
        return shader_value_type::FLOAT;
    else if (std::is_same_type:<ShaderValueT, float2>)
        return shader_value_type::FLOAT2;
    else if (std::is_same_type:<ShaderValueT, float3>)
        return shader_value_type::FLOAT3;
    else if (std::is_same_type:<ShaderValueT, float4>)
        return shader_value_type::FLOAT4;
    else if (std::is_same_type:<ShaderValueT, float2x2>)
        return shader_value_type::FLOAT2x2;
    else if (std::is_same_type:<ShaderValueT, float3x3>)
        return shader_value_type::FLOAT3x3;
    else if (std::is_same_type:<ShaderValueT, float4x4>)
        return shader_value_type::FLOAT4x4;
    else if (std::is_same_type:<ShaderValueT, texture2d>)
        return shader_value_type::TEXTURE2D;
    else if (std::is_same_type:<ShaderValueT, texture2dms>)
        return shader_value_type::TEXTURE2D_MS;
    else if (std::is_same_type:<ShaderValueT, texturecube>)
        return shader_value_type::TEXTURE_CUBE;
    else if (std::is_same_type:<ShaderValueT, integer>)
        return shader_value_type::INTEGER;
    else if (std::is_same_type:<ShaderValueT, structure>)
        return shader_value_type::STRUCT;

    std::halt("Unknown type, not shader type?");
}

extern("libjoyecs", "jeecs_shader_apply_operation")
private func _apply_operation<ShaderResultT>(
    result_type : shader_value_type,
    operation_name : string,
    ...
)=> ShaderResultT;

private func apply_operation<ShaderResultT>(operation_name:string, ...) 
    => ShaderResultT
{
    return _apply_operation:<ShaderResultT>(
                _get_type_enum:<ShaderResultT>(), 
                operation_name, ......);
}

extern("libjoyecs", "jeecs_shader_create_uniform_variable")
private func _uniform<ShaderResultT>(
    result_type : shader_value_type,
    uniform_name : string,
    is_uniform_block : bool
)=> ShaderResultT;

extern("libjoyecs", "jeecs_shader_create_uniform_variable_with_init_value")
private func _uniform_with_init<ShaderResultT>(
    result_type : shader_value_type,
    uniform_name : string,
    init_value : ShaderResultT
)=> ShaderResultT;

public func uniform_texture<ShaderResultT>(uniform_name:string, sampler: sampler2d, pass: int)=> ShaderResultT
    where std::declval:<ShaderResultT>() is texture2d
        || std::declval:<ShaderResultT>() is texture2dms
        || std::declval:<ShaderResultT>() is texturecube;
{
    extern("libjoyecs", "jeecs_shader_texture2d_set_channel")
        public func channel<T>(self: T, pass: int)=> T;
    
    let tex = channel(_uniform:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, false), pass);
    sampler->append_bind(tex);
    return tex;
}

public func uniform<ShaderResultT>(uniform_name:string, init_value: ShaderResultT)=> ShaderResultT
{
    return _uniform_with_init:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, init_value);
}

public func shared_uniform<ShaderResultT>(uniform_name:string)=> ShaderResultT
{
    return _uniform:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, true);
}

extern("libjoyecs", "jeecs_shader_create_rot_mat4x4")
public func rotation(x:real, y:real, z:real)=> float4x4;

using vertex_in = handle;
namespace vertex_in
{
    extern("libjoyecs", "jeecs_shader_create_vertex_in")
    public func create()=> vertex_in;

    extern("libjoyecs", "jeecs_shader_get_vertex_in")
    private func _in<ValueT>(self:vertex_in, type:shader_value_type, id:int)=> ValueT;

    public func in<ValueT>(self:vertex_in, id:int)=> ValueT
    {
        return self->_in:<ValueT>(_get_type_enum:<ValueT>(), id) as ValueT;
    }
}

using vertex_out = handle; // nogc! will free by shader_wrapper
namespace vertex_out
{   
    func create<VertexOutT>(vout : VertexOutT)=> vertex_out
    {
        extern("libjoyecs", "jeecs_shader_create_shader_value_out")
        func _create_shader_out<VertexOutT>(is_vertex: bool, out_val: VertexOutT)=> vertex_out;

        return _create_shader_out(true, vout);
    }
}

using fragment_in = handle;
namespace fragment_in
{
    public func create<VertexOutT>(data_from_vert: vertex_out)=> VertexOutT
    {
        extern("libjoyecs", "jeecs_shader_create_fragment_in")
        func _parse_vertex_out_to_struct<VertexOutT>(vout: vertex_out)=> VertexOutT;

        return _parse_vertex_out_to_struct:<VertexOutT>(data_from_vert);
    }
}

using fragment_out = handle; // nogc! will free by shader_wrapper
namespace fragment_out
{
    public func create<FragementOutT>(fout : FragementOutT)=> fragment_out
    {
        extern("libjoyecs", "jeecs_shader_create_shader_value_out")
        func _create_shader_out<FragementOutT>(is_vertex: bool, out_val: FragementOutT)=> fragment_out;

        return _create_shader_out(false, fout);
    }
}

using function = struct{
    m_name: string,
    m_function_in: dynamic, // ArgumentTs
    m_result_out: fragment_out,
}
{
    let _generate_functions = []mut: vec<function>;
    public func register<ArgumentTs, ResultT>(name: string, vin: ArgumentTs, result: ResultT)
    {
        _generate_functions->add(
            function{
                m_name = "je_shader_uf_" + name, 
                m_function_in = vin: dynamic, 
                m_result_out = fragment_out::create((result,))
            });
        return func(...){
            return apply_operation:<ResultT>("#je_shader_uf_" + name, ......);
        };
    }
}

namespace float
{
    public let zero = float::new(0.);
    public let one = float::new(1.);

    extern("libjoyecs", "jeecs_shader_float_create")
    public func new(init_val:real)=> float;

    public func create(...)=> float{return apply_operation:<float>("float", ......);}

    public func operator + <T>(a:float, b:T)=> float
        where b is float || b is real;
    {
        return apply_operation:<float>("+", a, b);
    }
    public func operator - <T>(a:float, b:T)=> float
        where b is float || b is real;
    {
        return apply_operation:<float>("-", a, b);
    }
    public func operator * <T>(a:float, b:T)
        where b is real 
            || b is float 
            || b is float2 
            || b is float3 
            || b is float4;
    {
        if (b is float || b is real)
            return apply_operation:<float>("*", a, b);
        else
            return b * a;
    }

    public func operator / <T>(a:float, b:T)=> float
        where b is float || b is real;
    {
        return apply_operation:<float>("/", a, b);
    }
}
namespace float2
{
    public let zero = float2::new(0., 0.);
    public let one = float2::new(1., 1.);

    extern("libjoyecs", "jeecs_shader_float2_create")
    public func new(x:real, y:real)=> float2;

    public func create(...)=> float2{return apply_operation:<float2>("float2", ......);}

    public func x(self:float2)=> float{return apply_operation:<float>(".x", self);}
    public func y(self:float2)=> float{return apply_operation:<float>(".y", self);}
    public func xy(self:float2)=> float2{return apply_operation:<float2>(".xy", self);}
    public func yx(self:float2)=> float2{return apply_operation:<float2>(".yx", self);}

    public func operator + (a:float2, b:float2)=> float2
    {
        return apply_operation:<float2>("+", a, b);
    }
    public func operator - (a:float2, b:float2)=> float2
    {
        return apply_operation:<float2>("-", a, b);
    }
    public func operator * <T>(a:float2, b:T)=> float2
        where b is real || b is float || b is float2;
    {
        return apply_operation:<float2>("*", a, b);
    }

    public func operator / <T>(a:float2, b: T)=> float2
        where b is real || b is float || b is float2;
    {
        return apply_operation:<float2>("/", a, b);
    }
}
namespace float3
{
    public let zero = float3::new(0., 0., 0.);
    public let one = float3::new(1., 1., 1.);

    extern("libjoyecs", "jeecs_shader_float3_create")
    public func new(x:real, y:real, z:real)=> float3;

    public func create(...)=> float3{return apply_operation:<float3>("float3", ......);}

    public func x(self:float3)=> float{return apply_operation:<float>(".x", self);}
    public func y(self:float3)=> float{return apply_operation:<float>(".y", self);}
    public func z(self:float3)=> float{return apply_operation:<float>(".z", self);}
    public func xy(self:float3)=> float2{return apply_operation:<float2>(".xy", self);}
    public func yz(self:float3)=> float2{return apply_operation:<float2>(".yz", self);}
    public func xz(self:float3)=> float2{return apply_operation:<float2>(".xz", self);}
    public func yx(self:float3)=> float2{return apply_operation:<float2>(".yx", self);}
    public func zy(self:float3)=> float2{return apply_operation:<float2>(".zy", self);}
    public func zx(self:float3)=> float2{return apply_operation:<float2>(".zx", self);}
    public func xyz(self:float3)=> float3{return apply_operation:<float3>(".xyz", self);}
    public func xzy(self:float3)=> float3{return apply_operation:<float3>(".xzy", self);}
    public func yxz(self:float3)=> float3{return apply_operation:<float3>(".yxz", self);}
    public func yzx(self:float3)=> float3{return apply_operation:<float3>(".yzx", self);}
    public func zxy(self:float3)=> float3{return apply_operation:<float3>(".zxy", self);}
    public func zyx(self:float3)=> float3{return apply_operation:<float3>(".zyx", self);}

    public func operator + (a:float3, b:float3)=> float3
    {
        return apply_operation:<float3>("+", a, b);
    }
    public func operator - (a:float3, b:float3)=> float3
    {
        return apply_operation:<float3>("-", a, b);
    }
    public func operator * <T>(a:float3, b:T)=> float3
        where b is real || b is float || b is float3 || b is float3x3;
    {
        return apply_operation:<float3>("*", a, b);
    }

    public func operator / <T>(a:float3, b: T)=> float3
        where b is real || b is float || b is float3;
    {
        return apply_operation:<float3>("/", a, b);
    }
}
)" R"(
namespace float4
{
    public let zero = float4::new(0., 0., 0., 0.);
    public let one = float4::new(1., 1., 1., 1.);

    extern("libjoyecs", "jeecs_shader_float4_create")
    public func new(x:real, y:real, z:real, w:real)=> float4;

    public func create(...)=> float4{return apply_operation:<float4>("float4", ......);}

    public func x(self:float4)=> float{return apply_operation:<float>(".x", self);}
    public func y(self:float4)=> float{return apply_operation:<float>(".y", self);}
    public func z(self:float4)=> float{return apply_operation:<float>(".z", self);}
    public func w(self:float4)=> float{return apply_operation:<float>(".w", self);}

    public func xy(self:float4)=> float2{return apply_operation:<float2>(".xy", self);}
    public func yz(self:float4)=> float2{return apply_operation:<float2>(".yz", self);}
    public func xz(self:float4)=> float2{return apply_operation:<float2>(".xz", self);}
    public func yx(self:float4)=> float2{return apply_operation:<float2>(".yx", self);}
    public func zy(self:float4)=> float2{return apply_operation:<float2>(".zy", self);}
    public func zx(self:float4)=> float2{return apply_operation:<float2>(".zx", self);}
    public func xw(self:float4)=> float2{return apply_operation:<float2>(".xw", self);}
    public func wx(self:float4)=> float2{return apply_operation:<float2>(".wx", self);}
    public func yw(self:float4)=> float2{return apply_operation:<float2>(".yw", self);}
    public func wy(self:float4)=> float2{return apply_operation:<float2>(".wy", self);}
    public func zw(self:float4)=> float2{return apply_operation:<float2>(".zw", self);}
    public func wz(self:float4)=> float2{return apply_operation:<float2>(".wz", self);}

    public func xyz(self:float4)=> float3{return apply_operation:<float3>(".xyz", self);}
    public func xzy(self:float4)=> float3{return apply_operation:<float3>(".xzy", self);}
    public func yxz(self:float4)=> float3{return apply_operation:<float3>(".yxz", self);}
    public func yzx(self:float4)=> float3{return apply_operation:<float3>(".yzx", self);}
    public func zxy(self:float4)=> float3{return apply_operation:<float3>(".zxy", self);}
    public func zyx(self:float4)=> float3{return apply_operation:<float3>(".zyx", self);}
    public func wyz(self:float4)=> float3{return apply_operation:<float3>(".wyz", self);}
    public func wzy(self:float4)=> float3{return apply_operation:<float3>(".wzy", self);}
    public func ywz(self:float4)=> float3{return apply_operation:<float3>(".ywz", self);}
    public func yzw(self:float4)=> float3{return apply_operation:<float3>(".yzw", self);}
    public func zwy(self:float4)=> float3{return apply_operation:<float3>(".zwy", self);}
    public func zyw(self:float4)=> float3{return apply_operation:<float3>(".zyw", self);}
    public func xwz(self:float4)=> float3{return apply_operation:<float3>(".xwz", self);}
    public func xzw(self:float4)=> float3{return apply_operation:<float3>(".xzw", self);}
    public func wxz(self:float4)=> float3{return apply_operation:<float3>(".wxz", self);}
    public func wzx(self:float4)=> float3{return apply_operation:<float3>(".wzx", self);}
    public func zxw(self:float4)=> float3{return apply_operation:<float3>(".zxw", self);}
    public func zwx(self:float4)=> float3{return apply_operation:<float3>(".zwx", self);}
    public func xyw(self:float4)=> float3{return apply_operation:<float3>(".xyw", self);}
    public func xwy(self:float4)=> float3{return apply_operation:<float3>(".xwy", self);}
    public func yxw(self:float4)=> float3{return apply_operation:<float3>(".yxw", self);}
    public func ywx(self:float4)=> float3{return apply_operation:<float3>(".ywx", self);}
    public func wxy(self:float4)=> float3{return apply_operation:<float3>(".wxy", self);}
    public func wyx(self:float4)=> float3{return apply_operation:<float3>(".wyx", self);}

    public func xyzw(self:float4)=> float4{return apply_operation:<float4>(".xyzw", self);}
    public func xzyw(self:float4)=> float4{return apply_operation:<float4>(".xzyw", self);}
    public func yxzw(self:float4)=> float4{return apply_operation:<float4>(".yxzw", self);}
    public func yzxw(self:float4)=> float4{return apply_operation:<float4>(".yzxw", self);}
    public func zxyw(self:float4)=> float4{return apply_operation:<float4>(".zxyw", self);}
    public func zyxw(self:float4)=> float4{return apply_operation:<float4>(".zyxw", self);}
    public func wyzx(self:float4)=> float4{return apply_operation:<float4>(".wyzx", self);}
    public func wzyx(self:float4)=> float4{return apply_operation:<float4>(".wzyx", self);}
    public func ywzx(self:float4)=> float4{return apply_operation:<float4>(".ywzx", self);}
    public func yzwx(self:float4)=> float4{return apply_operation:<float4>(".yzwx", self);}
    public func zwyx(self:float4)=> float4{return apply_operation:<float4>(".zwyx", self);}
    public func zywx(self:float4)=> float4{return apply_operation:<float4>(".zywx", self);}
    public func xwzy(self:float4)=> float4{return apply_operation:<float4>(".xwzy", self);}
    public func xzwy(self:float4)=> float4{return apply_operation:<float4>(".xzwy", self);}
    public func wxzy(self:float4)=> float4{return apply_operation:<float4>(".wxzy", self);}
    public func wzxy(self:float4)=> float4{return apply_operation:<float4>(".wzxy", self);}
    public func zxwy(self:float4)=> float4{return apply_operation:<float4>(".zxwy", self);}
    public func zwxy(self:float4)=> float4{return apply_operation:<float4>(".zwxy", self);}
    public func xywz(self:float4)=> float4{return apply_operation:<float4>(".xywz", self);}
    public func xwyz(self:float4)=> float4{return apply_operation:<float4>(".xwyz", self);}
    public func yxwz(self:float4)=> float4{return apply_operation:<float4>(".yxwz", self);}
    public func ywxz(self:float4)=> float4{return apply_operation:<float4>(".ywxz", self);}
    public func wxyz(self:float4)=> float4{return apply_operation:<float4>(".wxyz", self);}
    public func wyxz(self:float4)=> float4{return apply_operation:<float4>(".wyxz", self);}

    public func operator + (a:float4, b:float4)=> float4
    {
        return apply_operation:<float4>("+", a, b);
    }
    public func operator - (a:float4, b:float4)=> float4
    {
        return apply_operation:<float4>("-", a, b);
    }
    public func operator * <T>(a:float4, b:T)=> float4
        where b is real || b is float || b is float4 || b is float4x4;
    {
        return apply_operation:<float4>("*", a, b);
    }

    public func operator /<T>(a:float4, b: T)=> float4
        where b is real || b is float || b is float4;
    {
        return apply_operation:<float4>("/", a, b);
    }
}

namespace float4x4
{
    public let unit = float4x4::new(
        1., 0., 0., 0.,
        0., 1., 0., 0.,
        0., 0., 1., 0.,
        0., 0., 0., 1.);
    extern("libjoyecs", "jeecs_shader_float4x4_create")
    public func new(
        p00:real, p01:real, p02:real, p03:real,
        p10:real, p11:real, p12:real, p13:real,
        p20:real, p21:real, p22:real, p23:real,
        p30:real, p31:real, p32:real, p33:real)=> float4x4;

    public func create(...)=> float4x4{return apply_operation:<float4x4>("float4x4", ......);}

    public func float3x3(self: float4x4)=> float3x3
    {
        return apply_operation:<float3x3>("%float3x3", self);
    }

    public func operator * <T>(a:float4x4, b:T)=> T
        where b is float4 || b is float4x4;
    {
        if (b is float4x4)
            return apply_operation:<float4x4>("*", a, b);
        else
            return apply_operation:<float4>("*", a, b);
    }
}

namespace float3x3
{
    public let unit = float3x3::new(
        1., 0., 0.,
        0., 1., 0.,
        0., 0., 1.,);

    extern("libjoyecs", "jeecs_shader_float3x3_create")
    public func new(
        p00:real, p01:real, p02:real,
        p10:real, p11:real, p12:real,
        p20:real, p21:real, p22:real)=> float3x3;

    public func create(...)=> float3x3{return apply_operation:<float3x3>("float3x3", ......);}

    public func operator * <T>(a:float3x3, b:T)=> T
        where b is float3 || b is float3x3;
    {
        if (b is float3x3)
            return apply_operation:<float3x3>("*", a, b);
        else
            return apply_operation:<float3>("*", a, b);
    }
}

namespace shader
{
    using shader_wrapper = gchandle;

    using ShaderConfig = struct {
        shared    : mut bool,
        ztest     : mut ZConfig,
        zwrite    : mut GConfig,
        blend_src : mut BlendConfig,
        blend_dst : mut BlendConfig,
        cull      : mut CullConfig
    };
    let configs = ShaderConfig
    {
        shared = mut false,
        ztest = mut LESS,
        zwrite = mut ENABLE,
        blend_src = mut ONE,
        blend_dst = mut ZERO,
        cull = mut NONE,
    };

    let struct_uniform_blocks_decls = []mut: vec<struct_define>;

    extern("libjoyecs", "jeecs_shader_wrap_result_pack")
    private func _wraped_shader<VertexInType>(
        vertin: VertexInType,
        vertout: vertex_out, 
        fragout: fragment_out, 
        shader_config: ShaderConfig,
        struct_or_uniform_block_decl_list: array<struct_define>,
        sampler_defines: array<sampler2d>,
        custom_methods: array<(string, (string, string))>,
        function_declear: array<function>)=> shader_wrapper;

    private extern func generate()
    {
        let v_in = _JE_BUILT_VAO_STRUCT(vertex_in::create());

        // 'v_out' is a struct with member of shader variable as vertex outputs.
        let v_out = vert(v_in);
        
        // 'vertex_out' will analyze struct, then 'fragment_in' will build a new struct
        let vertext_out_result = vertex_out::create(v_out);
        let f_in = fragment_in::create:<typeof(v_out)>(vertext_out_result);
    
        // 'f_out' is a struct with output shader variable.
        let f_out = frag(f_in);
        let fragment_out_result = fragment_out::create(f_out);

        return _wraped_shader(
            v_in,
            vertext_out_result,
            fragment_out_result,
            configs,
            struct_uniform_blocks_decls->toarray,
            sampler2d::created_sampler2ds->toarray,
            registered_custom_methods->unmapping,
            function::_generate_functions->toarray);
    }
}

public func texture(tex:texture2d, uv:float2)=> float4
{
    return apply_operation:<float4>("texture", tex, uv);
}

public func step<T, U>(a: T, b: U)=> float
    where a is float || a is real
        , b is float || b is real;
{
    return apply_operation:<float>("step", a, b);
}

public func uvtrans(uv: float2, tiling: float2, offset: float2)
{
    return uv * tiling + offset;
}

let _uvframebuf = custom_method:<float2>("JEBUILTIN_Uvframebuffer",
@"
vec2 JEBUILTIN_Uvframebuffer(vec2 v)
{
    return v;
}
"@,
@"
float2 JEBUILTIN_Uvframebuffer(float2 v)
{
    return float2(v.x, 1.0 - v.y);
}
"@);
public func uvframebuf(uv: float2)
{
     return _uvframebuf(uv);
}

// Math functions

let is_glvalue<T> = 
    std::declval:<T>() is real
    || std::declval:<T>() is float
    || std::declval:<T>() is float2
    || std::declval:<T>() is float3
    || std::declval:<T>() is float4
;

let is_float<T> = 
    std::declval:<T>() is real
    || std::declval:<T>() is float
;

public func lerp<T>(a: T, b: T, uv:float)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("lerp", a, b, uv);
}

public func sin<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("sin", a);
}
public func cos<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("cos", a);
}
public func tan<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("tan", a);
}

public func asin<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("asin", a);
}
public func acos<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("acos", a);
}
public func atan<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("atan", a);
}
public func atan2<AT, BT>(a: AT, b: BT)=> float
    where a is float || a is real
        , b is float || b is real;
{
    return apply_operation:<float>("atan2", a, b);
}

public func abs<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("abs", a);
}

public func negative<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("-", a);
}

public func pow<T>(a: T, b: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("pow", a, b);
}

public func max<T>(a: T, b: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("max", a, b);
}

public func min<T>(a: T, b: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("min", a, b);
}

public func normalize<T>(a: T)=> T
    where is_glvalue:<T> && !is_float:<T>;
{
    return apply_operation:<T>("normalize", a);
}

let _movement = custom_method:<float3>("JEBUILTIN_Movement",
@"
vec3 JEBUILTIN_Movement(mat4 trans)
{
    return vec3(trans[3][0], trans[3][1], trans[3][2]);
}
"@,
@"
float3 JEBUILTIN_Movement(float4x4 trans)
{
    return float3(trans[0].w, trans[1].w, trans[2].w);
}
"@);
public func movement(trans4x4: float4x4)=> float3
{
    return _movement(trans4x4);
}

public func clamp<T, FT>(a: T, b: FT, c: FT)=> T
    where is_glvalue:<T> && (is_float:<FT> || b is T);
{
    return apply_operation:<T>("clamp", a, b, c);
}

public func dot<T>(a: T, b: T)=> float
    where is_glvalue:<T> && !(is_float:<T>);
{
    return apply_operation:<float>("dot", a, b);
}

public func cross(a: float3, b: float3)=> float3
{
    return apply_operation:<float3>("cross", a, b);
}

public func length<T>(a: T)=> float
    where is_glvalue:<T> && !(is_float:<T>);
{
    return apply_operation:<float>("length", a);
}

public func distance<T>(a: T, b: T)=> float
    where is_glvalue:<T> && !(is_float:<T>);
{
    return apply_operation:<float>("distance", a, b);
}

// Engine builtin function
let _alphatest = custom_method:<float4>("JEBUILTIN_AlphaTest",
@"
vec4 JEBUILTIN_AlphaTest(vec4 color)
{
    if (color.a <= 0.0)
        discard;
    return color;
}
"@,
@"
float4 JEBUILTIN_AlphaTest(float4 color)
{
    if (color.a <= 0.0)
        clip(-1.0);
    return color;
}
"@);
public func alphatest(colf4: float4)=> float4
{
    return _alphatest(colf4);
}

)"
R"(
enum ZConfig
{
    OFF = 0,
    NEVER,
    LESS,       /* DEFAULT */
    EQUAL,
    LESS_EQUAL,
    GREATER,
    NOT_EQUAL,
    GREATER_EQUAL,
    ALWAYS,
}
public let OFF = ZConfig::OFF;
public let NEVER = ZConfig::NEVER;
public let LESS = ZConfig::LESS;
public let EQUAL = ZConfig::EQUAL;
public let GREATER = ZConfig::GREATER;
public let LESS_EQUAL = ZConfig::LESS_EQUAL;
public let NOT_EQUAL = ZConfig::NOT_EQUAL;
public let GREATER_EQUAL = ZConfig::GREATER_EQUAL;
public let ALWAYS = ZConfig::ALWAYS;

enum GConfig
{
    DISABLE = 0,
    ENABLE
}
public let DISABLE = GConfig::DISABLE;
public let ENABLE = GConfig::ENABLE;

enum BlendConfig
{
    ZERO = 0,       /* DEFAULT SRC = ONE, DST = ZERO (DISABLE BLEND.) */
    ONE,

    SRC_COLOR,
    SRC_ALPHA,

    ONE_MINUS_SRC_ALPHA,
    ONE_MINUS_SRC_COLOR,

    DST_COLOR,
    DST_ALPHA,

    ONE_MINUS_DST_ALPHA,
    ONE_MINUS_DST_COLOR,
}

public let ZERO = BlendConfig::ZERO;
public let ONE = BlendConfig::ONE;
public let SRC_COLOR = BlendConfig::SRC_COLOR;
public let SRC_ALPHA = BlendConfig::SRC_ALPHA;
public let ONE_MINUS_SRC_ALPHA = BlendConfig::ONE_MINUS_SRC_ALPHA;
public let ONE_MINUS_SRC_COLOR = BlendConfig::ONE_MINUS_SRC_COLOR;
public let DST_COLOR = BlendConfig::DST_COLOR;
public let DST_ALPHA = BlendConfig::DST_ALPHA;
public let ONE_MINUS_DST_ALPHA = BlendConfig::ONE_MINUS_DST_ALPHA;
public let ONE_MINUS_DST_COLOR = BlendConfig::ONE_MINUS_DST_COLOR;

enum CullConfig
{
    NONE = 0,       /* DEFAULT */
    FRONT,
    BACK,
}

public let NONE = CullConfig::NONE;
public let FRONT = CullConfig::FRONT;
public let BACK = CullConfig::BACK;

public func SHARED(enable: bool)
{
    shader::configs.shared = enable;
}

public func ZTEST(zconfig: ZConfig)
{
    shader::configs.ztest = zconfig;
}

public func ZWRITE(zwrite: GConfig)
{
    shader::configs.zwrite = zwrite;
}

public func BLEND(src: BlendConfig, dst: BlendConfig)
{
    shader::configs.blend_src = src;
    shader::configs.blend_dst = dst;
}

public func CULL(cull: CullConfig)
{
    shader::configs.cull = cull;
}
)" R"(
#macro VAO_STRUCT
{
    let eat_token = func(expect_name: string, expect_type: std::token_type)
                    {
                        let (token, out_result) = lexer->nexttoken;
                        if (token != expect_type)
                            lexer->error(F"Expect '{expect_name}' here, but get '{out_result}'");
                        return out_result;
                    };
    let try_eat_token = func(expect_type: std::token_type)=> option<string>
                    {
                        let (token, out_result) = lexer->peektoken();
                        if (token != expect_type)
                            return option::none;
                        do lexer->nexttoken;
                        return option::value(out_result);
                    };

    // using VAO_STRUCT! vin { ...
    // 0. Get struct name, then eat '{'
    let vao_struct_name = eat_token("IDENTIFIER", std::token_type::l_identifier);

    do eat_token("{", std::token_type::l_left_curly_braces);
    
    // 1. Get struct item name.
    let struct_infos = []mut: vec<(string, string)>;
    while (true)
    {
        if (try_eat_token(std::token_type::l_right_curly_braces)->has())
            // Meet '}', end work!
            break;

        let struct_member = eat_token("IDENTIFIER", std::token_type::l_identifier);
        do eat_token(":", std::token_type::l_typecast);

        // Shader type only have a identifier and without template.
        let struct_shader_type = eat_token("IDENTIFIER", std::token_type::l_identifier);

        struct_infos->add((struct_member, struct_shader_type));

        if (!try_eat_token(std::token_type::l_comma)->has())
        {
            do eat_token("}", std::token_type::l_right_curly_braces);
            break;
        }
    }
    // End, need a ';' here.
    do eat_token(";", std::token_type::l_semicolon);

    //  OK We have current vao struct info, built struct out
    let mut out_struct_decl = F"public using {vao_struct_name} = struct {"{"}\n";

    for(let _, (vao_member_name, vao_shader_type) : struct_infos)
        out_struct_decl += F"{vao_member_name} : {vao_shader_type}, \n";

    out_struct_decl += "};\n";

    // Last step, we generate "_JE_BUILT_VAO" function here.
    out_struct_decl += 
        @"
        public func _JE_BUILT_VAO_STRUCT(vertex_data_in: vertex_in)
        {
            return "@ 
        + vao_struct_name + " {\n";

    let mut vinid = 0;

    for(let _, (vao_member_name, vao_shader_type) : struct_infos)
    {
        out_struct_decl += F"{vao_member_name} = vertex_data_in->in:<{vao_shader_type}>({vinid}), \n";
        vinid += 1;
    }
    out_struct_decl += "};}\n";

    return out_struct_decl;
}

namespace structure
{
    public func get<ElemT>(self: structure, name: string)
    {
        return apply_operation:<ElemT>("." + name, self);
    }
}

using struct_define = handle
{
    extern("libjoyecs", "jeecs_shader_append_struct_member")
    func _append_member(self: struct_define, type: shader_value_type, name: string, st: option<struct_define>)=> void;

    public func create(name: string)
    {
        extern("libjoyecs", "jeecs_shader_create_struct_define")
        func _create(name: string)=> struct_define;
    
        let block = _create(name);
        shader::struct_uniform_blocks_decls->add(block);
        return block;
    }
    public func append_member<T>(self: struct_define, name: string)
    {
        _append_member(self, _get_type_enum:<T>(), name, option::none);
    }
    public func append_struct_member(self: struct_define, name: string, struct_type: struct_define)
    {
        _append_member(self, shader_value_type::STRUCT, name, option::value(struct_type));
    }
    
    extern("libjoyecs", "jeecs_shader_bind_struct_as_uniform_buffer")
    func bind_as_uniform_buffer(self: struct_define, binding_place: int)=> void;
    
}

#macro GRAPHIC_STRUCT
{
    let eat_token = func(expect_name: string, expect_type: std::token_type)
                    {
                        let (token, out_result) = lexer->nexttoken;
                        if (token != expect_type)
                            lexer->error(F"Expect '{expect_name}' here, but get '{out_result}'");
                        return out_result;
                    };
    let try_eat_token = func(expect_type: std::token_type)=> option<string>
                    {
                        let (token, out_result) = lexer->peektoken();
                        if (token != expect_type)
                            return option::none;
                        do lexer->nexttoken;
                        return option::value(out_result);
                    };

    // using GRAPHIC_STRUCT example { ...
    // 0. Get struct name, then eat '{'
    let graphic_struct_name = eat_token("IDENTIFIER", std::token_type::l_identifier);

    do eat_token("{", std::token_type::l_left_curly_braces);
    
    // 1. Get struct item name.
    let struct_infos = []mut: vec<(string, (string, bool))>;
    while (true)
    {
        if (try_eat_token(std::token_type::l_right_curly_braces)->has())
            // Meet '}', end work!
            break;

        let struct_member = eat_token("IDENTIFIER", std::token_type::l_identifier);
        do eat_token(":", std::token_type::l_typecast);

        // Shader type only have a identifier and without template.
        let is_struct = try_eat_token(std::token_type::l_struct);
        let struct_shader_type = eat_token("IDENTIFIER", std::token_type::l_identifier);

        struct_infos->add((struct_member, (struct_shader_type, is_struct->has)));

        if (!try_eat_token(std::token_type::l_comma)->has())
        {
            do eat_token("}", std::token_type::l_right_curly_braces);
            break;
        }
    }
    // End, need a ';' here.
    do eat_token(";", std::token_type::l_semicolon);

    //  OK We have current struct info, built struct out
    let mut out_struct_decl = F"public let {graphic_struct_name} = struct_define::create(\"{graphic_struct_name}\");";
    for(let _, (vao_member_name, (vao_shader_type, is_struct_type)) : struct_infos)
        if (is_struct_type)
            out_struct_decl += F"{graphic_struct_name}->append_struct_member(\"{vao_member_name}\", {vao_shader_type});\n";
        else
            out_struct_decl += F"{graphic_struct_name}->append_member:<{vao_shader_type}>(\"{vao_member_name}\");\n";

    out_struct_decl += F"public using {graphic_struct_name}_t = structure\n\{\n";
    for(let _, (vao_member_name, (vao_shader_type, is_struct_type)) : struct_infos)
    {
        out_struct_decl += F"    public func {vao_member_name}(self: {graphic_struct_name}_t)\n\{\n        ";
        
        if (is_struct_type)
            out_struct_decl += F"return self: gchandle: structure->get:<structure>(\"{vao_member_name}\"): gchandle: {vao_shader_type}_t;\n";
        else
            out_struct_decl += F"return self: gchandle: structure->get:<{vao_shader_type}>(\"{vao_member_name}\");\n";

        out_struct_decl += "    }\n";
    }
    out_struct_decl += "}\n";

    return out_struct_decl;
}

using uniform_block = struct_define
{
    public func create(name: string, binding_place: int)
    {
        let ubo = struct_define::create(name);
        ubo->bind_as_uniform_buffer(binding_place);

        return ubo: handle: uniform_block;
    }

    public func append_uniform<T>(self: uniform_block, name: string)
    {
        self: handle: struct_define->append_member:<T>(name);
        return shared_uniform:<T>(name);
    }

    public func append_struct_uniform(self: uniform_block, name: string, struct_type: struct_define)
    {
        self: handle: struct_define->append_struct_member(name, struct_type);
        return shared_uniform:<structure>(name);
    }
}

#macro UNIFORM_BUFFER
{
    let eat_token = func(expect_name: string, expect_type: std::token_type)
                    {
                        let (token, out_result) = lexer->nexttoken;
                        if (token != expect_type)
                            lexer->error(F"Expect '{expect_name}' here, but get '{out_result}'");
                        return out_result;
                    };
    let try_eat_token = func(expect_type: std::token_type)=> option<string>
                    {
                        let (token, out_result) = lexer->peektoken();
                        if (token != expect_type)
                            return option::none;
                        do lexer->nexttoken;
                        return option::value(out_result);
                    };

    // using UNIFORM_BUFFER example = BIND_PLACE { ...
    // 0. Get struct name, then eat '{'
    let graphic_struct_name = eat_token("IDENTIFIER", std::token_type::l_identifier);

    do eat_token("=", std::token_type::l_assign);

    let bind_place = eat_token("INTEGER", std::token_type::l_literal_integer): int;

    do eat_token("{", std::token_type::l_left_curly_braces);
    
    // 1. Get struct item name.
    let struct_infos = []mut: vec<(string, (string, bool))>;
    while (true)
    {
        if (try_eat_token(std::token_type::l_right_curly_braces)->has())
            // Meet '}', end work!
            break;

        let struct_member = eat_token("IDENTIFIER", std::token_type::l_identifier);
        do eat_token(":", std::token_type::l_typecast);

        // Shader type only have a identifier and without template.
        let is_struct = try_eat_token(std::token_type::l_struct);
        let struct_shader_type = eat_token("IDENTIFIER", std::token_type::l_identifier);

        struct_infos->add((struct_member, (struct_shader_type, is_struct->has)));

        if (!try_eat_token(std::token_type::l_comma)->has())
        {
            do eat_token("}", std::token_type::l_right_curly_braces);
            break;
        }
    }
    // End, need a ';' here.
    do eat_token(";", std::token_type::l_semicolon);

    //  OK We have current struct info, built struct out
    let mut out_struct_decl = F"public let {graphic_struct_name} = uniform_block::create(\"{graphic_struct_name}\", {bind_place});";
    for(let _, (vao_member_name, (vao_shader_type, is_struct_type)) : struct_infos)
        if (is_struct_type)
            out_struct_decl += F"public let {vao_member_name} = {graphic_struct_name}->append_struct_uniform(\"{vao_member_name->upper}\", {vao_shader_type}): gchandle: {vao_shader_type}_t;\n";
        else
            out_struct_decl += F"public let {vao_member_name} = {graphic_struct_name}->append_uniform:<{vao_shader_type}>(\"{vao_member_name->upper}\");\n";

    return out_struct_decl;
}

#macro SHADER_FUNCTION
{
    /*
    SHADER_FUNCTION!
    public func add(a: float, b: float)
    */

    let mut desc = "";
    for (;;)
    {
        let token = lexer->next;
        if (token == "")
        {
            lexer->error("Unexpected EOF.");
            return "";
        }
        else if (token == "func")
            break;
        else
            desc += token + " ";
    }

    let func_name = lexer->next;
    if (lexer->next != "(")
    {
        lexer->error("Expected '(' here.");
        return "";
    }

    let args = []mut: vec<(string, string)>;
    for (;;)
    {
        let arg_name = lexer->next;
        if (arg_name == "\x29")
            break;
        else if (arg_name == "")
        {
            lexer->error("Unexpected EOF.");
            return "";
        }
        
        if (lexer->next != ":")
        {
            lexer->error("Expected ':' here.");
            return "";
        }
        
        let arg_type = lexer->next;
        if (arg_type == "")
        {
            lexer->error("Unexpected EOF.");
            return "";
        }
        args->add((arg_name, arg_type));

        if (lexer->peek == ",")
            do lexer->next;
    }

    let mut result = F"let {func_name}_uf_vin = vertex_in::create();";
    result += F"let {func_name}_uf_args = (";

    for (let idx, (_, arg_type): args)
    {
        result += F"{func_name}_uf_vin->in:<{arg_type}>({idx}), ";
    }
    result += ");";

    result += desc + F"let {func_name} = function::register(\"{func_name}\", {func_name}_uf_args, {func_name}_uf_impl({func_name}_uf_args...));";

    result += F"func {func_name}_uf_impl(";
    for (let idx, (arg_name, arg_type): args)
    {
        if (idx != 0)
            result += ", ";
        result += F"{arg_name}: {arg_type}";
    }
    result += ")\n";

    return result;
}

UNIFORM_BUFFER! JOYENGINE_DEFAULT = 0
{
    je_v    : float4x4,
    je_p    : float4x4,
    je_vp   : float4x4,
    
    je_time : float4,
};

// je_mvp = je_p * je_v * je_m;
// je_mv  = je_v * je_m;
// je_vp  = je_p * je_v;

// Default uniform
public let je_m = uniform("JOYENGINE_TRANS_M", float4x4::unit);

public let je_mvp = uniform("JOYENGINE_TRANS_MVP", float4x4::unit);
public let je_mv = uniform("JOYENGINE_TRANS_MV", float4x4::unit);

public let je_local_scale = uniform("JOYENGINE_LOCAL_SCALE", float3::one);

public let je_tiling = uniform("JOYENGINE_TEXTURE_TILING", float2::one);
public let je_offset = uniform("JOYENGINE_TEXTURE_OFFSET", float2::zero);

public let je_color = uniform("JOYENGINE_MAIN_COLOR", float4::one);
)";

void _scan_used_uniforms_in_vals(
    shader_wrapper* wrap,
    jegl_shader_value* val,
    bool in_vertex,
    std::unordered_set<jegl_shader_value*>* sanned)
{
    if (val->is_calc_value())
    {
        if (val->is_uniform_variable())
        {
            if (!val->is_block_uniform_variable())
            {
                if (wrap->uniform_variables.find(val->m_unifrom_varname)
                    == wrap->uniform_variables.end())
                {
                    wrap->uniform_variables[val->m_unifrom_varname] =
                        _shader_wrapper_contex::get_uniform_info(val);
                }
                auto& uinfo = wrap->uniform_variables.at(val->m_unifrom_varname);
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
                    if (in_vertex)
                        fnd->second.m_used_in_vertex = true;
                    else
                        fnd->second.m_used_in_fragment = true;
                }
            }

            for (size_t i = 0; i < val->m_opnums_count; ++i)
            {
                _scan_used_uniforms_in_vals(wrap, val->m_opnums[i], in_vertex, sanned);
            }
        }
    }
}

void scan_used_uniforms_in_wrap(shader_wrapper* wrap)
{
    std::unordered_set<jegl_shader_value*> _scanned_val;
    for (auto* vout : wrap->vertex_out->out_values)
        _scan_used_uniforms_in_vals(wrap, vout, true, &_scanned_val);
    for (auto* vout : wrap->fragment_out->out_values)
        _scan_used_uniforms_in_vals(wrap, vout, false, &_scanned_val);

    for (auto& [_, func_info] : wrap->user_define_functions)
    {
        assert(func_info.m_result != nullptr && func_info.m_result->out_values.size() == 1);
        if (func_info.m_used_in_vertex)
            _scan_used_uniforms_in_vals(wrap, func_info.m_result->out_values[0], true, &_scanned_val);
        if (func_info.m_used_in_fragment)
            _scan_used_uniforms_in_vals(wrap, func_info.m_result->out_values[0], false, &_scanned_val);
    }
}

void jegl_shader_generate_glsl(void* shader_generator, jegl_shader* write_to_shader)
{
    shader_wrapper* shader_wrapper_ptr = (shader_wrapper*)shader_generator;

    write_to_shader->m_enable_to_shared = shader_wrapper_ptr->shader_config.m_enable_shared;
    write_to_shader->m_depth_test = shader_wrapper_ptr->shader_config.m_depth_test;
    write_to_shader->m_depth_mask = shader_wrapper_ptr->shader_config.m_depth_mask;
    write_to_shader->m_blend_src_mode = shader_wrapper_ptr->shader_config.m_blend_src;
    write_to_shader->m_blend_dst_mode = shader_wrapper_ptr->shader_config.m_blend_dst;
    write_to_shader->m_cull_mode = shader_wrapper_ptr->shader_config.m_cull_mode;

    scan_used_uniforms_in_wrap(shader_wrapper_ptr);

    jeecs::shader_generator::glsl_generator _glsl_generator;
    write_to_shader->m_vertex_glsl_src
        = jeecs::basic::make_new_string(
            _glsl_generator.generate_vertex(shader_wrapper_ptr).c_str());
    write_to_shader->m_fragment_glsl_src
        = jeecs::basic::make_new_string(
            _glsl_generator.generate_fragment(shader_wrapper_ptr).c_str());

    jeecs::shader_generator::hlsl_generator _hlsl_generator;
    write_to_shader->m_vertex_hlsl_src
        = jeecs::basic::make_new_string(
            _hlsl_generator.generate_vertex(shader_wrapper_ptr).c_str());
    write_to_shader->m_fragment_hlsl_src
        = jeecs::basic::make_new_string(
            _hlsl_generator.generate_fragment(shader_wrapper_ptr).c_str());

    write_to_shader->m_vertex_in_count = shader_wrapper_ptr->vertex_in.size();
    write_to_shader->m_vertex_in = new jegl_shader::vertex_in_variables[write_to_shader->m_vertex_in_count];
    for (size_t i = 0; i < write_to_shader->m_vertex_in_count; ++i)
        write_to_shader->m_vertex_in[i].m_type =
        _shader_wrapper_contex::get_outside_type(shader_wrapper_ptr->vertex_in[i]);

    std::unordered_map<std::string, shader_struct_define*> _uniform_blocks;
    for (auto& struct_def : shader_wrapper_ptr->shader_struct_define_may_uniform_block)
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
        jegl_shader::unifrom_variables** last = &write_to_shader->m_custom_uniforms;
        for (auto& [name, uniform_info] : shader_wrapper_ptr->uniform_variables)
        {
            jegl_shader::unifrom_variables* variable = new jegl_shader::unifrom_variables();
            variable->m_next = nullptr;

            variable->m_name = jeecs::basic::make_new_string(name.c_str());
            variable->m_uniform_type = uniform_info.m_type;

            variable->m_index = jeecs::typing::INVALID_UINT32;

            auto utype = uniform_info.m_value->get_type();
            auto* init_val = (
                utype == jegl_shader_value::type::TEXTURE2D ||
                utype == jegl_shader_value::type::TEXTURE2D_MS ||
                utype == jegl_shader_value::type::TEXTURE_CUBE)
                ? uniform_info.m_value
                : uniform_info.m_value->m_uniform_init_val_may_nil;

            if (init_val != nullptr)
            {
                switch (utype)
                {
                case jegl_shader_value::type::FLOAT:
                    variable->x = init_val->m_float; break;
                case jegl_shader_value::type::FLOAT2:
                    variable->x = init_val->m_float2[0];
                    variable->y = init_val->m_float2[1]; break;
                case jegl_shader_value::type::FLOAT3:
                    variable->x = init_val->m_float3[0];
                    variable->y = init_val->m_float3[1];
                    variable->z = init_val->m_float3[2]; break;
                case jegl_shader_value::type::FLOAT4:
                    variable->x = init_val->m_float4[0];
                    variable->y = init_val->m_float4[1];
                    variable->z = init_val->m_float4[2];
                    variable->w = init_val->m_float4[3]; break;
                case jegl_shader_value::type::FLOAT4x4:
                    memcpy(variable->mat4x4, init_val->m_float4x4, 4 * 4 * sizeof(float));
                    break;
                case jegl_shader_value::type::INTEGER:
                    variable->n = init_val->m_integer; break;
                case jegl_shader_value::type::TEXTURE2D:
                case jegl_shader_value::type::TEXTURE2D_MS:
                case jegl_shader_value::type::TEXTURE_CUBE:
                    variable->n = uniform_info.m_value->m_uniform_texture_channel; break;
                default:
                    jeecs::debug::logerr("Unsupport uniform variable type."); break;
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
        jegl_shader::uniform_blocks** last = &write_to_shader->m_custom_uniform_blocks;
        for (auto& [_/*useless*/, uniform_block_info] : _uniform_blocks)
        {
            jegl_shader::uniform_blocks* block = new jegl_shader::uniform_blocks();
            block->m_next = nullptr;

            assert(uniform_block_info->binding_place != jeecs::typing::INVALID_UINT32);

            block->m_name = jeecs::basic::make_new_string(uniform_block_info->name);
            block->m_specify_binding_place = uniform_block_info->binding_place;

            *last = block;
            last = &block->m_next;
        }
    } while (false);

    write_to_shader->m_sampler_count = shader_wrapper_ptr->decleared_samplers.size();
    auto* sampler_methods = new jegl_shader::sampler_method[write_to_shader->m_sampler_count];
    for (size_t i = 0; i < write_to_shader->m_sampler_count; ++i)
    {
        auto* sampler = shader_wrapper_ptr->decleared_samplers[i];
        sampler_methods[i].m_min = sampler->m_min;
        sampler_methods[i].m_mag = sampler->m_mag;
        sampler_methods[i].m_mip = sampler->m_mip;
        sampler_methods[i].m_uwrap = sampler->m_uwrap;
        sampler_methods[i].m_vwrap = sampler->m_vwrap;

        sampler_methods[i].m_sampler_id = sampler->m_sampler_id;
        sampler_methods[i].m_pass_id_count = sampler->m_binded_texture_passid.size();
        auto* passids = new uint32_t[sampler->m_binded_texture_passid.size()];

        static_assert(std::is_same<decltype(sampler_methods[i].m_pass_ids), uint32_t*>::value);

        memcpy(passids, sampler->m_binded_texture_passid.data(), sampler_methods[i].m_pass_id_count * sizeof(uint32_t));
        sampler_methods[i].m_pass_ids = passids;
    }
    write_to_shader->m_sampler_methods = sampler_methods;
}

void jegl_shader_free_generated_glsl(jegl_shader* write_to_shader)
{
    je_mem_free((void*)write_to_shader->m_vertex_glsl_src);
    je_mem_free((void*)write_to_shader->m_fragment_glsl_src);
    je_mem_free((void*)write_to_shader->m_vertex_hlsl_src);
    je_mem_free((void*)write_to_shader->m_fragment_hlsl_src);

    delete[]write_to_shader->m_vertex_in;

    auto* uniform_variable_info = write_to_shader->m_custom_uniforms;
    while (uniform_variable_info)
    {
        auto* current_uniform_variable = uniform_variable_info;
        uniform_variable_info = uniform_variable_info->m_next;

        je_mem_free((void*)current_uniform_variable->m_name);

        delete current_uniform_variable;
    }
    auto* uniform_block_info = write_to_shader->m_custom_uniform_blocks;
    while (uniform_block_info)
    {
        auto* cur_uniform_block = uniform_block_info;
        uniform_block_info = uniform_block_info->m_next;

        je_mem_free((void*)cur_uniform_block->m_name);

        delete cur_uniform_block;
    }

    for (size_t i = 0; i < write_to_shader->m_sampler_count; ++i)
        delete[]write_to_shader->m_sampler_methods[i].m_pass_ids;
    delete[]write_to_shader->m_sampler_methods;
}