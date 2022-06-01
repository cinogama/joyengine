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
                rs_fail(0xD000, "Unknown type to init, should be f");
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

    jegl_shader_value(type resulttype, const char* uniform_name, bool is_predef)
        : m_type((type)(resulttype | type::CALC_VALUE | (is_predef ? type::UNIFORM_BLOCK_VARIABLE : type::UNIFORM_VARIABLE)))
        , m_unifrom_varname(jeecs::basic::make_new_string(uniform_name))
        , m_ref_count(0)
    {
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

RS_API rs_api jeecs_shader_float_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm, new jegl_shader_value((float)rs_real(args + 0)), nullptr, _free_shader_value);
}
RS_API rs_api jeecs_shader_float2_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        new jegl_shader_value(
            (float)rs_real(args + 0),
            (float)rs_real(args + 1)), nullptr, _free_shader_value);
}
RS_API rs_api jeecs_shader_float3_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        new jegl_shader_value(
            (float)rs_real(args + 0),
            (float)rs_real(args + 1),
            (float)rs_real(args + 2)), nullptr, _free_shader_value);
}
RS_API rs_api jeecs_shader_float4_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        new jegl_shader_value(
            (float)rs_real(args + 0),
            (float)rs_real(args + 1),
            (float)rs_real(args + 2),
            (float)rs_real(args + 3)), nullptr, _free_shader_value);
}
RS_API rs_api jeecs_shader_float4x4_create(rs_vm vm, rs_value args, size_t argc)
{
    float data[16] = {};
    for (size_t i = 0; i < 16; i++)
        data[i] = (float)rs_real(args + i);
    return rs_ret_gchandle(vm,
        new jegl_shader_value(data, jegl_shader_value::FLOAT4x4),
        nullptr, _free_shader_value);
}
RS_API rs_api jeecs_shader_create_rot_mat4x4(rs_vm vm, rs_value args, size_t argc)
{
    float data[16] = {};
    jeecs::math::quat q((float)rs_real(args + 0), (float)rs_real(args + 1), (float)rs_real(args + 2));
    q.create_matrix(data);
    return rs_ret_gchandle(vm,
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

RS_API rs_api jeecs_shader_create_uniform_variable(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        new jegl_shader_value((jegl_shader_value::type)rs_int(args + 0), rs_string(args + 1), rs_bool(args + 2))
        , nullptr, _free_shader_value);
}
RS_API rs_api jeecs_shader_apply_operation(rs_vm vm, rs_value args, size_t argc)
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
        auto value_type = rs_valuetype(args + i);
        if (value_type != RS_INTEGER_TYPE && value_type != RS_REAL_TYPE && value_type != RS_GCHANDLE_TYPE)
        {
            rs_halt("Cannot do this operations: argument type should be number or shader_value.");
            return rs_ret_nil(vm);
        }

        jegl_shader_value* sval;
        if (value_type == RS_GCHANDLE_TYPE)
            sval = (jegl_shader_value*)rs_pointer(args + i);
        else
        {
            sval = new jegl_shader_value((float)rs_cast_real(args + i));
            tmp_svalue.push_back(sval);
        }
        _types[i - 2] = sval->get_type();
        _args[i - 2] = sval;

        if (sval->is_calc_value())
            result_is_const = false;
    }
    jegl_shader_value::type result_type = (jegl_shader_value::type)rs_int(args + 0);
    rs_string_t operation = rs_string(args + 1);
    auto* reduce_func = get_const_reduce_func(operation, _types.data(), _types.size());
    if (!reduce_func)
    {
        rs_halt("Cannot do this operations: no matched operation with these types.");
        return rs_ret_nil(vm);
    }

    if (result_is_const)
    {
        auto* result = (*reduce_func)(_args.size(), _args.data());
        if (result) // if result == nullptr, there is no method for constant, calc it in shader~
        {
            if (result->get_type() != result_type)
            {
                _free_shader_value(result);
                rs_halt("Cannot do this operations: return type dis-matched.");
                return rs_ret_nil(vm);
            }
            return rs_ret_gchandle(vm, result, nullptr, _free_shader_value);
        }
    }

    jegl_shader_value* val =
        new jegl_shader_value(result_type, rs_string(args + 1), argc - 2);
    for (size_t i = 2; i < argc; ++i)
        val->set_used_val(i - 2, _args[i - 2]);

    return rs_ret_gchandle(vm, val, nullptr, _free_shader_value);
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

