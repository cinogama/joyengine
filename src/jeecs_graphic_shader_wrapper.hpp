#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
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
            uint32_t m_uniform_texture_channel;
            uint32_t m_binded_sampler_id;
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
        , m_uniform_texture_channel(jeecs::typing::INVALID_UINT32)
        , m_binded_sampler_id(jeecs::typing::INVALID_UINT32)
        , m_ref_count(0)
        , m_uniform_init_val_may_nil(init_val)
    {
        if (m_uniform_init_val_may_nil)
            m_uniform_init_val_may_nil->add_useref_count();
    }

    inline bool is_init_value() const noexcept
    {
        return !(m_type & CALC_VALUE);
    }
    inline bool is_calc_value() const noexcept
    {
        return !is_init_value();
    }
    inline bool is_shader_in_value() const noexcept
    {
        return m_type & SHADER_IN_VALUE;
    }
    inline bool is_uniform_variable() const noexcept
    {
        return (m_type & UNIFORM_VARIABLE) || (m_type & UNIFORM_BLOCK_VARIABLE);
    }
    inline bool is_block_uniform_variable() const noexcept
    {
        return m_type & UNIFORM_BLOCK_VARIABLE;
    }
    inline type get_type() const
    {
        return (type)(m_type & TYPE_MASK);
    }
};

struct uniform_information
{
    bool m_used_in_vertex;
    bool m_used_in_fragment;
    jegl_shader::uniform_type m_type;
    jegl_shader_value* m_value;
    std::string m_name;
};

class _shader_wrapper_contex
{
public:
    enum class generate_target
    {
        VERTEX,
        FRAGMENT,
        USER,
    };

    std::unordered_map<jegl_shader_value*, std::string> _calced_value;
    std::unordered_map<jegl_shader_value*, std::pair<size_t, std::string>> _in_value;
    int _variable_count = 0;

    std::unordered_set<std::string> _used_builtin_func;

    bool get_var_name(jegl_shader_value* val, std::string& var_name, generate_target target)
    {
        if (val->is_init_value())
            return true;

        auto fnd = _calced_value.find(val);
        if (fnd == _calced_value.end())
        {
            if (val->is_uniform_variable())
            {
                var_name = _calced_value[val] = val->m_unifrom_varname;
            }
            else if (val->is_shader_in_value())
            {
                switch (target)
                {
                case _shader_wrapper_contex::generate_target::VERTEX:
                    var_name = "_in_" + std::to_string(val->m_shader_in_index);
                    _in_value[val] = std::pair{ val->m_shader_in_index, var_name };
                    break;
                case _shader_wrapper_contex::generate_target::FRAGMENT:
                    var_name = "_v2f_" + std::to_string(val->m_shader_in_index);
                    _in_value[val] = std::pair{ val->m_shader_in_index, var_name };
                    break;
                case _shader_wrapper_contex::generate_target::USER:
                    var_name = "_in_" + std::to_string(val->m_shader_in_index);
                    break;
                default:
                    break;
                }
                _calced_value[val] = var_name;
            }
            else
            {
                var_name = "_val_" + std::to_string(_variable_count++);
                _calced_value[val] = var_name;
            }
            return true;
        }

        var_name = fnd->second;
        return false;
    }

    static jegl_shader::uniform_type get_outside_type(jegl_shader_value::type ty)
    {
        switch (ty)
        {
        case jegl_shader_value::type::INTEGER:
            return jegl_shader::uniform_type::INT;
        case jegl_shader_value::type::FLOAT:
            return jegl_shader::uniform_type::FLOAT;
        case jegl_shader_value::type::FLOAT2:
            return jegl_shader::uniform_type::FLOAT2;
        case jegl_shader_value::type::FLOAT3:
            return jegl_shader::uniform_type::FLOAT3;
        case jegl_shader_value::type::FLOAT4:
            return jegl_shader::uniform_type::FLOAT4;
        case jegl_shader_value::type::FLOAT4x4:
            return jegl_shader::uniform_type::FLOAT4X4;
        case jegl_shader_value::type::TEXTURE2D:
        case jegl_shader_value::type::TEXTURE2D_MS:
        case jegl_shader_value::type::TEXTURE_CUBE:
            return jegl_shader::uniform_type::TEXTURE;
        default:
            jeecs::debug::logerr("Unsupport uniform variable type.");
            return jegl_shader::uniform_type::INT; // return as default;
        }
    }

    static uniform_information get_uniform_info(jegl_shader_value* value)
    {
        assert(value->is_uniform_variable() && !value->is_block_uniform_variable());

        jegl_shader::uniform_type uniform_type =
            _shader_wrapper_contex::get_outside_type(value->get_type());

        return uniform_information{
            false,
            false,
            uniform_type,
            value,
            value->m_unifrom_varname,
        };
    }
};

