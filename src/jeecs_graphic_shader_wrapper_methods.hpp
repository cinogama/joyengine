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

{"+", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0] + args[1]->m_float2[0], args[0]->m_float2[1] + args[1]->m_float2[1]); }}},
{"-", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0] - args[1]->m_float2[0], args[0]->m_float2[1] - args[1]->m_float2[1]); }}},
{"*", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0] * args[1]->m_float2[0], args[0]->m_float2[1] * args[1]->m_float2[1]); }}},
{"*", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0] * args[1]->m_float, args[0]->m_float2[1] * args[1]->m_float); }}},
{"/", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0] / args[1]->m_float, args[0]->m_float2[1] / args[1]->m_float); }}},
{"/", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0] / args[1]->m_float2[0], args[0]->m_float2[1] / args[1]->m_float2[1]); }}},

{"+", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return new jegl_shader_value(
args[0]->m_float3[0] + args[1]->m_float3[0],
args[0]->m_float3[1] + args[1]->m_float3[1],
args[0]->m_float3[2] + args[1]->m_float3[2]); }}},
{"-", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return new jegl_shader_value(
args[0]->m_float3[0] - args[1]->m_float3[0],
args[0]->m_float3[1] - args[1]->m_float3[1],
args[0]->m_float3[2] - args[1]->m_float3[2]); }}},
{"*", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return new jegl_shader_value(
args[0]->m_float3[0] * args[1]->m_float3[0],
args[0]->m_float3[1] * args[1]->m_float3[1],
args[0]->m_float3[2] * args[1]->m_float3[2]); }}},
{"*", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(
    args[0]->m_float3[0] * args[1]->m_float, 
    args[0]->m_float3[1] * args[1]->m_float,
    args[0]->m_float3[2] * args[1]->m_float); }}},
{"/", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(
    args[0]->m_float3[0] / args[1]->m_float,
    args[0]->m_float3[1] / args[1]->m_float,
    args[0]->m_float3[2] / args[1]->m_float); }}},
{"/", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return new jegl_shader_value(
    args[0]->m_float3[0] / args[1]->m_float3[0],
    args[0]->m_float3[1] / args[1]->m_float3[1],
    args[0]->m_float3[2] / args[1]->m_float3[2]); }}},

{"+", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return new jegl_shader_value(
args[0]->m_float4[0] + args[1]->m_float4[0],
args[0]->m_float4[1] + args[1]->m_float4[1],
args[0]->m_float4[2] + args[1]->m_float4[2],
args[0]->m_float4[3] + args[1]->m_float4[3]); }}},
{"-", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return new jegl_shader_value(
args[0]->m_float4[0] - args[1]->m_float4[0],
args[0]->m_float4[1] - args[1]->m_float4[1],
args[0]->m_float4[2] - args[1]->m_float4[2],
args[0]->m_float4[3] - args[1]->m_float4[3]); }}},
{"*", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return new jegl_shader_value(
args[0]->m_float4[0] * args[1]->m_float4[0],
args[0]->m_float4[1] * args[1]->m_float4[1],
args[0]->m_float4[2] * args[1]->m_float4[2],
args[0]->m_float4[3] * args[1]->m_float4[3]); }}},
{"*", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(
    args[0]->m_float4[0] * args[1]->m_float,
    args[0]->m_float4[1] * args[1]->m_float,
    args[0]->m_float4[2] * args[1]->m_float,
    args[0]->m_float4[3] * args[1]->m_float); }}},
{"/", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(
    args[0]->m_float4[0] / args[1]->m_float,
    args[0]->m_float4[1] / args[1]->m_float,
    args[0]->m_float4[2] / args[1]->m_float,
    args[0]->m_float4[3] / args[1]->m_float); }}},
{"/", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return new jegl_shader_value(
    args[0]->m_float4[0] / args[1]->m_float4[0],
    args[0]->m_float4[1] / args[1]->m_float4[1],
    args[0]->m_float4[2] / args[1]->m_float4[2],
    args[0]->m_float4[3] / args[1]->m_float4[3]); }}},
