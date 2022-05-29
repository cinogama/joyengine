class _glsl_wrapper_contex
{
public:

    std::unordered_map<jegl_shader_value*, std::string> _calced_value;
    std::unordered_map<jegl_shader_value*, std::pair<size_t, std::string>> _in_value;
    std::unordered_map<jegl_shader_value*, std::string> _uniform_value;
    int _variable_count = 0;

    static std::string get_type_name(jegl_shader_value* val)
    {
        switch (val->get_type())
        {
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
        default:
            jeecs::debug::log_error("Unknown value type when generating glsl, please check!");
            return "unknown";
            break;
        }
    }
    bool get_var_name(jegl_shader_value* val, std::string& var_name, bool is_in_fragment)
    {
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
};

std::string _generate_code_for_glsl_impl(
    _glsl_wrapper_contex* contex,
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
                ;
            }
            else if (value->is_uniform_variable())
            {
                ;
            }
            else
            {
                std::string apply = "    " + _glsl_wrapper_contex::get_type_name(value) + " " + varname + " = ";

                std::vector<std::string> variables;
                for (size_t i = 0; i < value->m_opnums_count; i++)
                    variables.push_back(_generate_code_for_glsl_impl(contex, out, value->m_opnums[i], is_fragment));

                if (value->m_opname == "+"s
                    || value->m_opname == "-"s
                    || value->m_opname == "*"s
                    || value->m_opname == "/"s)
                {
                    assert(variables.size() == 2);
                    apply += variables[0] + " " + value->m_opname + " " + variables[1];
                }
                else if (value->m_opname[0] == '.')
                {
                    assert(variables.size() == 1);
                    apply += variables[0] + value->m_opname;
                }
                else
                {
                    std::string funcname = value->m_opname;
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

                    apply += funcname + "("s;
                    for (size_t i = 0; i < variables.size(); i++)
                    {
                        apply += variables[i];
                        if (i + 1 != variables.size())
                            apply += ", ";
                    }
                    apply += ")";
                }
                apply += ";";

                out += apply + "\n";
            }
        }
        else
        {
            std::string apply = "    const " + _glsl_wrapper_contex::get_type_name(value) + " " + varname + " = ";

            switch (value->get_type())
            {
            case jegl_shader_value::type::FLOAT:
                apply += std::to_string(value->m_float);
                break;
            case jegl_shader_value::type::FLOAT2:
                apply += "vec2(" + std::to_string(value->m_float2[0]) + "," + std::to_string(value->m_float2[1]) + ")";
                break;
            case jegl_shader_value::type::FLOAT3:
                apply += "vec3(" + std::to_string(value->m_float3[0]) + "," + std::to_string(value->m_float3[1]) + "," + std::to_string(value->m_float3[2]) + ")";
                break;
            case jegl_shader_value::type::FLOAT4:
                apply += "vec4(" + std::to_string(value->m_float4[0]) + "," + std::to_string(value->m_float4[1]) + "," + std::to_string(value->m_float4[2]) + "," + std::to_string(value->m_float4[3]) + ")";
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
                        result += std::to_string(value->m_float2x2[ix][iy]) + ((ix == 2 && iy == 2) ? ")" : ",");

                apply += result;
                break;
            }
            case jegl_shader_value::type::FLOAT4x4:
            {
                std::string result = "mat4(";
                for (size_t iy = 0; iy < 4; iy++)
                    for (size_t ix = 0; ix < 4; ix++)
                        result += std::to_string(value->m_float2x2[ix][iy]) + ((ix == 3 && iy == 3) ? ")" : ",");

                apply += result;
                break;
            }
            default:
                jeecs::debug::log_error("Unknown constant value when generating glsl, please check!");
                apply += "invalid";
                break;
            }

            out += apply + ";\n";
        }
    }

    return varname;
}
std::string _generate_code_for_glsl_vertex(shader_value_outs* wrap)
{
    _glsl_wrapper_contex contex;
    std::string          body_result;
    std::string          io_declear;

    const std::string    unifrom_block = R"(
layout (std140) uniform _JE_UNIFORM_DATA_BLOCK
{
    vec4 JOYENGINE_TIMES;
};

)";

    std::vector<std::pair<jegl_shader_value*, std::string>> outvalue;
    for (auto* out_val : wrap->out_values)
        outvalue.push_back(std::make_pair(out_val, _generate_code_for_glsl_impl(&contex, body_result, out_val, false)));

    for (auto& uniformdecl : contex._uniform_value)
        io_declear += "uniform " + _glsl_wrapper_contex::get_type_name(uniformdecl.first) + " " + uniformdecl.second + ";\n";
    io_declear += "\n";
    for (auto& indecl : contex._in_value)
        io_declear += "layout(location = " + std::to_string(indecl.second.first) + ") in "
        + _glsl_wrapper_contex::get_type_name(indecl.first) + " " + indecl.second.second + ";\n";
    io_declear += "\n";

    size_t outid = 0;
    for (auto& outvarname : outvalue)
        io_declear += "out "
        + _glsl_wrapper_contex::get_type_name(outvarname.first) + " _v2f_" + std::to_string(outid++) + ";\n";

    body_result = "\nvoid main()\n{\n" + body_result + "\n    // value out:\n";

    outid = 0;
    for (auto& outvarname : outvalue)
    {
        if (outid == 0)
            body_result += "    gl_Position = " + outvarname.second + ";\n";
        body_result += "    _v2f_" + std::to_string(outid++) + " = " + outvarname.second + ";\n";
    }

    body_result += "\n}";

    return std::move("// Vertex shader source\n#version 330\n" + unifrom_block + io_declear + body_result);
}

std::string _generate_code_for_glsl_fragment(shader_value_outs* wrap)
{
    _glsl_wrapper_contex contex;
    std::string          body_result;
    std::string          io_declear;

    const std::string    unifrom_block = R"(
layout (std140) uniform _JE_UNIFORM_DATA_BLOCK
{
    vec4 JOYENGINE_TIMES;
};

)";
    std::vector<std::pair<jegl_shader_value*, std::string>> outvalue;
    for (auto* out_val : wrap->out_values)
        outvalue.push_back(std::make_pair(out_val, _generate_code_for_glsl_impl(&contex, body_result, out_val, true)));

    for (auto& uniformdecl : contex._uniform_value)
        io_declear += "uniform " + _glsl_wrapper_contex::get_type_name(uniformdecl.first) + " " + uniformdecl.second + ";\n";
    io_declear += "\n";
    for (auto& indecl : contex._in_value)
        io_declear += "in "
        + _glsl_wrapper_contex::get_type_name(indecl.first) + " " + indecl.second.second + ";\n";
    io_declear += "\n";

    size_t outid = 0;
    for (auto& outvarname : outvalue)
    {
        size_t oid = outid++;
        io_declear += "layout(location = " + std::to_string(oid) + ") out "
            + _glsl_wrapper_contex::get_type_name(outvarname.first) + " _out_" + std::to_string(oid) + ";\n";
    }

    body_result = "\nvoid main()\n{\n" + body_result + "\n    // value out:\n";

    outid = 0;
    for (auto& outvarname : outvalue)
    {
        body_result += "    _out_" + std::to_string(outid++) + " = " + outvarname.second + ";\n";
    }

    body_result += "\n}";

    return std::move("// Fragment shader source\n#version 330\n" + unifrom_block + io_declear + body_result);
}