RS_API rs_api jeecs_shader_create_vertex_in(rs_vm vm, rs_value args, size_t argc)
{
    // This function is used for debug
    return rs_ret_gchandle(vm, new vertex_in_data_storage, nullptr, [](void* ptr) {
        delete (vertex_in_data_storage*)ptr;
        });
}

RS_API rs_api jeecs_shader_get_vertex_in(rs_vm vm, rs_value args, size_t argc)
{
    vertex_in_data_storage* storage = (vertex_in_data_storage*)rs_pointer(args + 0);
    jegl_shader_value::type type = (jegl_shader_value::type)rs_int(args + 1);
    size_t pos = (size_t)rs_int(args + 2);

    auto* result = storage->get_val_at(pos, type);
    if (!result)
        rs_halt(("vertex_in[" + std::to_string(pos) + "] has been used, but type didn't match.").c_str());

    return rs_ret_gchandle(vm, result, nullptr, nullptr);
}

RS_API rs_api jeecs_shader_set_vertex_out(rs_vm vm, rs_value args, size_t argc)
{
    jegl_shader_value* vertex_out_pos = (jegl_shader_value*)rs_pointer(args + 0);
    if (vertex_out_pos->get_type() != jegl_shader_value::FLOAT4)
        rs_halt("First value of vertex_out must be FLOAT4 for position.");

    return rs_ret_nil(vm);
}

struct shader_value_outs
{
    std::vector<jegl_shader_value*> out_values;
    ~shader_value_outs()
    {
        for (auto* val : out_values)
            delete_shader_value(val);
    }
};

struct shader_wrapper
{
    shader_value_outs* vertex_out;
    shader_value_outs* fragment_out;

    ~shader_wrapper()
    {
        delete vertex_out;
        delete fragment_out;
    }
};

RS_API rs_api jeecs_shader_create_shader_value_out(rs_vm vm, rs_value args, size_t argc)
{
    shader_value_outs* values = new shader_value_outs;
    values->out_values.resize(argc);
    for (size_t i = 0; i < argc; i++)
    {
        values->out_values[i] = (jegl_shader_value*)rs_pointer(args + i);
        values->out_values[i]->add_useref_count();
    }
    return rs_ret_pointer(vm, values);
}
RS_API rs_api jeecs_shader_create_fragment_in(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_val(vm, args + 0);
}
RS_API rs_api jeecs_shader_get_fragment_in(rs_vm vm, rs_value args, size_t argc)
{
    shader_value_outs* values = (shader_value_outs*)rs_pointer(args + 0);
    jegl_shader_value::type type = (jegl_shader_value::type)rs_int(args + 1);
    size_t pos = (size_t)rs_int(args + 2);

    if (pos >= values->out_values.size())
        rs_halt(("fragment_in[" + std::to_string(pos) + "] out of range.").c_str());

    auto* result = values->out_values[pos];
    if (result->get_type() != type)
        rs_halt(("fragment_in[" + std::to_string(pos) + "] type didn't match.").c_str());

    auto* val = new jegl_shader_value(type);
    val->m_shader_in_index = pos;
    return rs_ret_gchandle(vm, val, nullptr, _free_shader_value);
}
RS_API rs_api jeecs_shader_wrap_result_pack(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm, new shader_wrapper{
        (shader_value_outs*)rs_pointer(args + 0),
        (shader_value_outs*)rs_pointer(args + 1)
        }, nullptr,
        [](void* ptr) {
            delete (shader_wrapper*)ptr;
        });
}

#include "jeecs_graphic_shader_wrapper_glsl.hpp"

std::string _generate_glsl_vertex_by_wrapper(shader_wrapper* wrap)
{
    return _generate_code_for_glsl_vertex(wrap->vertex_out);
}

