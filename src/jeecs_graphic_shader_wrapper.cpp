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
        TEXTURE_CUBE = 0x1000,
        TEXTURE2D_MS = 0x2000,

        INTEGER = 0x4000,

        STRUCT = 0x8000,

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

    jegl_shader_value(int init_val)
        : m_type((type)(type::INTEGER | type::INIT_VALUE))
        , m_integer(init_val)
        , m_ref_count(0)
    {
    }

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
        else if (value_type == WO_INTEGER_TYPE)
            sval = new jegl_shader_value((int)wo_int(args + i));
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

struct shader_struct_define
{
    // If binding_place == jeecs::typing::INVALID_UINT32, it is a struct define.
    // or else, it will be a uniform block declear;
    uint32_t binding_place;
    std::string name;

    struct struct_variable
    {
        std::string name;
        jegl_shader_value::type type;

        shader_struct_define* struct_type_may_nil;
    };

    std::vector<struct_variable> variables;
};

struct shader_configs
{
    jegl_shader::depth_test_method m_depth_test;
    jegl_shader::depth_mask_method m_depth_mask;
    jegl_shader::blend_method m_blend_src, m_blend_dst;
    jegl_shader::cull_mode m_cull_mode;
};

struct shader_wrapper
{
    shader_value_outs* vertex_out;
    shader_value_outs* fragment_out;
    shader_configs shader_config;
    shader_struct_define** shader_struct_define_may_uniform_block;

    ~shader_wrapper()
    {
        assert(nullptr != shader_struct_define_may_uniform_block);

        auto** block = shader_struct_define_may_uniform_block;
        while (nullptr != *block)
        {
            delete (*block);
            ++block;
        }
        delete[] shader_struct_define_may_uniform_block;

        delete vertex_out;
        delete fragment_out;
    }
};

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
        variable_member_define.struct_type_may_nil = (shader_struct_define*)wo_pointer(wo_struct_get(args + 3, 1));
    else
        variable_member_define.struct_type_may_nil = nullptr;

    block->variables.push_back(variable_member_define);

    return wo_ret_void(vm);
}

