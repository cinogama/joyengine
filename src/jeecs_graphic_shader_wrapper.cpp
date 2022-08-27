#define JE_IMPL
#include "jeecs.hpp"
#include <functional>
#include <variant>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct jegl_shader_value
{
    using type_base_t = uint32_t;
    enum type : type_base_t
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
        INTEGER = 0X1000,

    };

    type m_type;
    size_t m_ref_count;

    mutable std::atomic_flag _m_spin;
    inline void lock() const noexcept
    {
        while (_m_spin.test_and_set());
    }
    inline void unlock() const noexcept
    {
        _m_spin.clear();
    }

    union
    {
        float m_float;
        float m_float2[2];
        float m_float3[3];
        float m_float4[4];
        float m_float2x2[2][2];
        float m_float3x3[3][3];
        float m_float4x4[4][4];
        int m_integer;
        struct
        {
            const char* m_opname;
            size_t m_opnums_count;
            jegl_shader_value** m_opnums; // NOTE: IF THERE ARE MANY USE-REF OF ONE VALUE, CHECKIT
        };
        struct
        {
            size_t m_shader_in_index;
        };
        struct
        {
            const char* m_unifrom_varname;
            int m_uniform_texture_channel;
            jegl_shader_value* m_uniform_init_val_may_nil;
        };
    };

    jegl_shader_value(float init_val)
        : m_type((type)(type::FLOAT | type::INIT_VALUE))
        , m_float(init_val)
        , m_ref_count(0)
    {
    }

    jegl_shader_value(float x, float y)
        : m_type((type)(type::FLOAT2 | type::INIT_VALUE))
        , m_ref_count(0)
    {
        m_float2[0] = x;
        m_float2[1] = y;
    }
    jegl_shader_value(float x, float y, float z)
        : m_type((type)(type::FLOAT3 | type::INIT_VALUE))
        , m_ref_count(0)
    {
        m_float3[0] = x;
        m_float3[1] = y;
        m_float3[2] = z;
    }
    jegl_shader_value(float x, float y, float z, float w)
        : m_type((type)(type::FLOAT4 | type::INIT_VALUE))
        , m_ref_count(0)
    {
        m_float4[0] = x;
        m_float4[1] = y;
        m_float4[2] = z;
        m_float4[3] = w;
    }
    jegl_shader_value(float* data, type typing)
        : m_type((type)(typing | type::INIT_VALUE))
        , m_ref_count(0)
    {
        if (data)
        {
            if (typing == type::FLOAT4x4)
            {
                float* wdata = (float*)&m_float4x4;
                for (size_t i = 0; i < 16; i++)
                    wdata[i] = data[i];
            }
            else if (typing == type::FLOAT3x3)
            {
                float* wdata = (float*)&m_float3x3;
                for (size_t i = 0; i < 9; i++)
                    wdata[i] = data[i];
            }
            else if (typing == type::FLOAT2x2)
            {
                float* wdata = (float*)&m_float2x2;
                for (size_t i = 0; i < 4; i++)
                    wdata[i] = data[i];
            }
            else
                wo_fail(0xD000, "Unknown type to init, should be f");
        }
    }
    void set_used_val(size_t id)
    {
    }

    void add_useref_count()
    {
        std::lock_guard g1(*this);
        ++m_ref_count;
    }

    template<typename T, typename ... TS>
    void set_used_val(size_t id, T val, TS ... args)
    {
        static_assert(std::is_same<T, jegl_shader_value*>::value);
        m_opnums[id] = val;

        val->add_useref_count();

        set_used_val(id + 1, args...);
    }

    template<typename T, typename ... TS>
    jegl_shader_value(type resulttype, const char* operat, T val, TS... args)
        : m_type((type)(resulttype | type::CALC_VALUE))
        , m_opname(jeecs::basic::make_new_string(operat))
        , m_opnums_count(sizeof...(args) + 1)
        , m_ref_count(0)
    {
        m_opnums = new jegl_shader_value * [m_opnums_count];
        set_used_val(0, val, args...);
    }

    jegl_shader_value(type resulttype, const char* operat, size_t opnum_count)
        : m_type((type)(resulttype | type::CALC_VALUE))
        , m_opname(jeecs::basic::make_new_string(operat))
        , m_opnums_count(opnum_count)
        , m_ref_count(0)
    {
        m_opnums = new jegl_shader_value * [m_opnums_count];
    }

    jegl_shader_value(type resulttype)
        : m_type((type)(resulttype | type::CALC_VALUE | type::SHADER_IN_VALUE))
        , m_shader_in_index(0)
        , m_ref_count(0)
    {
    }

    jegl_shader_value(type resulttype, const char* uniform_name, jegl_shader_value* init_val, bool is_predef)
        : m_type((type)(resulttype | type::CALC_VALUE | (is_predef ? type::UNIFORM_BLOCK_VARIABLE : type::UNIFORM_VARIABLE)))
        , m_unifrom_varname(jeecs::basic::make_new_string(uniform_name))
        , m_uniform_texture_channel(0)
        , m_ref_count(0)
        , m_uniform_init_val_may_nil(init_val)
    {
        if (m_uniform_init_val_may_nil)
            m_uniform_init_val_may_nil->add_useref_count();
    }

    inline bool is_init_value() const noexcept
    {
        std::lock_guard g(*this);
        return !(m_type & CALC_VALUE);
    }
    inline bool is_calc_value() const noexcept
    {
        return !is_init_value();
    }
    inline bool is_shader_in_value() const noexcept
    {
        std::lock_guard g(*this);
        return m_type & SHADER_IN_VALUE;
    }
    inline bool is_uniform_variable() const noexcept
    {
        std::lock_guard g(*this);
        return (m_type & UNIFORM_VARIABLE) || (m_type & UNIFORM_BLOCK_VARIABLE);
    }
    inline bool is_block_uniform_variable() const noexcept
    {
        std::lock_guard g(*this);
        return m_type & UNIFORM_BLOCK_VARIABLE;
    }
    inline type get_type() const
    {
        std::lock_guard g(*this);
        return (type)(m_type & TYPE_MASK);
    }
};