//
{"float", {jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float); }} },
//
{"float2", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float, args[1]->m_float); }} },
{"float2", {jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0], args[0]->m_float2[1]); }} },
//
{"float3", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float, args[1]->m_float, args[2]->m_float); }} },
{"float3", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0], args[0]->m_float2[1], args[2]->m_float); }} },
{"float3", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(args[0]->m_float, args[0]->m_float2[0], args[0]->m_float2[1]); }} },
{"float3", {jegl_shader_value::FLOAT3,
reduce_method{return new jegl_shader_value(args[0]->m_float3[0], args[0]->m_float3[1], args[0]->m_float3[2]); }} },
//
{"float4", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float, args[1]->m_float, args[2]->m_float, args[3]->m_float); }} },
{"float4", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0], args[0]->m_float2[1], args[1]->m_float, args[2]->m_float); }} },
{"float4", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float, args[1]->m_float2[0], args[1]->m_float2[1], args[2]->m_float); }} },
{"float4", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(args[0]->m_float, args[1]->m_float, args[2]->m_float2[0], args[2]->m_float2[1]); }} },
{"float4", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(args[0]->m_float2[0], args[0]->m_float2[1], args[1]->m_float2[0], args[1]->m_float2[1]); }} },
{"float4", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT3,
reduce_method{return new jegl_shader_value(args[0]->m_float, args[1]->m_float3[0], args[1]->m_float3[1], args[1]->m_float3[2]); }} },
{"float4", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float3[0], args[0]->m_float3[1], args[0]->m_float3[2], args[1]->m_float); }} },
{"float4", {jegl_shader_value::FLOAT4,
reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[1], args[0]->m_float4[2], args[0]->m_float4[3]); }} },
{"float4x4", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method
{
float matdata[16];
matdata[0] = args[0]->m_float4[0]; matdata[1] = args[0]->m_float4[1]; matdata[2] = args[0]->m_float4[2]; matdata[3] = args[0]->m_float4[3];
matdata[4] = args[1]->m_float4[0]; matdata[5] = args[1]->m_float4[1]; matdata[6] = args[1]->m_float4[2]; matdata[7] = args[1]->m_float4[3];
matdata[8] = args[2]->m_float4[0]; matdata[9] = args[2]->m_float4[1]; matdata[10] = args[2]->m_float4[2]; matdata[11] = args[2]->m_float4[3];
matdata[12] = args[3]->m_float4[0]; matdata[13] = args[3]->m_float4[1]; matdata[14] = args[3]->m_float4[2]; matdata[15] = args[3]->m_float4[3];

return new jegl_shader_value(matdata, jegl_shader_value::FLOAT4x4);
}} },
{"float4x4", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method
{
float matdata[16];
for (size_t i = 0; i < 16; i++)
matdata[i] = args[i]->m_float;

return new jegl_shader_value(matdata, jegl_shader_value::FLOAT4x4);
}} },
//
#define A(M,N) (args[0]->m_float4x4[M][N])
#define B(M,N) (args[1]->m_float4x4[M][N])
{"*", {jegl_shader_value::FLOAT4x4, jegl_shader_value::FLOAT4,
reduce_method{
float result[4] = {};
for (size_t iy = 0; iy < 4; iy++)
for (size_t ix = 0; ix < 4; ix++)
result[iy] += args[0]->m_float4x4[ix][iy] * args[1]->m_float4[ix];
return new jegl_shader_value(result[0], result[1], result[2], result[3]);
}}},
#undef A
#undef B
{"*", {jegl_shader_value::FLOAT4x4, jegl_shader_value::FLOAT4x4,
reduce_method{
float result[4][4] = {};
jeecs::math::mat4xmat4(result, args[0]->m_float4x4, args[1]->m_float4x4);
return new jegl_shader_value((float*)&result, jegl_shader_value::FLOAT4x4);
}}},
//
{"texture", {jegl_shader_value::TEXTURE2D, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "step", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(args[0]->m_float < args[1]->m_float ? 1.0f : 0.0f); }} },
{ "step", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "step", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "step", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },

