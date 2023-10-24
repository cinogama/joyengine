#pragma once

#include "jeecs_graphic_shader_wrapper.hpp"

namespace jeecs
{
    namespace shader_generator
    {
        namespace hlsl
        {
            std::string get_type_name_from_type(jegl_shader_value::type ty)
            {
                switch (ty)
                {
                case jegl_shader_value::type::FLOAT:
                    return "float";
                case jegl_shader_value::type::FLOAT2:
                    return "float2";
                case jegl_shader_value::type::FLOAT3:
                    return "float3";
                case jegl_shader_value::type::FLOAT4:
                    return "float4";
                case jegl_shader_value::type::FLOAT2x2:
                    return "float2x2";
                case jegl_shader_value::type::FLOAT3x3:
                    return "float3x3";
                case jegl_shader_value::type::FLOAT4x4:
                    return "float4x4";
                case jegl_shader_value::type::TEXTURE2D:
                    return "Texture2D";
                case jegl_shader_value::type::TEXTURE2D_MS:
                    return "Texture2DMS";
                case jegl_shader_value::type::TEXTURE_CUBE:
                    return "TextureCube";
                default:
                    jeecs::debug::logerr("Unknown value type when generating hlsl, please check!");
                    return "unknown";
                    break;
                }
            }
            std::string get_type_name(jegl_shader_value* val)
            {
                return get_type_name_from_type(val->get_type());
            }

            std::string _generate_uniform_block_for_hlsl(shader_wrapper* wrap)
            {
                std::string result;

                for (auto* block : wrap->shader_struct_define_may_uniform_block)
                {
                    if (!block->variables.empty())
                    {
                        std::string uniform_block_decl =
                            block->binding_place == jeecs::typing::INVALID_UINT32
                            ? "struct " + block->name + "\n{\n"
                            : "cbuffer " + block->name + ": register(b" + std::to_string(block->binding_place + 1) + ")\n{\n";

                        for (auto& variable_inform : block->variables)
                            if (variable_inform.type == jegl_shader_value::type::STRUCT)
                                uniform_block_decl += "    " + variable_inform.struct_type_may_nil->name + " " + variable_inform.name + ";\n";
                            else
                                uniform_block_decl += "    " + get_type_name_from_type(variable_inform.type) + " " + variable_inform.name + ";\n";

                        result += uniform_block_decl + "};\n";
                    }
                }

                return result;
            }

