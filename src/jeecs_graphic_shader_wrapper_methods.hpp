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
    {".x", {jegl_shader_value::FLOAT2 | jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0]); }}},
    {".y", {jegl_shader_value::FLOAT2 | jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1]); }}},
    {".z", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2]); }}},
    {".w", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3]); }}},
            //
    {".xy", {jegl_shader_value::FLOAT2 | jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[1]); }}},
    {".yx", {jegl_shader_value::FLOAT2 | jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[0]); }}},
    {".xz", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[2]); }}},
    {".zx", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[0]); }}},
    {".yz", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[2]); }}},
    {".zy", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[1]); }}},
    {".xw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[3]); }}},
    {".wx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[0]); }}},
    {".yw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[3]); }}},
    {".wy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[1]); }}},
    {".zw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[3]); }}},
    {".wz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[2]); }}},
            //
    {".xyz", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[1], args[0]->m_float4[2]); }}},
    {".xzy", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[2], args[0]->m_float4[1]); }}},
    {".yxz", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[0], args[0]->m_float4[2]); }}},
    {".yzx", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[2], args[0]->m_float4[0]); }}},
    {".zxy", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[0], args[0]->m_float4[1]); }}},
    {".zyx", {jegl_shader_value::FLOAT3 | jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[1], args[0]->m_float4[0]); }}},
    {".wyz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[1], args[0]->m_float4[2]); }}},
    {".wzy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[2], args[0]->m_float4[1]); }}},
    {".ywz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[3], args[0]->m_float4[2]); }}},
    {".yzw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[2], args[0]->m_float4[3]); }}},
    {".zwy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[3], args[0]->m_float4[1]); }}},
    {".zyw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[1], args[0]->m_float4[3]); }}},
    {".xwz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[3], args[0]->m_float4[2]); }}},
    {".xzw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[2], args[0]->m_float4[3]); }}},
    {".wxz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[0], args[0]->m_float4[2]); }}},
    {".wzx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[2], args[0]->m_float4[0]); }}},
    {".zxw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[0], args[0]->m_float4[3]); }}},
    {".zwx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[3], args[0]->m_float4[0]); }}},
    {".xyw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[1], args[0]->m_float4[3]); }}},
    {".xwy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[3], args[0]->m_float4[1]); }}},
    {".yxw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[0], args[0]->m_float4[3]); }}},
    {".ywx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[3], args[0]->m_float4[0]); }}},
    {".wxy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[0], args[0]->m_float4[1]); }}},
    {".wyx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[1], args[0]->m_float4[0]); }}},
    {".xyzw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[1], args[0]->m_float4[2], args[0]->m_float4[3]); }}},
    {".xzyw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[2], args[0]->m_float4[1], args[0]->m_float4[3]); }}},
    {".yxzw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[0], args[0]->m_float4[2], args[0]->m_float4[3]); }}},
    {".yzxw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[2], args[0]->m_float4[0], args[0]->m_float4[3]); }}},
    {".zxyw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[0], args[0]->m_float4[1], args[0]->m_float4[3]); }}},
    {".zyxw", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[1], args[0]->m_float4[0], args[0]->m_float4[3]); }}},
    {".xywz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[1], args[0]->m_float4[3], args[0]->m_float4[2]); }} },
    {".xzwy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[2], args[0]->m_float4[3], args[0]->m_float4[1]); }} },
    {".yxwz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[0], args[0]->m_float4[3], args[0]->m_float4[2]); }} },
    {".yzwx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[2], args[0]->m_float4[3], args[0]->m_float4[0]); }} },
    {".zxwy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[0], args[0]->m_float4[3], args[0]->m_float4[1]); }} },
    {".zywx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[1], args[0]->m_float4[3], args[0]->m_float4[0]); }} },
    {".xwyz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[3], args[0]->m_float4[1], args[0]->m_float4[2]); }}},
    {".xwzy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[0], args[0]->m_float4[3], args[0]->m_float4[2], args[0]->m_float4[1]); }}},
    {".ywxz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[3], args[0]->m_float4[0], args[0]->m_float4[2]); }}},
    {".ywzx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[1], args[0]->m_float4[3], args[0]->m_float4[2], args[0]->m_float4[0]); }}},
    {".zwxy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[3], args[0]->m_float4[0], args[0]->m_float4[1]); }}},
    {".zwyx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[2], args[0]->m_float4[3], args[0]->m_float4[1], args[0]->m_float4[0]); }}},
    {".wxyz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[0], args[0]->m_float4[1], args[0]->m_float4[2]); }} },
    {".wxzy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[0], args[0]->m_float4[2], args[0]->m_float4[1]); }} },
    {".wyxz", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[1], args[0]->m_float4[0], args[0]->m_float4[2]); }} },
    {".wyzx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[1], args[0]->m_float4[2], args[0]->m_float4[0]); }} },
    {".wzxy", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[2], args[0]->m_float4[0], args[0]->m_float4[1]); }} },
    {".wzyx", {jegl_shader_value::FLOAT4,
            reduce_method{return new jegl_shader_value(args[0]->m_float4[3], args[0]->m_float4[2], args[0]->m_float4[1], args[0]->m_float4[0]); }} },
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
};