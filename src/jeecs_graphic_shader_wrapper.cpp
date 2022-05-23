#define JE_IMPL
#include "jeecs.hpp"
#include <functional>
#include <variant>
#include <string>
#include <unordered_map>
struct jegl_shader_value
{
    enum type : uint16_t
    {
        INIT_VALUE = 0x0000,
        CALC_VALUE = 0x0001,
        //
        TYPE_MASK = 0x0FF0,

        FLOAT = 0x0010,
        FLOAT2 = 0x0020,
        FLOAT3 = 0x0040,
        FLOAT4 = 0x0080,

        FLOAT2x2 = 0x0100,
        FLOAT3x3 = 0x0200,
        FLOAT4x4 = 0x0400,


        HAS_BEEN_USED = 0x8000,
    };

    type m_type;
    size_t m_ref_count;

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
    };

    jegl_shader_value(float init_val)
        : m_type((type)(type::FLOAT | type::INIT_VALUE))
        , m_float(init_val)
        , m_ref_count(0)
    {
    }

    void set_used_val(size_t id)
    {
    }

    template<typename T, typename ... TS>
    void set_used_val(size_t id, T&& val, TS&& ... args)
    {
        static_assert(std::is_same<T, jegl_shader_value*>::value);
        m_opnums[id] = val;
        val->m_type = (type)(val->m_type | type::HAS_BEEN_USED);
        ++val->m_ref_count;
        set_used_val(id + 1, args...);
    }

    template<typename ... TS>
    jegl_shader_value(type resulttype, const char* operat, TS&&... args)
        : m_type((type)(resulttype | type::CALC_VALUE))
        , m_opname(jeecs::basic::make_new_string(operat))
        , m_opnums_count(sizeof...(args))
    {
        m_opnums = new jegl_shader_value * [m_opnums_count];
        set_used_val(0, args...);
    }

    jegl_shader_value(type resulttype, const char* operat, size_t opnum_count)
        : m_type((type)(resulttype | type::CALC_VALUE))
        , m_opname(jeecs::basic::make_new_string(operat))
        , m_opnums_count(opnum_count)
    {
        m_opnums = new jegl_shader_value * [m_opnums_count];
    }

    inline bool is_init_value() const noexcept
    {
        return !(m_type & CALC_VALUE);
    }
    inline bool is_calc_value() const noexcept
    {
        return !is_init_value();
    }
};

void delete_shader_value(jegl_shader_value* shader_val)
{
    if (shader_val->m_type & jegl_shader_value::HAS_BEEN_USED)
    {
        if (0 == --shader_val->m_ref_count)
            // Last ref cleared, reset it to NOT USED VALUE
            shader_val->m_type = (jegl_shader_value::type)(shader_val->m_type & ~(jegl_shader_value::HAS_BEEN_USED));

        //Still have some value ref it, continue~
        return;
    }

    if (shader_val->is_calc_value())
    {
        for (size_t i = 0; i < shader_val->m_opnums_count; ++i)
            delete_shader_value(shader_val->m_opnums[i]);
        delete[]shader_val->m_opnums;

        je_mem_free((void*)shader_val->m_opname);
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
    if (rs_valuetype(args + 0) == rs_type::RS_REAL_TYPE)
        return rs_ret_gchandle(vm, new jegl_shader_value((float)rs_real(args + 0)), nullptr, _free_shader_value);
    return rs_ret_val(vm, args + 0);
}

using calc_func_t = std::function<jegl_shader_value* (size_t, jegl_shader_value**)>;
using operation_t = std::variant<jegl_shader_value::type, calc_func_t>;

const std::vector<std::pair<std::string, std::vector<operation_t>>> _operation_table =
{
    {"+", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
            [](size_t argc, jegl_shader_value** args)->jegl_shader_value*
            {
                return new jegl_shader_value(args[0]->m_float + args[1]->m_float);
            }}},
    {"-", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
            [](size_t argc, jegl_shader_value** args)->jegl_shader_value*
            {
                return new jegl_shader_value(args[0]->m_float - args[1]->m_float);
            }}},
};

struct action_node
{
    std::vector<action_node*> m_next_step;

    jegl_shader_value::type m_acceptable_type;
    calc_func_t m_reduce_const_function;