std::string _generate_glsl_fragment_by_wrapper(shader_wrapper* wrap)
{
    return _generate_code_for_glsl_fragment(wrap->fragment_out);
}

RS_API rs_api jeecs_shader_wrap_glsl_vertex(rs_vm vm, rs_value args, size_t argc)
{
    shader_wrapper* wrap = (shader_wrapper*)rs_pointer(args + 0);
    return rs_ret_string(vm, _generate_glsl_vertex_by_wrapper(wrap).c_str());
}

RS_API rs_api jeecs_shader_wrap_glsl_fragment(rs_vm vm, rs_value args, size_t argc)
{
    shader_wrapper* wrap = (shader_wrapper*)rs_pointer(args + 0);
    return rs_ret_string(vm, _generate_glsl_fragment_by_wrapper(wrap).c_str());
}

const char* shader_wrapper_path = "je/shader.rsn";
const char* shader_wrapper_src = R"(
// JoyEngineECS RScene shader wrapper

import rscene.std;

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

}

using float = gchandle;
using float2 = gchandle;
using float3 = gchandle;
using float4 = gchandle;

using float2x2 = gchandle;
using float3x3 = gchandle;
using float4x4 = gchandle;

using texture2d = gchandle;

protected func _type_is_same<AT, BT>() : bool
{
    if (func():AT{}() is BT)
        return true;
    return false;
}

protected func _get_type_enum<ShaderValueT>() : shader_value_type
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

    std::halt("Unknown type, not shader type?");
}

extern("libjoyecs", "jeecs_shader_apply_operation")
protected func _apply_operation<ShaderResultT>(
    var result_type : shader_value_type,
    var operation_name : string,
    ...
) : ShaderResultT;

protected func apply_operation<ShaderResultT>(var operation_name:string, ...) 
    : ShaderResultT
{
    return _apply_operation:<ShaderResultT>(
                _get_type_enum:<ShaderResultT>(), 
                operation_name, ......);
}

extern("libjoyecs", "jeecs_shader_create_uniform_variable")
protected func _uniform<ShaderResultT>(
    var result_type : shader_value_type,
    var uniform_name : string,
    var is_uniform_block : bool
) : ShaderResultT;

func uniform<ShaderResultT>(var uniform_name:string) : ShaderResultT
{
    return _uniform:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, false);
}

func shared_uniform<ShaderResultT>(var uniform_name:string) : ShaderResultT
{
    return _uniform:<ShaderResultT>(_get_type_enum:<ShaderResultT>(), uniform_name, true);
}

extern("libjoyecs", "jeecs_shader_create_rot_mat4x4")
func rotation(var x:real, var y:real, var z:real) : float4x4;

using vertex_in = handle;
namespace vertex_in
{
    extern("libjoyecs", "jeecs_shader_create_vertex_in")
    func create() : vertex_in;

    extern("libjoyecs", "jeecs_shader_get_vertex_in")
    private func _in<ValueT>(var self:vertex_in, var type:shader_value_type, var id:int) : ValueT;

    func in<ValueT>(var self:vertex_in, var id:int) : ValueT
    {
        return self->_in:<ValueT>(_get_type_enum:<ValueT>(), id) as ValueT;
    }
}

using vertex_out = handle; // nogc! will free by shader_wrapper
namespace vertex_out
{
    extern("libjoyecs", "jeecs_shader_create_shader_value_out")
    func create(var position : float4, ...) : vertex_out;
}

using fragment_in = handle;
namespace fragment_in
{
    extern("libjoyecs", "jeecs_shader_create_fragment_in")
    func create(var data_from_vert:vertex_out) : fragment_in;

    extern("libjoyecs", "jeecs_shader_get_fragment_in")
    private func _in<ValueT>(var self:fragment_in, var type:shader_value_type, var id:int) : ValueT;

    func in<ValueT>(var self:fragment_in, var id:int) : ValueT
    {
        return self->_in:<ValueT>(_get_type_enum:<ValueT>(), id) as ValueT;
    }
}