            std::string _generate_code_for_hlsl_impl(
                _shader_wrapper_contex* contex,
                std::string& out,
                jegl_shader_value* value,
                bool is_fragment)
            {
                using namespace std;

                std::string varname;
                if (contex->get_var_name(value, varname, is_fragment))
                {
                    if (value->is_calc_value())
                    {
                        if (value->is_shader_in_value())
                        {
                            // DO NOTHING;
                        }
                        else if (value->is_uniform_variable())
                        {
                            // DO NOTHING;
                        }
                        else
                        {
                            std::string apply = "    " + get_type_name(value) + " " + varname + " = ";

                            std::vector<std::string> variables;
                            for (size_t i = 0; i < value->m_opnums_count; i++)
                                variables.push_back(_generate_code_for_hlsl_impl(contex, out, value->m_opnums[i], is_fragment));

                            if (value->m_opname == "+"s
                                || value->m_opname == "-"s
                                || value->m_opname == "/"s)
                            {
                                assert(variables.size() == 2);
                                apply += variables[0] + " " + value->m_opname + " " + variables[1];
                            }
                            else if (value->m_opname == "*"s)
                            {
                                assert(variables.size() == 2);
                                if (value->m_opnums[0]->get_type() == jegl_shader_value::type::FLOAT2x2
                                    || value->m_opnums[0]->get_type() == jegl_shader_value::type::FLOAT3x3
                                    || value->m_opnums[0]->get_type() == jegl_shader_value::type::FLOAT4x4)
                                    apply += "mul(" + variables[0] + "," + variables[1] + ")";
                                else
                                {
                                    apply += variables[0] + " " + value->m_opname + " " + variables[1];
                                }
                            }
                            else if (value->m_opname[0] == '.')
                            {
                                assert(variables.size() == 1);
                                apply += variables[0] + value->m_opname;
                            }
                            else
                            {
                                std::string funcname = value->m_opname;
                                //if (funcname == "float2")
                                //    funcname = "vec2";
                                //else if (funcname == "float3")
                                //    funcname = "vec3";
                                //else if (funcname == "float4")
                                //    funcname = "vec4";
                                //else if (funcname == "float2x2")
                                //    funcname = "mat2";
                                //else if (funcname == "float3x3")
                                //    funcname = "mat3";
                                //else if (funcname == "float4x4")
                                //    funcname = "mat4";
                                //else if (funcname == "lerp")
                                //    funcname = "mix";

                                // 此处塞函数转发，部分函数的姿势和gl不一样，为了保持上层一致性，在此转发！
                                if (funcname == "texture")
                                {
                                    assert(variables.size() == 2);
                                    apply +=
                                        variables[0]
                                        + ".Sample(sampler_"
                                        + std::to_string(value->m_opnums[0]->m_binded_sampler_id)
                                        + ", "
                                        + variables[1]
                                        + ")";
                                }
                                else
                                {
                                    if (funcname.find("JEBUILTIN_") == 0)
                                        contex->_used_builtin_func.insert(funcname);
                                    apply += funcname + "(";
                                    for (size_t i = 0; i < variables.size(); i++)
                                    {
                                        apply += variables[i];
                                        if (i + 1 != variables.size())
                                            apply += ", ";
                                    }
                                    apply += ")";
                                }
                            }
                            apply += ";";

                            out += apply + "\n";
                        }
                    }
                    else
                    {
                        std::string apply/* = "    const " + _shader_wrapper_contex::get_type_name(value) + " " + varname + " = "*/;

                        switch (value->get_type())
                        {
                        case jegl_shader_value::type::INTEGER:
                            apply += std::to_string(value->m_integer);
                            break;
                        case jegl_shader_value::type::FLOAT:
                            apply += std::to_string(value->m_float);
                            break;
                        case jegl_shader_value::type::FLOAT2:
                            apply += "float2(" + std::to_string(value->m_float2[0]) + "," + std::to_string(value->m_float2[1]) + ")";
                            break;
                        case jegl_shader_value::type::FLOAT3:
                            apply += "float3(" + std::to_string(value->m_float3[0]) + "," + std::to_string(value->m_float3[1]) + "," + std::to_string(value->m_float3[2]) + ")";
                            break;
                        case jegl_shader_value::type::FLOAT4:
                            apply += "float4(" + std::to_string(value->m_float4[0]) + "," + std::to_string(value->m_float4[1]) + "," + std::to_string(value->m_float4[2]) + "," + std::to_string(value->m_float4[3]) + ")";
                            break;
                        case jegl_shader_value::type::FLOAT2x2:
                        {
                            std::string result = "float2x2(";
                            for (size_t iy = 0; iy < 2; iy++)
                                for (size_t ix = 0; ix < 2; ix++)
                                    result += std::to_string(value->m_float2x2[ix][iy]) + ((ix == 1 && iy == 1) ? ")" : ",");

                            apply += result;
                            break;
                        }
                        case jegl_shader_value::type::FLOAT3x3:
                        {
                            std::string result = "float3x3(";
                            for (size_t iy = 0; iy < 3; iy++)
                                for (size_t ix = 0; ix < 3; ix++)
                                    result += std::to_string(value->m_float3x3[ix][iy]) + ((ix == 2 && iy == 2) ? ")" : ",");

                            apply += result;
                            break;
                        }
                        case jegl_shader_value::type::FLOAT4x4:
                        {
                            std::string result = "float4x4(";
                            for (size_t iy = 0; iy < 4; iy++)
                                for (size_t ix = 0; ix < 4; ix++)
                                    result += std::to_string(value->m_float4x4[ix][iy]) + ((ix == 3 && iy == 3) ? ")" : ",");

                            apply += result;
                            break;
                        }
                        default:
                            jeecs::debug::logerr("Unknown constant value when generating hlsl, please check!");
                            apply += "invalid";
                            break;
                        }

                        /*out += apply + ";\n";*/
                        return apply;
                    }
                }

                if (value->is_shader_in_value())
                {
                    if (is_fragment)
                        varname = "_v2f." + varname;
                    else
                        varname = "_vin." + varname;
                }

                return varname;
            }

            std::string _hlsl_pragma()
            {
                return R"()";
            }

