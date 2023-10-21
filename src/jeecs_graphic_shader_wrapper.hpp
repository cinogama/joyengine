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

using uniform_information = std::tuple<
    std::string, jegl_shader::uniform_type, jegl_shader_value*>;

class _shader_wrapper_contex
{
public:
    std::unordered_map<jegl_shader_value*, std::string> _calced_value;
    std::unordered_map<jegl_shader_value*, std::pair<size_t, std::string>> _in_value;
    std::unordered_map<jegl_shader_value*, std::string> _uniform_value;
    int _variable_count = 0;

    std::unordered_set<std::string> _used_builtin_func;

    bool get_var_name(jegl_shader_value* val, std::string& var_name, bool is_in_fragment)
    {
        if (val->is_init_value())
            return true;

        auto fnd = _calced_value.find(val);
        if (fnd == _calced_value.end())
        {
            if (val->is_uniform_variable())
            {
                var_name = _calced_value[val] = val->m_unifrom_varname;
                if (!val->is_block_uniform_variable())
                    _uniform_value[val] = var_name;
            }
            else if (val->is_shader_in_value())
            {
                if (is_in_fragment)
                    var_name = "_v2f_" + std::to_string(val->m_shader_in_index);
                else
                    var_name = "_in_" + std::to_string(val->m_shader_in_index);

                _in_value[val] = std::pair{ val->m_shader_in_index, var_name };

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

    static uniform_information get_uniform_info(const std::string& name, jegl_shader_value* value)
    {
        jegl_shader::uniform_type uniform_type =
            _shader_wrapper_contex::get_outside_type(value->get_type());

        auto* init_value =
            uniform_type == jegl_shader::uniform_type::TEXTURE
            ? value
            : value->m_uniform_init_val_may_nil
            ;
        return std::make_tuple(name, uniform_type, init_value);
    }
};

void delete_shader_value(jegl_shader_value* shader_val);

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

struct shader_wrapper
{
    jegl_shader_value::type* vertex_in;
    size_t vertex_in_count;

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

        delete[]vertex_in;
    }
};