using fragment_out = handle; // nogc! will free by shader_wrapper
namespace fragment_out
{
    extern("libjoyecs", "jeecs_shader_create_shader_value_out")
    func create(...):fragment_out;
}

namespace float
{
    extern("libjoyecs", "jeecs_shader_float_create")
    func create(var init_val:real) : float;

    func create(...) : float{return apply_operation:<float>("float", ......);}

    func operator + (var a:float, var b:float) : float
    {
        return apply_operation:<float>("+", a, b);
    }
    func operator - (var a:float, var b:float) : float
    {
        return apply_operation:<float>("-", a, b);
    }
    func operator * (var a:float, var b:float) : float
    {
        return apply_operation:<float>("*", a, b);
    }
    func operator / (var a:float, var b:float) : float
    {
        return apply_operation:<float>("/", a, b);
    }
}
namespace float2
{
    extern("libjoyecs", "jeecs_shader_float2_create")
    func create(var x:real, var y:real) : float2;

    func create(...) : float2{return apply_operation:<float2>("float2", ......);}

    func x(var self:float2) : float{return apply_operation:<float>(".x", self);}
    func y(var self:float2) : float{return apply_operation:<float>(".y", self);}
    func xy(var self:float2) : float2{return apply_operation:<float2>(".xy", self);}
    func yx(var self:float2) : float2{return apply_operation:<float2>(".yx", self);}
}
namespace float3
{
    extern("libjoyecs", "jeecs_shader_float3_create")
    func create(var x:real, var y:real, var z:real) : float3;

    func create(...) : float3{return apply_operation:<float3>("float3", ......);}

    func x(var self:float3) : float{return apply_operation:<float>(".x", self);}
    func y(var self:float3) : float{return apply_operation:<float>(".y", self);}
    func z(var self:float3) : float{return apply_operation:<float>(".z", self);}
    func xy(var self:float3) : float2{return apply_operation:<float2>(".xy", self);}
    func yz(var self:float3) : float2{return apply_operation:<float2>(".yz", self);}
    func xz(var self:float3) : float2{return apply_operation:<float2>(".xz", self);}
    func yx(var self:float3) : float2{return apply_operation:<float2>(".yx", self);}
    func zy(var self:float3) : float2{return apply_operation:<float2>(".zy", self);}
    func zx(var self:float3) : float2{return apply_operation:<float2>(".zx", self);}
    func xyz(var self:float3) : float3{return apply_operation:<float3>(".xyz", self);}
    func xzy(var self:float3) : float3{return apply_operation:<float3>(".xzy", self);}
    func yxz(var self:float3) : float3{return apply_operation:<float3>(".yxz", self);}
    func yzx(var self:float3) : float3{return apply_operation:<float3>(".yzx", self);}
    func zxy(var self:float3) : float3{return apply_operation:<float3>(".zxy", self);}
    func zyx(var self:float3) : float3{return apply_operation:<float3>(".zyx", self);}
}
namespace float4
{
    extern("libjoyecs", "jeecs_shader_float4_create")
    func create(var x:real, var y:real, var z:real, var w:real) : float4;

    func create(...) : float4{return apply_operation:<float4>("float4", ......);}

    func x(var self:float4) : float{return apply_operation:<float>(".x", self);}
    func y(var self:float4) : float{return apply_operation:<float>(".y", self);}
    func z(var self:float4) : float{return apply_operation:<float>(".z", self);}
    func w(var self:float4) : float{return apply_operation:<float>(".w", self);}

    func xy(var self:float4) : float2{return apply_operation:<float2>(".xy", self);}
    func yz(var self:float4) : float2{return apply_operation:<float2>(".yz", self);}
    func xz(var self:float4) : float2{return apply_operation:<float2>(".xz", self);}
    func yx(var self:float4) : float2{return apply_operation:<float2>(".yx", self);}
    func zy(var self:float4) : float2{return apply_operation:<float2>(".zy", self);}
    func zx(var self:float4) : float2{return apply_operation:<float2>(".zx", self);}
    func xw(var self:float4) : float2{return apply_operation:<float2>(".xw", self);}
    func wx(var self:float4) : float2{return apply_operation:<float2>(".wx", self);}
    func yw(var self:float4) : float2{return apply_operation:<float2>(".yw", self);}
    func wy(var self:float4) : float2{return apply_operation:<float2>(".wy", self);}
    func zw(var self:float4) : float2{return apply_operation:<float2>(".zw", self);}
    func wz(var self:float4) : float2{return apply_operation:<float2>(".wz", self);}