            std::string _generate_code_for_hlsl_vertex(shader_wrapper* wrap)
            {
                _shader_wrapper_contex contex;
                std::string          body_result;
                std::string          io_declear;

                const std::string    unifrom_block = _generate_uniform_block_for_hlsl(wrap);

                std::vector<std::pair<jegl_shader_value*, std::string>> outvalue;
                for (auto* out_val : wrap->vertex_out->out_values)
                    outvalue.push_back(std::make_pair(out_val, _generate_code_for_hlsl_impl(&contex, body_result, out_val, false)));

                // Generate built function src here.
                std::string built_in_srcs;
                for (auto* sampler : wrap->decleared_samplers)
                {
                    built_in_srcs += "SamplerState sampler_"
                        + std::to_string(sampler->m_sampler_id)
                        + ": register(s"
                        + std::to_string(sampler->m_sampler_id)
                        + ");\n";
                }

                for (auto& builtin_func_name : contex._used_builtin_func)
                {
                     if (builtin_func_name == "JEBUILTIN_Movement")
                     {
                         const std::string unifrom_block = R"(
float3 JEBUILTIN_Movement(float4x4 trans)
{
    return trans[3].xyz;
}
)";
                         built_in_srcs += unifrom_block;
                     }
                     else if (builtin_func_name == "JEBUILTIN_Negative")
                     {
                         const std::string unifrom_block = R"(
float JEBUILTIN_Negative(float v)
{
    return -v;
}
float2 JEBUILTIN_Negative(float2 v)
{
    return -v;
}
float3 JEBUILTIN_Negative(float3 v)
{
    return -v;
}
float4 JEBUILTIN_Negative(float4 v)
{
    return -v;
}
)";
                         built_in_srcs += unifrom_block;
                     }
                     else if (builtin_func_name == "JEBUILTIN_Uvframebuffer")
                     {
                         const std::string unifrom_block = R"(
float2 JEBUILTIN_Uvframebuffer(float2 v)
{
    return float2(v.x, 1.0 - v.y);
}
)";
                         built_in_srcs += unifrom_block;
                     }
                }

                // Generate shaders
                std::string texture_decl = "";
                std::string uniform_decl = "";
                size_t uniform_count = 0;

                uniform_decl += "cbuffer SHADER_UNIFORM: register(b0)\n{\n";
                for (auto& [name, uinfo] : wrap->uniform_variables)
                {
                    auto value_type = uinfo.m_value->get_type();
                    if (value_type == jegl_shader_value::type::TEXTURE2D
                        || value_type == jegl_shader_value::type::TEXTURE2D_MS
                        || value_type == jegl_shader_value::type::TEXTURE_CUBE)
                    {
                        if (uinfo.m_used_in_vertex)
                        {
                            texture_decl
                                += get_type_name_from_type(value_type)
                                + " "
                                + name
                                + ": register(t"
                                + std::to_string(uinfo.m_value->m_uniform_texture_channel)
                                + ");\n";
                        }
                    }
                    else
                    {
                        ++uniform_count;
                        uniform_decl += "    " + get_type_name_from_type(value_type) + " " + name + ";\n";
                    }
                }
                uniform_decl += "};";

                io_declear += texture_decl;
                if (uniform_count != 0)
                {
                    io_declear += "\n";
                    io_declear += uniform_decl;
                }

                io_declear += "\n";