void delete_shader_value(jegl_shader_value* shader_val)
{
    do
    {
        std::lock_guard g(*shader_val);
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
    return wo_ret_gchandle(vm, new jegl_shader_value((float)wo_real(args + 0)), nullptr, _free_shader_value);
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
    assert(texture2d_val->get_type() == jegl_shader_value::type::TEXTURE2D);

    texture2d_val->m_uniform_texture_channel = (int)wo_int(args + 1);
    return wo_ret_val(vm, args+0);
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

using calc_func_t = std::function<jegl_shader_value* (size_t, jegl_shader_value**)>;
using operation_t = std::variant<jegl_shader_value::type_base_t, calc_func_t>;

#define reduce_method [](size_t argc, jegl_shader_value** args)->jegl_shader_value*
#include "jeecs_graphic_shader_wrapper_methods.hpp"
#undef reduce_method

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
                    jeecs::debug::log_fatal("Shader operation map conflict.");
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

    assert(index < argc&& cur_node);

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
        new jegl_shader_value((jegl_shader_value::type)wo_int(args + 0), wo_string(args + 1), (jegl_shader_value*)nullptr, wo_bool(args + 2))
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
        if (value_type != WO_INTEGER_TYPE && value_type != WO_REAL_TYPE && value_type != WO_GCHANDLE_TYPE)
            return wo_ret_halt(vm, "Cannot do this operations: argument type should be number or shader_value.");

        jegl_shader_value* sval;
        if (value_type == WO_GCHANDLE_TYPE)
            sval = (jegl_shader_value*)wo_pointer(args + i);
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
    auto* reduce_func = get_const_reduce_func(operation, _types.data(), _types.size());
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

    jegl_shader_value* val =
        new jegl_shader_value(result_type, wo_string(args + 1), argc - 2);
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
        auto fnd = m_in_from_vao_guard.find(pos);
        if (fnd != m_in_from_vao_guard.end())
        {
            if (fnd->second->get_type() != type)
                return nullptr;
            return fnd->second;
        }
        auto& shval = m_in_from_vao_guard[pos];
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

    return wo_ret_gchandle(vm, result, nullptr, nullptr);
}

WO_API wo_api jeecs_shader_set_vertex_out(wo_vm vm, wo_value args, size_t argc)
{
    jegl_shader_value* vertex_out_pos = (jegl_shader_value*)wo_pointer(args + 0);
    if (vertex_out_pos->get_type() != jegl_shader_value::FLOAT4)
        return wo_ret_halt(vm, "First value of vertex_out must be FLOAT4 for position.");

    return wo_ret_void(vm);
}

using uniform_information = std::tuple<std::string, jegl_shader::uniform_type, jegl_shader_value*>;

struct shader_value_outs
{
    std::vector<jegl_shader_value*> out_values;
    std::vector<uniform_information> uniform_variables;
    ~shader_value_outs()
    {
        for (auto* val : out_values)
            delete_shader_value(val);
    }
};

struct shader_configs
{
    jegl_shader::depth_test_method m_depth_test;
    jegl_shader::depth_mask_method m_depth_mask;
    jegl_shader::alpha_test_method m_alpha_test;
    jegl_shader::blend_method m_blend_src, m_blend_dst;
    jegl_shader::cull_mode m_cull_mode;
};

struct shader_wrapper
{
    shader_value_outs* vertex_out;
    shader_value_outs* fragment_out;
    shader_configs shader_config;

    ~shader_wrapper()
    {
        delete vertex_out;
        delete fragment_out;
    }
};

WO_API wo_api jeecs_shader_create_shader_value_out(wo_vm vm, wo_value args, size_t argc)
{
    wo_value voutstruct = args + 1;
    uint16_t structsz = 0;

    // is vertex out, check it
    if (wo_valuetype(voutstruct) != WO_STRUCT_TYPE
        || (structsz = wo_lengthof(voutstruct)) == 0
        || (wo_bool(args + 0)
            ? jegl_shader_value::type::FLOAT4 != ((jegl_shader_value*)wo_pointer(wo_struct_get(voutstruct, 0)))->get_type()
            : false))
        return wo_ret_halt(vm, "'vert' must return a struct with first member of 'float4'.");

    shader_value_outs* values = new shader_value_outs;
    values->out_values.resize(structsz);
    for (uint16_t i = 0; i < structsz; i++)
    {
        values->out_values[i] = (jegl_shader_value*)wo_pointer(wo_struct_get(voutstruct, i));
        values->out_values[i]->add_useref_count();
    }
    return wo_ret_pointer(vm, values);
}
WO_API wo_api jeecs_shader_create_fragment_in(wo_vm vm, wo_value args, size_t argc)
{
    shader_value_outs* values = (shader_value_outs*)wo_pointer(args + 0);
    wo_value out_struct = args + 1;

    uint16_t fragmentin_size = (uint16_t)values->out_values.size();
    wo_set_struct(out_struct, fragmentin_size);
    for (uint16_t i = 0; i < fragmentin_size; i++)
    {
        auto* val = new jegl_shader_value(values->out_values[i]->get_type());
        val->m_shader_in_index = i;
        wo_set_gchandle(wo_struct_get(out_struct, i), val, nullptr, _free_shader_value);
    }

    return wo_ret_val(vm, out_struct);
}

WO_API wo_api jeecs_shader_wrap_result_pack(wo_vm vm, wo_value args, size_t argc)
{
    shader_configs config;

    config.m_depth_test = (jegl_shader::depth_test_method)wo_int(wo_struct_get(args + 2, 0));
    config.m_depth_mask = (jegl_shader::depth_mask_method)wo_int(wo_struct_get(args + 2, 1));
    config.m_alpha_test = (jegl_shader::alpha_test_method)wo_int(wo_struct_get(args + 2, 2));
    config.m_blend_src = (jegl_shader::blend_method)wo_int(wo_struct_get(args + 2, 3));
    config.m_blend_dst = (jegl_shader::blend_method)wo_int(wo_struct_get(args + 2, 4));
    config.m_cull_mode = (jegl_shader::cull_mode)wo_int(wo_struct_get(args + 2, 5));

    return wo_ret_gchandle(vm,
        new shader_wrapper
        {
            (shader_value_outs*)wo_pointer(args + 0),
            (shader_value_outs*)wo_pointer(args + 1),
            config
        }, nullptr,
        [](void* ptr) {
            delete (shader_wrapper*)ptr;
        });
}

#include "jeecs_graphic_shader_wrapper_glsl.hpp"

std::string _generate_glsl_vertex_by_wrapper(shader_wrapper* wrap)
{
    return _generate_code_for_glsl_vertex(wrap);
}

std::string _generate_glsl_fragment_by_wrapper(shader_wrapper* wrap)
{
    return _generate_code_for_glsl_fragment(wrap);
}

WO_API wo_api jeecs_shader_wrap_glsl_vertex(wo_vm vm, wo_value args, size_t argc)
{
    shader_wrapper* wrap = (shader_wrapper*)wo_pointer(args + 0);
    return wo_ret_string(vm, _generate_glsl_vertex_by_wrapper(wrap).c_str());
}

WO_API wo_api jeecs_shader_wrap_glsl_fragment(wo_vm vm, wo_value args, size_t argc)
{
    shader_wrapper* wrap = (shader_wrapper*)wo_pointer(args + 0);
    return wo_ret_string(vm, _generate_glsl_fragment_by_wrapper(wrap).c_str());
}

const char* shader_wrapper_path = "je/shader.wo";
const char* shader_wrapper_src = R"(
// JoyEngineECS RScene shader wrapper

import woo.std;

enum shader_value_type
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
    INTEGER = 0X1000,
}