    func xyz(var self:float4) : float3{return apply_operation:<float3>(".xyz", self);}
    func xzy(var self:float4) : float3{return apply_operation:<float3>(".xzy", self);}
    func yxz(var self:float4) : float3{return apply_operation:<float3>(".yxz", self);}
    func yzx(var self:float4) : float3{return apply_operation:<float3>(".yzx", self);}
    func zxy(var self:float4) : float3{return apply_operation:<float3>(".zxy", self);}
    func zyx(var self:float4) : float3{return apply_operation:<float3>(".zyx", self);}
    func wyz(var self:float4) : float3{return apply_operation:<float3>(".wyz", self);}
    func wzy(var self:float4) : float3{return apply_operation:<float3>(".wzy", self);}
    func ywz(var self:float4) : float3{return apply_operation:<float3>(".ywz", self);}
    func yzw(var self:float4) : float3{return apply_operation:<float3>(".yzw", self);}
    func zwy(var self:float4) : float3{return apply_operation:<float3>(".zwy", self);}
    func zyw(var self:float4) : float3{return apply_operation:<float3>(".zyw", self);}
    func xwz(var self:float4) : float3{return apply_operation:<float3>(".xwz", self);}
    func xzw(var self:float4) : float3{return apply_operation:<float3>(".xzw", self);}
    func wxz(var self:float4) : float3{return apply_operation:<float3>(".wxz", self);}
    func wzx(var self:float4) : float3{return apply_operation:<float3>(".wzx", self);}
    func zxw(var self:float4) : float3{return apply_operation:<float3>(".zxw", self);}
    func zwx(var self:float4) : float3{return apply_operation:<float3>(".zwx", self);}
    func xyw(var self:float4) : float3{return apply_operation:<float3>(".xyw", self);}
    func xwy(var self:float4) : float3{return apply_operation:<float3>(".xwy", self);}
    func yxw(var self:float4) : float3{return apply_operation:<float3>(".yxw", self);}
    func ywx(var self:float4) : float3{return apply_operation:<float3>(".ywx", self);}
    func wxy(var self:float4) : float3{return apply_operation:<float3>(".wxy", self);}
    func wyx(var self:float4) : float3{return apply_operation:<float3>(".wyx", self);}

    func xyzw(var self:float4) : float4{return apply_operation:<float3>(".xyzw", self);}
    func xzyw(var self:float4) : float4{return apply_operation:<float3>(".xzyw", self);}
    func yxzw(var self:float4) : float4{return apply_operation:<float3>(".yxzw", self);}
    func yzxw(var self:float4) : float4{return apply_operation:<float3>(".yzxw", self);}
    func zxyw(var self:float4) : float4{return apply_operation:<float3>(".zxyw", self);}
    func zyxw(var self:float4) : float4{return apply_operation:<float3>(".zyxw", self);}
    func wyzx(var self:float4) : float4{return apply_operation:<float3>(".wyzx", self);}
    func wzyx(var self:float4) : float4{return apply_operation:<float3>(".wzyx", self);}
    func ywzx(var self:float4) : float4{return apply_operation:<float3>(".ywzx", self);}
    func yzwx(var self:float4) : float4{return apply_operation:<float3>(".yzwx", self);}
    func zwyx(var self:float4) : float4{return apply_operation:<float3>(".zwyx", self);}
    func zywx(var self:float4) : float4{return apply_operation:<float3>(".zywx", self);}
    func xwzy(var self:float4) : float4{return apply_operation:<float3>(".xwzy", self);}
    func xzwy(var self:float4) : float4{return apply_operation:<float3>(".xzwy", self);}
    func wxzy(var self:float4) : float4{return apply_operation:<float3>(".wxzy", self);}
    func wzxy(var self:float4) : float4{return apply_operation:<float3>(".wzxy", self);}
    func zxwy(var self:float4) : float4{return apply_operation:<float3>(".zxwy", self);}
    func zwxy(var self:float4) : float4{return apply_operation:<float3>(".zwxy", self);}
    func xywz(var self:float4) : float4{return apply_operation:<float3>(".xywz", self);}
    func xwyz(var self:float4) : float4{return apply_operation:<float3>(".xwyz", self);}
    func yxwz(var self:float4) : float4{return apply_operation:<float3>(".yxwz", self);}
    func ywxz(var self:float4) : float4{return apply_operation:<float3>(".ywxz", self);}
    func wxyz(var self:float4) : float4{return apply_operation:<float3>(".wxyz", self);}
    func wyxz(var self:float4) : float4{return apply_operation:<float3>(".wyxz", self);}
    
}
namespace float4x4
{
    extern("libjoyecs", "jeecs_shader_float4x4_create")
    func create(var p00:real, var p01:real, var p02:real, var p03:real,
                var p10:real, var p11:real, var p12:real, var p13:real,
                var p20:real, var p21:real, var p22:real, var p23:real,
                var p30:real, var p31:real, var p32:real, var p33:real): float4x4;