    ~action_node()
    {
        for (auto* node : m_next_step)
            delete node;
    }
    action_node*& get_next_step(jegl_shader_value::type type)
    {
        auto fnd = std::find_if(m_next_step.begin(), m_next_step.end(),
            [&](action_node* v) {if (v->m_acceptable_type == type)return true; return false; });
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
        auto& act_node = tree[action];
        if (!act_node) act_node = new action_node;

        for (auto& type_or_act : type_lists)
        {
            if (auto type = std::get_if<jegl_shader_value::type>(&type_or_act))
            {
                if (auto& new_act_node = act_node->get_next_step(*type))
                    act_node = new_act_node;
                else
                {
                    new_act_node = new action_node;
                    new_act_node->m_acceptable_type = *type;
                    act_node = new_act_node;
                }
            }
            else
            {
                if (act_node->m_reduce_const_function)
                    jeecs::debug::log_fatal("Shader operation map conflict.");
                act_node->m_reduce_const_function = std::get<calc_func_t>(type_or_act);
            }
        }
    }
}

const std::unordered_map<std::string, action_node*>& shader_operation_map() {
    const static std::unordered_map<std::string, action_node*>& _s = _generate_accpetable_tree();
    return _s;
}

calc_func_t* get_const_reduce_func(const char* op, jegl_shader_value::type* argts, size_t argc)
{
    auto& _shader_operation_map = shader_operation_map();
    auto fnd = _shader_operation_map.find(op);
    if (fnd == _shader_operation_map.end())
        return nullptr;
    auto* cur_node = fnd->second;
    for (size_t i = 0; i < argc; ++i)
    {
        if (auto* next_node = cur_node->get_next_step(argts[i]))
            cur_node = next_node;
        else
            return nullptr;
    }
    if (cur_node->m_reduce_const_function)
        return &cur_node->m_reduce_const_function;
    return nullptr;
}

RS_API rs_api jeecs_shader_apply_operation(rs_vm vm, rs_value args, size_t argc)
{
    bool result_is_const = true;
    std::vector<jegl_shader_value::type> _types(argc - 2);
    std::vector<jegl_shader_value*> _args(argc - 2);
    for (size_t i = 2; i < argc; ++i)
    {
        jegl_shader_value* sval = (jegl_shader_value*)rs_pointer(args + i);
        _types[i - 2] = sval->m_type;
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
        auto * result = (*reduce_func)(_args.size(), _args.data());
        if ((result->m_type & result_type & jegl_shader_value::TYPE_MASK) != result_type)
        {
            _free_shader_value(result);
            rs_halt("Cannot do this operations: return type dis-matched.");
            return rs_ret_nil(vm);
        }
        return rs_ret_gchandle(vm, result, nullptr, _free_shader_value);
    }

    jegl_shader_value* val =
        new jegl_shader_value(result_type, rs_string(args + 1), argc - 2);
    for (size_t i = 2; i < argc; ++i)
        val->set_used_val(i - 2, (jegl_shader_value*)rs_pointer(args + i));

    return rs_ret_gchandle(vm, val, nullptr, _free_shader_value);
}


const char* shader_wrapper_path = "je/shader.rsn";
const char* shader_wrapper_src = R"(
// JoyEngineECS RScene shader wrapper

import rscene.std;

enum shader_value_type
{
    INIT_VALUE = 0x0000,
    CALC_VALUE = 0x0001,
    //
    FLOAT = 0x0010,
    FLOAT2 = 0x0020,
    FLOAT3 = 0x0040,
    FLOAT4 = 0x0080,

    FLOAT2x2 = 0x0100,
    FLOAT3x3 = 0x0200,
    FLOAT4x4 = 0x0400,

    HAS_BEEN_USED = 0x8000,
}

using float = gchandle;
using float2 = gchandle;
using float3 = gchandle;
using float4 = gchandle;

using float2x2 = gchandle;
using float3x3 = gchandle;
using float4x4 = gchandle;

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

    std::panic("Unknown type, not shader type?");
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


namespace float
{
    extern("libjoyecs", "jeecs_shader_float_create")
    func create(var init_val:real) : float;

    extern("libjoyecs", "jeecs_shader_float_create")
    func create(var init_val:float) : float;

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
)";