using float = gchandle;
using float2 = gchandle;
using float3 = gchandle;
using float4 = gchandle;

using float2x2 = gchandle;
using float3x3 = gchandle;
using float4x4 = gchandle;

using texture2d = gchandle;
using integer = gchandle;

private func _type_is_same<AT, BT>()=> bool
{
    if (func()=>AT{}() is BT)
        return true;
    return false;
}

private func _get_type_enum<ShaderValueT>()=> shader_value_type
{
    if (_type_is_same:<ShaderValueT, float>())
        return shader_value_type::FLOAT;
    else if (_type_is_same:<ShaderValueT, float2>())
        return shader_value_type::FLOAT2;
    else if (_type_is_same:<ShaderValueT, float3>())
        return shader_value_type::FLOAT3;
    else if (_type_is_same:<ShaderValueT, float4>())
        return shader_value_type::FLOAT4;
    else if (_type_is_same:<ShaderValueT, float2x2>())
        return shader_value_type::FLOAT2x2;
    else if (_type_is_same:<ShaderValueT, float3x3>())
        return shader_value_type::FLOAT3x3;
    else if (_type_is_same:<ShaderValueT, float4x4>())
        return shader_value_type::FLOAT4x4;
    else if (_type_is_same:<ShaderValueT, texture2d>())
        return shader_value_type::TEXTURE2D;
    else if (_type_is_same:<ShaderValueT, integer>())
        return shader_value_type::INTEGER;

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
private func _uniform<ShaderResultT>(
    result_type : shader_value_type,
    uniform_name : string,
    init_value : ShaderResultT
)=> ShaderResultT;

func uniform<ShaderResultT>(uniform_name:string)=> ShaderResultT
{
    return _uniform:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, false);
}

