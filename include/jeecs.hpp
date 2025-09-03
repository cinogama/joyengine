#pragma once

#define JE_CORE_VERSION JE_VERSION_WRAP(4, 8, 6)

#ifndef JE_MSVC_RC_INCLUDE

#define _CRT_SECURE_NO_WARNINGS

#ifndef __cplusplus
#error jeecs.h only support for c++
#else

#include "wo.h"

#define WO_FAIL_JE_FATAL_ERROR 0xD101
#define WO_FAIL_JE_BAD_INIT_SHADER_VALUE 0xD102

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cfloat>

#include <typeinfo>

#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

#include <set>
#include <map>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <cstddef>
#include <cmath>
#include <random>
#include <sstream>
#include <climits>
#include <initializer_list>
#include <optional>
#ifdef __cpp_lib_execution
#include <execution>
#endif

#define JE_FORCE_CAPI WO_FORCE_CAPI
#define JE_FORCE_CAPI_END WO_FORCE_CAPI_END

#ifdef WO_SHARED_LIB
#define JE4_SHARED_CORE
#else
#define JE4_STATIC_CORE
#endif

#define JE_IMPORT WO_IMPORT
#define JE_EXPORT WO_EXPORT

#ifdef JE_IMPL
#define JE_IMPORT_OR_EXPORT JE_EXPORT
#else
#define JE_IMPORT_OR_EXPORT JE_IMPORT
#endif

#ifdef JE4_STATIC_CORE
#define JE_API
#else
#define JE_API JE_IMPORT_OR_EXPORT
#endif

// Supported platform list
#define JE4_PLATFORM_WINDOWS 1
#define JE4_PLATFORM_LINUX 2
#define JE4_PLATFORM_ANDROID 3
#define JE4_PLATFORM_WEBGL 4
#define JE4_PLATFORM_MACOS 5

#ifndef JE4_CURRENT_PLATFORM
#if defined(_WIN32)
#define JE4_CURRENT_PLATFORM JE4_PLATFORM_WINDOWS
#elif defined(__ANDROID__)
#define JE4_CURRENT_PLATFORM JE4_PLATFORM_ANDROID
#elif defined(__linux__)
#define JE4_CURRENT_PLATFORM JE4_PLATFORM_LINUX
#elif defined(__APPLE__)
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       define JE4_CURRENT_PLATFORM JE4_PLATFORM_MACOS
#   else
#       error Unsupported Apple platform.
#   endif
#elif defined(__EMSCRIPTEN__)
#define JE4_CURRENT_PLATFORM JE4_PLATFORM_WEBGL
#else
#error Unsupported platform.
#endif
#elif JE4_CURRENT_PLATFORM != JE4_PLATFORM_WINDOWS \
    && JE4_CURRENT_PLATFORM != JE4_PLATFORM_LINUX \
    && JE4_CURRENT_PLATFORM != JE4_PLATFORM_ANDROID \
    && JE4_CURRENT_PLATFORM != JE4_PLATFORM_WEBGL \
    && JE4_CURRENT_PLATFORM != JE4_PLATFORM_MACOS
#   error JE4_CURRENT_PLATFORM must be one of JE4_PLATFORM_WINDOWS, JE4_PLATFORM_LINUX, JE4_PLATFORM_ANDROID, JE4_PLATFORM_WEBGL
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

/*
jeecs [命名空间]
此处定义引擎自带的所有的C++接口类、函数、类型和常量
*/
namespace jeecs
{
    /*
    jeecs::rendchain_branch [类型]
    可编程图形接口类型，用于表示一组绘制流程
    请参见
        jegl_rendchain
    */
    struct rendchain_branch;

    /*
    jeecs::graphic_uhost [类型]
    可编程图形接口类型，用于表示一个渲染上下文的总和
    */
    struct graphic_uhost;

    /*
    jeecs::typing [命名空间]
    此处定义引擎使用的常用类型和常量值
    */
    namespace typing
    {
        /*
        jeecs::typing::typehash_t [类型别名]
        用于储存哈希值结果的类型
        */
        using typehash_t = size_t;

        /*
        jeecs::typing::typeid_t [类型别名]
        用于储存引擎的类型工厂管理的类型ID，规定的无效值是 jeecs::typing::INVALID_TYPE_ID
            请参见：
            jeecs::typing::INVALID_TYPE_ID
        */
        using typeid_t = size_t;

        /*
        jeecs::typing::INVALID_TYPE_ID [常量]
        jeecs::typing::typeid_t 类型的无效值
        请参见：
            jeecs::typing::typeid_t
        */
        constexpr typeid_t INVALID_TYPE_ID = SIZE_MAX;

        /*
        jeecs::typing::INVALID_UINT32 [常量]
        uint32_t 类型的无效值，通常被用于在图形库的资源中指示此资源不包含有效资源
        请参见：
            jegl_resource
        */
        constexpr uint32_t INVALID_UINT32 = (uint32_t)-1;

        /*
        jeecs::typing::PENDING_UNIFORM_LOCATION [常量]
        图形库的一致变量位置信息初始值，由于此信息需要在对应着色器编译完成后才能确定，
        因此此值暗示当前一致变量位置尚未确定，可通过此值确认shader状态，但不推荐
        请参见：
            jegl_shader
        */
        constexpr uint32_t PENDING_UNIFORM_LOCATION = (uint32_t)-2;

        struct type_info;

        using module_entry_t = void (*)(wo_dylib_handle_t);
        using module_leave_t = void (*)(void);

        using construct_func_t = void (*)(void*, void*, const jeecs::typing::type_info*);
        using destruct_func_t = void (*)(void*);
        using copy_construct_func_t = void (*)(void*, const void*);
        using move_construct_func_t = void (*)(void*, void*);

        using on_enable_or_disable_func_t = void (*)(void*);
        using update_func_t = void (*)(void*);

        using parse_c2w_func_t = void (*)(const void*, wo_vm, wo_value);
        using parse_w2c_func_t = void (*)(void*, wo_vm, wo_value);

        using entity_id_in_chunk_t = size_t;
        using version_t = size_t;

        /*
        jeecs::typing::uuid [类型]
        UUID，一般作为全局唯一标识符使用
        请参见：
            jeecs::typing::uid_t
        */
        struct uuid
        {
            union
            {
                struct
                {
                    uint64_t a;
                    uint64_t b;
                };
                struct
                {
                    uint32_t x; // Time stamp
                    uint16_t y; // Time stamp
                    uint16_t z; // Random

                    uint16_t w; // Inc L16
                    uint16_t u; // Inc H16
                    uint32_t v; // Random
                };
            };

            static uuid generate() noexcept;

            inline bool operator==(const uuid& uid) const noexcept
            {
                return a == uid.a && b == uid.b;
            }

            inline bool operator!=(const uuid& uid) const noexcept
            {
                return !(this->operator==(uid));
            }

            static const char* JEScriptTypeName()
            {
                return "uuid";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "public using uuid = string;";
            }
            void JEParseFromScriptType(wo_vm vm, wo_value v)
            {
                unsigned long long aa, bb;
                ((void)sscanf(wo_string(v), "%llX-%llX", &aa, &bb));
                a = (uint64_t)aa;
                b = (uint64_t)bb;
            }
            void JEParseToScriptType(wo_vm vm, wo_value v) const
            {
                wo_set_string_fmt(v, vm, "%016llX-%016llX",
                    (unsigned long long)a,
                    (unsigned long long)b);
            }
        };

        /*
        jeecs::typing::uid_t [类型别名]
        全局唯一标识符的类型别名
        请参见：
            jeecs::typing::uuid
        */
        using uid_t = uuid;

        /*
        jeecs::typing::ms_stamp_t [类型别名]
        用于储存以毫秒为单位的时间戳的类型别名
        请参见：
            je_clock_time_stamp
        */
        using ms_stamp_t = uint64_t;

        using debug_eid_t = uint64_t;

        template <typename T>
        struct _origin_type
        {
            template <typename U>
            using _origin_t =
                typename std::remove_cv<
                typename std::remove_reference<
                typename std::remove_pointer<U>::type>::type>::type;

            static auto _type_selector() // -> T*
            {
                if constexpr (
                    std::is_reference<T>::value || std::is_pointer<T>::value || std::is_const<T>::value || std::is_volatile<T>::value)
                    return _origin_type<_origin_t<T>>::_type_selector();
                else
                    return (T*)nullptr;
            }

            using type = typename std::remove_pointer<decltype(_type_selector())>::type;
        };

        /*
        jeecs::typing::origin_t<T> [泛型类型别名]
        等效于指定类型T去除 const volatile reference 和 pointer之后的原始类型
        */
        template <typename T>
        using origin_t = typename _origin_type<T>::type;

        template <class F>
        struct function_traits
        {
        private:
            using call_type = function_traits<decltype(&F::operator())>;

        public:
            using return_type = typename call_type::return_type;
            using flat_func_t = typename call_type::flat_func_t;
            using this_t = void;

            static const std::size_t arity = call_type::arity;

            template <std::size_t N>
            struct argument
            {
                static_assert(N < arity, "error: invalid parameter index.");
                using type = typename call_type::template argument<N>::type;
            };
        };

        template <class R, class... Args>
        struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
        {
        };

        template <class R, class... Args>
        struct function_traits<R(Args...)>
        {
            using return_type = R;
            using flat_func_t = R(Args...);
            using this_t = void;

            static const std::size_t arity = sizeof...(Args);

            template <std::size_t N>
            struct argument
            {
                static_assert(N < arity, "error: invalid parameter index.");
                using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
            };
        };

        // member function pointer
        template <class C, class R, class... Args>
        struct function_traits<R(C::*)(Args...)> : public function_traits<R(Args...)>
        {
            using this_t = C;
        };

        // const member function pointer
        template <class C, class R, class... Args>
        struct function_traits<R(C::*)(Args...) const> : public function_traits<R(Args...)>
        {
            using this_t = C;
        };

        // member object pointer
        template <class C, class R>
        struct function_traits<R(C::*)> : public function_traits<R(void)>
        {
            using this_t = C;
        };

        template <class F>
        struct function_traits<F&> : public function_traits<F>
        {
        };

        template <class F>
        struct function_traits<F&&> : public function_traits<F>
        {
        };

        template <size_t n, typename T, typename... Ts>
        struct _variadic_type_indexer
        {
            static auto _type_selector() // -> T*
            {
                if constexpr (n != 0)
                    return _variadic_type_indexer<n - 1, Ts...>::_type_selector();
                else
                    return (T*)nullptr;
            }

            using type = typename std::remove_pointer<decltype(_type_selector())>::type;
        };

        /*
        jeecs::typing::index_types_t<n, Ts...> [泛型类型别名]
        获取给定类型序列Ts，等效于其中第n个类型
        */
        template <size_t n, typename... Ts>
        using index_types_t = typename _variadic_type_indexer<n, Ts...>::type;

        class type_unregister_guard;
    }

    /*
    jeecs::audio [命名空间]
    此处定义引擎音频相关的接口、常量等
    */
    namespace audio
    {
        /*
        jeecs::audio::MAX_AUXILIARY_SENDS [常量]
        用于指示引擎支持的最大辅助发送数量（不等于设备支持的实际发射数量）
        */
        constexpr static size_t MAX_AUXILIARY_SENDS = 8;
    }

    class game_system;
    class game_world;

    /*
    jeecs::game_entity [核心类型]
    引擎的实体索引类型
    用于指示一个ArchChunk中的位置，实体索引会因为组件发生变化（移除或添加）而失效
    因此除非能确保实体绝不发生变动，否则不应该长期持有任何实体索引；即建议每一帧
    即取即用。
    注意：
        * 若实体索引所在的世界仍然存在，那么失效的实体索引仍能进行各项操作，但均
            不生效，亦无法从中获取任何组件。
        * 若实体索引所在的世界已经被销毁，那么实体索引进行的操作可能直接导致崩溃。
            换言之，任何情况下不要跨世界地储存实体索引
    说明：
    ECS引擎中，实体是一个仅存在于概念中的存在：一组有“关联”的组件集合，其关联
    即为实体。而由管理组件存储的ArchType特性决定，组件会在不同的ArchType中迁移，因
    此没有切实有效的手段可以持有一个实体的长期索引。
    实体索引是一个储存有：所属Chunk，位置和版本的三元组。其中，所属Chunk和位置可以
    确定和获取实体的组件，版本则用于确认当前索引是否是有效的——因为地址和位置会在
    实体迁移之后复用，此前持有的实体可能已经移动到其他位置。每次实体的变动，所属的
    Chunk都会对指定位置记录的版本信息进行更新。只需要校验Chunk内的版本和索引的版本
    即可。
    */
    struct game_entity
    {
        enum class entity_stat : uint8_t
        {
            UNAVAILABLE = 0, // Entity is destroied or just not ready,
            READY,           // Entity is OK, and just work as normal.
            PREFAB,          // Current entity is prefab, cannot be selected from arch-system and cannot
        };

        struct meta
        {
            jeecs::typing::version_t m_version;
            jeecs::game_entity::entity_stat m_stat;
        };

        void* _m_in_chunk;
        jeecs::typing::entity_id_in_chunk_t _m_id;
        jeecs::typing::version_t _m_version;

        inline game_entity& _set_arch_chunk_info(
            void* chunk,
            jeecs::typing::entity_id_in_chunk_t index,
            jeecs::typing::version_t ver) noexcept
        {
            _m_in_chunk = chunk;
            _m_id = index;
            _m_version = ver;

            return *this;
        }

        /*
        jeecs::game_entity::get_component [方法]
        从指定实体中获取一个指定类型的组件。
        若实体失效或实体不存在指定组件，则返回nullptr
        否则返回指定类型组件的地址
        */
        template <typename T>
        inline T* get_component() const noexcept;

        /*
        jeecs::game_entity::add_component [方法]
        向指定实体中添加一个指定类型的组件，若最终导致实体的组件发生变化，
        那么旧的实体索引在下一帧失效
            * 若实体失效则返回nullptr,
            * 无论实体是否存在指定的组件，总是创建一个新的组件实例并返回其地址
            * `最后`执行对组件的操作会真正生效，如果生效的是添加操作，那么：
                1. 如果实体此前已有存在的组件，则替换之
                2. 如果实体此前不存在此组件，则更新实体，旧的实体索引将失效
        */
        template <typename T>
        inline T* add_component() const noexcept;

        /*
        jeecs::game_entity::remove_component [方法]
        向指定实体中移除一个指定类型的组件，若最终导致实体的组件发生变化，
        那么旧的实体索引在下一帧失效
            * `最后`执行对组件的操作会真正生效，如果生效的是移除操作，那么：
                1. 如果实体此前已经存在此组件，那么更新实体，旧的实体索引将失效
                2. 如果实体此前不存在此组件，那么无事发生
        */
        template <typename T>
        inline void remove_component() const noexcept;

        /*
        jeecs::game_entity::game_world [方法]
        获取当前实体所属的世界
            * 即便实体索引失效，此方法依然能返回所属的世界
        */
        inline jeecs::game_world game_world() const noexcept;

        /*
        jeecs::game_entity::close [方法]
        若索引未失效，则关闭当前实体索引指向的实体。实体索引将在下一帧失效
        */
        inline void close() const noexcept;

        inline bool operator==(const game_entity& e) const noexcept
        {
            return _m_in_chunk == e._m_in_chunk &&
                _m_id == e._m_id &&
                _m_version == e._m_version;
        }
        inline bool operator!=(const game_entity& e) const noexcept
        {
            return _m_in_chunk != e._m_in_chunk ||
                _m_id != e._m_id ||
                _m_version != e._m_version;
        }
    };

    struct dependence;

    /*
    jeecs::input [命名空间]
    此处定义引擎IO相关的接口、常量等
    */
    namespace input
    {
        constexpr size_t MAX_MOUSE_GROUP_COUNT = 16;

        enum class mousecode : uint8_t
        {
            LEFT,
            MID,
            RIGHT,

            CUSTOM_0 = 16,
            CUSTOM_1,
            CUSTOM_2,
            CUSTOM_3,
            CUSTOM_4,
            CUSTOM_5,
            CUSTOM_6,
            CUSTOM_7,
            CUSTOM_8,

            _COUNT, //
        };
        enum class keycode : uint16_t
        {
            UNKNOWN = 0,

            APOSTROPHE = '\'',
            COMMA = ',',
            MINUS = '-',
            PERIOD = '.',
            SLASH = '/',

            A = 'A',
            B,
            C,
            D,
            E,
            F,
            G,
            H,
            I,
            J,
            K,
            L,
            M,
            N,
            O,
            P,
            Q,
            R,
            S,
            T,
            U,
            V,
            W,
            X,
            Y,
            Z,
            _1 = '1',
            _2,
            _3,
            _4,
            _5,
            _6,
            _7,
            _8,
            _9,
            _0,
            _ = ' ',

            SEMICOLON = ';',
            EQUAL = '=',
            LEFT_BRACKET = '[',
            BACKSLASH = '\\',
            RIGHT_BRACKET = ']',
            GRAVE_ACCENT = '`',

            L_SHIFT = 128,
            R_SHIFT,
            L_CTRL,
            R_CTRL,
            L_ALT,
            R_ALT,
            TAB,
            ENTER,
            ESC,
            BACKSPACE,

            NP_0,
            NP_1,
            NP_2,
            NP_3,
            NP_4,
            NP_5,
            NP_6,
            NP_7,
            NP_8,
            NP_9,
            NP_DECIMAL,
            NP_DIVIDE,
            NP_MULTIPLY,
            NP_SUBTRACT,
            NP_ADD,
            NP_ENTER,

            UP,
            DOWN,
            LEFT,
            RIGHT,

            F1,
            F2,
            F3,
            F4,
            F5,
            F6,
            F7,
            F8,
            F9,
            F10,
            F11,
            F12,
            F13,
            F14,
            F15,
            F16,

            CUSTOM_0 = 256,
            CUSTOM_1,
            CUSTOM_2,
            CUSTOM_3,
            CUSTOM_4,
            CUSTOM_5,
            CUSTOM_6,
            CUSTOM_7,
            CUSTOM_8,

            _COUNT, //
        };
        enum class gamepadcode : uint8_t
        {
            UP,
            DOWN,
            LEFT,
            RIGHT,

            A,
            B,
            X,
            Y,

            // LT,
            // RT,
            LB,
            RB,
            LS,
            RS,

            SELECT,
            START,
            GUIDE,

            _COUNT, //
        };
        enum class joystickcode : uint8_t
        {
            L,
            R,
            LT, // Use x value only.
            RT, // Use x value only.

            _COUNT, //
        };
    }

    /*
    jeecs::graphic [命名空间]
    此处定义引擎图形库相关资源的封装类型和工具
    */
    namespace graphic
    {
        struct character;
        /*
        jeecs::graphic::EDITOR_GIZMO_BRANCH_QUEUE [常量]
        用于指示默认的编辑器Gizmo 绘制集合的优先级，通常情况下没有特别的作用
        */
        inline constexpr int EDITOR_GIZMO_BRANCH_QUEUE = 50000;

    }
}

namespace std
{
    template <>
    struct hash<jeecs::typing::uid_t>
    {
        inline constexpr size_t operator()(const jeecs::typing::uid_t& uid) const noexcept
        {
            if constexpr (sizeof(size_t) == 8)
                return uid.b ^ uid.a;
            else
                return (size_t)(uid.b >> 32) ^ (size_t)uid.a;
        }
    };

    template <>
    struct equal_to<jeecs::typing::uid_t>
    {
        inline constexpr size_t operator()(const jeecs::typing::uid_t& a, const jeecs::typing::uid_t& b) const noexcept
        {
            return a.a == b.a && a.b == b.b;
        }
    };
}

JE_FORCE_CAPI

/*
je_mem_alloc [基本接口]
引擎的统一内存申请函数。
*/
JE_API void* je_mem_alloc(size_t sz);

/*
je_mem_realloc [基本接口]
引擎的统一内存重申请函数。
*/
JE_API void* je_mem_realloc(void* mem, size_t sz);

/*
je_mem_free [基本接口]
引擎的统一内存释放函数。
*/
JE_API void je_mem_free(void* ptr);

/*
je_init [基本接口]
引擎的基本初始化操作，应当在所有其他引擎相关的操作之前调用
参数为应用程序启动时的命令行参数，若不希望提供，可以传入：
    argc = 0, argv = nullptr
请参见：
    je_finish
*/
JE_API void je_init(int argc, char** argv);

/*
je_finish [基本接口]
引擎的退出操作，应当在所有其他引擎相关的操作之后调用
    * 引擎退出完毕之后可以重新调用 je_init 重新启动
请参见：
    je_init
*/
JE_API void je_finish(void);

/*
je_build_version [基本接口]
用于获取引擎编译时信息
*/
JE_API const char* je_build_version();

/*
je_build_commit [基本接口]
用于获取当前引擎所在的提交哈希
* 若使用的是手动构建的引擎，此函数将返回 "untracked"
*/
JE_API const char* je_build_commit();

/*
JE_LOG_NORMAL [宏常量]
用于标志通常日志等级
*/
#define JE_LOG_NORMAL 0

/*
JE_LOG_INFO [宏常量]
用于标志信息日志等级
*/
#define JE_LOG_INFO 1

/*
JE_LOG_WARNING [宏常量]
用于标志警告日志等级
*/
#define JE_LOG_WARNING 2

/*
JE_LOG_ERROR [宏常量]
用于标志错误日志等级
*/
#define JE_LOG_ERROR 3

/*
JE_LOG_FATAL [宏常量]
用于标志致命错误日志等级
*/
#define JE_LOG_FATAL 4

/*
je_log_register_callback [基本接口]
用于注册一个日志发生时的回调函数，返回注册句柄。
*/
JE_API size_t je_log_register_callback(void (*func)(int level, const char* msg, void* custom), void* custom);

/*
je_log_unregister_callback [基本接口]
用于释放（解除注册）一个之前注册的日志发生时回调函数。
*/
JE_API void* je_log_unregister_callback(size_t regid);

/*
je_log [基本接口]
以指定的日志等级，向日志系统提交一条日志
*/
JE_API void je_log(int level, const char* format, ...);

typedef enum je_typing_class
{
    JE_BASIC_TYPE,
    JE_COMPONENT,
    JE_SYSTEM,
} je_typing_class;

/*
je_typing_register [基本接口]
向引擎的类型管理器注册一个类型及其基本信息，返回记录当前类型的类型地址
* 类型的名称是区分的唯一标记符，不同类型必须使用不同的名字。
* 必须通过 je_typing_unregister 在适当时机释放
请参见：
    jeecs::typing::typeid_t
    je_typing_unregister
*/
JE_API const jeecs::typing::type_info* je_typing_register(
    const char* _name,
    jeecs::typing::typehash_t _hash,
    size_t _size,
    size_t _align,
    je_typing_class _typecls,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t _destructor,
    jeecs::typing::copy_construct_func_t _copy_constructor,
    jeecs::typing::move_construct_func_t _move_constructor);

/*
je_typing_reset [基本接口]
被设计用于重置指定类型的大小、对齐、成员和构造函数等
    * 成员字段将被重置，请重新注册
*/
JE_API void je_typing_reset(
    const jeecs::typing::type_info* _tinfo,
    size_t _size,
    size_t _align,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t _destructor,
    jeecs::typing::copy_construct_func_t _copy_constructor,
    jeecs::typing::move_construct_func_t _move_constructor);

/*
je_typing_get_info_by_id [基本接口]
通过类型id获取类型信息，若给定的id不合法，返回nullptr
请参见：
    jeecs::typing::typeid_t
    jeecs::typing::type_info
*/
JE_API const jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id);

/*
je_typing_get_info_by_hash [基本接口]
通过类型的哈希值获取类型信息，若给定的类型哈希不合法，返回nullptr
请参见：
    jeecs::typing::typehash_t
    jeecs::typing::type_info
*/
JE_API const jeecs::typing::type_info* je_typing_get_info_by_hash(
    jeecs::typing::typehash_t _hash);

/*
je_typing_get_info_by_name [基本接口]
通过类型的名称获取类型信息，若给定的类型名不合法或不存在，返回nullptr
请参见：
    jeecs::typing::type_info
*/
JE_API const jeecs::typing::type_info* je_typing_get_info_by_name(
    const char* type_name);

/*
je_typing_unregister [基本接口]
向引擎的类型管理器要求解除注册指定的类型信息
需要注意的是，一般而言引擎推荐遵循“谁注册谁释放”原则，请确保释放的
类型是当前模块通过 je_typing_register 成功注册的类型。
若释放的类型不合法，则给出级别错误的日志信息。
请参见：
    jeecs::typing::type_info
    je_typing_register
*/
JE_API void je_typing_unregister(const jeecs::typing::type_info* tinfo);

/*
je_register_member [基本接口]
向引擎的类型管理器注册指定类型的成员信息。
* 使用本地typeinfo，而非全局通用typeinfo
请参见：
    jeecs::typing::type_info
*/
JE_API void je_register_member(
    const jeecs::typing::type_info* _classtype,
    const jeecs::typing::type_info* _membertype,
    const char* _member_name,
    const char* _woovalue_type_may_null,
    wo_value _woovalue_init_may_null,
    ptrdiff_t _member_offset);

/*
je_register_script_parser [基本接口]
向引擎的类型管理器注册指定类型的脚本转换方法。
* 使用本地typeinfo，而非全局通用typeinfo
请参见：
    jeecs::typing::type_info
*/
JE_API void je_register_script_parser(
    const jeecs::typing::type_info* _type,
    jeecs::typing::parse_c2w_func_t c2w,
    jeecs::typing::parse_w2c_func_t w2c,
    const char* woolang_typename,
    const char* woolang_typedecl);

/*
je_register_system_updater [基本接口]
向引擎的类型管理器注册指定类型的系统更新方法。
* 使用本地typeinfo，而非全局通用typeinfo
请参见：
    jeecs::typing::type_info
*/
JE_API void je_register_system_updater(
    const jeecs::typing::type_info* _type,
    jeecs::typing::on_enable_or_disable_func_t _on_enable,
    jeecs::typing::on_enable_or_disable_func_t _on_disable,
    jeecs::typing::update_func_t _pre_update,
    jeecs::typing::update_func_t _state_update,
    jeecs::typing::update_func_t _update,
    jeecs::typing::update_func_t _physics_update,
    jeecs::typing::update_func_t _transform_update,
    jeecs::typing::update_func_t _late_update,
    jeecs::typing::update_func_t _commit_update,
    jeecs::typing::update_func_t _graphic_update);

////////////////////// ToWoo //////////////////////
/*
je_towoo_update_api [基本接口]
根据类型信息重新生成 je/api/.. 的接口脚本
请参见：
    jeecs::typing::type_info
*/
JE_API void je_towoo_update_api();

JE_API const jeecs::typing::type_info* je_towoo_register_system(
    const char* system_name,
    const char* script_path);

JE_API void je_towoo_unregister_system(const jeecs::typing::type_info* tinfo);

////////////////////// ARCH //////////////////////

/*
je_arch_get_chunk [基本接口]
通过给定的ArchType，获取其首个Chunk，由于ArchType总是至少有一个Chunk，
所以总是返回非nullptr值。
    * 此方法一般由selector调用，用于获取指定ArchType中的组件信息
*/
JE_API void* je_arch_get_chunk(void* archtype);

/*
je_arch_next_chunk [基本接口]
通过给定的Chunk，获取其下一个Chunk，如果给定Chunk没有后继则返回nullptr
    * 此方法一般由selector调用，用于获取指定ArchType中的组件信息
*/
JE_API void* je_arch_next_chunk(void* chunk);

/*
je_arch_entity_meta_addr_in_chunk [基本接口]
通过给定的Chunk，获取Chunk中的实体元数据起始地址。
*/
JE_API const jeecs::game_entity::meta* je_arch_entity_meta_addr_in_chunk(void* chunk);

////////////////////// ECS //////////////////////

/*
je_ecs_universe_create [基本接口]
创建指定的宇宙（全局上下文）
引擎允许同时存在多个Universe，原则上不同Universe之间的数据严格隔离并无关
引擎中所有的世界、实体（组件）都位于特定的宇宙中。
说明：
    宇宙创建后会创建一个线程，在循环内执行逻辑更新等操作。此循环会在调用
    je_ecs_universe_stop终止且所有世界关闭之后退出，并依次执行以下操作：
        1. 处理剩余的Universe消息
        2. 按注册的相反顺序调用退出时回调，若Universe仍有消息未处理，
        返回第一步
        3. 解除注册所有Job
    完成全部操作后，宇宙将处于可销毁状态。
*/
JE_API void* je_ecs_universe_create(void);

/*
je_ecs_universe_loop [基本接口]
等待指定的宇宙直到其完全退出运行，这是一个阻塞函数。
此函数一般用于宇宙创建并完成初始状态设定之后，阻塞主线程避免过早的退出。
    * 由于实现机制（依赖Universe的退出回调函数）限制，不允许在Universe被
    stop之后调用，否则将导致死锁
请参见：
    je_ecs_universe_register_exit_callback
*/
JE_API void je_ecs_universe_loop(void* universe);

/*
je_ecs_universe_destroy [基本接口]
销毁一个宇宙，阻塞直到Universe完全销毁
请参见：
    je_ecs_universe_loop
*/
JE_API void je_ecs_universe_destroy(void* universe);

/*
je_ecs_universe_stop [基本接口]
请求终止指定宇宙的运行，此函数会请求销毁宇宙中的所有世界，宇宙会在所有世界完全关闭
后终止运行。
* 这意味着如果在退出过程中创建了新的世界，宇宙的工作将继续持续直到这些世界完全退出，
    因此不推荐在组件/系统的析构函数中做多余的逻辑操作。析构函数仅用于释放资源。
*/
JE_API void je_ecs_universe_stop(void* universe);

/*
je_ecs_universe_register_exit_callback [基本接口]
向指定宇宙中注册宇宙关闭之后的回调函数，关于回调函数的调用时机：
    * 不能对已经终止的宇宙注册回调，回调函数将不会得到执行，并导致内存泄漏！
请参见
    je_ecs_universe_create
*/
JE_API void je_ecs_universe_register_exit_callback(
    void* universe,
    void (*callback)(void*),
    void* arg);

typedef void (*je_job_for_worlds_t)(void* /*world*/, void* /*custom_data*/);
typedef void (*je_job_call_once_t)(void* /*custom_data*/);

/*
je_ecs_universe_register_pre_for_worlds_job [基本接口]
向指定宇宙中注册优先遍历世界任务（Pre job for worlds）
*/
JE_API void je_ecs_universe_register_pre_for_worlds_job(
    void* universe,
    je_job_for_worlds_t job,
    void* data,
    void (*freefunc)(void*));

/*
je_ecs_universe_register_pre_for_worlds_job [基本接口]
向指定宇宙中注册优先单独任务（Pre job for once）
*/
JE_API void je_ecs_universe_register_pre_call_once_job(
    void* universe,
    je_job_call_once_t job,
    void* data,
    void (*freefunc)(void*));

/*
je_ecs_universe_register_for_worlds_job [基本接口]
向指定宇宙中注册普通遍历世界任务（Job for worlds）
*/
JE_API void je_ecs_universe_register_for_worlds_job(
    void* universe,
    je_job_for_worlds_t job,
    void* data,
    void (*freefunc)(void*));

/*
je_ecs_universe_register_call_once_job [基本接口]
向指定宇宙中注册普通单独任务（Job for once）
*/
JE_API void je_ecs_universe_register_call_once_job(
    void* universe,
    je_job_call_once_t job,
    void* data,
    void (*freefunc)(void*));

/*
je_ecs_universe_register_after_for_worlds_job [基本接口]
向指定宇宙中注册延后遍历世界任务（Defer job for worlds）
*/
JE_API void je_ecs_universe_register_after_for_worlds_job(
    void* universe,
    je_job_for_worlds_t job,
    void* data,
    void (*freefunc)(void*));

/*
je_ecs_universe_register_after_call_once_job [基本接口]
向指定宇宙中注册延后单独任务（Defer job for once）
*/
JE_API void je_ecs_universe_register_after_call_once_job(
    void* universe,
    je_job_call_once_t job,
    void* data,
    void (*freefunc)(void*));

/*
je_ecs_universe_unregister_pre_for_worlds_job [基本接口]
从指定宇宙中取消优先遍历世界任务（Pre job for worlds）
*/
JE_API void je_ecs_universe_unregister_pre_for_worlds_job(
    void* universe,
    je_job_for_worlds_t job);

/*
je_ecs_universe_unregister_pre_call_once_job [基本接口]
从指定宇宙中取消优先单独任务（Pre job for once）
*/
JE_API void je_ecs_universe_unregister_pre_call_once_job(
    void* universe,
    je_job_call_once_t job);

/*
je_ecs_universe_unregister_for_worlds_job [基本接口]
从指定宇宙中取消普通遍历世界任务（Job for worlds）
*/
JE_API void je_ecs_universe_unregister_for_worlds_job(
    void* universe,
    je_job_for_worlds_t job);

/*
je_ecs_universe_unregister_call_once_job [基本接口]
从指定宇宙中取消普通单独任务（Job for once）
*/
JE_API void je_ecs_universe_unregister_call_once_job(
    void* universe,
    je_job_call_once_t job);

/*
je_ecs_universe_unregister_after_for_worlds_job [基本接口]
从指定宇宙中取消延后遍历世界任务（After job for worlds）
*/
JE_API void je_ecs_universe_unregister_after_for_worlds_job(
    void* universe,
    je_job_for_worlds_t job);

/*
je_ecs_universe_unregister_after_call_once_job [基本接口]
从指定宇宙中取消延后单独任务（After job for once）
*/
JE_API void je_ecs_universe_unregister_after_call_once_job(
    void* universe,
    je_job_call_once_t job);

/*
je_ecs_universe_get_frame_deltatime [基本接口]
获取当前宇宙的期待的帧更新间隔时间，默认为 1.0/60.0
请参见：
    je_ecs_universe_set_deltatime
*/
JE_API double je_ecs_universe_get_frame_deltatime(
    void* universe);

/*
je_ecs_universe_set_frame_deltatime [基本接口]
设置当前宇宙的帧更新间隔时间
*/
JE_API void je_ecs_universe_set_frame_deltatime(
    void* universe,
    double delta);

/*
je_ecs_universe_get_real_deltatime [基本接口]
获取当前宇宙的实际更新间隔，即距离上次更新的实际时间差异
*/
JE_API double je_ecs_universe_get_real_deltatime(void* universe);

/*
je_ecs_universe_get_smooth_deltatime [基本接口]
获取当前宇宙的平滑更新间隔，是过去若干帧的间隔平均值
*/
JE_API double je_ecs_universe_get_smooth_deltatime(void* universe);

/*
je_ecs_universe_get_max_deltatime [基本接口]
获取当前宇宙的最大时间间隔，deltatime的最大值即为此值
*/
JE_API double je_ecs_universe_get_max_deltatime(void* universe);

/*
je_ecs_universe_set_max_deltatime [基本接口]
设置当前宇宙的最大时间间隔，deltatime的最大值即为此值
    * 此值仅约束缩放之前的deltatime
请参见：
    je_ecs_universe_set_time_scale
*/
JE_API void je_ecs_universe_set_max_deltatime(void* universe, double val);

/*
je_ecs_universe_set_time_scale [基本接口]
设置当前宇宙的时间缩放系数
*/
JE_API void je_ecs_universe_set_time_scale(void* universe, double scale);

/*
je_ecs_universe_get_time_scale [基本接口]
获取当前宇宙的时间缩放系数
*/
JE_API double je_ecs_universe_get_time_scale(void* universe);

/*
je_ecs_world_in_universe [基本接口]
获取指定世界所属的宇宙
*/
JE_API void* je_ecs_world_in_universe(void* world);

/*
je_ecs_world_create [基本接口]
在指定的宇宙中创建一个世界
    * 世界在创建之后，默认为非激活状态，请在初始化操作完成后调用
        je_ecs_world_set_able 将世界激活
请参见：
    je_ecs_world_set_able
*/
JE_API void* je_ecs_world_create(void* in_universe);

/*
je_ecs_world_destroy [基本接口]
从指定的宇宙中销毁一个世界
    * 销毁世界不会立即生效，而是要等到下一次逻辑更新
    * 世界销毁时，会按照如下顺序执行所有销毁操作：
        0. 被标记为即将销毁
        1. 销毁所有系统
        2. 销毁所有实体
        3. 执行最后命令缓冲区更新
        4. 被宇宙从世界列表中移除
    * 向一个正在销毁中的世界中创建实体或添加系统是无效的
请参见：
    je_ecs_world_add_system_instance
    je_ecs_world_create_entity_with_components
*/
JE_API void je_ecs_world_destroy(void* world);

/*
je_ecs_world_is_valid [基本接口]
检验指定的世界是否是一个有效的世界——即是否仍然存活且未开始销毁
*/
JE_API bool je_ecs_world_is_valid(void* world);

/*
je_ecs_world_archmgr_updated_version [基本接口]
获取当前世界的 ArchManager 的版本号
此函数可获取世界的 ArchType 是否有增加，一般用于selector检测是否需要更新 ArchType 缓存
*/
JE_API size_t je_ecs_world_archmgr_updated_version(void* world);

/*
je_ecs_world_update_dependences_archinfo [基本接口]
从当前世界更新类型依赖信息（即 ArchType 缓存）
此函数一般用于selector更新自身某步 dependence 的 ArchType 缓存
*/
JE_API void je_ecs_world_update_dependences_archinfo(
    void* world,
    jeecs::dependence* dependence);

/*
je_ecs_clear_dependence_archinfos [基本接口]
释放依赖中的 ArchType 缓存信息
此函数一般用于selector销毁时，释放持有的 ArchType 缓存
*/
JE_API void je_ecs_clear_dependence_archinfos(jeecs::dependence* dependence);

/*
je_ecs_world_add_system_instance [基本接口]
向指定世界中添加一个指定类型的系统实例，返回此实例的指针
每次更新时，一帧内`最后`执行的操作将会生效，如果生效的是添加系统操作，那么：
    1. 若此前世界中不存在同类型的系统，则添加
    2. 若此前世界中已经存在同类型系统，则替换

    * 若向一个正在销毁中的世界添加系统实例，返回 nullptr
*/
JE_API jeecs::game_system* je_ecs_world_add_system_instance(
    void* world,
    jeecs::typing::typeid_t type);

/*
je_ecs_world_get_system_instance [基本接口]
从指定世界中获取一个指定类型的系统实例，返回此实例的指针
若世界中不存在此类型的系统，返回nullptr
*/
JE_API jeecs::game_system* je_ecs_world_get_system_instance(
    void* world,
    jeecs::typing::typeid_t type);

/*
je_ecs_world_remove_system_instance [基本接口]
从指定世界中移除一个指定类型的系统实例
每次更新时，一帧内`最后`执行的操作将会生效，如果生效的是移除系统操作，那么：
    1. 若此前世界中不存在同类型的系统，则移除
    2. 若此前世界中已经存在同类型系统，则无事发生
*/
JE_API void je_ecs_world_remove_system_instance(
    void* world,
    jeecs::typing::typeid_t type);

/*
je_ecs_world_create_entity_with_components [基本接口]
向指定世界中创建一个用于指定组件集合的实体，创建结果通过参数 out_entity 返回
component_ids 应该指向一个储存有N+1个jeecs::typing::typeid_t实例的连续空间，
其中，N是组件种类数量且不应该为0，空间的最后应该是jeecs::typing::INVALID_TYPE_ID
以表示结束。
    * 若向一个正在销毁中的世界创建实体，则创建失败，out_entity将被写入`无效值`
请参见：
    jeecs::typing::typeid_t
    jeecs::typing::INVALID_TYPE_ID
    jeecs::game_entity
    jeecs::game_world::add_entity
*/
JE_API void je_ecs_world_create_entity_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::typing::typeid_t* component_ids);

/*
je_ecs_world_create_prefab_with_components [基本接口]
向指定世界中创建一个用于指定组件集合的预设体，预设体的行为类似实体但不会被选择器获取，
也不能添加或者删除组件。
    * 预设体被用于快速实例化对象
请参见：
    je_ecs_world_create_entity_with_components
*/
JE_API void je_ecs_world_create_prefab_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::typing::typeid_t* component_ids);

/*
je_ecs_world_create_entity_with_prefab [基本接口]
向指定世界中创建一个用于指定组件集合的实体，该实体的组件及数据由指定的原件实体决定，
    * 原件实体可以由 je_ecs_world_create_entity_with_components，
        je_ecs_world_create_prefab_with_components，或
        je_ecs_world_create_entity_with_prefab 创建
    * 用于快速实例化对象
请参见：
    je_ecs_world_create_entity_with_components
    je_ecs_world_create_prefab_with_components
*/
JE_API void je_ecs_world_create_entity_with_prefab(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::game_entity* prefab);

/*
je_ecs_world_destroy_entity [基本接口]
从世界中销毁一个实体索引指定的相关组件
若实体索引是`无效值`或已失效，则无事发生
请参见：
    jeecs::game_entity::close
*/
JE_API void je_ecs_world_destroy_entity(
    void* world,
    const jeecs::game_entity* entity);

/*
je_ecs_world_entity_add_component [基本接口]
向实体中添加一个组件，无论如何，总是返回一个新的组件实例的地址
若实体索引是`无效值`或已失效，则最终无事发生
引擎对于增加或移除组件的操作遵循最后生效原则，即每次更新时，针对一个特定组件类型一帧
内`最后`执行的操作将会生效，如果生效的是添加组件操作，那么：
    1. 若实体已经存在同类型组件，则替换之
    2. 若实体不存在同类型组件，则更新实体
请参见：
    jeecs::game_entity::add_component
*/
JE_API void* je_ecs_world_entity_add_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type);

/*
je_ecs_world_entity_remove_component [基本接口]
从实体中移除一个组件
若实体索引是`无效值`或已失效，则最终无事发生
引擎对于增加或移除组件的操作遵循最后生效原则，即每次更新时，针对一个特定组件类型一帧
内`最后`执行的操作将会生效，如果生效的是移除组件操作，那么：
    1. 若实体已经存在同类型组件，则移除之
    2. 若实体不存在同类型组件，则无事发生
请参见：
    jeecs::game_entity::remove_component
*/
JE_API void je_ecs_world_entity_remove_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type);

/*
je_ecs_world_entity_get_component [基本接口]
从实体中获取一个组件
若实体索引是`无效值`或已失效，或者实体不存在指定类型的组件，则返回nullptr
请参见：
    jeecs::game_entity::get_component
*/
JE_API void* je_ecs_world_entity_get_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type);

/*
je_ecs_world_of_entity [基本接口]
获取实体所在的世界
若实体索引是`无效值`，则返回nullptr
请参见：
    jeecs::game_entity::game_world
*/
JE_API void* je_ecs_world_of_entity(
    const jeecs::game_entity* entity);

/*
je_ecs_world_set_able [基本接口]
设置世界是否被激活
    * 若世界未激活，则实体组件系统更新将被暂停，世界任务亦将被跳过，仅响应
        世界销毁请求和激活世界请求
    * 世界在激活/取消激活时，所有系统的对应回调会被执行；如果系统实例创建时，
        世界尚未激活，则系统的回调函数不会被执行
*/
JE_API void je_ecs_world_set_able(void* world, bool enable);

// ATTENTION: These 2 functions have no thread-safe-promise.
/*
je_ecs_get_name_of_entity [基本接口]
获取实体的名称，一般只用于调试使用，不建议使用在实际项目中
若实体不包含 Editor::Name 或实体索引是`无效值`或已失效，则返回空字符串
从实体中取出的名称必须立即复制到其他位置或使用完毕，此函数直接
返回 Editor::Name 组件中的字符串地址，这意味着如果实体发生更新、重命名
或者其他操作，取出的字符串可能失效
*/
JE_API const char* je_ecs_get_name_of_entity(
    const jeecs::game_entity* entity);

/*
je_ecs_set_name_of_entity [基本接口]
设置实体的名称，一般只用于调试使用，不建议使用在实际项目中
若实体索引是`无效值`或已失效，则返回空字符串，否则返回设置完成之后的新名称
实体的名称被储存在内置组件 Editor::Name 中，这意味着给实体命名将可能
使得实体索引失效
另外，从实体中取出的名称必须立即复制到其他位置或使用完毕，此函数直接
返回 Editor::Name 组件中的字符串地址，这意味着如果实体发生更新、重命名
或者其他操作，取出的字符串可能失效
若实体不包含 Editor::Name，则创建后再进行设置
*/
JE_API const char* je_ecs_set_name_of_entity(
    const jeecs::game_entity* entity,
    const char* name);
/////////////////////////// Time&Sleep /////////////////////////////////

/*
je_clock_get_sleep_suppression [基本接口]
获取引擎的提前唤醒间隔，单位是秒
此值影响sleep操作，将会减少sleep的实际时间，这有助于提升画面的流畅度，
但会增加CPU空转导致的开销（虽然不影响实际性能）
请参见：
    je_clock_set_sleep_suppression
*/
JE_API double je_clock_get_sleep_suppression();

/*
je_clock_set_sleep_suppression [基本接口]
设置引擎的提前唤醒间隔，单位是秒
此值影响sleep操作，将会减少sleep的实际时间，这有助于提升画面的流畅度，
但会增加CPU空转导致的开销（虽然不影响实际性能）
请参见：
    je_clock_get_sleep_suppression
*/
JE_API void je_clock_set_sleep_suppression(double v);

/*
je_clock_time [基本接口]
获取引擎的运行时间戳，单位是秒
获取到的时间是引擎自启动到当前为止的时间
*/
JE_API double je_clock_time();

/*
je_clock_time_stamp [基本接口]
获取引擎的运行时间戳，单位是毫秒
获取到的时间是引擎自启动到当前为止的时间
*/
JE_API jeecs::typing::ms_stamp_t je_clock_time_stamp();

/*
je_clock_sleep_until [基本接口]
挂起当前线程直到指定时间点
*/
JE_API void je_clock_sleep_until(double time);

/*
je_clock_sleep_until [基本接口]
挂起当前线程若干秒
*/
JE_API void je_clock_sleep_for(double time);

/////////////////////////// JUID /////////////////////////////////

/*
je_uid_generate [基本接口]
生成一个uuid
请参见：
    jeecs::typing::uid_t
*/
JE_API void je_uid_generate(jeecs::typing::uid_t* out_uid);

/////////////////////////// FILE /////////////////////////////////

/*
jeecs_fimg_file [类型]
镜像文件类型，用于加载从镜像中读取的文件时储存镜像文件的上下文信息
一般使用此类型的指针类型
请参见：
    jeecs_file
*/
struct jeecs_fimg_file;

/*
fimg_creating_context [类型]
镜像上下文，用于创建镜像时保存创建上下文信息
一般使用此类型的指针类型
请参见：
    jeecs_file_image_begin
    jeecs_file_image_pack_file
    jeecs_file_image_pack_buffer
    jeecs_file_image_finish
*/
struct fimg_creating_context;

enum je_read_file_seek_mode
{
    JE_READ_FILE_SET = SEEK_SET,
    JE_READ_FILE_CURRENT = SEEK_CUR,
    JE_READ_FILE_END = SEEK_END,
};

typedef void* jeecs_raw_file;

typedef jeecs_raw_file(*je_read_file_open_func_t)(const char*, size_t*);
typedef size_t(*je_read_file_func_t)(void*, size_t, size_t, jeecs_raw_file);
typedef size_t(*je_read_file_tell_func_t)(jeecs_raw_file);
typedef int (*je_read_file_seek_func_t)(jeecs_raw_file, int64_t, je_read_file_seek_mode);
typedef int (*je_read_file_close_func_t)(jeecs_raw_file);

/*
jeecs_file [类型]
文件，用于保存引擎读取的文件
一般使用此类型的指针类型
请参见：
    jeecs_file_open
    jeecs_file_close
    jeecs_file_read
*/
struct jeecs_file
{
    jeecs_fimg_file* m_image_file_handle;
    jeecs_raw_file m_native_file_handle;
    size_t m_file_length;
};

/*
jeecs_file_set_host_path [基本接口]
设置当前引擎的宿主环境在路径，不影响“实际可执行文件所在路径”
    * 设置此路径将影响以 ! 开头的路径的实际位置
    * 正常情况下，这个接口不需要调用，在初始化时，
    引擎会自动设置为实际二进制文件所在路径，但是对于一些特殊平台，引擎
    不能使用这些路径（出于需要创建缓存文件或需要读取镜像等资源文件）
    使用此接口可以使得引擎内置机制使用指定的路径
*/
JE_API void jeecs_file_set_host_path(const char* path);

/*
jeecs_file_set_runtime_path [基本接口]
设置当前引擎的运行时路径，不影响“工作路径”
    * 设置此路径将影响以 @ 开头的路径的实际位置
    * 设置路径时，引擎会尝试以相同参数调用jeecs_file_update_default_fimg
请参考：
    jeecs_file_update_default_fimg
*/
JE_API void jeecs_file_set_runtime_path(const char* path);

/*
* jeecs_file_update_default_fimg [基本接口]
读取指定位置的镜像文件作为默认镜像
    * 以 @/ 开头的路径将优先从默认镜像中读取
    * 无论打开是否成功，之前打开的默认镜像都将被关闭
    * 若 path == nullptr，则仅关闭旧的镜像
*/
JE_API void jeecs_file_update_default_fimg(const char* path);

/*
jeecs_file_get_host_path [基本接口]
获取当前引擎的宿主环境路径
若没有使用 jeecs_file_set_host_path 设置此路径，则此路径默认为引擎的可
执行文件所在路径
请参见：
    jeecs_file_set_host_path
*/
JE_API const char* jeecs_file_get_host_path();

/*
jeecs_file_get_runtime_path [基本接口]
获取当前引擎的运行时路径，与“工作路径”无关
若没有使用 jeecs_file_set_runtime_path 设置此路径，则此路径默认为引擎的可
执行文件所在路径
请参见：
    jeecs_file_set_runtime_path
*/
JE_API const char* jeecs_file_get_runtime_path();

/*
jeecs_register_native_file_operator [基本接口]
设置引擎底层的文件读取接口
*/
JE_API void jeecs_register_native_file_operator(
    je_read_file_open_func_t opener,
    je_read_file_func_t reader,
    je_read_file_tell_func_t teller,
    je_read_file_seek_func_t seeker,
    je_read_file_close_func_t closer);

/*
jeecs_file_open [基本接口]
从指定路径打开一个文件，若打开失败返回nullptr
若文件成功打开，使用完毕后需要使用 jeecs_file_close 关闭
    * 若路径由 '@' 开头，则如同加载指定运行目录下的文件（优先加
    实际存在的文件）
    * 若路径由 '!' 开头，则如同加载可执行文件所在目录下的文件
请参见：
    jeecs_file_close
*/
JE_API jeecs_file* jeecs_file_open(const char* path);

/*
jeecs_file_close [基本接口]
关闭一个文件
*/
JE_API void jeecs_file_close(jeecs_file* file);

/*
jeecs_file_read [基本接口]
从文件中读取若干个指定大小的元素，返回成功读取的元素数量
*/
JE_API size_t jeecs_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    jeecs_file* file);

/*
jeecs_file_tell [基本接口]
获取当前文件下一个读取的位置偏移量
*/
JE_API size_t jeecs_file_tell(jeecs_file* file);

/*
jeecs_file_seek [基本接口]
按照指定的模式，对文件即将读取的位置进行偏移和跳转
请参见：
    je_read_file_seek_mode
*/
JE_API void jeecs_file_seek(jeecs_file* file, int64_t offset, je_read_file_seek_mode mode);

/*
jeecs_file_image_begin [基本接口]
准备开始创建文件镜像，镜像文件将被保存到 storing_path 指定的目录下
镜像会进行分卷操作，max_image_size是单个镜像文件的大小
*/
JE_API fimg_creating_context* jeecs_file_image_begin(
    const char* storing_path,
    size_t max_image_size);

/*
jeecs_file_image_pack_file [基本接口]
向指定镜像中写入由 filepath 指定的一个文件，此文件在镜像中的路径被
指定为packingpath
*/
JE_API bool jeecs_file_image_pack_file(
    fimg_creating_context* context,
    const char* filepath,
    const char* packingpath);

/*
jeecs_file_image_pack_file [基本接口]
向指定镜像中写入一个缓冲区指定的内容作为文件，此文件在镜像中的路径被
指定为packingpath
*/
JE_API bool jeecs_file_image_pack_buffer(
    fimg_creating_context* context,
    const void* buffer,
    size_t len,
    const char* packingpath);

/*
jeecs_file_image_finish [基本接口]
结束镜像创建，将最后剩余数据写入镜像并创建镜像索引文件
*/
JE_API void jeecs_file_image_finish(
    fimg_creating_context* context);

// If ignore_crc64 == true, cache will always work even if origin file changed.

/*
jeecs_load_cache_file [基本接口]
尝试读取 filepath 对应的缓存文件，将校验缓存文件的格式版本和校验码
缓存文件存在且校验通过则成功读取，否则返回nullptr
    * 若 virtual_crc64 被指定为 -1，则忽略检验原始文件一致性
    * 若 virtual_crc64 被指定为 0，则使用 filepath 指定文件的内容计算校验值
    * 其他情况，使用 virtual_crc64 直接指定校验值
    * 打开后的缓存文件需要使用 jeecs_file_close 关闭
    * 使用 jeecs_file_read 读取数据
请参见：
    jeecs_file_read
    jeecs_file_close
*/
JE_API jeecs_file* jeecs_load_cache_file(
    const char* filepath,
    uint32_t format_version,
    wo_integer_t virtual_crc64);

// If usecrc64 != 0, cache file will use it instead of reading from origin file.

/*
jeecs_create_cache_file [基本接口]
为 filepath 指定的文件创建缓存文件，将覆盖已有的缓存文件（如果已有的话）
若创建文件失败，则返回nullptr
    * 若 usecrc64 被指定为 0，则自动计算 filepath 指定文件的校验值
    * 否则直接使用 usecrc64 作为校验值
    * 打开后的缓存文件需要使用 jeecs_close_cache_file 关闭
    * 使用 jeecs_write_cache_file 写入数据
请参见：
    jeecs_close_cache_file
    jeecs_write_cache_file
*/
JE_API void* jeecs_create_cache_file(
    const char* filepath,
    uint32_t format_version,
    wo_integer_t usecrc64);

/*
jeecs_write_cache_file [基本接口]
向已创建的缓存文件中写入若干个指定大小的元素，返回成功写入的元素数量
*/
JE_API size_t jeecs_write_cache_file(
    const void* write_buffer,
    size_t elem_size,
    size_t count,
    void* file);

/*
jeecs_close_cache_file [基本接口]
关闭创建出的缓存文件
*/
JE_API void jeecs_close_cache_file(void* file);

/////////////////////////// GRAPHIC //////////////////////////////
// Here to store low-level-graphic-api.

/*
jegl_interface_config [类型]
图形接口的配置类型，创建图形线程时传入并生效
*/
struct jegl_interface_config
{
    enum display_mode
    {
        WINDOWED,
        FULLSCREEN,
        BOARDLESS,
    };

    // 显示模式，指示图形应该在全屏、窗口或无边框窗口中显示
    // * 在一些平台上无效
    display_mode m_display_mode;

    // 是否允许用户调整窗口大小
    // * 在一些平台上无效
    bool m_enable_resize;

    // 若MSAA值为0，则说明关闭超采样抗锯齿
    //  * MSAA配置应该是2的整数次幂
    //  * 最终能否使用取决于图形库
    size_t m_msaa;

    // 启动时的窗口尺寸，即绘制区域的大小，如果设置为0，则使用默认可用的绘制空间大小
    // * 如需全屏，通常设置为 0 即可。
    // * 在一些平台上无效
    size_t m_width;
    size_t m_height;

    // 限制帧数，若指定为0，则启用垂直同步
    // 不限制帧率请设置为 SIZE_MAX
    size_t m_fps;

    // 窗口标题
    const char* m_title;

    // 用户数据，针对一些特殊的平台（例如 Metal），需要通过此参数传递一些特定的参数
    // * 不同图形库可能以不同的方式使用此参数，请根据额外约定使用。
    void* m_userdata;
};

struct jegl_context_notifier;
struct jegl_graphic_api;
struct jegl_context_promise_t;

enum jegl_update_action
{
    JEGL_UPDATE_CONTINUE,
    JEGL_UPDATE_SKIP,
    JEGL_UPDATE_STOP,
};

/*
jegl_context [类型]
图形上下文，储存有当前图形线程的各项信息
*/
struct jegl_context
{
    using graphic_impl_context_t = void*;
    using frame_job_func_t =
        void (*)(jegl_context*, void*, jegl_update_action);

    frame_job_func_t _m_frame_rend_work;
    void* _m_frame_rend_work_arg;
    void* _m_sync_callback_arg;

    jegl_context_notifier* _m_thread_notifier;
    void* _m_interface_handle;

    void* m_universe_instance;
    jeecs::typing::version_t m_version;
    jegl_interface_config m_config;
    jegl_graphic_api* m_apis;
    graphic_impl_context_t m_graphic_impl_context;
};

/*
jegl_texture [类型]
纹理原始数据，储存有纹理的采样方式和像素数据等信息
*/
struct jegl_texture
{
    using pixel_data_t = uint8_t;
    enum format : uint16_t
    {
        MONO = 0x0001,
        RGBA = 0x0004,
        COLOR_DEPTH_MASK = 0x000F,

        FLOAT16 = 0x0010,
        DEPTH = 0x0020,
        FRAMEBUF = 0x0040,
        CUBE = 0x0080,

        FORMAT_MASK = 0xFFF0,
    };

    // NOTE:
    // * Pixel data is storage from LEFT/BOTTOM to RIGHT/TOP
    // * If texture's m_pixels is nullptr, only create a texture in pipeline.
    pixel_data_t* m_pixels;
    size_t m_width;
    size_t m_height;
    format m_format;

    // Partical texture data update
    size_t m_modified_min_x;
    size_t m_modified_min_y;
    size_t m_modified_max_x;
    size_t m_modified_max_y;
};

/*
jegl_vertex [类型]
顶点原始数据，储存有顶点的绘制方式、格式、大小和顶点数据等信息
*/
struct jegl_vertex
{
    enum type
    {
        LINESTRIP = 0,
        TRIANGLES,
        TRIANGLESTRIP,
    };
    enum data_type
    {
        INT32 = 0,
        FLOAT32,
    };

    struct data_layout
    {
        data_type m_type;
        size_t m_count;
    };

    struct bone_data
    {
        const char* m_name;
        size_t m_index;
        float m_m2b_trans[4][4];
    };

    float m_x_min, m_x_max,
        m_y_min, m_y_max,
        m_z_min, m_z_max;

    const void* m_vertexs;
    size_t m_vertex_length;
    const uint32_t* m_indexs;
    size_t m_index_count;
    const data_layout* m_formats;
    size_t m_format_count;
    size_t m_data_size_per_point;
    type m_type;
    const bone_data** m_bones;
    size_t m_bone_count;
};

/*
jegl_shader [类型]
着色器原始数据，储存有着色器源码、绘制参数和一致变量等信息
*/
struct jegl_shader
{
    enum fliter_mode
    {
        NEAREST,
        LINEAR,
    };
    enum wrap_mode
    {
        CLAMP,
        REPEAT,
    };
    struct sampler_method
    {
        fliter_mode m_min;
        fliter_mode m_mag;
        fliter_mode m_mip;
        wrap_mode m_uwrap;
        wrap_mode m_vwrap;
        uint32_t m_sampler_id; // Used for DX11 & HLSL generation
        uint64_t m_pass_id_count;
        uint32_t* m_pass_ids; // Used for GL3 & GLSL generation
    };

    enum uniform_type
    {
        INT,
        INT2,
        INT3,
        INT4,
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        TEXTURE,
        FLOAT2X2,
        FLOAT3X3,
        FLOAT4X4,
    };
    struct builtin_uniform_location
    {
        // NOTE: 不要写入这个位置，ndc_scale 是为了纠正渲染到纹理的 UV 映射关系用的
        //  仅由底层的图形库实现负责写入；改变和翻转 ndc 需要底层图形库配合正面旋向
        //  相关设置才能保证渲染结果正确
        uint32_t m_builtin_uniform_ndc_scale = jeecs::typing::PENDING_UNIFORM_LOCATION;

        uint32_t m_builtin_uniform_m = jeecs::typing::PENDING_UNIFORM_LOCATION;
        uint32_t m_builtin_uniform_mv = jeecs::typing::PENDING_UNIFORM_LOCATION;
        uint32_t m_builtin_uniform_mvp = jeecs::typing::PENDING_UNIFORM_LOCATION;

        uint32_t m_builtin_uniform_local_scale = jeecs::typing::PENDING_UNIFORM_LOCATION;

        uint32_t m_builtin_uniform_tiling = jeecs::typing::PENDING_UNIFORM_LOCATION;
        uint32_t m_builtin_uniform_offset = jeecs::typing::PENDING_UNIFORM_LOCATION;

        uint32_t m_builtin_uniform_color = jeecs::typing::PENDING_UNIFORM_LOCATION;

        uint32_t m_builtin_uniform_light2d_resolution = jeecs::typing::PENDING_UNIFORM_LOCATION;
        uint32_t m_builtin_uniform_light2d_decay = jeecs::typing::PENDING_UNIFORM_LOCATION;
    };

    struct unifrom_variables
    {
        const char* m_name;
        uint32_t m_index;
        uniform_type m_uniform_type;
        bool m_updated;
        union value
        {
            struct
            {
                float x, y, z, w;
            };
            struct
            {
                int ix, iy, iz, iw;
            };
            float mat2x2[2][2];
            float mat3x3[3][3];
            float mat4x4[4][4];
        };

        unifrom_variables* m_next;
        value m_value;
    };
    struct uniform_blocks
    {
        const char* m_name;
        uint32_t m_specify_binding_place;

        uniform_blocks* m_next;
    };

    enum class depth_test_method : int8_t
    {
        INVALID = -1,

        OFF,
        NEVER,
        LESS, /* DEFAULT */
        EQUAL,
        LESS_EQUAL,
        GREATER,
        NOT_EQUAL,
        GREATER_EQUAL,
        ALWAYS,
    };
    enum class depth_mask_method : int8_t
    {
        INVALID = -1,

        DISABLE,
        ENABLE, /* DEFAULT */
    };
    enum class blend_equation : int8_t
    {
        INVALID = -1,

        ADD, /* DEFAULT */
        SUBTRACT,
        REVERSE_SUBTRACT,
        MIN,
        MAX,
    };
    enum class blend_method : int8_t
    {
        INVALID = -1,

        ZERO, /* DEFAULT SRC = ONE, DST = ZERO (DISABLE BLEND.) */
        ONE,

        SRC_COLOR,
        SRC_ALPHA,

        ONE_MINUS_SRC_ALPHA,
        ONE_MINUS_SRC_COLOR,

        DST_COLOR,
        DST_ALPHA,

        ONE_MINUS_DST_ALPHA,
        ONE_MINUS_DST_COLOR,

        // CONST_COLOR,
        // ONE_MINUS_CONST_COLOR,

        // CONST_ALPHA,
        // ONE_MINUS_CONST_ALPHA,
    };
    enum class cull_mode : int8_t
    {
        INVALID = -1,

        NONE, /* DEFAULT */
        FRONT,
        BACK,
    };

    using spir_v_code_t = uint32_t;

    const char* m_vertex_hlsl_src;
    const char* m_fragment_hlsl_src;
    const char* m_vertex_glsl_src;
    const char* m_fragment_glsl_src;
    const char* m_vertex_glsles_src;
    const char* m_fragment_glsles_src;
    const char* m_vertex_msl_mac_src;
    const char* m_fragment_msl_mac_src;

    size_t m_vertex_spirv_count;
    const spir_v_code_t* m_vertex_spirv_codes;

    size_t m_fragment_spirv_count;
    const spir_v_code_t* m_fragment_spirv_codes;

    size_t m_vertex_in_count;
    uniform_type* m_vertex_in;

    size_t m_fragment_out_count;
    uniform_type* m_fragment_out;

    unifrom_variables* m_custom_uniforms;
    uniform_blocks* m_custom_uniform_blocks;
    builtin_uniform_location m_builtin_uniforms;

    bool m_enable_to_shared;
    depth_test_method m_depth_test;
    depth_mask_method m_depth_mask;
    blend_equation m_blend_equation;
    blend_method m_blend_src_mode, m_blend_dst_mode;
    cull_mode m_cull_mode;

    sampler_method* m_sampler_methods;
    size_t m_sampler_count;
};

/*
jegl_frame_buffer [类型]
帧缓冲区原始数据，储存有帧大小、附件等信息
*/
struct jegl_frame_buffer
{
    // In fact, attachment_t is jeecs::basic::resource<jeecs::graphic::texture>
    typedef struct attachment_t attachment_t;
    attachment_t* m_output_attachments;
    size_t m_attachment_count;
    size_t m_width;
    size_t m_height;
};

/*
jegl_uniform_buffer [类型]
一致变量缓冲区原始数据，储存有一致变量的数据和容量等信息
*/
struct jegl_uniform_buffer
{
    size_t m_buffer_binding_place;
    size_t m_buffer_size;
    uint8_t* m_buffer;

    // Used for marking update range;
    size_t m_update_begin_offset;
    size_t m_update_length;
};

using jegl_resource_blob = void*;
struct jegl_resource_bind_counter;

/*
jegl_resource [类型]
图形资源初级封装，纹理、着色器、帧缓冲区等均为图形资源
*/
struct jegl_resource
{
    using jegl_custom_resource_t = void*;
    enum type : uint8_t
    {
        VERTEX,     // Mesh
        TEXTURE,    // Texture
        SHADER,     // Shader
        FRAMEBUF,   // Framebuffer
        UNIFORMBUF, // UniformBlock
    };
    union resource_handle
    {
        void* m_ptr;
        size_t m_hdl;
        struct
        {
            uint32_t m_uint1;
            uint32_t m_uint2;
        };
    };

    jegl_resource_bind_counter* m_raw_ref_count;

    jegl_context* m_graphic_thread;
    jeecs::typing::version_t m_graphic_thread_version;
    resource_handle m_handle;

    type m_type;
    bool m_modified;

    const char* m_path;
    union
    {
        jegl_custom_resource_t m_custom_resource;
        jegl_texture* m_raw_texture_data;
        jegl_vertex* m_raw_vertex_data;
        jegl_shader* m_raw_shader_data;
        jegl_frame_buffer* m_raw_framebuf_data;
        jegl_uniform_buffer* m_raw_uniformbuf_data;
    };
};

/*
jegl_graphic_api [类型]
图形接口，包含底层使用的各项基本函数
*/
struct jegl_graphic_api
{
    using startup_func_t = jegl_context::graphic_impl_context_t(*)(jegl_context*, const jegl_interface_config*, bool);
    using shutdown_func_t = void (*)(jegl_context*, jegl_context::graphic_impl_context_t, bool);

    using update_func_t = jegl_update_action(*)(jegl_context::graphic_impl_context_t);
    using commit_func_t = jegl_update_action(*)(jegl_context::graphic_impl_context_t, jegl_update_action);

    using create_blob_func_t = jegl_resource_blob(*)(jegl_context::graphic_impl_context_t, jegl_resource*);
    using close_blob_func_t = void (*)(jegl_context::graphic_impl_context_t, jegl_resource_blob);

    using create_resource_func_t = void (*)(jegl_context::graphic_impl_context_t, jegl_resource_blob, jegl_resource*);
    using using_resource_func_t = void (*)(jegl_context::graphic_impl_context_t, jegl_resource*);
    using close_resource_func_t = void (*)(jegl_context::graphic_impl_context_t, jegl_resource*);

    using bind_ubuffer_func_t = void (*)(jegl_context::graphic_impl_context_t, jegl_resource*);
    using bind_shader_func_t = bool (*)(jegl_context::graphic_impl_context_t, jegl_resource*);
    using bind_texture_func_t = void (*)(jegl_context::graphic_impl_context_t, jegl_resource*, size_t);
    using draw_vertex_func_t = void (*)(jegl_context::graphic_impl_context_t, jegl_resource*);

    using bind_framebuf_func_t = void (*)(jegl_context::graphic_impl_context_t, jegl_resource*, size_t, size_t, size_t, size_t);
    using clear_color_func_t = void (*)(jegl_context::graphic_impl_context_t, float[4]);
    using clear_depth_func_t = void (*)(jegl_context::graphic_impl_context_t);
    using set_uniform_func_t = void (*)(jegl_context::graphic_impl_context_t, uint32_t, jegl_shader::uniform_type, const void*);

    /*
    jegl_graphic_api::interface_startup [成员]
    图形接口在初始化或者被请求重新启动时调用，按照JoyEngine的约定，图形实现应当在此处创建窗口，并完成渲染所需
    的初始化工作。
        * 请求重新启动通常是因为需要变更图形设置、例如MSAA设置或窗口标题、帧率等，因此建议图形实现在处理重新启动
            时，应当保留可以保留的资源以节约开销；若不提供对此的优化支持，也可以直接忽略，按照默认的启停规则进行操作。
        * 当图形上下文被请求重新启动时，会再次调用此函数，此时第3个参数将被设置为true，否则为 false 。
    */
    startup_func_t interface_startup;

    /*
    jegl_graphic_api::interface_shutdown_before_resource_release [成员]
    图形接口在被关闭或请求重新启动时调用，按照JoyEngine的约定，所有申请而暂未释放的图形资源将在调用此函数完成之后
    执行释放，然后调用 interface_shutdown；因此图形实现应当在此做好释放图形资源的准备，例如让渲染设备进入暂停状态。
        * 请求重新启动通常是因为需要变更图形设置、例如MSAA设置或窗口标题、帧率等，因此建议图形实现在处理重新启动
            时，应当保留可以保留的资源以节约开销；若不提供对此的优化支持，也可以直接忽略，按照默认的启停规则进行操作。
        * 当图形上下文被请求重新启动时，第3个参数将被设置为true，否则为 false。
    请参见：
        jegl_graphic_api::interface_post_shutdown
    */
    shutdown_func_t interface_shutdown_before_resource_release;

    /*
    jegl_graphic_api::interface_shutdown [成员]
    图形接口在被关闭或请求重新启动时调用，按照JoyEngine的约定，图形接口应当在此函数中完成所有资源、设备和窗口的释放。
        * 此接口将在图形接口被请求关闭之后，在完成调用interface_shutdown_before_resource_release接口、进而释放全部
            未释放的图形资源，最后调用interface_shutdown。
        * 请求重新启动通常是因为需要变更图形设置、例如MSAA设置或窗口标题、帧率等，因此建议图形实现在处理重新启动
            时，应当保留可以保留的资源以节约开销；若不提供对此的优化支持，也可以直接忽略，按照默认的启停规则进行操作。
        * 当图形上下文被请求重新启动时，第3个参数将被设置为true，否则为 false。
    请参见：
        jegl_graphic_api::interface_pre_shutdown
    */
    shutdown_func_t interface_shutdown;

    /*
    jegl_graphic_api::update_frame_ready [成员]
    图形接口在更新一帧画面的起始位置会调用的接口，图形实现应当在此接口的开头完成前一帧画面的交换以呈现在屏幕上（这样可
    以获得最佳的画面流畅度），然后做好其他准备：正式的主要渲染任务会在此接口返回后开始。
        * 接口若返回 jegl_update_action::STOP，则表示图形实现请求关闭渲染
            创建图形线程时指示的绘制任务将被跳过，在帧同步工作完成后进入图形线程的退出流程。
        * 接口若返回 jegl_update_action::SKIP，则表示图形实现请求跳过本帧的渲染，
            通常这种情况是由于窗口被切换至后台、最小化等情况导致的，
            创建图形线程时指示的绘制任务将被跳过，但 **不会** 进入图形线程的退出流程。
        无论如何，update_draw_commit 都会被执行（目的是能正确执行 gui 的逻辑），update_draw_commit将通过参数
        获知渲染任务是否被执行
    */
    update_func_t update_frame_ready;

    /*
    jegl_graphic_api::update_draw_commit [成员]
    图形接口在完成指示的渲染操作之后会调用的接口，图形实现应当在此接口中，将既存的提交任务提交到图形设备中（以最大化利用
    设备空闲），如果可以，渲染GUI的任务也应当在此处实现，并一并提交。
        * 接口若返回 jegl_update_action::STOP，则表示图形实现请求关闭渲染，在帧同步工作完成后进入图形线程的退出流程。
        * 接口若返回 jegl_update_action::SKIP，由于并没有后续的渲染任务可被跳过，因此事实上如同返回 jegl_update_action::CONTINUE。
    */
    commit_func_t update_draw_commit;

    /*
    jegl_graphic_api::create_resource_blob_cache [成员]
    图形接口在创建资源之前会调用此接口以生成运行时缓存。该缓存将被传入create_resource用于加速资源的创建。
        * 一个常见的用途是，在创建着色器之前，先根据着色器的原始资源创建出预备的缓存，然后在create_resource中使用此缓存
            实例化真正的着色器实例。
        * 若确实没有值得缓存的数据，可以返回nullptr，如果这么做，close_resource_blob_cache和create_resource时也将收到nullptr。
        * 图形实现应当为缓存信息预备能够指示类型信息的字段，以便于close_resource_blob_cache时可以用正确的方法释放缓存。
    请参见：
        jegl_graphic_api::close_resource_blob_cache
        jegl_graphic_api::create_resource
    */
    create_blob_func_t create_resource_blob_cache;

    /*
    jegl_graphic_api::close_resource_blob_cache [成员]
    释放一个图形实现的缓存，这通常是因为图形线程被请求关闭，或者引擎认为该缓存已经过时。
        * 图形实现应当检查缓存是否为nullptr，以及缓存的类型，然后再释放缓存。
    */
    close_blob_func_t close_resource_blob_cache;

    /*
    jegl_graphic_api::create_resource [成员]
    创建一个图形资源，图形实现应当检查资源的类型，通过类型实例中提供的原始数据以初始化创建图形资源，并将资源句柄保存到实例
    的m_handle字段中。
    */
    create_resource_func_t create_resource;

    /*
    jegl_graphic_api::using_resource [成员]
    在正式使用一个图形资源之前，会调用此接口对资源进行更新、预备工作；此接口常用于更新纹理、一致缓冲区数据
        * 具体的操作可以是是图形资源实现的
        * 在使用资源的原始数据部分时，请检查原始数据字段是否置空；一些情况下图像任务仍然会使用已经被请求释放的图形资源（
        这通常是因为相关的绘制操作已经被“录制”），这种图形资源的原始数据已经被销毁并置空；不过JoyEngine保证使用的图形资
        源本身尚未被close_resource关闭。
    请参见：
        jegl_graphic_api::close_resource
    */
    using_resource_func_t using_resource;

    /*
    jegl_graphic_api::close_resource [成员]
    关闭一个图形资源，图形实现应当检查资源的类型，通过类型实例中提供的原始数据以释放图形资源。
    */
    close_resource_func_t close_resource;

    /*
    jegl_graphic_api::bind_uniform_buffer [成员]
    绑定一个一致缓冲区到对应位置
        * 约定：由于RendChain的延迟渲染特性，接口假定所有相同的 uniform_buffer 实例在一帧之内
            不会发生数据改动。
    */
    bind_ubuffer_func_t bind_uniform_buffer;

    /*
    jegl_graphic_api::bind_texture [成员]
    绑定一个纹理到对应的通道位置。
    */
    bind_texture_func_t bind_texture;

    /*
    jegl_graphic_api::bind_shader [成员]
    绑定一个着色器作为当前渲染使用的着色器。
    */
    bind_shader_func_t bind_shader;

    /*
    jegl_graphic_api::bind_framebuf [成员]
    设置渲染目标，若传入nullptr，则目标为屏幕空间。
    */
    bind_framebuf_func_t bind_framebuf;

    /*
    jegl_graphic_api::draw_vertex [成员]
    使用之前绑定的着色器和纹理，绘制给定的顶点模型。
    */
    draw_vertex_func_t draw_vertex;

    /*
    jegl_graphic_api::clear_frame_color [成员]
    以指定颜色清除渲染目标的所有颜色附件。
    */
    clear_color_func_t clear_frame_color;

    /*
    jegl_graphic_api::clear_frame_depth [成员]
    以`无穷远`清空渲染目标的深度附件。
    */
    clear_depth_func_t clear_frame_depth;

    /*
    jegl_graphic_api::set_uniform [成员]
    向当前正在绑定的着色器设置一致变量。
    */
    set_uniform_func_t set_uniform;
};
static_assert(sizeof(jegl_graphic_api) % sizeof(void*) == 0);

using jeecs_api_register_func_t = void (*)(jegl_graphic_api*);
using jeecs_sync_callback_func_t = void (*)(jegl_context*, void*);

/*
jegl_register_sync_graphic_callback [基本接口]
一些平台下，由于上下文等限制，不能创建独立的渲染线程，
对于这种情况，可以使用此函数注册一个回调函数，在本应
创建图形线程的地方调用此回调，用于获取对应的universe
和图形上下文，之后则需要手动调用同步图形更新接口。
    * 必须在 je_init 之后注册，引擎初始化会重置回调
请参见：
    je_init
*/
JE_API void jegl_register_sync_thread_callback(
    jeecs_sync_callback_func_t callback,
    void* arg);

enum jegl_sync_state
{
    JEGL_SYNC_COMPLETE,
    JEGL_SYNC_SHUTDOWN,
    JEGL_SYNC_REBOOT,
};

/*
jegl_sync_init [基本接口]
一些平台下，由于上下文等限制，不能创建独立的渲染线程，
对于这种情况，可以使用jegl_register_sync_graphic_callback
注册回调函数，并手动执行 jegl_sync_init、jegl_sync_update
和 jegl_sync_shutdown。
    * jegl_sync_init 用于图形在首次启动或重启时执行图形
        任务的初始化操作，初始化完成后，则循环执行
        jegl_sync_update 直到其返回 JEGL_SYNC_SHUTDOWN
        或 JEGL_SYNC_REBOOT 之前，都不需要重新初始化
请参见：
    jegl_register_sync_graphic_callback
    jegl_sync_update
    jegl_sync_shutdown
*/
JE_API void jegl_sync_init(
    jegl_context* thread,
    bool isreboot);

/*
jegl_sync_update [基本接口]
一些平台下，由于上下文等限制，不能创建独立的渲染线程，
对于这种情况，可以使用jegl_register_sync_graphic_callback
注册回调函数，并手动执行 jegl_sync_init、jegl_sync_update
和 jegl_sync_shutdown。
    * jegl_sync_update 用于更新图形图形的一帧
    * 函数返回 JEGL_SYNC_COMPLETE 表示正常结束
    * 返回 JEGL_SYNC_SHUTDOWN 表示绘制将结束，请以 isreboot = false
        调用 jegl_sync_shutdown，结束全部绘制工作
    * 返回 JEGL_SYNC_REBOOT 表示绘制需要重新初始化，请以
        isreboot = true 调用 jegl_sync_shutdown 释放当前
        上下文，然后以 isreboot = true 调用jegl_sync_init
        重新执行初始化
请参见：
    jegl_register_sync_graphic_callback
    jegl_sync_init
    jegl_sync_shutdown
*/
JE_API jegl_sync_state jegl_sync_update(jegl_context* thread);

/*
jegl_sync_shutdown [基本接口]
一些平台下，由于上下文等限制，不能创建独立的渲染线程，
对于这种情况，可以使用jegl_register_sync_graphic_callback
注册回调函数，并手动执行 jegl_sync_init、jegl_sync_update
和 jegl_sync_shutdown。
    * jegl_sync_shutdown 用于释放上下文，根据jegl_sync_update
        的返回值，以适当的参数调用此函数以正确完成渲染逻辑
    * 返回值事实上只与 isreboot 有关：返回 true 表示确实退出，
        返回 false 表示仍需重新初始化
请参见：
    jegl_register_sync_graphic_callback
    jegl_sync_init
    jegl_sync_shutdown
*/
JE_API bool jegl_sync_shutdown(jegl_context* thread, bool isreboot);

/*
jegl_start_graphic_thread [基本接口]
创建图形线程
指定配置、图形库接口和帧更新函数创建一个图形绘制线程。
图形线程的帧更新操作 jegl_update 将调用指定的帧更新函数，
实际的绘制任务应该放在帧更新函数中
创建出来的图形线程需要使用jegl_terminate_graphic_thread释放
请参见：
    jegl_update
    jegl_terminate_graphic_thread
*/
JE_API jegl_context* jegl_start_graphic_thread(
    jegl_interface_config config,
    void* universe_instance,
    jeecs_api_register_func_t register_func,
    jegl_context::frame_job_func_t frame_rend_work,
    void* arg);

/*
jegl_terminate_graphic_thread [基本接口]
终止图形线程，将会阻塞直到图形线程完全退出
创建图形线程之后，无论图形绘制工作是否终止，都需要使用此接口释放图形线程
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_terminate_graphic_thread(jegl_context* thread_handle);

enum jegl_update_sync_mode
{
    JEGL_WAIT_THIS_FRAME_END,
    JEGL_WAIT_LAST_FRAME_END,
};

typedef void (*jegl_update_sync_callback_t)(void*);

/*
jegl_update [基本接口]
调度图形线程更新一帧，此函数将阻塞直到一帧的绘制操作完成
    * mode 用于指示更新的同步模式，其中：
        WAIT_THIS_FRAME_END：阻塞直到当前帧渲染完毕
        * 同步更简单，但是绘制时间无法和其他逻辑同步执行
        WAIT_LAST_FRAME_END：阻塞直到上一帧渲染完毕（绘制信号发出后立即返回）
        * 更适合CPU密集操作，但需要更复杂的机制以保证数据完整性
*/
JE_API bool jegl_update(
    jegl_context* thread_handle,
    jegl_update_sync_mode mode,
    jegl_update_sync_callback_t callback_after_wait_may_null,
    void* callback_param);

/*
jegl_reboot_graphic_thread [基本接口]
以指定的配置重启一个图形线程
    * 若不需要改变图形配置，请使用nullptr
*/
JE_API void jegl_reboot_graphic_thread(
    jegl_context* thread_handle,
    const jegl_interface_config* config);

/*
jegl_load_texture [基本接口]
从指定路径加载一个纹理资源，加载的路径规则与 jeecs_file_open 相同
    * 若指定的文件不存在或不是一个合法的纹理，则返回nullptr
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
请参见：
    jeecs_file_open
    jegl_close_resource
*/
JE_API jegl_resource* jegl_load_texture(
    jegl_context* context,
    const char* path);

/*
jegl_create_texture [基本接口]
创建一个指定大小和格式的纹理资源
    * 若指定的格式包含 COLOR16、DEPTH、FRAMEBUF、CUBE 或有 MSAA 支持，则不创建像素缓冲，
        对应纹理原始数据的像素将被设置为 nullptr
    * 若创建像素缓冲，像素缓存的将按字节置为 0 填充初始化
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
    * 考虑到部分图形库的实现，如果指定的宽度或高度为 0，jegl_create_texture将视其为 1
请参见：
    jegl_close_resource
*/
JE_API jegl_resource* /* NOT NULL */ jegl_create_texture(
    size_t width,
    size_t height,
    jegl_texture::format format);

/*
jegl_load_vertex [基本接口]
从指定路径加载一个顶点（模型）资源，加载的路径规则与 jeecs_file_open 相同
    * 若指定的文件不存在或不是一个合法的模型，则返回nullptr
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
    * 使用此方法加载的模型，始终按照三角面排布，其顶点数据格式如下所示：
        顶点坐标        3 * f32
        UV映射坐标      2 * f32
        法线方向        3 * f32
        骨骼索引        4 * i32
        骨骼权重        4 * f32
    * 对于绑定骨骼数量小于4的顶点，保证空余位置的骨骼索引被 0 填充，权重为 0.f
请参见：
    jeecs_file_open
    jegl_close_resource
*/
JE_API jegl_resource* jegl_load_vertex(
    jegl_context* context,
    const char* path);

/*
jegl_create_vertex [基本接口]
用指定的顶点数据创建一个顶点（模型）资源
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
请参见：
    jegl_close_resource
*/
JE_API jegl_resource* jegl_create_vertex(
    jegl_vertex::type type,
    const void* datas,
    size_t data_length,
    const uint32_t* indexs,
    size_t index_count,
    const jegl_vertex::data_layout* format,
    size_t format_count);

/*
jegl_create_framebuf [基本接口]
使用指定的附件配置创建一个纹理缓冲区资源
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
请参见：
    jegl_close_resource
*/
JE_API jegl_resource* jegl_create_framebuf(
    size_t width,
    size_t height,
    const jegl_texture::format* attachment_formats,
    size_t attachment_count);

struct je_stb_font_data;
typedef void (*je_font_char_updater_t)(jegl_texture::pixel_data_t*, size_t, size_t);

struct je_font
{
    const char* m_path;
    uint8_t* m_font_file_buf;

    float m_scale_x;
    float m_scale_y;
    size_t m_board_size_x;
    size_t m_board_size_y;
    je_font_char_updater_t m_updater;

    int32_t m_ascent;
    int32_t m_descent;
    int32_t m_line_gap;
    int32_t m_line_space;

    float m_x_scale_for_pix;
    float m_y_scale_for_pix;

    je_stb_font_data* m_stb_font_data;
};

/*
je_font_load [基本接口]
加载一个字体
scalex和scaley 分别是此字体的横线和纵向字号（单位像素）
samp用于指示此字体创建的文字纹理的采样方式
board_blank_size_x、board_blank_size_y用于指示文字纹理的横线和纵向预留空间
char_texture_updater 用于指示文字纹理创建后所需的预处理方法，不需要可以指定为nullptr
使用完毕之后的字体需要使用je_font_free关闭
请参见：
    je_font_free
*/
JE_API je_font* je_font_load(
    const char* font_path,
    float scalex,
    float scaley,
    size_t board_blank_size_x,
    size_t board_blank_size_y,
    je_font_char_updater_t char_texture_updater);

/*
je_font_free [基本接口]
关闭一个字体
*/
JE_API void je_font_free(je_font* font);

/*
je_font_get_char [基本接口]
从字体中加载指定的一个字符的纹理及其他信息
请参见：
    jeecs::graphic::character
*/
JE_API const jeecs::graphic::character* je_font_get_char(je_font* font, char32_t unicode32_char);

/*
jegl_mark_shared_resources_outdated [基本接口]
将指定路径对应的共享资源缓存标记为过时的，若成功标记，返回true
    * 标记成功：指的是之前此资源存在，并非过时的，在此次操作之后被标记
    * 这将使得下次加载此资源时更新资源，对于已经加载的资源没有影响
请参见：
    jegl_mark_shared_resources_outdated
*/
JE_API bool jegl_mark_shared_resources_outdated(const char* path);

/*
jegl_shrink_shared_resource_cache [基本接口]
缩减共享资源缓存，将缓存中的资源数量缩减到指定的数量
    * 缩减的资源将会被标记为过时并释放，下次加载此资源时将重新加载。
    * 若缓存中的资源数量小于指定的数量，则不会进行任何操作。
*/
JE_API void jegl_shrink_shared_resource_cache(
    jegl_context* context, size_t shrink_target_count);

/*
jegl_load_shader_source [基本接口]
从源码加载一个着色器实例，可创建或使用缓存文件以加速着色器的加载
    * 实际上jegl_load_shader会读取文件内容之后，调用此函数进行实际上的着色器加载
若不需要创建缓存文件，请将 is_virtual_file 指定为 false
请参见：
    jegl_load_shader
*/
JE_API jegl_resource* jegl_load_shader_source(
    const char* path,
    const char* src,
    bool is_virtual_file);

/*
jegl_load_shader [基本接口]
从源码文件加载一个着色器实例，会创建或使用缓存文件以加速着色器的加载
*/
JE_API jegl_resource* jegl_load_shader(
    jegl_context* context,
    const char* path);

/*
jegl_create_uniformbuf [基本接口]
创建一个指定大小和绑定位置的一致变量缓冲区资源
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
请参见：
    jegl_close_resource
*/
JE_API jegl_resource* jegl_create_uniformbuf(
    size_t binding_place,
    size_t length);

/*
jegl_update_uniformbuf [基本接口]
更新一个一致变量缓冲区中，指定位置起，若干长度的数据
*/
JE_API void jegl_update_uniformbuf(
    jegl_resource* uniformbuf,
    const void* buf,
    size_t update_offset,
    size_t update_length);

/*
jegl_graphic_api_entry [类型]
基础图形库的入口类型
指向一个用于初始化基础图形接口的入口函数
*/
typedef void (*jegl_graphic_api_entry)(jegl_graphic_api*);

/*
jegl_set_host_graphic_api [基本接口]
设置使用的图形库
    * 在图形线程启动前或结束后设置生效
    * 若不设置或设置为 nullptr，引擎将使用默认的图形接口、
    * 请在je_init之后设置，引擎的初始化操作将重置此设置
*/
JE_API void jegl_set_host_graphic_api(jegl_graphic_api_entry api);

/*
jegl_get_host_graphic_api [基本接口]
获取使用的图形库
    * 若尚未使用 jegl_set_host_graphic_api 设置图形库接口，
        则返回引擎根据平台决定使用的默认使用的图形库
    * 否则返回 jegl_set_host_graphic_api 设置的接口
*/
JE_API jegl_graphic_api_entry jegl_get_host_graphic_api(void);

/*
jegl_using_none_apis [基本接口]
加载JoyEngine基础图形接口的空实现，通常与jegl_start_graphic_thread一起使用用于指定图形线程使
用的基本图形库。
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_none_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_opengl3_apis [基本接口]
加载OpenGL 3.3 或 OpenGLES 3.0 API集合，通常与jegl_start_graphic_thread一起使用以指定图形线程
使用的基本图形实现。
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_opengl3_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_vulkan130_apis [基本接口] (暂未实现)
加载Vulkan API v1.3集合，通常与jegl_start_graphic_thread一起使用以指定图形线程使用的基本图形
实现。
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_vk130_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_metal_apis [基本接口] (暂未实现)
加载Metal API集合，通常与jegl_start_graphic_thread一起使用以指定图形线程使用的基本图形
实现。
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_metal_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_dx11_apis [基本接口]
加载directx 11 API集合，通常与jegl_start_graphic_thread一起使用以指定图形线程使用的基本图形
实现。
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_resource [基本接口]
当图形资源即将被使用时，此接口被调用用于创建/更新资源。
    * 通常不需要手动调用，通常用于在图形线程的外部功能需要初始化资源时调用（例如GUI）
    * 函数返回true表示此资源在本次调用期间完成初始化
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
*/
JE_API bool jegl_using_resource(jegl_resource* resource);

/*
jegl_share_resource [基本接口]
对资源的占有，通常用于避免正在使用的图形资源被其他持有者释放
    * 占有的资源应当在使用完毕后，使用 jegl_close_resource 释放
参见：
    jegl_close_resource
*/
JE_API void jegl_share_resource(jegl_resource* resource);

/*
jegl_close_resource [基本接口]
关闭指定的图形资源，图形资源的原始数据信息会被立即回收，对应图形库的实际资源会在
对应的图形线程中延迟销毁
*/
JE_API void jegl_close_resource(jegl_resource* resource);

/*
jegl_bind_texture [基本接口]
将指定的纹理绑定到指定的纹理通道
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
*/
JE_API void jegl_bind_texture(jegl_resource* texture, size_t pass);

/*
jegl_bind_shader [基本接口]
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
    * 当着色器发生内部错误（通常是引擎生成的shader代码无法被图形库正常编译）时，
        绑定失败，返回false
*/
JE_API bool jegl_bind_shader(jegl_resource* shader);

/*
jegl_bind_uniform_buffer [基本接口]
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
*/
JE_API void jegl_bind_uniform_buffer(jegl_resource* uniformbuf);

/*
jegl_draw_vertex [基本接口]
使用当前着色器（通过jegl_bind_shader绑定）和纹理（通过jegl_bind_texture绑定）,
以指定方式绘制一个模型
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
请参见：
    jegl_bind_shader
    jegl_bind_texture
*/
JE_API void jegl_draw_vertex(jegl_resource* vert);

/*
jegl_clear_framebuffer_color [基本接口]
以color指定的颜色清除当前帧缓冲的颜色信息
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
*/
JE_API void jegl_clear_framebuffer_color(float color[4]);

/*
jegl_clear_framebuffer [基本接口]
清除指定帧缓冲的深度信息
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
*/
JE_API void jegl_clear_framebuffer_depth();

/*
jegl_rend_to_framebuffer [基本接口]
指定接下来的绘制操作作用于指定缓冲区，xywh用于指定绘制剪裁区域的左下角位置和区域大小
若 framebuffer == nullptr 则绘制目标缓冲区设置为屏幕缓冲区
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
*/
JE_API void jegl_rend_to_framebuffer(jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h);

/*
jegl_uniform_int [基本接口]
向当前着色器指定位置的一致变量设置一个整型数值
jegl_uniform_int 不会初始化着色器，请在操作之前调用 jegl_bind_shader
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_bind_shader
*/
JE_API void jegl_uniform_int(uint32_t location, int value);

/*
jegl_uniform_int2 [基本接口]
向当前着色器指定位置的一致变量设置一个二维整型矢量数值
jegl_uniform_int2 不会初始化着色器，请在操作之前调用 jegl_bind_shader
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
*/
JE_API void jegl_uniform_int2(uint32_t location, int x, int y);

/*
jegl_uniform_int3 [基本接口]
向当前着色器指定位置的一致变量设置一个三维整型矢量数值
jegl_uniform_int3 不会初始化着色器，请在操作之前调用 jegl_bind_shader
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
*/
JE_API void jegl_uniform_int3(uint32_t location, int x, int y, int z);

/*
jegl_uniform_int4 [基本接口]
向当前着色器指定位置的一致变量设置一个四维整型矢量数值
jegl_uniform_int4 不会初始化着色器，请在操作之前调用 jegl_bind_shader
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
*/
JE_API void jegl_uniform_int4(uint32_t location, int x, int y, int z, int w);

/*
jegl_uniform_float [基本接口]
向当前着色器指定位置的一致变量设置一个单精度浮点数值
    * jegl_uniform_float 不会初始化着色器，请在操作之前调用 jegl_bind_shader
        以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_bind_shader
*/
JE_API void jegl_uniform_float(uint32_t location, float value);

/*
jegl_uniform_float2 [基本接口]
向当前着色器指定位置的一致变量设置一个二维单精度浮点矢量数值
    * jegl_uniform_float2 不会初始化着色器，请在操作之前调用 jegl_bind_shader
        以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_bind_shader
*/
JE_API void jegl_uniform_float2(uint32_t location, float x, float y);

/*
jegl_uniform_float3 [基本接口]
向当前着色器指定位置的一致变量设置一个三维单精度浮点矢量数值
    * jegl_uniform_float3 不会初始化着色器，请在操作之前调用 jegl_bind_shader
        以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_bind_shader
*/
JE_API void jegl_uniform_float3(uint32_t location, float x, float y, float z);

/*
jegl_uniform_float4 [基本接口]
向当前着色器指定位置的一致变量设置一个四维单精度浮点矢量数值
    * jegl_uniform_float4 不会初始化着色器，请在操作之前调用 jegl_bind_shader
        以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_bind_shader
*/
JE_API void jegl_uniform_float4(uint32_t location, float x, float y, float z, float w);

/*
jegl_uniform_float2x2 [基本接口]
向当前着色器指定位置的一致变量设置一个2x2单精度浮点矩阵数值
    * jegl_uniform_float2x2 不会初始化着色器，请在操作之前调用 jegl_bind_shader
        以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_bind_shader
*/
JE_API void jegl_uniform_float2x2(uint32_t location, const float (*mat)[2]);

/*
jegl_uniform_float3x3 [基本接口]
向当前着色器指定位置的一致变量设置一个3x3单精度浮点矩阵数值
    * jegl_uniform_float3x3 不会初始化着色器，请在操作之前调用 jegl_bind_shader
        以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_bind_shader
*/
JE_API void jegl_uniform_float3x3(uint32_t location, const float (*mat)[3]);

/*
jegl_uniform_float4x4 [基本接口]
向当前着色器指定位置的一致变量设置一个4x4单精度浮点矩阵数值
    * jegl_uniform_float4x4 不会初始化着色器，请在操作之前调用 jegl_bind_shader
        以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_bind_shader
*/
JE_API void jegl_uniform_float4x4(uint32_t location, const float (*mat)[4]);

// jegl rendchain api

/*
jegl_rendchain [类型]
绘制链对象
RendChain是引擎提供的一致绘制操作接口，支持不同线程提交不同的渲染链，并最终完成渲染
一个RendChain是对一个缓冲区的渲染操作，包括以下流程：
    * 第0步：创建
    * 第1步：调用jegl_rchain_begin绑定缓冲区
    * 第2步：根据需要，调用jegl_rchain_clear_color_buffer或jegl_rchain_clear_depth_buffer
            用于清除指定缓冲区的状态
    * 第3步：根据需要，调用jegl_rchain_bind_uniform_buffer绑定所需的一致变量缓冲区
    * 第4步：进行若干次绘制
    * 第5步：在图形线程中调用jegl_rchain_commit提交绘制链
    * 第6步：如果需要复用此链，可以保留链实例，从第1步重新开始绘制，否则：
    * 第7步：调用jegl_rchain_close销毁当前链
请参见：
    jegl_rchain_begin
    jegl_rchain_close
    jegl_rchain_bind_uniform_buffer
    jegl_rchain_clear_color_buffer
    jegl_rchain_clear_depth_buffer
    jegl_rchain_close
*/
struct jegl_rendchain;

/*
jegl_rendchain_rend_action [类型]
绘制操作对象，调用jegl_rchain_draw执行绘制操作时返回此对象地址用于
后续指定一致变量操作使用
请参见：
    jegl_rchain_draw
*/
struct jegl_rendchain_rend_action;

/*
jegl_rchain_create [基本接口]
创建绘制链实例
请参见：
    jegl_rendchain
*/
JE_API jegl_rendchain* jegl_rchain_create();

/*
jegl_rchain_close [基本接口]
销毁绘制链实例
请参见：
    jegl_rendchain
*/
JE_API void jegl_rchain_close(jegl_rendchain* chain);

/*
jegl_rchain_begin [基本接口]
绑定绘制链的绘制目标，此操作是绘制周期的开始，若指定的framebuffer == nullptr，
指示绘制目标为屏幕缓冲区，xywh为绘制剪裁空间左下角的位置和宽高，单位是像素
请参见：
    jegl_rendchain
*/
JE_API void jegl_rchain_begin(jegl_rendchain* chain, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h);

/*
jegl_rchain_bind_uniform_buffer [基本接口]
绑定绘制链的一致变量缓冲区
*/
JE_API void jegl_rchain_bind_uniform_buffer(jegl_rendchain* chain, jegl_resource* uniformbuffer);

/*
jegl_rchain_clear_color_buffer [基本接口]
指示此链绘制开始时需要清除目标缓冲区的颜色缓存
*/
JE_API void jegl_rchain_clear_color_buffer(jegl_rendchain* chain, const float* color);

/*
jegl_rchain_clear_depth_buffer [基本接口]
指示此链绘制开始时需要清除目标缓冲区的深度缓存
*/
JE_API void jegl_rchain_clear_depth_buffer(jegl_rendchain* chain);

typedef size_t jegl_rchain_texture_group_idx_t;

/*
jegl_rchain_allocate_texture_group [基本接口]
创建纹理组，返回可通过jegl_rchain_draw作用于绘制操作或jegl_rchain_bind_pre_texture_group
作用于全局的纹理组句柄
可通过jegl_rchain_bind_texture向纹理组中提交纹理
请参见：
    jegl_rchain_draw
    jegl_rchain_bind_texture
    jegl_rchain_bind_pre_texture_group
*/
JE_API jegl_rchain_texture_group_idx_t jegl_rchain_allocate_texture_group(
    jegl_rendchain* chain);

/*
jegl_rchain_draw [基本接口]
将指定的顶点，使用指定的着色器和纹理将绘制操作作用到绘制链上
    * 若绘制的物体不需要使用纹理，可以使用不绑定纹理的纹理组或传入 SIZE_MAX
    * 返回的对象仅限在同一个渲染链的下一次绘制命令开始之前使用。
*/
JE_API jegl_rendchain_rend_action* jegl_rchain_draw(
    jegl_rendchain* chain,
    jegl_resource* shader,
    jegl_resource* vertex,
    jegl_rchain_texture_group_idx_t texture_group);

/*
jegl_rchain_set_uniform_buffer [基本接口]
为 act 指定的绘制操作应用一致变量缓冲区
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_buffer(
    jegl_rendchain_rend_action* act,
    jegl_resource* uniform_buffer);

/*
jegl_rchain_set_uniform_int [基本接口]
为 act 指定的绘制操作应用整型一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_int(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    int val);

/*
jegl_rchain_set_uniform_int2 [基本接口]
为 act 指定的绘制操作应用二维整型矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_int2(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    int x,
    int y);

/*
jegl_rchain_set_uniform_int3 [基本接口]
为 act 指定的绘制操作应用三维整型矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_int3(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    int x,
    int y,
    int z);

/*
jegl_rchain_set_uniform_int4 [基本接口]
为 act 指定的绘制操作应用四维整型矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_int4(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    int x,
    int y,
    int z,
    int w);

/*
jegl_rchain_set_uniform_float [基本接口]
为 act 指定的绘制操作应用单精度浮点数一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    float val);

/*
jegl_rchain_set_uniform_float2 [基本接口]
为 act 指定的绘制操作应用二维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float2(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    float x,
    float y);

/*
jegl_rchain_set_uniform_float3 [基本接口]
为 act 指定的绘制操作应用三维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float3(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    float x,
    float y,
    float z);

/*
jegl_rchain_set_uniform_float4 [基本接口]
为 act 指定的绘制操作应用四维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float4(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    float x,
    float y,
    float z,
    float w);

/*
jegl_rchain_set_uniform_float4x4 [基本接口]
为 act 指定的绘制操作应用4x4单精度浮点数矩阵一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float4x4(
    jegl_rendchain_rend_action* act,
    uint32_t binding_place,
    const float (*mat)[4]);

/*
jegl_rchain_set_builtin_uniform_int [基本接口]
为 act 指定的绘制操作应用整型一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_int(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    int val);

/*
jegl_rchain_set_builtin_uniform_int2 [基本接口]
为 act 指定的绘制操作应用二维整型一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_int2(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    int x, int y);

/*
jegl_rchain_set_builtin_uniform_int3 [基本接口]
为 act 指定的绘制操作应用三维整型一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_int3(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    int x, int y, int z);

/*
jegl_rchain_set_builtin_uniform_int4 [基本接口]
为 act 指定的绘制操作应用四维整型一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_int4(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    int x, int y, int z, int w);

/*
jegl_rchain_set_builtin_uniform_float [基本接口]
为 act 指定的绘制操作应用单精度浮点数一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    float val);

/*
jegl_rchain_set_builtin_uniform_float2 [基本接口]
为 act 指定的绘制操作应用二维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float2(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    float x,
    float y);

/*
jegl_rchain_set_builtin_uniform_float3 [基本接口]
为 act 指定的绘制操作应用三维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float3(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    float x,
    float y,
    float z);

/*
jegl_rchain_set_builtin_uniform_float4 [基本接口]
为 act 指定的绘制操作应用四维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float4(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    float x,
    float y,
    float z,
    float w);

/*
jegl_rchain_set_builtin_uniform_float4x4 [基本接口]
为 act 指定的绘制操作应用4x4单精度浮点数矩阵一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float4x4(
    jegl_rendchain_rend_action* act,
    uint32_t* binding_place,
    const float (*mat)[4]);

/*
jegl_rchain_bind_texture [基本接口]
为指定的纹理组，在指定的纹理通道绑定一个纹理
请参见：
    jegl_rchain_allocate_texture_group
*/
JE_API void jegl_rchain_bind_texture(
    jegl_rendchain* chain,
    jegl_rchain_texture_group_idx_t texture_group,
    size_t binding_pass,
    jegl_resource* texture);

/*
jegl_rchain_bind_pre_texture_group [基本接口]
将指定的纹理组在全部绘制操作开始前绑定
    * 预先绑定的纹理可能被覆盖，请保证与其他绘制操作占据的通道做出区分
请参见：
    jegl_rchain_allocate_texture_group
    jegl_rchain_bind_texture
*/
JE_API void jegl_rchain_bind_pre_texture_group(
    jegl_rendchain* chain,
    size_t texture_group);

/*
jegl_rchain_commit [基本接口]
将指定的绘制链在图形线程中进行提交
    * 此函数只允许在图形线程内调用
*/
JE_API void jegl_rchain_commit(
    jegl_rendchain* chain,
    jegl_context* glthread);

/*
jegl_rchain_get_target_framebuf [基本接口]
获取当前绘制链的目标帧缓冲区
    * 如果当前绘制链的目标帧缓冲区是屏幕缓冲区，则返回 nullptr
*/
JE_API jegl_resource* jegl_rchain_get_target_framebuf(
    jegl_rendchain* chain);

/*
jegl_uhost_get_or_create_for_universe [基本接口]
获取或创建指定Universe的可编程图形上下文接口 uhost
    * config 被用于指示图形配置，若首次创建图形接口则使用此设置，
    若图形接口已经创建，则调用jegl_reboot_graphic_thread以应用图形配置
    * 若需要使用默认配置或不改变图形设置，请传入 nullptr
    * 创建出的 uhost 将通过 je_ecs_universe_register_exit_callback 接口注册
    Universe 的退出回调，以便在 Universe 退出时释放 uhost 实例
请参见：
    jegl_reboot_graphic_thread
    je_ecs_universe_register_exit_callback
*/
JE_API jeecs::graphic_uhost* jegl_uhost_get_or_create_for_universe(
    void* universe,
    const jegl_interface_config* config);

/*
jegl_uhost_get_context [基本接口]
从指定的可编程图形上下文接口获取图形线程的正式描述符
*/
JE_API jegl_context* jegl_uhost_get_context(
    jeecs::graphic_uhost* host);

/*
jegl_uhost_set_skip_behavior [基本接口]
设置图形实现请求跳过这一帧时，uhost的绘制动作行为
    * skip_all_draw 为真时，跳过全部绘制动作；反之只跳过以屏幕缓冲区为目标的动作
    * uhost 实例创建时默认为真
*/
JE_API void jegl_uhost_set_skip_behavior(
    jeecs::graphic_uhost* host, bool skip_all_draw);

/*
jegl_uhost_alloc_branch [基本接口]
从指定的可编程图形上下文接口申请一个绘制组
    * 所有申请出的绘制组都需要在对应 uhost 关闭之前，通过 jegl_uhost_free_branch 释放
请参见：
    jegl_uhost_free_branch
*/
JE_API jeecs::rendchain_branch* jegl_uhost_alloc_branch(
    jeecs::graphic_uhost* host);

/*
jegl_uhost_free_branch [基本接口]
从指定的可编程图形上下文接口释放一个绘制组
*/
JE_API void jegl_uhost_free_branch(
    jeecs::graphic_uhost* host,
    jeecs::rendchain_branch* free_branch);

/*
jegl_branch_new_frame [基本接口]
在绘制开始之前，指示绘制组开始新的一帧，并指定优先级
*/
JE_API void jegl_branch_new_frame(
    jeecs::rendchain_branch* branch,
    int priority);

/*
jegl_branch_new_chain [基本接口]
从绘制组中获取一个新的RendChain
请参见：
    jegl_rendchain
*/
JE_API jegl_rendchain* jegl_branch_new_chain(
    jeecs::rendchain_branch* branch,
    jegl_resource* framebuffer,
    size_t x,
    size_t y,
    size_t w,
    size_t h);

/*
jegui_set_font [基本接口]
让ImGUI使用指定的路径和字体大小设置
    * 若 general_font_path 是空指针，则使用默认字体
    * 若 general_font_path 非空，则针对拉丁字符集使用 latin_font_path 指示的字体
    * 仅在 jegui_init_basic 前调用生效
    * 此接口仅适合用于对接自定义渲染API时使用
请参见：
    jegui_init_basic
*/
JE_API void jegui_set_font(
    const char* general_font_path,
    const char* latin_font_path,
    size_t size);

typedef uint64_t jegui_user_image_handle_t;
typedef jegui_user_image_handle_t(*jegui_user_image_loader_t)(jegl_context*, jegl_resource*);
typedef void (*jegui_user_sampler_loader_t)(jegl_context*, jegl_resource*);

/*
jegui_init_basic [基本接口]
初始化ImGUI
    * 参数1 是一个指向引擎图形上下文的指针
    * 参数2 是一个回调函数指针，用于获取一个纹理对应的底层图形库句柄以供ImGUI使用
    * 参数3 是一个回调函数指针，用于应用一个指定着色器指定的采样设置
    * 此接口仅适合用于对接自定义渲染API时使用
*/
JE_API void jegui_init_basic(
    jegl_context* gl_context,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler);

typedef void (*jegui_platform_draw_callback_t)(void*);

/*
jegui_update_basic [基本接口]
一帧渲染开始之后，需要调用此接口以完成ImGUI的绘制和更新操作
    * 此接口仅适合用于对接自定义渲染API时使用
*/
JE_API void jegui_update_basic(
    jegui_platform_draw_callback_t platform_draw_callback,
    void* data);

/*
jegui_shutdown_basic [基本接口]
图形接口关闭时，请调用此接口关闭ImGUI
    * 参数reboot用于指示当前是否正在重启图形接口，由统一基础图形接口给出
*/
JE_API void jegui_shutdown_basic(bool reboot);

/*
jegui_shutdown_callback [基本接口]
当通过关闭窗口等，请求关闭图形接口时，可以调用此函数用于询问ImGUI注册的
退出回调，获取是否确认关闭
    * 返回 true 表示注册的回调函数不存在或建议关闭图形接口
*/
JE_API bool jegui_shutdown_callback(void);

/*
je_io_update_key_state [基本接口]
更新指定键的状态信息
*/
JE_API void je_io_update_key_state(jeecs::input::keycode keycode, bool keydown);

/*
je_io_update_mouse_pos [基本接口]
更新鼠标（或触摸位置点）的坐标
    * 此操作`不会`影响光标的实际位置
*/
JE_API void je_io_update_mouse_pos(size_t group, int x, int y);

/*
je_io_update_mouse_state [基本接口]
更新鼠标（或触摸点）的状态
*/
JE_API void je_io_update_mouse_state(size_t group, jeecs::input::mousecode key, bool keydown);

/*
je_io_update_window_size [基本接口]
更新窗口大小
    * 此操作`不会`影响窗口的实际大小
*/
JE_API void je_io_update_window_size(int x, int y);

/*
je_io_update_window_pos [基本接口]
更新窗口位置
    * 此操作`不会`影响窗口的实际位置
*/
JE_API void je_io_update_window_pos(int x, int y);

/*
je_io_update_wheel [基本接口]
更新鼠标滚轮的计数
*/
JE_API void je_io_update_wheel(size_t group, float x, float y);

/*
je_io_get_key_down [基本接口]
获取指定的按键是否被按下
*/
JE_API bool je_io_get_key_down(jeecs::input::keycode keycode);

/*
je_io_get_mouse_pos [基本接口]
获取鼠标的坐标
*/
JE_API void je_io_get_mouse_pos(size_t group, int* out_x, int* out_y);

/*
je_io_get_mouse_state [基本接口]
获取鼠标的按键状态
*/
JE_API bool je_io_get_mouse_state(size_t group, jeecs::input::mousecode key);

/*
je_io_get_window_size [基本接口]
获取窗口的大小
*/
JE_API void je_io_get_window_size(int* out_x, int* out_y);

/*
je_io_get_window_pos [基本接口]
获取窗口的位置
*/
JE_API void je_io_get_window_pos(int* out_x, int* out_y);

/*
je_io_get_wheel [基本接口]
获取鼠标滚轮的计数
*/
JE_API void je_io_get_wheel(size_t group, float* out_x, float* out_y);

/*
je_io_set_lock_mouse [基本接口]
设置是否需要将鼠标锁定
*/
JE_API void je_io_set_lock_mouse(bool lock);

/*
je_io_get_lock_mouse [基本接口]
获取当前是否应该锁定鼠标
*/
JE_API bool je_io_get_lock_mouse();

/*
je_io_set_window_size [基本接口]
请求对窗口大小进行调整
*/
JE_API void je_io_set_window_size(int x, int y);

/*
je_io_fetch_update_window_size [基本接口]
获取当前窗口大小是否应该调整及调整的大小
*/
JE_API bool je_io_fetch_update_window_size(int* out_x, int* out_y);

/*
je_io_set_window_title [基本接口]
请求对窗口标题进行调整
*/
JE_API void je_io_set_window_title(const char* title);

/*
je_io_fetch_update_window_title [基本接口]
获取当前是否需要对窗口标题进行调整及调整之后的内容
*/
JE_API bool je_io_fetch_update_window_title(const char** out_title);

/*
je_io_gamepad_handle_t [类型]
指示虚拟手柄实例
*/
typedef struct _je_gamepad_state* je_io_gamepad_handle_t;

/*
je_io_create_gamepad [基本接口]
创建一个虚拟手柄实例
    * name 可用于以可读形式分辨不同的控制器设备，如果为 nullptr，则分配一个默认的名称
    * guid 可用于区分不同的物理设备，如果为nullptr，则由引擎生成。
*/
JE_API je_io_gamepad_handle_t je_io_create_gamepad(
    const char* name_may_null, const char* guid_may_null);
/*
je_io_close_gamepad [基本接口]
关闭一个虚拟手柄实例
*/
JE_API void je_io_close_gamepad(je_io_gamepad_handle_t gamepad);

/*
je_io_gamepad_name [基本接口]
获取指定虚拟手柄的名称
    * 名称在创建时指定，用于以可读形式分辨不同的控制器设备
参见：
    je_io_create_gamepad
*/
JE_API const char* je_io_gamepad_name(je_io_gamepad_handle_t gamepad);
/*
je_io_gamepad_guid [基本接口]
获取指定虚拟手柄的GUID
    * GUID在创建时指定，用于区分不同的物理设备
参见：
    je_io_create_gamepad
*/
JE_API const char* je_io_gamepad_guid(je_io_gamepad_handle_t gamepad);
/*
je_io_gamepad_get [基本接口]
获取所有虚拟手柄的句柄
    * count 用于指示传入的out_gamepads可容纳的最大数量，如果实际手柄数量大于count，
        则只返回前count个句柄；如果实际手柄数量小于count，则返回实际数量。
    * 作为特例，当 count 为 0 时，out_gamepads 可以为 nullptr，此时函数返回实际手柄数量。
*/
JE_API size_t je_io_gamepad_get(size_t count, je_io_gamepad_handle_t* out_gamepads);

/*
je_io_gamepad_is_active [基本接口]
检查指定的虚拟手柄，其对应的实际设备是否存在
    * 虚拟手柄和输入设备的对应关系通过 je_io_create_gamepad 构建，
        其不一定是实际的物理游戏手柄；例如，可以将键盘按键的按动情况
        映射到虚拟手柄上。当 je_io_close_gamepad 调用时，虚拟手柄实例
        会被销毁，此时 je_io_gamepad_is_active 返回 false。
    * 如果 out_last_pushed_time_may_null 不为 nullptr，则将返回手柄的最后
        操作时间。
参见：
    je_io_create_gamepad
    je_io_close_gamepad
*/
JE_API bool je_io_gamepad_is_active(
    je_io_gamepad_handle_t gamepad,
    jeecs::typing::ms_stamp_t* out_last_pushed_time_may_null);
/*
je_io_gamepad_get_button_down [基本接口]
获取指定虚拟手柄的指定按键是否被按下
    * code 是一个枚举值，用于指示按键的类型
    * 若指定的按键不存在，则始终返回 false
    * 如果手柄已经被断开（je_io_close_gamepad），调用此接口依然是合法的，但返回值始终是 false
参见：
    je_io_close_gamepad
*/
JE_API bool je_io_gamepad_get_button_down(
    je_io_gamepad_handle_t gamepad, jeecs::input::gamepadcode code);
/*
je_io_gamepad_update_button_state [基本接口]
更新指定的虚拟手柄按键状态，可以被 je_io_gamepad_get_button_down 获取
    * code 是一个枚举值，用于指示按键的类型
    * 不允许对已经断开（je_io_close_gamepad）的手柄进行此操作
    * 如果输入坐标的模长大于1，坐标将被归一化
参见：
    je_io_gamepad_get_button_down
    je_io_close_gamepad
*/
JE_API void je_io_gamepad_update_button_state(
    je_io_gamepad_handle_t gamepad, jeecs::input::gamepadcode code, bool down);

/*
je_io_gamepad_get_stick [基本接口]
获取指定虚拟手柄的指定摇杆的坐标
    * stickid 是一个枚举值，用于指示摇杆的类型
    * out_x 和 out_y 用于接收摇杆的坐标，坐标的模长取值范围是 [0, 1]
    * 如果手柄已经被断开（je_io_close_gamepad），调用此接口依然是合法的，但获取到的坐标都将是 0
    * 获取扳机键（LT, RT）时，请使用 x 值，y 值始终为 0
参见：
    je_io_close_gamepad
*/
JE_API void je_io_gamepad_get_stick(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float* out_x, float* out_y);
/*
je_io_gamepad_update_stick [基本接口]
更新指定虚拟手柄的指定摇杆的坐标
    * stickid 是一个枚举值，用于指示摇杆的类型
    * x 和 y 是摇杆的坐标，应当确保坐标的模长不大于1
    * 不允许对已经断开（je_io_close_gamepad）的手柄进行此操作
    * 获取扳机键（LT, RT）时，请使用 x 值，并保持y 值始终为 0
    * 当摇杆向上拨动时，y 值应当为正数；当摇杆向右拨动时，x 值应当为正数；
    * 当扳机键按下时，x 值应当为正数
参见：
    je_io_close_gamepad
*/
JE_API void je_io_gamepad_update_stick(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float x, float y);

/*
je_io_gamepad_stick_set_deadzone [基本接口]
设置指定虚拟手柄的指定摇杆的死区
    * stickid 是一个枚举值，用于指示摇杆的类型
    * 当设置摇杆的坐标（je_io_gamepad_update_stick）时，如果坐标的模长小于死区，则坐标将被视为0
*/
JE_API void je_io_gamepad_stick_set_deadzone(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float deadzone);

// Library / Module loader

/*
je_module_load [基本接口]
以name为名字，加载指定路径的动态库（遵循woolang规则）加载失败返回nullptr
*/
JE_API wo_dylib_handle_t je_module_load(const char* name, const char* path);

/*
je_module_func [基本接口]
从动态库中加载某个函数，返回函数的地址
*/
JE_API void* je_module_func(wo_dylib_handle_t lib, const char* funcname);

/*
je_module_unload [基本接口]
立即卸载某个动态库
*/
JE_API void je_module_unload(wo_dylib_handle_t lib);

// Audio
struct jeal_native_play_device_instance;
struct jeal_play_device
{
    jeal_native_play_device_instance*
        m_device_instance;

    const char* m_name;        // 设备名称
    bool m_active;             // 是否是当前使用的播放设备
    int m_max_auxiliary_sends; // 最大辅助发送数量，如果等于0，则不支持
};

/*
jeal_state [类型]
用于表示当前声源的播放状态，包括停止、播放中和暂停
*/
enum jeal_state
{
    JE_AUDIO_STATE_STOPPED,
    JE_AUDIO_STATE_PLAYING,
    JE_AUDIO_STATE_PAUSED,
};

/*
jeal_format [类型]
用于表示波形的格式
*/
enum jeal_format
{
    JE_AUDIO_FORMAT_MONO8,
    JE_AUDIO_FORMAT_MONO16,
    JE_AUDIO_FORMAT_MONO32F,
    JE_AUDIO_FORMAT_STEREO8,
    JE_AUDIO_FORMAT_STEREO16,
    JE_AUDIO_FORMAT_STEREO32F,
};

struct jeal_native_buffer_instance;
struct jeal_buffer
{
    jeal_native_buffer_instance*
        m_buffer_instance;

    size_t m_references; // 引用计数

    const void* m_data;
    size_t m_size;
    size_t m_sample_rate;
    size_t m_sample_size;
    size_t m_byte_rate;
    jeal_format m_format;
};

struct jeal_native_source_instance;
struct jeal_source
{
    jeal_native_source_instance*
        m_source_instance;

    bool m_loop;         // 是否循环播放
    float m_gain;        // 音量增益
    float m_pitch;       // 播放速度
    float m_location[3]; // 声音位置
    float m_velocity[3]; // 声源自身速度（非播放速度）
};

struct jeal_listener
{
    float m_gain;        // 音量增益，最终起效的是 m_gain * m_global_gain
    float m_global_gain; // 全局增益，最终起效的是 m_gain * m_global_gain
    float m_location[3]; // 监听器位置
    float m_velocity[3]; // 监听器自身速度（非播放速度）
    float m_forward[3];  // 监听器朝向（前方向）
    float m_upward[3];   // 监听器朝向（顶方向）
};

struct jeal_native_effect_slot_instance;
struct jeal_effect_slot
{
    jeal_native_effect_slot_instance*
        m_effect_slot_instance;

    size_t m_references; // 引用计数

    float m_gain; // 增益，默认值为 1.0，范围 [0.0, 1.0]
};

/*
jeal_effect_reverb [类型]
混响效果
模拟声音在封闭空间（如房间、大厅）中的反射效果，增加空间感。
*/
struct jeal_effect_reverb
{
    float m_density;                // 密度, 默认值为 1.0，范围 [0.0, 1.0]
    float m_diffusion;              // 扩散度, 默认值为 1.0，范围 [0.0, 1.0]
    float m_gain;                   // 增益, 默认值为 0.32，范围 [0.0, 1.0]
    float m_gain_hf;                // 高频增益, 默认值为 0.89，范围 [0.0, 1.0]
    float m_decay_time;             // 衰减时间, 默认值为 1.49，范围 [0.1, 20.0]
    float m_decay_hf_ratio;         // 高频衰减比率, 默认值为 0.83，范围 [0.1, 2.0]
    float m_reflections_gain;       // 反射增益, 默认值为 0.05，范围 [0.0, 3.16]
    float m_reflections_delay;      // 反射延迟, 默认值为 0.007，范围 [0.0, 0.3]
    float m_late_reverb_gain;       // 后混响增益, 默认值为 1.26，范围 [0.0, 10.0]
    float m_late_reverb_delay;      // 后混响延迟, 默认值为 0.011，范围 [0.0, 0.1]
    float m_air_absorption_gain_hf; // 高频吸收, 默认值为 0.994，范围 [0.892, 1.0]
    float m_room_rolloff_factor;    // 房间衰减因子, 默认值为 0.0，范围 [0.0, 10.0]
    bool m_decay_hf_limit;          // 高频衰减限制器, 默认值为 false
};

/*
jeal_effect_chorus [类型]
合唱效果
通过轻微延迟和音高变化复制原始声音，产生多个声部同时发声的合唱效果。
*/
struct jeal_effect_chorus
{
    enum waveform
    {
        SINUSOID = 0,
        TRIANGLE = 1,
    };

    waveform m_waveform; // 波形类型，默认值为 SINUSOID
    int m_phase;         // 相位，默认值为 90，范围 [-180, 180]
    float m_rate;        // 速率，默认值为 1.1，范围 [0.0, 10.0]
    float m_depth;       // 深度，默认值为 0.1，范围 [0.0, 1.0]
    float m_feedback;    // 反馈，默认值为 0.25，范围 [-1.0, 1.0]
    float m_delay;       // 延迟，默认值为 0.16，范围 [0.0, 0.016]
};

/*
jeal_effect_distortion [类型]
失真效果
故意扭曲音频波形，常见于电吉他等乐器，制造粗糙或过载的音色。
*/
struct jeal_effect_distortion
{
    float m_edge;                  // 边缘，默认值为 0.2，范围 [0.0, 1.0]
    float m_gain;                  // 增益，默认值为 0.05，范围 [0.01, 1.0]
    float m_lowpass_cutoff;        // 低通截止频率，默认值为 8000.0，范围 [80.0, 24000.0]
    float m_equalizer_center_freq; // 均衡器中心频率，默认值为 3600.0，范围 [80.0, 24000.0]
    float m_equalizer_bandwidth;   // 均衡器带宽，默认值为 3600.0，范围 [80.0, 24000.0]
};

/*
jeal_effect_echo [类型]
回声效果
模拟声音在远处反射后延迟返回的效果（如山谷中的回声）。
*/
struct jeal_effect_echo
{
    float m_delay;    // 延迟时间，默认值为 0.1，范围 [0.0, 0.207]
    float m_lr_delay; // 左声道延迟，默认值为 0.1，范围 [0.0, 0.404]
    float m_damping;  // 阻尼，默认值为 0.5，范围 [0.0, 0.99]
    float m_feedback; // 反馈，默认值为 0.5，范围 [0.0, 1.0]
    float m_spread;   // 扩散，默认值为 -1.0，范围 [-1.0, 1.0]
};

/*
jeal_effect_flanger [类型]
镶边效果
通过混合延迟时间变化的原始信号，产生类似“喷气机”或太空感的波动音效
*/
struct jeal_effect_flanger
{
    typedef jeal_effect_chorus::waveform waveform;

    waveform m_waveform; // 波形类型，默认值为 TRIANGLE
    int m_phase;         // 相位，默认值为 0，范围 [-180, 180]
    float m_rate;        // 速率，默认值为 0.27，范围 [0.0, 10.0]
    float m_depth;       // 深度，默认值为 1.0，范围 [0.0, 1.0]
    float m_feedback;    // 反馈，默认值为 -0.5，范围 [-1.0, 1.0]
    float m_delay;       // 延迟，默认值为 0.002，范围 [0.0, 0.004]
};

/*
jeal_effect_frequency_shifter [类型]
移频器
改变输入信号的频率，用于创造科幻或非自然的声音效果。
*/
struct jeal_effect_frequency_shifter
{
    enum direction
    {
        DOWN = 0,
        UP = 1,
        OFF = 2,
    };

    float m_frequency;           // 频率，默认值为 0.0，范围 [0.0, 24000.0]
    direction m_left_direction;  // 左声道方向，默认值为 DOWN
    direction m_right_direction; // 右声道方向，默认值为 DOWN
};

/*
jeal_effect_vocal_morpher [类型]
人声变声器
调制人声的某些频段，实现机器人声、电话音效等特殊变声效果。
*/
struct jeal_effect_vocal_morpher
{
    enum phoneme
    {
        A = 0,
        E,
        I,
        O,
        U,
        AA,
        AE,
        AH,
        AO,
        EH,
        ER,
        IH,
        IY,
        UH,
        UW,
        B,
        D,
        F,
        G,
        J,
        K,
        L,
        M,
        N,
        P,
        R,
        S,
        T,
        V,
        Z,
    };

    enum waveform
    {
        SINUSOID = 0,
        TRIANGLE = 1,
        SAWTOOTH = 2,
    };

    phoneme m_phoneme_a;           // 元音a，默认值为 A
    int m_phoneme_a_coarse_tuning; // 元音a粗调，默认值为 0，范围 [-24, 24]
    phoneme m_phoneme_b;           // 元音b，默认值为 ER
    int m_phoneme_b_coarse_tuning; // 元音b粗调，默认值为 0，范围 [-24, 24]
    waveform m_waveform;           // 波形类型，默认值为 SINUSOID
    float m_rate;                  // 速率，默认值为 1.41，范围 [0.0, 10.0]
};

/*
jeal_effect_pitch_shifter [类型]
移调器
实时调整音高（如升高或降低音调），不改变播放速度。
*/
struct jeal_effect_pitch_shifter
{
    int m_coarse_tune; // 粗调，默认值为 12，范围 [-12, 12]
    int m_fine_tune;   // 细调，默认值为 0，范围 [-50, 50]
};

/*
jeal_effect_ring_modulator [类型]
环形调制器
用输入信号与载波频率混合，产生金属感或机器人般的音效。
*/
struct jeal_effect_ring_modulator
{
    enum waveform
    {
        SINUSOID = 0,
        SAWTOOTH = 1,
        SQUARE = 2,
    };

    float m_frequency;       // 频率，默认值为 440.0，范围 [0.0, 8000.0]
    float m_highpass_cutoff; // 高通截止频率，默认值为 800.0，范围 [0.0, 24000.0]
    waveform m_waveform;     // 波形类型，默认值为 SINUSOID
};

/*
jeal_effect_autowah [类型]
自动哇音效果
根据输入音量动态调整滤波器频率，产生“哇哇”声（常用于吉他）。
*/
struct jeal_effect_autowah
{
    float m_attack_time;  // 攻击时间，默认值为 0.06，范围 [0.0001, 1.0]
    float m_release_time; // 释放时间，默认值为 0.06，范围 [0.0001, 1.0]
    float m_resonance;    // 共鸣，默认值为 1000.0，范围 [2.0, 1000]
    float m_peak_gain;    // 峰值增益，默认值为 11.22，范围 [0.00003, 31621.0]
};

/*
jeal_effect_compressor [类型]
压缩器
减小音频的动态范围，使安静部分更响亮、响亮部分更柔和，平衡音量。
*/
struct jeal_effect_compressor
{
    bool m_enabled; // 是否启用，默认值为 true
};

/*
jeal_effect_equalizer [类型]
均衡器
调整音频频率响应，增强或削弱特定频段的音量，改善音质。
*/
struct jeal_effect_equalizer
{
    float m_low_gain;    // 低频增益，默认值为 1.0，范围 [0.126, 7.943]
    float m_low_cutoff;  // 低频截止频率，默认值为 200.0，范围 [50.0, 800.0]
    float m_mid1_gain;   // 中频1增益，默认值为 1.0，范围 [0.126, 7.943]
    float m_mid1_center; // 中频1中心频率，默认值为 500.0，范围 [200.0, 3000.0]
    float m_mid1_width;  // 中频1带宽，默认值为 1.0，范围 [0.01, 1.0]
    float m_mid2_gain;   // 中频2增益，默认值为 1.0，范围 [0.126, 7.943]
    float m_mid2_center; // 中频2中心频率，默认值为 3000.0，范围 [1000.0, 8000.0]
    float m_mid2_width;  // 中频2带宽，默认值为 1.0，范围 [0.01, 1.0]
    float m_high_gain;   // 高频增益，默认值为 1.0，范围 [0.126, 7.943]
    float m_high_cutoff; // 高频截止频率，默认值为 6000.0，范围 [4000.0, 160000.0]
};

/*
jeal_effect_eaxreverb [类型]
拓展EAX混响效果
扩展的混响效果，支持更复杂的环境模拟（如洞穴、水下），需兼容EAX的硬件支持。
*/
struct jeal_effect_eaxreverb
{
    float m_density;                // 密度，默认值为 1.0，范围 [0.0, 1.0]
    float m_diffusion;              // 扩散度，默认值为 1.0，范围 [0.0, 1.0]
    float m_gain;                   // 增益，默认值为 0.32，范围 [0.0, 1.0]
    float m_gain_hf;                // 高频增益，默认值为 0.89，范围 [0.0, 1.0]
    float m_gain_lf;                // 低频增益，默认值为 0.0，范围 [0.0, 1.0]
    float m_decay_time;             // 衰减时间，默认值为 1.49，范围 [0.1, 20.0]
    float m_decay_hf_ratio;         // 高频衰减比率，默认值为 0.83，范围 [0.1, 2.0]
    float m_decay_lf_ratio;         // 低频衰减比率，默认值为 1.0，范围 [0.1, 2.0]
    float m_reflections_gain;       // 反射增益，默认值为 0.05，范围 [0.0, 3.16]
    float m_reflections_delay;      // 反射延迟，默认值为 0.007，范围 [0.0, 0.3]
    float m_reflections_pan_xyz[3]; // 反射声道位置，默认值为 {0.0, 0.0, 0.0}
    float m_late_reverb_gain;       // 后混响增益，默认值为 1.26，范围 [0.0, 10.0]
    float m_late_reverb_delay;      // 后混响延迟，默认值为 0.011，范围 [0.0, 0.1]
    float m_late_reverb_pan_xyz[3]; // 后混响声道位置，默认值为 {0.0, 0.0, 0.0}
    float m_echo_time;              // 回声时间，默认值为 0.25，范围 [0.075, 0.25]
    float m_echo_depth;             // 回声深度，默认值为 0.0，范围 [0.0, 1.0]
    float m_modulation_time;        // 调制时间，默认值为 0.25，范围 [0.04, 4.0]
    float m_modulation_depth;       // 调制深度，默认值为 0.0，范围 [0.0, 1.0]
    float m_air_absorption_gain_hf; // 高频吸收，默认值为 0.994，范围 [0.892, 1.0]
    float m_hf_reference;           // 高频反射，默认值为 5000.0，范围 [1000.0, 20000.0]
    float m_lf_reference;           // 低频反射，默认值为 250.0，范围 [20.0, 1000.0]
    float m_room_rolloff_factor;    // 房间衰减因子，默认值为 0.0，范围 [0.0, 10.0]
    bool m_decay_hf_limit;          // 高频衰减限制器，默认值为 true
};

/*
jeal_create_buffer [基本接口]
创建一个音频缓冲区
    * data 是音频数据的指针
    * buffer_data_len 是音频数据的长度（字节数）
    * sample_rate 是采样率（Hz）
    * format 是音频格式
    返回值是一个指向音频缓冲区的指针
    使用完毕后需要调用 jeal_close_buffer 释放资源
参见：
    jeal_close_buffer
*/
JE_API const jeal_buffer* jeal_create_buffer(
    const void* data,
    size_t buffer_data_len,
    size_t sample_rate,
    jeal_format format);

/*
jeal_load_buffer_wav [基本接口]
加载一个WAV格式的音频文件
    * path 是音频文件的路径
    * 返回值是一个指向音频缓冲区的指针
    使用完毕后需要调用 jeal_close_buffer 释放资源
    如果加载失败，返回值为 nullptr
参见：
    jeal_close_buffer
*/
JE_API const jeal_buffer* jeal_load_buffer_wav(const char* path);

/*
jeal_close_buffer [基本接口]
释放一个音频缓冲区
*/
JE_API void jeal_close_buffer(const jeal_buffer* buffer);

/*
jeal_create_source [基本接口]
创建一个音频源
    使用完毕后需要调用 jeal_close_source 释放资源
参见：
    jeal_close_source
*/
JE_API jeal_source* jeal_create_source();

/*
jeal_update_source [基本接口]
将对 jeal_source 的参数修改应用到实际的音频源上
*/
JE_API void jeal_update_source(jeal_source* source);

/*
jeal_close_source [基本接口]
释放一个音频源
*/
JE_API void jeal_close_source(jeal_source* source);

/*
jeal_set_source_buffer [基本接口]
设置音频源的播放音频
    只有音源设置有音频时，其他的播放等动作才会生效
*/
JE_API void jeal_set_source_buffer(jeal_source* source, const jeal_buffer* buffer);

/*
jeal_set_source_effect_slot [基本接口]
设置音频源的效果槽
    slot_idx 必须小于 jeecs::audio::MAX_AUXILIARY_SENDS
*/
JE_API void jeal_set_source_effect_slot(jeal_source* source, jeal_effect_slot* slot_may_null, size_t slot_idx);

/*
jeal_play_source [基本接口]
播放音频源
    如果音频源已经在播放或者音频源没有设置音频，则不会有任何效果
    如果音频源在此前被暂停，则继续播放
    如果音频源被停止，则从音频的起始位置开始播放
*/
JE_API void jeal_play_source(jeal_source* source);

/*
jeal_pause_source [基本接口]
暂停音频源的播放
*/
JE_API void jeal_pause_source(jeal_source* source);

/*
jeal_stop_source [基本接口]
停止音频源的播放
*/
JE_API void jeal_stop_source(jeal_source* source);

/*
jeal_get_source_play_state [基本接口]
获取当前音频源的状态（是否播放、暂停或停止）
*/
JE_API jeal_state jeal_get_source_play_state(jeal_source* source);

/*
jeal_get_source_play_process [基本接口]
获取当前音频源的播放位置，单位是字节
*/
JE_API size_t jeal_get_source_play_process(jeal_source* source);

/*
jeal_set_source_play_process [基本接口]
设置音频源的播放进度，单位是字节
    注意，设置的偏移量应当是目标播放音频的采样大小的整数倍；
    如果未对齐，会被强制对齐。
*/
JE_API void jeal_set_source_play_process(jeal_source* source, size_t offset);

/*
jeal_get_listener [基本接口]
获取监听者的实例
*/
JE_API jeal_listener* jeal_get_listener();

/*
jeal_update_listener [基本接口]
将对 jeal_listener 的参数修改应用到实际的监听者上
*/
JE_API void jeal_update_listener();

/*
jeal_create_effect_slot [基本接口]
创建一个效果槽
    * 效果槽是一个用于存放效果的容器，效果槽可以被添加到声源上
    * 一个效果槽可以被添加到多个声源上
    * 一个声源亦可附加多个效果槽，但通常会受到平台限制，单个声源只能附加 1-4 个效果槽
*/
JE_API jeal_effect_slot* jeal_create_effect_slot();

/*
jeal_effect_slot_bind [基本接口]
绑定一个效果到效果槽
    * 效果传入 nullptr 表示解除绑定
*/
JE_API void jeal_effect_slot_bind(jeal_effect_slot* slot, void* effect_may_null);

/*
jeal_effect_slot_close [基本接口]
关闭一个效果槽
*/
JE_API void jeal_close_effect_slot(jeal_effect_slot* slot);

/*
jeal_update_effect_slot [基本接口]
将对 jeal_effect_slot 的参数修改应用到实际的效果槽上
*/
JE_API void jeal_update_effect_slot(jeal_effect_slot* slot);

/*
jeal_create_effect_... [基本接口]
创建一个音频效果实例
*/
JE_API jeal_effect_reverb* jeal_create_effect_reverb();
JE_API jeal_effect_chorus* jeal_create_effect_chorus();
JE_API jeal_effect_distortion* jeal_create_effect_distortion();
JE_API jeal_effect_echo* jeal_create_effect_echo();
JE_API jeal_effect_flanger* jeal_create_effect_flanger();
JE_API jeal_effect_frequency_shifter* jeal_create_effect_frequency_shifter();
JE_API jeal_effect_vocal_morpher* jeal_create_effect_vocal_morpher();
JE_API jeal_effect_pitch_shifter* jeal_create_effect_pitch_shifter();
JE_API jeal_effect_ring_modulator* jeal_create_effect_ring_modulator();
JE_API jeal_effect_autowah* jeal_create_effect_autowah();
JE_API jeal_effect_compressor* jeal_create_effect_compressor();
JE_API jeal_effect_equalizer* jeal_create_effect_equalizer();
JE_API jeal_effect_eaxreverb* jeal_create_effect_eaxreverb();

/*
jeal_close_effect [基本接口]
关闭一个音频效果实例
*/
JE_API void jeal_close_effect(void* effect);

/*
jeal_update_effect [基本接口]
将对 jeal_effect 的参数修改应用到实际的效果上
    需要注意，对效果的更新不会应用到已经绑定的效果槽和音频源上，
    需要通过 jeal_effect_slot_bind 重新绑定
参见：
    jeal_effect_slot_bind
*/
JE_API void jeal_update_effect(void* effect);

/*
jeal_refetch_devices [基本接口]
获取当前已链接的所有音频设备
    此前获取的所有设备的指针/引用都将失效，因此需要明确设备指针的所有者
*/
JE_API const jeal_play_device* jeal_refetch_devices(size_t* out_device_count);

/*
jeal_using_device [基本接口]
指示当前使用的设备
*/
JE_API void jeal_using_device(const jeal_play_device* device);

/*
jeal_check_device_connected [基本接口]
检查设备是否链接
*/
JE_API bool jeal_check_device_connected(const jeal_play_device* device);

/*
je_main_script_entry [基本接口]
运行入口脚本
    * 阻塞直到入口脚本运行完毕
    * 尝试带缓存地加载 @/builtin/editor/main.wo
    * 如果未能加载 @/builtin/editor/main.wo，则尝试 @/builtin/main.wo
*/
JE_API bool je_main_script_entry();

// DEBUG API, SHOULD NOT BE USED IN GAME PROJECT, ONLY USED FOR EDITOR
#ifdef JE_ENABLE_DEBUG_API

// NOTE: need free the return result by 'je_mem_free'
// will return all alive world pointer in the universe.
// [world1, world2,..., nullptr]
JE_API void** jedbg_get_all_worlds_in_universe(void* _universes);

JE_API const char* jedbg_get_world_name(void* _world);

JE_API void jedbg_set_world_name(void* _world, const char* name);

JE_API void jedbg_free_entity(jeecs::game_entity* _entity_list);

// NOTE: need free the return result by 'je_mem_free'(and elem with jedbg_free_entity)
JE_API jeecs::game_entity** jedbg_get_all_entities_in_world(void* _world);

// NOTE: need free the return result by 'je_mem_free'
JE_API const jeecs::typing::type_info** jedbg_get_all_components_from_entity(const jeecs::game_entity* _entity);

// NOTE: need free the return result by 'je_mem_free'
JE_API const jeecs::typing::type_info** jedbg_get_all_registed_types(void);

JE_API size_t jedbg_get_unregister_type_count(void);

// NOTE: need free the return result by 'je_mem_free'
JE_API const jeecs::typing::type_info** jedbg_get_all_system_attached_in_world(void* _world);

JE_API void jedbg_set_editing_entity_uid(const jeecs::typing::debug_eid_t uid);

JE_API jeecs::typing::debug_eid_t jedbg_get_editing_entity_uid();

JE_API jeecs::typing::debug_eid_t jedbg_get_entity_uid(const jeecs::game_entity* e);

JE_API void jedbg_get_entity_arch_information(
    jeecs::game_entity* _entity,
    size_t* _out_chunk_size,
    size_t* _out_entity_size,
    size_t* _out_all_entity_count_in_chunk);

#endif

// Atomic operator API
#define JE_DECL_ATOMIC_OPERATOR_API(TYPE)                                    \
    JE_API TYPE je_atomic_exchange_##TYPE(TYPE *aim, TYPE value);            \
    JE_API bool je_atomic_cas_##TYPE(TYPE *aim, TYPE *comparer, TYPE value); \
    JE_API TYPE je_atomic_fetch_add_##TYPE(TYPE *aim, TYPE value);           \
    JE_API TYPE je_atomic_fetch_sub_##TYPE(TYPE *aim, TYPE value);           \
    JE_API TYPE je_atomic_fetch_##TYPE(TYPE *aim);                           \
    JE_API void je_atomic_store_##TYPE(TYPE *aim, TYPE value)

JE_DECL_ATOMIC_OPERATOR_API(int8_t);
JE_DECL_ATOMIC_OPERATOR_API(uint8_t);
JE_DECL_ATOMIC_OPERATOR_API(int16_t);
JE_DECL_ATOMIC_OPERATOR_API(uint16_t);
JE_DECL_ATOMIC_OPERATOR_API(int32_t);
JE_DECL_ATOMIC_OPERATOR_API(uint32_t);
JE_DECL_ATOMIC_OPERATOR_API(int64_t);
JE_DECL_ATOMIC_OPERATOR_API(uint64_t);
JE_DECL_ATOMIC_OPERATOR_API(size_t);
JE_DECL_ATOMIC_OPERATOR_API(intptr_t);

#undef JE_DECL_ATOMIC_OPERATOR_API

JE_FORCE_CAPI_END

namespace jeecs
{
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

    /*
    jeecs::debug [命名空间]
    此处包含用于调试的工具类或工具函数
    */
    namespace debug
    {
        /*
        jeecs::debug::log [函数]
        用于产生一般日志
        */
        template <typename... ArgTs>
        inline void log(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_NORMAL, format, args...);
        }

        /*
        jeecs::debug::loginfo [函数]
        用于产生信息日志
        */
        template <typename... ArgTs>
        inline void loginfo(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_INFO, format, args...);
        }

        /*
        jeecs::debug::logwarn [函数]
        用于产生警告日志
        */
        template <typename... ArgTs>
        inline void logwarn(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_WARNING, format, args...);
        }

        /*
        jeecs::debug::logerr [函数]
        用于产生错误日志
        */
        template <typename... ArgTs>
        inline void logerr(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_ERROR, format, args...);
        }

        /*
        jeecs::debug::logfatal [函数]
        用于产生致命错误日志
        */
        template <typename... ArgTs>
        inline void logfatal(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_FATAL, format, args...);
            wo_fail(WO_FAIL_JE_FATAL_ERROR, format, args...);
        }
    }

    namespace typing
    {
#define JE_DECL_SFINAE_CHECKER_HELPLER(name, memberexpr)                        \
    template <typename T, typename VoidT = void>                                \
    struct sfinae_##name : std::false_type                                      \
    {                                                                           \
        static_assert(std::is_void<VoidT>::value);                              \
    };                                                                          \
    template <typename T>                                                       \
    struct sfinae_##name<T, std::void_t<decltype(memberexpr)>> : std::true_type \
    {                                                                           \
    };

        JE_DECL_SFINAE_CHECKER_HELPLER(has_JERefRegsiter, &T::JERefRegsiter);
        JE_DECL_SFINAE_CHECKER_HELPLER(
            match_JERefRegsiter, T::JERefRegsiter((jeecs::typing::type_unregister_guard*)nullptr));

        // static const char* T::JEScriptTypeName()
        JE_DECL_SFINAE_CHECKER_HELPLER(has_JEScriptTypeName, &T::JEScriptTypeName);
        // static const char* T::JEScriptTypeDeclare()
        JE_DECL_SFINAE_CHECKER_HELPLER(has_JEScriptTypeDeclare, &T::JEScriptTypeDeclare);
        // void T::JEParseFromScriptType(wo_vm vm, wo_value val)
        JE_DECL_SFINAE_CHECKER_HELPLER(has_JEParseFromScriptType, &T::JEParseFromScriptType);
        // void T::JEParseToScriptType(wo_vm vm, wo_value val) const
        JE_DECL_SFINAE_CHECKER_HELPLER(has_JEParseToScriptType, &T::JEParseToScriptType);

        JE_DECL_SFINAE_CHECKER_HELPLER(has_OnEnable, &T::OnEnable);
        JE_DECL_SFINAE_CHECKER_HELPLER(has_OnDisable, &T::OnDisable);

        JE_DECL_SFINAE_CHECKER_HELPLER(has_PreUpdate, &T::PreUpdate);
        JE_DECL_SFINAE_CHECKER_HELPLER(has_StateUpdate, &T::StateUpdate);
        JE_DECL_SFINAE_CHECKER_HELPLER(has_Update, &T::Update);
        JE_DECL_SFINAE_CHECKER_HELPLER(has_PhysicsUpdate, &T::PhysicsUpdate);
        JE_DECL_SFINAE_CHECKER_HELPLER(has_TransformUpdate, &T::TransformUpdate);
        JE_DECL_SFINAE_CHECKER_HELPLER(has_LateUpdate, &T::LateUpdate);
        JE_DECL_SFINAE_CHECKER_HELPLER(has_CommitUpdate, &T::CommitUpdate);
        JE_DECL_SFINAE_CHECKER_HELPLER(has_GraphicUpdate, &T::GraphicUpdate);

        JE_DECL_SFINAE_CHECKER_HELPLER(has__select_begin, &T::_select_begin);
        JE_DECL_SFINAE_CHECKER_HELPLER(has__select_continue, &T::_select_continue);

        template <typename T>
        constexpr bool sfinae_is_game_system_v =
            typing::sfinae_has__select_begin<T>::value &&
            typing::sfinae_has__select_continue<T>::value;

#undef JE_DECL_SFINAE_CHECKER_HELPLER
    }

    /*
    jeecs::basic [命名空间]
    用于存放引擎的基本工具
    */
    namespace basic
    {
        /*
        jeecs::basic::singleton [类型]
        用于管理系统之间共享的单例实例，使用acquire方法获取和创建reference
            * 当最后一个reference析构时，单例实例会被释放
            * 首次（或之前的单例释放之后的首次）获取时，会调用默认构造函数创建实例
        */
        template <typename T>
        class singleton
        {
            std::shared_mutex
                m_singleton_mutex;
            T* m_instance;
            size_t m_ref_count;

            JECS_DISABLE_MOVE_AND_COPY(singleton);

        public:
            singleton()
                : m_instance(nullptr), m_ref_count(0)
            {
            }
            ~singleton()
            {
                assert(m_instance == nullptr);
                assert(m_ref_count == 0);
            }

            class reference
            {
                singleton* m_singleton;
                T* m_instance;

            public:
                reference(singleton* s, T* inst)
                    : m_singleton(s), m_instance(inst)
                {
                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);
                }
                reference(const reference& r)
                    : m_singleton(r.m_singleton), m_instance(r.m_instance)
                {
                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);

                    std::lock_guard g(m_singleton->m_singleton_mutex);
                    m_singleton->m_ref_count++;
                }
                reference(reference&& mr)
                    : m_singleton(mr.m_singleton), m_instance(mr.m_instance)
                {
                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);

                    mr.m_singleton = nullptr;
                    mr.m_instance = nullptr;
                }

                reference& operator=(const reference& r)
                {
                    m_singleton = r.m_singleton;
                    m_instance = r.m_instance;

                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);

                    std::lock_guard g(m_singleton->m_singleton_mutex);
                    m_singleton->m_ref_count++;

                    return *this;
                }
                reference& operator=(reference&& mr)
                {
                    m_singleton = mr.m_singleton;
                    m_instance = mr.m_instance;

                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);

                    mr.m_singleton = nullptr;
                    mr.m_instance = nullptr;

                    return *this;
                }

                void lock()
                {
                    m_singleton->m_singleton_mutex.lock();
                }
                bool try_lock()
                {
                    return m_singleton->m_singleton_mutex.try_lock();
                }
                void unlock()
                {
                    m_singleton->m_singleton_mutex.unlock();
                }

                void lock_shared()
                {
                    m_singleton->m_singleton_mutex.lock_shared();
                }
                bool try_lock_shared()
                {
                    return m_singleton->m_singleton_mutex.try_lock_shared();
                }
                void unlock_shared()
                {
                    m_singleton->m_singleton_mutex.unlock_shared();
                }

                ~reference()
                {
                    if (m_singleton != nullptr)
                    {
                        assert(m_instance == m_singleton->m_instance);
                        m_singleton->_release();
                    }
                }

                T* operator->()
                {
                    return m_instance;
                }
                T* get()
                {
                    return m_instance;
                }
            };

            reference acquire()
            {
                std::unique_lock g(m_singleton_mutex);

                if (0 == m_ref_count++)
                {
                    // Create instance.
                    m_instance = new T();
                }

                g.unlock();
                return reference(this, m_instance);
            }
            void _release()
            {
                std::lock_guard g(m_singleton_mutex);
                if (0 == --m_ref_count)
                {
                    delete m_instance;
                    m_instance = nullptr;
                }
            }
        };

        /*
        jeecs::basic::vector [类型]
        用于存放大小可变的连续存储容器
            * 为了保证模块之间的二进制一致性，公共组件中请不要使用std::vector
        */
        template <typename ElemT>
        class vector
        {
            ElemT* _elems_ptr_begin = nullptr;
            ElemT* _elems_ptr_end = nullptr;
            ElemT* _elems_buffer_end = nullptr;

            static constexpr size_t _single_elem_size = sizeof(ElemT);

            inline static size_t _move(ElemT* to_begin, ElemT* from_begin, ElemT* from_end) noexcept
            {
                for (ElemT* origin_elem = from_begin; origin_elem < from_end; ++origin_elem)
                {
                    new (to_begin++) ElemT(std::move(*origin_elem));
                    origin_elem->~ElemT();
                }
                return (size_t)(from_end - from_begin);
            }
            inline static size_t _r_move(ElemT* to_begin, ElemT* from_begin, ElemT* from_end) noexcept
            {
                for (ElemT* origin_elem = from_end; origin_elem > from_begin;)
                {
                    size_t offset = (size_t)((--origin_elem) - from_begin);
                    new (to_begin + offset) ElemT(std::move(*origin_elem));
                    origin_elem->~ElemT();
                }
                return (size_t)(from_end - from_begin);
            }
            inline static size_t _copy(ElemT* to_begin, ElemT* from_begin, ElemT* from_end) noexcept
            {
                for (ElemT* origin_elem = from_begin; origin_elem < from_end;)
                {
                    new (to_begin++) ElemT(*(origin_elem++));
                }

                return (size_t)(from_end - from_begin);
            }
            inline static size_t _erase(ElemT* from_begin, ElemT* from_end) noexcept
            {
                if constexpr (!std::is_trivial<ElemT>::value)
                {
                    for (ElemT* origin_elem = from_begin; origin_elem < from_end;)
                    {
                        (origin_elem++)->~ElemT();
                    }
                }

                return (size_t)(from_end - from_begin);
            }
            inline void _reserve(size_t elem_reserving_count)
            {
                const size_t _pre_reserved_count = (size_t)(_elems_buffer_end - _elems_ptr_begin);
                if (elem_reserving_count > _pre_reserved_count)
                {
                    ElemT* new_reserved_begin = (ElemT*)je_mem_alloc(elem_reserving_count * _single_elem_size);
                    _elems_buffer_end = new_reserved_begin + elem_reserving_count;

                    _elems_ptr_end = new_reserved_begin + _move(new_reserved_begin, _elems_ptr_begin, _elems_ptr_end);
                    je_mem_free(_elems_ptr_begin);
                    _elems_ptr_begin = new_reserved_begin;
                }
            }
            inline void _assure(size_t assure_sz)
            {
                if (assure_sz > reserved_size())
                    _reserve(2 * assure_sz);
            }

        public:
            vector() noexcept
            {
            }
            ~vector() noexcept
            {
                clear();
                je_mem_free(_elems_ptr_begin);
            }

            vector(const vector& another_list) noexcept
            {
                _reserve(another_list.size());
                _elems_ptr_end += _copy(_elems_ptr_begin, another_list.begin(), another_list.end());
            }
            vector(const std::initializer_list<ElemT>& another_list) noexcept
            {
                for (auto& elem : another_list)
                    push_back(elem);
            }
            vector(ElemT* ptr, size_t length) noexcept
            {
                _elems_ptr_begin = ptr;
                _elems_ptr_end = _elems_buffer_end = _elems_ptr_begin + length;
            }
            vector(vector&& another_list) noexcept
            {
                _elems_ptr_begin = another_list._elems_ptr_begin;
                _elems_ptr_end = another_list._elems_ptr_end;
                _elems_buffer_end = another_list._elems_buffer_end;

                another_list._elems_ptr_begin =
                    another_list._elems_ptr_end =
                    another_list._elems_buffer_end = nullptr;
            }

            inline vector& operator=(const vector& another_list) noexcept
            {
                _reserve(another_list.size());
                _elems_ptr_end += _copy(_elems_ptr_begin, another_list.begin(), another_list.end());

                return *this;
            }
            inline vector& operator=(vector&& another_list) noexcept
            {
                clear();
                je_mem_free(_elems_ptr_begin);

                _elems_ptr_begin = another_list._elems_ptr_begin;
                _elems_ptr_end = another_list._elems_ptr_end;
                _elems_buffer_end = another_list._elems_buffer_end;

                another_list._elems_ptr_begin =
                    another_list._elems_ptr_end =
                    another_list._elems_buffer_end = nullptr;

                return *this;
            }

            inline size_t size() const noexcept
            {
                return _elems_ptr_end - _elems_ptr_begin;
            }
            inline bool empty() const noexcept
            {
                return _elems_ptr_end == _elems_ptr_begin;
            }
            inline ElemT& front() noexcept
            {
                return *_elems_ptr_begin;
            }
            inline ElemT& back() noexcept
            {
                return *(_elems_ptr_end - 1);
            }
            inline size_t reserved_size() const noexcept
            {
                return _elems_buffer_end - _elems_ptr_begin;
            }
            inline void clear() noexcept
            {
                _erase(_elems_ptr_begin, _elems_ptr_end);
                _elems_ptr_end = _elems_ptr_begin;
            }
            inline void push_front(const ElemT& _e) noexcept
            {
                _assure(size() + 1);
                _r_move(_elems_ptr_begin + 1, _elems_ptr_begin, _elems_ptr_end++);
                new (_elems_ptr_begin) ElemT(_e);
            }
            inline void push_back(const ElemT& _e) noexcept
            {
                _assure(size() + 1);
                new (_elems_ptr_end++) ElemT(_e);
            }
            inline void pop_back() noexcept
            {
                if constexpr (!std::is_trivial<ElemT>::value)
                    (_elems_ptr_end--)->~ElemT();
                else
                    _elems_ptr_end--;
            }
            inline auto begin() const noexcept -> ElemT*
            {
                return _elems_ptr_begin;
            }
            inline auto end() const noexcept -> ElemT*
            {
                return _elems_ptr_end;
            }
            inline auto front() const noexcept -> ElemT&
            {
                return *_elems_ptr_begin;
            }
            inline auto back() const noexcept -> ElemT&
            {
                return *(_elems_ptr_end - 1);
            }
            inline void erase(size_t index) noexcept
            {
                _elems_ptr_begin[index].~ElemT();
                _move(_elems_ptr_begin + index, _elems_ptr_begin + index + 1, _elems_ptr_end--);
            }
            inline void erase(ElemT* index) noexcept
            {
                index->~ElemT();
                _move(index, index + 1, _elems_ptr_end--);
            }
            inline void erase_data(const ElemT& data) noexcept
            {
                auto fnd_place = std::find(begin(), end(), data);
                if (fnd_place != end())
                    erase(fnd_place - begin());
            }
            ElemT* data() const noexcept
            {
                return _elems_ptr_begin;
            }
            ElemT& at(size_t index) const noexcept
            {
                return _elems_ptr_begin[index];
            }
            ElemT& operator[](size_t index) const noexcept
            {
                return _elems_ptr_begin[index];
            }
        };

        /*
        jeecs::basic::map [类型]
        用于存放大小可变的唯一键值对
            * 为了保证模块之间的二进制一致性，公共组件中请不要使用std::map
        */
        template <typename KeyT, typename ValT>
        class map
        {
            struct pair
            {
                KeyT k;
                ValT v;
            };
            basic::vector<pair> dats;

        public:
            ValT& operator[](const KeyT& k) noexcept
            {
                auto* fnd = find(k);
                if (fnd == dats.end())
                {
                    dats.push_back({ k, {} });
                    return dats.back().v;
                }
                return fnd->v;
            }
            void clear() noexcept
            {
                dats.clear();
            }
            pair* find(const KeyT& k) const noexcept
            {
                return std::find_if(dats.begin(), dats.end(), [&k](pair& p)
                    { return p.k == k; });
            }
            bool erase(const KeyT& k)
            {
                auto* fnd = find(k);
                if (fnd != end())
                {
                    dats.erase(fnd);
                    return true;
                }
                return false;
            }
            inline auto begin() const noexcept -> pair*
            {
                return dats.begin();
            }
            inline auto end() const noexcept -> pair*
            {
                return dats.end();
            }
            inline size_t size() const
            {
                return dats.size();
            }
        };

        /*
        jeecs::basic::string [类型]
        用于存放字符串
            * 为了保证模块之间的二进制一致性，公共组件中请不要使用std::string
        */
        class string
        {
            char* _c_str = nullptr;
            size_t _str_len = 0;
            size_t _buf_len = 0;

            inline void _reserve(size_t buf_sz)
            {
                if (buf_sz > _buf_len)
                {
                    _buf_len = buf_sz + 1;
                    _c_str = (char*)je_mem_realloc(_c_str, _buf_len);
                }
            }

        public:
            ~string()
            {
                if (_c_str != nullptr)
                    je_mem_free(_c_str);
            }

            string() noexcept
            {
                _reserve(1);
            }

            string(const string& str) noexcept
                : string(str.c_str())
            {
            }

            string(const std::string& str) noexcept
                : string(str.c_str())
            {
            }

            string(string&& str) noexcept
                : _c_str(str._c_str)
                , _str_len(str._str_len)
                , _buf_len(str._buf_len)
            {
                str._c_str = 0;
                str._buf_len = 0;
                str._str_len = 0;
            }

            string(const char* str) noexcept
                : _str_len(strlen(str))
            {
                _reserve(_str_len + 1);
                memcpy(_c_str, str, _str_len);
            }

            inline bool operator==(const string& str) noexcept
            {
                return 0 == strcmp(c_str(), str.c_str());
            }
            inline bool operator!=(const string& str) noexcept
            {
                return 0 != strcmp(c_str(), str.c_str());
            }
            inline string& operator=(const string& str) noexcept
            {
                return *this = str.c_str();
            }
            inline string& operator=(string&& str) noexcept
            {
                je_mem_free(_c_str);

                _c_str = str._c_str;
                _buf_len = str._buf_len;
                _str_len = str._str_len;

                str._c_str = 0;
                str._buf_len = 0;
                str._str_len = 0;

                return *this;
            }
            inline string& operator=(const std::string& str)
            {
                return *this = str.c_str();
            }
            inline string& operator=(const char* str)
            {
                _reserve((_str_len = strlen(str)) + 1);
                memcpy(_c_str, str, _str_len);
                return *this;
            }

            size_t size() const
            {
                return _str_len;
            }
            const char* c_str() const
            {
                _c_str[_str_len] = 0;
                return _c_str;
            }
            std::string cpp_str() const
            {
                return c_str();
            }
        };

        constexpr typing::typehash_t prime = (typing::typehash_t)0x100000001B3ull;
        constexpr typing::typehash_t basis = (typing::typehash_t)0xCBF29CE484222325ull;

        /*
        jeecs::basic::hash_compile_time [函数]
        可在编译时计算字符串的哈希值的哈希函数
        */
        constexpr typing::typehash_t hash_compile_time(char const* str, typing::typehash_t last_value = basis)
        {
            return *str ? hash_compile_time(str + 1, (*str ^ last_value) * prime) : last_value;
        }

        /*
        jeecs::basic::allign_size [函数]
        用于计算满足对齐要求的下一个起始偏移量位置
        */
        constexpr static size_t allign_size(size_t _origin_sz, size_t _allign_base)
        {
            size_t aligned_sz = (_origin_sz / _allign_base) * _allign_base;
            if (aligned_sz == _origin_sz)
                return aligned_sz;
            return aligned_sz + _allign_base;
        }

        template <typename T, typename... ArgTs>
        T* create_new(ArgTs &&...args)
        {
            static_assert(!std::is_void<T>::value);

            return new (je_mem_alloc(sizeof(T))) T(args...);
        }
        template <typename T, typename... ArgTs>
        T* create_new_n(size_t n)
        {
            static_assert(!std::is_void<T>::value);

            return new (je_mem_alloc(sizeof(T) * n)) T[n];
        }

        template <typename T>
        void destroy_free(T* address)
        {
            static_assert(!std::is_void<T>::value);

            address->~T();
            je_mem_free(address);
        }
        template <typename T>
        void destroy_free_n(T* address, size_t n)
        {
            static_assert(!std::is_void<T>::value);

            for (size_t i = 0; i < n; i++)
                address[i].~T();

            je_mem_free(address);
        }

        inline char* make_new_string(const char* _str)
        {
            size_t str_length = strlen(_str);
            char* str = (char*)je_mem_alloc(str_length + 1);
            memcpy(str, _str, str_length + 1);

            return str;
        }
        inline char* make_new_string(const std::string& _str)
        {
            return make_new_string(_str.c_str());
        }
        inline char* make_new_string(const basic::string& _str)
        {
            return make_new_string(_str.c_str());
        }

        inline std::string make_cpp_string(const char* _str)
        {
            std::string str = _str;
            je_mem_free((void*)_str);
            return str;
        }

        template <typename NodeT>
        struct atomic_list
        {
            std::atomic<NodeT*> last_node = nullptr;

            void add_one(NodeT* node) noexcept
            {
                NodeT* last_last_node = last_node.load(); // .exchange(node);
                do
                {
                    node->last = last_last_node;
                } while (!last_node.compare_exchange_weak(last_last_node, node));
            }

            NodeT* pick_all() noexcept
            {
                NodeT* result = nullptr;
                result = last_node.exchange(nullptr);

                return result;
            }

            NodeT* peek() const noexcept
            {
                return last_node;
            }

            bool empty() const noexcept
            {
                return last_node == nullptr;
            }
        };

        template <typename T>
        struct default_functions
        {
            template <typename W>
            struct _true_type : public std::true_type
            {
            };

            template <typename U>
            struct has_pointer_typeinfo_constructor_function
            {
                template <typename V>
                static auto _tester(int) -> _true_type<decltype(new V(std::declval<void*>(), std::declval<const jeecs::typing::type_info*>()))>;

                template <typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };
            template <typename U>
            struct has_pointer_constructor_function
            {
                template <typename V>
                static auto _tester(int) -> _true_type<decltype(new V(std::declval<void*>()))>;

                template <typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };
            template <typename U>
            struct has_typeinfo_constructor_function
            {
                template <typename V>
                static auto _tester(int) -> _true_type<decltype(new V(std::declval<const jeecs::typing::type_info*>()))>;

                template <typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };
            template <typename U>
            struct has_default_constructor_function
            {
                template <typename V>
                static auto _tester(int) -> _true_type<decltype(new V())>;

                template <typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };

            static void constructor(void* _ptr, void* arg_ptr, const jeecs::typing::type_info* tinfo)
            {
                if constexpr (has_pointer_typeinfo_constructor_function<T>::value)
                    new (_ptr) T(arg_ptr, tinfo);
                else if constexpr (has_pointer_constructor_function<T>::value)
                    new (_ptr) T(arg_ptr);
                else if constexpr (has_typeinfo_constructor_function<T>::value)
                    new (_ptr) T(tinfo);
                else
                {
                    static_assert(has_default_constructor_function<T>::value);
                    new (_ptr) T{};
                }
            }
            static void destructor(void* _ptr)
            {
                ((T*)_ptr)->~T();
            }
            static void copier(void* _ptr, const void* _be_copy_ptr)
            {
                if constexpr (std::is_copy_constructible<T>::value)
                    new (_ptr) T(*(const T*)_be_copy_ptr);
                else
                    debug::logerr("This type: '%s' is not copy-constructible but you try to do it.", typeid(T).name());
            }
            static void mover(void* _ptr, void* _be_moved_ptr)
            {
                if constexpr (std::is_move_constructible<T>::value)
                    new (_ptr) T(std::move(*(T*)_be_moved_ptr));
                else
                    debug::logerr("This type: '%s' is not move-constructible but you try to do it.", typeid(T).name());
            }
            static void on_enable(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T> && typing::sfinae_has_OnEnable<T>::value)
                {
                    reinterpret_cast<T*>(_ptr)->OnEnable();
                }
            }
            static void on_disable(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T> && typing::sfinae_has_OnDisable<T>::value)
                {
                    reinterpret_cast<T*>(_ptr)->OnDisable();
                }
            }
            static void pre_update(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T>)
                {
                    T* sys = reinterpret_cast<T*>(_ptr);
                    sys->_select_begin();

                    if constexpr (typing::sfinae_has_PreUpdate<T>::value)
                        sys->PreUpdate(sys->_select_continue());
                }
            }
            static void state_update(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T>)
                {
                    if constexpr (typing::sfinae_has_StateUpdate<T>::value)
                    {
                        T* sys = reinterpret_cast<T*>(_ptr);
                        sys->StateUpdate(sys->_select_continue());
                    }
                }
            }
            static void update(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T>)
                {
                    if constexpr (typing::sfinae_has_Update<T>::value)
                    {
                        T* sys = reinterpret_cast<T*>(_ptr);
                        sys->Update(sys->_select_continue());
                    }
                }
            }
            static void physics_update(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T>)
                {
                    if constexpr (typing::sfinae_has_PhysicsUpdate<T>::value)
                    {
                        T* sys = reinterpret_cast<T*>(_ptr);
                        sys->PhysicsUpdate(sys->_select_continue());
                    }
                }
            }
            static void transform_update(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T>)
                {
                    if constexpr (typing::sfinae_has_TransformUpdate<T>::value)
                    {
                        T* sys = reinterpret_cast<T*>(_ptr);
                        sys->TransformUpdate(sys->_select_continue());
                    }
                }
            }
            static void late_update(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T>)
                {
                    if constexpr (typing::sfinae_has_LateUpdate<T>::value)
                    {
                        T* sys = reinterpret_cast<T*>(_ptr);
                        sys->LateUpdate(sys->_select_continue());
                    }
                }
            }
            static void commit_update(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T>)
                {
                    if constexpr (typing::sfinae_has_CommitUpdate<T>::value)
                    {
                        T* sys = reinterpret_cast<T*>(_ptr);
                        sys->CommitUpdate(sys->_select_continue());
                    }
                }
            }
            static void graphic_update(void* _ptr)
            {
                if constexpr (typing::sfinae_is_game_system_v<T>)
                {
                    if constexpr (typing::sfinae_has_GraphicUpdate<T>::value)
                    {
                        T* sys = reinterpret_cast<T*>(_ptr);
                        sys->GraphicUpdate(sys->_select_continue());
                    }
                }
            }

            static void parse_from_script_type(void* _ptr, wo_vm vm, wo_value val)
            {
                if constexpr (typing::sfinae_has_JEParseFromScriptType<T>::value)
                    reinterpret_cast<T*>(_ptr)->JEParseFromScriptType(vm, val);
            }
            static void parse_to_script_type(const void* _ptr, wo_vm vm, wo_value val)
            {
                if constexpr (typing::sfinae_has_JEParseToScriptType<T>::value)
                    reinterpret_cast<const T*>(_ptr)->JEParseToScriptType(vm, val);
            }
        };

        template <typename T>
        constexpr auto type_hash()
        {
            return typeid(T).hash_code();
        }

        template <typename... ArgTs>
        struct type_index_in_varargs
        {
            template <size_t Index, typename AimT, typename CurrentT, typename... Ts>
            constexpr static size_t _index()
            {
                if constexpr (std::is_same<AimT, CurrentT>::value)
                    return Index;
                else
                    return _index<Index + 1, AimT, Ts...>();
            }

            template <typename AimArgT>
            constexpr size_t index_of() const noexcept
            {
                return _index<0, AimArgT, ArgTs...>();
            }
        };

        /*
        jeecs::basic::shared_pointer [类型]
        线程安全的共享智能指针类型
        */
        template <typename T>
        class shared_pointer
        {
            using count_t = size_t;
            using free_func_t = void (*)(T*);

            static void _default_free_func(T* ptr)
            {
                delete ptr;
            }

            T* m_resource = nullptr;
            mutable count_t* m_count = nullptr;
            free_func_t m_freer = nullptr;

            inline const static count_t* _COUNT_USING_SPIN_LOCK_MARK = (count_t*)SIZE_MAX;

            static count_t* _alloc_counter()
            {
                return create_new<count_t>(1);
            }
            static void _free_counter(count_t* p)
            {
                destroy_free(p);
            }

            count_t* _spin_lock() const
            {
                count_t* result;
                do
                {
                    result = (count_t*)je_atomic_exchange_intptr_t(
                        (intptr_t*)&m_count, (intptr_t)_COUNT_USING_SPIN_LOCK_MARK);
                } while (result == _COUNT_USING_SPIN_LOCK_MARK);

                return result;
            }
            void _spin_unlock(count_t* p) const
            {
                je_atomic_exchange_intptr_t(
                    (intptr_t*)&m_count, (intptr_t)p);
            }

            static count_t* _release_nolock_impl(T** ref_resource, free_func_t _freer, count_t* count)
            {
                if (count != nullptr)
                {
                    if (1 == je_atomic_fetch_sub_size_t(count, 1))
                    {
                        _freer(*ref_resource);
                        _free_counter(count);
                        *ref_resource = nullptr;
                        count = nullptr;
                    }
                }
                return count;
            }

            // This function not lock!
            count_t* _release_nolock(count_t* count)
            {
                return _release_nolock_impl(&m_resource, m_freer, count);
            }
            count_t* _move_nolock(count_t* count, shared_pointer* out_ptr)
            {
                out_ptr->m_resource = m_resource;
                out_ptr->m_freer = m_freer;
                m_resource = nullptr;
                return nullptr;
            }
            count_t* _borrow_nolocks(count_t* count, shared_pointer* out_ptr) const
            {
                out_ptr->m_resource = m_resource;
                out_ptr->m_freer = m_freer;
                if (count != nullptr)
                    je_atomic_fetch_add_size_t(count, 1);

                return count;
            }

        public:
            ~shared_pointer()
            {
                _release_nolock((count_t*)je_atomic_fetch_intptr_t((intptr_t*)&m_count));
            }

            shared_pointer() = delete;
            explicit shared_pointer(T* v, void (*f)(T*) = &_default_free_func)
                : m_resource(v)
                , m_count(_alloc_counter())
                , m_freer(f)
            {
                assert(m_resource != nullptr);
            }
            shared_pointer(const shared_pointer& v) noexcept
            {
                auto* counter = v._spin_lock();
                auto* unlocker = v._borrow_nolocks(counter, this);
                je_atomic_store_intptr_t((intptr_t*)&m_count, (intptr_t)counter);
                v._spin_unlock(unlocker);
            }
            shared_pointer(shared_pointer&& v) noexcept
            {
                auto* counter = v._spin_lock();
                auto* unlocker = v._move_nolock(counter, this);
                je_atomic_store_intptr_t((intptr_t*)&m_count, (intptr_t)counter);
                v._spin_unlock(unlocker);
            }
            shared_pointer& operator=(const shared_pointer& v) noexcept
            {
                if (this != &v)
                {
                    _release_nolock(_spin_lock());
                    auto* counter = v._spin_lock();

                    auto* unlocker = v._borrow_nolocks(counter, this);

                    _spin_unlock(counter);
                    v._spin_unlock(unlocker);
                }
                return *this;
            }
            shared_pointer& operator=(shared_pointer&& v) noexcept
            {
                if (this != &v)
                {
                    _release_nolock(_spin_lock());
                    auto* counter = v._spin_lock();

                    auto* unlocker = v._move_nolock(counter, this);

                    _spin_unlock(counter);
                    v._spin_unlock(unlocker);
                }
                return *this;
            }

            T* get() const
            {
                return m_resource;
            }
            T* operator->() const noexcept
            {
                return m_resource;
            }
            T& operator*() const noexcept
            {
                return *m_resource;
            }

            bool operator==(const T* ptr) const
            {
                return m_resource == ptr;
            }
            bool operator!=(const T* ptr) const
            {
                return m_resource != ptr;
            }
        };

        template <typename T>
        class optional
        {
            union
            {
                char takeplace;
                T storage;
            };

            bool has_constructed;

        public:
            ~optional()
            {
                if (has_constructed)
                    storage.~T();
            }

            optional() noexcept
                : takeplace(0), has_constructed(false)
            {
            }
            optional(const std::nullopt_t&) noexcept
                : takeplace(0), has_constructed(false)
            {
            }
            optional(const std::optional<T>& opt) noexcept
                : has_constructed(opt.has_value())
            {
                if (has_constructed)
                    new (&storage) T(opt.value());
            }
            optional(const T& v) noexcept
                : has_constructed(true)
            {
                new (&storage) T(v);
            }
            optional(T&& v) noexcept
                : has_constructed(true)
            {
                new (&storage) T(std::move(v));
            }
            optional(optional&& another) noexcept
                : has_constructed(another.has_constructed)
            {
                if (has_constructed)
                {
                    new (&storage) T(std::move(another.storage));
                    another.reset();
                }
            }
            optional(const optional& another) noexcept
                : has_constructed(another.has_constructed)
            {
                if (has_constructed)
                {
                    new (&storage) T(another.storage);
                }
            }
            optional& operator=(optional&& another) noexcept
            {
                if (this != &another)
                {
                    if (has_constructed)
                        storage.~T();

                    has_constructed = another.has_constructed;
                    if (has_constructed)
                    {
                        new (&storage) T(std::move(another.storage));
                        another.storage.~T();
                        another.has_constructed = false;
                    }
                }
                return *this;
            }
            optional& operator=(const optional& another) noexcept
            {
                if (this != &another)
                {
                    if (has_constructed)
                        storage.~T();

                    has_constructed = another.has_constructed;
                    if (has_constructed)
                    {
                        new (&storage) T(another.storage);
                    }
                }
                return *this;
            }

            bool has_value() const noexcept
            {
                return has_constructed;
            }
            T& value() noexcept
            {
                if (!has_constructed)
                {
                    jeecs::debug::logfatal(
                        "jeecs::basic::optional: value is not constructed, you should call emplace() first.");
                    abort();
                }
                return storage;
            }
            const T& value() const noexcept
            {
                if (!has_constructed)
                {
                    jeecs::debug::logfatal(
                        "jeecs::basic::optional: value is not constructed, you should call emplace() first.");
                    abort();
                }
                return storage;
            }
            void reset() noexcept
            {
                if (has_constructed)
                {
                    storage.~T();
                    has_constructed = false;
                }
            }
            template <typename... Args>
            void emplace(Args &&...args) noexcept
            {
                if (has_constructed)
                    storage.~T();
                else
                    has_constructed = true;
                new (&storage) T(std::forward<Args>(args)...);
            }

            T* operator->() noexcept
            {
                return &value();
            }
            const T* operator->() const noexcept
            {
                return &value();
            }
        };

        /*
        jeecs::basic::resource [类型别名]
        智能指针的类型别名，一般用于保管需要共享和自动管理的资源类型
        */
        template <typename T>
        using resource = shared_pointer<T>;

        /*
        jeecs::basic::fileresource [类型]
        文件资源包装类型，用于组件内的成员变量
            * 类型T应该有 load 方法以创建和返回自身
            * 类型T如果是void，那么相当于只读取文件名
        */
        template <typename T>
        class fileresource
        {
            struct file_content_t
            {
                basic::string m_path;
                basic::resource<T> m_resource;
            };
            std::optional<file_content_t> m_file;

        public:
            bool load(const std::string& path)
            {
                clear();
                if (path != "")
                {
                    auto res = T::load(path);
                    if (res.has_value())
                    {
                        m_file.emplace(
                            file_content_t{
                                path,
                                res.value(),
                            });
                        return true;
                    }
                    return false;
                }
                return true;
            }
            void set_resource(const basic::resource<T>& res)
            {
                m_file.emplace(
                    file_content_t{
                        "<builtin>",
                        res,
                    });
            }
            bool has_resource() const
            {
                return m_file.has_value();
            }
            std::optional<basic::resource<T>> get_resource() const
            {
                if (m_file.has_value())
                    return m_file->m_resource;

                return std::nullopt;
            }
            std::optional<std::string> get_path() const
            {
                if (m_file.has_value())
                    return m_file->m_path.cpp_str();

                return std::nullopt;
            }
            void clear()
            {
                m_file.reset();
            }
        };

        template <>
        class fileresource<void>
        {
            struct file_content_t
            {
                basic::string m_path;
            };
            std::optional<file_content_t> m_file;

        public:
            bool load(const std::string& path)
            {
                clear();
                if (path != "")
                {
                    m_file.emplace(
                        file_content_t{
                            path,
                        });
                }
                return true;
            }
            std::optional<std::string> get_path() const
            {
                if (m_file.has_value())
                    return m_file->m_path.cpp_str();

                return std::nullopt;
            }
            bool has_resource() const
            {
                return m_file.has_value();
            }
            void clear()
            {
                m_file.reset();
            }
        };
    }

    namespace typing
    {
        inline uuid uuid::generate() noexcept
        {
            uuid u;
            je_uid_generate(&u);
            return u;
        }

        /*
        jeecs::typing::typeinfo_member [类型]
        用于储存组件的成员信息
        */
        struct typeinfo_member
        {
            struct member_info
            {
                const type_info* m_class_type;

                const char* m_member_name;

                const char* m_woovalue_type_may_null;
                wo_pin_value m_woovalue_init_may_null;

                const type_info* m_member_type;
                ptrdiff_t m_member_offset;

                member_info* m_next_member;
            };

            size_t m_member_count;
            member_info* m_members;
        };

        /*
        jeecs::typing::typeinfo_script_parser [类型]
        用于储存与woolang进行转换的方法和类型信息
        */
        struct typeinfo_script_parser
        {
            parse_c2w_func_t m_script_parse_c2w;
            parse_w2c_func_t m_script_parse_w2c;
            const char* m_woolang_typename;
            const char* m_woolang_typedecl;
        };

        /*
        jeecs::typing::typeinfo_system_updater [类型]
        用于储存系统的更新方法
        */
        struct typeinfo_system_updater
        {
            on_enable_or_disable_func_t m_on_enable;
            on_enable_or_disable_func_t m_on_disable;

            update_func_t m_pre_update;
            update_func_t m_state_update;
            update_func_t m_update;
            update_func_t m_physics_update;
            update_func_t m_transform_update;
            update_func_t m_late_update;
            update_func_t m_commit_update;
            update_func_t m_graphic_update;
        };

        class type_unregister_guard
        {
            friend struct type_info;

            using id_typeinfo_map_t = std::unordered_map<
                jeecs::typing::typeid_t,
                const jeecs::typing::type_info*>;
            using registered_type_hash_map_t = std::unordered_map<
                jeecs::typing::typehash_t,
                jeecs::typing::typeid_t>;

            mutable std::mutex _m_mx;

            id_typeinfo_map_t _m_self_registed_id_typeinfo;
            registered_type_hash_map_t _m_self_registed_hash;

        public:
            type_unregister_guard() = default;

            ~type_unregister_guard()
            {
                assert(_m_self_registed_id_typeinfo.empty());
            }
            template <typename T>
            bool _register_or_get_local_type_info(const char* _typename, const type_info** out_typeinfo)
            {
                do
                {
                    std::lock_guard g1(_m_mx);
                    auto fnd = _m_self_registed_hash.find(typeid(T).hash_code());
                    if (fnd != _m_self_registed_hash.end())
                    {
                        *out_typeinfo = _m_self_registed_id_typeinfo.at(fnd->second);
                        return false;
                    }
                } while (0);

                bool is_basic_type = false;
                if (nullptr == _typename)
                {
                    // If _typename is nullptr, we will not treat it as a component
                    _typename = typeid(T).name();
                    is_basic_type = true;
                }

                je_typing_class current_type =
                    is_basic_type
                    ? je_typing_class::JE_BASIC_TYPE
                    : (std::is_base_of<game_system, T>::value
                        ? je_typing_class::JE_SYSTEM
                        : je_typing_class::JE_COMPONENT);

                // store to list for unregister
                auto* local_type_info = je_typing_register(
                    _typename,
                    basic::type_hash<T>(),
                    sizeof(T),
                    alignof(T),
                    current_type,
                    basic::default_functions<T>::constructor,
                    basic::default_functions<T>::destructor,
                    basic::default_functions<T>::copier,
                    basic::default_functions<T>::mover);

                do
                {
                    std::lock_guard g1(_m_mx);

                    assert(_m_self_registed_id_typeinfo.find(local_type_info->m_id) == _m_self_registed_id_typeinfo.end());
                    _m_self_registed_id_typeinfo[local_type_info->m_id] = local_type_info;
                    _m_self_registed_hash[typeid(T).hash_code()] = local_type_info->m_id;

                } while (0);

                *out_typeinfo = local_type_info;
                return true;
            }

            void unregister_all_types()
            {
                std::lock_guard g1(_m_mx);
                for (auto& [_, local_type_info] : _m_self_registed_id_typeinfo)
                    je_typing_unregister(local_type_info);
                _m_self_registed_id_typeinfo.clear();
                _m_self_registed_hash.clear();
            }
            const jeecs::typing::type_info* get_local_type_info(jeecs::typing::typeid_t id) const
            {
                std::lock_guard g1(_m_mx);
                return _m_self_registed_id_typeinfo.at(id);
            }
        };

        /*
        jeecs::typing::type_info [类型]
        用于储存类型信息和基本接口
        */
        struct type_info
        {
            typeid_t m_id;

            const char* m_typename; // will be free by je_typing_unregister
            size_t m_size;
            size_t m_align;
            size_t m_chunk_size; // calc by je_typing_register
            typehash_t m_hash;

            construct_func_t m_constructor;
            destruct_func_t m_destructor;
            copy_construct_func_t m_copier;
            move_construct_func_t m_mover;

            je_typing_class m_type_class;

            const typeinfo_member* m_member_types;
            const typeinfo_script_parser* m_script_parsers;
            const typeinfo_system_updater* m_system_updaters;

            const type_info* m_next;

        public:
            template <typename T>
            inline static const type_info* of()
            {
                return je_typing_get_info_by_hash(typeid(T).hash_code());
            }
            inline static const type_info* of(typeid_t _tid)
            {
                return je_typing_get_info_by_id(_tid);
            }
            inline static const type_info* of(const char* name)
            {
                return je_typing_get_info_by_name(name);
            }

            template <typename T>
            inline static typeid_t id()
            {
                return of<T>()->m_id;
            }
            inline static typeid_t id(typeid_t _tid)
            {
                return of(_tid)->m_id;
            }
            inline static typeid_t id(const char* name)
            {
                return of(name)->m_id;
            }

            template <typename T>
            inline static const type_info* register_type(
                jeecs::typing::type_unregister_guard* guard, const char* _typename)
            {
                const type_info* local_type = nullptr;
                if (guard->_register_or_get_local_type_info<T>(_typename, &local_type))
                {
                    if constexpr (sfinae_has_JERefRegsiter<T>::value)
                    {
                        if constexpr (sfinae_match_JERefRegsiter<T>::value)
                            T::JERefRegsiter(guard);
                        else
                            static_assert(sfinae_match_JERefRegsiter<T>::value,
                                "T::JERefRegsiter must be `static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)`.");
                    }
                }

                if (local_type->m_type_class == je_typing_class::JE_SYSTEM)
                {
                    je_register_system_updater(
                        local_type,
                        basic::default_functions<T>::on_enable,
                        basic::default_functions<T>::on_disable,
                        basic::default_functions<T>::pre_update,
                        basic::default_functions<T>::state_update,
                        basic::default_functions<T>::update,
                        basic::default_functions<T>::physics_update,
                        basic::default_functions<T>::transform_update,
                        basic::default_functions<T>::late_update,
                        basic::default_functions<T>::commit_update,
                        basic::default_functions<T>::graphic_update);
                }

                if constexpr (sfinae_has_JEScriptTypeName<T>::value &&
                    sfinae_has_JEScriptTypeDeclare<T>::value &&
                    sfinae_has_JEParseFromScriptType<T>::value &&
                    sfinae_has_JEParseToScriptType<T>::value)
                {
                    je_register_script_parser(local_type,
                        basic::default_functions<T>::parse_to_script_type,
                        basic::default_functions<T>::parse_from_script_type,
                        T::JEScriptTypeName(),
                        T::JEScriptTypeDeclare());
                }

                return local_type;
            }

            void construct(void* addr, void* arg = nullptr) const
            {
                m_constructor(addr, arg, this);
            }
            void destruct(void* addr) const
            {
                m_destructor(addr);
            }
            void copy(void* dst_addr, const void* src_addr) const
            {
                m_copier(dst_addr, src_addr);
            }
            void move(void* dst_addr, void* src_addr) const
            {
                m_mover(dst_addr, src_addr);
            }

            inline bool is_system() const noexcept
            {
                return m_type_class == je_typing_class::JE_SYSTEM;
            }

            inline bool is_component() const noexcept
            {
                return m_type_class == je_typing_class::JE_COMPONENT;
            }

            inline const typeinfo_member::member_info* find_member_by_name(const char* name) const noexcept
            {
                if (m_member_types == nullptr)
                {
                    if (m_next != nullptr)
                        return m_next->find_member_by_name(name);
                }
                else
                {
                    auto* member_info_ptr = m_member_types->m_members;
                    while (member_info_ptr != nullptr)
                    {
                        if (strcmp(member_info_ptr->m_member_name, name) == 0)
                            return member_info_ptr;

                        member_info_ptr = member_info_ptr->m_next_member;
                    }
                }
                jeecs::debug::logerr("Failed to find member named: '%s' in '%s'.", name, this->m_typename);
                return nullptr;
            }
            inline const typeinfo_script_parser* get_script_parser() const
            {
                if (m_script_parsers == nullptr && m_next != nullptr)
                    return m_next->get_script_parser();

                return m_script_parsers;
            }
        };

        template <typename ClassT, typename MemberT>
        inline void register_member(
            jeecs::typing::type_unregister_guard* guard,
            ptrdiff_t member_offset,
            const char* membname)
        {
            const type_info* membt = type_info::register_type<MemberT>(guard, nullptr);
            assert(membt->m_type_class == je_typing_class::JE_BASIC_TYPE);

            je_register_member(
                guard->get_local_type_info(type_info::id<ClassT>()),
                membt,
                membname,
                nullptr,
                nullptr,
                member_offset);
        }

        template <typename ClassT, typename MemberT>
        inline void register_member(
            jeecs::typing::type_unregister_guard* guard,
            MemberT(ClassT::* _memboffset),
            const char* membname)
        {
            ptrdiff_t member_offset =
                reinterpret_cast<ptrdiff_t>(&(((ClassT*)nullptr)->*_memboffset));

            register_member<ClassT, MemberT>(guard, member_offset, membname);
        }
        template <typename T>
        inline void register_script_parser(
            jeecs::typing::type_unregister_guard* guard,
            void (*c2w)(const T*, wo_vm, wo_value),
            void (*w2c)(T*, wo_vm, wo_value),
            const std::string& woolang_typename,
            const std::string& woolang_typedecl)
        {
            const typing::type_info* local_typeinfo = nullptr;
            guard->_register_or_get_local_type_info<T>(nullptr, &local_typeinfo);

            je_register_script_parser(
                local_typeinfo,
                reinterpret_cast<jeecs::typing::parse_c2w_func_t>(c2w),
                reinterpret_cast<jeecs::typing::parse_w2c_func_t>(w2c),
                woolang_typename.c_str(),
                woolang_typedecl.c_str());
        }
    }

    class game_universe;

    class game_world
    {
        void* _m_ecs_world_addr;

    public:
        game_world(void* ecs_world_addr)
            : _m_ecs_world_addr(ecs_world_addr)
        {
        }

    private:
        friend class game_system;

    public:
        inline void* handle() const noexcept
        {
            return _m_ecs_world_addr;
        }

        template <typename FirstCompT, typename... CompTs>
        inline game_entity add_entity()
        {
            typing::typeid_t component_ids[] = {
                typing::type_info::id<FirstCompT>(),
                typing::type_info::id<CompTs>()...,
                typing::INVALID_TYPE_ID };
            game_entity gentity;
            je_ecs_world_create_entity_with_components(
                handle(), &gentity, component_ids);

            return gentity;
        }
        inline game_entity add_entity(const game_entity& prefab)
        {
            game_entity gentity;
            je_ecs_world_create_entity_with_prefab(
                handle(), &gentity, &prefab);

            return gentity;
        }
        template <typename FirstCompT, typename... CompTs>
        inline game_entity add_prefab()
        {
            typing::typeid_t component_ids[] = {
                typing::type_info::id<FirstCompT>(),
                typing::type_info::id<CompTs>()...,
                typing::INVALID_TYPE_ID };
            game_entity gentity;
            je_ecs_world_create_prefab_with_components(
                handle(), &gentity, component_ids);

            return gentity;
        }

        inline jeecs::game_system* add_system(jeecs::typing::typeid_t type)
        {
            return je_ecs_world_add_system_instance(handle(), type);
        }

        template <typename SystemT>
        inline SystemT* add_system()
        {
            return static_cast<SystemT*>(add_system(
                typing::type_info::id<SystemT>()));
        }

        inline jeecs::game_system* get_system(jeecs::typing::typeid_t type)
        {
            return je_ecs_world_get_system_instance(handle(), type);
        }

        template <typename SystemT>
        inline SystemT* get_system()
        {
            return static_cast<SystemT*>(get_system(
                typing::type_info::id<SystemT>()));
        }

        inline void remove_system(jeecs::typing::typeid_t type)
        {
            je_ecs_world_remove_system_instance(handle(), type);
        }

        template <typename SystemT>
        inline void remove_system()
        {
            remove_system(typing::type_info::id<SystemT>());
        }

        // This function only used for editor.
        inline game_entity _add_entity(std::vector<typing::typeid_t> components)
        {
            components.push_back(typing::INVALID_TYPE_ID);

            game_entity gentity;
            je_ecs_world_create_entity_with_components(
                handle(), &gentity, components.data());

            return gentity;
        }
        // This function only used for editor.
        inline game_entity _add_prefab(std::vector<typing::typeid_t> components)
        {
            components.push_back(typing::INVALID_TYPE_ID);

            game_entity gentity;
            je_ecs_world_create_prefab_with_components(
                handle(), &gentity, components.data());

            return gentity;
        }

        inline void remove_entity(const game_entity& entity)
        {
            je_ecs_world_destroy_entity(handle(), &entity);
        }

        inline operator bool() const noexcept
        {
            return handle() != nullptr;
        }

        void close() const noexcept
        {
            je_ecs_world_destroy(_m_ecs_world_addr);
        }

        bool is_valid() const noexcept
        {
            return je_ecs_world_is_valid(handle());
        }

        void set_able(bool able) const noexcept
        {
            je_ecs_world_set_able(handle(), able);
        }

        inline game_universe get_universe() const noexcept;
    };

    // Used for select the components of entities which match spcify requirements.
    struct requirement
    {
        enum type : uint8_t
        {
            CONTAINS, // Must have spcify component
            MAYNOT,  // May have or not have
            ANYOF,   // Must have one of 'ANYOF' components
            EXCEPT,  // Must not contain spcify component
        };

        type m_require;
        size_t m_require_group_id;
        typing::typeid_t m_type;

        requirement(type _require, size_t group_id, typing::typeid_t _type)
            : m_require(_require), m_require_group_id(group_id), m_type(_type)
        {
        }
    };

    struct dependence
    {
        basic::vector<requirement> m_requirements;

        // Store archtypes here?
        game_world m_world = nullptr;
        size_t m_current_arch_version = 0;

        // archs of dependences:
        struct arch_chunks_info
        {
            void* m_arch;
            size_t m_entity_count;

            /* An arch will contain a chain of chunks
            --------------------------------------
            | ArchType
            | chunk5->chunk4->chunk3->chunk2...
            --------------------------------------

            Components data buf will store at chunk' head.
            --------------------------------------
            | Chunk
            | [COMPONENT_BUFFER 64KByte] [OTHER DATAS ..Byte]
            --------------------------------------

            In buffer, components will store like this:
            --------------------------------------
            | Buffer
            | [COMPONENT_1 0 1 2...] [COMPONENT_2 0 1 2...]...
            --------------------------------------

            Each type of components will have a size, and begin-offset in buffer.
            We can use these informations to get all components to walk through.
            */
            size_t m_component_count;
            size_t* m_component_sizes;
            size_t* m_component_offsets;
        };
        basic::vector<arch_chunks_info*> m_archs;
        dependence() = default;
        dependence(const dependence& d)
            : m_requirements(d.m_requirements), m_world(d.m_world), m_current_arch_version(0), m_archs({})
        {
        }
        dependence(dependence&& d)
            : m_requirements(std::move(d.m_requirements)), m_world(std::move(d.m_world)), m_current_arch_version(d.m_current_arch_version), m_archs(std::move(d.m_archs))
        {
            d.m_current_arch_version = 0;
        }
        dependence& operator=(const dependence& d)
        {
            m_requirements = d.m_requirements;
            m_world = d.m_world;
            m_current_arch_version = 0;
            m_archs = {};

            return *this;
        }
        dependence& operator=(dependence&& d)
        {
            m_requirements = std::move(d.m_requirements);
            m_world = std::move(d.m_world);
            m_current_arch_version = d.m_current_arch_version;
            d.m_current_arch_version = 0;
            m_archs = std::move(d.m_archs);

            return *this;
        }
        ~dependence()
        {
            je_ecs_clear_dependence_archinfos(this);
        }

        void update(const game_world& aim_world) noexcept
        {
            assert(aim_world.handle() != nullptr);

            size_t arch_updated_ver = je_ecs_world_archmgr_updated_version(aim_world.handle());
            if (m_world != aim_world || m_current_arch_version != arch_updated_ver)
            {
                m_current_arch_version = arch_updated_ver;
                m_world = aim_world;
                je_ecs_world_update_dependences_archinfo(m_world.handle(), this);
            }
        }
    };

    /*
     * 早上好，这一站我们来到了选择器，JoyEngine中第二混乱的东西
     *
     * 实际上只要和ArchSystem扯上关系，就永远不可能干净。很不幸，选择器正是一根搅屎棍，它负责从
     * ArchSystem管理的区域内按照我们的需求，分离出满足我们需求的ArchType，再从上面把合法的组件
     * 一个个摘出来递到我们面前。
     *
     * 在这里——jeecs.h中，选择器的实现已经显得非常麻烦，但实际上这里只是选择器的一部分，在
     * ArchSystem中，有一个名为je_ecs_world_update_dependences_archinfo的函数。这个函数在黑暗处
     * 负责在适当的实际更新选择器的筛选结果。
     *
     * 为了优雅，背后就得承担代价；为了性能我们就得做出牺牲。伟大的圣窝窝头，这么做真的值得吗？
     *
     *                                                                   ——虔诚的窝窝头信徒
     *                                                                       mr_cino
     */
    struct selector
    {
        JECS_DISABLE_MOVE_AND_COPY(selector);
        size_t m_curstep = 0;
        size_t m_any_id = 0;
        game_world m_current_world = nullptr;

        bool m_first_time_to_work = true;
        basic::vector<dependence> m_dependence_caches;

        game_system* m_system_instance = nullptr;

    private:
        template <size_t ArgN, typename FT>
        void _apply_dependence(dependence& dep)
        {
            using f = typing::function_traits<FT>;
            if constexpr (ArgN < f::arity)
            {
                using CurRequireT = typename f::template argument<ArgN>::type;

                // NOTE: Must be here for correct order.
                _apply_dependence<ArgN + 1, FT>(dep);

                if constexpr (ArgN == 0 && std::is_same<CurRequireT, game_entity>::value)
                {
                    // First argument is game_entity, skip this argument
                }
                else if constexpr (std::is_reference<CurRequireT>::value)
                    // Reference, means CONTAINS
                    dep.m_requirements.push_front(
                        requirement(requirement::type::CONTAINS, 0,
                            typing::type_info::id<jeecs::typing::origin_t<CurRequireT>>()));
                else if constexpr (std::is_pointer<CurRequireT>::value)
                    // Pointer, means MAYNOT
                    dep.m_requirements.push_front(
                        requirement(requirement::type::MAYNOT, 0,
                            typing::type_info::id<jeecs::typing::origin_t<CurRequireT>>()));
                else
                {
                    static_assert(std::is_void<CurRequireT>::value || !std::is_void<CurRequireT>::value,
                        "'exec' of selector only accept ref or ptr type of Components.");
                }
            }
        }

        template <typename CurRequireT, typename... Ts>
        void _apply_except(dependence& dep)
        {
            static_assert(std::is_same<typing::origin_t<CurRequireT>, CurRequireT>::value);

            auto id = typing::type_info::id<CurRequireT>();

#ifndef NDEBUG
            for (const requirement& req : dep.m_requirements)
                if (req.m_type == id)
                    debug::logwarn("Repeat or conflict when excepting component '%s'.",
                        typing::type_info::of<CurRequireT>()->m_typename);
#endif
            dep.m_requirements.push_back(requirement(requirement::type::EXCEPT, 0, id));
            if constexpr (sizeof...(Ts) > 0)
                _apply_except<Ts...>(dep);
        }

        template <typename CurRequireT, typename... Ts>
        void _apply_contain(dependence& dep)
        {
            static_assert(std::is_same<typing::origin_t<CurRequireT>, CurRequireT>::value);

            auto id = typing::type_info::id<CurRequireT>();

#ifndef NDEBUG
            for (const requirement& req : dep.m_requirements)
                if (req.m_type == id)
                    debug::logwarn("Repeat or conflict when containing component '%s'.",
                        typing::type_info::of<CurRequireT>()->m_typename);
#endif
            dep.m_requirements.push_back(requirement(requirement::type::CONTAINS, 0, id));
            if constexpr (sizeof...(Ts) > 0)
                _apply_contain<Ts...>(dep);
        }

        template <typename CurRequireT, typename... Ts>
        void _apply_anyof(dependence& dep, size_t any_group)
        {
            static_assert(std::is_same<typing::origin_t<CurRequireT>, CurRequireT>::value);

            auto id = typing::type_info::id<CurRequireT>();
#ifndef NDEBUG
            for (const requirement& req : dep.m_requirements)
                if (req.m_type == id && req.m_require != requirement::type::MAYNOT)
                    debug::logwarn("Repeat or conflict when require any of component '%s'.",
                        typing::type_info::of<CurRequireT>()->m_typename);
#endif
            dep.m_requirements.push_back(requirement(requirement::type::ANYOF, any_group, id));
            if constexpr (sizeof...(Ts) > 0)
                _apply_anyof<Ts...>(dep, any_group);
        }

        template <typename FT>
        struct _executor_extracting_agent : std::false_type
        {
        };

        template <typename ComponentT, typename... ArgTs>
        struct _const_type_index
        {
            using f_t = typing::function_traits<void(ArgTs...)>;
            template <size_t id = 0>
            static constexpr size_t _index()
            {
                if constexpr (std::is_same<typename f_t::template argument<id>::type, ComponentT>::value)
                    return id;
                else
                    return _index<id + 1>();
            }
            static constexpr size_t index = _index();
        };

    public:
        inline static bool get_entity_avaliable(
            const game_entity::meta* entity_meta, size_t eid) noexcept
        {
            return jeecs::game_entity::entity_stat::READY == entity_meta[eid].m_stat;
        }

        inline static bool get_entity_avaliable_version(
            const game_entity::meta* entity_meta, size_t eid, typing::version_t* out_version) noexcept
        {
            if (jeecs::game_entity::entity_stat::READY == entity_meta[eid].m_stat)
            {
                *out_version = entity_meta[eid].m_version;
                return true;
            }
            return false;
        }
        inline static void* get_component_from_archchunk_ptr(
            dependence::arch_chunks_info* archinfo, void* chunkbuf, size_t entity_id, size_t cid)
        {
            assert(cid < archinfo->m_component_count);

            if (archinfo->m_component_sizes[cid])
            {
                size_t offset = archinfo->m_component_offsets[cid] + archinfo->m_component_sizes[cid] * entity_id;
                return reinterpret_cast<void*>(reinterpret_cast<intptr_t>(chunkbuf) + offset);
            }
            else
                return nullptr;
        }
        template <typename ComponentT, typename... ArgTs>
        inline static ComponentT get_component_from_archchunk(
            dependence::arch_chunks_info* archinfo, void* chunkbuf, size_t entity_id)
        {
            constexpr size_t cid = _const_type_index<ComponentT, ArgTs...>::index;
            auto* component_ptr = std::launder(reinterpret_cast<typename typing::origin_t<ComponentT> *>(
                get_component_from_archchunk_ptr(archinfo, chunkbuf, entity_id, cid)));

            if (component_ptr != nullptr)
            {
                if constexpr (std::is_reference<ComponentT>::value)
                    return *component_ptr;
                else
                {
                    static_assert(std::is_pointer<ComponentT>::value);
                    return component_ptr;
                }
            }

            if constexpr (std::is_reference<ComponentT>::value)
            {
                assert(("Only maynot/anyof canbe here. 'je_ecs_world_update_dependences_archinfo' may have some problem.", false));
                abort();
            }
            else
                return nullptr; // Only maynot/anyof can be here, no need to cast the type;
        }

    private:
        template <typename RT, typename... ArgTs>
        struct _executor_extracting_agent<RT(ArgTs...)> : std::true_type
        {
            template <typename FT>
            inline static void exec(dependence* depend, FT&& f, game_system* sys) noexcept
            {
                for (auto* archinfo : depend->m_archs)
                {
                    auto cur_chunk = je_arch_get_chunk(archinfo->m_arch);
                    while (cur_chunk)
                    {
                        auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(cur_chunk);
                        for (size_t eid = 0; eid < archinfo->m_entity_count; ++eid)
                        {
                            if (get_entity_avaliable(entity_meta_addr, eid))
                            {
                                if constexpr (std::is_void<typename typing::function_traits<FT>::this_t>::value)
                                    f(get_component_from_archchunk<ArgTs, ArgTs...>(archinfo, cur_chunk, eid)...);
                                else
                                    (static_cast<typename typing::function_traits<FT>::this_t*>(sys)->*f)(
                                        get_component_from_archchunk<ArgTs, ArgTs...>(archinfo, cur_chunk, eid)...);
                            }
                        }
                        cur_chunk = je_arch_next_chunk(cur_chunk);
                    }
                }
            }
        };

        template <typename RT, typename... ArgTs>
        struct _executor_extracting_agent<RT(game_entity, ArgTs...)> : std::true_type
        {
            template <typename FT>
            inline static void exec(dependence* depend, FT&& f, game_system* sys) noexcept
            {
                for (auto* archinfo : depend->m_archs)
                {
                    auto cur_chunk = je_arch_get_chunk(archinfo->m_arch);
                    while (cur_chunk)
                    {
                        auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(cur_chunk);
                        typing::version_t version;
                        for (size_t eid = 0; eid < archinfo->m_entity_count; ++eid)
                        {
                            if (get_entity_avaliable_version(entity_meta_addr, eid, &version))
                            {
                                if constexpr (std::is_void<typename typing::function_traits<FT>::this_t>::value)
                                    f(game_entity{ cur_chunk, eid, version },
                                        get_component_from_archchunk<ArgTs, ArgTs...>(archinfo, cur_chunk, eid)...);
                                else
                                    (static_cast<typename typing::function_traits<FT>::this_t*>(sys)->*f)(
                                        game_entity{ cur_chunk, eid, version },
                                        get_component_from_archchunk<ArgTs, ArgTs...>(archinfo, cur_chunk, eid)...);
                            }
                        }

                        cur_chunk = je_arch_next_chunk(cur_chunk);
                    }
                }
            }
        };

        template <typename FT>
        void _update(FT&& exec)
        {
            assert((bool)m_current_world);

            assert(m_curstep < m_dependence_caches.size());
            if (m_first_time_to_work)
            {
                // First times to execute this job or arch/world changed, register requirements
                assert(m_curstep + 1 == m_dependence_caches.size());

                dependence& dep = m_dependence_caches.back();
                _apply_dependence<0, FT>(dep);

                m_dependence_caches.push_back(dependence());
            }

            dependence& cur_dependence = m_dependence_caches[m_curstep];
            cur_dependence.update(m_current_world);

            // OK! Execute step function!
            static_assert(
                _executor_extracting_agent<typename typing::function_traits<FT>::flat_func_t>::value,
                "Fail to extract types of arguments from 'FT'.");
            _executor_extracting_agent<typename typing::function_traits<FT>::flat_func_t>::exec(
                &cur_dependence, exec, m_system_instance);

            ++m_curstep;
        }

    public:
        selector(game_system* game_sys)
            : m_system_instance(game_sys)
        {
        }

        ~selector()
        {
            m_dependence_caches.clear();
        }

        inline void select_begin(game_world w)
        {
            if (m_first_time_to_work)
            {
                if (m_dependence_caches.empty())
                    m_dependence_caches.push_back(dependence());
                else
                {
                    m_dependence_caches.erase(m_dependence_caches.end() - 1);
                    m_first_time_to_work = false;
                }
            }

            m_curstep = 0;
            m_current_world = w;
        }

        template <typename FT>
        inline void exec(FT&& _exec)
        {
            _update(_exec);
        }

        template <typename... Ts>
        inline void except() noexcept
        {
            if (m_first_time_to_work)
            {
                auto& depend = m_dependence_caches[m_curstep];
                _apply_except<Ts...>(depend);
            }
        }
        template <typename... Ts>
        inline void contains() noexcept
        {
            if (m_first_time_to_work)
            {
                auto& depend = m_dependence_caches[m_curstep];
                _apply_contain<Ts...>(depend);
            }
        }
        template <typename... Ts>
        inline void anyof() noexcept
        {
            if (m_first_time_to_work)
            {
                auto& depend = m_dependence_caches[m_curstep];
                _apply_anyof<Ts...>(depend, m_any_id);
                ++m_any_id;
            }
        }
    };

    class game_universe
    {
        void* _m_universe_addr;

    public:
        game_universe(void* universe_addr)
            : _m_universe_addr(universe_addr)
        {
        }
        inline void* handle() const noexcept
        {
            return _m_universe_addr;
        }
        inline game_world create_world()
        {
            return je_ecs_world_create(_m_universe_addr);
        }
        inline void wait() const noexcept
        {
            je_ecs_universe_loop(handle());
        }
        inline void stop() const noexcept
        {
            je_ecs_universe_stop(handle());
        }
        inline double get_real_deltatimed() const
        {
            return je_ecs_universe_get_real_deltatime(handle());
        }
        inline float get_real_deltatime() const
        {
            return (float)get_real_deltatimed();
        }
        inline double get_smooth_deltatimed() const
        {
            return je_ecs_universe_get_smooth_deltatime(handle());
        }
        inline float get_smooth_deltatime() const
        {
            return (float)get_smooth_deltatimed();
        }
        inline double get_timescaled() const
        {
            return je_ecs_universe_get_time_scale(handle());
        }
        inline float get_timescale() const
        {
            return (float)get_timescaled();
        }
        inline void set_timescaled(double scale) const
        {
            je_ecs_universe_set_time_scale(handle(), scale);
        }
        inline void set_timescale(float scale) const
        {
            set_timescaled((double)scale);
        }

        inline operator bool() const noexcept
        {
            return _m_universe_addr;
        }

    public:
        static game_universe create_universe()
        {
            return game_universe(je_ecs_universe_create());
        }
        static void destroy_universe(game_universe universe)
        {
            return je_ecs_universe_destroy(universe.handle());
        }
    };

    class game_system
    {
        JECS_DISABLE_MOVE_AND_COPY(game_system);

    private:
        game_world _m_game_world;
        selector _m_default_selector;

    public:
        game_system(game_world world)
            : _m_game_world(world), _m_default_selector(this)
        {
        }

        inline double deltatimed() const
        {
            return _m_game_world.get_universe().get_real_deltatimed();
        }
        inline float deltatime() const
        {
            return _m_game_world.get_universe().get_real_deltatime();
        }
        inline double smooth_deltatimed() const
        {
            return _m_game_world.get_universe().get_smooth_deltatimed();
        }
        inline float smooth_deltatime() const
        {
            return _m_game_world.get_universe().get_smooth_deltatime();
        }
        inline double timescaled() const
        {
            return _m_game_world.get_universe().get_timescaled();
        }
        inline float timescale() const
        {
            return _m_game_world.get_universe().get_timescale();
        }

        // Get binded world or attached world
        inline game_world get_world() const noexcept
        {
            assert(_m_game_world);
            return _m_game_world;
        }

        // Select from default selector
        inline selector& _select_begin() noexcept
        {
            _m_default_selector.select_begin(get_world());
            return _m_default_selector;
        }
        inline selector& _select_continue() noexcept
        {
            return _m_default_selector;
        }
    };

    inline game_universe game_world::get_universe() const noexcept
    {
        return game_universe(je_ecs_world_in_universe(handle()));
    }

    template <typename T>
    inline T* game_entity::get_component() const noexcept
    {
        return (T*)je_ecs_world_entity_get_component(this,
            typing::type_info::id<T>());
    }

    template <typename T>
    inline T* game_entity::add_component() const noexcept
    {
        return (T*)je_ecs_world_entity_add_component(this,
            typing::type_info::id<T>());
    }

    template <typename T>
    inline void game_entity::remove_component() const noexcept
    {
        return je_ecs_world_entity_remove_component(this,
            typing::type_info::id<T>());
    }

    inline jeecs::game_world game_entity::game_world() const noexcept
    {
        return jeecs::game_world(je_ecs_world_of_entity(this));
    }

    inline void game_entity::close() const noexcept
    {
        if (_m_in_chunk == nullptr)
            return;

        game_world().remove_entity(*this);
    }

    namespace math
    {
        constexpr static float PI = 3.14159265f;
        constexpr static float RAD2DEG = 180.f / PI;
        constexpr static float DEG2RAD = PI / 180.f;
        constexpr static float EPSILON = FLT_EPSILON;

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
            static std::random_device rd;
            static std::mt19937 gen(rd());

            if constexpr (std::is_floating_point<T>::value)
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
            void JEParseFromScriptType(wo_vm vm, wo_value v)
            {
                _wo_value elem;

                wo_struct_get(&elem, v, 0);
                x = wo_float(&elem);

                wo_struct_get(&elem, v, 1);
                y = wo_float(&elem);
            }
            void JEParseToScriptType(wo_vm vm, wo_value v) const
            {
                wo_set_struct(v, vm, 2);
                _wo_value elem;

                wo_set_float(&elem, x);
                wo_struct_set(v, 0, &elem);

                wo_set_float(&elem, y);
                wo_struct_set(v, 1, &elem);
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

            inline float max() const noexcept
            {
                return std::max(x, y);
            }

            inline float min() const noexcept
            {
                return std::min(x, y);
            }

            void JEParseFromScriptType(wo_vm vm, wo_value v)
            {
                _wo_value elem;

                wo_struct_get(&elem, v, 0);
                x = (int)wo_int(&elem);

                wo_struct_get(&elem, v, 1);
                y = (int)wo_int(&elem);
            }
            void JEParseToScriptType(wo_vm vm, wo_value v) const
            {
                wo_set_struct(v, vm, 2);
                _wo_value elem;

                wo_set_int(&elem, (wo_integer_t)x);
                wo_struct_set(v, 0, &elem);

                wo_set_int(&elem, (wo_integer_t)y);
                wo_struct_set(v, 1, &elem);
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
            void JEParseFromScriptType(wo_vm vm, wo_value v)
            {
                _wo_value elem;

                wo_struct_get(&elem, v, 0);
                x = wo_float(&elem);

                wo_struct_get(&elem, v, 1);
                y = wo_float(&elem);

                wo_struct_get(&elem, v, 2);
                z = wo_float(&elem);
            }
            void JEParseToScriptType(wo_vm vm, wo_value v) const
            {
                wo_set_struct(v, vm, 3);
                _wo_value elem;

                wo_set_float(&elem, x);
                wo_struct_set(v, 0, &elem);

                wo_set_float(&elem, y);
                wo_struct_set(v, 1, &elem);

                wo_set_float(&elem, z);
                wo_struct_set(v, 2, &elem);
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
            void JEParseFromScriptType(wo_vm vm, wo_value v)
            {
                _wo_value elem;

                wo_struct_get(&elem, v, 0);
                x = wo_float(&elem);

                wo_struct_get(&elem, v, 1);
                y = wo_float(&elem);

                wo_struct_get(&elem, v, 2);
                z = wo_float(&elem);

                wo_struct_get(&elem, v, 3);
                w = wo_float(&elem);
            }
            void JEParseToScriptType(wo_vm vm, wo_value v) const
            {
                wo_set_struct(v, vm, 4);
                _wo_value elem;

                wo_set_float(&elem, x);
                wo_struct_set(v, 0, &elem);

                wo_set_float(&elem, y);
                wo_struct_set(v, 1, &elem);

                wo_set_float(&elem, z);
                wo_struct_set(v, 2, &elem);

                wo_set_float(&elem, w);
                wo_struct_set(v, 3, &elem);
            }
        };
        inline static constexpr vec4 operator*(float _f, const vec4& _v4) noexcept
        {
            return vec4(_v4.x * _f, _v4.y * _f, _v4.z * _f, _v4.w * _f);
        }

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
                auto axis = b.cross(a);
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
            void JEParseFromScriptType(wo_vm vm, wo_value v)
            {
                _wo_value elem;

                wo_struct_get(&elem, v, 0);
                x = wo_float(&elem);

                wo_struct_get(&elem, v, 1);
                y = wo_float(&elem);

                wo_struct_get(&elem, v, 2);
                z = wo_float(&elem);

                wo_struct_get(&elem, v, 3);
                w = wo_float(&elem);
            }
            void JEParseToScriptType(wo_vm vm, wo_value v) const
            {
                wo_set_struct(v, vm, 4);
                _wo_value elem;

                wo_set_float(&elem, x);
                wo_struct_set(v, 0, &elem);

                wo_set_float(&elem, y);
                wo_struct_set(v, 1, &elem);

                wo_set_float(&elem, z);
                wo_struct_set(v, 2, &elem);

                wo_set_float(&elem, w);
                wo_struct_set(v, 3, &elem);
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

    namespace graphic
    {
        class resource_basic
        {
            JECS_DISABLE_MOVE_AND_COPY(resource_basic);

            jegl_resource* _m_resource;

        protected:
            resource_basic(jegl_resource* res) noexcept
                : _m_resource(res)
            {
                assert(_m_resource != nullptr);
            }

        public:
            inline jegl_resource* resource() const noexcept
            {
                return _m_resource;
            }
            ~resource_basic()
            {
                assert(_m_resource != nullptr);
                jegl_close_resource(_m_resource);
            }
        };
        class texture : public resource_basic
        {
            explicit texture(jegl_resource* res)
                : resource_basic(res)
            {
            }

        public:
            static std::optional<basic::resource<texture>> load(jegl_context* context, const std::string& str)
            {
                jegl_resource* res = jegl_load_texture(context, str.c_str());
                if (res != nullptr)
                    return basic::resource<texture>(new texture(res));
                return std::nullopt;
            }
            static basic::resource<texture> create(size_t width, size_t height, jegl_texture::format format)
            {
                jegl_resource* res = jegl_create_texture(width, height, format);

                // Create texture must be successfully.
                assert(res != nullptr);
                return basic::resource<texture>(new texture(res));
            }
            static basic::resource<texture> clip(
                const basic::resource<texture>& src, size_t x, size_t y, size_t w, size_t h)
            {
                jegl_texture::format new_texture_format = (jegl_texture::format)(src->resource()->m_raw_texture_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK);
                jegl_resource* res = jegl_create_texture(w, h, new_texture_format);

                // Create texture must be successfully.
                assert(res != nullptr);

                auto* new_texture = new texture(res);

#if 0
                for (size_t iy = 0; iy < h; ++iy)
                {
                    for (size_t ix = 0; ix < w; ++ix)
                    {
                        new_texture->pix(ix, iy).set(
                            src->pix(ix + x, iy + y).get());
                    }
                }
#else
                auto color_depth = (int)new_texture_format;
                auto* dst_pixels = new_texture->resource()->m_raw_texture_data->m_pixels;
                auto* src_pixels = src->resource()->m_raw_texture_data->m_pixels;

                size_t src_w = std::min(w, src->resource()->m_raw_texture_data->m_width);
                size_t src_h = std::min(h, src->resource()->m_raw_texture_data->m_height);

                for (size_t iy = 0; iy < src_h; ++iy)
                {
                    memcpy(
                        dst_pixels + iy * w * color_depth,
                        src_pixels + (x + (y + iy) * src_w) * color_depth,
                        src_w * color_depth);
                }
#endif
                return basic::resource<texture>(new_texture);
            }

            class pixel
            {
                jegl_resource* _m_texture;
                jegl_texture::pixel_data_t*
                    _m_pixel;
                size_t _m_x, _m_y;

            public:
                pixel(jegl_resource* _texture, size_t x, size_t y) noexcept
                    : _m_texture(_texture), _m_x(x), _m_y(y)
                {
                    assert(_texture->m_type == jegl_resource::type::TEXTURE);
                    assert(sizeof(jegl_texture::pixel_data_t) == 1);

                    auto color_depth =
                        _m_texture->m_raw_texture_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK;

                    if (x < _m_texture->m_raw_texture_data->m_width && y < _m_texture->m_raw_texture_data->m_height)
                        _m_pixel = _m_texture->m_raw_texture_data->m_pixels + y * _m_texture->m_raw_texture_data->m_width * color_depth + x * color_depth;
                    else
                        _m_pixel = nullptr;
                }
                inline math::vec4 get() const noexcept
                {
                    if (_m_pixel == nullptr)
                        return {};
                    switch (_m_texture->m_raw_texture_data->m_format)
                    {
                    case jegl_texture::format::MONO:
                        return math::vec4{
                            _m_pixel[0] / 255.0f,
                            _m_pixel[0] / 255.0f,
                            _m_pixel[0] / 255.0f,
                            _m_pixel[0] / 255.0f };
                    case jegl_texture::format::RGBA:
                        return math::vec4{
                            _m_pixel[0] / 255.0f,
                            _m_pixel[1] / 255.0f,
                            _m_pixel[2] / 255.0f,
                            _m_pixel[3] / 255.0f };
                    default:
                        assert(0);
                        return {};
                    }
                }
                inline void set(const math::vec4& value) const noexcept
                {
                    if (_m_pixel == nullptr)
                        return;

                    auto* raw_texture_data = _m_texture->m_raw_texture_data;

                    if (_m_texture->m_graphic_thread != nullptr)
                    {
                        if (!_m_texture->m_modified)
                        {
                            // Set texture modified flag & range.
                            raw_texture_data->m_modified_max_x = _m_x;
                            raw_texture_data->m_modified_max_y = _m_y;
                            raw_texture_data->m_modified_min_x = _m_x;
                            raw_texture_data->m_modified_min_y = _m_y;

                            _m_texture->m_modified = true;
                        }
                        else
                        {
                            // Update texture updating range.

                            if (_m_x < raw_texture_data->m_modified_min_x)
                                raw_texture_data->m_modified_min_x = _m_x;
                            else if (_m_x > raw_texture_data->m_modified_max_x)
                                raw_texture_data->m_modified_max_x = _m_x;

                            if (_m_y < raw_texture_data->m_modified_min_y)
                                raw_texture_data->m_modified_min_y = _m_y;
                            else if (_m_y > raw_texture_data->m_modified_max_y)
                                raw_texture_data->m_modified_max_y = _m_y;
                        }
                    }

                    switch (raw_texture_data->m_format)
                    {
                    case jegl_texture::format::MONO:
                        _m_pixel[0] = (jegl_texture::pixel_data_t)round(math::clamp(value.x, 0.f, 1.f) * 255.0f);
                        break;
                    case jegl_texture::format::RGBA:
                        _m_pixel[0] = (jegl_texture::pixel_data_t)round(math::clamp(value.x, 0.f, 1.f) * 255.0f);
                        _m_pixel[1] = (jegl_texture::pixel_data_t)round(math::clamp(value.y, 0.f, 1.f) * 255.0f);
                        _m_pixel[2] = (jegl_texture::pixel_data_t)round(math::clamp(value.z, 0.f, 1.f) * 255.0f);
                        _m_pixel[3] = (jegl_texture::pixel_data_t)round(math::clamp(value.w, 0.f, 1.f) * 255.0f);
                        break;
                    default:
                        assert(0);
                        break;
                    }
                }

                inline operator math::vec4() const noexcept
                {
                    return get();
                }
                inline pixel& operator=(const math::vec4& col) noexcept
                {
                    set(col);
                    return *this;
                }
            };

            // pixel's x/y is from LEFT-BOTTOM to RIGHT/TOP
            pixel pix(size_t x, size_t y) const noexcept
            {
                return pixel(resource(), x, y);
            }
            inline size_t height() const noexcept
            {
                assert(resource()->m_raw_texture_data != nullptr);
                return resource()->m_raw_texture_data->m_height;
            }
            inline size_t width() const noexcept
            {
                assert(resource()->m_raw_texture_data != nullptr);
                return resource()->m_raw_texture_data->m_width;
            }
            inline math::ivec2 size() const noexcept
            {
                assert(resource()->m_raw_texture_data != nullptr);
                return math::ivec2(
                    (int)resource()->m_raw_texture_data->m_width,
                    (int)resource()->m_raw_texture_data->m_height);
            }
        };
        class shader : public resource_basic
        {
        private:
            explicit shader(jegl_resource* res)
                : resource_basic(res), m_builtin(nullptr)
            {
                m_builtin = &this->resource()->m_raw_shader_data->m_builtin_uniforms;
            }

        public:
            jegl_shader::builtin_uniform_location* m_builtin;

            static std::optional<basic::resource<shader>> create(const std::string& name_path, const std::string& src)
            {
                jegl_resource* res = jegl_load_shader_source(name_path.c_str(), src.c_str(), true);
                if (res != nullptr)
                    return basic::resource<shader>(new shader(res));
                return std::nullopt;
            }
            static std::optional<basic::resource<shader>> load(jegl_context* context, const std::string& src_path)
            {
                jegl_resource* res = jegl_load_shader(context, src_path.c_str());
                if (res != nullptr)
                    return basic::resource<shader>(new shader(res));
                return std::nullopt;
            }

            void set_uniform(const std::string& name, int val) noexcept
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::INT)
                            debug::logerr(
                                "Trying set uniform('%s' = %d) to shader(%p), but current uniform type is not 'INT'.",
                                name.c_str(), val, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.ix = val;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, int x, int y) noexcept
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::INT)
                            debug::logerr(
                                "Trying set uniform('%s' = %d, %d) to shader(%p), but current uniform type is not 'INT2'.",
                                name.c_str(), x, y, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.ix = x;
                            jegl_shad_uniforms->m_value.iy = y;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, int x, int y, int z) noexcept
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::INT)
                            debug::logerr(
                                "Trying set uniform('%s' = %d, %d, %d) to shader(%p), but current uniform type is not 'INT3'.",
                                name.c_str(), x, y, z, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.ix = x;
                            jegl_shad_uniforms->m_value.iy = y;
                            jegl_shad_uniforms->m_value.iz = z;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, int x, int y, int z, int w) noexcept
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::INT)
                            debug::logerr(
                                "Trying set uniform('%s' = %d, %d, %d, %d) to shader(%p), but current uniform type is not 'INT4'.",
                                name.c_str(), x, y, z, w, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.ix = x;
                            jegl_shad_uniforms->m_value.iy = y;
                            jegl_shad_uniforms->m_value.iz = z;
                            jegl_shad_uniforms->m_value.iw = w;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, float val) noexcept
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::FLOAT)
                            debug::logerr(
                                "Trying set uniform('%s' = %f) to shader(%p), but current uniform type is not 'FLOAT'.",
                                name.c_str(), val, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.x = val;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec2& val) noexcept
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::FLOAT2)
                            debug::logerr(
                                "Trying set uniform('%s' = (%f, %f)) to shader(%p), but current uniform type is not 'FLOAT2'.",
                                name.c_str(), val.x, val.y, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.x = val.x;
                            jegl_shad_uniforms->m_value.y = val.y;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec3& val) noexcept
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::FLOAT3)
                            debug::logerr(
                                "Trying set uniform('%s' = (%f, %f, %f)) to shader(%p), but current uniform type is not 'FLOAT3'.",
                                name.c_str(), val.x, val.y, val.z, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.x = val.x;
                            jegl_shad_uniforms->m_value.y = val.y;
                            jegl_shad_uniforms->m_value.z = val.z;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec4& val) noexcept
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::FLOAT4)
                            debug::logerr(
                                "Trying set uniform('%s' = (%f, %f, %f, %f)) to shader(%p), but current uniform type is not 'FLOAT4'.",
                                name.c_str(), val.x, val.y, val.z, val.w, this);
                        else
                        {
                            jegl_shad_uniforms->m_value.x = val.x;
                            jegl_shad_uniforms->m_value.y = val.y;
                            jegl_shad_uniforms->m_value.z = val.z;
                            jegl_shad_uniforms->m_value.w = val.w;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }

            uint32_t get_uniform_location(const std::string& name)
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        return jegl_shad_uniforms->m_index;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }

                return typing::INVALID_UINT32;
            }
            uint32_t* get_uniform_location_as_builtin(const std::string& name)
            {
                auto* jegl_shad_uniforms = resource()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        return &jegl_shad_uniforms->m_index;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }

                return nullptr;
            }
        };
        class vertex : public resource_basic
        {
            explicit vertex(jegl_resource* res)
                : resource_basic(res)
            {
            }

        public:
            static std::optional<basic::resource<vertex>> load(
                jegl_context* context,
                const std::string& str)
            {
                auto* res = jegl_load_vertex(context, str.c_str());
                if (res != nullptr)
                    return basic::resource<vertex>(new vertex(res));
                return std::nullopt;
            }
            static std::optional<basic::resource<vertex>> create(
                jegl_vertex::type type,
                const void* pdatas,
                size_t pdatalen,
                const std::vector<uint32_t> idatas, // EBO Indexs
                const std::vector<jegl_vertex::data_layout> fdatas)
            {
                auto* res = jegl_create_vertex(
                    type,
                    pdatas, pdatalen,
                    idatas.data(), idatas.size(),
                    fdatas.data(), fdatas.size());

                if (res != nullptr)
                    return basic::resource<vertex>(new vertex(res));
                return std::nullopt;
            }
        };
        class framebuffer : public resource_basic
        {
            explicit framebuffer(jegl_resource* res)
                : resource_basic(res)
            {
            }

        public:
            static std::optional<basic::resource<framebuffer>> create(
                size_t reso_w, size_t reso_h, const std::vector<jegl_texture::format>& attachment)
            {
                auto* res = jegl_create_framebuf(reso_w, reso_h, attachment.data(), attachment.size());
                if (res != nullptr)
                    return basic::resource<framebuffer>(new framebuffer(res));
                return std::nullopt;
            }
            std::optional<basic::resource<texture>> get_attachment(size_t index) const
            {
                if (index < resource()->m_raw_framebuf_data->m_attachment_count)
                {
                    auto* attachments = std::launder(
                        reinterpret_cast<basic::resource<graphic::texture> *>(
                            resource()->m_raw_framebuf_data->m_output_attachments));
                    return attachments[index];
                }
                return std::nullopt;
            }

            inline size_t height() const noexcept
            {
                return resource()->m_raw_framebuf_data->m_height;
            }
            inline size_t width() const noexcept
            {
                return resource()->m_raw_framebuf_data->m_width;
            }
            inline math::ivec2 size() const noexcept
            {
                return math::ivec2(
                    (int)resource()->m_raw_framebuf_data->m_width,
                    (int)resource()->m_raw_framebuf_data->m_height);
            }
        };
        class uniformbuffer : public resource_basic
        {
            explicit uniformbuffer(jegl_resource* res)
                : resource_basic(res)
            {
                assert(resource() != nullptr);
            }

        public:
            static std::optional<basic::resource<uniformbuffer>> create(
                size_t binding_place, size_t buffersize)
            {
                jegl_resource* res = jegl_create_uniformbuf(binding_place, buffersize);
                if (res != nullptr)
                    return basic::resource<uniformbuffer>(new uniformbuffer(res));
                return std::nullopt;
            }
            void update_buffer(size_t offset, size_t size, const void* datafrom) const noexcept
            {
                jegl_update_uniformbuf(resource(), datafrom, offset, size);
            }
        };

        inline constexpr float ORTHO_PROJECTION_RATIO = 5.0f;

        inline void ortho_projection(
            float (*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float scale,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;

            const float R = WIDTH_HEIGHT_RATIO * ORTHO_PROJECTION_RATIO / scale;
            const float L = -R;
            const float T = ORTHO_PROJECTION_RATIO / scale;
            const float B = -T;

            auto m = out_proj_mat;
            m[0][0] = 2.0f / (R - L);
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = 2.0f / (T - B);
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = 1.0f / (zfar - znear);
            m[2][3] = 0;

            m[3][0] = -((R + L) / (R - L));
            m[3][1] = -((T + B) / (T - B));
            m[3][2] = 0;
            m[3][3] = 1;
        }

        inline void ortho_inv_projection(
            float (*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float scale,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;

            const float R = WIDTH_HEIGHT_RATIO * ORTHO_PROJECTION_RATIO / scale;
            const float L = -R;
            const float T = ORTHO_PROJECTION_RATIO / scale;
            const float B = -T;

            auto m = out_proj_mat;
            m[0][0] = (R - L) / 2.0f;
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = (T - B) / 2.0f;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = zfar - znear;
            m[2][3] = 0;

            m[3][0] = (R + L) / 2.0f;
            m[3][1] = (T + B) / 2.0f;
            m[3][2] = 0;
            m[3][3] = 1;
        }

        inline void perspective_projection(
            float (*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float angle,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;
            const float ZRANGE = znear - zfar;
            const float TAN_HALF_FOV = tanf(angle / math::RAD2DEG / 2.0f);

            auto m = out_proj_mat;
            m[0][0] = 1.0f / (TAN_HALF_FOV * WIDTH_HEIGHT_RATIO);
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = 1.0f / TAN_HALF_FOV;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = -zfar / ZRANGE;
            m[2][3] = 1.0f;

            m[3][0] = 0;
            m[3][1] = 0;
            m[3][2] = zfar * znear / ZRANGE;
            m[3][3] = 0;
        }

        inline void perspective_inv_projection(
            float (*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float angle,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;
            const float ZRANGE = znear - zfar;
            const float TAN_HALF_FOV = tanf(angle / math::RAD2DEG / 2.0f);

            auto m = out_proj_mat;
            m[0][0] = TAN_HALF_FOV * WIDTH_HEIGHT_RATIO;
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = TAN_HALF_FOV;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = 0;
            m[2][3] = ZRANGE / (zfar * znear);

            m[3][0] = 0;
            m[3][1] = 0;
            m[3][2] = 1;
            m[3][3] = 1.f / znear;
        }

        struct character
        {
            // 字符的纹理
            basic::resource<texture> m_texture;

            // 字符本身
            char32_t m_char = 0;
            // 字符本身的宽度和高度, 单位是像素
            // * 包含边框的影响
            int m_width = 0;
            int m_height = 0;

            // 建议的字符横向进展，通常用于计算下一列字符的起始位置，单位是像素，正数表示向右方延伸
            int m_advance_x = 0;
            // 建议的字符纵向进展，即行间距，通常用于计算下一行字符的起始位置，单位是像素，正数表示向上方延伸
            int m_advance_y = 0;

            // 当前字符基线横向偏移量，单位是像素，正数表示向右方偏移
            // * 包含边框的影响
            int m_baseline_offset_x = 0;
            // 当前字符基线纵向偏移量，单位是像素，正数表示向上方偏移
            // * 包含边框的影响
            int m_baseline_offset_y = 0;
        };
        class font
        {
            JECS_DISABLE_MOVE_AND_COPY(font);

            je_font* m_font;

        private:
            font(je_font* font_resource) noexcept
                : m_font(font_resource)
            {
            }

        public:
            static std::optional<basic::resource<font>> load(
                const std::string& fontfile,
                size_t size,
                size_t board_size = 0,
                je_font_char_updater_t char_texture_updater = nullptr)
            {
                auto* font_res = je_font_load(
                    fontfile.c_str(),
                    (float)size,
                    (float)size,
                    board_size,
                    board_size,
                    char_texture_updater);

                if (font_res != nullptr)
                    return basic::resource<font>(new font(font_res));
                return std::nullopt;
            }
            ~font()
            {
                je_font_free(m_font);
            }
            const character* get_character(char32_t wcharacter) const noexcept
            {
                return je_font_get_char(m_font, wcharacter);
            }

            basic::resource<texture> u32text_texture(
                const std::u32string& text)
            {
                return text_texture_impl(*this, text);
            }
            basic::resource<texture> u8text_texture(
                const std::string& text)
            {
                return text_texture_impl(*this, wo_str_to_u32str(text.c_str()));
            }

            je_font* resource() const noexcept
            {
                return m_font;
            }
        private:
            struct parallel_pixel_index_iter
            {
                size_t id;
                parallel_pixel_index_iter operator++()
                {
                    return { ++id };
                }
                parallel_pixel_index_iter operator++(int)
                {
                    return { id++ };
                }
                bool operator==(const parallel_pixel_index_iter& pindex) const
                {
                    return id == pindex.id;
                }
                bool operator!=(const parallel_pixel_index_iter& pindex) const
                {
                    return id != pindex.id;
                }
                size_t& operator*()
                {
                    return id;
                }
                ptrdiff_t operator-(const parallel_pixel_index_iter& another) const
                {
                    return ptrdiff_t(id - another.id);
                }

                typedef std::forward_iterator_tag iterator_category;
                typedef size_t value_type;
                typedef ptrdiff_t difference_type;
                typedef const size_t* pointer;
                typedef const size_t& reference;
            };

            inline static basic::resource<texture> text_texture_impl(
                font& font_base,
                const std::u32string& text) noexcept
            {
                const auto* base_font_resource = font_base.resource();
                const auto walk_through_all_character =
                    [&text](
                        const std::function<void(std::u32string_view, std::u32string_view)>& callback_attr,
                        const std::function<void(char32_t)>& callback_char)
                    {
                        bool in_escape = false;

                        const auto text_end = text.cend();
                        for (auto iter = text.cbegin(); iter != text_end; ++iter)
                        {
                            const char32_t ch = *iter;

                            if (!in_escape)
                            {
                                if (ch == U'\\')
                                {
                                    in_escape = false;
                                    continue;
                                }
                                else if (ch == U'{')
                                {
                                    bool in_field_name = true;
                                    std::u32string field;
                                    std::u32string value;

                                    for (++iter; iter != text_end; ++iter)
                                    {
                                        const char32_t ch_in_scope = *iter;

                                        if (ch_in_scope == U':')
                                            in_field_name = false;
                                        else if (ch_in_scope != U'}')
                                        {
                                            if (in_field_name)
                                                field += ch_in_scope;
                                            else
                                                value += ch_in_scope;
                                        }
                                        else // if (ch_in_scope == U'}')
                                        {
                                            callback_attr(field, value);
                                            break;
                                        }
                                    }
                                    continue;
                                }
                                // Normal character.
                            }
                            // Normal character, mark as not in escape.
                            in_escape = false;
                            callback_char(ch);
                        }
                    };

                using font_and_size_pair_t = std::pair<std::string, size_t>;
                std::map<font_and_size_pair_t, basic::resource<font>>
                    FONT_POOL;
                float       TEXT_SCALE = 1.0f;
                math::vec4  TEXT_COLOR = { 1, 1, 1, 1 };
                math::vec2  TEXT_OFFSET = { 0, 0 };
                font* TEXT_FONT_CURRENT = &font_base;

                int next_ch_x = 0;
                int next_ch_y = 0;
                int line_count = 0;

                // Calculate the size of the text.
                int min_px = INT_MAX, min_py = INT_MAX, max_px = INT_MIN, max_py = INT_MIN;

                walk_through_all_character(
                    [&](std::u32string_view field, std::u32string_view value) {
                        const auto u8value = wo_u32strn_to_str(value.data(), value.size());

                        if (field == U"scale")
                        {
                            TEXT_SCALE = std::stof(u8value);
                            if (TEXT_SCALE == 1.0f)
                                TEXT_FONT_CURRENT = &font_base;
                            else
                            {
                                const auto font_pair = std::make_pair(
                                    base_font_resource->m_path,
                                    static_cast<size_t>(round(TEXT_SCALE * base_font_resource->m_scale_x)));

                                auto font_fnd = FONT_POOL.find(font_pair);
                                if (font_fnd == FONT_POOL.end())
                                {
                                    auto loaded_font = font::load(
                                        font_pair.first,
                                        font_pair.second,
                                        base_font_resource->m_board_size_x,
                                        base_font_resource->m_updater);

                                    if (!loaded_font.has_value())
                                        debug::logerr(
                                            "Failed to open font: '%s'.",
                                            base_font_resource->m_path);
                                    else
                                        TEXT_FONT_CURRENT = FONT_POOL.insert(
                                            std::make_pair(
                                                font_pair,
                                                loaded_font.value())).first->second.get();
                                }
                                else
                                    TEXT_FONT_CURRENT = font_fnd->second.get();
                            }
                        }
                        else if (field == U"offset")
                        {
                            math::vec2 offset;
                            ((void)sscanf(u8value, "(%f,%f)", &offset.x, &offset.y));
                            TEXT_OFFSET += offset;
                        }
                    },
                    [&](char32_t ch) {
                        if (auto* character_info = TEXT_FONT_CURRENT->get_character(ch))
                        {
                            if (ch == U'\n')
                            {
                                next_ch_x = 0;
                                next_ch_y += character_info->m_advance_y;
                                line_count++;
                            }
                            else
                            {
                                const int px_min =
                                    next_ch_x
                                    + character_info->m_baseline_offset_x
                                    + static_cast<int>(
                                        TEXT_OFFSET.x
                                        * base_font_resource->m_scale_x);
                                const int py_min =
                                    next_ch_y
                                    + character_info->m_baseline_offset_y
                                    + static_cast<int>(
                                        TEXT_OFFSET.y
                                        * base_font_resource->m_scale_x);

                                const int px_max =
                                    next_ch_x
                                    + character_info->m_width
                                    + character_info->m_baseline_offset_x
                                    + static_cast<int>(
                                        TEXT_OFFSET.x
                                        * base_font_resource->m_scale_x);
                                const int py_max =
                                    next_ch_y
                                    + character_info->m_height
                                    + character_info->m_baseline_offset_y
                                    + static_cast<int>(
                                        TEXT_OFFSET.y
                                        * base_font_resource->m_scale_x);

                                min_px = min_px > px_min ? px_min : min_px;
                                min_py = min_py > py_min ? py_min : min_py;

                                max_px = max_px < px_max ? px_max : max_px;
                                max_py = max_py < py_max ? py_max : max_py;

                                next_ch_x += character_info->m_advance_x;
                            }
                        }
                    });

                const int size_x = max_px - min_px + 1;
                const int size_y = max_py - min_py + 1;

                const int correct_x = -min_px;
                const int correct_y = -min_py;

                next_ch_x = 0;
                next_ch_y = 0;
                line_count = 0;
                TEXT_SCALE = 1.0f;
                TEXT_OFFSET = { 0, 0 };
                TEXT_FONT_CURRENT = &font_base;

                auto new_texture = texture::create(
                    static_cast<size_t>(size_x),
                    static_cast<size_t>(size_y),
                    jegl_texture::format::RGBA);

                std::memset(
                    new_texture->resource()->m_raw_texture_data->m_pixels,
                    0,
                    size_x * size_y * 4);

                walk_through_all_character(
                    [&](std::u32string_view field, std::u32string_view value) {
                        const auto u8value = wo_u32strn_to_str(value.data(), value.size());

                        if (field == U"color")
                        {
                            char color[9] = "00000000";
                            strncpy(color, u8value, 8);

                            unsigned int colordata = strtoul(color, NULL, 16);

                            unsigned char As = (*(unsigned char*)(&colordata));
                            unsigned char Bs = (*(((unsigned char*)(&colordata)) + 1));
                            unsigned char Gs = (*(((unsigned char*)(&colordata)) + 2));
                            unsigned char Rs = (*(((unsigned char*)(&colordata)) + 3));

                            TEXT_COLOR = { Rs / 255.0f, Gs / 255.0f, Bs / 255.0f, As / 255.0f };
                        }
                        else if (field == U"scale")
                        {
                            TEXT_SCALE = std::stof(u8value);

                            if (TEXT_SCALE == 1.0f)
                                TEXT_FONT_CURRENT = &font_base;
                            else
                            {
                                const auto& font_in_pool = FONT_POOL.at(
                                    std::make_pair(
                                        base_font_resource->m_path,
                                        static_cast<size_t>(
                                            round(TEXT_SCALE * base_font_resource->m_scale_x))));

                                TEXT_FONT_CURRENT = font_in_pool.get();
                            }
                        }
                        else if (field == U"offset")
                        {
                            math::vec2 offset;
                            ((void)sscanf(u8value, "(%f,%f)", &offset.x, &offset.y));
                            TEXT_OFFSET = TEXT_OFFSET + offset;
                        }
                    },
                    [&](char32_t ch) {
                        if (auto* character_info = TEXT_FONT_CURRENT->get_character(ch))
                        {
                            if (ch == U'\n')
                            {
                                next_ch_x = 0;
                                next_ch_y += character_info->m_advance_y;
                                line_count++;
                            }
                            else
                            {
                                std::for_each(
#ifdef __cpp_lib_execution
                                    std::execution::par_unseq,
#endif
                                    parallel_pixel_index_iter{ 0 },
                                    parallel_pixel_index_iter{ size_t(character_info->m_texture->height()) },
                                    [&](size_t fy)
                                    {
                                        std::for_each(
#ifdef __cpp_lib_execution
                                            std::execution::par_unseq,
#endif
                                            parallel_pixel_index_iter{ 0 },
                                            parallel_pixel_index_iter{
                                                size_t(character_info->m_texture->width())
                                            },
                                            [&](size_t fx)
                                            {
                                                const auto x =
                                                    correct_x
                                                    + next_ch_x
                                                    + static_cast<int>(fx)
                                                    + character_info->m_baseline_offset_x
                                                    + static_cast<int>(
                                                        TEXT_OFFSET.x
                                                        * base_font_resource->m_scale_x);

                                                const auto y =
                                                    correct_y
                                                    + next_ch_y
                                                    + static_cast<int>(fy)
                                                    + character_info->m_baseline_offset_y
                                                    + static_cast<int>(
                                                        TEXT_OFFSET.y
                                                        * base_font_resource->m_scale_x);

                                                auto pdst = new_texture->pix(
                                                    static_cast<size_t>(x), static_cast<size_t>(y));
                                                const auto psrc = character_info->m_texture->pix(
                                                    static_cast<size_t>(fx), static_cast<size_t>(fy)).get();

                                                float src_alpha = psrc.w * TEXT_COLOR.w;

                                                pdst.set(
                                                    math::vec4(
                                                        src_alpha * psrc.x * TEXT_COLOR.x + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().x : 1.0f),
                                                        src_alpha * psrc.y * TEXT_COLOR.y + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().y : 1.0f),
                                                        src_alpha * psrc.z * TEXT_COLOR.z + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().z : 1.0f),
                                                        src_alpha * psrc.w * TEXT_COLOR.w + (1.0f - src_alpha) * pdst.get().w));
                                            }); // end of for each
                                    });
                                next_ch_x += character_info->m_advance_x;
                            }
                        }
                    });

                return new_texture;
            }
        };

        /*
        jeecs::graphic::BasePipelineInterface [`系统(System)`接口]
        可编程图形管线系统的接口类，用于简化图形管线的创建，包装了一些基本函数
        （实际上开发者可以直接通过jegl_uhost_get_or_create_for_universe获取uhost进行绘制）
        请参见：
            jegl_uhost_get_or_create_for_universe
        */
        struct BasePipelineInterface : game_system
        {
            // Locate uniform buffer 0.
            struct default_uniform_buffer_data_t
            {
                float m_v_float4x4[4][4];
                float m_p_float4x4[4][4];
                float m_vp_float4x4[4][4];
                float m_time[4];
            };

            graphic_uhost* _m_graphic_host;
            std::vector<rendchain_branch*> _m_rchain_pipeline;
            size_t _m_this_frame_allocate_rchain_pipeline_count;

            BasePipelineInterface(game_world w, const jegl_interface_config* config)
                : game_system(w), _m_graphic_host(jegl_uhost_get_or_create_for_universe(w.get_universe().handle(), config)), _m_this_frame_allocate_rchain_pipeline_count(0)
            {
            }
            ~BasePipelineInterface()
            {
                OnDisable();
            }

            void OnDisable()
            {
                // Free all branch if disabled.
                for (auto* branch : _m_rchain_pipeline)
                    jegl_uhost_free_branch(_m_graphic_host, branch);

                _m_rchain_pipeline.clear();
                _m_this_frame_allocate_rchain_pipeline_count = 0;
            }

            void branch_allocate_begin()
            {
                _m_this_frame_allocate_rchain_pipeline_count = 0;
            }
            rendchain_branch* allocate_branch(int priority)
            {
                if (_m_this_frame_allocate_rchain_pipeline_count >= _m_rchain_pipeline.size())
                {
                    assert(_m_this_frame_allocate_rchain_pipeline_count == _m_rchain_pipeline.size());
                    _m_rchain_pipeline.push_back(jegl_uhost_alloc_branch(_m_graphic_host));
                }
                auto* result = _m_rchain_pipeline[_m_this_frame_allocate_rchain_pipeline_count++];
                jegl_branch_new_frame(result, priority);
                return result;
            }
            void branch_allocate_end()
            {
                for (size_t i = _m_this_frame_allocate_rchain_pipeline_count; i < _m_rchain_pipeline.size(); ++i)
                    jegl_uhost_free_branch(_m_graphic_host, _m_rchain_pipeline[i]);

                _m_rchain_pipeline.resize(_m_this_frame_allocate_rchain_pipeline_count);
            }
        };

        class graphic_syncer_host
        {
            JECS_DISABLE_MOVE_AND_COPY(graphic_syncer_host);

            std::mutex mx;
            std::condition_variable cv;
            std::thread entry_script_thread;

            bool entry_script_ended = false;
            std::optional<jegl_context*> graphic_context = std::nullopt;

            jegl_context* in_frame_current_context = nullptr;

            static void callback(jegl_context* context, void* p)
            {
                graphic_syncer_host* self =
                    reinterpret_cast<graphic_syncer_host*>(p);

                std::lock_guard g(self->mx);
                self->graphic_context = context;
                self->cv.notify_one();
            }
            bool check_context_ready_no_lock()
            {
                if (graphic_context.has_value())
                {
                    in_frame_current_context = graphic_context.value();
                    graphic_context.reset();

                    jegl_sync_init(in_frame_current_context, false);
                    return true;
                }
                return false;
            }
        public:
            // 被设计用于自定义的图形同步上下文，用于获取引擎提供的图形配置信息
            // 
            // NOTE: 必须在 check_context_ready_block 或 check_context_ready_noblock 确认上下文已
            //      就绪后才能调用
            jegl_context* get_graphic_context_after_context_ready() const
            {
                assert(in_frame_current_context != nullptr);
                return in_frame_current_context;
            }
            bool check_context_ready_block()
            {
                for (;;)
                {
                    std::unique_lock ug(mx);
                    cv.wait(ug, [this]() {return entry_script_ended || graphic_context.has_value(); });

                    if (entry_script_ended)
                        return false;

                    if (!check_context_ready_no_lock())
                    {
                        jeecs::debug::logerr(
                            "Failed to get graphic context, it should not happend.");
                        continue;
                    }
                    break;
                }
                return true;
            }
            bool check_context_ready_noblock()
            {
                std::lock_guard g(mx);
                return check_context_ready_no_lock();
            }
            bool frame()
            {
                assert(in_frame_current_context != nullptr);
                switch (jegl_sync_update(in_frame_current_context))
                {
                case jegl_sync_state::JEGL_SYNC_COMPLETE:
                    // Normal frame end, do nothing.
                    break;
                case jegl_sync_state::JEGL_SYNC_REBOOT:
                    // Require to reboot graphic thread.
                    jegl_sync_shutdown(in_frame_current_context, true);
                    jegl_sync_init(in_frame_current_context, true);
                    break;
                case jegl_sync_state::JEGL_SYNC_SHUTDOWN:
                    // Graphic thread want to shutdown, exit the loop.
                    jegl_sync_shutdown(in_frame_current_context, false);
                    in_frame_current_context = nullptr;
                    return false;
                }
                return true;
            }
            void loop()
            {
                for (;;)
                {
                    if (!check_context_ready_block())
                        break; // If the entry script ended, exit the loop.

                    while (frame())
                        ; // Update frames until the graphic context request to shutdown.
                }
            }
            graphic_syncer_host()
            {
                jegl_register_sync_thread_callback(
                    graphic_syncer_host::callback, this);

                entry_script_thread = std::thread(
                    [this]()
                    {
                        je_main_script_entry();

                        std::lock_guard g(mx);
                        entry_script_ended = true;
                        cv.notify_one();
                    });
            }
            ~graphic_syncer_host()
            {
                entry_script_thread.join();
            }
        };
    }

    namespace audio
    {
        template <typename T>
        class effect
        {
            JECS_DISABLE_MOVE_AND_COPY(effect);

            T* m_effect;

            effect(T* effect)
                : m_effect(effect)
            {
                assert(m_effect != nullptr);
            }

        public:
            ~effect()
            {
                jeal_close_effect(m_effect);
            }

            static basic::resource<effect<T>> create()
            {
                T* instance;
                if constexpr (std::is_same_v<T, jeal_effect_reverb>)
                    instance = jeal_create_effect_reverb();
                else if constexpr (std::is_same_v<T, jeal_effect_chorus>)
                    instance = jeal_create_effect_chorus();
                else if constexpr (std::is_same_v<T, jeal_effect_distortion>)
                    instance = jeal_create_effect_distortion();
                else if constexpr (std::is_same_v<T, jeal_effect_echo>)
                    instance = jeal_create_effect_echo();
                else if constexpr (std::is_same_v<T, jeal_effect_flanger>)
                    instance = jeal_create_effect_flanger();
                else if constexpr (std::is_same_v<T, jeal_effect_frequency_shifter>)
                    instance = jeal_create_effect_frequency_shifter();
                else if constexpr (std::is_same_v<T, jeal_effect_vocal_morpher>)
                    instance = jeal_create_effect_vocal_morpher();
                else if constexpr (std::is_same_v<T, jeal_effect_pitch_shifter>)
                    instance = jeal_create_effect_pitch_shifter();
                else if constexpr (std::is_same_v<T, jeal_effect_ring_modulator>)
                    instance = jeal_create_effect_ring_modulator();
                else if constexpr (std::is_same_v<T, jeal_effect_autowah>)
                    instance = jeal_create_effect_autowah();
                else if constexpr (std::is_same_v<T, jeal_effect_compressor>)
                    instance = jeal_create_effect_compressor();
                else if constexpr (std::is_same_v<T, jeal_effect_equalizer>)
                    instance = jeal_create_effect_equalizer();
                else if constexpr (std::is_same_v<T, jeal_effect_eaxreverb>)
                    instance = jeal_create_effect_eaxreverb();
                else
                    static_assert(
                        std::is_void_v<T> && !std::is_void_v<T> /* false */,
                        "Unsupported effect type");

                return basic::resource<effect<T>>(new effect<T>(instance));
            }

            T* handle() const
            {
                return m_effect;
            }
            void update(const std::function<void(T*)>& func)
            {
                func(m_effect);
                jeal_update_effect(m_effect);
            }
        };
        class effect_slot
        {
            JECS_DISABLE_MOVE_AND_COPY(effect_slot);
            jeal_effect_slot* m_effect_slot;
            effect_slot(jeal_effect_slot* effect_slot)
                : m_effect_slot(effect_slot)
            {
                assert(m_effect_slot != nullptr);
            }

        public:
            ~effect_slot()
            {
                jeal_close_effect_slot(m_effect_slot);
            }
            template <typename T>
            void bind_effect(const basic::resource<effect<T>>& effect)
            {
                jeal_effect_slot_bind(m_effect_slot, effect->handle());
            }

            static basic::resource<effect_slot> create()
            {
                auto* slot = jeal_create_effect_slot();
                assert(slot != nullptr);
                return basic::resource<effect_slot>(new effect_slot(slot));
            }
            jeal_effect_slot* handle() const
            {
                return m_effect_slot;
            }
            void update(const std::function<void(jeal_effect_slot*)>& func)
            {
                func(m_effect_slot);
                jeal_update_effect_slot(m_effect_slot);
            }
        };
        class buffer
        {
            JECS_DISABLE_MOVE_AND_COPY(buffer);

            const jeal_buffer* _m_audio_buffer;
            buffer(const jeal_buffer* audio_buffer)
                : _m_audio_buffer(audio_buffer)
            {
                assert(_m_audio_buffer != nullptr);
            }

        public:
            ~buffer()
            {
                jeal_close_buffer(_m_audio_buffer);
            }
            inline static std::optional<basic::resource<buffer>> create(
                const void* data,
                size_t length,
                size_t samplerate,
                jeal_format format)
            {
                auto* buf = jeal_create_buffer(data, length, samplerate, format);
                if (buf != nullptr)
                    return basic::resource<buffer>(new buffer(buf));
                return std::nullopt;
            }
            inline static std::optional<basic::resource<buffer>> load(const std::string& path)
            {
                auto* buf = jeal_load_buffer_wav(path.c_str());
                if (buf != nullptr)
                    return basic::resource<buffer>(new buffer(buf));
                return std::nullopt;
            }
            const jeal_buffer* handle() const
            {
                return _m_audio_buffer;
            }
        };
        class source
        {
            JECS_DISABLE_MOVE_AND_COPY(source);

            jeal_source* _m_audio_source;

            source(jeal_source* audio_source)
                : _m_audio_source(audio_source)
            {
                assert(_m_audio_source != nullptr);
            }

        public:
            ~source()
            {
                stop();
                jeal_close_source(_m_audio_source);
            }
            inline jeal_state get_state() const
            {
                return jeal_get_source_play_state(_m_audio_source);
            }
            inline static basic::resource<source> create()
            {
                return basic::resource<source>(new source(jeal_create_source()));
            }
            inline void set_playing_buffer(const basic::resource<buffer>& buffer)
            {
                jeal_set_source_buffer(_m_audio_source, buffer->handle());
            }
            jeal_source* handle() const
            {
                return _m_audio_source;
            }
            void play()
            {
                jeal_play_source(_m_audio_source);
            }
            void pause()
            {
                jeal_pause_source(_m_audio_source);
            }
            void stop()
            {
                jeal_stop_source(_m_audio_source);
            }
            size_t get_playing_offset() const
            {
                return jeal_get_source_play_process(_m_audio_source);
            }
            void set_playing_offset(size_t offset)
            {
                jeal_set_source_play_process(_m_audio_source, offset);
            }
            void update(const std::function<void(jeal_source*)>& func)
            {
                func(_m_audio_source);
                jeal_update_source(_m_audio_source);
            }
            void bind_effect_slot(const basic::resource<effect_slot>& slot, size_t pass)
            {
                if (pass < MAX_AUXILIARY_SENDS)
                    jeal_set_source_effect_slot(_m_audio_source, slot->handle(), pass);
            }
        };
        class listener
        {
        public:
            static jeal_listener* handle()
            {
                return jeal_get_listener();
            }
            static void update(const std::function<void(jeal_listener*)>& func)
            {
                func(jeal_get_listener());
                jeal_update_listener();
            }
        };
    }

    namespace Transform
    {
        // An entity without childs and parent will contain these components:
        // LocalPosition/LocalRotation/LocalScale and using LocalToWorld to apply
        // local transform to Translation
        // If an entity have childs, it will have Anchor
        // If an entity have parent, it will have LocalToParent and without
        // LocalToWorld.
        /*
                                                    Parent's Translation--\
                                                         /(apply)          \
                                                        /                   \
                                 LocalToParent-----LocalToParent             >Translation (2x  4x4 matrix)
                                                 yes  /                     /
        LocalPosition------------>(HAVE_PARENT?)-----/                     /
                       /    /              \ no                           /
        LocalScale----/    /                ---------> LocalToWorld------/
                          /
        LocalRotation----/                                (LocalToXXX) Still using 2x 3 vector + 1x quat

        */
        struct LocalRotation
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalRotation);
            JECS_DEFAULT_CONSTRUCTOR(LocalRotation);

            math::quat rot;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalRotation::rot, "rot");
            }
        };
        struct LocalPosition
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalPosition);
            JECS_DEFAULT_CONSTRUCTOR(LocalPosition);

            math::vec3 pos;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalPosition::pos, "pos");
            }
        };
        struct LocalScale
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalScale);
            JECS_DEFAULT_CONSTRUCTOR(LocalScale);

            math::vec3 scale = { 1.0f, 1.0f, 1.0f };

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalScale::scale, "scale");
            }
        };
        struct Translation
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Translation);
            JECS_DEFAULT_CONSTRUCTOR(Translation);

            float object2world[4][4] = {};

            math::vec3 world_position = { 0.0f, 0.0f, 0.0f };
            math::quat world_rotation;
            math::vec3 local_scale = { 1.0f, 1.0f, 1.0f };

            math::quat get_parent_rotation(const LocalRotation* local_rot) const noexcept
            {
                if (local_rot != nullptr)
                    return world_rotation * local_rot->rot.inverse();
                return world_rotation;
            }
            math::vec3 get_parent_position(const LocalPosition* local_pos, const LocalRotation* rotation) const noexcept
            {
                if (local_pos)
                    return world_position - get_parent_rotation(rotation) * local_pos->pos;
                return world_position;
            }

            void set_global_rotation(const math::quat& _rot, LocalRotation* rot)
            {
                if (rot)
                    rot->rot = _rot * get_parent_rotation(rot).inverse();
                world_rotation = _rot;
            }
            void set_global_position(const math::vec3& _pos, LocalPosition* pos, const LocalRotation* rot)
            {
                if (pos)
                    pos->pos = get_parent_rotation(rot).inverse() * (_pos - get_parent_position(pos, rot));
                world_position = _pos;
            }
        };
        struct Anchor
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Anchor);

            typing::uid_t uid = typing::uid_t::generate();

            Anchor() = default;
            Anchor(Anchor&&) = default;
            Anchor(Anchor&) {}

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Anchor::uid, "uid");
            }
        };
        struct LocalToParent
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalToParent);
            JECS_DEFAULT_CONSTRUCTOR(LocalToParent);

            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;

            typing::uid_t parent_uid = {};

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalToParent::parent_uid, "parent_uid");
            }
        };
        struct LocalToWorld
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(LocalToWorld);
            JECS_DEFAULT_CONSTRUCTOR(LocalToWorld);

            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;
        };
        static_assert(offsetof(LocalToParent, pos) == offsetof(LocalToWorld, pos));
        static_assert(offsetof(LocalToParent, scale) == offsetof(LocalToWorld, scale));
        static_assert(offsetof(LocalToParent, rot) == offsetof(LocalToWorld, rot));
    }
    namespace UserInterface
    {
        struct Origin
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Origin);
            JECS_DEFAULT_CONSTRUCTOR(Origin);

            enum origin_center : uint8_t
            {
                center = 0,

                left = 1 << 0,
                right = 1 << 1,
                top = 1 << 2,
                bottom = 1 << 3,
            };

            origin_center elem_center = origin_center::center;
            origin_center root_center = origin_center::center;

            // Will be update by uipipeline
            // Abs
            math::vec2 size = {};
            math::vec2 global_offset = {};
            // Rel
            math::vec2 scale = {};
            math::vec2 global_location = {};

            bool keep_vertical_ratio = false;

            // 用于计算ui元素的绝对坐标和大小，接受显示区域的宽度和高度，获取以屏幕左下角为原点的元素位置和大小。
            // 其中位置是ui元素中心位置，而非坐标原点位置。
            void get_layout(
                float w,
                float h,
                math::vec2* out_absoffset,
                math::vec2* out_abssize,
                math::vec2* out_center_offset) const
            {
                math::vec2 rel2abssize = scale * math::vec2(w, h);
                math::vec2 rel2absoffset = global_location * math::vec2(w, h);

                if (keep_vertical_ratio)
                {
                    rel2abssize.x *= h / w;
                    rel2absoffset.x *= h / w;
                }
                else
                {
                    rel2abssize.y *= w / h;
                    rel2absoffset.y *= w / h;
                }

                math::vec2 abssize = size + rel2abssize;
                math::vec2 absoffset = global_offset + rel2absoffset;

                // 消除中心偏差
                math::vec2 center_offset = {};

                absoffset.x += w / 2.0f;
                absoffset.y += h / 2.0f;

                ////////////////////////////////////
                if (root_center & origin_center::left)
                    absoffset.x += -w / 2.0f;
                if (root_center & origin_center::right)
                    absoffset.x += w / 2.0f;

                if (root_center & origin_center::top)
                    absoffset.y += h / 2.0f;
                if (root_center & origin_center::bottom)
                    absoffset.y += -h / 2.0f;

                ////////////////////////////////////
                if (elem_center & origin_center::left)
                {
                    absoffset.x += abssize.x / 2.0f;
                    center_offset.x = abssize.x / 2.0f;
                }
                if (elem_center & origin_center::right)
                {
                    absoffset.x += -abssize.x / 2.0f;
                    center_offset.x = -abssize.x / 2.0f;
                }
                if (elem_center & origin_center::top)
                {
                    absoffset.y += -abssize.y / 2.0f;
                    center_offset.y = -abssize.y / 2.0f;
                }
                if (elem_center & origin_center::bottom)
                {
                    absoffset.y += abssize.y / 2.0f;
                    center_offset.y = abssize.y / 2.0f;
                }

                if (out_center_offset)
                    *out_center_offset = center_offset;
                if (out_abssize)
                    *out_abssize = abssize;
                if (out_absoffset)
                    *out_absoffset = absoffset;
            }

            bool mouse_on(
                float w,
                float h,
                float rot_angle,
                math::vec2 mouse_view_pos) const
            {
                math::vec2 absoffset;
                math::vec2 abssize;

                math::vec2 absmouse = (mouse_view_pos + math::vec2(1.f, 1.f)) / 2.f * math::vec2(w, h);
                get_layout(w, h, &absoffset, &abssize, nullptr);

                const math::vec3 corrected_mouse_diff =
                    (math::quat::euler(0., 0., -rot_angle) * math::vec3(absmouse - absoffset));

                const float absdiffx = abs(corrected_mouse_diff.x);
                const float absdiffy = abs(corrected_mouse_diff.y);

                return absdiffx < abssize.x / 2.f && absdiffy < abssize.y / 2.f;
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Origin::elem_center, "elem_center");
            }
        };
        struct Absolute
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Absolute);
            JECS_DEFAULT_CONSTRUCTOR(Absolute);

            math::vec2 size = { 0.0f, 0.0f };
            math::vec2 offset = { 0.0f, 0.0f };

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Absolute::size, "size");
                typing::register_member(guard, &Absolute::offset, "offset");
            }
        };
        struct Relatively
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Relatively);
            JECS_DEFAULT_CONSTRUCTOR(Relatively);

            jeecs::math::vec2 location = {};
            jeecs::math::vec2 scale = { 0.0f, 0.0f };
            bool use_vertical_ratio = false;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Relatively::location, "location");
                typing::register_member(guard, &Relatively::scale, "scale");
                typing::register_member(guard, &Relatively::use_vertical_ratio, "use_vertical_ratio");
            }
        };
        struct Rotation
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Rotation);
            JECS_DEFAULT_CONSTRUCTOR(Rotation);

            float angle = 0.0f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Rotation::angle, "angle");
            }
        };
    };
    namespace Renderer
    {
        // An entity need to be rendered, must have Transform::Translation and
        // Renderer,
        /*

        (Viewport)
        OrthoProjection/PerspectiveProjection
        Projection
        Clip  ----------\
                         >------> Cameras
                        /          / | \
        Translation ---<          /  |  \ (for each..)
                        \        /   |   \
                         >--  The entities need
        Material--------/         be rend..
                       /
        (Shape)-------/

        */
        struct Rendqueue
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Rendqueue);
            JECS_DEFAULT_CONSTRUCTOR(Rendqueue);

            int rend_queue = 0;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Rendqueue::rend_queue, "rend_queue");
            }
        };
        struct Shape
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Shape);
            JECS_DEFAULT_CONSTRUCTOR(Shape);

            basic::optional<basic::resource<graphic::vertex>> vertex;
        };
        struct Shaders
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Shaders);
            JECS_DEFAULT_CONSTRUCTOR(Shaders);

            basic::vector<basic::resource<graphic::shader>> shaders;

            template <typename T>
            void set_uniform(const std::string& name, const T& val) noexcept
            {
                for (auto& shad : shaders)
                    shad->set_uniform(name, val);
            }
        };
        struct Textures
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Textures);
            JECS_DEFAULT_CONSTRUCTOR(Textures);

            // NOTE: textures should not be nullptr!
            struct texture_with_passid
            {
                size_t m_pass_id;
                basic::resource<graphic::texture> m_texture;

                texture_with_passid(size_t pass, const basic::resource<graphic::texture>& tex)
                    : m_pass_id(pass), m_texture(tex)
                {
                }
            };
            math::vec2 tiling = math::vec2(1.f, 1.f);
            math::vec2 offset = math::vec2(0.f, 0.f);
            basic::vector<texture_with_passid> textures;

            void bind_texture(size_t passid, const basic::resource<graphic::texture>& texture)
            {
                for (auto& pair : textures)
                {
                    if (pair.m_pass_id == passid)
                    {
                        pair.m_texture = texture;
                        return;
                    }
                }
                textures.push_back(texture_with_passid(passid, texture));
            }
            void remove_texture(size_t passid)
            {
                for (auto i = textures.begin(); i != textures.end(); ++i)
                {
                    if (i->m_pass_id == passid)
                    {
                        textures.erase(i);
                        return;
                    }
                }
            }
            std::optional<basic::resource<graphic::texture>> get_texture(size_t passid) const
            {
                for (auto& [pass, tex] : textures)
                    if (pass == passid)
                        return tex;

                return std::nullopt;
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Textures::tiling, "tiling");
                typing::register_member(guard, &Textures::offset, "offset");
            }
        };
        struct Color
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Color);
            JECS_DEFAULT_CONSTRUCTOR(Color);

            math::vec4 color = math::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Color::color, "color");
            }
        };
    }
    namespace Camera
    {
        struct Clip
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Clip);
            JECS_DEFAULT_CONSTRUCTOR(Clip);

            float znear = 0.3f;
            float zfar = 1000.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Clip::znear, "znear");
                typing::register_member(guard, &Clip::zfar, "zfar");
            }
        };
        struct FrustumCulling
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(FrustumCulling);
            JECS_DEFAULT_CONSTRUCTOR(FrustumCulling);

            float frustum_plane_distance[6] = {};
            math::vec3 frustum_plane_normals[6] = {};

            bool test_circle(const math::vec3& origin, float r) const
            {
                for (size_t index = 0; index < 6; ++index)
                {
                    auto distance_vec = frustum_plane_normals[index] * origin;
                    auto distance = distance_vec.x + distance_vec.y + distance_vec.z +
                        frustum_plane_distance[index];

                    if (distance < -r)
                        return false;
                }
                return true;
            }
        };
        struct Projection
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Projection);

            jeecs::basic::resource<jeecs::graphic::uniformbuffer>
                default_uniform_buffer = jeecs::graphic::uniformbuffer::create(
                    0, sizeof(graphic::BasePipelineInterface::default_uniform_buffer_data_t))
                .value();

            float view[4][4] = {};
            float projection[4][4] = {};
            float view_projection[4][4] = {};
            float inv_projection[4][4] = {};

            Projection() = default;
            Projection(const Projection&) { /* Do nothing */ }
            Projection(Projection&&) = default;
        };
        struct OrthoProjection
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(OrthoProjection);
            JECS_DEFAULT_CONSTRUCTOR(OrthoProjection);

            float scale = 1.0f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &OrthoProjection::scale, "scale");
            }
        };
        struct PerspectiveProjection
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(PerspectiveProjection);
            JECS_DEFAULT_CONSTRUCTOR(PerspectiveProjection);

            float angle = 75.0f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &PerspectiveProjection::angle, "angle");
            }
        };
        struct Viewport
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Viewport);
            JECS_DEFAULT_CONSTRUCTOR(Viewport);

            math::vec4 viewport = math::vec4(0, 0, 1, 1);
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Viewport::viewport, "viewport");
            }
        };
        struct Clear
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Clear);
            JECS_DEFAULT_CONSTRUCTOR(Clear);

            math::vec4 color = math::vec4(0.f, 0.f, 0.f, 1.f);
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Clear::color, "color");
            }
        };
        struct RendToFramebuffer
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(RendToFramebuffer);
            JECS_DEFAULT_CONSTRUCTOR(RendToFramebuffer);

            basic::optional<basic::resource<graphic::framebuffer>> framebuffer;
        };
    }
    namespace Physics2D
    {
        struct World
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(World);
            JECS_DEFAULT_CONSTRUCTOR(World);

            size_t layerid = 0;
            math::vec2 gravity = math::vec2(0.f, -9.8f);
            bool sleepable = true;
            bool continuous = true;
            size_t step = 4;

            basic::fileresource<void> group_config;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &World::layerid, "layerid");
                typing::register_member(guard, &World::gravity, "gravity");
                typing::register_member(guard, &World::sleepable, "sleepable");
                typing::register_member(guard, &World::continuous, "continuous");
                typing::register_member(guard, &World::step, "step");
                typing::register_member(guard, &World::group_config, "group_config");
            }
        };
        struct Rigidbody
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Rigidbody);

            using rigidbody_id_t = uint64_t;
            constexpr static rigidbody_id_t null_rigidbody = 0;

            rigidbody_id_t native_rigidbody = null_rigidbody;
            Rigidbody* _arch_updated_modify_hack = nullptr;

            bool rigidbody_just_created = false;
            math::vec2 record_body_scale = math::vec2(0.f, 0.f);
            float record_density = 0.f;
            float record_friction = 0.f;
            float record_restitution = 0.f;

            size_t layerid = 0;

            Rigidbody() = default;
            Rigidbody(Rigidbody&&) = default;
            Rigidbody(const Rigidbody& other)
                : layerid(other.layerid)
            {
                // Do nothing
            }
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Rigidbody::layerid, "layerid");
            }
        };
        struct Mass
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Mass);
            JECS_DEFAULT_CONSTRUCTOR(Mass);

            float density = 1.f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Mass::density, "density");
            }
        };
        struct Friction
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Friction);
            JECS_DEFAULT_CONSTRUCTOR(Friction);

            float value = 1.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Friction::value, "value");
            }
        };
        struct Restitution
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Restitution);
            JECS_DEFAULT_CONSTRUCTOR(Restitution);

            float value = 1.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Restitution::value, "value");
            }
        };
        struct Kinematics
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Kinematics);
            JECS_DEFAULT_CONSTRUCTOR(Kinematics);

            math::vec2 linear_velocity = {};
            float angular_velocity = 0.f;
            float linear_damping = 0.f;
            float angular_damping = 0.f;
            float gravity_scale = 1.f;

            bool lock_movement_x = false;
            bool lock_movement_y = false;
            bool lock_rotation = false;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Kinematics::linear_velocity, "linear_velocity");
                typing::register_member(guard, &Kinematics::angular_velocity, "angular_velocity");
                typing::register_member(guard, &Kinematics::linear_damping, "linear_damping");
                typing::register_member(guard, &Kinematics::angular_damping, "angular_damping");
                typing::register_member(guard, &Kinematics::gravity_scale, "gravity_scale");
                typing::register_member(guard, &Kinematics::lock_movement_x, "lock_movement_x");
                typing::register_member(guard, &Kinematics::lock_movement_y, "lock_movement_y");
                typing::register_member(guard, &Kinematics::lock_rotation, "lock_rotation");
            }
        };
        struct Bullet
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Bullet);
            JECS_DEFAULT_CONSTRUCTOR(Bullet);
        };
        namespace Transform
        {
            struct Position
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Position);
                JECS_DEFAULT_CONSTRUCTOR(Position);

                math::vec2 offset = { 0.f, 0.f };
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Position::offset, "offset");
                }
            };
            struct Rotation
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Rotation);
                JECS_DEFAULT_CONSTRUCTOR(Rotation);

                float angle = 0.f;
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Rotation::angle, "angle");
                }
            };
            struct Scale
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Scale);
                JECS_DEFAULT_CONSTRUCTOR(Scale);

                math::vec2 scale = { 1.f, 1.f };
                static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
                {
                    typing::register_member(guard, &Scale::scale, "scale");
                }
            };
        }
        namespace Collider
        {
            using shape_id_t = uint64_t;
            constexpr static shape_id_t null_shape = 0;

            struct Box
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Box);

                shape_id_t native_shape = null_shape;

                Box() = default;
                Box(Box&&) = default;
                Box(const Box&) {};
            };
            struct Circle
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Circle);

                shape_id_t native_shape = null_shape;

                Circle() = default;
                Circle(Circle&&) = default;
                Circle(const Circle&) {};
            };
            struct Capsule
            {
                JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Capsule);

                shape_id_t native_shape = null_shape;

                Capsule() = default;
                Capsule(Capsule&&) = default;
                Capsule(const Capsule&) {};
            };
        }
        struct CollisionResult
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(CollisionResult);

            struct collide_result
            {
                math::vec2 position;
                math::vec2 normalize;
            };
            basic::map<Rigidbody*, collide_result> results;

            CollisionResult() = default;
            CollisionResult(CollisionResult&&) {};
            CollisionResult(const CollisionResult&) {};

            const collide_result* check(Rigidbody* rigidbody) const
            {
                auto fnd = results.find(rigidbody);
                if (fnd == results.end())
                    return nullptr;
                return &fnd->v;
            }
        };
    }
    namespace Light2D
    {
        struct TopDown
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(TopDown);
            JECS_DEFAULT_CONSTRUCTOR(TopDown);
        };
        struct Gain
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Gain);
            JECS_DEFAULT_CONSTRUCTOR(Gain);

            float gain = 1.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Gain::gain, "gain");
            }
        };
        struct Range
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Range);
            JECS_DEFAULT_CONSTRUCTOR(Range);

            struct light_shape
            {
                size_t m_point_count;
                basic::vector<float> m_strength;
                basic::vector<math::vec2> m_positions;
                basic::optional<jeecs::basic::resource<jeecs::graphic::vertex>> m_light_mesh;

                light_shape()
                    : m_point_count(0)
                {
                }

                static const char* JEScriptTypeName()
                {
                    return "Light2D::Range::light_shape";
                }
                static const char* JEScriptTypeDeclare()
                {
                    return
                        "namespace Light2D::Range\n"
                        "{\n"
                        "    public using light_shape = struct{\n"
                        "        public m_point_count: int,\n"
                        "        public m_strength: array<float>,\n"
                        "        public m_positions: array<vec2>,\n"
                        "    };\n"
                        "}";
                }
                void JEParseFromScriptType(wo_vm vm, wo_value v)
                {
                    wo_value val = wo_register(vm, WO_REG_T0);
                    wo_value strengths = wo_register(vm, WO_REG_T1);
                    wo_value positions = wo_register(vm, WO_REG_T2);

                    wo_struct_get(val, v, 0);
                    wo_struct_get(strengths, v, 1);
                    wo_struct_get(positions, v, 2);
                    size_t position_count = (size_t)wo_int(val);
                    size_t layer_count = (size_t)wo_arr_len(strengths);

                    m_point_count = position_count;
                    m_positions.clear();
                    m_strength.clear();

                    m_light_mesh.reset();

                    for (size_t ilayer = 0; ilayer < layer_count; ++ilayer)
                    {
                        float strength = 0.0f;
                        if (wo_arr_try_get(val, strengths, ilayer))
                            strength = (float)wo_float(val);

                        m_strength.push_back(strength);

                        for (size_t iposition = 0; iposition < position_count; ++iposition)
                        {
                            math::vec2 pos = {};

                            if (wo_arr_try_get(val, positions, iposition + ilayer * position_count))
                                pos.JEParseFromScriptType(vm, val);

                            m_positions.push_back(pos);
                        }
                    }
                }
                void JEParseToScriptType(wo_vm vm, wo_value v) const
                {
                    wo_value val = wo_register(vm, WO_REG_T0);
                    wo_value arr = wo_register(vm, WO_REG_T1);

                    wo_set_struct(v, vm, 3);

                    wo_set_int(val, (wo_integer_t)m_point_count);
                    wo_struct_set(v, 0, val);

                    size_t layer_count = m_strength.size();

                    wo_set_arr(arr, vm, (wo_integer_t)layer_count);
                    for (size_t i = 0; i < layer_count; ++i)
                    {
                        wo_set_float(val, m_strength.at(i));
                        wo_arr_set(arr, (wo_integer_t)i, val);
                    }
                    wo_struct_set(v, 1, arr);

                    wo_set_arr(arr, vm, (wo_integer_t)m_positions.size());
                    for (size_t i = 0; i < m_positions.size(); ++i)
                    {
                        m_positions.at(i).JEParseToScriptType(vm, val);
                        wo_arr_set(arr, (wo_integer_t)i, val);
                    }
                    wo_struct_set(v, 2, arr);
                }
            };

            float decay = 1.0f;
            light_shape shape;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Range::decay, "decay");
                typing::register_member(guard, &Range::shape, "shape");
            }
        };
        struct Point
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Point);
            JECS_DEFAULT_CONSTRUCTOR(Point);

            float decay = 1.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Point::decay, "decay");
            }
        };
        struct Parallel
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Parallel);
            JECS_DEFAULT_CONSTRUCTOR(Parallel);
        };
        struct ShadowBuffer
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(ShadowBuffer);

            float resolution_ratio = 0.5f;
            basic::optional<basic::resource<graphic::framebuffer>> buffer;

            ShadowBuffer() = default;
            ShadowBuffer(const ShadowBuffer& another)
                : resolution_ratio(another.resolution_ratio)
            {
            }
            ShadowBuffer(ShadowBuffer&&) = default;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &ShadowBuffer::resolution_ratio, "resolution_ratio");
            }
        };
        struct CameraPostPass
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(CameraPostPass);

            basic::optional<basic::resource<graphic::framebuffer>> post_rend_target;
            basic::optional<basic::resource<jeecs::graphic::framebuffer>> post_light_target;

            float light_rend_ratio = 0.5f;

            CameraPostPass() = default;
            CameraPostPass(const CameraPostPass& another) : light_rend_ratio(another.light_rend_ratio) {}
            CameraPostPass(CameraPostPass&&) = default;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &CameraPostPass::light_rend_ratio, "light_rend_ratio");
            }
        };
        struct BlockShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(BlockShadow);
            JECS_DEFAULT_CONSTRUCTOR(BlockShadow);

            struct block_mesh
            {
                basic::vector<math::vec2> m_block_points = {
                    math::vec2(-0.5f, 0.5f),
                    math::vec2(-0.5f, -0.5f),
                    math::vec2(0.5f, -0.5f),
                    math::vec2(0.5f, 0.5f),
                    math::vec2(-0.5f, 0.5f),
                };
                basic::optional<basic::resource<graphic::vertex>> m_block_mesh;

                static const char* JEScriptTypeName()
                {
                    return "Light2D::BlockShadow::block_mesh";
                }
                static const char* JEScriptTypeDeclare()
                {
                    return
                        "namespace Light2D::BlockShadow\n"
                        "{\n"
                        "    public using block_mesh = array<vec2>;\n"
                        "}";
                }
                void JEParseFromScriptType(wo_vm vm, wo_value v)
                {
                    m_block_mesh.reset();

                    wo_value pos = wo_register(vm, WO_REG_T0);
                    size_t point_count = (size_t)wo_arr_len(v);

                    m_block_points.clear();

                    for (size_t i = 0; i < point_count; ++i)
                    {
                        wo_arr_get(pos, v, (wo_integer_t)i);

                        math::vec2 position;
                        position.JEParseFromScriptType(vm, pos);

                        m_block_points.push_back(position);
                    }
                }
                void JEParseToScriptType(wo_vm vm, wo_value v) const
                {
                    wo_value pos = wo_register(vm, WO_REG_T1);

                    wo_set_arr(v, vm, (wo_integer_t)m_block_points.size());

                    for (size_t i = 0; i < m_block_points.size(); ++i)
                    {
                        m_block_points.at(i).JEParseToScriptType(vm, pos);
                        wo_arr_set(v, (wo_integer_t)i, pos);
                    }
                }
            };

            block_mesh mesh;
            float factor = 1.0f;
            bool reverse = false;
            bool auto_disable = true;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &BlockShadow::mesh, "mesh");
                typing::register_member(guard, &BlockShadow::factor, "factor");
                typing::register_member(guard, &BlockShadow::reverse, "reverse");
                typing::register_member(guard, &BlockShadow::auto_disable, "auto_disable");
            }
        };
        struct ShapeShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(ShapeShadow);
            JECS_DEFAULT_CONSTRUCTOR(ShapeShadow);

            float factor = 1.0f;
            float distance = 1.0f;
            math::vec2 tiling_scale = math::vec2(1.f, 1.f);
            bool auto_disable = true;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &ShapeShadow::factor, "factor");
                typing::register_member(guard, &ShapeShadow::distance, "distance");
                typing::register_member(guard, &ShapeShadow::tiling_scale, "tiling_scale");
                typing::register_member(guard, &ShapeShadow::auto_disable, "auto_disable");
            }
        };
        struct SpriteShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(SpriteShadow);
            JECS_DEFAULT_CONSTRUCTOR(SpriteShadow);

            float factor = 1.0f;
            float distance = 1.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &SpriteShadow::factor, "factor");
                typing::register_member(guard, &SpriteShadow::distance, "distance");
            }
        };
        struct SelfShadow
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(SelfShadow);
            JECS_DEFAULT_CONSTRUCTOR(SelfShadow);

            float factor = 1.0f;
            bool auto_disable = true;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &SelfShadow::factor, "factor");
                typing::register_member(guard, &SelfShadow::auto_disable, "auto_disable");
            }
        };
    }
    namespace Animation
    {
        struct FrameAnimation
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(FrameAnimation);
            JECS_DEFAULT_CONSTRUCTOR(FrameAnimation);

            struct animation_list
            {
                struct frame_data
                {
                    struct data_value
                    {
                        enum type : uint8_t
                        {
                            INT,
                            FLOAT,
                            VEC2,
                            VEC3,
                            VEC4,
                            QUAT4,
                        };
                        union value
                        {
                            int i32;
                            float f32;
                            math::vec2 v2;
                            math::vec3 v3;
                            math::vec4 v4;
                            math::quat q4;
                        };

                        type m_type = type::INT;
                        value m_value = { 0 };

                        data_value() noexcept = default;
                        data_value(const data_value& val) noexcept
                        {
                            m_type = val.m_type;
                            memcpy((void*)&m_value, &val.m_value, sizeof(value));
                        }
                        data_value(data_value&& val) noexcept
                        {
                            m_type = val.m_type;
                            memcpy((void*)&m_value, &val.m_value, sizeof(value));
                        }
                        data_value& operator=(const data_value& val) noexcept
                        {
                            m_type = val.m_type;
                            memcpy((void*)&m_value, &val.m_value, sizeof(value));
                            return *this;
                        }
                        data_value& operator=(data_value&& val) noexcept
                        {
                            m_type = val.m_type;
                            memcpy((void*)&m_value, &val.m_value, sizeof(value));
                            return *this;
                        }
                    };
                    struct component_data
                    {
                        const jeecs::typing::type_info*
                            m_component_type;
                        const jeecs::typing::typeinfo_member::member_info*
                            m_member_info;
                        data_value m_member_value;
                        bool m_offset_mode;

                        void* m_member_addr_cache;
                        jeecs::game_entity m_entity_cache;
                    };
                    struct uniform_data
                    {
                        basic::string m_uniform_name;
                        data_value m_uniform_value;
                    };

                    float m_frame_time;
                    basic::vector<component_data> m_component_data;
                    basic::vector<uniform_data> m_uniform_data;
                };
                struct animation_data
                {
                    basic::vector<frame_data> frames;
                };

                struct animation_data_set
                {
                    basic::map<basic::string, animation_data> m_animations;
                    basic::string m_path;

                    basic::string m_current_action = "";
                    size_t m_current_frame_index = SIZE_MAX;
                    double m_next_update_time = 0.0f;
                    float m_last_speed = 1.0f;

                    bool m_loop = false;

                    void set_loop(bool loop)
                    {
                        m_loop = loop;
                    }
                    void set_action(const std::string& animation_name)
                    {
                        m_current_action = animation_name;
                        m_current_frame_index = SIZE_MAX;
                        m_next_update_time = 0.0f;
                    }
                    std::string get_action() const
                    {
                        return m_current_action.c_str();
                    }

                    void load_animation(const std::string& str)
                    {
                        m_animations.clear();
                        m_path = str;

                        if (str != "")
                        {
                            auto* file_handle = jeecs_file_open(m_path.c_str());
                            if (file_handle == nullptr)
                            {
                                jeecs::debug::logerr("Cannot open animation file '%s'.", str.c_str());

                                return;
                            }
                            else
                            {
                                // 0. 读取魔数，验证文件是否是合法的动画文件
                                uint32_t mg_number = 0;
                                jeecs_file_read(&mg_number, sizeof(uint32_t), 1, file_handle);
                                if (mg_number != 0xA213A710u)
                                {
                                    jeecs::debug::logerr("Invalid animation file '%s'.", str.c_str());
                                    jeecs_file_close(file_handle);
                                    return;
                                }

                                // 1. 读取动画组数量
                                uint64_t animation_count = 0;
                                jeecs_file_read(&animation_count, sizeof(uint64_t), 1, file_handle);

                                for (uint64_t i = 0; i < animation_count; ++i)
                                {
                                    // 2. 读取当前动画的帧数量、名称
                                    uint64_t frame_count = 0;
                                    uint64_t frames_name_len = 0;

                                    jeecs_file_read(&frame_count, sizeof(uint64_t), 1, file_handle);
                                    jeecs_file_read(&frames_name_len, sizeof(uint64_t), 1, file_handle);

                                    std::string frame_name((size_t)frames_name_len, '\0');
                                    jeecs_file_read(frame_name.data(), sizeof(char), (size_t)frames_name_len, file_handle);

                                    auto& animation_frame_datas = m_animations[frame_name.c_str()];

                                    for (uint64_t j = 0; j < frame_count; ++j)
                                    {
                                        // 3. 读取每一帧的持续时间和数据数量
                                        frame_data frame_dat;
                                        jeecs_file_read(&frame_dat.m_frame_time, sizeof(frame_dat.m_frame_time), 1, file_handle);

                                        uint64_t component_data_count = 0;
                                        jeecs_file_read(&component_data_count, sizeof(component_data_count), 1, file_handle);
                                        for (uint64_t k = 0; k < component_data_count; ++k)
                                        {
                                            // 4. 读取帧组件数据
                                            uint64_t component_name_len = 0;
                                            jeecs_file_read(&component_name_len, sizeof(uint64_t), 1, file_handle);

                                            std::string component_name((size_t)component_name_len, '\0');
                                            jeecs_file_read(component_name.data(), sizeof(char), (size_t)component_name_len, file_handle);

                                            uint64_t member_name_len = 0;
                                            jeecs_file_read(&member_name_len, sizeof(uint64_t), 1, file_handle);

                                            std::string member_name((size_t)member_name_len, '\0');
                                            jeecs_file_read(member_name.data(), sizeof(char), (size_t)member_name_len, file_handle);

                                            uint8_t offset_mode = 0;
                                            jeecs_file_read(&offset_mode, sizeof(offset_mode), 1, file_handle);

                                            frame_data::data_value value;
                                            jeecs_file_read(&value.m_type, sizeof(value.m_type), 1, file_handle);
                                            switch (value.m_type)
                                            {
                                            case frame_data::data_value::type::INT:
                                                jeecs_file_read(&value.m_value.i32, sizeof(value.m_value.i32), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::FLOAT:
                                                jeecs_file_read(&value.m_value.f32, sizeof(value.m_value.f32), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::VEC2:
                                                jeecs_file_read(&value.m_value.v2, sizeof(value.m_value.v2), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::VEC3:
                                                jeecs_file_read(&value.m_value.v3, sizeof(value.m_value.v3), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::VEC4:
                                                jeecs_file_read(&value.m_value.v4, sizeof(value.m_value.v4), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::QUAT4:
                                                jeecs_file_read(&value.m_value.q4, sizeof(value.m_value.q4), 1, file_handle);
                                                break;
                                            default:
                                                jeecs::debug::logerr("Unknown value type(%d) for component frame data when reading animation '%s' frame %zu in '%s'.",
                                                    (int)value.m_type, frame_name.c_str(), (size_t)j, str.c_str());
                                                break;
                                            }

                                            auto* component_type = jeecs::typing::type_info::of(component_name.c_str());
                                            if (component_type == nullptr)
                                                jeecs::debug::logerr("Failed to found component type named '%s' when reading animation '%s' frame %zu in '%s'.",
                                                    component_name.c_str(), frame_name.c_str(), (size_t)j, str.c_str());
                                            else
                                            {
                                                frame_data::component_data cdata;
                                                cdata.m_component_type = component_type;
                                                cdata.m_member_info = component_type->find_member_by_name(member_name.c_str());
                                                cdata.m_member_value = value;
                                                cdata.m_offset_mode = offset_mode != 0;
                                                cdata.m_member_addr_cache = nullptr;
                                                cdata.m_entity_cache = {};

                                                if (cdata.m_member_info == nullptr)
                                                    jeecs::debug::logerr("Component '%s' donot have member named '%s' when reading animation '%s' frame %zu in '%s'.",
                                                        component_name.c_str(), member_name.c_str(), frame_name.c_str(), (size_t)j, str.c_str());
                                                else
                                                    frame_dat.m_component_data.push_back(cdata);
                                            }
                                        }

                                        uint64_t uniform_data_count = 0;
                                        jeecs_file_read(&uniform_data_count, sizeof(uniform_data_count), 1, file_handle);
                                        for (uint64_t k = 0; k < uniform_data_count; ++k)
                                        {
                                            // 5. 读取帧一致变量数据
                                            uint64_t uniform_name_len = 0;
                                            jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, file_handle);

                                            std::string uniform_name((size_t)uniform_name_len, '\0');
                                            jeecs_file_read(uniform_name.data(), sizeof(char), (size_t)uniform_name_len, file_handle);

                                            frame_data::data_value value;
                                            jeecs_file_read(&value.m_type, sizeof(value.m_type), 1, file_handle);
                                            switch (value.m_type)
                                            {
                                            case frame_data::data_value::type::INT:
                                                jeecs_file_read(&value.m_value.i32, sizeof(value.m_value.i32), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::FLOAT:
                                                jeecs_file_read(&value.m_value.f32, sizeof(value.m_value.f32), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::VEC2:
                                                jeecs_file_read(&value.m_value.v2, sizeof(value.m_value.v2), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::VEC3:
                                                jeecs_file_read(&value.m_value.v3, sizeof(value.m_value.v3), 1, file_handle);
                                                break;
                                            case frame_data::data_value::type::VEC4:
                                                jeecs_file_read(&value.m_value.v4, sizeof(value.m_value.v4), 1, file_handle);
                                                break;
                                            default:
                                                jeecs::debug::logerr("Unknown value type(%d) for uniform frame data when reading animation '%s' frame %zu in '%s'.",
                                                    (int)value.m_type, str.c_str());
                                                break;
                                            }

                                            frame_data::uniform_data udata;
                                            udata.m_uniform_name = uniform_name;
                                            udata.m_uniform_value = value;

                                            frame_dat.m_uniform_data.push_back(udata);
                                        }

                                        animation_frame_datas.frames.push_back(frame_dat);
                                    }
                                }

                                // OK，读取完毕！
                                jeecs_file_close(file_handle);
                            }
                        }
                    }
                };
                basic::vector<animation_data_set> m_animations;

                void active_action(size_t id, const char* act_name, bool loop)
                {
                    if (id < m_animations.size())
                    {
                        if (m_animations[id].get_action() != act_name)
                            m_animations[id].set_action(act_name);
                        m_animations[id].set_loop(loop);
                    }
                }
                bool is_playing(size_t id) const
                {
                    if (id < m_animations.size())
                        return m_animations[id].m_current_action != "";
                    return false;
                }

                static const char* JEScriptTypeName()
                {
                    return "Animation::FrameAnimation::animation_list";
                }
                static const char* JEScriptTypeDeclare()
                {
                    return
                        "namespace Animation::FrameAnimation\n"
                        "{\n"
                        "    public using animation_state = struct{\n"
                        "        public m_path: string,\n"
                        "        public m_animation: string,\n"
                        "        public m_loop: bool,\n"
                        "    };\n"
                        "    public using animation_list = array<animation_state>;\n"
                        "}";
                }
                void JEParseFromScriptType(wo_vm vm, wo_value v)
                {
                    m_animations.clear();

                    wo_value animation = wo_register(vm, WO_REG_T0);
                    wo_value tmp = wo_register(vm, WO_REG_T1);
                    size_t animation_count = (size_t)wo_arr_len(v);

                    for (size_t i = 0; i < animation_count; ++i)
                    {
                        m_animations.push_back(animation_data_set{});
                        auto& animation_inst = m_animations.back();

                        wo_arr_get(animation, v, (wo_integer_t)i);

                        wo_struct_get(tmp, animation, 0);
                        animation_inst.load_animation(wo_string(tmp));

                        wo_struct_get(tmp, animation, 1);
                        animation_inst.set_action(wo_string(tmp));

                        wo_struct_get(tmp, animation, 2);
                        animation_inst.set_loop(wo_bool(tmp));
                    }
                }
                void JEParseToScriptType(wo_vm vm, wo_value v) const
                {
                    wo_value animation = wo_register(vm, WO_REG_T0);
                    wo_value tmp = wo_register(vm, WO_REG_T1);

                    wo_set_arr(v, vm, (wo_integer_t)m_animations.size());

                    for (size_t i = 0; i < m_animations.size(); ++i)
                    {
                        auto animation_inst = m_animations.at(i);
                        wo_set_struct(animation, vm, 3);

                        wo_set_string(tmp, vm, animation_inst.m_path.c_str());
                        wo_struct_set(animation, 0, tmp);

                        wo_set_string(tmp, vm, animation_inst.get_action().c_str());
                        wo_struct_set(animation, 1, tmp);

                        wo_set_bool(tmp, animation_inst.m_loop);
                        wo_struct_set(animation, 2, tmp);

                        wo_arr_set(v, (wo_integer_t)i, animation);
                    }
                }
            };

            animation_list animations;
            float jitter = 0.0f;
            float speed = 1.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &FrameAnimation::animations, "animations");
                typing::register_member(guard, &FrameAnimation::jitter, "jitter");
                typing::register_member(guard, &FrameAnimation::speed, "speed");
            }
        };
    }
    namespace Audio
    {
        struct Source
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Source);

            basic::resource<audio::source> source;

            float pitch;
            float volume;

            math::vec3 last_position;

            Source() noexcept
                : source(audio::source::create()), pitch(1.0f), volume(1.0f)

            {
            }
            Source(const Source& another) noexcept
                : Source()
            {
                pitch = another.pitch;
                volume = another.volume;
                last_position = another.last_position;
            }
            Source(Source&& another) noexcept
                : source(std::move(another.source)), pitch(another.pitch), volume(another.volume), last_position(another.last_position)
            {
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Source::pitch, "pitch");
                typing::register_member(guard, &Source::volume, "volume");
            }
        };
        struct Listener
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Listener);
            JECS_DEFAULT_CONSTRUCTOR(Listener);

            float volume = 1.0f;
            math::vec3 face = { 0, 0, 1 };
            math::vec3 up = { 0, 1, 0 };

            math::vec3 last_position = {};

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Listener::volume, "volume");
                typing::register_member(guard, &Listener::face, "face");
                typing::register_member(guard, &Listener::up, "up");
            }
        };
        struct Playing
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Playing);
            JECS_DEFAULT_CONSTRUCTOR(Playing);

            basic::fileresource<audio::buffer> buffer;
            bool is_playing = false;

            bool play = true;
            bool loop = true;

            void set_buffer(const basic::resource<audio::buffer>& buf)
            {
                buffer.set_resource(buf);
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Playing::buffer, "buffer");
                typing::register_member(guard, &Playing::play, "play");
                typing::register_member(guard, &Playing::loop, "loop");
            }
        };
    }
    namespace Input
    {
        struct VirtualGamepad
        {
            JECS_DISABLE_MOVE_AND_COPY_OPERATOR(VirtualGamepad);

            je_io_gamepad_handle_t gamepad;

            basic::map<input::keycode, input::gamepadcode> keymap;
            input::keycode left_stick_up_left_down_right[4];

            VirtualGamepad()
                : gamepad(je_io_create_gamepad(nullptr, nullptr)), left_stick_up_left_down_right{
                                                                       input::keycode::W,
                                                                       input::keycode::A,
                                                                       input::keycode::S,
                                                                       input::keycode::D }
            {
                keymap[input::keycode::UP] = input::gamepadcode::UP;
                keymap[input::keycode::DOWN] = input::gamepadcode::DOWN;
                keymap[input::keycode::LEFT] = input::gamepadcode::LEFT;
                keymap[input::keycode::RIGHT] = input::gamepadcode::RIGHT;

                keymap[input::keycode::ENTER] = input::gamepadcode::START;
                keymap[input::keycode::ESC] = input::gamepadcode::SELECT;
            }
            VirtualGamepad(const VirtualGamepad& another)
                : gamepad(je_io_create_gamepad(nullptr, nullptr)), keymap(another.keymap), left_stick_up_left_down_right{
                                                                                               another.left_stick_up_left_down_right[0],
                                                                                               another.left_stick_up_left_down_right[1],
                                                                                               another.left_stick_up_left_down_right[2],
                                                                                               another.left_stick_up_left_down_right[3] }
            {
            }
            VirtualGamepad(VirtualGamepad&& another)
                : gamepad(another.gamepad), keymap(std::move(another.keymap)), left_stick_up_left_down_right{
                                                                                   another.left_stick_up_left_down_right[0],
                                                                                   another.left_stick_up_left_down_right[1],
                                                                                   another.left_stick_up_left_down_right[2],
                                                                                   another.left_stick_up_left_down_right[3] }
            {
                another.gamepad = nullptr;
            }
            ~VirtualGamepad()
            {
                if (gamepad != nullptr)
                    je_io_close_gamepad(gamepad);
            }
        };
    }

    namespace math
    {
        struct ray
        {
        public:
            vec3 orgin = { 0, 0, 0 };
            vec3 direction = { 0, 0, 1 };

            ray() = default;

            ray(ray&&) = default;
            ray(const ray&) = default;

            ray& operator=(ray&&) = default;
            ray& operator=(const ray&) = default;

            ray(const vec3& _orgin, const vec3& _direction) : orgin(_orgin),
                direction(_direction)
            {
            }
            ray(const Transform::Translation& camera_trans, const Camera::Projection& camera_proj, const vec2& screen_pos, bool ortho)
            {
                // 根据摄像机和屏幕坐标创建射线
                float ray_eye[4] = { screen_pos.x, screen_pos.y, 1.0f, 1.0f };

                float ray_world[4];
                mat4xvec4(ray_world, camera_proj.inv_projection, ray_eye);

                if (ray_world[3] != 0.0f)
                {
                    // is perspective
                    ray_world[0] /= ray_world[3];
                    ray_world[1] /= ray_world[3];
                    ray_world[2] /= ray_world[3];
                }

                if (ortho)
                {
                    // not perspective
                    orgin = camera_trans.world_position + camera_trans.world_rotation * vec3{ ray_world[0], ray_world[1], 0 };
                    direction = vec3(0, 0, 1);
                }
                else
                {
                    vec3 ray_dir(ray_world[0], ray_world[1], ray_world[2]);
                    orgin = camera_trans.world_position;
                    direction = (camera_trans.world_rotation * ray_dir).unit();
                }
            }

            struct intersect_result
            {
                bool intersected = false;
                vec3 place = {};
                float distance = INFINITY;

                intersect_result() = default;
                intersect_result(bool rslt, float dist = INFINITY, const vec3& plce = vec3(0, 0, 0)) : intersected(rslt),
                    place(plce),
                    distance(dist)
                {
                }
            };

            intersect_result intersect_triangle(const vec3& v0, const vec3& v1, const vec3& v2) const
            {
                float _t, _u, _v;
                float* t = &_t, * u = &_u, * v = &_v;

                // E1
                vec3 E1 = v1 - v0;
                // E2
                vec3 E2 = v2 - v0;
                // P
                vec3 P = direction.cross(E2);
                // determinant
                float det = E1.dot(P);
                // keep det > 0, modify T accordingly
                vec3 T;
                if (det > 0)
                {
                    T = orgin - v0;
                }
                else
                {
                    T = v0 - orgin;
                    det = -det;
                }
                // If determinant is near zero, ray lies in plane of triangle
                if (det < 0.0001f)
                    return false;
                // Calculate u and make sure u <= 1

                *u = T.dot(P); // T.dot(P);
                if (*u < 0.0f || *u > det)
                    return false;

                // Q

                vec3 Q = T.cross(E1); // T.Cross(E1);

                // Calculate v and make sure u + v <= 1
                *v = direction.dot(Q); // direction.dot(Q);

                if (*v < 0.0f || *u + *v > det)
                    return false;

                // Calculate t, scale parameters, ray intersects triangle

                *t = E2.dot(Q); // .dot(Q);

                float fInvDet = 1.0f / det;
                *t *= fInvDet;
                *u *= fInvDet;
                *v *= fInvDet;

                auto&& clid = ((1.0f - *u - *v) * v0 + *u * v1 + *v * v2);
                auto&& delta = clid - orgin;

                if (delta.dot(direction) < 0)
                {
                    return false;
                }
                // if (vec3::dot(delta, direction) < 0.0f)
                //	return false;
                return intersect_result(true, delta.length(), clid);
            }
            intersect_result intersect_rectangle(const vec3& v0, const vec3& v1, const vec3& v2, const vec3& v3) const
            {
                /*

                v0-----------v3
                |             |
                |             |
                |             |
                v1-----------v2

                */

                auto&& inresult = intersect_triangle(v0, v1, v2);
                if (inresult.intersected)
                    return inresult;
                return intersect_triangle(v0, v3, v2);
            }
            intersect_result intersect_box(const vec3& offset, const vec3& size, const quat& rotation) const
            {
                /*
                        4----------5
                    /   |      /   |
                0----------1       |
                |       |  |       |
                |       |  |       |
                |       |  |       |
                |       6--|-------7
                |   /     |	   /
                2----------3

                */
                vec3 finalBoxPos[8];
                intersect_result minResult = false;
                minResult.distance = INFINITY;

                // pos
                finalBoxPos[0].x = finalBoxPos[2].x = finalBoxPos[4].x = finalBoxPos[6].x =
                    -(finalBoxPos[1].x = finalBoxPos[3].x = finalBoxPos[5].x = finalBoxPos[7].x = size.x / 2.0f);
                finalBoxPos[2].y = finalBoxPos[3].y = finalBoxPos[6].y = finalBoxPos[7].y =
                    -(finalBoxPos[0].y = finalBoxPos[1].y = finalBoxPos[4].y = finalBoxPos[5].y = size.y / 2.0f);
                finalBoxPos[0].z = finalBoxPos[1].z = finalBoxPos[2].z = finalBoxPos[3].z =
                    -(finalBoxPos[4].z = finalBoxPos[5].z = finalBoxPos[6].z = finalBoxPos[7].z = size.z / 2.0f);

                // rot and transform
                for (int i = 0; i < 8; i++)
                    finalBoxPos[i] = (rotation * finalBoxPos[i]) + offset;

                // front
                {
                    auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[3], finalBoxPos[2]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // back
                {
                    auto&& f = intersect_rectangle(finalBoxPos[4], finalBoxPos[5], finalBoxPos[7], finalBoxPos[6]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // left
                {
                    auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[2], finalBoxPos[6], finalBoxPos[4]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // right
                {
                    auto&& f = intersect_rectangle(finalBoxPos[1], finalBoxPos[3], finalBoxPos[7], finalBoxPos[5]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // top
                {
                    auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[5], finalBoxPos[4]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // bottom
                {
                    auto&& f = intersect_rectangle(finalBoxPos[2], finalBoxPos[3], finalBoxPos[7], finalBoxPos[6]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }

                return minResult;
            }
            intersect_result intersect_mesh(
                const basic::resource<graphic::vertex>& mesh,
                const vec3& offset,
                const quat& rotation,
                const vec3& scale) const
            {
                jegl_vertex* raw_vertex_data = mesh->resource()->m_raw_vertex_data;

                intersect_result minResult = false;
                minResult.distance = INFINITY;

                switch (raw_vertex_data->m_type)
                {
                case jegl_vertex::type::TRIANGLES:
                    for (size_t i = 0; i + 2 < raw_vertex_data->m_index_count; i += 3)
                    {
                        const float* point_0 =
                            std::launder(reinterpret_cast<const float*>(
                                (intptr_t)raw_vertex_data->m_vertexs + raw_vertex_data->m_data_size_per_point * i));
                        const float* point_1 =
                            std::launder(reinterpret_cast<const float*>(
                                (intptr_t)raw_vertex_data->m_vertexs + raw_vertex_data->m_data_size_per_point * (i + 1)));
                        const float* point_2 =
                            std::launder(reinterpret_cast<const float*>(
                                (intptr_t)raw_vertex_data->m_vertexs + raw_vertex_data->m_data_size_per_point * (i + 2)));

                        vec3 triangle_point[3] = {
                            {point_0[0], point_0[1], point_0[2]},
                            {point_1[0], point_1[1], point_1[2]},
                            {point_2[0], point_2[1], point_2[2]} };

                        auto&& f = intersect_triangle(
                            offset + rotation * triangle_point[0] * scale,
                            offset + rotation * triangle_point[1] * scale,
                            offset + rotation * triangle_point[2] * scale);

                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    break;
                case jegl_vertex::type::TRIANGLESTRIP:
                {
                    const float* point_0 =
                        std::launder(reinterpret_cast<const float*>(
                            (intptr_t)raw_vertex_data->m_vertexs));
                    const float* point_1 =
                        std::launder(reinterpret_cast<const float*>(
                            (intptr_t)raw_vertex_data->m_vertexs + raw_vertex_data->m_data_size_per_point));

                    vec3 triangle_point[3] = {
                        {point_0[0], point_0[1], point_0[2]},
                        {point_1[0], point_1[1], point_1[2]},
                        {},
                    };

                    for (size_t i = 2; i < raw_vertex_data->m_index_count; ++i)
                    {
                        const float* point = std::launder(reinterpret_cast<const float*>(
                            (intptr_t)raw_vertex_data->m_vertexs + raw_vertex_data->m_data_size_per_point * i));

                        triangle_point[i % 3] = { point[0], point[1], point[2] };

                        auto&& f = intersect_triangle(
                            offset + rotation * triangle_point[0] * scale,
                            offset + rotation * triangle_point[1] * scale,
                            offset + rotation * triangle_point[2] * scale);

                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    break;
                }
                default:
                    // Support triangles only
                    break;
                }

                return minResult;
            }

            intersect_result intersect_entity(
                const Transform::Translation& translation,
                const Renderer::Shape* entity_shape_may_null,
                bool consider_mesh) const
            {
                vec3 entity_box_center, entity_box_size;
                if (entity_shape_may_null)
                {
                    if (entity_shape_may_null->vertex.has_value())
                    {
                        auto* vertex_dat = entity_shape_may_null->vertex.value()->resource()->m_raw_vertex_data;
                        entity_box_center = vec3(
                            (vertex_dat->m_x_max + vertex_dat->m_x_min) / 2.0f,
                            (vertex_dat->m_y_max + vertex_dat->m_y_min) / 2.0f,
                            (vertex_dat->m_z_max + vertex_dat->m_z_min) / 2.0f);
                        entity_box_size = vec3(
                            vertex_dat->m_x_max - vertex_dat->m_x_min,
                            vertex_dat->m_y_max - vertex_dat->m_y_min,
                            vertex_dat->m_z_max - vertex_dat->m_z_min);
                    }
                    else
                    {
                        // Default shape size
                        entity_box_center = vec3(0.f, 0.f, 0.f);
                        entity_box_size = vec3(1.f, 1.f, 0.f);
                    }
                }
                else
                {
                    // Treat as a box
                    entity_box_center = vec3(0.f, 0.f, 0.f);
                    entity_box_size = vec3(1.f, 1.f, 1.f);
                }

                intersect_result minResult =
                    intersect_box(
                        entity_box_center + translation.world_position,
                        translation.local_scale * entity_box_size,
                        translation.world_rotation);

                if (minResult.intersected && consider_mesh && entity_shape_may_null != nullptr && entity_shape_may_null->vertex.has_value())
                {
                    return intersect_mesh(
                        entity_shape_may_null->vertex.value(),
                        translation.world_position,
                        translation.world_rotation,
                        translation.local_scale);
                }

                return minResult;
            }
        };
    }
    namespace entry
    {
        inline void module_entry(jeecs::typing::type_unregister_guard* guard)
        {
            // 0. register built-in components
            using namespace typing;

            type_info::register_type<Transform::LocalPosition>(guard, "Transform::LocalPosition");
            type_info::register_type<Transform::LocalRotation>(guard, "Transform::LocalRotation");
            type_info::register_type<Transform::LocalScale>(guard, "Transform::LocalScale");
            type_info::register_type<Transform::Anchor>(guard, "Transform::Anchor");
            type_info::register_type<Transform::LocalToWorld>(guard, "Transform::LocalToWorld");
            type_info::register_type<Transform::LocalToParent>(guard, "Transform::LocalToParent");
            type_info::register_type<Transform::Translation>(guard, "Transform::Translation");

            type_info::register_type<UserInterface::Origin>(guard, "UserInterface::Origin");
            type_info::register_type<UserInterface::Rotation>(guard, "UserInterface::Rotation");
            type_info::register_type<UserInterface::Absolute>(guard, "UserInterface::Absolute");
            type_info::register_type<UserInterface::Relatively>(guard, "UserInterface::Relatively");

            type_info::register_type<Renderer::Rendqueue>(guard, "Renderer::Rendqueue");
            type_info::register_type<Renderer::Shape>(guard, "Renderer::Shape");
            type_info::register_type<Renderer::Shaders>(guard, "Renderer::Shaders");
            type_info::register_type<Renderer::Textures>(guard, "Renderer::Textures");
            type_info::register_type<Renderer::Color>(guard, "Renderer::Color");

            type_info::register_type<Animation::FrameAnimation>(guard, "Animation::FrameAnimation");

            type_info::register_type<Camera::Clip>(guard, "Camera::Clip");
            type_info::register_type<Camera::FrustumCulling>(guard, "Camera::FrustumCulling");
            type_info::register_type<Camera::Projection>(guard, "Camera::Projection");
            type_info::register_type<Camera::OrthoProjection>(guard, "Camera::OrthoProjection");
            type_info::register_type<Camera::PerspectiveProjection>(guard, "Camera::PerspectiveProjection");
            type_info::register_type<Camera::Viewport>(guard, "Camera::Viewport");
            type_info::register_type<Camera::RendToFramebuffer>(guard, "Camera::RendToFramebuffer");
            type_info::register_type<Camera::Clear>(guard, "Camera::Clear");

            type_info::register_type<Light2D::TopDown>(guard, "Light2D::TopDown");
            type_info::register_type<Light2D::Gain>(guard, "Light2D::Gain");
            type_info::register_type<Light2D::Point>(guard, "Light2D::Point");
            type_info::register_type<Light2D::Range>(guard, "Light2D::Range");
            type_info::register_type<Light2D::Parallel>(guard, "Light2D::Parallel");
            type_info::register_type<Light2D::ShadowBuffer>(guard, "Light2D::ShadowBuffer");
            type_info::register_type<Light2D::CameraPostPass>(guard, "Light2D::CameraPostPass");
            type_info::register_type<Light2D::BlockShadow>(guard, "Light2D::BlockShadow");
            type_info::register_type<Light2D::ShapeShadow>(guard, "Light2D::ShapeShadow");
            type_info::register_type<Light2D::SpriteShadow>(guard, "Light2D::SpriteShadow");
            type_info::register_type<Light2D::SelfShadow>(guard, "Light2D::SelfShadow");

            type_info::register_type<Physics2D::World>(guard, "Physics2D::World");
            type_info::register_type<Physics2D::Rigidbody>(guard, "Physics2D::Rigidbody");
            type_info::register_type<Physics2D::Kinematics>(guard, "Physics2D::Kinematics");
            type_info::register_type<Physics2D::Mass>(guard, "Physics2D::Mass");
            type_info::register_type<Physics2D::Bullet>(guard, "Physics2D::Bullet");
            type_info::register_type<Physics2D::Collider::Box>(guard, "Physics2D::Collider::Box");
            type_info::register_type<Physics2D::Collider::Circle>(guard, "Physics2D::Collider::Circle");
            type_info::register_type<Physics2D::Collider::Capsule>(guard, "Physics2D::Collider::Capsule");
            type_info::register_type<Physics2D::Transform::Position>(guard, "Physics2D::Transform::Position");
            type_info::register_type<Physics2D::Transform::Rotation>(guard, "Physics2D::Transform::Rotation");
            type_info::register_type<Physics2D::Transform::Scale>(guard, "Physics2D::Transform::Scale");
            type_info::register_type<Physics2D::Restitution>(guard, "Physics2D::Restitution");
            type_info::register_type<Physics2D::Friction>(guard, "Physics2D::Friction");
            type_info::register_type<Physics2D::CollisionResult>(guard, "Physics2D::CollisionResult");

            type_info::register_type<Audio::Source>(guard, "Audio::Source");
            type_info::register_type<Audio::Listener>(guard, "Audio::Listener");
            type_info::register_type<Audio::Playing>(guard, "Audio::Playing");

            type_info::register_type<Input::VirtualGamepad>(guard, "Input::VirtualGamepad");

            // 1. register basic types
            type_info::register_type<math::ivec2>(guard, nullptr);

            typing::register_script_parser<basic::fileresource<void>>(
                guard,
                [](const basic::fileresource<void>* v, wo_vm vm, wo_value value)
                {
                    wo_value result = wo_register(vm, WO_REG_T0);
                    wo_set_struct(value, vm, 1);

                    if (v->has_resource())
                        wo_set_option_string(result, vm, v->get_path()->c_str());
                    else
                        wo_set_option_none(result, vm);

                    wo_struct_set(value, 0, result);
                },
                [](basic::fileresource<void>* v, wo_vm vm, wo_value value)
                {
                    wo_value result = wo_register(vm, WO_REG_T0);
                    wo_struct_get(result, value, 0);

                    if (wo_option_get(result, result))
                        v->load(wo_string(result));
                    else
                        v->clear();
                },
                "fileresource_void",
                "public using fileresource_void = struct{ public path: option<string> };");

            typing::register_script_parser<basic::fileresource<audio::buffer>>(
                guard,
                [](const basic::fileresource<audio::buffer>* v, wo_vm vm, wo_value value)
                {
                    wo_value result = wo_register(vm, WO_REG_T0);
                    wo_set_struct(value, vm, 1);

                    if (v->has_resource())
                        wo_set_option_string(result, vm, v->get_path()->c_str());
                    else
                        wo_set_option_none(result, vm);

                    wo_struct_set(value, 0, result);
                },
                [](basic::fileresource<audio::buffer>* v, wo_vm vm, wo_value value)
                {
                    wo_value result = wo_register(vm, WO_REG_T0);
                    wo_struct_get(result, value, 0);

                    if (wo_option_get(result, result))
                        v->load(wo_string(result));
                    else
                        v->clear();
                },
                "fileresource_audio_buffer",
                "public using fileresource_audio_buffer = fileresource_void;");

            typing::register_script_parser<bool>(
                guard,
                [](const bool* v, wo_vm, wo_value value)
                {
                    wo_set_bool(value, *v);
                },
                [](bool* v, wo_vm, wo_value value)
                {
                    *v = wo_bool(value);
                },
                "bool", "");

            auto integer_uniform_parser_c2w = [](const auto* v, wo_vm, wo_value value)
                {
                    wo_set_int(value, (wo_integer_t)*v);
                };
            auto integer_uniform_parser_w2c = [](auto* v, wo_vm, wo_value value)
                {
                    *v = (typename std::remove_reference<decltype(*v)>::type)wo_int(value);
                };
            typing::register_script_parser<int8_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int8", "public alias int8 = int;");
            typing::register_script_parser<int16_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int16", "public alias int16 = int;");
            typing::register_script_parser<int32_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int32", "public alias int32 = int;");
            typing::register_script_parser<int64_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int64", "public alias int64 = int;");
            typing::register_script_parser<uint8_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint8", "public alias uint8 = int;");
            typing::register_script_parser<uint16_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint16", "public alias uint16 = int;");
            typing::register_script_parser<uint32_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint32", "public alias uint32 = int;");
            typing::register_script_parser<uint64_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint64", "public alias uint64 = int;");

            if constexpr (!std::is_same_v<size_t, uint32_t> && !std::is_same_v<size_t, uint64_t>)
            {
                // In webgl, size_t is a individual type, not a alias of uint32_t or uint64_t.
                static_assert(!std::is_signed_v<size_t>);
                if constexpr (sizeof(size_t) == sizeof(uint32_t))
                {
                    typing::register_script_parser<size_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                        "uint32", "public alias uint32 = int;");
                }
                else if constexpr (sizeof(size_t) == sizeof(uint64_t))
                {
                    typing::register_script_parser<size_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                        "uint64", "public alias uint64 = int;");
                }
                else
                {
                    // Unknown size_t type? assert and support it if found.
                    static_assert(sizeof(size_t) == sizeof(uint64_t) || sizeof(size_t) == sizeof(uint32_t));
                }
            }
            // Or size_t is same as uint32_t or uint64_t, skip.

            typing::register_script_parser<float>(
                guard,
                [](const float* v, wo_vm, wo_value value)
                {
                    wo_set_float(value, *v);
                },
                [](float* v, wo_vm, wo_value value)
                {
                    *v = wo_float(value);
                },
                "float", "public alias float = real;");
            typing::register_script_parser<double>(
                guard,
                [](const double* v, wo_vm, wo_value value)
                {
                    wo_set_real(value, *v);
                },
                [](double* v, wo_vm, wo_value value)
                {
                    *v = wo_real(value);
                },
                "real", "");

            typing::register_script_parser<jeecs::basic::string>(
                guard,
                [](const jeecs::basic::string* v, wo_vm vm, wo_value value)
                {
                    wo_set_string(value, vm, v->c_str());
                },
                [](jeecs::basic::string* v, wo_vm, wo_value value)
                {
                    *v = wo_string(value);
                },
                "string", "");

            typing::register_script_parser<std::string>(
                guard,
                [](const std::string* v, wo_vm vm, wo_value value)
                {
                    wo_set_string(value, vm, v->c_str());
                },
                [](std::string* v, wo_vm, wo_value value)
                {
                    *v = wo_string(value);
                },
                "string", "");

            typing::register_script_parser<UserInterface::Origin::origin_center>(
                guard,
                [](const UserInterface::Origin::origin_center* v, wo_vm vm, wo_value value)
                {
                    wo_set_int(value, *v);
                },
                [](UserInterface::Origin::origin_center* v, wo_vm, wo_value value)
                {
                    *v = (UserInterface::Origin::origin_center)wo_int(value);
                },
                "UserInterface::Origin::origin_center",
                "namespace UserInterface::Origin\n"
                "{\n"
                "    public enum origin_center\n"
                "    {\n"
                "        center  = 0,\n"
                "\n"
                "        left    = 1,\n"
                "        right   = 2,\n"
                "        top     = 4,\n"
                "        bottom  = 8,\n"
                "    }\n"
                "}\n");

            je_towoo_update_api();
        }

        inline void module_leave(jeecs::typing::type_unregister_guard* guard)
        {
            // 0. ungister this module components
            guard->unregister_all_types();
        }
    }
    namespace input
    {
        inline bool keydown(keycode key)
        {
            return je_io_get_key_down(key);
        }
        inline bool mousedown(size_t group, mousecode key)
        {
            return je_io_get_mouse_state(group, key);
        }
        inline math::vec2 wheel(size_t group)
        {
            float x, y;
            je_io_get_wheel(group, &x, &y);
            return { x, y };
        }
        inline math::ivec2 mousepos(size_t group)
        {
            int x, y;
            je_io_get_mouse_pos(group, &x, &y);
            return { x, y };
        }
        inline math::vec2 mouseviewpos(size_t group)
        {
            int x, y, w, h;
            je_io_get_mouse_pos(group, &x, &y);
            je_io_get_window_size(&w, &h);
            return { ((float)x / (float)w - 0.5f) * 2.0f, ((float)y / (float)h - 0.5f) * -2.0f };
        }
        inline math::ivec2 windowsize()
        {
            int x, y;
            je_io_get_window_size(&x, &y);
            return { x, y };
        }
        inline math::ivec2 windowpos()
        {
            int x, y;
            je_io_get_window_pos(&x, &y);
            return { x, y };
        }

        template <typing::typehash_t hash_v1, int v2>
        static bool _isUp(bool keystate)
        {
            static bool lastframekeydown;
            bool res = (!keystate) && lastframekeydown;
            lastframekeydown = keystate;
            return res;
        }
        template <typing::typehash_t hash_v1, int v2>
        static bool _firstDown(bool keystate)
        {
            static bool lastframekeydown;
            bool res = (keystate) && !lastframekeydown;
            lastframekeydown = keystate;
            return res;
        }
        template <typing::typehash_t hash_v1, int v2>
        static bool _doubleClick(bool keystate, float i = 0.1f)
        {
            static typing::ms_stamp_t lact_click_tm = je_clock_time_stamp();
            static bool release_for_next_click = false;

            auto cur_time = je_clock_time_stamp();

            // 1. first click.
            if (keystate)
            {
                if (release_for_next_click && cur_time - lact_click_tm < (typing::ms_stamp_t)(i * 1000.f))
                {
                    // Is Double click!
                    lact_click_tm = 0; // reset the time.
                    release_for_next_click = false;
                    return true;
                }
                // Release for a long time, re calc the time
                if (!release_for_next_click || cur_time - lact_click_tm > (typing::ms_stamp_t)(i * 1000.f))
                {
                    lact_click_tm = cur_time;
                    release_for_next_click = false;
                }
            }
            else
            {
                if (!release_for_next_click && cur_time - lact_click_tm < (typing::ms_stamp_t)(i * 1000.f))
                    release_for_next_click = true;
            }
            return false;
        }

        [[maybe_unused]] void is_up(...);
        [[maybe_unused]] void first_down(...);
        [[maybe_unused]] void double_click(...); // just for fool ide

        class gamepad
        {
            je_io_gamepad_handle_t m_gamepad_handle;

        public:
            gamepad(je_io_gamepad_handle_t gamepad_handle)
                : m_gamepad_handle(gamepad_handle)
            {
            }
            gamepad(const gamepad&) = default;
            gamepad(gamepad&&) = default;
            gamepad& operator=(const gamepad&) = default;
            gamepad& operator=(gamepad&&) = default;
            ~gamepad() = default;

            bool button(gamepadcode button) const
            {
                return je_io_gamepad_get_button_down(m_gamepad_handle, button);
            }
            math::vec2 stick(joystickcode stick) const
            {
                float x, y;
                je_io_gamepad_get_stick(m_gamepad_handle, stick, &x, &y);
                return { x, y };
            }
            bool actived(typing::ms_stamp_t* out_last_update_time_may_null) const
            {
                return je_io_gamepad_is_active(
                    m_gamepad_handle, out_last_update_time_may_null);
            }

        private:
            static std::vector<je_io_gamepad_handle_t> _get_all_handle()
            {
                size_t current_gamepad_count = je_io_gamepad_get(0, nullptr);
                std::vector<je_io_gamepad_handle_t> gamepad_handles(current_gamepad_count);

                je_io_gamepad_get(current_gamepad_count, gamepad_handles.data());
                return gamepad_handles;
            }

        public:
            static std::vector<gamepad> all()
            {
                auto gamepad_handles = _get_all_handle();

                std::vector<gamepad> gamepads;
                gamepads.reserve(gamepad_handles.size());

                for (auto& handle : gamepad_handles)
                {
                    gamepads.emplace_back(handle);
                }
                return gamepads;
            }
            static std::optional<gamepad> last()
            {
                auto gamepad_handles = _get_all_handle();

                typing::ms_stamp_t last_update_time = 0;
                size_t last_index = SIZE_MAX;

                const size_t size = gamepad_handles.size();
                const je_io_gamepad_handle_t* data = gamepad_handles.data();

                for (size_t i = 0; i < size; ++i)
                {
                    typing::ms_stamp_t cur_time;
                    if (je_io_gamepad_is_active(data[i], &cur_time))
                    {
                        if (cur_time > last_update_time)
                        {
                            last_update_time = cur_time;
                            last_index = i;
                        }
                    }
                }

                if (last_index != SIZE_MAX)
                    return gamepad(gamepad_handles[last_index]);

                return std::nullopt;
            }
        };

#define is_up _isUp<jeecs::basic::hash_compile_time(__FILE__), __LINE__>
#define first_down _firstDown<jeecs::basic::hash_compile_time(__FILE__), __LINE__>
#define double_click _doubleClick<jeecs::basic::hash_compile_time(__FILE__), __LINE__>
    }

    class game_engine_context
    {
        JECS_DISABLE_MOVE_AND_COPY(game_engine_context);

        typing::type_unregister_guard types;

        enum class graphic_state
        {
            GRAPHIC_CONTEXT_NOT_READY,
            GRAPHIC_CONTEXT_READY,
        };
        graphic_state graphic_context_state_for_update_manually;
        graphic::graphic_syncer_host* graphic_syncer;

    public:
        game_engine_context(int argc, char** argv)
            : graphic_context_state_for_update_manually(
                graphic_state::GRAPHIC_CONTEXT_NOT_READY)
        {
            je_init(argc, argv);
            entry::module_entry(&types);
        }
        ~game_engine_context()
        {
            if (graphic_syncer != nullptr)
                delete graphic_syncer;

            entry::module_leave(&types);
            je_finish();
        }
        void loop()
        {
            (void)prepare_graphic();
            graphic_syncer->loop();
        }
    protected:
        enum class frame_update_result
        {
            FRAME_UPDATE_NOT_READY,
            FRAME_UPDATE_READY,
            FRAME_UPDATE_CLOSE_REQUESTED,
        };
        graphic::graphic_syncer_host* prepare_graphic()
        {
            graphic_syncer = new graphic::graphic_syncer_host();
            return graphic_syncer;
        }
        frame_update_result frame()
        {
            assert(graphic_syncer != nullptr);

            switch (graphic_context_state_for_update_manually)
            {
            case graphic_state::GRAPHIC_CONTEXT_NOT_READY:
                if (!graphic_syncer->check_context_ready_noblock())
                    break;

                graphic_context_state_for_update_manually = graphic_state::GRAPHIC_CONTEXT_READY;
                /* FALL THROUGH */
                [[fallthrough]];
            case graphic_state::GRAPHIC_CONTEXT_READY:
                if (graphic_syncer->frame())
                    return frame_update_result::FRAME_UPDATE_READY;
                else
                    return frame_update_result::FRAME_UPDATE_CLOSE_REQUESTED;
            default:
                debug::logfatal(
                    "Unknown graphic context state: %d",
                    (int)graphic_context_state_for_update_manually);
            }
            return frame_update_result::FRAME_UPDATE_NOT_READY;
        }
    };
}
#endif
#endif