    func create(...) : float4x4{return apply_operation:<float4x4>("float4x4", ......);}

    func operator * (var a:float4x4, var b:float4x4) : float4x4
    {
        return apply_operation:<float4x4>("*", a, b);
    }

    func operator * (var a:float4x4, var b:float4) : float4
    {
        return apply_operation:<float4>("*", a, b);
    }
}

namespace shader
{
    using shader_wrapper = gchandle;

    extern("libjoyecs", "jeecs_shader_wrap_result_pack")
    private func _wraped_shader(var vertout, var fragout) : shader_wrapper;

    protected extern func generate()
    {
        var vertex_out = vert(vertex_in());
        var fragment_out = frag(fragment_in(vertex_out));

        return _wraped_shader(vertex_out, fragment_out);
    }

    namespace debug
    {
        extern("libjoyecs", "jeecs_shader_wrap_glsl_vertex")
        func generate_glsl_vertex(var wrapper:shader_wrapper) : string;

        extern("libjoyecs", "jeecs_shader_wrap_glsl_fragment")
        func generate_glsl_fragment(var wrapper:shader_wrapper) : string;
    }
}

// Default unifrom
var je_time = shared_uniform:<float4>("JOYENGINE_TIMES");

var je_mt = uniform:<float4x4>("JOYENGINE_TRANS_M_TRANSLATE");
var je_mr = uniform:<float4x4>("JOYENGINE_TRANS_M_ROTATION");

var je_vt = uniform:<float4x4>("JOYENGINE_TRANS_V_TRANSLATE");
var je_vr = uniform:<float4x4>("JOYENGINE_TRANS_V_ROTATION");

var je_m = uniform:<float4x4>("JOYENGINE_TRANS_M");
var je_v = uniform:<float4x4>("JOYENGINE_TRANS_V");
var je_p = uniform:<float4x4>("JOYENGINE_TRANS_P");

var je_mvp = uniform:<float4x4>("JOYENGINE_TRANS_MVP");
var je_mv = uniform:<float4x4>("JOYENGINE_TRANS_MV");
var je_vp = uniform:<float4x4>("JOYENGINE_TRANS_VP");

func texture(var tex:texture2d, var uv:float2):float4
{
    return apply_operation:<float4>("texture", tex, uv);
};

)";

void jegl_shader_generate_glsl(void* shader_generator, jegl_shader* write_to_shader)
{
    write_to_shader->m_vertex_glsl_src
        = jeecs::basic::make_new_string(
            _generate_glsl_vertex_by_wrapper((shader_wrapper*)shader_generator).c_str());

    write_to_shader->m_fragment_glsl_src
        = jeecs::basic::make_new_string(
            _generate_glsl_fragment_by_wrapper((shader_wrapper*)shader_generator).c_str());
}