func uniform<ShaderResultT>(uniform_name:string, init_value: ShaderResultT)=> ShaderResultT
{
    return _uniform:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, init_value);
}

func shared_uniform<ShaderResultT>(uniform_name:string)=> ShaderResultT
{
    return _uniform:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, true);
}

extern("libjoyecs", "jeecs_shader_create_rot_mat4x4")
func rotation(x:real, y:real, z:real)=> float4x4;

using vertex_in = handle;
namespace vertex_in
{
    extern("libjoyecs", "jeecs_shader_create_vertex_in")
    func create()=> vertex_in;

    extern("libjoyecs", "jeecs_shader_get_vertex_in")
    private func _in<ValueT>(self:vertex_in, type:shader_value_type, id:int)=> ValueT;

    func in<ValueT>(self:vertex_in, id:int)=> ValueT
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
    func create<VertexOutT>(data_from_vert: vertex_out)=> VertexOutT
    {
        extern("libjoyecs", "jeecs_shader_create_fragment_in")
        func _parse_vertex_out_to_struct<VertexOutT>(vout: vertex_out, ref _out_struct: dynamic)=> VertexOutT;

        return _parse_vertex_out_to_struct:<VertexOutT>(data_from_vert, nil: dynamic);
    }
}

using fragment_out = handle; // nogc! will free by shader_wrapper
namespace fragment_out
{
    func create<FragementOutT>(fout : FragementOutT)=> fragment_out
    {
        extern("libjoyecs", "jeecs_shader_create_shader_value_out")
        func _create_shader_out<FragementOutT>(is_vertex: bool, out_val: FragementOutT)=> fragment_out;

        return _create_shader_out(false, fout);
    }
}

namespace float
{
    extern("libjoyecs", "jeecs_shader_float_create")
    func create(init_val:real)=> float;

    func create(...)=> float{return apply_operation:<float>("float", ......);}

    func operator + (a:float, b:float)=> float
    {
        return apply_operation:<float>("+", a, b);
    }
    func operator - (a:float, b:float)=> float
    {
        return apply_operation:<float>("-", a, b);
    }
    func operator * (a:float, b:float)=> float
    {
        return apply_operation:<float>("*", a, b);
    }
    func operator / (a:float, b:float)=> float
    {
        return apply_operation:<float>("/", a, b);
    }

    func operator * (a:float, b:float2)=> float2
    {
        return apply_operation:<float2>("*", b, a);
    }
    func operator * (a:float, b:float3)=> float3
    {
        return apply_operation:<float3>("*", b, a);
    }
    func operator * (a:float, b:float4)=> float4
    {
        return apply_operation:<float4>("*", b, a);
    }   
}
namespace float2
{
    extern("libjoyecs", "jeecs_shader_float2_create")
    func create(x:real, y:real)=> float2;

    func create(...)=> float2{return apply_operation:<float2>("float2", ......);}

    func x(self:float2)=> float{return apply_operation:<float>(".x", self);}
    func y(self:float2)=> float{return apply_operation:<float>(".y", self);}
    func xy(self:float2)=> float2{return apply_operation:<float2>(".xy", self);}
    func yx(self:float2)=> float2{return apply_operation:<float2>(".yx", self);}

    func operator + (a:float2, b:float2)=> float2
    {
        return apply_operation:<float2>("+", a, b);
    }
    func operator - (a:float2, b:float2)=> float2
    {
        return apply_operation:<float2>("-", a, b);
    }
    func operator * (a:float2, b:float2)=> float2
    {
        return apply_operation:<float2>("*", a, b);
    }

    func operator * (a:float2, b:float)=> float2
    {
        return apply_operation:<float2>("*", a, b);
    }
    
    func operator / (a:float2, b:float)=> float2
    {
        return apply_operation:<float2>("/", a, b);
    }
}
namespace float3
{
    extern("libjoyecs", "jeecs_shader_float3_create")
    func create(x:real, y:real, z:real)=> float3;

    func create(...)=> float3{return apply_operation:<float3>("float3", ......);}

    func x(self:float3)=> float{return apply_operation:<float>(".x", self);}
    func y(self:float3)=> float{return apply_operation:<float>(".y", self);}
    func z(self:float3)=> float{return apply_operation:<float>(".z", self);}
    func xy(self:float3)=> float2{return apply_operation:<float2>(".xy", self);}
    func yz(self:float3)=> float2{return apply_operation:<float2>(".yz", self);}
    func xz(self:float3)=> float2{return apply_operation:<float2>(".xz", self);}
    func yx(self:float3)=> float2{return apply_operation:<float2>(".yx", self);}
    func zy(self:float3)=> float2{return apply_operation:<float2>(".zy", self);}
    func zx(self:float3)=> float2{return apply_operation:<float2>(".zx", self);}
    func xyz(self:float3)=> float3{return apply_operation:<float3>(".xyz", self);}
    func xzy(self:float3)=> float3{return apply_operation:<float3>(".xzy", self);}
    func yxz(self:float3)=> float3{return apply_operation:<float3>(".yxz", self);}
    func yzx(self:float3)=> float3{return apply_operation:<float3>(".yzx", self);}
    func zxy(self:float3)=> float3{return apply_operation:<float3>(".zxy", self);}
    func zyx(self:float3)=> float3{return apply_operation:<float3>(".zyx", self);}