void delete_shader_value(jegl_shader_value* shader_val);

struct shader_value_outs
{
    std::vector<jegl_shader_value*> out_values;
    ~shader_value_outs()
    {
        for (auto* val : out_values)
            delete_shader_value(val);
    }
};

struct shader_configs
{
    bool m_enable_shared;
    jegl_shader::depth_test_method m_depth_test;
    jegl_shader::depth_mask_method m_depth_mask;
    jegl_shader::blend_method m_blend_src, m_blend_dst;
    jegl_shader::cull_mode m_cull_mode;
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

struct shader_sampler
{
    jegl_shader::fliter_mode m_min;
    jegl_shader::fliter_mode m_mag;
    jegl_shader::fliter_mode m_mip;
    jegl_shader::wrap_mode m_uwrap;
    jegl_shader::wrap_mode m_vwrap;
    uint32_t m_sampler_id;

    std::vector<uint32_t> m_binded_texture_passid;
};

struct shader_wrapper
{
    struct custom_shader_src
    {
        std::string m_glsl_impl;
        std::string m_hlsl_impl;
    };

    shader_value_outs* vertex_out;
    shader_value_outs* fragment_out;
    shader_configs shader_config;

    std::vector<shader_struct_define*> shader_struct_define_may_uniform_block;
    std::vector<jegl_shader_value::type> vertex_in;

    std::vector<shader_sampler*> decleared_samplers;

    std::unordered_map<std::string, uniform_information> uniform_variables;
    std::unordered_map<std::string, custom_shader_src> custom_methods;

    struct user_function_information
    {
        std::string m_name;
        std::vector<jegl_shader_value::type> m_args;
        shader_value_outs* m_result;

        bool m_used_in_vertex;
        bool m_used_in_fragment;
    };
    std::unordered_map<std::string, user_function_information> user_define_functions;

    shader_wrapper(
        shader_value_outs* vout,
        shader_value_outs* fout)
        : vertex_out(vout)
        , fragment_out(fout)
    {

    }
    ~shader_wrapper()
    {
        for (auto* block : shader_struct_define_may_uniform_block)
            delete block;

        for (auto& userfunc : user_define_functions)
            delete userfunc.second.m_result;

        delete vertex_out;
        delete fragment_out;
    }
};

class shader_source_generator
{
public:
    using generate_target = _shader_wrapper_contex::generate_target;

protected:
    virtual std::string get_typename(jegl_shader_value::type type) = 0;
    virtual std::string generate_struct(shader_wrapper* wrapper, shader_struct_define* st) = 0;
    virtual std::string generate_uniform_block(shader_wrapper* wrapper, shader_struct_define* st) = 0;

    virtual std::string generate_code(
        _shader_wrapper_contex* context,
        jegl_shader_value* value,
        generate_target target,
        std::string* out_src) = 0;

    virtual std::string use_custom_function(const shader_wrapper::custom_shader_src& src) = 0;
    virtual std::string use_user_function(shader_wrapper* wrap, _shader_wrapper_contex* context, const shader_wrapper::user_function_information& user) = 0;

    std::string get_value_typename(jegl_shader_value* val)
    {
        return get_typename(val->get_type());
    }
    std::string generate_uniform_block_and_struct(shader_wrapper* wrap)
    {
        std::string result;

        for (auto* block : wrap->shader_struct_define_may_uniform_block)
        {
            if (!block->variables.empty())
            {
                if (block->binding_place == jeecs::typing::INVALID_UINT32)
                    result += generate_struct(wrap, block);
                else
                    result += generate_uniform_block(wrap, block);
            }
        }
        return result;
    }
    std::string generate_builtin_codes(shader_wrapper* wrap, _shader_wrapper_contex* context)
    {
        std::string builtin_functions;
        std::vector<std::string> user_functions;

        std::unordered_set<std::string> generated_function;

        while (generated_function.size() != context->_used_builtin_func.size())
        {
            for (auto& builtin_func_name : context->_used_builtin_func)
            {
                if (false == generated_function.insert(builtin_func_name).second)
                    continue;

                auto fnd = wrap->custom_methods.find(builtin_func_name);
                if (fnd != wrap->custom_methods.end())
                {
                    builtin_functions += use_custom_function(fnd->second) + "\n";
                }

                auto fnd2 = wrap->user_define_functions.find(builtin_func_name);
                if (fnd2 != wrap->user_define_functions.end())
                {
                    user_functions.push_back(use_user_function(wrap, context, fnd2->second));
                }
            }
        }

        for (auto ri = user_functions.rbegin(); ri != user_functions.rend(); ++ri)
            builtin_functions += *ri + "\n";

        return builtin_functions;
    }
public:
    virtual std::string generate_vertex(shader_wrapper* wrap) = 0;
    virtual std::string generate_fragment(shader_wrapper* wrap) = 0;
};