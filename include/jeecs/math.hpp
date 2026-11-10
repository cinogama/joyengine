#pragma once

// ============================================================================
// jeecs/math.hpp —— 数学库（C++）：jeecs::math 向量/四元数/矩阵/颜色/变换
// =============================================================================

#include "jeecs/prefix.hpp"

#ifdef __cplusplus
namespace jeecs
{
    namespace math
    {
        constexpr static float PI = 3.14159265f;
        constexpr static float RAD2DEG = 180.f / PI;
        constexpr static float DEG2RAD = PI / 180.f;
        constexpr static float EPSILON = FLT_EPSILON;

        template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
        inline bool almost_equal(T a, T b, T eps = (T)1e-6) noexcept
        {
            if (a == b)
                return true;

            return abs(a - b) <= eps;
        }

        template <typename T>
        static T clamp(T src, T min, T max)
        {
            return std::min(std::max(src, min), max);
        }
        template <typename T>
        inline T lerp(T va, T vb, float deg)
        {
            return va * (1.0f - deg) + vb * deg;
        }

        template <typename T>
        inline T random(T from, T to)
        {
            // thread_local：粒子系统（并行阶段）等可能并发调用，避免数据竞争
            thread_local std::mt19937 gen{ std::random_device{}() };

            if constexpr (std::is_floating_point_v<T>)
            {
                std::uniform_real_distribution<> dis(from, to);
                return (T)dis(gen);
            }
            else
            {
                std::uniform_int_distribution<> dis(from, to);
                return (T)dis(gen);
            }
        }

        inline void mat4xmat4(float* out_result, const float* left, const float* right)
        {
#define R(x, y) (out_result[(x) + (y) * 4])
#define A(x, y) (left[(x) + (y) * 4])
#define B(x, y) (right[(x) + (y) * 4])
            memset(out_result, 0, 16 * sizeof(float));
            for (size_t iy = 0; iy < 4; iy++)
                for (size_t ix = 0; ix < 4; ix++)
                {
                    R(ix, iy) += A(ix, 0) * B(0, iy) + A(ix, 1) * B(1, iy) + A(ix, 2) * B(2, iy) + A(ix, 3) * B(3, iy);
                }

#undef R
#undef A
#undef B
        }
        inline void mat4xmat4(float (*out_result)[4], const float (*left)[4], const float (*right)[4])
        {
            return mat4xmat4((float*)out_result, (const float*)left, (const float*)right);
        }

        inline void mat3xmat3(float* out_result, const float* left, const float* right)
        {
#define R(x, y) (out_result[(x) + (y) * 3])
#define A(x, y) (left[(x) + (y) * 3])
#define B(x, y) (right[(x) + (y) * 3])
            memset(out_result, 0, 9 * sizeof(float));
            for (size_t iy = 0; iy < 3; iy++)
                for (size_t ix = 0; ix < 3; ix++)
                {
                    R(ix, iy) += A(ix, 0) * B(0, iy) + A(ix, 1) * B(1, iy) + A(ix, 2) * B(2, iy);
                }

#undef R
#undef A
#undef B
        }
        inline void mat3xmat3(float (*out_result)[3], const float (*left)[3], const float (*right)[3])
        {
            return mat3xmat3((float*)out_result, (const float*)left, (const float*)right);
        }

        inline void mat3xvec3(float* out_result, const float* left_mat, const float* right_vex)
        {
#define R(x) (out_result[x])
#define A(x, y) (left_mat[(x) + (y) * 3])
#define B(x) (right_vex[x])
            R(0) = A(0, 0) * B(0) + A(0, 1) * B(1) + A(0, 2) * B(2);
            R(1) = A(1, 0) * B(0) + A(1, 1) * B(1) + A(1, 2) * B(2);
            R(2) = A(2, 0) * B(0) + A(2, 1) * B(1) + A(2, 2) * B(2);
#undef R
#undef A
#undef B
        }
        inline void mat3xvec3(float* out_result, const float (*left)[3], const float* right)
        {
            return mat3xvec3(out_result, (const float*)left, right);
        }

        inline void mat4xvec4(float* out_result, const float* left_mat, const float* right_vex)
        {
#define R(x) (out_result[x])
#define A(x, y) (left_mat[(x) + (y) * 4])
#define B(x) (right_vex[x])
            R(0) = A(0, 0) * B(0) + A(0, 1) * B(1) + A(0, 2) * B(2) + A(0, 3) * B(3);
            R(1) = A(1, 0) * B(0) + A(1, 1) * B(1) + A(1, 2) * B(2) + A(1, 3) * B(3);
            R(2) = A(2, 0) * B(0) + A(2, 1) * B(1) + A(2, 2) * B(2) + A(2, 3) * B(3);
            R(3) = A(3, 0) * B(0) + A(3, 1) * B(1) + A(3, 2) * B(2) + A(3, 3) * B(3);
#undef R
#undef A
#undef B
        }
        inline void mat4xvec4(float* out_result, const float (*left)[4], const float* right)
        {
            return mat4xvec4(out_result, (const float*)left, right);
        }