    func operator + (a:float3, b:float3)=> float3
    {
        return apply_operation:<float3>("+", a, b);
    }
    func operator - (a:float3, b:float3)=> float3
    {
        return apply_operation:<float3>("-", a, b);
    }
    func operator * (a:float3, b:float3)=> float3
    {
        return apply_operation:<float3>("*", a, b);
    }

    func operator * (a:float3, b:float)=> float3
    {
        return apply_operation:<float3>("*", a, b);
    }
    
    func operator / (a:float3, b:float)=> float3
    {
        return apply_operation:<float3>("/", a, b);
    }
}
namespace float4
{
    extern("libjoyecs", "jeecs_shader_float4_create")
    func create(x:real, y:real, z:real, w:real)=> float4;

    func create(...)=> float4{return apply_operation:<float4>("float4", ......);}

    func x(self:float4)=> float{return apply_operation:<float>(".x", self);}
    func y(self:float4)=> float{return apply_operation:<float>(".y", self);}
    func z(self:float4)=> float{return apply_operation:<float>(".z", self);}
    func w(self:float4)=> float{return apply_operation:<float>(".w", self);}

    func xy(self:float4)=> float2{return apply_operation:<float2>(".xy", self);}
    func yz(self:float4)=> float2{return apply_operation:<float2>(".yz", self);}
    func xz(self:float4)=> float2{return apply_operation:<float2>(".xz", self);}
    func yx(self:float4)=> float2{return apply_operation:<float2>(".yx", self);}
    func zy(self:float4)=> float2{return apply_operation:<float2>(".zy", self);}
    func zx(self:float4)=> float2{return apply_operation:<float2>(".zx", self);}
    func xw(self:float4)=> float2{return apply_operation:<float2>(".xw", self);}
    func wx(self:float4)=> float2{return apply_operation:<float2>(".wx", self);}
    func yw(self:float4)=> float2{return apply_operation:<float2>(".yw", self);}
    func wy(self:float4)=> float2{return apply_operation:<float2>(".wy", self);}
    func zw(self:float4)=> float2{return apply_operation:<float2>(".zw", self);}
    func wz(self:float4)=> float2{return apply_operation:<float2>(".wz", self);}

    func xyz(self:float4)=> float3{return apply_operation:<float3>(".xyz", self);}
    func xzy(self:float4)=> float3{return apply_operation:<float3>(".xzy", self);}
    func yxz(self:float4)=> float3{return apply_operation:<float3>(".yxz", self);}
    func yzx(self:float4)=> float3{return apply_operation:<float3>(".yzx", self);}
    func zxy(self:float4)=> float3{return apply_operation:<float3>(".zxy", self);}
    func zyx(self:float4)=> float3{return apply_operation:<float3>(".zyx", self);}
    func wyz(self:float4)=> float3{return apply_operation:<float3>(".wyz", self);}
    func wzy(self:float4)=> float3{return apply_operation:<float3>(".wzy", self);}
    func ywz(self:float4)=> float3{return apply_operation:<float3>(".ywz", self);}
    func yzw(self:float4)=> float3{return apply_operation:<float3>(".yzw", self);}
    func zwy(self:float4)=> float3{return apply_operation:<float3>(".zwy", self);}
    func zyw(self:float4)=> float3{return apply_operation:<float3>(".zyw", self);}
    func xwz(self:float4)=> float3{return apply_operation:<float3>(".xwz", self);}
    func xzw(self:float4)=> float3{return apply_operation:<float3>(".xzw", self);}
    func wxz(self:float4)=> float3{return apply_operation:<float3>(".wxz", self);}
    func wzx(self:float4)=> float3{return apply_operation:<float3>(".wzx", self);}
    func zxw(self:float4)=> float3{return apply_operation:<float3>(".zxw", self);}
    func zwx(self:float4)=> float3{return apply_operation:<float3>(".zwx", self);}
    func xyw(self:float4)=> float3{return apply_operation:<float3>(".xyw", self);}
    func xwy(self:float4)=> float3{return apply_operation:<float3>(".xwy", self);}
    func yxw(self:float4)=> float3{return apply_operation:<float3>(".yxw", self);}
    func ywx(self:float4)=> float3{return apply_operation:<float3>(".ywx", self);}
    func wxy(self:float4)=> float3{return apply_operation:<float3>(".wxy", self);}
    func wyx(self:float4)=> float3{return apply_operation:<float3>(".wyx", self);}