{ "max", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },
{ "max", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "max", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "max", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },

{ "min", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },
{ "min", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "min", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "min", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },

{ "length", {jegl_shader_value::FLOAT2, 
reduce_method{return nullptr; }} },
{ "length", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "length", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },

{ "distance", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "distance", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "distance", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },

{ "lerp", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(jeecs::math::lerp(args[0]->m_float, args[1]->m_float, args[2]->m_float)); }} },
{ "lerp", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(
    jeecs::math::lerp(args[0]->m_float2[0], args[1]->m_float2[0], args[2]->m_float),
    jeecs::math::lerp(args[0]->m_float2[1], args[1]->m_float2[1], args[2]->m_float)
); }} },
{ "lerp", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(
    jeecs::math::lerp(args[0]->m_float3[0], args[1]->m_float3[0], args[2]->m_float),
    jeecs::math::lerp(args[0]->m_float3[1], args[1]->m_float3[1], args[2]->m_float),
    jeecs::math::lerp(args[0]->m_float3[2], args[1]->m_float3[2], args[2]->m_float)
); }} },
{ "lerp", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(
    jeecs::math::lerp(args[0]->m_float4[0], args[1]->m_float4[0], args[2]->m_float),
    jeecs::math::lerp(args[0]->m_float4[1], args[1]->m_float4[1], args[2]->m_float),
    jeecs::math::lerp(args[0]->m_float4[2], args[1]->m_float3[2], args[2]->m_float),
    jeecs::math::lerp(args[0]->m_float4[3], args[1]->m_float4[3], args[2]->m_float)
); }} },
{ "JEBUILTIN_AlphaTest", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "JEBUILTIN_TextureMs", {jegl_shader_value::TEXTURE2D_MS, jegl_shader_value::FLOAT2, jegl_shader_value::INTEGER,
reduce_method{return nullptr; }} },
{ "JEBUILTIN_TextureFastMs", {jegl_shader_value::TEXTURE2D_MS, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },

{ "JEBUILTIN_Movement", {jegl_shader_value::FLOAT4x4,
reduce_method{return new jegl_shader_value(args[0]->m_float4x4[3][0], args[0]->m_float4x4[3][1], args[0]->m_float4x4[3][2]); }} },

{ "sin", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "cos", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "tan", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },

{ "sin", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "cos", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "tan", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },

{ "sin", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "cos", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "tan", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },

{ "sin", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },
{ "cos", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },
{ "tan", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "asin", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "acos", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "atan", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },

{ "asin", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "acos", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "atan", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },

{ "asin", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "acos", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "atan", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },

{ "asin", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },
{ "acos", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },
{ "atan", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "atan2", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "normalize", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "normalize", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "normalize", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },

{ "abs", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "abs", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "abs", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "abs", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "JEBUILTIN_Negative", {jegl_shader_value::FLOAT4,
reduce_method{return new jegl_shader_value(-args[0]->m_float4[0], -args[0]->m_float4[1],-args[0]->m_float4[2],-args[0]->m_float4[3]); }} },
{ "JEBUILTIN_Negative", {jegl_shader_value::FLOAT3,
reduce_method{return new jegl_shader_value(-args[0]->m_float3[0], -args[0]->m_float3[1],-args[0]->m_float3[2]); }} },
{ "JEBUILTIN_Negative", {jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(-args[0]->m_float2[0], -args[0]->m_float2[1]); }} },
{ "JEBUILTIN_Negative", {jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(-args[0]->m_float); }} },

{ "pow", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "pow", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "pow", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "pow", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "clamp", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "clamp", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "clamp", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "clamp", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "clamp", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },
{ "clamp", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },
{ "clamp", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "dot", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "dot", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "dot", {jegl_shader_value::FLOAT2, jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },

{ "cross", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
};