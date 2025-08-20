#pragma once

#include "jeecs_graphic_shader_wrapper.hpp"

namespace jeecs
{
    namespace shader_generator
    {
        class hlsl_generator : public shader_source_generator
        {
        protected:
            std::string attrib_binding(size_t loc, size_t set)
            {
                return "#ifdef GLSLANG_HLSL_TO_SPIRV\n[[vk::binding(" +
                       std::to_string(loc) + ", " +
                       std::to_string(set) +
                       ")]]\n#endif\n";
            }
            std::string attrib_location(size_t loc)
            {
                return "#ifdef GLSLANG_HLSL_TO_SPIRV\n[[vk::location(" +
                       std::to_string(loc) +
                       ")]]\n#endif\n";
            }
            virtual std::string get_typename(jegl_shader_value::type type) override
            {
                switch (type)
                {
                case jegl_shader_value::type::INTEGER:
                    return "int";
                case jegl_shader_value::type::INTEGER2:
                    return "int2";
                case jegl_shader_value::type::INTEGER3:
                    return "int3";
                case jegl_shader_value::type::INTEGER4:
                    return "int4";
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
            std::string generate_struct_body(shader_wrapper *wrapper, shader_struct_define *st)
            {
                std::string decl = "";
                for (auto &variable_inform : st->variables)
                {
                    if (variable_inform.type == jegl_shader_value::type::STRUCT)
                        decl += "    " + variable_inform.struct_type_may_nil->name + " " + variable_inform.name;
                    else
                        decl += "    " + get_typename(variable_inform.type) + " " + variable_inform.name + "";

                    if (variable_inform.array_size)
                        decl += "[" + std::to_string(variable_inform.array_size.value()) + "]";

                    decl += +";\n";
                }
                return decl;
            }
            virtual std::string generate_struct(shader_wrapper *wrapper, shader_struct_define *st) override
            {
                std::string decl = "struct " + st->name + "\n{\n";
                decl += generate_struct_body(wrapper, st);
                return decl + "};\n";
            }
            virtual std::string generate_uniform_block(shader_wrapper *wrapper, shader_struct_define *st) override
            {
                std::string decl = attrib_binding(st->binding_place, 1) + "cbuffer " + st->name + ": register(b" + std::to_string(st->binding_place + 1) + ")\n{\n";
                decl += generate_struct_body(wrapper, st);
                return decl + "};\n";
            }

            virtual std::string generate_code(
                _shader_wrapper_contex *context,
                jegl_shader_value *value,
                generate_target target,
                std::string *out_src) override
            {
                using namespace std;

                std::string varname;
                if (context->get_var_name_and_check_if_need_generate_expr(value, varname, target))
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
                            std::string apply = "    " + get_value_typename(value) + " " + varname + " = ";

                            std::vector<std::string> variables;
                            for (size_t i = 0; i < value->m_opnums_count; i++)
                                variables.push_back(generate_code(context, value->m_opnums[i], target, out_src));

                            std::string eval_expr;

                            if (value->m_opname == "+"s || value->m_opname == "-"s || value->m_opname == "/"s)
                            {
                                assert(variables.size() == 2 || value->m_opname == "-"s);
                                if (variables.size() == 1)
                                    eval_expr += value->m_opname + variables[0];
                                else
                                    eval_expr += variables[0] + " " + value->m_opname + " " + variables[1];
                            }
                            else if (value->m_opname == "*"s)
                            {
                                assert(variables.size() == 2);
                                if (value->m_opnums[0]->get_type() == jegl_shader_value::type::FLOAT2x2 || value->m_opnums[0]->get_type() == jegl_shader_value::type::FLOAT3x3 || value->m_opnums[0]->get_type() == jegl_shader_value::type::FLOAT4x4 || value->m_opnums[1]->get_type() == jegl_shader_value::type::FLOAT2x2 || value->m_opnums[1]->get_type() == jegl_shader_value::type::FLOAT3x3 || value->m_opnums[1]->get_type() == jegl_shader_value::type::FLOAT4x4)
                                    eval_expr += "mul(" + variables[0] + "," + variables[1] + ")";
                                else
                                {
                                    eval_expr += variables[0] + " " + value->m_opname + " " + variables[1];
                                }
                            }
                            else if (value->m_opname[0] == '.')
                            {
                                assert(variables.size() == 1 || variables.size() == 2);
                                eval_expr += variables[0] + value->m_opname;

                                if (variables.size() == 2)
                                    eval_expr += "[" + variables[1] + "]";
                            }
                            else
                            {
                                std::string funcname = value->m_opname;

                                if (funcname == "texture")
                                {
                                    assert(variables.size() == 2);

                                    if (value->m_opnums[0]->is_shader_in_value())
                                        eval_expr +=
                                            variables[0] + ".Sample(" + variables[0] + "_sampler" + ", " + variables[1] + ")";
                                    else
                                        eval_expr +=
                                            variables[0] + ".Sample(sampler_" + std::to_string(value->m_opnums[0]->m_binded_sampler_id) + ", " + variables[1] + ")";
                                }
                                else if (funcname == "fract")
                                    funcname = "frac";
                                else
                                {
                                    bool is_casting_op = false;
                                    if (!funcname.empty())
                                    {
                                        if (funcname[0] == '#')
                                        {
                                            funcname = funcname.substr(1);
                                            context->_used_builtin_func.insert(funcname);
                                        }
                                        else if (funcname[0] == '%')
                                        {
                                            is_casting_op = true;

                                            funcname = funcname.substr(1);
                                            context->_used_builtin_func.insert(funcname);
                                        }
                                    }

                                    if (is_casting_op)
                                        eval_expr += "(" + funcname + ")(";
                                    else
                                        eval_expr += funcname + "(";

                                    for (size_t i = 0; i < variables.size(); i++)
                                    {
                                        eval_expr += variables[i];

                                        if (value->m_opnums[i]->is_texture())
                                        {
                                            if (value->m_opnums[i]->is_shader_in_value())
                                                eval_expr += ", " + variables[i] + "_sampler";
                                            else
                                                eval_expr += ", sampler_" + std::to_string(value->m_opnums[i]->m_binded_sampler_id);
                                        }

                                        if (i + 1 != variables.size())
                                            eval_expr += ", ";
                                    }
                                    eval_expr += ")";
                                }
                            }

                            if (context->update_fast_eval_var_name(value, eval_expr))
                                varname = eval_expr;
                            else
                            {
                                apply += eval_expr + ";";
                                *out_src += apply + "\n";
                            }
                        }
                    }
                    else
                    {
                        std::string apply;

                        switch (value->get_type())
                        {
                        case jegl_shader_value::type::INTEGER:
                            apply += std::to_string(value->m_integer);
                            break;
                        case jegl_shader_value::type::FLOAT:
                            apply += std::to_string(value->m_float);
                            break;
                        case jegl_shader_value::type::FLOAT2:
                            apply += "float2(" 
                                + std::to_string(value->m_float2[0])  + "," 
                                + std::to_string(value->m_float2[1]) + ")";
                            break;
                        case jegl_shader_value::type::FLOAT3:
                            apply += "float3(" 
                                + std::to_string(value->m_float3[0]) + "," 
                                + std::to_string(value->m_float3[1]) + "," 
                                + std::to_string(value->m_float3[2]) + ")";
                            break;
                        case jegl_shader_value::type::FLOAT4:
                            apply += "float4(" 
                                + std::to_string(value->m_float4[0]) + "," 
                                + std::to_string(value->m_float4[1]) + "," 
                                + std::to_string(value->m_float4[2]) + "," 
                                + std::to_string(value->m_float4[3]) + ")";
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
                    switch (target)
                    {
                    case shader_source_generator::generate_target::VERTEX:
                        varname = "_vin." + varname;
                        break;
                    case shader_source_generator::generate_target::FRAGMENT:
                        varname = "_v2f." + varname;
                        break;
                    case shader_source_generator::generate_target::USER:
                        break;
                    default:
                        break;
                    }
                }

                return varname;
            }

            virtual std::string use_custom_function(const shader_wrapper::custom_shader_src &src) override
            {
                return src.m_hlsl_impl;
            }
            virtual std::string use_user_function(shader_wrapper *wrap, _shader_wrapper_contex *context, const shader_wrapper::user_function_information &user) override
            {
                assert(user.m_result != nullptr && user.m_result->out_values.size() == 1);
                std::string function_declear = get_value_typename(user.m_result->out_values[0]) + " " + user.m_name + "(";

                for (size_t inidx = 0; inidx < user.m_args.size(); ++inidx)
                {
                    if (inidx != 0)
                        function_declear += ", ";
                    function_declear += get_typename(user.m_args[inidx]) + " _in_" + std::to_string(inidx);

                    if (0 != (user.m_args[inidx] & (jegl_shader_value::type::TEXTURE2D | jegl_shader_value::type::TEXTURE2D_MS | jegl_shader_value::type::TEXTURE_CUBE)))
                    {
                        function_declear += ", SamplerState _in_" + std::to_string(inidx) + "_sampler";
                    }
                }

                std::string function_body;
                std::string result_var_name = generate_code(context, user.m_result->out_values[0], generate_target::USER, &function_body);

                function_declear += ")\n{\n" + function_body + "\n    return " + result_var_name + ";\n}";
                return function_declear;
            }

        public:
            void generate_uniform_declear(shader_wrapper *wrap, bool in_vertex, std::string *out_uniform, std::string *out_utexture)
            {
                std::string uniform_decl = "\n";
                size_t uniform_count = 0;

                uniform_decl += attrib_binding(0, 0) + "cbuffer SHADER_UNIFORM: register(b0)\n{\n";
                for (auto &[name, uinfo] : wrap->uniform_variables)
                {
                    auto value_type = uinfo.m_value->get_type();
                    if (value_type == jegl_shader_value::type::TEXTURE2D || value_type == jegl_shader_value::type::TEXTURE2D_MS || value_type == jegl_shader_value::type::TEXTURE_CUBE)
                    {
                        if ((uinfo.m_used_in_vertex && in_vertex) || (uinfo.m_used_in_fragment && !in_vertex))
                        {
                            *out_utexture += 
                                attrib_binding(uinfo.m_value->m_uniform_texture_channel, 2) 
                                + get_typename(value_type) 
                                + " " 
                                + name 
                                + ": register(t" + std::to_string(uinfo.m_value->m_uniform_texture_channel) + ");\n";
                        }
                    }
                    else
                    {
                        ++uniform_count;
                        uniform_decl += "    " + get_typename(value_type) + " " + name + ";\n";
                    }
                }
                uniform_decl += "};";

                if (uniform_count != 0)
                {
                    *out_uniform = uniform_decl;
                }
            }

            virtual std::string generate_vertex(shader_wrapper *wrap) override
            {
                _shader_wrapper_contex contex;
                std::string body_result;
                std::string io_declear;

                const std::string unifrom_block = generate_uniform_block_and_struct(wrap);

                std::vector<std::pair<jegl_shader_value *, std::string>> outvalue;
                for (auto *out_val : wrap->vertex_out->out_values)
                    outvalue.push_back(std::make_pair(out_val, generate_code(&contex, out_val, generate_target::VERTEX, &body_result)));

                // Generate built function src here.
                std::string sampler_decl;
                for (auto *sampler : wrap->decleared_samplers)
                {
                    sampler_decl +=
                        attrib_binding(sampler->m_sampler_id, 3) 
                        + "SamplerState sampler_" 
                        + std::to_string(sampler->m_sampler_id) 
                        + ": register(s" + std::to_string(sampler->m_sampler_id) + ");\n";
                }

                std::string built_in_srcs = generate_builtin_codes(wrap, &contex);

                // Generate shaders
                std::string texture_decl = "";
                std::string uniform_decl = "";

                generate_uniform_declear(wrap, true, &uniform_decl, &texture_decl);

                io_declear += texture_decl;
                io_declear += uniform_decl;
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
                    case jegl_shader_value::type::INTEGER2:
                    case jegl_shader_value::type::INTEGER3:
                    case jegl_shader_value::type::INTEGER4:
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

                std::map<size_t, std::string> in_value_wraper;
                for (auto &indecl : contex._in_value)
                {
                    in_value_wraper[indecl.second.first] = 
                        attrib_location(indecl.second.first) 
                        + "    " 
                        + get_value_typename(indecl.first) 
                        + " _in_" + std::to_string(indecl.second.first) 
                        + ": " 
                        + vertex_in_semantics[indecl.second.first] 
                        + ";\n";
                }

                for (auto &codegendata : in_value_wraper)
                    io_declear += codegendata.second;

                io_declear += "};\n";

                io_declear += "struct v2f_t\n{\n";
                io_declear += "    float4 vout_position: SV_POSITION;\n";
                size_t outid = 0;
                INT_COUNT = 0;
                FLOAT_COUNT = 0;
                FLOAT2_COUNT = 0;
                FLOAT3_4_COUNT = 0;
                for (auto &outvarname : outvalue)
                {
                    io_declear +=
                        "    " + get_value_typename(outvarname.first) + " _v2f_" + std::to_string(outid++) + ": ";

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
                    "\nv2f_t vertex_main(vin_t _vin)\n{\n" + body_result + "\n    // value out:\n" + "    v2f_t vout;\n";

                outid = 0;
                for (auto &outvarname : outvalue)
                {
                    if (outid == 0)
                    {
                        body_result += 
                            "    float4 _je_position = (" 
                            + outvarname.second 
                            + " + float4(0.0, 0.0, " + outvarname.second + ".w, 0.0)) * float4(1.0, 1.0, 0.5, 1.0);\n";
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

                return 
                    "// Vertex shader source\n" + unifrom_block + sampler_decl + io_declear + built_in_srcs + body_result;
            }
            virtual std::string generate_fragment(shader_wrapper *wrap) override
            {
                _shader_wrapper_contex contex;
                std::string body_result;
                std::string io_declear;

                const std::string unifrom_block = generate_uniform_block_and_struct(wrap);

                std::vector<jegl_shader_value *> invalue;
                for (auto *in_val : wrap->vertex_out->out_values)
                    invalue.push_back(in_val);

                std::vector<std::pair<jegl_shader_value *, std::string>> outvalue;
                for (auto *out_val : wrap->fragment_out->out_values)
                    outvalue.push_back(std::make_pair(out_val, generate_code(&contex, out_val, generate_target::FRAGMENT, &body_result)));

                // Generate built function src here.
                std::string sampler_decl;
                for (auto *sampler : wrap->decleared_samplers)
                {
                    sampler_decl +=
                        attrib_binding(sampler->m_sampler_id, 3) 
                        + "SamplerState sampler_" 
                        + std::to_string(sampler->m_sampler_id) 
                        + ": register(s" + std::to_string(sampler->m_sampler_id) + ");\n";
                }

                std::string built_in_srcs = generate_builtin_codes(wrap, &contex);

                // Generate shaders
                std::string texture_decl = "";
                std::string uniform_decl = "";

                generate_uniform_declear(wrap, false, &uniform_decl, &texture_decl);

                io_declear += texture_decl;
                io_declear += uniform_decl;

                io_declear += "\n";

                io_declear += "struct v2f_t\n{\n";
                io_declear += "    float4 vout_position: SV_POSITION;\n";

                size_t outid = 0;
                size_t INT_COUNT = 0;
                size_t FLOAT_COUNT = 0;
                size_t FLOAT2_COUNT = 0;
                size_t FLOAT3_4_COUNT = 0;
                for (auto &inval : invalue)
                {
                    io_declear +=
                        "    " + get_value_typename(inval) + " _v2f_" + std::to_string(outid++) + ": ";

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
                for (auto &outvarname : outvalue)
                {
                    auto oid = outid++;
                    io_declear +=
                        "    " 
                        + get_value_typename(outvarname.first) 
                        + " _out_" + std::to_string(oid) 
                        + ": SV_TARGET" + std::to_string(oid) + ";\n";
                }
                io_declear += "};\n";

                body_result =
                    "\nfout_t fragment_main(v2f_t _v2f)\n{\n" + body_result + "\n    // value out:\n" + "    fout_t fout;\n";

                outid = 0;
                for (auto &outvarname : outvalue)
                {
                    body_result += "    fout._out_" + std::to_string(outid++) + " = " + outvarname.second + ";\n";
                }

                body_result += "    return fout;\n}\n";

                return 
                    "// Fragment shader source\n" + unifrom_block + sampler_decl + io_declear + built_in_srcs + body_result;
            }
        };
    }
}