    func xyzw(self:float4)=> float4{return apply_operation:<float4>(".xyzw", self);}
    func xzyw(self:float4)=> float4{return apply_operation:<float4>(".xzyw", self);}
    func yxzw(self:float4)=> float4{return apply_operation:<float4>(".yxzw", self);}
    func yzxw(self:float4)=> float4{return apply_operation:<float4>(".yzxw", self);}
    func zxyw(self:float4)=> float4{return apply_operation:<float4>(".zxyw", self);}
    func zyxw(self:float4)=> float4{return apply_operation:<float4>(".zyxw", self);}
    func wyzx(self:float4)=> float4{return apply_operation:<float4>(".wyzx", self);}
    func wzyx(self:float4)=> float4{return apply_operation:<float4>(".wzyx", self);}
    func ywzx(self:float4)=> float4{return apply_operation:<float4>(".ywzx", self);}
    func yzwx(self:float4)=> float4{return apply_operation:<float4>(".yzwx", self);}
    func zwyx(self:float4)=> float4{return apply_operation:<float4>(".zwyx", self);}
    func zywx(self:float4)=> float4{return apply_operation:<float4>(".zywx", self);}
    func xwzy(self:float4)=> float4{return apply_operation:<float4>(".xwzy", self);}
    func xzwy(self:float4)=> float4{return apply_operation:<float4>(".xzwy", self);}
    func wxzy(self:float4)=> float4{return apply_operation:<float4>(".wxzy", self);}
    func wzxy(self:float4)=> float4{return apply_operation:<float4>(".wzxy", self);}
    func zxwy(self:float4)=> float4{return apply_operation:<float4>(".zxwy", self);}
    func zwxy(self:float4)=> float4{return apply_operation:<float4>(".zwxy", self);}
    func xywz(self:float4)=> float4{return apply_operation:<float4>(".xywz", self);}
    func xwyz(self:float4)=> float4{return apply_operation:<float4>(".xwyz", self);}
    func yxwz(self:float4)=> float4{return apply_operation:<float4>(".yxwz", self);}
    func ywxz(self:float4)=> float4{return apply_operation:<float4>(".ywxz", self);}
    func wxyz(self:float4)=> float4{return apply_operation:<float4>(".wxyz", self);}
    func wyxz(self:float4)=> float4{return apply_operation:<float4>(".wyxz", self);}

    func operator + (a:float4, b:float4)=> float4
    {
        return apply_operation:<float4>("+", a, b);
    }
    func operator - (a:float4, b:float4)=> float4
    {
        return apply_operation:<float4>("-", a, b);
    }
    func operator * (a:float4, b:float4)=> float4
    {
        return apply_operation:<float4>("*", a, b);
    }

    func operator * (a:float4, b:float)=> float4
    {
        return apply_operation:<float4>("*", a, b);
    }

    func operator / (a:float4, b:float)=> float4
    {
        return apply_operation:<float4>("/", a, b);
    }

    )" R"(
}

namespace float4x4
{
    extern("libjoyecs", "jeecs_shader_float4x4_create")
    func create(p00:real, p01:real, p02:real, p03:real,
                p10:real, p11:real, p12:real, p13:real,
                p20:real, p21:real, p22:real, p23:real,
                p30:real, p31:real, p32:real, p33:real)=> float4x4;

    func create(...)=> float4x4{return apply_operation:<float4x4>("float4x4", ......);}

    func operator * (a:float4x4, b:float4x4)=> float4x4
    {
        return apply_operation:<float4x4>("*", a, b);
    }

    func operator * (a:float4x4, b:float4)=> float4
    {
        return apply_operation:<float4>("*", a, b);
    }
}

namespace shader
{
    using shader_wrapper = gchandle;

    using ShaderConfig = struct {
        ztest     : ZConfig,
        zwrite    : GConfig,
        alpha     : GConfig,
        blend_src : BlendConfig,
        blend_dst : BlendConfig,
        cull      : CullConfig
    };
    let configs = ShaderConfig
    {
        ztest = LESS,
        zwrite = ENABLE,
        alpha = DISABLE,
        blend_src = ONE,
        blend_dst = ZERO,
        cull = NONE,
    };

    extern("libjoyecs", "jeecs_shader_wrap_result_pack")
    private func _wraped_shader(
        vertout: vertex_out, 
        fragout: fragment_out, 
        shader_config: ShaderConfig)=> shader_wrapper;

    private extern func generate()
    {
        // 'v_out' is a struct with member of shader variable as vertex outputs.
        let v_out = vert(_JE_BUILT_VAO_STRUCT(vertex_in()));
        
        // 'vertex_out' will analyze struct, then 'fragment_in' will build a new struct
        let vertext_out_result = vertex_out(v_out);
        let f_in = fragment_in:<typeof(v_out)>(vertext_out_result);
    
        // 'f_out' is a struct with output shader variable.
        let f_out = frag(f_in);
        let fragment_out_result = fragment_out(f_out);

        return _wraped_shader(vertext_out_result, fragment_out_result, configs);
    }

    namespace debug
    {
        extern("libjoyecs", "jeecs_shader_wrap_glsl_vertex")
        func generate_glsl_vertex(wrapper:shader_wrapper)=> string;

        extern("libjoyecs", "jeecs_shader_wrap_glsl_fragment")
        func generate_glsl_fragment(wrapper:shader_wrapper)=> string;
    }
}

namespace texture2d
{
    extern("libjoyecs", "jeecs_shader_texture2d_set_channel")
    func channel(self: texture2d, pass: int)=> texture2d;
}

// Default unifrom
let je_time = shared_uniform:<float4>("JOYENGINE_TIMES");