                size_t INT_COUNT = 0;
                size_t FLOAT_COUNT = 0;
                size_t FLOAT2_COUNT = 0;
                size_t FLOAT3_4_COUNT = 0;
                std::vector<std::string> vertex_in_semantics(wrap->vertex_in.size());
                for (size_t i = 0; i < wrap->vertex_in.size(); ++i)
                {
                    switch (wrap->vertex_in[i])
                    {
                    case jegl_shader_value::type::INTEGER:
                        vertex_in_semantics[i] = "BLENDINDICES" + std::to_string(INT_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT:
                        vertex_in_semantics[i] = "BLENDWEIGHT" + std::to_string(FLOAT_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT2:
                        vertex_in_semantics[i] = "TEXCOORD" + std::to_string(FLOAT2_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT3:
                    case jegl_shader_value::type::FLOAT4:
                    {
                        auto count = FLOAT3_4_COUNT++;
                        if (count == 0)
                            vertex_in_semantics[i] = "POSITION0";
                        else if (count == 1)
                            vertex_in_semantics[i] = "NORMAL0";
                        else
                            vertex_in_semantics[i] = "COLOR" + std::to_string(count - 2);
                        break;
                    }
                    default:
                        abort();
                    }
                }

                io_declear += "struct vin_t\n{\n";

                for (auto& indecl : contex._in_value)
                {
                    io_declear +=
                        "    "
                        + get_type_name(indecl.first)
                        + " _in_"
                        + std::to_string(indecl.second.first)
                        + ": "
                        + vertex_in_semantics[indecl.second.first]
                        + ";\n";
                }
                io_declear += "};\n";

                io_declear += "struct v2f_t\n{\n";
                io_declear += "    float4 vout_position: SV_POSITION0;\n";
                size_t outid = 0;
                INT_COUNT = 0;
                FLOAT_COUNT = 0;
                FLOAT2_COUNT = 0;
                FLOAT3_4_COUNT = 0;
                for (auto& outvarname : outvalue)
                {
                    io_declear +=
                        "    "
                        + get_type_name(outvarname.first) + " _v2f_" + std::to_string(outid++) + ": ";

                    switch (outvarname.first->get_type())
                    {
                    case jegl_shader_value::type::INTEGER:
                        io_declear += "BLENDINDICES" + std::to_string(INT_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT:
                        io_declear += "BLENDWEIGHT" + std::to_string(FLOAT_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT2:
                        io_declear += "TEXCOORD" + std::to_string(FLOAT2_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT3:
                    case jegl_shader_value::type::FLOAT4:
                    {
                        auto count = FLOAT3_4_COUNT++;
                        if (count == 0)
                            io_declear += "POSITION0";
                        else
                            io_declear += "COLOR" + std::to_string(count - 1);
                        break;
                    }
                    default:
                        abort();
                    }
                    io_declear += ";\n";
                }
                io_declear += "};\n";

                body_result =
                    "\nv2f_t vertex_main(vin_t _vin)\n{\n"
                    + body_result
                    + "\n    // value out:\n"
                    + "    v2f_t vout;\n";

                outid = 0;
                for (auto& outvarname : outvalue)
                {
                    if (outid == 0)
                    {
                        body_result += "    float4 _je_position = ("
                            + outvarname.second
                            + " + float4(0.0, 0.0, "
                            + outvarname.second
                            + ".w, 0.0)) * float4(1.0, 1.0, 0.5, 1.0);\n";
                        body_result += "    vout.vout_position = _je_position;\n";
                        body_result += "    vout._v2f_" + std::to_string(outid) + " = _je_position;\n";
                    }
                    else
                    {
                        body_result += "    vout._v2f_" + std::to_string(outid) + " = " + outvarname.second + ";\n";
                    }

                    outid++;
                }

                body_result += "    return vout;\n}\n";

                return std::move(
                    "// Vertex shader source\n"
                    + _hlsl_pragma()
                    + unifrom_block
                    + built_in_srcs
                    + io_declear
                    + body_result);
            }
            std::string _generate_code_for_hlsl_fragment(shader_wrapper* wrap)
            {
                _shader_wrapper_contex contex;
                std::string          body_result;
                std::string          io_declear;

                const std::string    unifrom_block = _generate_uniform_block_for_hlsl(wrap);

                std::vector<jegl_shader_value*> invalue;
                for (auto* in_val : wrap->vertex_out->out_values)
                    invalue.push_back(in_val);

                std::vector<std::pair<jegl_shader_value*, std::string>> outvalue;
                for (auto* out_val : wrap->fragment_out->out_values)
                    outvalue.push_back(std::make_pair(out_val, _generate_code_for_hlsl_impl(&contex, body_result, out_val, true)));

                // Generate built function src here.
                std::string built_in_srcs;
                for (auto* sampler : wrap->decleared_samplers)
                {
                    built_in_srcs += "SamplerState sampler_"
                        + std::to_string(sampler->m_sampler_id)
                        + ": register(s"
                        + std::to_string(sampler->m_sampler_id)
                        + ");\n";
                }

                for (auto& builtin_func_name : contex._used_builtin_func)
                {
                    if (builtin_func_name == "JEBUILTIN_AlphaTest")
                    {
                        const std::string unifrom_block = R"(
float4 JEBUILTIN_AlphaTest(float4 color)
{
    if (color.a <= 0.0)
        clip(-1.0);
    return color;
}
)";
                        built_in_srcs += unifrom_block;
                    }
                    else if (builtin_func_name == "JEBUILTIN_TextureMs")
                    {
                        const std::string unifrom_block = R"(
// TODO;
)";
                        built_in_srcs += unifrom_block;
                    }
                    else if (builtin_func_name == "JEBUILTIN_TextureFastMs")
                    {
                        const std::string unifrom_block = R"(
// TODO;
)";
                        built_in_srcs += unifrom_block;
                    }
                    else if (builtin_func_name == "JEBUILTIN_Movement")
                    {
                        const std::string unifrom_block = R"(
float3 JEBUILTIN_Movement(float4x4 trans)
{
    return trans[3].xyz;
}
)";
                        built_in_srcs += unifrom_block;
                    }
                    else if (builtin_func_name == "JEBUILTIN_Negative")
                    {
                        const std::string unifrom_block = R"(
float JEBUILTIN_Negative(float v)
{
    return -v;
}
float2 JEBUILTIN_Negative(float2 v)
{
    return -v;
}
float3 JEBUILTIN_Negative(float3 v)
{
    return -v;
}
float4 JEBUILTIN_Negative(float4 v)
{
    return -v;
}
)";
                        built_in_srcs += unifrom_block;
                    }
                    else if (builtin_func_name == "JEBUILTIN_Uvframebuffer")
                    {
                        const std::string unifrom_block = R"(
float2 JEBUILTIN_Uvframebuffer(float2 v)
{
    return float2(v.x, 1.0 - v.y);
}
)";
                        built_in_srcs += unifrom_block;
                    }
                }

