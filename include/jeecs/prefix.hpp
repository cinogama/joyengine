#pragma once

// ============================================================================
// jeecs/prefix.hpp —— 公共前导：woort/std 头、JE_API 导入导出宏、平台检测、关键字身份宏、JECS_* 宏与基础标量类型（je_type_hash_t / je_type_id_t）
// =============================================================================


#define _CRT_SECURE_NO_WARNINGS
#include "woort.h"
#define WO_FAIL_JE_FATAL_ERROR 0xD201
#define WO_FAIL_JE_BAD_INIT_SHADER_VALUE 0xD202
#ifdef __cplusplus
#   include <cstdint>
#   include <cstring>
#   include <cstdlib>
#   include <cassert>
#   include <cfloat>
#   include <cstdio>

#   include <typeinfo>

#   include <atomic>
#   include <thread>
#   include <mutex>
#   include <shared_mutex>
#   include <condition_variable>

#   include <set>
#   include <map>
#   include <vector>
#   include <unordered_set>
#   include <unordered_map>
#   include <algorithm>
#   include <functional>
#   include <type_traits>
#   include <cstddef>
#   include <cmath>
#   include <random>
#   include <sstream>
#   include <climits>
#   include <initializer_list>
#   include <optional>
#   include <tuple>
#   include <array>
#   include <variant>
#   include <concepts>

#   include <execution>
#endif // __cplusplus
#ifdef __cplusplus
#   define JE_FORCE_CAPI extern "C"{
#   define JE_FORCE_CAPI_END }
#else
#   define JE_FORCE_CAPI
#   define JE_FORCE_CAPI_END
#endif
#ifdef WO_SHARED_LIB
#   define JE4_SHARED_CORE
#else
#   define JE4_STATIC_CORE
#endif

#define JE_IMPORT WOORT_IMPORT
#define JE_EXPORT WOORT_EXPORT

#ifdef JE_IMPL
#   define JE_IMPORT_OR_EXPORT JE_EXPORT
#else
#   define JE_IMPORT_OR_EXPORT JE_IMPORT
#endif

#ifdef JE4_STATIC_CORE
#   define JE_API
#else
#   define JE_API JE_IMPORT_OR_EXPORT
#endif
// Supported platform list
#define JE4_PLATFORM_WINDOWS 1
#define JE4_PLATFORM_LINUX 2
#define JE4_PLATFORM_ANDROID 3
#define JE4_PLATFORM_WEBGL 4
#define JE4_PLATFORM_MACOS 5

#ifndef JE4_CURRENT_PLATFORM
#   if defined(_WIN32)
#       define JE4_CURRENT_PLATFORM JE4_PLATFORM_WINDOWS
#   elif defined(__ANDROID__)
#       define JE4_CURRENT_PLATFORM JE4_PLATFORM_ANDROID
#   elif defined(__linux__)
#       define JE4_CURRENT_PLATFORM JE4_PLATFORM_LINUX
#   elif defined(__APPLE__)
#       include <TargetConditionals.h>
#       if TARGET_OS_MAC
#           define JE4_CURRENT_PLATFORM JE4_PLATFORM_MACOS
#       else
#           error Unsupported Apple platform.
#       endif
#   elif defined(__EMSCRIPTEN__)
#       define JE4_CURRENT_PLATFORM JE4_PLATFORM_WEBGL
#   else
#       error Unsupported platform.
#   endif
#elif JE4_CURRENT_PLATFORM != JE4_PLATFORM_WINDOWS \
    && JE4_CURRENT_PLATFORM != JE4_PLATFORM_LINUX \
    && JE4_CURRENT_PLATFORM != JE4_PLATFORM_ANDROID \
    && JE4_CURRENT_PLATFORM != JE4_PLATFORM_WEBGL \
    && JE4_CURRENT_PLATFORM != JE4_PLATFORM_MACOS