WO_API wo_api jeecs_shader_create_shader_value_out(wo_vm vm, wo_value args, size_t argc)
{
    wo_value voutstruct = args + 1;
    uint16_t structsz = 0;

    // is vertex out, check it
    if (wo_valuetype(voutstruct) != WO_STRUCT_TYPE
        || (structsz = (uint16_t)wo_lengthof(voutstruct)) == 0
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

    uint16_t fragmentin_size = (uint16_t)values->out_values.size();
    wo_value out_struct = wo_push_struct(vm, fragmentin_size);

    for (uint16_t i = 0; i < fragmentin_size; i++)
    {
        auto* val = new jegl_shader_value(values->out_values[i]->get_type());
        val->m_shader_in_index = i;
        wo_set_gchandle(wo_struct_get(out_struct, i), vm, val, nullptr, _free_shader_value);
    }

    return wo_ret_val(vm, out_struct);
}

WO_API wo_api jeecs_shader_wrap_result_pack(wo_vm vm, wo_value args, size_t argc)
{
    shader_configs config;

    config.m_depth_test = (jegl_shader::depth_test_method)wo_int(wo_struct_get(args + 2, 0));
    config.m_depth_mask = (jegl_shader::depth_mask_method)wo_int(wo_struct_get(args + 2, 1));
    config.m_blend_src = (jegl_shader::blend_method)wo_int(wo_struct_get(args + 2, 2));
    config.m_blend_dst = (jegl_shader::blend_method)wo_int(wo_struct_get(args + 2, 3));
    config.m_cull_mode = (jegl_shader::cull_mode)wo_int(wo_struct_get(args + 2, 4));

    shader_struct_define** ubos = nullptr;
    size_t ubo_count = wo_lengthof(args + 3);

    ubos = new shader_struct_define * [ubo_count + 1];
    for (size_t i = 0; i < ubo_count; ++i)
        ubos[i] = (shader_struct_define*)wo_pointer(wo_arr_get(args + 3, i));
    ubos[ubo_count] = nullptr;

    return wo_ret_gchandle(vm,
        new shader_wrapper
        {
            (shader_value_outs*)wo_pointer(args + 0),
            (shader_value_outs*)wo_pointer(args + 1),
            config,
            ubos
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
    else if (_type_is_same:<ShaderValueT, texture2dms>())
        return shader_value_type::TEXTURE2D_MS;
    else if (_type_is_same:<ShaderValueT, texturecube>())
        return shader_value_type::TEXTURE_CUBE;
    else if (_type_is_same:<ShaderValueT, integer>())
        return shader_value_type::INTEGER;
    else if (_type_is_same:<ShaderValueT, structure>())
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

public func uniform_texture<ShaderResultT>(uniform_name:string, pass: int)=> ShaderResultT
    where std::declval:<ShaderResultT>() is texture2d
        || std::declval:<ShaderResultT>() is texture2dms
        || std::declval:<ShaderResultT>() is texturecube;
{
    extern("libjoyecs", "jeecs_shader_texture2d_set_channel")
        public func channel<T>(self: T, pass: int)=> T;
    
    return channel(_uniform:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, false), pass);
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

public using vertex_in = handle;
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

public using vertex_out = handle; // nogc! will free by shader_wrapper
namespace vertex_out
{   
    func create<VertexOutT>(vout : VertexOutT)=> vertex_out
    {
        extern("libjoyecs", "jeecs_shader_create_shader_value_out")
        func _create_shader_out<VertexOutT>(is_vertex: bool, out_val: VertexOutT)=> vertex_out;

        return _create_shader_out(true, vout);
    }
}

public using fragment_in = handle;
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

namespace float
{
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
        where b is real || b is float || b is float3;
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
        where b is real || b is float || b is float4;
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
    extern("libjoyecs", "jeecs_shader_float4x4_create")
    public func new(p00:real, p01:real, p02:real, p03:real,
                p10:real, p11:real, p12:real, p13:real,
                p20:real, p21:real, p22:real, p23:real,
                p30:real, p31:real, p32:real, p33:real)=> float4x4;

    public func create(...)=> float4x4{return apply_operation:<float4x4>("float4x4", ......);}

    public func operator * <T>(a:float4x4, b:T)=> T
        where b is float4 || b is float4x4;
    {
        if (b is float4x4)
            return apply_operation:<float4x4>("*", a, b);
        else
            return apply_operation:<float4>("*", a, b);
    }
}

namespace shader
{
    public using shader_wrapper = gchandle;

    using ShaderConfig = struct {
        mut ztest     : ZConfig,
        mut zwrite    : GConfig,
        mut blend_src : BlendConfig,
        mut blend_dst : BlendConfig,
        mut cull      : CullConfig
    };
    let configs = ShaderConfig
    {
        ztest = mut LESS,
        zwrite = mut ENABLE,
        blend_src = mut ONE,
        blend_dst = mut ZERO,
        cull = mut NONE,
    };

    let struct_uniform_blocks_decls = []mut: vec<struct_define>;

    extern("libjoyecs", "jeecs_shader_wrap_result_pack")
    private func _wraped_shader(
        vertout: vertex_out, 
        fragout: fragment_out, 
        shader_config: ShaderConfig,
        struct_or_uniform_block_decl_list: array<struct_define>)=> shader_wrapper;

    private extern func generate()
    {
        // 'v_out' is a struct with member of shader variable as vertex outputs.
        let v_out = vert(_JE_BUILT_VAO_STRUCT(vertex_in::create()));
        
        // 'vertex_out' will analyze struct, then 'fragment_in' will build a new struct
        let vertext_out_result = vertex_out::create(v_out);
        let f_in = fragment_in::create:<typeof(v_out)>(vertext_out_result);
    
        // 'f_out' is a struct with output shader variable.
        let f_out = frag(f_in);
        let fragment_out_result = fragment_out::create(f_out);

        return _wraped_shader(vertext_out_result, fragment_out_result, configs, struct_uniform_blocks_decls->toarray);
    }

    namespace debug
    {
        extern("libjoyecs", "jeecs_shader_wrap_glsl_vertex")
        public func generate_glsl_vertex(wrapper:shader_wrapper)=> string;

        extern("libjoyecs", "jeecs_shader_wrap_glsl_fragment")
        public func generate_glsl_fragment(wrapper:shader_wrapper)=> string;
    }
}
public let float_zero = float::new(0.);
public let float_one = float::new(1.);
public let float2_zero = float2::new(0., 0.);
public let float2_one = float2::new(1., 1.);
public let float3_zero = float3::new(0., 0., 0.);
public let float3_one = float3::new(1., 1., 1.);
public let float4_zero = float4::new(0., 0., 0., 0.);
public let float4_one = float4::new(1., 1., 1., 1.);
public let float4x4_unit = float4x4::new(
    1., 0., 0., 0.,
    0., 1., 0., 0.,
    0., 0., 1., 0.,
    0., 0., 0., 1.);

// Default uniform
public let je_m = uniform("JOYENGINE_TRANS_M", float4x4_unit);
public let je_v = uniform("JOYENGINE_TRANS_V", float4x4_unit);
public let je_p = uniform("JOYENGINE_TRANS_P", float4x4_unit);

// je_mvp = je_p * je_v * je_m;
// je_mv  = je_v * je_m;
// je_vp  = je_p * je_v;
public let je_mvp = uniform("JOYENGINE_TRANS_MVP", float4x4_unit);
public let je_mv = uniform("JOYENGINE_TRANS_MV", float4x4_unit);
public let je_vp = uniform("JOYENGINE_TRANS_VP", float4x4_unit);

public let je_local_scale = uniform("JOYENGINE_LOCAL_SCALE", float3_one);

public let je_tiling = uniform("JOYENGINE_TEXTURE_TILING", float2_one);
public let je_offset = uniform("JOYENGINE_TEXTURE_OFFSET", float2_zero);

public let je_color = uniform("JOYENGINE_MAIN_COLOR", float4_one);

public func texture(tex:texture2d, uv:float2)=> float4
{
    return apply_operation:<float4>("texture", tex, uv);
}
public func texture_ms(tex:texture2dms, uv:float2, msaa_level: int)=> float4
{
    return apply_operation:<float4>("JEBUILTIN_TextureMs", tex, uv, msaa_level);
}
public func texture_fastms(tex:texture2dms, uv:float2)=> float4
{
    return apply_operation:<float4>("JEBUILTIN_TextureFastMs", tex, uv);
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

// Math functions

let is_glvalue<T> = 
    std::declval:<T>() is real
    || std::declval:<T>() is int
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

public func abs<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("abs", a);
}

public func negative<T>(a: T)=> T
    where is_glvalue:<T>;
{
    return apply_operation:<T>("JEBUILTIN_Negative", a);
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

public func movement(trans4x4: float4x4)=> float3
{
    return apply_operation:<float3>("JEBUILTIN_Movement", trans4x4);
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

public func alphatest(colf4: float4)=> float4
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

    CONST_COLOR,
    ONE_MINUS_CONST_COLOR,

    CONST_ALPHA,
    ONE_MINUS_CONST_ALPHA,
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
public let CONST_COLOR = BlendConfig::CONST_COLOR;
public let ONE_MINUS_CONST_COLOR = BlendConfig::ONE_MINUS_CONST_COLOR;
public let CONST_ALPHA = BlendConfig::CONST_ALPHA;
public let ONE_MINUS_CONST_ALPHA = BlendConfig::ONE_MINUS_CONST_ALPHA;

enum CullConfig
{
    NONE = 0,       /* DEFAULT */
    FRONT,
    BACK,
    ALL,
}

public let NONE = CullConfig::NONE;
public let FRONT = CullConfig::FRONT;
public let BACK = CullConfig::BACK;
public let ALL = CullConfig::ALL;

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

    lexer->lex(out_struct_decl);
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

    lexer->lex(out_struct_decl);
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

    lexer->lex(out_struct_decl);
}

UNIFORM_BUFFER! JOYENGINE_DEFAULT = 0
{
    je_time: float4,
};
)";

void jegl_shader_generate_glsl(void* shader_generator, jegl_shader* write_to_shader)
{
    shader_wrapper* shader_wrapper_ptr = (shader_wrapper*)shader_generator;

    write_to_shader->m_depth_test = shader_wrapper_ptr->shader_config.m_depth_test;
    write_to_shader->m_depth_mask = shader_wrapper_ptr->shader_config.m_depth_mask;
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
            case jegl_shader_value::type::FLOAT4x4:
                memcpy(variable->mat4x4, init_val->m_float4x4, 4 * 4 * sizeof(float));
                break;
            case jegl_shader_value::type::INTEGER:
                variable->n = init_val->m_integer; break;
            case jegl_shader_value::type::TEXTURE2D:
            case jegl_shader_value::type::TEXTURE2D_MS:
            case jegl_shader_value::type::TEXTURE_CUBE:
                variable->n = init_val->m_uniform_texture_channel; break;
            default:
                jeecs::debug::logerr("Unsupport uniform variable type."); break;
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