let je_m = uniform:<float4x4>("JOYENGINE_TRANS_M");
let je_v = uniform:<float4x4>("JOYENGINE_TRANS_V");
let je_p = uniform:<float4x4>("JOYENGINE_TRANS_P");

// je_mvp = je_p * je_v * je_m;
// je_mv  = je_v * je_m;
// je_vp  = je_p * je_v;
let je_mvp = uniform:<float4x4>("JOYENGINE_TRANS_MVP");
let je_mv = uniform:<float4x4>("JOYENGINE_TRANS_MV");
let je_vp = uniform:<float4x4>("JOYENGINE_TRANS_VP");

func texture(tex:texture2d, uv:float2)=> float4
{
    return apply_operation:<float4>("texture", tex, uv);
}

func lerp<T>(a: T, b: T, uv:float)=> T
{
    return apply_operation:<T>("lerp", a, b, uv);
}

func step(a: float, b: float)=> float
{
    return apply_operation:<float>("step", a, b);
}

func alphatest(colf4: float4)=> float4
{
    return apply_operation:<float4>("JEBUILTIN_AlphaTest", colf4);
}

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
let const OFF = ZConfig::OFF;
let const NEVER = ZConfig::NEVER;
let const LESS = ZConfig::LESS;
let const EQUAL = ZConfig::EQUAL;
let const GREATER = ZConfig::GREATER;
let const LESS_EQUAL = ZConfig::LESS_EQUAL;
let const NOT_EQUAL = ZConfig::NOT_EQUAL;
let const GREATER_EQUAL = ZConfig::GREATER_EQUAL;
let const ALWAYS = ZConfig::ALWAYS;

enum GConfig
{
    DISABLE = 0,
    ENABLE
}
let const DISABLE = GConfig::DISABLE;
let const ENABLE = GConfig::ENABLE;

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

    CONST_COLOR,
    ONE_MINUS_CONST_COLOR,

    CONST_ALPHA,
    ONE_MINUS_CONST_ALPHA,
}

let const ZERO = BlendConfig::ZERO;
let const ONE = BlendConfig::ONE;
let const SRC_COLOR = BlendConfig::SRC_COLOR;
let const SRC_ALPHA = BlendConfig::SRC_ALPHA;
let const ONE_MINUS_SRC_ALPHA = BlendConfig::ONE_MINUS_SRC_ALPHA;
let const ONE_MINUS_SRC_COLOR = BlendConfig::ONE_MINUS_SRC_COLOR;
let const DST_COLOR = BlendConfig::DST_COLOR;
let const DST_ALPHA = BlendConfig::DST_ALPHA;
let const ONE_MINUS_DST_ALPHA = BlendConfig::ONE_MINUS_DST_ALPHA;
let const ONE_MINUS_DST_COLOR = BlendConfig::ONE_MINUS_DST_COLOR;
let const CONST_COLOR = BlendConfig::CONST_COLOR;
let const ONE_MINUS_CONST_COLOR = BlendConfig::ONE_MINUS_CONST_COLOR;
let const CONST_ALPHA = BlendConfig::CONST_ALPHA;
let const ONE_MINUS_CONST_ALPHA = BlendConfig::ONE_MINUS_CONST_ALPHA;

enum CullConfig
{
    NONE = 0,       /* DEFAULT */
    FRONT,
    BACK,
    ALL,
}

let const NONE = CullConfig::NONE;
let const FRONT = CullConfig::FRONT;
let const BACK = CullConfig::BACK;
let const ALL = CullConfig::ALL;

func ZTEST(zconfig: ZConfig)
{
    shader::configs.ztest = zconfig;
}

func ZWRITE(zwrite: GConfig)
{
    shader::configs.zwrite = zwrite;
}

func ALPHA(aenable: GConfig)
{
    shader::configs.alpha = aenable;
    std::println("'ALPHA' option for alpha-test is obsoleted, please use 'alphatest' function instead.");
}

func BLEND(src: BlendConfig, dst: BlendConfig)
{
    shader::configs.blend_src = src;
    shader::configs.blend_dst = dst;
}


func CULL(cull: CullConfig)
{
    shader::configs.cull = cull;
}