        inline void mat4xvec3(float* out_result, const float* left, const float* right)
        {
            float dat[4] = { right[0], right[1], right[2], 1.f };
            float result[4];
            mat4xvec4(result, left, dat);
            if (result[3] != 0.f)
            {
                out_result[0] = result[0] / result[3];
                out_result[1] = result[1] / result[3];
                out_result[2] = result[2] / result[3];
            }
            else
            {
                out_result[0] = result[0];
                out_result[1] = result[1];
                out_result[2] = result[2];
            }
        }
        inline void mat4xvec3(float* out_result, const float (*left)[4], const float* right)
        {
            mat4xvec3(out_result, (const float*)left, right);
        }

        struct _basevec2
        {
            float x, y;
            constexpr _basevec2(float _x, float _y) noexcept : x(_x), y(_y) {}
        };
        struct _basevec3
        {
            float x, y, z;
            constexpr _basevec3(float _x, float _y, float _z) noexcept : x(_x), y(_y), z(_z) {}
        };
        struct _basevec4
        {
            float x, y, z, w;
            constexpr _basevec4(float _x, float _y, float _z, float _w) noexcept : x(_x), y(_y), z(_z), w(_w) {}
        };

        struct vec2 : public _basevec2
        {
            constexpr vec2(float _x = 0.f, float _y = 0.f) noexcept
                : _basevec2(_x, _y)
            {
            }
            constexpr vec2(const vec2& _v2) noexcept
                : _basevec2(_v2.x, _v2.y)
            {
            }
            constexpr vec2(vec2&& _v2) noexcept
                : _basevec2(_v2.x, _v2.y)
            {
            }

            constexpr vec2(const _basevec3& _v3) noexcept
                : _basevec2(_v3.x, _v3.y)
            {
            }
            constexpr vec2(_basevec3&& _v3) noexcept
                : _basevec2(_v3.x, _v3.y)
            {
            }
            constexpr vec2(const _basevec4& _v4) noexcept
                : _basevec2(_v4.x, _v4.y)
            {
            }
            constexpr vec2(_basevec4&& _v4) noexcept
                : _basevec2(_v4.x, _v4.y)
            {
            }

            // + - * / with another vec2
            inline constexpr vec2 operator+(const vec2& _v2) const noexcept
            {
                return vec2(x + _v2.x, y + _v2.y);
            }
            inline constexpr vec2 operator-(const vec2& _v2) const noexcept
            {
                return vec2(x - _v2.x, y - _v2.y);
            }
            inline constexpr vec2 operator-() const noexcept
            {
                return vec2(-x, -y);
            }
            inline constexpr vec2 operator+() const noexcept
            {
                return vec2(x, y);
            }
            inline constexpr vec2 operator*(const vec2& _v2) const noexcept
            {
                return vec2(x * _v2.x, y * _v2.y);
            }
            inline constexpr vec2 operator/(const vec2& _v2) const noexcept
            {
                return vec2(x / _v2.x, y / _v2.y);
            }
            // * / with float
            inline constexpr vec2 operator*(float _f) const noexcept
            {
                return vec2(x * _f, y * _f);
            }
            inline constexpr vec2 operator/(float _f) const noexcept
            {
                return vec2(x / _f, y / _f);
            }