                // Generate shaders
                std::string texture_decl = "";
                std::string uniform_decl = "";
                size_t uniform_count = 0;

                uniform_decl += "cbuffer SHADER_UNIFORM: register(b0)\n{\n";
                for (auto& [name, uinfo] : wrap->uniform_variables)
                {
                    auto value_type = uinfo.m_value->get_type();
                    if (value_type == jegl_shader_value::type::TEXTURE2D
                        || value_type == jegl_shader_value::type::TEXTURE2D_MS
                        || value_type == jegl_shader_value::type::TEXTURE_CUBE)
                    {
                        if (uinfo.m_used_in_fragment)
                        {
                            texture_decl
                                += get_type_name_from_type(value_type)
                                + " "
                                + name
                                + ": register(t"
                                + std::to_string(uinfo.m_value->m_uniform_texture_channel)
                                + ");\n";
                        }
                    }
                    else
                    {
                        ++uniform_count;
                        uniform_decl += "    " + get_type_name_from_type(value_type) + " " + name + ";\n";
                    }
                }
                uniform_decl += "};";

                io_declear += texture_decl;
                if (uniform_count != 0)
                {
                    io_declear += "\n";
                    io_declear += uniform_decl;
                }

                io_declear += "\n";

                io_declear += "struct v2f_t\n{\n";
                io_declear += "    float4 vout_position: SV_POSITION0;\n";

                size_t outid = 0;
                size_t INT_COUNT = 0;
                size_t FLOAT_COUNT = 0;
                size_t FLOAT2_COUNT = 0;
                size_t FLOAT3_4_COUNT = 0;
                for (auto& inval : invalue)
                {
                    io_declear +=
                        "    "
                        + get_type_name(inval) + " _v2f_" + std::to_string(outid++) + ": ";

                    switch (inval->get_type())
                    {
                    case jegl_shader_value::type::INTEGER:
                        io_declear += "BLENDINDICES" + std::to_string(INT_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT:
                        io_declear += "BLENDWEIGHT" + std::to_string(FLOAT_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT2:
                        io_declear += "TEXCOORD" + std::to_string(FLOAT2_COUNT++);
                        break;
                    case jegl_shader_value::type::FLOAT3:
                    case jegl_shader_value::type::FLOAT4:
                    {
                        auto count = FLOAT3_4_COUNT++;
                        if (count == 0)
                            io_declear += "POSITION0";
                        else
                            io_declear += "COLOR" + std::to_string(count - 1);
                        break;
                    }
                    default:
                        abort();
                    }
                    io_declear += ";\n";
                }
                io_declear += "};\n";

                io_declear += "struct fout_t\n{\n";
                outid = 0;
                for (auto& outvarname : outvalue)
                {
                    auto oid = outid++;
                    io_declear +=
                        "    "
                        + get_type_name(outvarname.first)
                        + " _out_"
                        + std::to_string(oid)
                        + ": SV_TARGET"
                        + std::to_string(oid)
                        + ";\n";
                }
                io_declear += "};\n";

                body_result =
                    "\nfout_t fragment_main(v2f_t _v2f)\n{\n"
                    + body_result
                    + "\n    // value out:\n"
                    + "    fout_t fout;\n";

                outid = 0;
                for (auto& outvarname : outvalue)
                {
                    body_result += "    fout._out_" + std::to_string(outid++) + " = " + outvarname.second + ";\n";
                }

                body_result += "    return fout;\n}\n";

                return std::move(
                    "// Fragment shader source\n"
                    + _hlsl_pragma()
                    + unifrom_block
                    + built_in_srcs
                    + io_declear
                    + body_result);
            }
        }
    }
}