#macro VAO_STRUCT
{
    let eat_token = func(ref expect_name: string, expect_type: std::token_type)
                    {
                        let mut out_result = "";
                        if (lexer->next(ref out_result) != expect_type)
                            lexer->error(F"Expect '{expect_name}' here, but get '{out_result}'");
                        return out_result;
                    };
    let try_eat_token = func(expect_type: std::token_type)=> option<string>
                    {
                        let mut out_result = "";
                        if (lexer->peek("") != expect_type)
                            return option::none;
                        assert(lexer->next(ref out_result) == expect_type);
                        return option::value(out_result);
                    };

    // using VAO_STRUCT vin = struct { ...
    // 0. Get struct name, then eat '=', 'struct', '{'
    let vao_struct_name = eat_token("IDENTIFIER", std::token_type::l_identifier);
    eat_token("=", std::token_type::l_assign);
    eat_token("struct", std::token_type::l_struct);
    eat_token("{", std::token_type::l_left_curly_braces);
    
    // 1. Get struct item name.
    let struct_infos = []: array<(string, string)>;
    while (true)
    {
        if (try_eat_token(std::token_type::l_right_curly_braces)->has())
            // Meet '}', end work!
            break;

        let struct_member = eat_token("IDENTIFIER", std::token_type::l_identifier);
        eat_token(":", std::token_type::l_typecast);

        // Shader type only have a identifier and without template.
        let struct_shader_type = eat_token("IDENTIFIER", std::token_type::l_identifier);

        struct_infos->add((struct_member, struct_shader_type));

        if (!try_eat_token(std::token_type::l_comma)->has())
        {
            eat_token("}", std::token_type::l_right_curly_braces);
            break;
        }
    }
    // End, need a ';' here.
    eat_token(";", std::token_type::l_semicolon);

    //  OK We have current vao struct info, built struct out
    let mut out_struct_decl = F"{vao_struct_name} = struct {"{"}\n";

    for(let (vao_member_name, vao_shader_type) : struct_infos)
        out_struct_decl += F"{vao_member_name} : {vao_shader_type}, \n";

    out_struct_decl += "};\n";

    // Last step, we generate "_JE_BUILT_VAO" function here.
    out_struct_decl += 
        @"
        extern func _JE_BUILT_VAO_STRUCT(vertex_data_in: vertex_in)
        {
            return "@ 
        + vao_struct_name + " {\n";

    let mut vinid = 0;

    for(let (vao_member_name, vao_shader_type) : struct_infos)
    {
        out_struct_decl += F"{vao_member_name} = vertex_data_in->in:<{vao_shader_type}>({vinid}), \n";
        vinid += 1;
    }
    out_struct_decl += "};}\n";

    lexer->lex(out_struct_decl);
}

)";

void jegl_shader_generate_glsl(void* shader_generator, jegl_shader* write_to_shader)
{
    shader_wrapper* shader_wrapper_ptr = (shader_wrapper*)shader_generator;

    write_to_shader->m_depth_test = shader_wrapper_ptr->shader_config.m_depth_test;
    write_to_shader->m_depth_mask = shader_wrapper_ptr->shader_config.m_depth_mask;
    write_to_shader->m_alpha_test = shader_wrapper_ptr->shader_config.m_alpha_test;
    write_to_shader->m_blend_src_mode = shader_wrapper_ptr->shader_config.m_blend_src;
    write_to_shader->m_blend_dst_mode = shader_wrapper_ptr->shader_config.m_blend_dst;
    write_to_shader->m_cull_mode = shader_wrapper_ptr->shader_config.m_cull_mode;

    write_to_shader->m_vertex_glsl_src
        = jeecs::basic::make_new_string(
            _generate_glsl_vertex_by_wrapper(shader_wrapper_ptr).c_str());

    write_to_shader->m_fragment_glsl_src
        = jeecs::basic::make_new_string(
            _generate_glsl_fragment_by_wrapper(shader_wrapper_ptr).c_str());

    std::unordered_map<std::string, uniform_information*> _uniforms;
    for (auto& uniform_info : shader_wrapper_ptr->fragment_out->uniform_variables)
    {
        auto& [name, type, init_val] = uniform_info;
        _uniforms[name] = &uniform_info;
    }
    for (auto& uniform_info : shader_wrapper_ptr->vertex_out->uniform_variables)
    {
        auto& [name, type, init_val] = uniform_info;
        _uniforms[name] = &uniform_info;
    }

    jegl_shader::unifrom_variables** last = &write_to_shader->m_custom_uniforms;
    for (auto& [_/*useless*/, uniform_info] : _uniforms)
    {
        jegl_shader::unifrom_variables* variable = jeecs::basic::create_new<jegl_shader::unifrom_variables>();
        variable->m_next = nullptr;

        auto& [name, type, init_val] = *uniform_info;

        variable->m_name = jeecs::basic::make_new_string(name.c_str());
        variable->m_uniform_type = type;

        variable->m_index = jeecs::typing::INVALID_UINT32;

        if (init_val)
        {
            switch (init_val->get_type())
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
            case jegl_shader_value::type::INTEGER:
                variable->n = init_val->m_integer; break;
            case jegl_shader_value::type::TEXTURE2D:
                variable->n = init_val->m_uniform_texture_channel; break;
            default:
                jeecs::debug::log_error("Unsupport uniform variable type."); break;
            }
            variable->m_updated = true;
        }
        else
        {
            variable->x = variable->y = variable->z = variable->w = 0.f;
            variable->n = 0;
            variable->m_updated = false;
        }

        *last = variable;
        last = &variable->m_next;
    }
}

void jegl_shader_free_generated_glsl(jegl_shader* write_to_shader)
{
    je_mem_free((void*)write_to_shader->m_vertex_glsl_src);
    je_mem_free((void*)write_to_shader->m_fragment_glsl_src);

    auto* uniform_variable_info = write_to_shader->m_custom_uniforms;
    while (uniform_variable_info)
    {
        auto* current_uniform_variable = uniform_variable_info;
        uniform_variable_info = uniform_variable_info->m_next;

        je_mem_free((void*)current_uniform_variable->m_name);

        jeecs::basic::destroy_free(current_uniform_variable);
    }
}