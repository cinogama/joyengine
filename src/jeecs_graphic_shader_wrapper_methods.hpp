const std::vector<std::pair<std::string, std::vector<operation_t>>> _operation_table =
{
    {"+", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
            reduce_method{return new jegl_shader_value(args[0]->m_float + args[1]->m_float); }}},
    {"-", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
            reduce_method{return new jegl_shader_value(args[0]->m_float - args[1]->m_float); }}},
    {"*", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
            reduce_method{return new jegl_shader_value(args[0]->m_float * args[1]->m_float); }}},
    {"/", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
            reduce_method{return new jegl_shader_value(args[0]->m_float / args[1]->m_float); }}},
            //
    {"x", {jegl_shader_value::FLOAT2,
            reduce_method{return new jegl_shader_value(args[0]->m_float2[0]); }}},
    {"y", {jegl_shader_value::FLOAT2,
            reduce_method{return new jegl_shader_value(args[0]->m_float2[1]); }}},
            //
    {"x", {jegl_shader_value::FLOAT3,
            reduce_method{return new jegl_shader_value(args[0]->m_float3[0]); }}},
    {"y", {jegl_shader_value::FLOAT3,
            reduce_method{return new jegl_shader_value(args[0]->m_float3[1]); }}},
    {"z", {jegl_shader_value::FLOAT3,
            reduce_method{return new jegl_shader_value(args[0]->m_float3[2]); }}},
            //
    {"x", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0]); }}},
    {"y", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1]); }}},
    {"z", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2]); }}},
    {"w", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3]); }}},
            //
};