            inline constexpr vec2& operator=(const vec2& _v2) noexcept = default;
            inline constexpr vec2& operator+=(const vec2& _v2) noexcept
            {
                x += _v2.x;
                y += _v2.y;
                return *this;
            }
            inline constexpr vec2& operator-=(const vec2& _v2) noexcept
            {
                x -= _v2.x;
                y -= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator*=(const vec2& _v2) noexcept
            {
                x *= _v2.x;
                y *= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator*=(float _f) noexcept
            {
                x *= _f;
                y *= _f;
                return *this;
            }
            inline constexpr vec2& operator/=(const vec2& _v2) noexcept
            {
                x /= _v2.x;
                y /= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator/=(float _f) noexcept
            {
                x /= _f;
                y /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator==(const vec2& _v2) const noexcept
            {
                return x == _v2.x && y == _v2.y;
            }
            inline constexpr bool operator!=(const vec2& _v2) const noexcept
            {
                return x != _v2.x || y != _v2.y;
            }

            // length/ unit/ dot/ cross
            inline float length() const noexcept
            {
                return sqrt(x * x + y * y);
            }
            inline vec2 unit() const noexcept
            {
                float vlength = length();
                if (vlength != 0.f)
                    return vec2(x / vlength, y / vlength);
                return vec2();
            }
            inline constexpr float dot(const vec2& _v2) const noexcept
            {
                return x * _v2.x + y * _v2.y;
            }

            // 绕原点旋转 _angle（度）后的新向量。
            inline vec2 rotate(float _angle) const noexcept
            {
                const float rad = _angle * DEG2RAD;
                const float c = std::cos(rad), s = std::sin(rad);
                return vec2(c * x - s * y, s * x + c * y);
            }

            inline float max() const noexcept
            {
                return std::max(x, y);
            }

            inline float min() const noexcept
            {
                return std::min(x, y);
            }

            static const char* JEScriptTypeName()
            {
                return "vec2";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "public using vec2 = (real, real);";
            }
            void JEParseFromScriptType(woort_value v)
            {
                x = woort_struct_get_float(v, 0);
                y = woort_struct_get_float(v, 1);
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_set_struct(v, 2);
                woort_struct_set_float(v, 0, x);
                woort_struct_set_float(v, 1, y);
            }
        };
        inline static constexpr vec2 operator*(float _f, const vec2& _v2) noexcept
        {
            return vec2(_v2.x * _f, _v2.y * _f);
        }

        struct ivec2
        {
            int x;
            int y;
            constexpr ivec2(int _x = 0, int _y = 0) noexcept
                : x(_x), y(_y)
            {
            }
            constexpr ivec2(const vec2& _v2) noexcept
                : x((int)_v2.x), y((int)_v2.y)
            {
            }

            constexpr ivec2(vec2&& _v2) noexcept
                : x((int)_v2.x), y((int)_v2.y)
            {
            }

            // + - * / with another vec2
            inline constexpr ivec2 operator+(const ivec2& _v2) const noexcept
            {
                return ivec2(x + _v2.x, y + _v2.y);
            }
            inline constexpr ivec2 operator-(const ivec2& _v2) const noexcept
            {
                return ivec2(x - _v2.x, y - _v2.y);
            }
            inline constexpr ivec2 operator-() const noexcept
            {
                return ivec2(-x, -y);
            }
            inline constexpr ivec2 operator+() const noexcept
            {
                return ivec2(x, y);
            }
            inline constexpr ivec2 operator*(const ivec2& _v2) const noexcept
            {
                return ivec2(x * _v2.x, y * _v2.y);
            }
            inline constexpr ivec2 operator/(const ivec2& _v2) const noexcept
            {
                return ivec2(x / _v2.x, y / _v2.y);
            }
            // * / with float
            inline constexpr ivec2 operator*(int _f) const noexcept
            {
                return ivec2(x * _f, y * _f);
            }
            inline constexpr ivec2 operator/(int _f) const noexcept
            {
                return ivec2(x / _f, y / _f);
            }

            inline constexpr ivec2& operator=(const ivec2& _v2) noexcept = default;
            inline constexpr ivec2& operator+=(const ivec2& _v2) noexcept
            {
                x += _v2.x;
                y += _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator-=(const ivec2& _v2) noexcept
            {
                x -= _v2.x;
                y -= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator*=(const ivec2& _v2) noexcept
            {
                x *= _v2.x;
                y *= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator*=(float _f) noexcept
            {
                x = (int)((float)x * _f);
                y = (int)((float)y * _f);
                return *this;
            }
            inline constexpr ivec2& operator/=(const ivec2& _v2) noexcept
            {
                x /= _v2.x;
                y /= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator/=(int _f) noexcept
            {
                x /= _f;
                y /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator==(const ivec2& _v2) const noexcept
            {
                return x == _v2.x && y == _v2.y;
            }
            inline constexpr bool operator!=(const ivec2& _v2) const noexcept
            {
                return x != _v2.x || y != _v2.y;
            }

            static const char* JEScriptTypeName()
            {
                return "ivec2";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "public using ivec2 = (int, int);";
            }

            inline int max() const noexcept
            {
                return std::max(x, y);
            }

            inline int min() const noexcept
            {
                return std::min(x, y);
            }

            void JEParseFromScriptType(woort_value v)
            {
                x = static_cast<int>(woort_struct_get_int(v, 0));
                y = static_cast<int>(woort_struct_get_int(v, 1));
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_set_struct(v, 2);
                woort_struct_set_int(v, 0, static_cast<woort_Int>(x));
                woort_struct_set_int(v, 1, static_cast<woort_Int>(y));
            }
        };

        struct vec3 : public _basevec3
        {
            constexpr vec3(float _x = 0.f, float _y = 0.f, float _z = 0.f) noexcept
                : _basevec3(_x, _y, _z)
            {
            }
            constexpr vec3(const vec3& _v3) noexcept
                : _basevec3(_v3.x, _v3.y, _v3.z)
            {
            }
            constexpr vec3(vec3&& _v3) noexcept
                : _basevec3(_v3.x, _v3.y, _v3.z)
            {
            }

            constexpr vec3(const _basevec2& _v2) noexcept
                : _basevec3(_v2.x, _v2.y, 0.f)
            {
            }
            constexpr vec3(_basevec2&& _v2) noexcept
                : _basevec3(_v2.x, _v2.y, 0.f)
            {
            }
            constexpr vec3(const _basevec4& _v4) noexcept
                : _basevec3(_v4.x, _v4.y, _v4.z)
            {
            }
            constexpr vec3(_basevec4&& _v4) noexcept
                : _basevec3(_v4.x, _v4.y, _v4.z)
            {
            }

            // + - * / with another vec3
            inline constexpr vec3 operator+(const vec3& _v3) const noexcept
            {
                return vec3(x + _v3.x, y + _v3.y, z + _v3.z);
            }
            inline constexpr vec3 operator-(const vec3& _v3) const noexcept
            {
                return vec3(x - _v3.x, y - _v3.y, z - _v3.z);
            }
            inline constexpr vec3 operator-() const noexcept
            {
                return vec3(-x, -y, -z);
            }
            inline constexpr vec3 operator+() const noexcept
            {
                return vec3(x, y, z);
            }
            inline constexpr vec3 operator*(const vec3& _v3) const noexcept
            {
                return vec3(x * _v3.x, y * _v3.y, z * _v3.z);
            }
            inline constexpr vec3 operator/(const vec3& _v3) const noexcept
            {
                return vec3(x / _v3.x, y / _v3.y, z / _v3.z);
            }
            // * / with float
            inline constexpr vec3 operator*(float _f) const noexcept
            {
                return vec3(x * _f, y * _f, z * _f);
            }
            inline constexpr vec3 operator/(float _f) const noexcept
            {
                return vec3(x / _f, y / _f, z / _f);
            }

            inline constexpr vec3& operator=(const vec3& _v3) noexcept = default;
            inline constexpr vec3& operator+=(const vec3& _v3) noexcept
            {
                x += _v3.x;
                y += _v3.y;
                z += _v3.z;
                return *this;
            }
            inline constexpr vec3& operator-=(const vec3& _v3) noexcept
            {
                x -= _v3.x;
                y -= _v3.y;
                z -= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator*=(const vec3& _v3) noexcept
            {
                x *= _v3.x;
                y *= _v3.y;
                z *= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator*=(float _f) noexcept
            {
                x *= _f;
                y *= _f;
                z *= _f;
                return *this;
            }
            inline constexpr vec3& operator/=(const vec3& _v3) noexcept
            {
                x /= _v3.x;
                y /= _v3.y;
                z /= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator/=(float _f) noexcept
            {
                x /= _f;
                y /= _f;
                z /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator==(const vec3& _v3) const noexcept
            {
                return x == _v3.x && y == _v3.y && z == _v3.z;
            }
            inline constexpr bool operator!=(const vec3& _v3) const noexcept
            {
                return x != _v3.x || y != _v3.y || z != _v3.z;
            }

            // length/ unit/ dot/ cross
            inline float length() const noexcept
            {
                return std::sqrt(x * x + y * y + z * z);
            }

            inline float min() const noexcept
            {
                return std::min(std::min(x, y), z);
            }

            inline float max() const noexcept
            {
                return std::max(std::max(x, y), z);
            }

            inline vec3 unit() const noexcept
            {
                float vlength = length();
                if (vlength != 0.f)
                    return vec3(x / vlength, y / vlength, z / vlength);
                return vec3();
            }
            inline constexpr float dot(const vec3& _v3) const noexcept
            {
                return x * _v3.x + y * _v3.y + z * _v3.z;
            }
            inline constexpr vec3 cross(const vec3& _v3) const noexcept
            {
                return vec3(
                    y * _v3.z - z * _v3.y,
                    z * _v3.x - x * _v3.z,
                    x * _v3.y - y * _v3.x);
            }

            static const char* JEScriptTypeName()
            {
                return "vec3";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "public using vec3 = (real, real, real);";
            }
            void JEParseFromScriptType(woort_value v)
            {
                x = woort_struct_get_float(v, 0);
                y = woort_struct_get_float(v, 1);
                z = woort_struct_get_float(v, 2);
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_set_struct(v, 3);
                woort_struct_set_float(v, 0, x);
                woort_struct_set_float(v, 1, y);
                woort_struct_set_float(v, 2, z);
            }
        };
        inline static constexpr vec3 operator*(float _f, const vec3& _v3) noexcept
        {
            return vec3(_v3.x * _f, _v3.y * _f, _v3.z * _f);
        }

        struct vec4 : public _basevec4
        {
            constexpr vec4(float _x = 0.f, float _y = 0.f, float _z = 0.f, float _w = 0.f) noexcept
                : _basevec4(_x, _y, _z, _w)
            {
            }
            constexpr vec4(const vec4& _v4) noexcept
                : _basevec4(_v4.x, _v4.y, _v4.z, _v4.w)
            {
            }
            constexpr vec4(vec4&& _v4) noexcept
                : _basevec4(_v4.x, _v4.y, _v4.z, _v4.w)
            {
            }

            constexpr vec4(const _basevec2& _v2) noexcept
                : _basevec4(_v2.x, _v2.y, 0.f, 0.f)
            {
            }
            constexpr vec4(_basevec2&& _v2) noexcept
                : _basevec4(_v2.x, _v2.y, 0.f, 0.f)
            {
            }
            constexpr vec4(const _basevec3& _v3) noexcept
                : _basevec4(_v3.x, _v3.y, _v3.z, 0.f)
            {
            }
            constexpr vec4(_basevec3&& _v3) noexcept
                : _basevec4(_v3.x, _v3.y, _v3.z, 0.f)
            {
            }

            // + - * / with another vec4
            inline constexpr vec4 operator+(const vec4& _v4) const noexcept
            {
                return vec4(x + _v4.x, y + _v4.y, z + _v4.z, w + _v4.w);
            }
            inline constexpr vec4 operator-(const vec4& _v4) const noexcept
            {
                return vec4(x - _v4.x, y - _v4.y, z - _v4.z, w - _v4.w);
            }
            inline constexpr vec4 operator-() const noexcept
            {
                return vec4(-x, -y, -z, -w);
            }
            inline constexpr vec4 operator+() const noexcept
            {
                return vec4(x, y, z, w);
            }
            inline constexpr vec4 operator*(const vec4& _v4) const noexcept
            {
                return vec4(x * _v4.x, y * _v4.y, z * _v4.z, w * _v4.w);
            }
            inline constexpr vec4 operator/(const vec4& _v4) const noexcept
            {
                return vec4(x / _v4.x, y / _v4.y, z / _v4.z, w / _v4.w);
            }
            // * / with float
            inline constexpr vec4 operator*(float _f) const noexcept
            {
                return vec4(x * _f, y * _f, z * _f, w * _f);
            }
            inline constexpr vec4 operator/(float _f) const noexcept
            {
                return vec4(x / _f, y / _f, z / _f, w / _f);
            }

            inline constexpr vec4& operator=(const vec4& _v4) noexcept = default;
            inline constexpr vec4& operator+=(const vec4& _v4) noexcept
            {
                x += _v4.x;
                y += _v4.y;
                z += _v4.z;
                w += _v4.w;
                return *this;
            }
            inline constexpr vec4& operator-=(const vec4& _v4) noexcept
            {
                x -= _v4.x;
                y -= _v4.y;
                z -= _v4.z;
                w -= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator*=(const vec4& _v4) noexcept
            {
                x *= _v4.x;
                y *= _v4.y;
                z *= _v4.z;
                w *= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator*=(float _f) noexcept
            {
                x *= _f;
                y *= _f;
                z *= _f;
                w *= _f;
                return *this;
            }
            inline constexpr vec4& operator/=(const vec4& _v4) noexcept
            {
                x /= _v4.x;
                y /= _v4.y;
                z /= _v4.z;
                w /= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator/=(float _f) noexcept
            {
                x /= _f;
                y /= _f;
                z /= _f;
                w /= _f;
                return *this;
            }

            // == !=
            inline constexpr bool operator==(const vec4& _v4) const noexcept
            {
                return x == _v4.x && y == _v4.y && z == _v4.z && w == _v4.w;
            }
            inline constexpr bool operator!=(const vec4& _v4) const noexcept
            {
                return x != _v4.x || y != _v4.y || z != _v4.z || w != _v4.w;
            }

            // length/ unit/ dot/ cross
            inline float length() const noexcept
            {
                return sqrt(x * x + y * y + z * z + w * w);
            }
            inline vec4 unit() const noexcept
            {
                float vlength = length();
                if (vlength != 0.f)
                    return vec4(x / vlength, y / vlength, z / vlength, w / vlength);
                return vec4();
            }
            inline constexpr float dot(const vec4& _v4) const noexcept
            {
                return x * _v4.x + y * _v4.y + z * _v4.z + w * _v4.w;
            }

            inline float max() const noexcept
            {
                return std::max(std::max(std::max(x, y), z), w);
            }

            inline float min() const noexcept
            {
                return std::min(std::min(std::min(x, y), z), w);
            }

            static const char* JEScriptTypeName()
            {
                return "vec4";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "public using vec4 = (real, real, real, real);";
            }
            void JEParseFromScriptType(woort_value v)
            {
                x = woort_struct_get_float(v, 0);
                y = woort_struct_get_float(v, 1);
                z = woort_struct_get_float(v, 2);
                w = woort_struct_get_float(v, 3);
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_set_struct(v, 4);
                woort_struct_set_float(v, 0, x);
                woort_struct_set_float(v, 1, y);
                woort_struct_set_float(v, 2, z);
                woort_struct_set_float(v, 3, w);
            }
        };
        inline static constexpr vec4 operator*(float _f, const vec4& _v4) noexcept
        {
            return vec4(_v4.x * _f, _v4.y * _f, _v4.z * _f, _v4.w * _f);
        }

        /*
        标准 sRGB 传输函数（逐通道）。
            * srgb_to_linear：sRGB 编码值 -> 线性值；sRGB 无法表示负
              分量，负输入按 0 处理
            * linear_to_srgb：线性值 -> sRGB 编码值；负值按 0 处理，
              大于 1 的 HDR 分量保持大于 1
        */
        inline float srgb_to_linear(float c) noexcept
        {
            if (c <= 0.f)
                return 0.f;
            if (c <= 0.04045f)
                return c / 12.92f;
            return std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
        inline float linear_to_srgb(float c) noexcept
        {
            if (c <= 0.f)
                return 0.f;
            if (c <= 0.0031308f)
                return 12.92f * c;
            return 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
        }

        /*
        jeecs::math::color [类型]
        引擎内部使用的颜色类型。RGB 分量按线性颜色空间保存（通常为
        [0, 1]，允许 HDR 分量大于 1），alpha 不参与颜色空间转换。

        编辑器显示和最终输出需要 sRGB 编码时使用 to_srgb()；传递给
        shader/管线时统一使用 to_linear() 的结果；从 sRGB 数据（如
        颜色选择器、外部资源）导入时使用 from_srgb()。
        */
        struct color
        {
            float r;
            float g;
            float b;
            float a;

            constexpr color() noexcept
                : r(1.f)
                , g(1.f)
                , b(1.f)
                , a(1.f)
            {
            }
            constexpr color(float _r, float _g, float _b, float _a = 1.f) noexcept
                : r(_r), g(_g), b(_b), a(_a)
            {
            }
            constexpr color(const color&) noexcept = default;
            constexpr color(color&&) noexcept = default;
            constexpr color& operator=(const color&) noexcept = default;
            constexpr color& operator=(color&&) noexcept = default;

            // 线性分量 -> sRGB 编码（编辑器显示 / 最终输出）
            inline vec4 to_srgb() const noexcept
            {
                return vec4(
                    math::linear_to_srgb(r),
                    math::linear_to_srgb(g),
                    math::linear_to_srgb(b),
                    a);
            }
            // 颜色按线性空间保存，返回可供 shader/管线直接使用的 vec4
            inline constexpr vec4 to_linear() const noexcept
            {
                return vec4(r, g, b, a);
            }

            // 从 sRGB 编码构造（编辑器写回 / 外部 sRGB 数据导入）
            static color from_srgb(const vec4& srgb) noexcept
            {
                return color(
                    math::srgb_to_linear(srgb.x),
                    math::srgb_to_linear(srgb.y),
                    math::srgb_to_linear(srgb.z),
                    srgb.w);
            }
            // 从线性 vec4 构造
            static constexpr color from_linear(const vec4& linear) noexcept
            {
                return color(linear.x, linear.y, linear.z, linear.w);
            }

            inline constexpr bool operator==(const color& _color) const noexcept
            {
                return r == _color.r && g == _color.g && b == _color.b && a == _color.a;
            }
            inline constexpr bool operator!=(const color& _color) const noexcept
            {
                return !(*this == _color);
            }

            static const char* JEScriptTypeName()
            {
                return "color";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "public using color = (real, real, real, real);";
            }
            void JEParseFromScriptType(woort_value v)
            {
                r = woort_struct_get_float(v, 0);
                g = woort_struct_get_float(v, 1);
                b = woort_struct_get_float(v, 2);
                a = woort_struct_get_float(v, 3);
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_set_struct(v, 4);
                woort_struct_set_float(v, 0, r);
                woort_struct_set_float(v, 1, g);
                woort_struct_set_float(v, 2, b);
                woort_struct_set_float(v, 3, a);
            }
        };

        struct quat
        {
            float x, y, z, w;

            inline constexpr bool operator==(const quat& q) const noexcept
            {
                return x == q.x && y == q.y && z == q.z && w == q.w;
            }
            inline constexpr bool operator!=(const quat& q) const noexcept
            {
                return x != q.x || y != q.y || z != q.z || w != q.w;
            }

            inline quat(float _x, float _y, float _z, float _w) noexcept
            {
                auto len = sqrt(_x * _x + _y * _y + _z * _z + _w * _w);
                x = _x / len;
                y = _y / len;
                z = _z / len;
                w = _w / len;
            }

            constexpr quat() noexcept
                : x(0.f), y(0.f), z(0.f), w(1.f)
            {
            }

            quat(float yaw, float pitch, float roll) noexcept
            {
                set_euler_angle(yaw, pitch, roll);
            }

            inline void create_matrix(float* pMatrix) const noexcept
            {
                if (!pMatrix)
                    return;
                float m_x = x;
                float m_y = y;
                float m_z = z;
                float m_w = w;

                pMatrix[0] = 1.0f - 2.0f * (m_y * m_y + m_z * m_z);
                pMatrix[1] = 2.0f * (m_x * m_y + m_z * m_w);
                pMatrix[2] = 2.0f * (m_x * m_z - m_y * m_w);
                pMatrix[3] = 0.0f;

                pMatrix[4] = 2.0f * (m_x * m_y - m_z * m_w);
                pMatrix[5] = 1.0f - 2.0f * (m_x * m_x + m_z * m_z);
                pMatrix[6] = 2.0f * (m_z * m_y + m_x * m_w);
                pMatrix[7] = 0.0f;

                pMatrix[8] = 2.0f * (m_x * m_z + m_y * m_w);
                pMatrix[9] = 2.0f * (m_y * m_z - m_x * m_w);
                pMatrix[10] = 1.0f - 2.0f * (m_x * m_x + m_y * m_y);
                pMatrix[11] = 0.0f;

                pMatrix[12] = 0;
                pMatrix[13] = 0;
                pMatrix[14] = 0;
                pMatrix[15] = 1.0f;
            }
            inline void create_inv_matrix(float* pMatrix) const noexcept
            {

                if (!pMatrix)
                    return;
                float m_x = x;
                float m_y = y;
                float m_z = z;
                float m_w = -w;

                pMatrix[0] = 1.0f - 2.0f * (m_y * m_y + m_z * m_z);
                pMatrix[1] = 2.0f * (m_x * m_y + m_z * m_w);
                pMatrix[2] = 2.0f * (m_x * m_z - m_y * m_w);
                pMatrix[3] = 0.0f;

                pMatrix[4] = 2.0f * (m_x * m_y - m_z * m_w);
                pMatrix[5] = 1.0f - 2.0f * (m_x * m_x + m_z * m_z);
                pMatrix[6] = 2.0f * (m_z * m_y + m_x * m_w);
                pMatrix[7] = 0.0f;

                pMatrix[8] = 2.0f * (m_x * m_z + m_y * m_w);
                pMatrix[9] = 2.0f * (m_y * m_z - m_x * m_w);
                pMatrix[10] = 1.0f - 2.0f * (m_x * m_x + m_y * m_y);
                pMatrix[11] = 0.0f;

                pMatrix[12] = 0;
                pMatrix[13] = 0;
                pMatrix[14] = 0;
                pMatrix[15] = 1.0f;
            }

            inline void create_matrix(float (*pMatrix)[4]) const
            {
                create_matrix((float*)pMatrix);
            }
            inline void create_inv_matrix(float (*pMatrix)[4]) const
            {
                create_inv_matrix((float*)pMatrix);
            }

            inline constexpr float dot(const quat& _quat) const noexcept
            {
                return x * _quat.x + y * _quat.y + z * _quat.z + w * _quat.w;
            }
            inline void set_euler_angle(const vec3& euler) noexcept
            {
                set_euler_angle(euler.x, euler.y, euler.z);
            }
            inline void set_euler_angle(float yaw, float pitch, float roll) noexcept
            {
                float angle;
                float sinRoll, sinPitch, sinYaw, cosRoll, cosPitch, cosYaw;

                angle = yaw * DEG2RAD * 0.5f;
                sinYaw = sin(angle);
                cosYaw = cos(angle);
                angle = pitch * DEG2RAD * 0.5f;
                sinPitch = sin(angle);
                cosPitch = cos(angle);
                angle = roll * DEG2RAD * 0.5f;
                sinRoll = sin(angle);
                cosRoll = cos(angle);

                float _y = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
                float _x = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;
                float _z = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
                float _w = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;

                float mag = _x * _x + _y * _y + _z * _z + _w * _w;

                x = _x / mag;
                y = _y / mag;
                z = _z / mag;
                w = _w / mag;
            }
            inline constexpr void set(float _x, float _y, float _z, float _w) noexcept
            {
                x = _x;
                y = _y;
                z = _z;
                w = _w;
            }

            static inline quat euler(float x, float y, float z) noexcept
            {
                quat na;
                na.set_euler_angle(x, y, z);
                return na;
            }
            static inline quat euler(const vec3& v3) noexcept
            {
                quat na;
                na.set_euler_angle(v3.x, v3.y, v3.z);
                return na;
            }
            static inline quat axis_angle(const vec3& a, float arg) noexcept
            {
                auto sv = sin(arg * 0.5f * DEG2RAD);
                auto cv = cos(arg * 0.5f * DEG2RAD);
                return quat(a.x * sv, a.y * sv, a.z * sv, cv);
            }

            static inline quat lerp(const quat& a, const quat& b, float t)
            {
                return quat(
                    math::lerp(a.x, b.x, t),
                    math::lerp(a.y, b.y, t),
                    math::lerp(a.z, b.z, t),
                    math::lerp(a.w, b.w, t));
            }
            static inline quat slerp(const quat& a, const quat& b, float t)
            {
                float cos_theta = a.dot(b);

                // if B is on opposite hemisphere from A, use -B instead
                float sign;
                if (cos_theta < 0.f)
                {
                    cos_theta = -cos_theta;
                    sign = -1.f;
                }
                else
                    sign = 1.f;
                float c1, c2;
                if (cos_theta > 1.f - EPSILON)
                {
                    // if q2 is (within precision limits) the same as q1,
                    // just linear interpolate between A and B.
                    c2 = t;
                    c1 = 1.f - t;
                }
                else
                {
                    // float theta = gFloat::ArcCosTable(cos_theta);
                    // faster than table-based :
                    // const float theta = myacos(cos_theta);
                    float theta = acos(cos_theta);
                    float sin_theta = sin(theta);
                    float t_theta = t * theta;
                    float inv_sin_theta = 1.f / sin_theta;
                    c2 = sin(t_theta) * inv_sin_theta;
                    c1 = sin(theta - t_theta) * inv_sin_theta;
                }
                c2 *= sign; // or c1 *= sign
                // just affects the overrall sign of the output
                // interpolate
                return quat(a.x * c1 + b.x * c2, a.y * c1 + b.y * c2, a.z * c1 + b.z * c2, a.w * c1 + b.w * c2);
            }
            static inline float delta_angle(const quat& lhs, const quat& rhs)
            {
                float cos_theta = lhs.dot(rhs);
                if (cos_theta > 0.f)
                    return 2.f * RAD2DEG * acos(cos_theta);
                else
                    return -2.f * RAD2DEG * acos(-cos_theta);
            }

            static inline quat rotation(const vec3& a, const vec3& b) noexcept
            {
                // 求把 a 转到 b 的旋转：轴为 a × b
                auto axis = a.cross(b);
                auto angle = RAD2DEG * acos(b.dot(a) / (b.length() * a.length()));

                return quat::axis_angle(axis.unit(), angle);
            }

            inline quat conjugate() const noexcept
            {
                return quat(-x, -y, -z, w);
            }
            inline quat inverse() const noexcept
            {
                return quat(-x, -y, -z, w);
            }
            inline vec3 euler_angle() const noexcept
            {
                float yaw = atan2(2 * (w * x + z * y), 1 - 2 * (x * x + y * y));
                float pitch = asin(clamp(2 * (w * y - x * z), -1.0f, 1.0f));
                float roll = atan2(2 * (w * z + x * y), 1 - 2 * (z * z + y * y));
                return vec3(RAD2DEG * yaw, RAD2DEG * pitch, RAD2DEG * roll);
            }

            inline quat operator*(const quat& _quat) const noexcept
            {
                float w1 = w;
                float w2 = _quat.w;

                vec3 v1(x, y, z);
                vec3 v2(_quat.x, _quat.y, _quat.z);
                float w3 = w1 * w2 - v1.dot(v2);
                vec3 v3 = v1.cross(v2) + w1 * v2 + w2 * v1;
                return quat(v3.x, v3.y, v3.z, w3);
            }
            inline constexpr vec3 operator*(const vec3& _v3) const noexcept
            {
                vec3 u(x, y, z);
                return 2.0f * u.dot(_v3) * u + (w * w - u.dot(u)) * _v3 + 2.0f * w * u.cross(_v3);
            }
            static const char* JEScriptTypeName()
            {
                return "quat";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "public using quat = (real, real, real, real);";
            }
            void JEParseFromScriptType(woort_value v)
            {
                x = woort_struct_get_float(v, 0);
                y = woort_struct_get_float(v, 1);
                z = woort_struct_get_float(v, 2);
                w = woort_struct_get_float(v, 3);
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_set_struct(v, 4);
                woort_struct_set_float(v, 0, x);
                woort_struct_set_float(v, 1, y);
                woort_struct_set_float(v, 2, z);
                woort_struct_set_float(v, 3, w);
            }
        };

        inline math::vec3 mat4trans(const float* left_mat, const math::vec3& v3)
        {
            float v33[3] = { v3.x, v3.y, v3.z };
            float result[3];
            mat4xvec3(result, left_mat, v33);

            return math::vec3(result[0], result[1], result[2]);
        }
        inline math::vec3 mat4trans(const float (*left_mat)[4], const math::vec3& v3)
        {
            float v33[3] = { v3.x, v3.y, v3.z };
            float result[3];
            mat4xvec3(result, left_mat, v33);

            return math::vec3(result[0], result[1], result[2]);
        }
        inline math::vec4 mat4trans(const float* left_mat, const math::vec4& v4)
        {
            float v44[4] = { v4.x, v4.y, v4.z, v4.w };
            float result[4];
            mat4xvec4(result, left_mat, v44);

            return math::vec4(result[0], result[1], result[2], result[3]);
        }
        inline math::vec4 mat4trans(const float (*left_mat)[4], const math::vec4& v4)
        {
            float v44[4] = { v4.x, v4.y, v4.z, v4.w };
            float result[4];
            mat4xvec4(result, left_mat, v44);

            return math::vec4(result[0], result[1], result[2], result[3]);
        }

        inline void transform(
            float* out_result,
            const math::vec3& position,
            const math::quat& rotation,
            const math::vec3& scale)
        {
            float temp_mat_trans[4][4] = {};
            temp_mat_trans[0][0] =
                temp_mat_trans[1][1] =
                temp_mat_trans[2][2] =
                temp_mat_trans[3][3] = 1.0f;
            temp_mat_trans[3][0] = position.x;
            temp_mat_trans[3][1] = position.y;
            temp_mat_trans[3][2] = position.z;

            float temp_mat_rotation[4][4];
            rotation.create_matrix(temp_mat_rotation);

            float tmp_rot_trans_mat[4][4];
            math::mat4xmat4(tmp_rot_trans_mat, temp_mat_trans, temp_mat_rotation);

            float temp_mat_scale[4][4] = {};
            temp_mat_scale[0][0] = scale.x;
            temp_mat_scale[1][1] = scale.y;
            temp_mat_scale[2][2] = scale.z;
            temp_mat_scale[3][3] = 1.0f;
            math::mat4xmat4(out_result, (const float*)tmp_rot_trans_mat, (const float*)temp_mat_scale);
        }
        inline void transform(
            float (*out_result)[4],
            const math::vec3& position,
            const math::quat& rotation,
            const math::vec3& scale)
        {
            transform((float*)out_result, position, rotation, scale);
        }
    }
}
#endif // __cplusplus
