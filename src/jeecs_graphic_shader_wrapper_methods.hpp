#pragma once

#include "jeecs_graphic_shader_wrapper.hpp"

using calc_func_t = std::function<jegl_shader_value* (size_t, jegl_shader_value**)>;
using operation_t = std::variant<jegl_shader_value::type_base_t, calc_func_t>;

#define reduce_method [](size_t argc, jegl_shader_value** args)->jegl_shader_value*

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
{"-", {jegl_shader_value::FLOAT,
reduce_method{return new jegl_shader_value(-args[0]->m_float); }}},

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
{"-", {jegl_shader_value::FLOAT2,
reduce_method{return new jegl_shader_value(-args[0]->m_float2[0], -args[0]->m_float2[1]); }}},

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
{"-", {jegl_shader_value::FLOAT3,
reduce_method{return new jegl_shader_value(
-args[0]->m_float3[0],
-args[0]->m_float3[1],
-args[0]->m_float3[2]); }}},

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
{ "-", {jegl_shader_value::FLOAT4,
reduce_method{return new jegl_shader_value(
-args[0]->m_float4[0],
-args[0]->m_float4[1],
-args[0]->m_float4[2],
-args[0]->m_float4[3]); }} },

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
{ "float3x3", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3,
reduce_method
{
float matdata[9];
matdata[0] = args[0]->m_float3[0]; matdata[1] = args[0]->m_float3[1]; matdata[2] = args[0]->m_float3[2];
matdata[3] = args[1]->m_float3[0]; matdata[4] = args[1]->m_float3[1]; matdata[5] = args[1]->m_float3[2];
matdata[6] = args[2]->m_float3[0]; matdata[7] = args[2]->m_float3[1]; matdata[8] = args[2]->m_float3[2];

return new jegl_shader_value(matdata, jegl_shader_value::FLOAT3x3);
}} },
{ "float3x3", {jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
jegl_shader_value::FLOAT, jegl_shader_value::FLOAT, jegl_shader_value::FLOAT,
reduce_method
{
float matdata[9];
for (size_t i = 0; i < 9; i++)
matdata[i] = args[i]->m_float;

return new jegl_shader_value(matdata, jegl_shader_value::FLOAT3x3);
}} },
//
{"*", {jegl_shader_value::FLOAT4x4, jegl_shader_value::FLOAT4,
reduce_method{
float result[4] = {};
jeecs::math::mat4xvec4(result, args[0]->m_float4x4, args[1]->m_float4);
return new jegl_shader_value(result[0], result[1], result[2], result[3]);
}}},

{"*", {jegl_shader_value::FLOAT4, jegl_shader_value::FLOAT4x4,
reduce_method{
return nullptr; // TODO;
}} },

{"*", {jegl_shader_value::FLOAT4x4, jegl_shader_value::FLOAT4x4,
reduce_method{
float result[4][4] = {};
jeecs::math::mat4xmat4(result, args[0]->m_float4x4, args[1]->m_float4x4);
return new jegl_shader_value((float*)&result, jegl_shader_value::FLOAT4x4);
}}},
//
{ "*", {jegl_shader_value::FLOAT3x3, jegl_shader_value::FLOAT3,
reduce_method{
float result[3] = {};
jeecs::math::mat3xvec3(result, args[0]->m_float3x3, args[1]->m_float3);
return new jegl_shader_value(result[0], result[1], result[2]);
}} },

{ "*", {jegl_shader_value::FLOAT3, jegl_shader_value::FLOAT3x3,
reduce_method{
return nullptr; // TODO;
}} },

{ "*", {jegl_shader_value::FLOAT3x3, jegl_shader_value::FLOAT3x3,
reduce_method{
float result[3][3] = {};
jeecs::math::mat3xmat3(result, args[0]->m_float3x3, args[1]->m_float3x3);
return new jegl_shader_value((float*)&result, jegl_shader_value::FLOAT3x3);
}} },
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

{ "exp", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "exp", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "exp", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "exp", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "fract", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "fract", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "fract", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "fract", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "floor", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "floor", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "floor", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "floor", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "ceil", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "ceil", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "ceil", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "ceil", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "sign", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "sign", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "sign", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "sign", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "log", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "log", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "log", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "log", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "log2", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "log2", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "log2", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "log2", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

{ "log10", {jegl_shader_value::FLOAT4,
reduce_method{return nullptr; }} },
{ "log10", {jegl_shader_value::FLOAT3,
reduce_method{return nullptr; }} },
{ "log10", {jegl_shader_value::FLOAT2,
reduce_method{return nullptr; }} },
{ "log10", {jegl_shader_value::FLOAT,
reduce_method{return nullptr; }} },

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

#undef reduce_method
