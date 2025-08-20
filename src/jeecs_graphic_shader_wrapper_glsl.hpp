#pragma once

#include "jeecs_graphic_shader_wrapper.hpp"

namespace jeecs
{
    namespace shader_generator
    {
        class glsl_generator : public shader_source_generator
        {
        protected:
            virtual std::string get_typename(jegl_shader_value::type type) override
            {
                switch (type)
                {
                case jegl_shader_value::type::INTEGER:
                    return "int";
                case jegl_shader_value::type::INTEGER2:
                    return "ivec2";
                case jegl_shader_value::type::INTEGER3:
                    return "ivec3";
                case jegl_shader_value::type::INTEGER4:
                    return "ivec4";
                case jegl_shader_value::type::FLOAT:
                    return "float";
                case jegl_shader_value::type::FLOAT2:
                    return "vec2";
                case jegl_shader_value::type::FLOAT3:
                    return "vec3";
                case jegl_shader_value::type::FLOAT4:
                    return "vec4";
                case jegl_shader_value::type::FLOAT2x2:
                    return "mat2";
                case jegl_shader_value::type::FLOAT3x3:
                    return "mat3";
                case jegl_shader_value::type::FLOAT4x4:
                    return "mat4";
                case jegl_shader_value::type::TEXTURE2D:
                    return "sampler2D";
                case jegl_shader_value::type::TEXTURE2D_MS:
                    return "sampler2DMS";
                case jegl_shader_value::type::TEXTURE_CUBE:
                    return "samplerCube";
                default:
                    jeecs::debug::logerr("Unknown value type when generating glsl, please check!");
                    return "unknown";
                    break;
                }
            }
            std::string generate_struct_body(shader_wrapper* wrapper, shader_struct_define* st)
            {
                std::string decl = "";
                for (auto& variable_inform : st->variables)
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
            virtual std::string generate_struct(shader_wrapper* wrapper, shader_struct_define* st) override
            {
                std::string decl = "struct " + st->name + "\n{\n";
                decl += generate_struct_body(wrapper, st);
                return decl + "};\n";
            }
            virtual std::string generate_uniform_block(shader_wrapper* wrapper, shader_struct_define* st) override
            {
                std::string decl = "layout (std140) uniform " + st->name + "\n{\n";
                decl += generate_struct_body(wrapper, st);
                return decl + "};\n";
            }

            virtual std::string generate_code(
                _shader_wrapper_contex* context,
                jegl_shader_value* value,
                generate_target target,
                std::string* out_src) override
            {
                using namespace std;

                std::string varname;
                if (context->get_var_name_and_check_if_need_generate_expr(value, varname, target))
                {
                    if (value->is_calc_value())
                    {
                        if (value->is_shader_in_value())
                        {
                            ;
                        }
                        else if (value->is_uniform_variable())
                        {
                            ;
                        }
                        else
                        {
                            std::string apply = "    " + get_value_typename(value) + " " + varname + " = ";

                            std::vector<std::string> variables;
                            for (size_t i = 0; i < value->m_opnums_count; i++)
                                variables.push_back(generate_code(context, value->m_opnums[i], target, out_src));

                            std::string eval_expr;

                            if (value->m_opname == "+"s || value->m_opname == "-"s || value->m_opname == "*"s || value->m_opname == "/"s)
                            {
                                assert(variables.size() == 2 || value->m_opname == "-"s);
                                if (variables.size() == 1)
                                    eval_expr += value->m_opname + " "s + variables[0];
                                else
                                    eval_expr += variables[0] + " " + value->m_opname + " " + variables[1];
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

                                if (!funcname.empty())
                                {
                                    if (funcname[0] == '%')
                                        funcname = funcname.substr(1);

                                    if (funcname == "float2")
                                        funcname = "vec2";
                                    else if (funcname == "float3")
                                        funcname = "vec3";
                                    else if (funcname == "float4")
                                        funcname = "vec4";
                                    else if (funcname == "float2x2")
                                        funcname = "mat2";
                                    else if (funcname == "float3x3")
                                        funcname = "mat3";
                                    else if (funcname == "float4x4")
                                        funcname = "mat4";
                                    else if (funcname == "lerp")
                                        funcname = "mix";
                                    else if (funcname == "ddx")
                                        funcname = "dFdx";
                                    else if (funcname == "ddy")
                                        funcname = "dFdy";

                                    if (funcname[0] == '#')
                                    {
                                        funcname = funcname.substr(1);
                                        context->_used_builtin_func.insert(funcname);
                                    }
                                }

                                eval_expr += funcname + "("s;
                                for (size_t i = 0; i < variables.size(); i++)
                                {
                                    eval_expr += variables[i];
                                    if (i + 1 != variables.size())
                                        eval_expr += ", ";
                                }
                                eval_expr += ")";
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
                        std::string apply /* = "    const " + _shader_wrapper_contex::get_type_name(value) + " " + varname + " = "*/;

                        switch (value->get_type())
                        {
                        case jegl_shader_value::type::INTEGER:
                            apply += std::to_string(value->m_integer);
                            break;
                        case jegl_shader_value::type::FLOAT:
                            apply += std::to_string(value->m_float);
                            break;
                        case jegl_shader_value::type::FLOAT2:
                            apply += "vec2(" + 
                                std::to_string(value->m_float2[0]) + "," + 
                                std::to_string(value->m_float2[1]) + ")";
                            break;
                        case jegl_shader_value::type::FLOAT3:
                            apply += "vec3(" + 
                                std::to_string(value->m_float3[0]) + "," + 
                                std::to_string(value->m_float3[1]) + "," + 
                                std::to_string(value->m_float3[2]) + ")";
                            break;
                        case jegl_shader_value::type::FLOAT4:
                            apply += "vec4(" + 
                                std::to_string(value->m_float4[0]) + "," + 
                                std::to_string(value->m_float4[1]) + "," + 
                                std::to_string(value->m_float4[2]) + "," + 
                                std::to_string(value->m_float4[3]) + ")";
                            break;
                        case jegl_shader_value::type::FLOAT2x2:
                        {
                            std::string result = "mat2(";
                            for (size_t iy = 0; iy < 2; iy++)
                                for (size_t ix = 0; ix < 2; ix++)
                                    result += std::to_string(value->m_float2x2[ix][iy]) + ((ix == 1 && iy == 1) ? ")" : ",");

                            apply += result;
                            break;
                        }
                        case jegl_shader_value::type::FLOAT3x3:
                        {
                            std::string result = "mat3(";
                            for (size_t iy = 0; iy < 3; iy++)
                                for (size_t ix = 0; ix < 3; ix++)
                                    result += std::to_string(value->m_float3x3[ix][iy]) + ((ix == 2 && iy == 2) ? ")" : ",");

                            apply += result;
                            break;
                        }
                        case jegl_shader_value::type::FLOAT4x4:
                        {
                            std::string result = "mat4(";
                            for (size_t iy = 0; iy < 4; iy++)
                                for (size_t ix = 0; ix < 4; ix++)
                                    result += std::to_string(value->m_float4x4[ix][iy]) + ((ix == 3 && iy == 3) ? ")" : ",");

                            apply += result;
                            break;
                        }
                        default:
                            jeecs::debug::logerr("Unknown constant value when generating glsl, please check!");
                            apply += "invalid";
                            break;
                        }

                        /*out += apply + ";\n";*/
                        return apply;
                    }
                }

                return varname;
            }

            virtual std::string use_custom_function(const shader_wrapper::custom_shader_src& src) override
            {
                return src.m_glsl_impl;
            }
            virtual std::string use_user_function(shader_wrapper* wrap, _shader_wrapper_contex* context, const shader_wrapper::user_function_information& user) override
            {
                assert(user.m_result != nullptr && user.m_result->out_values.size() == 1);
                std::string function_declear = get_value_typename(user.m_result->out_values[0]) + " " + user.m_name + "(";

                for (size_t inidx = 0; inidx < user.m_args.size(); ++inidx)
                {
                    if (inidx != 0)
                        function_declear += ", ";
                    function_declear += get_typename(user.m_args[inidx]) + " _in_" + std::to_string(inidx);
                }

                std::string function_body;
                std::string result_var_name = generate_code(context, user.m_result->out_values[0], generate_target::USER, &function_body);

                function_declear += ")\n{\n" + function_body + "\n    return " + result_var_name + ";\n}";
                return function_declear;
            }

        public:
            virtual std::string generate_vertex(shader_wrapper* wrap) override
            {
                _shader_wrapper_contex contex;
                std::string body_result;
                std::string io_declear;

                const std::string unifrom_block = generate_uniform_block_and_struct(wrap);

                std::vector<std::pair<jegl_shader_value*, std::string>> outvalue;
                for (auto* out_val : wrap->vertex_out->out_values)
                    outvalue.push_back(std::make_pair(out_val, generate_code(&contex, out_val, generate_target::VERTEX, &body_result)));

                // Generate built function src here.
                std::string built_in_srcs = generate_builtin_codes(wrap, &contex);

                for (auto& [name, uinfo] : wrap->uniform_variables)
                {
                    if (uinfo.m_used_in_vertex)
                        io_declear += "uniform " + get_value_typename(uinfo.m_value) + " " + name + ";\n";
                }
                io_declear += "\n";
                for (auto& indecl : contex._in_value)
                    io_declear += "layout(location = " + std::to_string(indecl.second.first) + ") in " + get_value_typename(indecl.first) + " " + indecl.second.second + ";\n";
                io_declear += "\n";

                size_t outid = 0;
                for (auto& outvarname : outvalue)
                    io_declear += "out " + get_value_typename(outvarname.first) + " _v2f_" + std::to_string(outid++) + ";\n";

                body_result = "\nvoid main()\n{\n" + body_result + "\n    // value out:\n";

                outid = 0;
                for (auto& outvarname : outvalue)
                {
                    if (outid == 0)
                        body_result += "    gl_Position = " + outvarname.second + ";\n";
                    body_result += "    _v2f_" + std::to_string(outid++) + " = " + outvarname.second + ";\n";
                }

                body_result += "\n}\n";

                return 
                    "// Vertex shader source\n" + unifrom_block + io_declear + built_in_srcs + body_result;
            }
            virtual std::string generate_fragment(shader_wrapper* wrap) override
            {
                _shader_wrapper_contex contex;
                std::string body_result;
                std::string io_declear;

                const std::string unifrom_block = generate_uniform_block_and_struct(wrap);

                std::vector<std::pair<jegl_shader_value*, std::string>> outvalue;
                for (auto* out_val : wrap->fragment_out->out_values)
                    outvalue.push_back(std::make_pair(out_val, generate_code(&contex, out_val, generate_target::FRAGMENT, &body_result)));

                // Generate built function src here.
                std::string built_in_srcs = generate_builtin_codes(wrap, &contex);

                for (auto& [name, uinfo] : wrap->uniform_variables)
                {
                    if (uinfo.m_used_in_fragment)
                        io_declear += "uniform " + get_value_typename(uinfo.m_value) + " " + name + ";\n";
                }
                io_declear += "\n";
                for (auto& indecl : contex._in_value)
                    io_declear += "in " + get_value_typename(indecl.first) + " " + indecl.second.second + ";\n";
                io_declear += "\n";

                size_t outid = 0;
                for (auto& outvarname : outvalue)
                {
                    size_t oid = outid++;
                    io_declear += "layout(location = " + std::to_string(oid) + ") out " 
                        + get_value_typename(outvarname.first) 
                        + " _out_" + std::to_string(oid) + ";\n";
                }

                body_result = "\nvoid main()\n{\n" + body_result + "\n    // value out:\n";

                outid = 0;
                for (auto& outvarname : outvalue)
                {
                    body_result += "    _out_" + std::to_string(outid++) + " = " + outvarname.second + ";\n";
                }

                body_result += "\n}\n";

                return 
                    "// Fragment shader source\n" + unifrom_block + io_declear + built_in_srcs + body_result;
            }
        };
    }
}