#   error JE4_CURRENT_PLATFORM must be one of JE4_PLATFORM_WINDOWS, JE4_PLATFORM_LINUX, JE4_PLATFORM_ANDROID, JE4_PLATFORM_WEBGL
#endif
// =============================================================================
// 统一并行遍历入口 je_parallel_foreach
//
// 背景：libstdc++ 的 PSTL（<execution> 并行算法）在内部使用
//   catch(const std::bad_alloc&) 来回收工作线程的 OOM（见 pstl/utils.h），
//   系统头里没有 #ifdef __EXCEPTIONS 保护，因此任何真正的 par_unseq
//   模板实例化在 -fno-exceptions 下都无法编译。
//
// 调用方写：
//   jeecs::parallel_foreach(begin, end, lambda);
// 满足以下全部条件时启用 par_unseq，否则自动退化为串行 std::for_each：
//   1. 标准库提供 <execution>            (__cpp_lib_execution)
//   2. 当前编译单元启用了异常处理        (__EXCEPTIONS 或 _CPPUNWIND)
//   3. 不是 WebGL/Emscripten 目标         (无可用线程池)
//   4. 是 Release/RelWithDebInfo 构建     (NDEBUG，避免调试态并发开销)
//
// 注意：这里必须用 函数模板 而不是函数式宏。原因：调用方传入的 lambda
// 体内可能含有 #ifndef NDEBUG 等预处理指令（见 command_buffer::update），
// 若用函数式宏包装，预处理指令出现在宏实参列表里属未定义行为，
// 在 MSVC /Zc:preprocessor 下会直接报 C2121/C5101。
// =============================================================================
#if defined(__cpp_lib_execution) \
    && (defined(__EXCEPTIONS) || defined(_CPPUNWIND)) \
    && (JE4_CURRENT_PLATFORM != JE4_PLATFORM_WEBGL) \
    && defined(NDEBUG)
#   define JE_PARALLEL_FOREACH_ENABLED 1
#else
#   define JE_PARALLEL_FOREACH_ENABLED 0
#endif

// [用语]
// 此处定义引擎自定义使用的关键字/保留字

#define JERefRegsiter JERefRegsiter
#define JEScriptTypeName JEScriptTypeName
#define JEScriptTypeDeclare JEScriptTypeDeclare
#define JEParseFromScriptType JEParseFromScriptType
#define JEParseToScriptType JEParseToScriptType

#define OnEnable OnEnable   // * 用户系统激活，通常用于初始化
#define OnDisable OnDisable // * 用户系统失活，通常用于清理

#define PreUpdate PreUpdate             // * 用户预更新，
#define StateUpdate StateUpdate         // 用于将初始状态给予各个组件 (Animation VirtualGamepadInput)
#define Update Update                   // * 用户更新
#define TransformUpdate TransformUpdate // 用于更新物体的变换和关系 (Transform)
#define PhysicsUpdate PhysicsUpdate     // 用于物理引擎的状态更新 (PhysicsUpdate)
#define LateUpdate LateUpdate           // * 用户延迟更新
#define CommitUpdate CommitUpdate       // 用于提交最终生效的数据 (Transform, Audio, ScriptRuntime)
#define GraphicUpdate GraphicUpdate     // 用于将数据呈现到用户界面

#define typesof(...) <__VA_ARGS__>      // 用于优化泛型参数列表对代码自动整理的影响，配合需要大量泛型参数的接口使用
// ==========================================================================
// C ABI 类型命名：规范类型为 je_<domain>_<subject...>_t（如 je_type_hash_t），
// 并在 C ABI 段末尾为每个类型提供 je_<DomainSubject> 大驼峰别名（如 je_TypeHash）。
// 这些基础标量类型 C/C++ 通用，必须在 C++ 辅助声明段之前定义，以便 C++ 端
// 以它们为规范提供 jeecs::typing:: 别名，C ABI 签名也直接使用。
// ==========================================================================
typedef size_t   je_type_hash_t;     // 类型哈希值
typedef size_t   je_type_id_t;       // 引擎类型工厂管理的类型 ID（无效值 = (je_type_id_t)-1）
#define JE_INVALID_TYPE_ID ((je_type_id_t)SIZE_MAX) /* je_type_id_t 的无效值，与 JE_INVALID_TYPE_ID 等价 */
#define JECS_DISABLE_MOVE_AND_COPY_OPERATOR(TYPE) \
    TYPE &operator=(const TYPE &) = delete;       \
    TYPE &operator=(TYPE &&) = delete

#define JECS_DISABLE_MOVE_AND_COPY_CONSTRUCTOR(TYPE) \
    TYPE(const TYPE &) = delete;                     \
    TYPE(TYPE &&) = delete

#define JECS_DISABLE_MOVE_AND_COPY(TYPE)          \
    JECS_DISABLE_MOVE_AND_COPY_CONSTRUCTOR(TYPE); \
    JECS_DISABLE_MOVE_AND_COPY_OPERATOR(TYPE)

#define JECS_DEFAULT_CONSTRUCTOR(TYPE) \
    TYPE() = default;                  \
    TYPE(const TYPE &) = default;      \
    TYPE(TYPE &&) = default;
