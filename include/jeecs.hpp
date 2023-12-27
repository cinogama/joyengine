#pragma once

#define _CRT_SECURE_NO_WARNINGS

#ifndef __cplusplus
#error jeecs.h only support for c++
#else

#include "wo.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cfloat>

#include <typeinfo>

#include <atomic>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <cstddef>
#include <cmath>
#include <random>
#include <thread>
#include <type_traits>
#include <sstream>
#include <climits>
#include <initializer_list>
#ifdef __cpp_lib_execution
#include <execution>
#endif

#define WO_FORCE_CAPI extern "C"{
#define WO_FORCE_CAPI_END }

#ifdef _WIN32
#   define JE_IMPORT __declspec(dllimport)
#   define JE_EXPORT __declspec(dllexport)
#else
#   define JE_IMPORT extern
#   define JE_EXPORT extern
#endif

#ifdef JE_IMPL
#   define JE_IMPORT_OR_EXPORT JE_EXPORT
#else
#   define JE_IMPORT_OR_EXPORT JE_IMPORT
#endif

#define JE_API JE_IMPORT_OR_EXPORT

#ifdef _WIN32
#		define JE_OS_WINDOWS
#elif defined(__ANDROID__)
#		define JE_OS_ANDROID
#elif defined(__linux__)
#		define JE_OS_LINUX
#else
#		define JE_OS_UNKNOWN
#endif

#if defined(_X86_)||defined(__i386)||(defined(_WIN32)&&!defined(_WIN64))
#		define JE_PLATFORM_X86
#		define JE_PLATFORM_M32
#elif defined(__x86_64)||defined(_M_X64)
#		define JE_PLATFORM_X64
#		define JE_PLATFORM_M64
#elif defined(__arm)
#		define JE_PLATFORM_ARM
#		define JE_PLATFORM_M32
#elif defined(__aarch64__)
#		define JE_PLATFORM_ARM64
#		define JE_PLATFORM_M64
#else
#		define JE_PLATFORM_UNKNOWN
#endif

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

        using module_entry_t = void(*)(void);
        using module_leave_t = void(*)(void);

        using construct_func_t = void(*)(void*, void*, const jeecs::typing::type_info*);
        using destruct_func_t = void(*)(void*);
        using copy_func_t = void(*)(void*, const void*);
        using move_func_t = void(*)(void*, void*);
        using to_string_func_t = const char* (*)(const void*);
        using parse_func_t = void(*)(void*, const char*);

        using update_func_t = void(*)(void*);

        using parse_c2w_func_t = void(*)(wo_vm, wo_value, const void*);
        using parse_w2c_func_t = void(*)(wo_vm, wo_value, void*);

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

            inline bool operator == (const uuid& uid) const noexcept
            {
                return a == uid.a && b == uid.b;
            }

            inline bool operator != (const uuid& uid) const noexcept
            {
                return !(this->operator==(uid));
            }

            inline std::string to_string() const
            {
                char buf[sizeof(a) * 2 + sizeof(b) * 2 + 2];
                snprintf(buf, sizeof(buf), "%016llX-%016llX", (unsigned long long)a, (unsigned long long)b);
                return buf;
            }
            inline void parse(const std::string& buf)
            {
                unsigned long long aa, bb;
                sscanf(buf.c_str(), "%llX-%llX", &aa, &bb);
                a = (uint64_t)aa;
                b = (uint64_t)bb;
            }
        };

        /*
        jeecs::typing::euid_t [类型别名]
        引擎实体的跟踪ID，实体创建时自动分配，并不随组件变化而变化
        需要注意的是，此值仅适合用于调试或编辑器环境，因为引擎只简单地递增分配，不保证分配绝对唯一的ID
            * 无效值是 0，从失效实体中取出的ID即为0，引擎保证不分配0作为实体的ID
        请参见：
            je_ecs_entity_uid
            jeecs::game_entity::get_euid
        */
        using euid_t = size_t;

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

        template<typename T>
        struct _origin_type
        {
            template<typename U>
            using _origin_t =
                typename std::remove_cv<
                typename std::remove_reference<
                typename std::remove_pointer<U>::type
                >::type
                >::type;

            static auto _type_selector() // -> T*
            {
                if constexpr (
                    std::is_reference<T>::value
                    || std::is_pointer<T>::value
                    || std::is_const<T>::value
                    || std::is_volatile<T>::value)
                    return _origin_type<_origin_t<T>>::_type_selector();
                else
                    return (T*) nullptr;
            }

            using type = typename std::remove_pointer<decltype(_type_selector())>::type;
        };

        /*
        jeecs::typing::origin_t<T> [泛型类型别名]
        等效于指定类型T去除 const volatile reference 和 pointer之后的原始类型
        */
        template<typename T>
        using origin_t = typename _origin_type<T>::type;

        template<class F>
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

        template<class R, class... Args>
        struct function_traits<R(*)(Args...)> : public function_traits < R(Args...) >
        {
        };

        template<class R, class... Args>
        struct function_traits < R(Args...) >
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
        template<class C, class R, class... Args>
        struct function_traits<R(C::*)(Args...)> : public function_traits < R(Args...) >
        {
            using this_t = C;
        };

        // const member function pointer
        template<class C, class R, class... Args>
        struct function_traits<R(C::*)(Args...) const> : public function_traits < R(Args...) >
        {
            using this_t = C;
        };

        // member object pointer
        template<class C, class R>
        struct function_traits<R(C::*)> : public function_traits < R(void) >
        {
            using this_t = C;
        };

        template<class F>
        struct function_traits<F&> : public function_traits < F >
        {};

        template<class F>
        struct function_traits<F&&> : public function_traits < F >
        {};

        template<size_t n, typename T, typename ... Ts>
        struct _variadic_type_indexer
        {
            static auto _type_selector() // -> T*
            {
                if constexpr (n != 0)
                    return _variadic_type_indexer<n - 1, Ts...>::_type_selector();
                else
                    return (T*) nullptr;
            }

            using type = typename std::remove_pointer<decltype(_type_selector())>::type;
        };

        /*
        jeecs::typing::index_types_t<n, Ts...> [泛型类型别名]
        获取给定类型序列Ts，等效于其中第n个类型
        */
        template<size_t n, typename ... Ts>
        using index_types_t = typename _variadic_type_indexer<n, Ts...>::type;

        class type_unregister_guard;
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
            UNAVAILABLE = 0,// Entity is destroied or just not ready,
            READY,          // Entity is OK, and just work as normal.
            PREFAB,         // Current entity is prefab, cannot be selected from arch-system and cannot 
        };

        struct meta
        {
            size_t m_euid;
            jeecs::typing::version_t m_version;
            jeecs::game_entity::entity_stat m_stat;
        };

        void* _m_in_chunk;
        jeecs::typing::entity_id_in_chunk_t   _m_id;
        jeecs::typing::version_t              _m_version;

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
        template<typename T>
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
        template<typename T>
        inline T* add_component() const noexcept;

        /*
        jeecs::game_entity::remove_component [方法]
        向指定实体中移除一个指定类型的组件，若最终导致实体的组件发生变化，
        那么旧的实体索引在下一帧失效
            * `最后`执行对组件的操作会真正生效，如果生效的是移除操作，那么：
                1. 如果实体此前已经存在此组件，那么更新实体，旧的实体索引将失效
                2. 如果实体此前不存在此组件，那么无事发生
        */
        template<typename T>
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

        /*
        jeecs::game_entity::get_euid [方法]
        若索引未失效，则返回实体的跟踪ID，否则返回0
        请参见：
            jeecs::typing::euid_t
        */
        inline typing::euid_t get_euid() const noexcept;

        inline bool operator == (const game_entity& e) const noexcept
        {
            return _m_in_chunk == e._m_in_chunk && _m_id == e._m_id && _m_version == e._m_version;
        }
        inline bool operator != (const game_entity& e) const noexcept
        {
            return _m_in_chunk != e._m_in_chunk || _m_id != e._m_id || _m_version != e._m_version;
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
        enum class mousecode
        {
            LEFT, MID, RIGHT,

            CUSTOM_0 = 32,

            _COUNT = 64,
        };
        enum class keycode
        {
            UNKNOWN = 0,

            A = 'A', B, C, D, E, F, G, H, I, J, K, L,
            M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
            _1 = '1', _2, _3, _4, _5, _6, _7, _8, _9,
            _0, _ = ' ',

            L_SHIFT = 256, L_CTRL, L_ALT, TAB, ENTER,
            ESC, BACKSPACE,

            CUSTOM_0 = 512,

            _COUNT = 1024,
        };
    }

    /*
    jeecs::graphic [命名空间]
    此处定义引擎图形库相关资源的封装类型和工具
    */
    namespace graphic
    {
        struct character;
    }
}

namespace std
{
    template<>
    struct hash<jeecs::typing::uid_t>
    {
        inline constexpr size_t operator() (const jeecs::typing::uid_t& uid) const noexcept
        {
            if constexpr (sizeof(size_t) == 8)
                return uid.b ^ uid.a;
            else
                return (size_t)(uid.b >> 32) ^ (size_t)uid.a;
        }
    };

    template<>
    struct equal_to<jeecs::typing::uid_t>
    {
        inline constexpr size_t operator() (const jeecs::typing::uid_t& a, const jeecs::typing::uid_t& b) const noexcept
        {
            return a.a == b.a && a.b == b.b;
        }
    };
}

WO_FORCE_CAPI

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
JE_API size_t je_log_register_callback(void(*func)(int level, const char* msg, void* custom), void* custom);

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
    size_t                    _size,
    size_t                    _align,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t  _destructor,
    jeecs::typing::copy_func_t      _copier,
    jeecs::typing::move_func_t      _mover,
    jeecs::typing::to_string_func_t _to_string,
    jeecs::typing::parse_func_t     _parse,
    jeecs::typing::update_func_t    _state_update,
    jeecs::typing::update_func_t    _pre_update,
    jeecs::typing::update_func_t    _update,
    jeecs::typing::update_func_t    _script_update,
    jeecs::typing::update_func_t    _late_update,
    jeecs::typing::update_func_t    _apply_update,
    jeecs::typing::update_func_t    _commit_update,
    je_typing_class                 _typecls);

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
    ptrdiff_t                       _member_offset);

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
JE_API void je_ecs_universe_register_exit_callback(void* universe, void(*callback)(void*), void* arg);

typedef void(*je_job_for_worlds_t)(void* /*world*/, void* /*custom_data*/);
typedef void(*je_job_call_once_t)(void* /*custom_data*/);

/*
je_ecs_universe_register_pre_for_worlds_job [基本接口]
向指定宇宙中注册优先遍历世界任务（Pre job for worlds）
*/
JE_API void je_ecs_universe_register_pre_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void(*freefunc)(void*));

/*
je_ecs_universe_register_pre_for_worlds_job [基本接口]
向指定宇宙中注册优先单独任务（Pre job for once）
*/
JE_API void je_ecs_universe_register_pre_call_once_job(void* universe, je_job_call_once_t job, void* data, void(*freefunc)(void*));

/*
je_ecs_universe_register_for_worlds_job [基本接口]
向指定宇宙中注册普通遍历世界任务（Job for worlds）
*/
JE_API void je_ecs_universe_register_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void(*freefunc)(void*));

/*
je_ecs_universe_register_call_once_job [基本接口]
向指定宇宙中注册普通单独任务（Job for once）
*/
JE_API void je_ecs_universe_register_call_once_job(void* universe, je_job_call_once_t job, void* data, void(*freefunc)(void*));

/*
je_ecs_universe_register_after_for_worlds_job [基本接口]
向指定宇宙中注册延后遍历世界任务（Defer job for worlds）
*/
JE_API void je_ecs_universe_register_after_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void(*freefunc)(void*));

/*
je_ecs_universe_register_after_call_once_job [基本接口]
向指定宇宙中注册延后单独任务（Defer job for once）
*/
JE_API void je_ecs_universe_register_after_call_once_job(void* universe, je_job_call_once_t job, void* data, void(*freefunc)(void*));

/*
je_ecs_universe_unregister_pre_for_worlds_job [基本接口]
从指定宇宙中取消优先遍历世界任务（Pre job for worlds）
*/
JE_API void je_ecs_universe_unregister_pre_for_worlds_job(void* universe, je_job_for_worlds_t job);

/*
je_ecs_universe_unregister_pre_call_once_job [基本接口]
从指定宇宙中取消优先单独任务（Pre job for once）
*/
JE_API void je_ecs_universe_unregister_pre_call_once_job(void* universe, je_job_call_once_t job);

/*
je_ecs_universe_unregister_for_worlds_job [基本接口]
从指定宇宙中取消普通遍历世界任务（Job for worlds）
*/
JE_API void je_ecs_universe_unregister_for_worlds_job(void* universe, je_job_for_worlds_t job);

/*
je_ecs_universe_unregister_call_once_job [基本接口]
从指定宇宙中取消普通单独任务（Job for once）
*/
JE_API void je_ecs_universe_unregister_call_once_job(void* universe, je_job_call_once_t job);

/*
je_ecs_universe_unregister_after_for_worlds_job [基本接口]
从指定宇宙中取消延后遍历世界任务（After job for worlds）
*/
JE_API void je_ecs_universe_unregister_after_for_worlds_job(void* universe, je_job_for_worlds_t job);

/*
je_ecs_universe_unregister_after_call_once_job [基本接口]
从指定宇宙中取消延后单独任务（After job for once）
*/
JE_API void je_ecs_universe_unregister_after_call_once_job(void* universe, je_job_call_once_t job);

/*
je_ecs_universe_get_frame_deltatime [基本接口]
获取当前宇宙的期待的帧更新间隔时间，默认为 1.0/60.0
请参见：
    je_ecs_universe_set_deltatime
*/
JE_API double je_ecs_universe_get_frame_deltatime(void* universe);

/*
je_ecs_universe_set_frame_deltatime [基本接口]
设置当前宇宙的帧更新间隔时间
*/
JE_API void je_ecs_universe_set_frame_deltatime(void* universe, double delta);

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
JE_API void je_ecs_world_update_dependences_archinfo(void* world, jeecs::dependence* dependence);

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
JE_API jeecs::game_system* je_ecs_world_add_system_instance(void* world, const jeecs::typing::type_info* type);

/*
je_ecs_world_get_system_instance [基本接口]
从指定世界中获取一个指定类型的系统实例，返回此实例的指针
若世界中不存在此类型的系统，返回nullptr
*/
JE_API jeecs::game_system* je_ecs_world_get_system_instance(void* world, const jeecs::typing::type_info* type);

/*
je_ecs_world_remove_system_instance [基本接口]
从指定世界中移除一个指定类型的系统实例
每次更新时，一帧内`最后`执行的操作将会生效，如果生效的是移除系统操作，那么：
    1. 若此前世界中不存在同类型的系统，则移除
    2. 若此前世界中已经存在同类型系统，则无事发生
*/
JE_API void je_ecs_world_remove_system_instance(void* world, const jeecs::typing::type_info* type);

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
    jeecs::typing::typeid_t* component_ids);

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
    jeecs::typing::typeid_t* component_ids);

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
    const jeecs::typing::type_info* component_info);

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
    const jeecs::typing::type_info* component_info);

/*
je_ecs_world_entity_get_component [基本接口]
从实体中获取一个组件
若实体索引是`无效值`或已失效，或者实体不存在指定类型的组件，则返回nullptr
请参见：
    jeecs::game_entity::get_component
*/
JE_API void* je_ecs_world_entity_get_component(
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info);

/*
je_ecs_world_of_entity [基本接口]
获取实体所在的世界
若实体索引是`无效值`或已失效，则返回nullptr
请参见：
    jeecs::game_entity::game_world
*/
JE_API void* je_ecs_world_of_entity(const jeecs::game_entity* entity);

/*
je_ecs_entity_uid [基本接口]
获取实体的跟踪ID
若实体索引是`无效值`或已失效，则返回无效值0
请参见：
    jeecs::typing::euid_t
    jeecs::game_entity::get_euid
*/
JE_API jeecs::typing::euid_t je_ecs_entity_uid(const jeecs::game_entity* entity);

// ATTENTION: These 2 functions have no thread-safe-promise.
/*
je_ecs_get_name_of_entity [基本接口]
获取实体的名称，一般只用于调试使用，不建议使用在实际项目中
若实体不包含 Editor::Name 或实体索引是`无效值`或已失效，则返回空字符串
从实体中取出的名称必须立即复制到其他位置或使用完毕，此函数直接
返回 Editor::Name 组件中的字符串地址，这意味着如果实体发生更新、重命名
或者其他操作，取出的字符串可能失效
*/
JE_API const char* je_ecs_get_name_of_entity(const jeecs::game_entity* entity);

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
JE_API const char* je_ecs_set_name_of_entity(const jeecs::game_entity* entity, const char* name);
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

/////////////////////////// CORE /////////////////////////////////

/*
jeecs_entry_register_core_systems [基本接口]
注册引擎内置的组件和系统类型，在调用jeecs::entry::module_entry时会一并执行
请参见：
    jeecs::entry::module_entry
*/
JE_API void jeecs_entry_register_core_systems(jeecs::typing::type_unregister_guard* guard);

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
JE_API void        jeecs_file_set_host_path(const char* path);

/*
jeecs_file_set_runtime_path [基本接口]
设置当前引擎的运行时路径，不影响“工作路径”
    * 设置此路径将影响以 @ 开头的路径的实际位置
    * 设置路径时，引擎会尝试以相同参数调用jeecs_file_update_default_fimg
请参考：
    jeecs_file_update_default_fimg
*/
JE_API void        jeecs_file_set_runtime_path(const char* path);

/*
* jeecs_file_update_default_fimg [基本接口]
读取指定位置的镜像文件作为默认镜像
    * 以 @/ 开头的路径将优先从默认镜像中读取
    * 无论打开是否成功，之前打开的默认镜像都将被关闭
    * 若 path == nullptr，则仅关闭旧的镜像
*/
JE_API void         jeecs_file_update_default_fimg(const char* path);

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
    载镜像中的文件）
    * 若路径由 '!' 开头，则如同加载可执行文件所在目录下的文件
请参见：
    jeecs_file_close
*/
JE_API jeecs_file* jeecs_file_open(const char* path);

/*
jeecs_file_close [基本接口]
关闭一个文件
*/
JE_API void        jeecs_file_close(jeecs_file* file);

/*
jeecs_file_read [基本接口]
从文件中读取若干个指定大小的元素，返回成功读取的元素数量
*/
JE_API size_t      jeecs_file_read(
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
JE_API fimg_creating_context* jeecs_file_image_begin(const char* storing_path, size_t max_image_size);

/*
jeecs_file_image_pack_file [基本接口]
向指定镜像中写入由 filepath 指定的一个文件，此文件在镜像中的路径被
指定为packingpath
*/
JE_API bool jeecs_file_image_pack_file(fimg_creating_context* context, const char* filepath, const char* packingpath);

/*
jeecs_file_image_pack_file [基本接口]
向指定镜像中写入一个缓冲区指定的内容作为文件，此文件在镜像中的路径被
指定为packingpath
*/
JE_API bool jeecs_file_image_pack_buffer(fimg_creating_context* context, const void* buffer, size_t len, const char* packingpath);

/*
jeecs_file_image_finish [基本接口]
结束镜像创建，将最后剩余数据写入镜像并创建镜像索引文件
*/
JE_API void jeecs_file_image_finish(fimg_creating_context* context);

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
JE_API jeecs_file* jeecs_load_cache_file(const char* filepath, uint32_t format_version, wo_integer_t virtual_crc64);

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
JE_API void* jeecs_create_cache_file(const char* filepath, uint32_t format_version, wo_integer_t usecrc64);

/*
jeecs_write_cache_file [基本接口]
向已创建的缓存文件中写入若干个指定大小的元素，返回成功写入的元素数量
*/
JE_API size_t jeecs_write_cache_file(const void* write_buffer, size_t elem_size, size_t count, void* file);

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

    enum resolution_mode
    {
        SCALE,
        ABSOLUTE,
    };

    display_mode    m_display_mode;
    resolution_mode m_resolution_mode;

    bool            m_enable_resize;

    // 若MSAA值为0，则说明关闭超采样抗锯齿
    //  * MSAA配置应该是2的整数次幂
    //  * 最终能否使用取决于图形库
    size_t          m_msaa;

    // 启动时的窗口大小
    size_t          m_width;
    size_t          m_height;

    // 启动时的分辨率，若m_resolution_mode是ABSOLUTE
    // 则此处的值是绝对值，否则是实际窗口大小的缩放比例
    size_t          m_reso_x;
    size_t          m_reso_y;

    // 限制帧数，若指定为0，则启用垂直同步
    // 不限制帧率请设置为 SIZE_MAX
    size_t          m_fps;

    const char* m_title;
    void* m_userdata;
};

struct jegl_thread_notifier;
struct jegl_graphic_api;

/*
jegl_thread [类型]
图形上下文，储存有当前图形线程的各项信息
*/
struct jegl_thread
{
    using custom_thread_data_t = void*;
    using frame_rend_work_func_t = void(*)(jegl_thread*, void*);

    void* _m_promise;   // std::promise<void>
    frame_rend_work_func_t  _m_frame_rend_work;
    void* _m_frame_rend_work_arg;
    void* _m_sync_callback_arg;

    jegl_thread_notifier* _m_thread_notifier;
    void* _m_interface_handle;

    void* m_universe_instance;
    jeecs::typing::version_t    m_version;
    jegl_interface_config       m_config;
    jegl_graphic_api* m_apis;
    void* m_stop_update; // std::atomic_bool
    custom_thread_data_t        m_userdata;
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
    // * Pixel data is storage from LEFT/BUTTOM to RIGHT/TOP
    // * If texture's m_pixels is nullptr, only create a texture in pipeline.
    pixel_data_t* m_pixels;
    size_t          m_width;
    size_t          m_height;

    format  m_format;

    bool            m_modified;
};

/*
jegl_vertex [类型]
顶点原始数据，储存有顶点的绘制方式、格式、大小和顶点数据等信息
*/
struct jegl_vertex
{
    enum type
    {
        LINES = 0,
        LINESTRIP,
        TRIANGLES,
        TRIANGLESTRIP,
    };
    float m_size_x, m_size_y, m_size_z;
    float* m_vertex_datas;
    size_t* m_vertex_formats;
    size_t m_format_count;
    size_t m_point_count;
    size_t m_data_count_per_point;
    type m_type;
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
        wrap_mode   m_uwrap;
        wrap_mode   m_vwrap;

        uint32_t    m_sampler_id;   // Used for DX11 & HLSL generation

        size_t      m_pass_id_count;
        uint32_t* m_pass_ids;     // Used for GL3 & GLSL generation
    };
#ifdef JE_PLATFORM_M64
    static_assert(sizeof(sampler_method) == 24 + 16);
#else
    static_assert(sizeof(sampler_method) == 24 + 8);
#endif

    enum uniform_type
    {
        INT,
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        TEXTURE,
        FLOAT4X4,
    };
    struct builtin_uniform_location
    {
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
    struct vertex_in_variables
    {
        uniform_type m_type;
    };
    struct unifrom_variables
    {
        const char* m_name;
        uint32_t    m_index;
        uniform_type m_uniform_type;
        bool        m_updated;
        union
        {
            struct
            {
                float x, y, z, w;
            };
            int n;
            float mat4x4[4][4];
        };

        unifrom_variables* m_next;
    };
    struct uniform_blocks
    {
        const char* m_name;
        uint32_t    m_specify_binding_place;

        uniform_blocks* m_next;
    };

    enum class depth_test_method : int8_t
    {
        INVALID = -1,

        OFF,
        NEVER,
        LESS,       /* DEFAULT */
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
        ENABLE,     /* DEFAULT */
    };
    enum class blend_method : int8_t
    {
        INVALID = -1,

        ZERO,       /* DEFAULT SRC = ONE, DST = ZERO (DISABLE BLEND.) */
        ONE,

        SRC_COLOR,
        SRC_ALPHA,

        ONE_MINUS_SRC_ALPHA,
        ONE_MINUS_SRC_COLOR,

        DST_COLOR,
        DST_ALPHA,

        ONE_MINUS_DST_ALPHA,
        ONE_MINUS_DST_COLOR,

        //CONST_COLOR,
        //ONE_MINUS_CONST_COLOR,

        //CONST_ALPHA,
        //ONE_MINUS_CONST_ALPHA,
    };
    enum class cull_mode : int8_t
    {
        INVALID = -1,

        NONE,       /* DEFAULT */
        FRONT,
        BACK,
    };

    const char* m_vertex_glsl_src;
    const char* m_fragment_glsl_src;
    const char* m_vertex_hlsl_src;
    const char* m_fragment_hlsl_src;
    const char* m_vertex_spirv_bin;
    const char* m_fragment_spirv_bin;

    size_t m_vertex_in_count;
    vertex_in_variables* m_vertex_in;

    unifrom_variables* m_custom_uniforms;
    uniform_blocks* m_custom_uniform_blocks;
    builtin_uniform_location m_builtin_uniforms;

    bool                m_enable_to_shared;
    depth_test_method   m_depth_test;
    depth_mask_method   m_depth_mask;
    blend_method        m_blend_src_mode, m_blend_dst_mode;
    cull_mode           m_cull_mode;

    sampler_method* m_sampler_methods;
    size_t              m_sampler_count;
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
    size_t          m_attachment_count;
    size_t          m_width;
    size_t          m_height;
};

/*
jegl_uniform_buffer [类型]
一致变量缓冲区原始数据，储存有一致变量的数据和容量等信息
*/
struct jegl_uniform_buffer
{
    size_t      m_buffer_binding_place;
    size_t      m_buffer_size;
    uint8_t* m_buffer;

    // Used for marking update range;
    size_t      m_update_begin_offset;
    size_t      m_update_length;
};

using jegl_resource_blob = void*;
/*
jegl_resource [类型]
图形资源初级封装，纹理、着色器、帧缓冲区等均为图形资源
*/
struct jegl_resource
{
    using jegl_custom_resource_t = void*;
    enum type : uint8_t
    {
        VERTEX,         // Mesh
        TEXTURE,        // Texture
        SHADER,         // Shader
        FRAMEBUF,       // Framebuffer
        UNIFORMBUF,     // UniformBlock
    };

    type m_type;
    jegl_thread* m_graphic_thread;
    jeecs::typing::version_t m_graphic_thread_version;

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
    resource_handle m_handle;

    const char* m_path;
    uint32_t* m_raw_ref_count;
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
    using startup_interface_func_t = jegl_thread::custom_thread_data_t(*)(jegl_thread*, const jegl_interface_config*, bool);
    using shutdown_interface_func_t = void(*)(jegl_thread*, jegl_thread::custom_thread_data_t, bool);

    using update_interface_func_t = bool(*)(jegl_thread::custom_thread_data_t);

    using create_resource_blob_func_t = jegl_resource_blob(*)(jegl_thread::custom_thread_data_t, jegl_resource*);
    using close_resource_blob_func_t = void(*)(jegl_thread::custom_thread_data_t, jegl_resource_blob);

    using init_resource_func_t = void(*)(jegl_thread::custom_thread_data_t, jegl_resource_blob, jegl_resource*);
    using using_resource_func_t = void(*)(jegl_thread::custom_thread_data_t, jegl_resource*);
    using close_resource_func_t = void(*)(jegl_thread::custom_thread_data_t, jegl_resource*);

    using draw_vertex_func_t = void(*)(jegl_thread::custom_thread_data_t, jegl_resource*);
    using bind_texture_func_t = void(*)(jegl_thread::custom_thread_data_t, jegl_resource*, size_t);

    using set_rendbuf_func_t = void(*)(jegl_thread::custom_thread_data_t, jegl_resource*, size_t, size_t, size_t, size_t);
    using clear_framebuf_color_func_t = void(*)(jegl_thread::custom_thread_data_t, float color[4]);
    using clear_framebuf_depth_func_t = void(*)(jegl_thread::custom_thread_data_t);
    using get_uniform_location_func_t = uint32_t(*)(jegl_thread::custom_thread_data_t, jegl_resource*, const char*);
    using set_uniform_func_t = void(*)(jegl_thread::custom_thread_data_t, uint32_t, jegl_shader::uniform_type, const void*);

    startup_interface_func_t    init_interface;
    shutdown_interface_func_t   shutdown_interface;

    update_interface_func_t     pre_update_interface;
    update_interface_func_t     update_interface;
    update_interface_func_t     late_update_interface;

    create_resource_blob_func_t create_resource_blob;
    close_resource_blob_func_t  close_resource_blob;

    init_resource_func_t        init_resource;
    using_resource_func_t       using_resource;
    close_resource_func_t       close_resource;

    draw_vertex_func_t          draw_vertex;
    bind_texture_func_t         bind_texture;

    set_rendbuf_func_t          set_rend_buffer;

    clear_framebuf_color_func_t clear_rend_buffer_color;
    clear_framebuf_depth_func_t clear_rend_buffer_depth;

    set_uniform_func_t          set_uniform;
};
static_assert(sizeof(jegl_graphic_api) % sizeof(void*) == 0);

using jeecs_api_register_func_t = void(*)(jegl_graphic_api*);
using jeecs_sync_callback_func_t = void(*)(jegl_thread*, void*);

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
JE_API void jegl_register_sync_thread_callback(jeecs_sync_callback_func_t callback, void* arg);

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
JE_API void             jegl_sync_init(jegl_thread* thread, bool isreboot);

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
JE_API jegl_sync_state  jegl_sync_update(jegl_thread* thread);

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
JE_API bool             jegl_sync_shutdown(jegl_thread* thread, bool isreboot);

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
JE_API jegl_thread* jegl_start_graphic_thread(
    jegl_interface_config config,
    void* universe_instance,
    jeecs_api_register_func_t register_func,
    void(*frame_rend_work)(jegl_thread*, void*),
    void* arg);

/*
jegl_terminate_graphic_thread [基本接口]
终止图形线程，将会阻塞直到图形线程完全退出
创建图形线程之后，无论图形绘制工作是否终止，都需要使用此接口释放图形线程
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_terminate_graphic_thread(jegl_thread* thread_handle);

enum jegl_update_sync_mode
{
    JEGL_WAIT_THIS_FRAME_END,
    JEGL_WAIT_LAST_FRAME_END,
};

/*
jegl_update [基本接口]
调度图形线程更新一帧，此函数将阻塞直到一帧的绘制操作完成
    * mode 用于指示更新的同步模式，其中：
        WAIT_THIS_FRAME_END：阻塞直到当前帧渲染完毕
        * 同步更简单，但是绘制时间无法和其他逻辑同步执行
        WAIT_LAST_FRAME_END：阻塞直到上一帧渲染完毕（绘制信号发出后立即返回）
        * 更适合CPU密集操作，但需要更复杂的机制以保证数据完整性
*/
JE_API bool jegl_update(jegl_thread* thread_handle, jegl_update_sync_mode mode);

/*
jegl_reboot_graphic_thread [基本接口]
以指定的配置重启一个图形线程
    * 若不需要改变图形配置，请使用nullptr
*/
JE_API void jegl_reboot_graphic_thread(
    jegl_thread* thread_handle,
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
JE_API jegl_resource* jegl_load_texture(const char* path);

/*
jegl_create_texture [基本接口]
创建一个指定大小和格式的纹理资源
    * 若指定的格式包含 COLOR16、DEPTH、FRAMEBUF、CUBE 或有 MSAA 支持，则不创建像素缓冲，
        对应纹理原始数据的像素将被设置为 nullptr
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
请参见：
    jegl_close_resource
*/
JE_API jegl_resource* jegl_create_texture(size_t width, size_t height, jegl_texture::format format);

/*
jegl_load_vertex [基本接口]
从指定路径加载一个顶点（模型）资源，加载的路径规则与 jeecs_file_open 相同
    * 若指定的文件不存在或不是一个合法的模型，则返回nullptr
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
请参见：
    jeecs_file_open
    jegl_close_resource
*/
JE_API jegl_resource* jegl_load_vertex(const char* path);

/*
jegl_create_vertex [基本接口]
用指定的顶点数据创建一个顶点（模型）资源
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
请参见：
    jegl_close_resource
*/
JE_API jegl_resource* jegl_create_vertex(
    jegl_vertex::type    type,
    const float* datas,
    const size_t* format,
    size_t                      data_length,
    size_t                      format_length);

/*
jegl_create_framebuf [基本接口]
使用指定的附件配置创建一个纹理缓冲区资源
    * 所有的图形资源都通过 jegl_close_resource 关闭并等待图形线程释放
请参见：
    jegl_close_resource
*/
JE_API jegl_resource* jegl_create_framebuf(
    size_t                          width,
    size_t                          height,
    const jegl_texture::format* attachment_formats,
    size_t                          attachment_count);

typedef struct je_stb_font_data je_font;
typedef void (*je_font_char_updater_t)(jegl_texture::pixel_data_t*, size_t, size_t);

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
    float                   scalex,
    float                   scaley,
    size_t                  board_blank_size_x,
    size_t                  board_blank_size_y,
    je_font_char_updater_t  char_texture_updater);

/*
je_font_free [基本接口]
关闭一个字体
*/
JE_API void         je_font_free(je_font* font);

/*
je_font_get_char [基本接口]
从字体中加载指定的一个字符的纹理及其他信息
请参见：
    jeecs::graphic::character
*/
JE_API jeecs::graphic::character* je_font_get_char(je_font* font, unsigned long chcode);

/*
jegl_set_able_shared_resources [基本接口]
设置是否启用资源缓存机制
    * 若启用，则加载相同路径的纹理和共享的着色器时，将不会创建新的实例，而总是获取同一份
    * 若需要同时使用多个图形线程（上下文），请关闭共享以避免资源读取问题。
    * 引擎默认关闭此机制，用于保证行为的正确性，但编辑器会默认打开此机制以节省资源
    * 若在资源缓存启用时需要更新指定资源（即强制重新加载），请使用
        jegl_mark_shared_resources_outdated
请参见：
    jegl_mark_shared_resources_outdated
*/
JE_API void jegl_set_able_shared_resources(bool able);

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
jegl_load_shader_source [基本接口]
从源码加载一个着色器实例，可创建或使用缓存文件以加速着色器的加载
    * 实际上jegl_load_shader会读取文件内容之后，调用此函数进行实际上的着色器加载
若不需要创建缓存文件，请将 is_virtual_file 指定为 false
请参见：
    jegl_load_shader
*/
JE_API jegl_resource* jegl_load_shader_source(const char* path, const char* src, bool is_virtual_file);

/*
jegl_load_shader [基本接口]
从源码文件加载一个着色器实例，会创建或使用缓存文件以加速着色器的加载
*/
JE_API jegl_resource* jegl_load_shader(const char* path);

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
typedef void(*jegl_graphic_api_entry)(jegl_graphic_api*);

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
加载JoyEngine基础图形接口的空实现，通常与jegl_start_graphic_thread一起使用
用于指定图形线程使用的基本图形库
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_none_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_opengl3_apis [基本接口]
加载OpenGL 3.3 或 OpenGLES 3.0 API集合，通常与jegl_start_graphic_thread一起使用
用于指定图形线程使用的基本图形库
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_opengl3_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_vulkan110_apis [基本接口] (暂未实现)
加载vulkan API v1.1集合，通常与jegl_start_graphic_thread一起使用
用于指定图形线程使用的基本图形库
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_vulkan110_apis(jegl_graphic_api* write_to_apis);


/*
jegl_using_metal_apis [基本接口] (暂未实现)
加载Metal API集合，通常与jegl_start_graphic_thread一起使用
用于指定图形线程使用的基本图形库
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_metal_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_dx11_apis [基本接口]
加载directx 11 API集合，通常与jegl_start_graphic_thread一起使用
用于指定图形线程使用的基本图形库
请参见：
    jegl_start_graphic_thread
*/
JE_API void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis);

/*
jegl_using_resource [基本接口]
使用指定的资源，一般用于指定使用着色器或一致变量缓冲区，同时根据情况决定是否执行
资源的初始化操作
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
*/
JE_API void jegl_using_resource(jegl_resource* resource);

/*
jegl_close_resource [基本接口]
关闭指定的图形资源，图形资源的原始数据信息会被立即回收，对应图形库的实际资源会在
图形线程中延迟销毁
    * 若存在多个图形线程，因为相关实现的原因，可能出现部分图形资源无法正确释放而泄漏
*/
JE_API void jegl_close_resource(jegl_resource* resource);

/*
jegl_using_texture [基本接口]
将指定的纹理绑定到指定的纹理通道，与jegl_using_resource类似，会根据情况是否执行资源
的初始化操作
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
请参见：
    jegl_using_resource
*/
JE_API void jegl_using_texture(jegl_resource* texture, size_t pass);

/*
jegl_draw_vertex [基本接口]
使用当前着色器（通过jegl_using_resource绑定）和纹理（通过jegl_using_texture绑定）,
以指定方式绘制一个模型，与jegl_using_resource类似，会根据情况是否执行资源的初始化操作
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
请参见：
    jegl_using_resource
    jegl_using_texture
*/
JE_API void jegl_draw_vertex(jegl_resource* vert);

/*
jegl_clear_framebuffer_color [基本接口]
以color指定的颜色清除当前帧缓冲的颜色信息
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
请参见：
    jegl_using_resource
*/
JE_API void jegl_clear_framebuffer_color(float color[4]);

/*
jegl_clear_framebuffer [基本接口]
清除指定帧缓冲的深度信息
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
请参见：
    jegl_using_resource
*/
JE_API void jegl_clear_framebuffer_depth();

/*
jegl_rend_to_framebuffer [基本接口]
指定接下来的绘制操作作用于指定缓冲区，xywh用于指定绘制剪裁区域的左下角位置和区域大小
若 framebuffer == nullptr 则绘制目标缓冲区设置为屏幕缓冲区，与jegl_using_resource类
似，会根据情况是否执行资源的初始化操作
    * 此函数只允许在图形线程内调用
    * 任意图形资源只被设计运作于单个图形线程，不允许不同图形线程共享一个图形资源
请参见：
    jegl_using_resource
*/
JE_API void jegl_rend_to_framebuffer(jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h);

/*
jegl_uniform_int [基本接口]
向当前着色器指定位置的一致变量设置一个整型数值
jegl_uniform_int 不会初始化着色器，请在操作之前调用 jegl_using_resource
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_using_resource
*/
JE_API void jegl_uniform_int(uint32_t location, int value);

/*
jegl_uniform_float [基本接口]
向当前着色器指定位置的一致变量设置一个单精度浮点数值
jegl_uniform_float 不会初始化着色器，请在操作之前调用 jegl_using_resource
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_using_resource
*/
JE_API void jegl_uniform_float(uint32_t location, float value);

/*
jegl_uniform_float2 [基本接口]
向当前着色器指定位置的一致变量设置一个二维单精度浮点矢量数值
jegl_uniform_float2 不会初始化着色器，请在操作之前调用 jegl_using_resource
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_using_resource
*/
JE_API void jegl_uniform_float2(uint32_t location, float x, float y);

/*
jegl_uniform_float3 [基本接口]
向当前着色器指定位置的一致变量设置一个三维单精度浮点矢量数值
jegl_uniform_float3 不会初始化着色器，请在操作之前调用 jegl_using_resource
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_using_resource
*/
JE_API void jegl_uniform_float3(uint32_t location, float x, float y, float z);

/*
jegl_uniform_float4 [基本接口]
向当前着色器指定位置的一致变量设置一个四维单精度浮点矢量数值
jegl_uniform_float4 不会初始化着色器，请在操作之前调用 jegl_using_resource
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_using_resource
*/
JE_API void jegl_uniform_float4(uint32_t location, float x, float y, float z, float w);

/*
jegl_uniform_float4x4 [基本接口]
向当前着色器指定位置的一致变量设置一个4x4单精度浮点矩阵数值
jegl_uniform_float4x4 不会初始化着色器，请在操作之前调用 jegl_using_resource
以确保着色器完成初始化
    * 此函数只允许在图形线程内调用
请参见：
    jegl_using_resource
*/
JE_API void jegl_uniform_float4x4(uint32_t location, const float(*mat)[4]);

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
JE_API void jegl_rchain_clear_color_buffer(jegl_rendchain* chain, float* color);

/*
jegl_rchain_clear_depth_buffer [基本接口]
指示此链绘制开始时需要清除目标缓冲区的深度缓存
*/
JE_API void jegl_rchain_clear_depth_buffer(jegl_rendchain* chain);

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
JE_API size_t jegl_rchain_allocate_texture_group(jegl_rendchain* chain);

/*
jegl_rchain_draw [基本接口]
将指定的顶点，使用指定的着色器和纹理将绘制操作作用到绘制链上
    * 若绘制的物体不需要使用纹理，可以使用不绑定纹理的纹理组或传入 SIZE_MAX
*/
JE_API jegl_rendchain_rend_action* jegl_rchain_draw(jegl_rendchain* chain, jegl_resource* shader, jegl_resource* vertex, size_t texture_group);

/*
jegl_rchain_set_uniform_int [基本接口]
为 act 指定的绘制操作应用整型一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_int(jegl_rendchain_rend_action* act, uint32_t binding_place, int val);

/*
jegl_rchain_set_uniform_float [基本接口]
为 act 指定的绘制操作应用单精度浮点数一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float(jegl_rendchain_rend_action* act, uint32_t binding_place, float val);

/*
jegl_rchain_set_uniform_float2 [基本接口]
为 act 指定的绘制操作应用二维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float2(jegl_rendchain_rend_action* act, uint32_t binding_place, float x, float y);

/*
jegl_rchain_set_uniform_float3 [基本接口]
为 act 指定的绘制操作应用三维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float3(jegl_rendchain_rend_action* act, uint32_t binding_place, float x, float y, float z);

/*
jegl_rchain_set_uniform_float4 [基本接口]
为 act 指定的绘制操作应用四维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float4(jegl_rendchain_rend_action* act, uint32_t binding_place, float x, float y, float z, float w);

/*
jegl_rchain_set_uniform_float4x4 [基本接口]
为 act 指定的绘制操作应用4x4单精度浮点数矩阵一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_uniform_float4x4(jegl_rendchain_rend_action* act, uint32_t binding_place, const float(*mat)[4]);


/*
jegl_rchain_set_builtin_uniform_int [基本接口]
为 act 指定的绘制操作应用整型一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_int(jegl_rendchain_rend_action* act, uint32_t* binding_place, int val);

/*
jegl_rchain_set_builtin_uniform_float [基本接口]
为 act 指定的绘制操作应用单精度浮点数一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float(jegl_rendchain_rend_action* act, uint32_t* binding_place, float val);

/*
jegl_rchain_set_builtin_uniform_float2 [基本接口]
为 act 指定的绘制操作应用二维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float2(jegl_rendchain_rend_action* act, uint32_t* binding_place, float x, float y);

/*
jegl_rchain_set_builtin_uniform_float3 [基本接口]
为 act 指定的绘制操作应用三维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float3(jegl_rendchain_rend_action* act, uint32_t* binding_place, float x, float y, float z);

/*
jegl_rchain_set_builtin_uniform_float4 [基本接口]
为 act 指定的绘制操作应用四维单精度浮点数矢量一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float4(jegl_rendchain_rend_action* act, uint32_t* binding_place, float x, float y, float z, float w);

/*
jegl_rchain_set_builtin_uniform_float4x4 [基本接口]
为 act 指定的绘制操作应用4x4单精度浮点数矩阵一致变量
请参见：
    jegl_rendchain_rend_action
*/
JE_API void jegl_rchain_set_builtin_uniform_float4x4(jegl_rendchain_rend_action* act, uint32_t* binding_place, const float(*mat)[4]);

/*
jegl_rchain_bind_texture [基本接口]
为指定的纹理组，在指定的纹理通道绑定一个纹理
请参见：
    jegl_rchain_allocate_texture_group
*/
JE_API void jegl_rchain_bind_texture(jegl_rendchain* chain, size_t texture_group, size_t binding_pass, jegl_resource* texture);

/*
jegl_rchain_bind_pre_texture_group [基本接口]
将指定的纹理组在全部绘制操作开始前绑定
    * 预先绑定的纹理可能被覆盖，请保证与其他绘制操作占据的通道做出区分
请参见：
    jegl_rchain_allocate_texture_group
    jegl_rchain_bind_texture
*/
JE_API void jegl_rchain_bind_pre_texture_group(jegl_rendchain* chain, size_t texture_group);

/*
jegl_rchain_commit [基本接口]
将指定的绘制链在图形线程中进行提交
    * 此函数只允许在图形线程内调用
*/
JE_API void jegl_rchain_commit(jegl_rendchain* chain, jegl_thread* glthread);

/*
jegl_uhost_get_or_create_for_universe [基本接口]
获取或创建指定Universe的可编程图形上下文接口,
    * config 被用于指示图形配置，若首次创建图形接口则使用此设置，
    若图形接口已经创建，则调用jegl_reboot_graphic_thread以应用图形配置
    * 若需要使用默认配置或不改变图形设置，请传入 nullptr
请参见：
    jegl_reboot_graphic_thread
*/
JE_API jeecs::graphic_uhost* jegl_uhost_get_or_create_for_universe(
    void* universe, const jegl_interface_config* config);

/*
jegl_uhost_get_gl_thread [基本接口]
从指定的可编程图形上下文接口获取图形线程的正式描述符
*/
JE_API jegl_thread* jegl_uhost_get_gl_thread(jeecs::graphic_uhost* host);

/*
jegl_uhost_alloc_branch [基本接口]
从指定的可编程图形上下文接口申请一个绘制组
*/
JE_API jeecs::rendchain_branch* jegl_uhost_alloc_branch(jeecs::graphic_uhost* host);

/*
jegl_uhost_free_branch [基本接口]
从指定的可编程图形上下文接口释放一个绘制组
*/
JE_API void jegl_uhost_free_branch(jeecs::graphic_uhost* host, jeecs::rendchain_branch* free_branch);

/*
jegl_branch_new_frame [基本接口]
在绘制开始之前，指示绘制组开始新的一帧，并指定优先级
*/
JE_API void jegl_branch_new_frame(jeecs::rendchain_branch* branch, int priority);

/*
jegl_branch_new_chain [基本接口]
从绘制组中获取一个新的RendChain
请参见：
    jegl_rendchain
*/
JE_API jegl_rendchain* jegl_branch_new_chain(jeecs::rendchain_branch* branch, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h);

/*
jegui_set_font [基本接口]
让ImGUI使用指定的路径和字体大小设置
    * 若 path 是空指针，则使用默认字体
    * 仅在 jegui_init_basic 前调用生效
    * 此接口仅适合用于对接自定义渲染API时使用
请参见：
    jegui_init_basic
*/
JE_API void jegui_set_font(const char* path, size_t size);

/*
jegui_init_basic [基本接口]
初始化ImGUI
    * 参数1 用于指示是否需要反转帧纹理，部分图形库的内置字节序与预计的是相反的，这种情况
        需要手动反转
    * 参数2 是一个回调函数指针，用于获取一个纹理对应的底层图形库句柄以供ImGUI使用
    * 参数3 是一个回调函数指针，用于应用一个指定着色器指定的采样设置
    * 此接口仅适合用于对接自定义渲染API时使用
*/
JE_API void jegui_init_basic(
    bool need_flip_frame_buf,
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*));

/*
jegui_update_basic [基本接口]
一帧渲染开始之后，需要调用此接口以完成ImGUI的绘制和更新操作
    * 此接口仅适合用于对接自定义渲染API时使用
*/
JE_API void jegui_update_basic();

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
je_io_update_keystate [基本接口]
更新指定键的状态信息
*/
JE_API void je_io_update_keystate(jeecs::input::keycode keycode, bool keydown);

/*
je_io_update_mousepos [基本接口]
更新鼠标（或触摸位置点）的坐标
    * 此操作`不会`影响光标的实际位置
*/
JE_API void je_io_update_mousepos(size_t group, int x, int y);

/*
je_io_update_mouse_state [基本接口]
更新鼠标（或触摸点）的状态
*/
JE_API void je_io_update_mouse_state(size_t group, jeecs::input::mousecode key, bool keydown);

/*
je_io_update_windowsize [基本接口]
更新窗口大小
    * 此操作`不会`影响窗口的实际大小
*/
JE_API void je_io_update_windowsize(int x, int y);

/*
je_io_update_wheel [基本接口]
更新鼠标滚轮的计数
*/
JE_API void je_io_update_wheel(size_t group, float x, float y);

/*
je_io_get_keydown [基本接口]
获取指定的按键是否被按下
*/
JE_API bool je_io_get_keydown(jeecs::input::keycode keycode);

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
je_io_get_windowsize [基本接口]
获取窗口的大小
*/
JE_API void je_io_get_windowsize(int* out_x, int* out_y);

/*
je_io_get_wheel [基本接口]
获取鼠标滚轮的计数
*/
JE_API void je_io_get_wheel(size_t group, float* out_x, float* out_y);

/*
je_io_set_lock_mouse [基本接口]
设置是否需要将鼠标锁定在指定位置，x,y是窗口坐标
*/
JE_API void je_io_set_lock_mouse(bool lock, int x, int y);

/*
je_io_get_lock_mouse [基本接口]
获取当前是否应该锁定鼠标及锁定的位置
*/
JE_API bool je_io_get_lock_mouse(int* out_x, int* out_y);

/*
je_io_set_windowsize [基本接口]
请求对窗口大小进行调整
    * 此操作将在图形线程生效
*/
JE_API void je_io_set_windowsize(int x, int y);

/*
je_io_fetch_update_windowsize [基本接口]
获取当前窗口大小是否应该调整及调整的大小
    * 此操作会导致请求操作被拦截
*/
JE_API bool je_io_fetch_update_windowsize(int* out_x, int* out_y);

/*
je_io_set_windowtitle [基本接口]
请求对窗口标题进行调整
    * 此操作将在图形线程生效
*/
JE_API void je_io_set_windowtitle(const char* title);

/*
je_io_set_windowtitle [基本接口]
获取当前是否需要对窗口标题进行调整及调整之后的内容
    * 此操作会导致请求操作被拦截
    * TODO: Not thread safe.
*/
JE_API bool je_io_fetch_update_windowtitle(const char** out_title);

// Library / Module loader

/*
je_module_load [基本接口]
以name为名字，加载指定路径的动态库（遵循woolang规则）加载失败返回nullptr
*/
JE_API void* je_module_load(const char* name, const char* path);

/*
je_module_func [基本接口]
从动态库中加载某个函数，返回函数的地址
*/
JE_API void* je_module_func(void* lib, const char* funcname);

/*
je_module_unload [基本接口]
立即卸载某个动态库
*/
JE_API void je_module_unload(void* lib);

/*
je_module_delay_unload [基本接口]
延迟卸载某个动态库，会在执行je_finish时正式释放
*/
JE_API void je_module_delay_unload(void* lib);

// Audio
struct jeal_device;
struct jeal_source;
struct jeal_buffer;

/*
jeal_state [类型]
用于表示当前声源的播放状态，包括停止、播放中和暂停
*/
enum class jeal_state
{
    STOPPED,
    PLAYING,
    PAUSED,
};

/*
jeal_get_all_devices [基本接口]
获取所有可用设备
    * 若正在使用中的设备不复存在，那么会重新指定默认的设备
    * 枚举设备可能是一个耗时操作，因此请不要在性能敏感的地方频繁调用此函数
    * 这不是线程安全函数，任何设备实例都可能在此函数调用后因为移除而失效
*/
JE_API jeal_device** jeal_get_all_devices(size_t* out_len);

/*
jeal_device_name [基本接口]
获取某个设备的名称
    * 仅用于调试
*/
JE_API const char* jeal_device_name(jeal_device* device);

/*
jeal_using_device [基本接口]
指定声音库使用某个设备
    * 若指定的设备不在设备列表中，则返回false
*/
JE_API bool             jeal_using_device(jeal_device* device);

/*
jeal_load_buffer_from_wav [基本接口]
从wav文件加载一个波形
*/
JE_API jeal_buffer* jeal_load_buffer_from_wav(const char* filename);

enum jeal_format
{
    MONO8,
    MONO16,
    STEREO8,
    STEREO16,
};

/*
jeal_create_buffer [基本接口]
从指定的波形数据创建波形对象
*/
JE_API jeal_buffer* jeal_create_buffer(
    const void* buffer_data,
    size_t buffer_data_len,
    size_t frequency,
    size_t byterate,
    jeal_format format);

/*
jeal_close_buffer [基本接口]
关闭一个波形
*/
JE_API void             jeal_close_buffer(jeal_buffer* buffer);

/*
jeal_buffer_byte_size [基本接口]
获取一个波形的长度，单位是字节
*/
JE_API size_t           jeal_buffer_byte_size(jeal_buffer* buffer);

/*
jeal_buffer_byte_size [基本接口]
获取一个波形的速率，单位是比特率
    * 波形长度除以比特率即可得到波形的持续时间
*/
JE_API size_t           jeal_buffer_byte_rate(jeal_buffer* buffer);

/*
jeal_open_source [基本接口]
创建一个声源
*/
JE_API jeal_source* jeal_open_source();

/*
jeal_close_source [基本接口]
关闭一个声源
*/
JE_API void             jeal_close_source(jeal_source* source);

/*
jeal_source_set_buffer [基本接口]
向声源指定一个波形，注意：
    * 更换波形之前，需要调用jeal_source_stop终止当前声源的播放操作
请参见：
    jeal_source_stop
*/
JE_API void             jeal_source_set_buffer(jeal_source* source, jeal_buffer* buffer);

/*
jeal_source_loop [基本接口]
向声源指定是否需要循环播放
*/
JE_API void             jeal_source_loop(jeal_source* source, bool loop);

/*
jeal_source_play [基本接口]
让声源开始或继续播放
*/
JE_API void             jeal_source_play(jeal_source* source);

/*
jeal_source_pause [基本接口]
暂停当前声源的播放
*/
JE_API void             jeal_source_pause(jeal_source* source);

/*
jeal_source_stop [基本接口]
停止当前声源的播放
*/
JE_API void             jeal_source_stop(jeal_source* source);

/*
jeal_source_position [基本接口]
设置当前声源的位置
*/
JE_API void             jeal_source_position(jeal_source* source, float x, float y, float z);

/*
jeal_source_position [基本接口]
设置当前声源的速度
    * 此处的速度用于处理多普勒效应，并非是播放速率
*/
JE_API void             jeal_source_velocity(jeal_source* source, float x, float y, float z);

/*
jeal_source_get_byte_offset [基本接口]
获取当前声源播放到波形的偏移量
*/
JE_API size_t           jeal_source_get_byte_offset(jeal_source* source);

/*
jeal_source_get_byte_offset [基本接口]
设置当前声源播放到波形的偏移量
*/
JE_API void             jeal_source_set_byte_offset(jeal_source* source, size_t byteoffset);

/*
jeal_source_pitch [基本接口]
调整声源的播放速度，默认值是 1.0
*/
JE_API void             jeal_source_pitch(jeal_source* source, float playspeed);

/*
jeal_source_volume [基本接口]
调整声源的播放音量，默认值是 1.0
*/
JE_API void             jeal_source_volume(jeal_source* source, float volume);

/*
jeal_source_get_state [基本接口]
获取当前的声源处于的状态（已停止，播放中或暂停中？）
*/
JE_API jeal_state       jeal_source_get_state(jeal_source* source);

/*
jeal_listener_position [基本接口]
设置当前监听者的位置
*/
JE_API void             jeal_listener_position(float x, float y, float z);

/*
jeal_listener_position [基本接口]
设置当前监听者的速度
*/
JE_API void             jeal_listener_velocity(float x, float y, float z);

/*
jeal_listener_direction [基本接口]
设置当前监听者的旋转方向，接收欧拉角，默认面朝z轴正方向，头顶方向为y轴正方向
*/
JE_API void             jeal_listener_direction(float yaw, float pitch, float roll);

/*
jeal_listener_volume [基本接口]
设置当前全局声音的播放音量，默认是 1.0，与jeal_global_volume_scale设置的值相乘之后
得到最终的音量大小
请参考：
    jeal_global_volume_scale
*/
JE_API void             jeal_listener_volume(float volume);

/*
jeal_global_volume_scale [基本接口]
设置全局声音播放系数，默认是 1.0，与jeal_listener_volume设置的值相乘之后
得到最终的音量大小
    * 此接口仅应用于因平台本身原因（典型例子是安卓平台切换到后台）等情况
    时，实现暂停播放音乐的效果；
    * 仅限于平台兼容工作中使用，一般情况下请`不要`使用此接口调整全局音量，
    避免与平台实现冲突
请参考：
    jeal_listener_volume
*/
JE_API void             jeal_global_volume(float volume);

/*
je_main_script_entry [基本接口]
运行入口脚本
    * 阻塞直到入口脚本运行完毕
    * 若 include_editor_script = true，则尝试带缓存地加载 @/builtin/editor/main.wo
    * 如果未能加载 @/builtin/editor/main.wo，则尝试 @/builtin/main.wo
*/
JE_API bool             je_main_script_entry();

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

JE_API void jedbg_set_editing_entity_uid(const jeecs::typing::euid_t uid);

JE_API jeecs::typing::euid_t jedbg_get_editing_entity_uid();

JE_API void jedbg_get_entity_arch_information(
    const jeecs::game_entity* _entity,
    size_t* _out_chunk_size,
    size_t* _out_entity_size,
    size_t* _out_all_entity_count_in_chunk);

#endif

// Atomic operator API
#define JE_DECL_ATOMIC_OPERATOR_API(TYPE)\
    JE_API TYPE je_atomic_exchange_##TYPE(TYPE* aim, TYPE value);\
    JE_API bool je_atomic_cas_##TYPE(TYPE* aim, TYPE* comparer, TYPE value);\
    JE_API TYPE je_atomic_fetch_add_##TYPE(TYPE* aim, TYPE value);\
    JE_API TYPE je_atomic_fetch_sub_##TYPE(TYPE* aim, TYPE value);\
    JE_API TYPE je_atomic_fetch_##TYPE(TYPE* aim);\
    JE_API void je_atomic_store_##TYPE(TYPE* aim, TYPE value)

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

WO_FORCE_CAPI_END

namespace jeecs
{
#define JECS_DISABLE_MOVE_AND_COPY_OPERATOR(TYPE) \
    TYPE& operator = (const TYPE &) = delete;\
    TYPE& operator = (TYPE &&) = delete

#define JECS_DISABLE_MOVE_AND_COPY_CONSTRUCTOR(TYPE) \
    TYPE(const TYPE &)  = delete;\
    TYPE(TYPE &&)       = delete

#define JECS_DISABLE_MOVE_AND_COPY(TYPE) \
    JECS_DISABLE_MOVE_AND_COPY_CONSTRUCTOR(TYPE);\
    JECS_DISABLE_MOVE_AND_COPY_OPERATOR(TYPE)


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
        template<typename ... ArgTs>
        inline void log(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_NORMAL, format, args...);
        }

        /*
        jeecs::debug::loginfo [函数]
        用于产生信息日志
        */
        template<typename ... ArgTs>
        inline void loginfo(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_INFO, format, args...);
        }

        /*
        jeecs::debug::logwarn [函数]
        用于产生警告日志
        */
        template<typename ... ArgTs>
        inline void logwarn(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_WARNING, format, args...);
        }

        /*
        jeecs::debug::logerr [函数]
        用于产生错误日志
        */
        template<typename ... ArgTs>
        inline void logerr(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_ERROR, format, args...);
        }

        /*
        jeecs::debug::logfatal [函数]
        用于产生严重错误日志
        */
        template<typename ... ArgTs>
        inline void logfatal(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_FATAL, format, args...);
        }
    }

    namespace typing
    {
        template<typename T, typename VoidT = void>
        struct sfinae_has_to_string : std::false_type
        {
            static_assert(std::is_void<VoidT>::value);
        };
        template<typename T>
        struct sfinae_has_to_string<T, std::void_t<decltype(&T::to_string)>> : std::true_type
        {
        };

        template<typename T, typename VoidT = void>
        struct sfinae_has_parse : std::false_type
        {
            static_assert(std::is_void<VoidT>::value);
        };
        template<typename T>
        struct sfinae_has_parse<T, std::void_t<decltype(&T::parse)>> : std::true_type
        {
        };
    }

    /*
    jeecs::basic [命名空间]
    用于存放引擎的基本工具
    */
    namespace basic
    {
        /*
        jeecs::basic::vector [类型]
        用于存放大小可变的连续存储容器
            * 为了保证模块之间的二进制一致性，公共组件中请不要使用std::vector
        */
        template<typename ElemT>
        class vector
        {
            ElemT* _elems_ptr_begin = nullptr;
            ElemT* _elems_ptr_end = nullptr;
            ElemT* _elems_buffer_end = nullptr;

            static constexpr size_t _single_elem_size = sizeof(ElemT);

            inline static size_t _move(ElemT* to_begin, ElemT* from_begin, ElemT* from_end)noexcept
            {
                for (ElemT* origin_elem = from_begin; origin_elem < from_end; ++origin_elem)
                {
                    new(to_begin++)ElemT(std::move(*origin_elem));
                    origin_elem->~ElemT();
                }
                return (size_t)(from_end - from_begin);
            }
            inline static size_t _r_move(ElemT* to_begin, ElemT* from_begin, ElemT* from_end)noexcept
            {
                for (ElemT* origin_elem = from_end; origin_elem > from_begin;)
                {
                    size_t offset = (size_t)((--origin_elem) - from_begin);
                    new(to_begin + offset)ElemT(std::move(*origin_elem));
                    origin_elem->~ElemT();
                }
                return (size_t)(from_end - from_begin);
            }
            inline static size_t _copy(ElemT* to_begin, ElemT* from_begin, ElemT* from_end)noexcept
            {
                for (ElemT* origin_elem = from_begin; origin_elem < from_end;)
                {
                    new(to_begin++)ElemT(*(origin_elem++));
                }

                return (size_t)(from_end - from_begin);
            }
            inline static size_t _erase(ElemT* from_begin, ElemT* from_end)noexcept
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
            vector()noexcept
            {
            }
            ~vector()
            {
                clear();
                je_mem_free(_elems_ptr_begin);
            }

            vector(const vector& another_list)
            {
                _reserve(another_list.size());
                _elems_ptr_end += _copy(_elems_ptr_begin, another_list.begin(), another_list.end());
            }
            vector(const std::initializer_list<ElemT>& another_list)
            {
                for (auto& elem : another_list)
                    push_back(elem);
            }
            vector(ElemT* ptr, size_t length)
            {
                _elems_ptr_begin = ptr;
                _elems_ptr_end = _elems_buffer_end = _elems_ptr_begin + length;
            }
            vector(vector&& another_list)
            {
                _elems_ptr_begin = another_list._elems_ptr_begin;
                _elems_ptr_end = another_list._elems_ptr_end;
                _elems_buffer_end = another_list._elems_buffer_end;

                another_list._elems_ptr_begin =
                    another_list._elems_ptr_end =
                    another_list._elems_buffer_end = nullptr;
            }

            inline vector& operator = (const vector& another_list)
            {
                _reserve(another_list.size());
                _elems_ptr_end += _copy(_elems_ptr_begin, another_list.begin(), another_list.end());

                return *this;
            }
            inline vector& operator = (vector&& another_list)
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
            inline void clear()noexcept
            {
                _erase(_elems_ptr_begin, _elems_ptr_end);
                _elems_ptr_end = _elems_ptr_begin;
            }
            inline void push_front(const ElemT& _e)
            {
                _assure(size() + 1);
                _r_move(_elems_ptr_begin + 1, _elems_ptr_begin, _elems_ptr_end++);
                new (_elems_ptr_begin) ElemT(_e);
            }
            inline void push_back(const ElemT& _e)
            {
                _assure(size() + 1);
                new (_elems_ptr_end++) ElemT(_e);
            }
            inline void pop_back()noexcept
            {
                if constexpr (!std::is_trivial<ElemT>::value)
                    (_elems_ptr_end--)->~ElemT();
                else
                    _elems_ptr_end--;
            }
            inline auto begin() const noexcept->ElemT*
            {
                return _elems_ptr_begin;
            }
            inline auto end() const noexcept->ElemT*
            {
                return _elems_ptr_end;
            }
            inline auto front() const noexcept->ElemT&
            {
                return *_elems_ptr_begin;
            }
            inline auto back() const noexcept->ElemT&
            {
                return *(_elems_ptr_end - 1);
            }
            inline void erase(size_t index)
            {
                _elems_ptr_begin[index].~ElemT();
                _move(_elems_ptr_begin + index, _elems_ptr_begin + index + 1, _elems_ptr_end--);
            }
            inline void erase(ElemT* index)
            {
                index->~ElemT();
                _move(index, index + 1, _elems_ptr_end--);
            }
            inline void erase_data(const ElemT& data)
            {
                auto fnd_place = std::find(begin(), end(), data);
                if (fnd_place != end())
                    erase(fnd_place - begin());
            }
            ElemT* data()const noexcept
            {
                return _elems_ptr_begin;
            }
            ElemT& operator[](size_t index)const noexcept
            {
                return _elems_ptr_begin[index];
            }
        };

        /*
        jeecs::basic::map [类型]
        用于存放大小可变的唯一键值对
            * 为了保证模块之间的二进制一致性，公共组件中请不要使用std::map
        */
        template<typename KeyT, typename ValT>
        class map
        {
            struct pair {
                KeyT k;
                ValT v;
            };
            basic::vector<pair> dats;

        public:
            ValT& operator[](const KeyT& k)noexcept
            {
                auto* fnd = find(k);
                if (fnd == dats.end())
                {
                    dats.push_back({ k, {} });
                    return dats.back().v;
                }
                return fnd->v;
            }

            void clear()noexcept
            {
                dats.clear();
            }

            pair* find(const KeyT& k) const noexcept
            {
                return std::find_if(dats.begin(), dats.end(), [&k](pair& p) {return p.k == k; });
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

            inline auto begin() const noexcept->pair*
            {
                return dats.begin();
            }

            inline auto end() const noexcept->pair*
            {
                return dats.end();
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

            string()noexcept
            {
                _reserve(1);
            }

            string(const string& str)noexcept
                :string(str.c_str())
            {
            }

            string(const std::string& str)noexcept
                :string(str.c_str())
            {
            }

            string(string&& str)noexcept
                :_c_str(str._c_str)
                , _buf_len(str._buf_len)
                , _str_len(str._str_len)
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
            operator std::string()const
            {
                return c_str();
            }
            /*string substr(size_t from, size_t count = (size_t)(-1))const
            {
                return  std::string(c_str()).substr(from, count);
            }*/
            size_t size()const
            {
                return _str_len;
            }
            const char* c_str()const
            {
                _c_str[_str_len] = 0;
                return _c_str;
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

        template<typename T, typename  ... ArgTs>
        T* create_new(ArgTs&& ... args)
        {
            static_assert(!std::is_void<T>::value);

            return new(je_mem_alloc(sizeof(T)))T(args...);
        }
        template<typename T, typename  ... ArgTs>
        T* create_new_n(size_t n)
        {
            static_assert(!std::is_void<T>::value);

            return new(je_mem_alloc(sizeof(T) * n))T[n];
        }

        template<typename T>
        void destroy_free(T* address)
        {
            static_assert(!std::is_void<T>::value);

            address->~T();
            je_mem_free(address);
        }
        template<typename T>
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

        template<typename NodeT>
        struct atomic_list
        {
            std::atomic<NodeT*> last_node = nullptr;

            void add_one(NodeT* node) noexcept
            {
                NodeT* last_last_node = last_node;// .exchange(node);
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

        template<typename T>
        struct default_functions
        {
            template<typename W>
            struct _true_type : public std::true_type {};

            template<typename U>
            struct has_pointer_typeinfo_constructor_function
            {
                template<typename V>
                static auto _tester(int) ->
                    _true_type<decltype(new V(std::declval<void*>(), std::declval<const jeecs::typing::type_info*>()))>;

                template<typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };
            template<typename U>
            struct has_pointer_constructor_function
            {
                template<typename V>
                static auto _tester(int) ->
                    _true_type<decltype(new V(std::declval<void*>()))>;

                template<typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };
            template<typename U>
            struct has_typeinfo_constructor_function
            {
                template<typename V>
                static auto _tester(int) ->
                    _true_type<decltype(new V(std::declval<const jeecs::typing::type_info*>()))>;

                template<typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };
            template<typename U>
            struct has_default_constructor_function
            {
                template<typename V>
                static auto _tester(int) ->
                    _true_type<decltype(new V())>;

                template<typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };

            static void constructor(void* _ptr, void* arg_ptr, const jeecs::typing::type_info* tinfo)
            {
                if constexpr (has_pointer_typeinfo_constructor_function<T>::value)
                    new(_ptr)T(arg_ptr, tinfo);
                else  if constexpr (has_pointer_constructor_function<T>::value)
                    new(_ptr)T(arg_ptr);
                else  if constexpr (has_typeinfo_constructor_function<T>::value)
                    new(_ptr)T(tinfo);
                else
                {
                    static_assert(has_default_constructor_function<T>::value);
                    new(_ptr)T{};
                }
            }
            static void destructor(void* _ptr)
            {
                ((T*)_ptr)->~T();
            }
            static void copier(void* _ptr, const void* _be_copy_ptr)
            {
                if constexpr (std::is_copy_constructible<T>::value)
                    new(_ptr)T(*(const T*)_be_copy_ptr);
                else
                    debug::logfatal("This type: '%s' is not copy-constructible but you try to do it."
                        , typeid(T).name());
            }
            static void mover(void* _ptr, void* _be_moved_ptr)
            {
                if constexpr (std::is_move_constructible<T>::value)
                    new(_ptr)T(std::move(*(T*)_be_moved_ptr));
                else
                    debug::logfatal("This type: '%s' is not move-constructible but you try to do it."
                        , typeid(T).name());
            }
            static const char* to_string(const void* _ptr)
            {
                if constexpr (typing::sfinae_has_to_string<T>::value)
                    return basic::make_new_string(((const T*)_ptr)->to_string());
                else if constexpr (std::is_fundamental<T>::value)
                {
                    std::stringstream b;
                    std::string str;
                    b << *(const T*)_ptr;
                    b >> str;
                    return basic::make_new_string(str.c_str());
                }
                else if constexpr (std::is_convertible<T, std::string>::value)
                    return basic::make_new_string(*(const T*)_ptr);

                static auto call_once = []() {
                    debug::logfatal("This type: '%s' have no function named 'to_string'."
                        , typeid(T).name());
                    return 0;
                    }();
                    return basic::make_new_string("");
            }
            static void parse(void* _ptr, const char* _memb)
            {
                if constexpr (typing::sfinae_has_parse<T>::value)
                    ((T*)_ptr)->parse(_memb);
                else if constexpr (std::is_fundamental<T>::value)
                {
                    std::stringstream b;
                    b << _memb;
                    b >> *(T*)_ptr;
                }
                else if constexpr (std::is_convertible<const char*, T>::value)
                    *(T*)_ptr = _memb;
                else
                {
                    static auto call_once = []() {
                        debug::logfatal("This type: '%s' have no function named 'parse'."
                            , typeid(T).name());
                        return 0;
                        }();
                }
            }

#define has_specify_function(SpecifyT) \
    template<typename U, typename VoidT = void>\
    struct has_##SpecifyT##_function : std::false_type\
    {\
        static_assert(std::is_void<VoidT>::value);\
    };\
    template<typename U>\
    struct has_##SpecifyT##_function<U, std::void_t<decltype(&U::SpecifyT)>> : std::true_type\
    {}

            has_specify_function(StateUpdate);
            has_specify_function(PreUpdate);
            has_specify_function(Update);
            has_specify_function(ScriptUpdate);
            has_specify_function(LateUpdate);
            has_specify_function(ApplyUpdate);
            has_specify_function(CommitUpdate);

#undef has_specify_function

            static void state_update(void* _ptr)
            {
                if constexpr (has_StateUpdate_function<T>::value)
                    std::launder(reinterpret_cast<T*>(_ptr))->StateUpdate();
            }
            static void pre_update(void* _ptr)
            {
                if constexpr (has_PreUpdate_function<T>::value)
                    std::launder(reinterpret_cast<T*>(_ptr))->PreUpdate();
            }
            static void update(void* _ptr)
            {
                if constexpr (has_Update_function<T>::value)
                    std::launder(reinterpret_cast<T*>(_ptr))->Update();
            }
            static void script_update(void* _ptr)
            {
                if constexpr (has_ScriptUpdate_function<T>::value)
                    std::launder(reinterpret_cast<T*>(_ptr))->ScriptUpdate();
            }
            static void late_update(void* _ptr)
            {
                if constexpr (has_LateUpdate_function<T>::value)
                    std::launder(reinterpret_cast<T*>(_ptr))->LateUpdate();
            }
            static void apply_update(void* _ptr)
            {
                if constexpr (has_ApplyUpdate_function<T>::value)
                    std::launder(reinterpret_cast<T*>(_ptr))->ApplyUpdate();
            }
            static void commit_update(void* _ptr)
            {
                if constexpr (has_CommitUpdate_function<T>::value)
                    std::launder(reinterpret_cast<T*>(_ptr))->CommitUpdate();
            }
        };

        template<typename T>
        constexpr auto type_hash()
        {
            return typeid(T).hash_code();
        }

        template<typename ... ArgTs>
        struct type_index_in_varargs
        {
            template<size_t Index, typename AimT, typename CurrentT, typename ... Ts>
            constexpr static size_t _index()
            {
                if constexpr (std::is_same<AimT, CurrentT>::value)
                    return Index;
                else
                    return _index<Index + 1, AimT, Ts...>();
            }

            template<typename AimArgT>
            constexpr size_t index_of() const noexcept
            {
                return _index<0, AimArgT, ArgTs...>();
            }
        };

        /*
        jeecs::basic::shared_pointer [类型]
        线程安全的共享智能指针类型
        */
        template<typename T>
        class shared_pointer
        {
            using count_t = size_t;
            using free_func_t = void(*)(T*);

            static void _default_free_func(T* ptr)
            {
                delete ptr;
            }

            T* m_resource = nullptr;
            mutable count_t* m_count = nullptr;
            free_func_t         m_freer = nullptr;

            inline const static
                count_t* _COUNT_USING_SPIN_LOCK_MARK = (count_t*)SIZE_MAX;

            static count_t* _alloc_counter()
            {
                return create_new<count_t>(1);
            }
            static void _free_counter(count_t* p)
            {
                destroy_free(p);
            }

            count_t* _spin_lock()const
            {
                count_t* result;
                do
                {
                    result = (count_t*)je_atomic_exchange_intptr_t(
                        (intptr_t*)&m_count, (intptr_t)_COUNT_USING_SPIN_LOCK_MARK);
                } while (result == _COUNT_USING_SPIN_LOCK_MARK);

                return result;
            }
            void _spin_unlock(count_t* p)const
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

            shared_pointer() noexcept = default;
            shared_pointer(T* v, void(*f)(T*) = &_default_free_func)
                : m_resource(v)
                , m_freer(f)
                , m_count(nullptr)
            {
                if (m_resource != nullptr)
                    m_count = _alloc_counter();
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
            shared_pointer& operator =(const shared_pointer& v) noexcept
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
            shared_pointer& operator =(shared_pointer&& v)noexcept
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

            T* get()const
            {
                return m_resource;
            }
            T* operator -> ()const noexcept
            {
                return m_resource;
            }
            T& operator * ()const noexcept
            {
                return *m_resource;
            }
            operator bool()const noexcept
            {
                return m_resource != nullptr;
            }
            bool operator !()const noexcept
            {
                return m_resource == nullptr;
            }
            bool operator == (const T* ptr)const
            {
                return m_resource == ptr;
            }
            bool operator != (const T* ptr)const
            {
                return m_resource != ptr;
            }
        };

        /*
        jeecs::basic::resource [类型别名]
        智能指针的类型别名，一般用于保管需要共享和自动管理的资源类型
        */
        template<typename T>
        using resource = shared_pointer<T>;

        /*
        jeecs::basic::fileresource [类型]
        文件资源包装类型，用于组件内的成员变量
            * 类型T应该有 load 方法以创建和返回自身
            * 类型T如果是void，那么相当于只读取文件名
        */
        template<typename T>
        class fileresource
        {
            basic::resource<T> _m_resource = nullptr;
            basic::string _m_path = "";
        public:
            bool load(const std::string& path)
            {
                _m_path = path;
                _m_resource = nullptr;
                if (path != "")
                {
                    _m_resource = T::load(path);
                    return _m_resource != nullptr;
                }
                return true;
            }
            void set_resource(const basic::resource<T>& res)
            {
                _m_resource = res;
                _m_path = "<builtin>";
            }
            bool has_resource() const
            {
                return _m_resource != nullptr;
            }
            const basic::resource<T>& get_resource() const
            {
                return _m_resource;
            }
            std::string get_path() const
            {
                return _m_path;
            }
            std::string to_string()const
            {
                return "#je_file#" + get_path();
            }
            void parse(const char* databuf)
            {
                _m_resource = nullptr;
                const size_t head_length = strlen("#je_file#");
                if (strncmp(databuf, "#je_file#", head_length) == 0)
                {
                    databuf += head_length;
                    load(databuf);
                }
            }

        };

        template<>
        class fileresource<void>
        {
            bool _m_has_resource = false;
            basic::string _m_path = "";
        public:
            bool load(const std::string& path)
            {
                _m_path = path;
                return _m_has_resource = (path != "");
            }
            std::string get_path() const
            {
                return _m_path;
            }
            bool has_resource() const
            {
                return _m_has_resource;
            }
            std::string to_string()const
            {
                return "#je_file#" + get_path();
            }
            void parse(const char* databuf)
            {
                const size_t head_length = strlen("#je_file#");
                if (strncmp(databuf, "#je_file#", head_length) == 0)
                {
                    databuf += head_length;
                    load(databuf);
                }
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

#define JERefRegsiter zzz_jeref_register 

        template<typename T, typename VoidT = void>
        struct sfinae_has_ref_register : std::false_type
        {
            static_assert(std::is_void<VoidT>::value);
        };
        template<typename T>
        struct sfinae_has_ref_register<T, std::void_t<decltype(&T::JERefRegsiter)>> : std::true_type
        {
        };

        template<typename T, typename VoidT = void>
        struct sfinae_is_static_ref_register_function : std::false_type
        {
            static_assert(std::is_void<VoidT>::value);
        };
        template<typename T>
        struct sfinae_is_static_ref_register_function<T, std::void_t<decltype(T::JERefRegsiter(nullptr))>> : std::true_type
        {
        };

        /*
        jeecs::typing::member_info [类型]
        用于储存组件的成员信息
        */
        struct member_info
        {
            const type_info* m_class_type;

            const char* m_member_name;
            const type_info* m_member_type;
            ptrdiff_t m_member_offset;

            member_info* m_next_member;
        };

        /*
        jeecs::typing::script_parser_info [类型]
        用于储存与woolang进行转换的方法和类型信息
        */
        struct script_parser_info
        {
            parse_c2w_func_t m_script_parse_c2w;
            parse_w2c_func_t m_script_parse_w2c;
            const char* m_woolang_typename;
            const char* m_woolang_typedecl;
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
            template<typename T>
            typeid_t _register_or_get_type_id(const char* _typename)
            {
                do
                {
                    std::lock_guard g1(_m_mx);
                    auto fnd = _m_self_registed_hash.find(typeid(T).hash_code());
                    if (fnd != _m_self_registed_hash.end())
                        return fnd->second;
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
                    basic::default_functions<T>::constructor,
                    basic::default_functions<T>::destructor,
                    basic::default_functions<T>::copier,
                    basic::default_functions<T>::mover,
                    basic::default_functions<T>::to_string,
                    basic::default_functions<T>::parse,
                    basic::default_functions<T>::state_update,
                    basic::default_functions<T>::pre_update,
                    basic::default_functions<T>::update,
                    basic::default_functions<T>::script_update,
                    basic::default_functions<T>::late_update,
                    basic::default_functions<T>::apply_update,
                    basic::default_functions<T>::commit_update,
                    current_type);
                do
                {
                    std::lock_guard g1(_m_mx);

                    assert(_m_self_registed_id_typeinfo.find(local_type_info->m_id) == _m_self_registed_id_typeinfo.end());
                    _m_self_registed_id_typeinfo[local_type_info->m_id] = local_type_info;
                    _m_self_registed_hash[typeid(T).hash_code()] = local_type_info->m_id;

                } while (0);
                return local_type_info->m_id;
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
            typeid_t    m_id;

            typehash_t  m_hash;
            const char* m_typename;   // will be free by je_typing_unregister
            size_t      m_size;
            size_t      m_align;
            size_t      m_chunk_size; // calc by je_typing_register

            construct_func_t    m_constructor;
            destruct_func_t     m_destructor;
            copy_func_t         m_copier;
            move_func_t         m_mover;
            to_string_func_t    m_to_string;
            parse_func_t        m_parse;

            update_func_t       m_state_update;
            update_func_t       m_pre_update;
            update_func_t       m_update;
            update_func_t       m_script_update;
            update_func_t       m_late_update;
            update_func_t       m_apply_update;
            update_func_t       m_commit_update;

            je_typing_class     m_type_class;

            volatile size_t             m_member_count;
            const member_info* volatile m_member_types;
            const script_parser_info* volatile m_script_parser_info;
        public:
            template<typename T>
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

            template<typename T>
            inline static typeid_t id()
            {
                return of<T>()->m_id;
            }

            template<typename T>
            inline static void register_type(
                jeecs::typing::type_unregister_guard* guard, const char* _typename)
            {
                guard->_register_or_get_type_id<T>(_typename);

                if constexpr (sfinae_has_ref_register<T>::value)
                {
                    if constexpr (sfinae_is_static_ref_register_function<T>::value)
                        T::JERefRegsiter(guard);
                    else
                        static_assert(sfinae_is_static_ref_register_function<T>::value,
                            "T::JERefRegsiter must be static & callable with no arguments.");
                }
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

            inline void state_update(void* addr) const noexcept
            {
                assert(is_system());
                m_state_update(addr);
            }
            inline void pre_update(void* addr) const noexcept
            {
                assert(is_system());
                m_pre_update(addr);
            }
            inline void update(void* addr) const noexcept
            {
                assert(is_system());
                m_update(addr);
            }
            inline void script_update(void* addr) const noexcept
            {
                assert(is_system());
                m_script_update(addr);
            }
            inline void late_update(void* addr) const noexcept
            {
                assert(is_system());
                m_late_update(addr);
            }
            inline void apply_update(void* addr) const noexcept
            {
                assert(is_system());
                m_apply_update(addr);
            }
            inline void commit_update(void* addr) const noexcept
            {
                assert(is_system());
                m_commit_update(addr);
            }

            inline const member_info* find_member_by_name(const char* name) const noexcept
            {
                auto* member_info_ptr = m_member_types;
                while (member_info_ptr != nullptr)
                {
                    if (strcmp(member_info_ptr->m_member_name, name) == 0)
                        return member_info_ptr;

                    member_info_ptr = member_info_ptr->m_next_member;
                }
                jeecs::debug::logerr("Failed to find member named: '%s' in '%s'.", name, this->m_typename);
                return nullptr;
            }
        };

        template<typename ClassT, typename MemberT>
        inline void register_member(
            jeecs::typing::type_unregister_guard* guard,
            ptrdiff_t member_offset,
            const char* membname)
        {
            const type_info* membt = jeecs::typing::type_info::of(
                guard->_register_or_get_type_id<MemberT>(nullptr));

            assert(membt->m_type_class == je_typing_class::JE_BASIC_TYPE);

            je_register_member(
                guard->get_local_type_info(
                    type_info::id<ClassT>()),
                membt,
                membname,
                member_offset);
        }

        template<typename ClassT, typename MemberT>
        inline void register_member(
            jeecs::typing::type_unregister_guard* guard,
            MemberT(ClassT::* _memboffset),
            const char* membname)
        {
            ptrdiff_t member_offset =
                reinterpret_cast<ptrdiff_t>(&(((ClassT*)nullptr)->*_memboffset));

            register_member<ClassT, MemberT>(guard, member_offset, membname);
        }
        template<typename T>
        inline void register_script_parser(
            jeecs::typing::type_unregister_guard* guard,
            void(*c2w)(wo_vm, wo_value, const T*),
            void(*w2c)(wo_vm, wo_value, T*),
            const std::string& woolang_typename,
            const std::string& woolang_typedecl)
        {
            je_register_script_parser(
                guard->get_local_type_info(
                    guard->_register_or_get_type_id<T>(nullptr)),
                reinterpret_cast<jeecs::typing::parse_c2w_func_t>(c2w),
                reinterpret_cast<jeecs::typing::parse_w2c_func_t>(w2c),
                woolang_typename.c_str(),
                woolang_typedecl.c_str());
        }
    }

    namespace basic
    {
        class type
        {
            const typing::type_info* m_type_info = nullptr;
        public:
            void set_type(const typing::type_info* _type)
            {
                m_type_info = _type;
            }
            const typing::type_info* get_type() const
            {
                return m_type_info;
            }
            std::string to_string()const
            {
                std::string result = "#je_type_info#";

                if (m_type_info != nullptr)
                    result += m_type_info->m_typename;

                return result;
            }
            void parse(const char* databuf)
            {
                size_t readed_length;

                m_type_info = nullptr;
                if (sscanf(databuf, "#je_type_info#%zn", &readed_length) == 0)
                {
                    databuf += readed_length;
                    m_type_info = je_typing_get_info_by_name(databuf);
                }
            }
        };
    }

    class game_universe;

    class game_world
    {
        void* _m_ecs_world_addr;

    public:
        game_world(void* ecs_world_addr)
            :_m_ecs_world_addr(ecs_world_addr)
        {

        }
    private:
        friend class game_system;
    public:
        inline void* handle()const noexcept
        {
            return _m_ecs_world_addr;
        }

        template<typename FirstCompT, typename ... CompTs>
        inline game_entity add_entity()
        {
            typing::typeid_t component_ids[] = {
                typing::type_info::id<FirstCompT>(),
                typing::type_info::id<CompTs>()...,
                typing::INVALID_TYPE_ID
            };
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
        template<typename FirstCompT, typename ... CompTs>
        inline game_entity add_prefab()
        {
            typing::typeid_t component_ids[] = {
                typing::type_info::id<FirstCompT>(typeid(FirstCompT).name()),
                typing::type_info::id<CompTs>(typeid(CompTs).name())...,
                typing::INVALID_TYPE_ID
            };
            game_entity gentity;
            je_ecs_world_create_prefab_with_components(
                handle(), &gentity, component_ids);

            return gentity;
        }


        inline jeecs::game_system* add_system(const jeecs::typing::type_info* type)
        {
            assert(type->m_type_class == je_typing_class::JE_SYSTEM);
            return je_ecs_world_add_system_instance(handle(), type);
        }

        template<typename SystemT>
        inline SystemT* add_system()
        {
            return (SystemT*)add_system(typing::type_info::of<SystemT>(typeid(SystemT).name()));
        }

        // ATTENTION: 
        // This function is not thread safe, only useable for in-engine-runtime, do not use it in other thread.
        inline jeecs::game_system* get_system(const jeecs::typing::type_info* type)
        {
            assert(type->m_type_class == je_typing_class::JE_SYSTEM);
            return je_ecs_world_get_system_instance(handle(), type);
        }

        // ATTENTION: 
        // This function is not thread safe, only useable for in-engine-runtime, do not use it in other thread.
        template<typename SystemT>
        inline SystemT* get_system()
        {
            return static_cast<SystemT*>(get_system(typing::type_info::of<SystemT>()));
        }

        inline void remove_system(const jeecs::typing::type_info* type)
        {
            assert(type->m_type_class == je_typing_class::JE_SYSTEM);
            je_ecs_world_remove_system_instance(handle(), type);
        }

        template<typename SystemT>
        inline void remove_system()
        {
            remove_system(typing::type_info::of<SystemT>(typeid(SystemT).name()));
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

        inline operator bool()const noexcept
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

        inline game_universe get_universe() const noexcept;
    };

    // Used for select the components of entities which match spcify requirements.
    struct requirement
    {
        enum type : uint8_t
        {
            CONTAIN,        // Must have spcify component
            MAYNOT,         // May have or not have
            ANYOF,          // Must have one of 'ANYOF' components
            EXCEPT,         // Must not contain spcify component
        };

        type m_require;
        size_t m_require_group_id;
        typing::typeid_t m_type;

        requirement(type _require, size_t group_id, typing::typeid_t _type)
            : m_require(_require)
            , m_require_group_id(group_id)
            , m_type(_type)
        { }
    };

    struct dependence
    {
        basic::vector<requirement> m_requirements;

        // Store archtypes here?
        game_world  m_world = nullptr;
        size_t      m_current_arch_version = 0;

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
        basic::vector<arch_chunks_info*>       m_archs;
        dependence() = default;
        dependence(const dependence& d)
            : m_requirements(d.m_requirements)
            , m_world(d.m_world)
            , m_current_arch_version(0)
            , m_archs({})
        {

        }
        dependence(dependence&& d)
            : m_requirements(std::move(d.m_requirements))
            , m_world(std::move(d.m_world))
            , m_current_arch_version(d.m_current_arch_version)
            , m_archs(std::move(d.m_archs))
        {
            d.m_current_arch_version = 0;
        }
        dependence& operator = (const dependence& d)
        {
            m_requirements = d.m_requirements;
            m_world = d.m_world;
            m_current_arch_version = 0;
            m_archs = {};

            return *this;
        }
        dependence& operator = (dependence&& d)
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

        void update(const game_world& aim_world)noexcept
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
        size_t                      m_curstep = 0;
        size_t                      m_any_id = 0;
        game_world                  m_current_world = nullptr;

        bool                        m_first_time_to_work = true;
        basic::vector<dependence>   m_dependence_caches;

        game_system* m_system_instance = nullptr;
    private:
        template<size_t ArgN, typename FT>
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
                    // Reference, means CONTAIN
                    dep.m_requirements.push_front(
                        requirement(requirement::type::CONTAIN, 0,
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

        template<typename CurRequireT, typename ... Ts>
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

        template<typename CurRequireT, typename ... Ts>
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
            dep.m_requirements.push_back(requirement(requirement::type::CONTAIN, 0, id));
            if constexpr (sizeof...(Ts) > 0)
                _apply_contain<Ts...>(dep);
        }

        template<typename CurRequireT, typename ... Ts>
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

        template<typename FT>
        struct _executor_extracting_agent : std::false_type
        { };

        template<typename ComponentT, typename ... ArgTs>
        struct _const_type_index
        {
            using f_t = typing::function_traits<void(ArgTs...)>;
            template<size_t id = 0>
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
        inline static bool get_entity_avaliable(const game_entity::meta* entity_meta, size_t eid)noexcept
        {
            return jeecs::game_entity::entity_stat::READY == entity_meta[eid].m_stat;
        }

        inline static bool get_entity_avaliable_version(const game_entity::meta* entity_meta, size_t eid, typing::version_t* out_version)noexcept
        {
            if (jeecs::game_entity::entity_stat::READY == entity_meta[eid].m_stat)
            {
                *out_version = entity_meta[eid].m_version;
                return true;
            }
            return false;
        }
        inline static void* get_component_from_archchunk_ptr(dependence::arch_chunks_info* archinfo, void* chunkbuf, size_t entity_id, size_t cid)
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
        template<typename ComponentT, typename ... ArgTs>
        inline static ComponentT get_component_from_archchunk(dependence::arch_chunks_info* archinfo, void* chunkbuf, size_t entity_id)
        {
            constexpr size_t cid = _const_type_index<ComponentT, ArgTs...>::index;
            auto* component_ptr = std::launder(reinterpret_cast<typename typing::origin_t<ComponentT>*>(
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
        template<typename RT, typename ... ArgTs>
        struct _executor_extracting_agent<RT(ArgTs...)> : std::true_type
        {
            template<typename FT>
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
                                    (static_cast<typename typing::function_traits<FT>::this_t*>(sys)->*f)
                                    (get_component_from_archchunk<ArgTs, ArgTs...>(archinfo, cur_chunk, eid)...);
                            }
                        }
                        cur_chunk = je_arch_next_chunk(cur_chunk);
                    }
                }
            }
        };

        template<typename RT, typename ... ArgTs>
        struct _executor_extracting_agent<RT(game_entity, ArgTs...)> : std::true_type
        {
            template<typename FT>
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
                                    f(game_entity{ cur_chunk, eid, version }, get_component_from_archchunk<ArgTs, ArgTs...>(archinfo, cur_chunk, eid)...);
                                else
                                    (static_cast<typename typing::function_traits<FT>::this_t*>(sys)->*f)(
                                        game_entity{ cur_chunk, eid, version }, get_component_from_archchunk<ArgTs, ArgTs...>(archinfo, cur_chunk, eid)...);
                            }
                        }

                        cur_chunk = je_arch_next_chunk(cur_chunk);
                    }
                }
            }
        };

        template<typename FT>
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
#ifndef NDEBUG
            jeecs::debug::loginfo("New selector(%p) created.", this);
#endif
        }

        ~selector()
        {
            m_dependence_caches.clear();
#ifndef NDEBUG
            jeecs::debug::loginfo("Selector(%p) closed.", this);
#endif
        }

        selector& at(game_world w)
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
            return *this;
        }

        template<typename FT>
        selector& exec(FT&& _exec)
        {
            _update(_exec);
            return *this;
        }

        template<typename ... Ts>
        inline selector& except() noexcept
        {
            if (m_first_time_to_work)
            {
                auto& depend = m_dependence_caches[m_curstep];
                _apply_except<Ts...>(depend);
            }
            return *this;
        }
        template<typename ... Ts>
        inline selector& contain() noexcept
        {
            if (m_first_time_to_work)
            {
                auto& depend = m_dependence_caches[m_curstep];
                _apply_contain<Ts...>(depend);
            }
            return *this;
        }
        template<typename ... Ts>
        inline selector& anyof() noexcept
        {
            if (m_first_time_to_work)
            {
                auto& depend = m_dependence_caches[m_curstep];
                _apply_anyof<Ts...>(depend, m_any_id);
                ++m_any_id;
            }
            return *this;
        }
    };

    class game_universe
    {
        void* _m_universe_addr;
    public:
        game_universe(void* universe_addr)
            :_m_universe_addr(universe_addr)
        {

        }
        inline void* handle()const noexcept
        {
            return _m_universe_addr;
        }
        inline game_world create_world()
        {
            return je_ecs_world_create(_m_universe_addr);
        }
        inline void wait()const noexcept
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
        selector   _m_default_selector;
    public:
        game_system(game_world world)
            : _m_game_world(world)
            , _m_default_selector(this)
        {
        }

        inline double real_deltatimed() const
        {
            return _m_game_world.get_universe().get_real_deltatimed();
        }
        inline float real_deltatime()const
        {
            return _m_game_world.get_universe().get_real_deltatime();
        }
        inline double deltatimed() const
        {
            return _m_game_world.get_universe().get_smooth_deltatimed();
        }
        inline float deltatime()const
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
        // ATTENTION: Not thread safe!
        inline selector& select_from(game_world w) noexcept
        {
            return _m_default_selector.at(w);
        }
        inline selector& select() noexcept
        {
            return _m_default_selector;
        }

#define StateUpdate         StateUpdate     // 用于将初始状态给予各个组件(PhysicsUpdate Animation)
#define PreUpdate           PreUpdate       // * 用户读取
#define Update              Update          // * 用户写入
#define ScriptUpdate        ScriptUpdate    // 用于脚本控制和更新(RuntimeScript)
#define LateUpdate          LateUpdate      // * 用户更新
#define ApplyUpdate         ApplyUpdate     // 用于最终影响一些特殊组件，这些组件通常不会被其他地方写入(Translation)
#define CommitUpdate        CommitUpdate    // 用于最终提交(Graphic)
    };


    inline game_universe game_world::get_universe() const noexcept
    {
        return game_universe(je_ecs_world_in_universe(handle()));
    }

    template<typename T>
    inline T* game_entity::get_component()const noexcept
    {
        return (T*)je_ecs_world_entity_get_component(this,
            typing::type_info::of<T>());
    }

    template<typename T>
    inline T* game_entity::add_component()const noexcept
    {
        return (T*)je_ecs_world_entity_add_component(this,
            typing::type_info::of<T>());
    }

    template<typename T>
    inline void game_entity::remove_component() const noexcept
    {
        return je_ecs_world_entity_remove_component(this,
            typing::type_info::of<T>());
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

        template<typename T>
        static T clamp(T src, T min, T max)
        {
            return std::min(std::max(src, min), max);
        }
        template<typename T>
        inline T lerp(T va, T vb, float deg)
        {
            return va * (1.0f - deg) + vb * deg;
        }

        template<typename T>
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
#define R(x, y) (out_result[(x) + (y)* 4])
#define A(x, y) (left[(x) + (y)* 4])
#define B(x, y) (right[(x) + (y)* 4])
            memset(out_result, 0, 16 * sizeof(float));
            for (size_t iy = 0; iy < 4; iy++)
                for (size_t ix = 0; ix < 4; ix++)
                {
                    R(ix, iy)
                        += A(ix, 0) * B(0, iy)
                        + A(ix, 1) * B(1, iy)
                        + A(ix, 2) * B(2, iy)
                        + A(ix, 3) * B(3, iy);
                }

#undef R
#undef A
#undef B
        }
        inline void mat4xmat4(float(*out_result)[4], const float(*left)[4], const float(*right)[4])
        {
            return mat4xmat4((float*)out_result, (const float*)left, (const float*)right);
        }

        inline void mat3xmat3(float* out_result, const float* left, const float* right)
        {
#define R(x, y) (out_result[(x) + (y)* 3])
#define A(x, y) (left[(x) + (y)* 3])
#define B(x, y) (right[(x) + (y)* 3])
            memset(out_result, 0, 9 * sizeof(float));
            for (size_t iy = 0; iy < 3; iy++)
                for (size_t ix = 0; ix < 3; ix++)
                {
                    R(ix, iy)
                        += A(ix, 0) * B(0, iy)
                        + A(ix, 1) * B(1, iy)
                        + A(ix, 2) * B(2, iy);
                }

#undef R
#undef A
#undef B
        }
        inline void mat3xmat3(float(*out_result)[3], const float(*left)[3], const float(*right)[3])
        {
            return mat3xmat3((float*)out_result, (const float*)left, (const float*)right);
        }

        inline void mat3xvec3(float* out_result, const float* left_mat, const float* right_vex)
        {
#define R(x) (out_result[x])
#define A(x, y) (left_mat[(x) + (y)* 3])
#define B(x) (right_vex[x])
            R(0) = A(0, 0) * B(0) + A(0, 1) * B(1) + A(0, 2) * B(2);
            R(1) = A(1, 0) * B(0) + A(1, 1) * B(1) + A(1, 2) * B(2);
            R(2) = A(2, 0) * B(0) + A(2, 1) * B(1) + A(2, 2) * B(2);
#undef R
#undef A
#undef B
        }
        inline void mat3xvec3(float* out_result, const float(*left)[3], const float* right)
        {
            return mat3xvec3(out_result, (const float*)left, right);
        }

        inline void mat4xvec4(float* out_result, const float* left_mat, const float* right_vex)
        {
#define R(x) (out_result[x])
#define A(x, y) (left_mat[(x) + (y)* 4])
#define B(x) (right_vex[x])
            R(0) = A(0, 0) * B(0) + A(0, 1) * B(1) + A(0, 2) * B(2) + A(0, 3) * B(3);
            R(1) = A(1, 0) * B(0) + A(1, 1) * B(1) + A(1, 2) * B(2) + A(1, 3) * B(3);
            R(2) = A(2, 0) * B(0) + A(2, 1) * B(1) + A(2, 2) * B(2) + A(2, 3) * B(3);
            R(3) = A(3, 0) * B(0) + A(3, 1) * B(1) + A(3, 2) * B(2) + A(3, 3) * B(3);
#undef R
#undef A
#undef B
        }
        inline void mat4xvec4(float* out_result, const float(*left)[4], const float* right)
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
        inline void mat4xvec3(float* out_result, const float(*left)[4], const float* right)
        {
            mat4xvec3(out_result, (const float*)left, right);
        }

        struct _basevec2
        {
            float x, y;
            constexpr _basevec2(float _x, float _y) noexcept :x(_x), y(_y) {}
        };
        struct _basevec3
        {
            float x, y, z;
            constexpr _basevec3(float _x, float _y, float _z) noexcept :x(_x), y(_y), z(_z) {}
        };
        struct _basevec4
        {
            float x, y, z, w;
            constexpr _basevec4(float _x, float _y, float _z, float _w) noexcept :x(_x), y(_y), z(_z), w(_w) {}
        };

        struct vec2 : public _basevec2
        {
            constexpr vec2(float _x = 0.f, float _y = 0.f) noexcept
                :_basevec2(_x, _y) {}
            constexpr vec2(const vec2& _v2)noexcept
                :_basevec2(_v2.x, _v2.y) {}
            constexpr vec2(vec2&& _v2)noexcept
                :_basevec2(_v2.x, _v2.y) {}

            constexpr vec2(const _basevec3& _v3)noexcept
                :_basevec2(_v3.x, _v3.y) {}
            constexpr vec2(_basevec3&& _v3)noexcept
                :_basevec2(_v3.x, _v3.y) {}
            constexpr vec2(const _basevec4& _v4)noexcept
                :_basevec2(_v4.x, _v4.y) {}
            constexpr vec2(_basevec4&& _v4)noexcept
                :_basevec2(_v4.x, _v4.y) {}

            // + - * / with another vec2
            inline constexpr vec2 operator + (const vec2& _v2) const noexcept
            {
                return vec2(x + _v2.x, y + _v2.y);
            }
            inline constexpr vec2 operator - (const vec2& _v2) const noexcept
            {
                return vec2(x - _v2.x, y - _v2.y);
            }
            inline constexpr vec2 operator - () const noexcept
            {
                return vec2(-x, -y);
            }
            inline constexpr vec2 operator + () const noexcept
            {
                return vec2(x, y);
            }
            inline constexpr vec2 operator * (const vec2& _v2) const noexcept
            {
                return vec2(x * _v2.x, y * _v2.y);
            }
            inline constexpr vec2 operator / (const vec2& _v2) const noexcept
            {
                return vec2(x / _v2.x, y / _v2.y);
            }
            // * / with float
            inline constexpr vec2 operator * (float _f) const noexcept
            {
                return vec2(x * _f, y * _f);
            }
            inline constexpr vec2 operator / (float _f) const noexcept
            {
                return vec2(x / _f, y / _f);
            }

            inline constexpr vec2& operator = (const vec2& _v2) noexcept = default;
            inline constexpr vec2& operator += (const vec2& _v2) noexcept
            {
                x += _v2.x;
                y += _v2.y;
                return *this;
            }
            inline constexpr vec2& operator -= (const vec2& _v2) noexcept
            {
                x -= _v2.x;
                y -= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator *= (const vec2& _v2) noexcept
            {
                x *= _v2.x;
                y *= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator *= (float _f) noexcept
            {
                x *= _f;
                y *= _f;
                return *this;
            }
            inline constexpr vec2& operator /= (const vec2& _v2) noexcept
            {
                x /= _v2.x;
                y /= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator /= (float _f) noexcept
            {
                x /= _f;
                y /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator == (const vec2& _v2) const noexcept
            {
                return x == _v2.x && y == _v2.y;
            }
            inline constexpr bool operator != (const vec2& _v2) const noexcept
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
            /*inline constexpr vec2 cross(const vec2& _v2) const noexcept
            {

            }*/

            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%f,%f)", &x, &y);
            }
        };
        inline static constexpr vec2 operator * (float _f, const vec2& _v2) noexcept
        {
            return vec2(_v2.x * _f, _v2.y * _f);
        }

        struct ivec2
        {
            int x;
            int y;
            constexpr ivec2(int _x = 0, int _y = 0) noexcept
                : x(_x)
                , y(_y)
            {

            }
            constexpr ivec2(const vec2& _v2)noexcept
                : x((int)_v2.x)
                , y((int)_v2.y)
            {

            }

            constexpr ivec2(vec2&& _v2)noexcept
                : x((int)_v2.x)
                , y((int)_v2.y)
            {

            }

            // + - * / with another vec2
            inline constexpr ivec2 operator + (const ivec2& _v2) const noexcept
            {
                return ivec2(x + _v2.x, y + _v2.y);
            }
            inline constexpr ivec2 operator - (const ivec2& _v2) const noexcept
            {
                return ivec2(x - _v2.x, y - _v2.y);
            }
            inline constexpr ivec2 operator - () const noexcept
            {
                return ivec2(-x, -y);
            }
            inline constexpr ivec2 operator + () const noexcept
            {
                return ivec2(x, y);
            }
            inline constexpr ivec2 operator * (const ivec2& _v2) const noexcept
            {
                return ivec2(x * _v2.x, y * _v2.y);
            }
            inline constexpr ivec2 operator / (const ivec2& _v2) const noexcept
            {
                return ivec2(x / _v2.x, y / _v2.y);
            }
            // * / with float
            inline constexpr ivec2 operator * (int _f) const noexcept
            {
                return ivec2(x * _f, y * _f);
            }
            inline constexpr ivec2 operator / (int _f) const noexcept
            {
                return ivec2(x / _f, y / _f);
            }

            inline constexpr ivec2& operator = (const ivec2& _v2) noexcept = default;
            inline constexpr ivec2& operator += (const ivec2& _v2) noexcept
            {
                x += _v2.x;
                y += _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator -= (const ivec2& _v2) noexcept
            {
                x -= _v2.x;
                y -= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator *= (const ivec2& _v2) noexcept
            {
                x *= _v2.x;
                y *= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator *= (float _f) noexcept
            {
                x = (int)((float)x * _f);
                y = (int)((float)y * _f);
                return *this;
            }
            inline constexpr ivec2& operator /= (const ivec2& _v2) noexcept
            {
                x /= _v2.x;
                y /= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator /= (int _f) noexcept
            {
                x /= _f;
                y /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator == (const ivec2& _v2) const noexcept
            {
                return x == _v2.x && y == _v2.y;
            }
            inline constexpr bool operator != (const ivec2& _v2) const noexcept
            {
                return x != _v2.x || y != _v2.y;
            }

            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%d,%d)", &x, &y);
            }
        };

        struct vec3 :public _basevec3
        {
            constexpr vec3(float _x = 0.f, float _y = 0.f, float _z = 0.f)noexcept
                :_basevec3(_x, _y, _z) {}
            constexpr vec3(const vec3& _v3)noexcept
                :_basevec3(_v3.x, _v3.y, _v3.z) {}
            constexpr vec3(vec3&& _v3)noexcept
                :_basevec3(_v3.x, _v3.y, _v3.z) {}

            constexpr vec3(const _basevec2& _v2)noexcept
                :_basevec3(_v2.x, _v2.y, 0.f) {}
            constexpr vec3(_basevec2&& _v2)noexcept
                :_basevec3(_v2.x, _v2.y, 0.f) {}
            constexpr vec3(const _basevec4& _v4)noexcept
                :_basevec3(_v4.x, _v4.y, _v4.z) {}
            constexpr vec3(_basevec4&& _v4)noexcept
                :_basevec3(_v4.x, _v4.y, _v4.z) {}

            // + - * / with another vec3
            inline constexpr vec3 operator + (const vec3& _v3) const noexcept
            {
                return vec3(x + _v3.x, y + _v3.y, z + _v3.z);
            }
            inline constexpr vec3 operator - (const vec3& _v3) const noexcept
            {
                return vec3(x - _v3.x, y - _v3.y, z - _v3.z);
            }
            inline constexpr vec3 operator - () const noexcept
            {
                return vec3(-x, -y, -z);
            }
            inline constexpr vec3 operator + () const noexcept
            {
                return vec3(x, y, z);
            }
            inline constexpr vec3 operator * (const vec3& _v3) const noexcept
            {
                return vec3(x * _v3.x, y * _v3.y, z * _v3.z);
            }
            inline constexpr vec3 operator / (const vec3& _v3) const noexcept
            {
                return vec3(x / _v3.x, y / _v3.y, z / _v3.z);
            }
            // * / with float
            inline constexpr vec3 operator * (float _f) const noexcept
            {
                return vec3(x * _f, y * _f, z * _f);
            }
            inline constexpr vec3 operator / (float _f) const noexcept
            {
                return vec3(x / _f, y / _f, z / _f);
            }

            inline constexpr vec3& operator = (const vec3& _v3) noexcept = default;
            inline constexpr vec3& operator += (const vec3& _v3) noexcept
            {
                x += _v3.x;
                y += _v3.y;
                z += _v3.z;
                return *this;
            }
            inline constexpr vec3& operator -= (const vec3& _v3) noexcept
            {
                x -= _v3.x;
                y -= _v3.y;
                z -= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator *= (const vec3& _v3) noexcept
            {
                x *= _v3.x;
                y *= _v3.y;
                z *= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator *= (float _f) noexcept
            {
                x *= _f;
                y *= _f;
                z *= _f;
                return *this;
            }
            inline constexpr vec3& operator /= (const vec3& _v3) noexcept
            {
                x /= _v3.x;
                y /= _v3.y;
                z /= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator /= (float _f) noexcept
            {
                x /= _f;
                y /= _f;
                z /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator == (const vec3& _v3) const noexcept
            {
                return x == _v3.x && y == _v3.y && z == _v3.z;
            }
            inline constexpr bool operator != (const vec3& _v3) const noexcept
            {
                return x != _v3.x || y != _v3.y || z != _v3.z;
            }

            // length/ unit/ dot/ cross
            inline float length() const noexcept
            {
                return std::sqrt(x * x + y * y + z * z);
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

            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << "," << z << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%f,%f,%f)", &x, &y, &z);
            }
        };
        inline static constexpr vec3 operator * (float _f, const vec3& _v3) noexcept
        {
            return vec3(_v3.x * _f, _v3.y * _f, _v3.z * _f);
        }

        struct vec4 :public _basevec4
        {
            constexpr vec4(float _x = 0.f, float _y = 0.f, float _z = 0.f, float _w = 0.f)noexcept
                :_basevec4(_x, _y, _z, _w) {}
            constexpr vec4(const vec4& _v4)noexcept
                :_basevec4(_v4.x, _v4.y, _v4.z, _v4.w) {}
            constexpr vec4(vec4&& _v4)noexcept
                :_basevec4(_v4.x, _v4.y, _v4.z, _v4.w) {}

            constexpr vec4(const _basevec2& _v2)noexcept
                :_basevec4(_v2.x, _v2.y, 0.f, 0.f) {}
            constexpr vec4(_basevec2&& _v2)noexcept
                :_basevec4(_v2.x, _v2.y, 0.f, 0.f) {}
            constexpr vec4(const _basevec3& _v3)noexcept
                :_basevec4(_v3.x, _v3.y, _v3.z, 0.f) {}
            constexpr vec4(_basevec3&& _v3)noexcept
                :_basevec4(_v3.x, _v3.y, _v3.z, 0.f) {}

            // + - * / with another vec4
            inline constexpr vec4 operator + (const vec4& _v4) const noexcept
            {
                return vec4(x + _v4.x, y + _v4.y, z + _v4.z, w + _v4.w);
            }
            inline constexpr vec4 operator - (const vec4& _v4) const noexcept
            {
                return vec4(x - _v4.x, y - _v4.y, z - _v4.z, w - _v4.w);
            }
            inline constexpr vec4 operator - () const noexcept
            {
                return vec4(-x, -y, -z, -w);
            }
            inline constexpr vec4 operator + () const noexcept
            {
                return vec4(x, y, z, w);
            }
            inline constexpr vec4 operator * (const vec4& _v4) const noexcept
            {
                return vec4(x * _v4.x, y * _v4.y, z * _v4.z, w * _v4.w);
            }
            inline constexpr vec4 operator / (const vec4& _v4) const noexcept
            {
                return vec4(x / _v4.x, y / _v4.y, z / _v4.z, w / _v4.w);
            }
            // * / with float
            inline constexpr vec4 operator * (float _f) const noexcept
            {
                return vec4(x * _f, y * _f, z * _f, w * _f);
            }
            inline constexpr vec4 operator / (float _f) const noexcept
            {
                return vec4(x / _f, y / _f, z / _f, w / _f);
            }

            inline constexpr vec4& operator = (const vec4& _v4) noexcept = default;
            inline constexpr vec4& operator += (const vec4& _v4) noexcept
            {
                x += _v4.x;
                y += _v4.y;
                z += _v4.z;
                w += _v4.w;
                return *this;
            }
            inline constexpr vec4& operator -= (const vec4& _v4) noexcept
            {
                x -= _v4.x;
                y -= _v4.y;
                z -= _v4.z;
                w -= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator *= (const vec4& _v4) noexcept
            {
                x *= _v4.x;
                y *= _v4.y;
                z *= _v4.z;
                w *= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator *= (float _f) noexcept
            {
                x *= _f;
                y *= _f;
                z *= _f;
                w *= _f;
                return *this;
            }
            inline constexpr vec4& operator /= (const vec4& _v4) noexcept
            {
                x /= _v4.x;
                y /= _v4.y;
                z /= _v4.z;
                w /= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator /= (float _f) noexcept
            {
                x /= _f;
                y /= _f;
                z /= _f;
                w /= _f;
                return *this;
            }

            // == !=
            inline constexpr bool operator == (const vec4& _v4) const noexcept
            {
                return x == _v4.x && y == _v4.y && z == _v4.z && w == _v4.w;
            }
            inline constexpr bool operator != (const vec4& _v4) const noexcept
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
            /*inline constexpr vec4 cross(const vec4& _v4) const noexcept
            {
            }*/
            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << "," << z << "," << w << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%f,%f,%f,%f)", &x, &y, &z, &w);
            }
        };
        inline static constexpr vec4 operator * (float _f, const vec4& _v4) noexcept
        {
            return vec4(_v4.x * _f, _v4.y * _f, _v4.z * _f, _v4.w * _f);
        }

        struct quat
        {
            float x, y, z, w;

            inline constexpr bool operator == (const quat& q) const noexcept
            {
                return x == q.x && y == q.y && z == q.z && w == q.w;
            }
            inline constexpr bool operator != (const quat& q) const noexcept
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
                : x(0.f)
                , y(0.f)
                , z(0.f)
                , w(1.f) { }

            quat(float yaw, float pitch, float roll) noexcept
            {
                set_euler_angle(yaw, pitch, roll);
            }

            inline void create_matrix(float* pMatrix) const noexcept
            {
                if (!pMatrix) return;
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

                if (!pMatrix) return;
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

            inline void create_matrix(float(*pMatrix)[4]) const
            {
                create_matrix((float*)pMatrix);
            }
            inline void create_inv_matrix(float(*pMatrix)[4]) const
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
                else sign = 1.f;
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
                    //const float theta = myacos(cos_theta);
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

            static inline quat rotation(const vec3& a, const vec3& b)noexcept
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

            inline quat operator * (const quat& _quat) const noexcept
            {
                float w1 = w;
                float w2 = _quat.w;

                vec3 v1(x, y, z);
                vec3 v2(_quat.x, _quat.y, _quat.z);
                float w3 = w1 * w2 - v1.dot(v2);
                vec3 v3 = v1.cross(v2) + w1 * v2 + w2 * v1;
                return quat(v3.x, v3.y, v3.z, w3);
            }
            inline constexpr vec3 operator *(const vec3& _v3) const noexcept
            {
                vec3 u(x, y, z);
                return 2.0f * u.dot(_v3) * u
                    + (w * w - u.dot(u)) * _v3
                    + 2.0f * w * u.cross(_v3);
            }
            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << "," << z << "," << w << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%f,%f,%f,%f)", &x, &y, &z, &w);
            }
        };

        inline math::vec3 mat4trans(const float* left_mat, const math::vec3& v3)
        {
            float v33[3] = { v3.x, v3.y, v3.z };
            float result[3];
            mat4xvec3(result, left_mat, v33);

            return math::vec3(result[0], result[1], result[2]);
        }
        inline math::vec3 mat4trans(const float(*left_mat)[4], const math::vec3& v3)
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
        inline math::vec4 mat4trans(const float(*left_mat)[4], const math::vec4& v4)
        {
            float v44[4] = { v4.x, v4.y, v4.z, v4.w };
            float result[4];
            mat4xvec4(result, left_mat, v44);

            return math::vec4(result[0], result[1], result[2], result[3]);
        }
    }

    namespace graphic
    {
        class resource_basic
        {
            JECS_DISABLE_MOVE_AND_COPY(resource_basic);

            jegl_resource* _m_resouce;
        protected:
            resource_basic(jegl_resource* res) noexcept
                :_m_resouce(res)
            {
                assert(_m_resouce != nullptr);
            }
        public:
            inline jegl_resource* resouce() const noexcept
            {
                return _m_resouce;
            }
            ~resource_basic()
            {
                assert(_m_resouce != nullptr);
                jegl_close_resource(_m_resouce);
            }
        };

        class texture : public resource_basic
        {
            explicit texture(jegl_resource* res)
                : resource_basic(res)
            {
            }
        public:
            static basic::resource<texture> load(const std::string& str)
            {
                jegl_resource* res = jegl_load_texture(str.c_str());
                if (res != nullptr)
                    return new texture(res);
                return nullptr;
            }
            static basic::resource<texture> create(size_t width, size_t height, jegl_texture::format format)
            {
                jegl_resource* res = jegl_create_texture(width, height, format);

                // Create texture must be successfully.
                assert(res != nullptr);

                if (res != nullptr)
                    return new texture(res);
                return nullptr;
            }

            class pixel
            {
                jegl_texture* _m_texture;
                jegl_texture::pixel_data_t* _m_pixel;
            public:
                pixel(jegl_resource* _texture, size_t x, size_t y) noexcept
                    : _m_texture(_texture->m_raw_texture_data)
                {
                    assert(_texture->m_type == jegl_resource::type::TEXTURE);
                    assert(sizeof(jegl_texture::pixel_data_t) == 1);

                    if (x < _m_texture->m_width && y < _m_texture->m_height)
                        _m_pixel =
                        _m_texture->m_pixels
                        + y * _m_texture->m_width * _m_texture->m_format
                        + x * _m_texture->m_format;
                    else
                        _m_pixel = nullptr;
                }
                inline math::vec4 get() const noexcept
                {
                    if (_m_pixel == nullptr)
                        return {};
                    switch (_m_texture->m_format)
                    {
                    case jegl_texture::format::MONO:
                        return math::vec4{ _m_pixel[0] / 255.0f, _m_pixel[0] / 255.0f, _m_pixel[0] / 255.0f, _m_pixel[0] / 255.0f };
                    case jegl_texture::format::RGBA:
                        return math::vec4{ _m_pixel[0] / 255.0f, _m_pixel[1] / 255.0f, _m_pixel[2] / 255.0f, _m_pixel[3] / 255.0f };
                    default:
                        assert(0); return {};
                    }
                }
                inline void set(const math::vec4& value) const noexcept
                {
                    if (_m_pixel == nullptr)
                        return;
                    _m_texture->m_modified = true;
                    switch (_m_texture->m_format)
                    {
                    case jegl_texture::format::MONO:
                        _m_pixel[0] = math::clamp((jegl_texture::pixel_data_t)round(value.x * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        break;
                    case jegl_texture::format::RGBA:
                        _m_pixel[0] = math::clamp((jegl_texture::pixel_data_t)round(value.x * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        _m_pixel[1] = math::clamp((jegl_texture::pixel_data_t)round(value.y * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        _m_pixel[2] = math::clamp((jegl_texture::pixel_data_t)round(value.z * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        _m_pixel[3] = math::clamp((jegl_texture::pixel_data_t)round(value.w * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        break;
                    default:
                        assert(0); break;
                    }
                }

                inline operator math::vec4()const noexcept
                {
                    return get();
                }
                inline pixel& operator = (const math::vec4& col) noexcept
                {
                    set(col);
                    return *this;
                }
            };

            // pixel's x/y is from LEFT-BUTTOM to RIGHT/TOP
            pixel pix(size_t x, size_t y) const noexcept
            {
                return pixel(resouce(), x, y);
            }
            inline size_t height() const noexcept
            {
                assert(resouce()->m_raw_texture_data != nullptr);
                return resouce()->m_raw_texture_data->m_height;
            }
            inline size_t width() const noexcept
            {
                assert(resouce()->m_raw_texture_data != nullptr);
                return resouce()->m_raw_texture_data->m_width;
            }
            inline math::ivec2 size() const noexcept
            {
                assert(resouce()->m_raw_texture_data != nullptr);
                return math::ivec2(
                    (int)resouce()->m_raw_texture_data->m_width,
                    (int)resouce()->m_raw_texture_data->m_height);
            }
        };

        class shader : public resource_basic
        {
        private:
            explicit shader(jegl_resource* res)
                : resource_basic(res)
                , m_builtin(nullptr)
            {
                m_builtin = &this->resouce()->m_raw_shader_data->m_builtin_uniforms;
            }
        public:
            jegl_shader::builtin_uniform_location* m_builtin;

            static basic::resource<shader> create(const std::string& name_path, const std::string& src)
            {
                jegl_resource* res = jegl_load_shader_source(name_path.c_str(), src.c_str(), true);
                if (res != nullptr)
                    return new shader(res);
                return nullptr;
            }
            static basic::resource<shader> load(const std::string& src_path)
            {
                jegl_resource* res = jegl_load_shader(src_path.c_str());
                if (res != nullptr)
                    return new shader(res);
                return nullptr;
            }

            void set_uniform(const std::string& name, int val)noexcept
            {
                auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::INT)
                            debug::logerr("Trying set uniform('%s' = %d) to shader(%p), but current uniform type is not 'INT'."
                                , name.c_str(), val, this);
                        else
                        {
                            jegl_shad_uniforms->n = val;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, float val)noexcept
            {
                auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::FLOAT)
                            debug::logerr("Trying set uniform('%s' = %f) to shader(%p), but current uniform type is not 'FLOAT'."
                                , name.c_str(), val, this);
                        else
                        {
                            jegl_shad_uniforms->x = val;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec2& val)noexcept
            {
                auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::FLOAT2)
                            debug::logerr("Trying set uniform('%s' = %s) to shader(%p), but current uniform type is not 'FLOAT2'."
                                , name.c_str(), val.to_string().c_str(), this);
                        else
                        {
                            jegl_shad_uniforms->x = val.x;
                            jegl_shad_uniforms->y = val.y;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec3& val)noexcept
            {
                auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::FLOAT3)
                            debug::logerr("Trying set uniform('%s' = %s) to shader(%p), but current uniform type is not 'FLOAT3'."
                                , name.c_str(), val.to_string().c_str(), this);
                        else
                        {
                            jegl_shad_uniforms->x = val.x;
                            jegl_shad_uniforms->y = val.y;
                            jegl_shad_uniforms->z = val.z;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
            void set_uniform(const std::string& name, const math::vec4& val)noexcept
            {
                auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                while (jegl_shad_uniforms)
                {
                    if (jegl_shad_uniforms->m_name == name)
                    {
                        if (jegl_shad_uniforms->m_uniform_type !=
                            jegl_shader::uniform_type::FLOAT4)
                            debug::logerr("Trying set uniform('%s' = %s) to shader(%p), but current uniform type is not 'FLOAT4'."
                                , name.c_str(), val.to_string().c_str(), this);
                        else
                        {
                            jegl_shad_uniforms->x = val.x;
                            jegl_shad_uniforms->y = val.y;
                            jegl_shad_uniforms->z = val.z;
                            jegl_shad_uniforms->w = val.w;
                            jegl_shad_uniforms->m_updated = true;
                        }
                        return;
                    }
                    jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                }
            }
        };

        class vertex : public resource_basic
        {
            explicit vertex(jegl_resource* res)
                : resource_basic(res)
            {
            }
        public:
            static basic::resource<vertex> load(const std::string& str)
            {
                auto* res = jegl_load_vertex(str.c_str());
                if (res != nullptr)
                    return new vertex(res);
                return nullptr;
            }
            static basic::resource<vertex> create(jegl_vertex::type type, const std::vector<float>& pdatas, const std::vector<size_t>& formats)
            {
                auto* res = jegl_create_vertex(type, pdatas.data(), formats.data(), pdatas.size(), formats.size());
                if (res != nullptr)
                    return new vertex(res);
                return nullptr;
            }
        };

        class framebuffer : public resource_basic
        {
            explicit framebuffer(jegl_resource* res)
                : resource_basic(res)
            {
            }
        public:
            static basic::resource<framebuffer> create(size_t reso_w, size_t reso_h, const std::vector<jegl_texture::format>& attachment)
            {
                auto* res = jegl_create_framebuf(reso_w, reso_h, attachment.data(), attachment.size());
                if (res != nullptr)
                    return new framebuffer(res);
                return nullptr;
            }

            basic::resource<texture> get_attachment(size_t index) const
            {
                if (index < resouce()->m_raw_framebuf_data->m_attachment_count)
                {
                    auto* attachments = std::launder(
                        reinterpret_cast<basic::resource<graphic::texture>*>(
                            resouce()->m_raw_framebuf_data->m_output_attachments));
                    return attachments[index];
                }
                return nullptr;
            }

            inline size_t height() const noexcept
            {
                assert(resouce()->m_raw_framebuf_data != nullptr);
                return resouce()->m_raw_framebuf_data->m_height;
            }
            inline size_t width() const noexcept
            {
                assert(resouce()->m_raw_framebuf_data != nullptr);
                return resouce()->m_raw_framebuf_data->m_width;
            }
            inline math::ivec2 size() const noexcept
            {
                assert(resouce()->m_raw_framebuf_data != nullptr);
                return math::ivec2(
                    (int)resouce()->m_raw_framebuf_data->m_width,
                    (int)resouce()->m_raw_framebuf_data->m_height);
            }
        };

        class uniformbuffer : public resource_basic
        {
            explicit uniformbuffer(jegl_resource* res)
                :resource_basic(res)
            {
                assert(resouce() != nullptr);
            }
        public:
            static basic::resource<uniformbuffer> create(size_t binding_place, size_t buffersize)
            {
                jegl_resource* res = jegl_create_uniformbuf(binding_place, buffersize);
                if (res != nullptr)
                    return new uniformbuffer(res);
                return nullptr;
            }

            void update_buffer(size_t offset, size_t size, const void* datafrom) const noexcept
            {
                jegl_update_uniformbuf(resouce(), datafrom, offset, size);
            }
        };

        inline constexpr float ORTHO_PROJECTION_RATIO = 5.0f;

        inline void ortho_projection(
            float(*out_proj_mat)[4],
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
            m[2][2] = 2.0f / (zfar - znear);
            m[2][3] = 0;

            m[3][0] = -((R + L) / (R - L));
            m[3][1] = -((T + B) / (T - B));
            m[3][2] = -((zfar + znear) / (zfar - znear));
            m[3][3] = 1;
        }

        inline void ortho_inv_projection(
            float(*out_proj_mat)[4],
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
            m[2][2] = (zfar - znear) / 2.0f;
            m[2][3] = 0;

            m[3][0] = (R + L) / 2.0f;
            m[3][1] = (T + B) / 2.0f;
            m[3][2] = (zfar + znear) / 2.0f;
            m[3][3] = 1;
        }

        inline void perspective_projection(
            float(*out_proj_mat)[4],
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
            m[2][2] = (-znear - zfar) / ZRANGE;
            m[2][3] = 1.0f;

            m[3][0] = 0;
            m[3][1] = 0;
            m[3][2] = 2.0f * zfar * znear / ZRANGE;
            m[3][3] = 0;
        }

        inline void perspective_inv_projection(
            float(*out_proj_mat)[4],
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
            m[2][3] = ZRANGE / (2 * zfar * znear);

            m[3][0] = 0;
            m[3][1] = 0;
            m[3][2] = 1;
            m[3][3] = (zfar + znear) / (2 * zfar * znear);
        }

        struct character
        {
            // 字符的纹理
            basic::resource<texture> m_texture;
            // 字符本身
            wchar_t                  m_char = 0;
            // 字符本身的宽度和高度, 单位是像素
            // * 包含边框的影响
            int                      m_width = 0;
            int                      m_height = 0;

            // 建议的当前字符的实际横向占位，通常用于计算下一列字符的起始位置，单位是像素，正数表示向右方延伸
            int                      m_advised_w = 0;
            // 建议的当前字符的实际纵向占位，通常用于计算下一行字符的起始位置，单位是像素，正数表示向下方延伸
            int                      m_advised_h = 0;

            // 当前字符基线横向偏移量，单位是像素，正数表示向右方偏移
            // * 包含边框的影响
            int                      m_baseline_offset_x = 0;
            // 当前字符基线纵向偏移量，单位是像素，正数表示向下方偏移
            // * 包含边框的影响
            int                      m_baseline_offset_y = 0;
        };

        class font
        {
            JECS_DISABLE_MOVE_AND_COPY(font);

            je_font* m_font;
        public:
            const size_t          m_size;
            const basic::string   m_path;
        private:
            font(je_font* font_resource, size_t size, const char* path)noexcept
                : m_font(font_resource)
                , m_size(size)
                , m_path(path)
            {

            }
        public:
            static font* load(
                const std::string& fontfile,
                size_t                  size,
                size_t                  board_size = 0,
                je_font_char_updater_t  char_texture_updater = nullptr)
            {
                auto* font_res = je_font_load(
                    fontfile.c_str(), (float)size, (float)size,
                    board_size, board_size, char_texture_updater);

                if (font_res == nullptr)
                    return nullptr;

                return new font(font_res, size, fontfile.c_str());
            }

            ~font()
            {
                je_font_free(m_font);
            }
            character* get_character(unsigned int wcharacter) const noexcept
            {
                return je_font_get_char(m_font, wcharacter);
            }

            basic::resource<texture> text_texture(const std::wstring& text, float one_line_size = 10000.0f, int max_line_count = INT_MAX)
            {
                return text_texture(*this, text, one_line_size, max_line_count);
            }
            basic::resource<texture> text_texture(const std::string& text, float one_line_size = 10000.0f, int max_line_count = INT_MAX)
            {
                return text_texture(*this, wo_str_to_wstr(text.c_str()), one_line_size, max_line_count);
            }
        private:
            inline static basic::resource<texture> text_texture(
                font& font_base,
                const std::wstring& text,
                float max_line_character_size,
                int max_line_count) noexcept
            {
                std::map<std::string, std::map<int, basic::resource<font>>> FONT_POOL;
                math::vec4 TEXT_COLOR = { 1,1,1,1 };
                float TEXT_SCALE = 1.0f;
                math::vec2 TEXT_OFFSET = { 0,0 };

                //计算文字所需要纹理的大小
                int next_ch_x = 0;
                int next_ch_y = 0;
                int line_count = 0;

                int min_px = INT_MAX, min_py = INT_MAX, max_px = INT_MIN, max_py = INT_MIN;

                font* used_font = &font_base;

                for (size_t ti = 0; ti < text.size(); ti++)
                {
                    bool IgnoreEscapeSign = false;
                next_ch_sign:
                    wchar_t ch = text[ti];
                    auto gcs = used_font->get_character((unsigned int)ch);

                    if (gcs)
                    {
                        if (ch == L'\\' && ti + 1 < text.size() && !IgnoreEscapeSign)
                        {
                            ti++;
                            IgnoreEscapeSign = true;
                            goto next_ch_sign;
                        }
                        else if (ch == L'{' && !IgnoreEscapeSign)
                        {
                            std::string item_name;
                            std::string value;

                            for (size_t ei = ti + 1; ei < text.size(); ei++)
                            {
                                if (text[ei] == L':')
                                {
                                    item_name = wo_wstr_to_str(text.substr(ti + 1, ei - ti - 1).c_str());
                                    ti = ei;
                                }
                                else if (text[ei] == L'}')
                                {
                                    value = wo_wstr_to_str(text.substr(ti + 1, ei - ti - 1).c_str());
                                    ti = ei;

                                    if (item_name == "scale")
                                    {
                                        TEXT_SCALE = std::stof(value);
                                        if (TEXT_SCALE == 1.0f)
                                            used_font = &font_base;
                                        else
                                        {
                                            auto& new_font = FONT_POOL[font_base.m_path][int(TEXT_SCALE * font_base.m_size + 0.5f)/*四舍五入*/];
                                            if (new_font == nullptr)
                                                new_font = font::load(font_base.m_path, int(TEXT_SCALE * font_base.m_size + 0.5f));

                                            if (new_font == nullptr)
                                                debug::logerr("Failed to open font: '%s'.", font_base.m_path.c_str());
                                            else
                                                used_font = new_font.get();
                                        }
                                    }
                                    else if (item_name == "offset")
                                    {
                                        math::vec2 offset;
                                        offset.parse(value);
                                        TEXT_OFFSET += offset;
                                    }

                                    break;
                                }
                            }
                        }
                        else if (ch != L'\n')
                        {
                            int px_min = next_ch_x + 0 + gcs->m_baseline_offset_x + int(TEXT_OFFSET.x * font_base.m_size);
                            int py_min = -next_ch_y + 0 + gcs->m_baseline_offset_y - gcs->m_advised_h + int((TEXT_OFFSET.y + TEXT_SCALE - 1.0f) * (float)font_base.m_size);

                            int px_max = next_ch_x + (int)gcs->m_texture->width() - 1 + gcs->m_baseline_offset_x + int(TEXT_OFFSET.x * font_base.m_size);
                            int py_max = -next_ch_y + (int)gcs->m_texture->height() - 1 + gcs->m_baseline_offset_y - gcs->m_advised_h + int((TEXT_OFFSET.y + TEXT_SCALE - 1.0f) * font_base.m_size);

                            min_px = min_px > px_min ? px_min : min_px;
                            min_py = min_py > py_min ? py_min : min_py;

                            max_px = max_px < px_max ? px_max : max_px;
                            max_py = max_py < py_max ? py_max : max_py;

                            next_ch_x += gcs->m_advised_w;
                        }

                        if (ch == L'\n' || next_ch_x >= int(max_line_character_size * font_base.m_size))
                        {
                            next_ch_x = 0;
                            next_ch_y -= gcs->m_advised_h;
                            line_count++;
                        }
                        if (line_count >= max_line_count)
                            break;
                    }
                }
                int size_x = max_px - min_px + 1;
                int size_y = max_py - min_py + 1;

                int correct_x = -min_px;
                int correct_y = -min_py;

                next_ch_x = 0;
                next_ch_y = 0;
                line_count = 0;
                TEXT_SCALE = 1.0f;
                TEXT_OFFSET = { 0,0 };
                used_font = &font_base;

                auto new_texture = texture::create(size_x, size_y, jegl_texture::format::RGBA);
                assert(new_texture != nullptr);
                std::memset(new_texture->resouce()->m_raw_texture_data->m_pixels, 0, size_x * size_y * 4);

                for (size_t ti = 0; ti < text.size(); ti++)
                {
                    bool IgnoreEscapeSign = false;
                next_ch_sign_display:
                    wchar_t ch = text[ti];
                    auto gcs = used_font->get_character((unsigned int)ch);

                    if (gcs)
                    {
                        if (ch == L'\\' && ti + 1 < text.size() && !IgnoreEscapeSign)
                        {
                            ti++;
                            IgnoreEscapeSign = true;
                            goto next_ch_sign_display;
                        }
                        else if (ch == L'{' && !IgnoreEscapeSign)
                        {
                            std::string item_name;
                            std::string value;

                            for (size_t ei = ti + 1; ei < text.size(); ei++)
                            {
                                if (text[ei] == L':')
                                {
                                    item_name = wo_wstr_to_str(text.substr(ti + 1, ei - ti - 1).c_str());
                                    ti = ei;

                                }
                                else if (text[ei] == L'}')
                                {
                                    value = wo_wstr_to_str(text.substr(ti + 1, ei - ti - 1).c_str());
                                    ti = ei;

                                    if (item_name == "color")
                                    {
                                        char color[9] = "00000000";
                                        for (size_t i = 0; i < 8 && i < value.size(); i++)
                                            color[i] = (char)value[i];

                                        unsigned int colordata = strtoul(color, NULL, 16);

                                        unsigned char As = (*(unsigned char*)(&colordata));
                                        unsigned char Bs = (*(((unsigned char*)(&colordata)) + 1));
                                        unsigned char Gs = (*(((unsigned char*)(&colordata)) + 2));
                                        unsigned char Rs = (*(((unsigned char*)(&colordata)) + 3));

                                        TEXT_COLOR = { Rs / 255.0f,Gs / 255.0f ,Bs / 255.0f ,As / 255.0f };
                                    }
                                    else if (item_name == "scale")
                                    {
                                        TEXT_SCALE = std::stof(value);
                                        if (TEXT_SCALE == 1.0f)
                                            used_font = &font_base;
                                        else
                                        {
                                            auto& new_font = FONT_POOL[font_base.m_path][int(TEXT_SCALE * font_base.m_size + 0.5f)/*四舍五入*/];
                                            assert(new_font != nullptr);
                                            used_font = new_font.get();
                                        }
                                    }
                                    else if (item_name == "offset")
                                    {
                                        math::vec2 offset;
                                        offset.parse(value);
                                        TEXT_OFFSET = TEXT_OFFSET + offset;
                                    }
                                    break;
                                }
                            }
                        }
                        else if (ch != L'\n')
                        {
                            struct p_index
                            {
                                size_t id;
                                p_index operator ++()
                                {
                                    return { ++id };
                                }
                                p_index operator ++(int)
                                {
                                    return { id++ };
                                }
                                bool operator ==(const p_index& pindex)const
                                {
                                    return id == pindex.id;
                                }
                                bool operator !=(const p_index& pindex)const
                                {
                                    return id != pindex.id;
                                }
                                size_t& operator *()
                                {
                                    return id;
                                }
                                ptrdiff_t operator-(const p_index& another)const
                                {
                                    return ptrdiff_t(id - another.id);
                                }

                                typedef std::forward_iterator_tag iterator_category;
                                typedef size_t value_type;
                                typedef ptrdiff_t difference_type;
                                typedef const size_t* pointer;
                                typedef const size_t& reference;
                            };
                            std::for_each(
#ifdef __cpp_lib_execution
                                std::execution::par_unseq,
#endif
                                p_index{ 0 }, p_index{ size_t(gcs->m_texture->height()) },
                                [&](size_t fy) {
                                    std::for_each(
#ifdef __cpp_lib_execution
                                        std::execution::par_unseq,
#endif
                                        p_index{ 0 }, p_index{ size_t(gcs->m_texture->width()) },
                                        [&](size_t fx) {
                                            auto pdst = new_texture->pix(
                                                correct_x + next_ch_x + int(fx) + gcs->m_baseline_offset_x + int(TEXT_OFFSET.x * font_base.m_size),
                                                size_y - 1 -
                                                (correct_y - next_ch_y + gcs->m_height + gcs->m_baseline_offset_y - 1 - int(fy) - gcs->m_advised_h
                                                    + int((TEXT_OFFSET.y + TEXT_SCALE - 1.0f) * font_base.m_size))
                                            );

                                            auto psrc = gcs->m_texture->pix(int(fx), int(fy)).get();

                                            float src_alpha = psrc.w * TEXT_COLOR.w;

                                            pdst.set(
                                                math::vec4(
                                                    src_alpha * psrc.x * TEXT_COLOR.x + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().x : 1.0f),
                                                    src_alpha * psrc.y * TEXT_COLOR.y + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().y : 1.0f),
                                                    src_alpha * psrc.z * TEXT_COLOR.z + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().z : 1.0f),
                                                    src_alpha * psrc.w * TEXT_COLOR.w + (1.0f - src_alpha) * pdst.get().w
                                                )
                                            );
                                        }
                                    ); // end of  for each
                                }
                            );
                            next_ch_x += gcs->m_advised_w;
                        }
                        if (ch == L'\n' || next_ch_x >= int(max_line_character_size * font_base.m_size))
                        {
                            next_ch_x = 0;
                            next_ch_y -= gcs->m_advised_h;
                            line_count++;
                        }
                        if (line_count >= max_line_count)
                            break;
                    }
                }
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
            struct default_uniform_buffer_data_t
            {
                float m_v_float4x4[4][4];
                float m_p_float4x4[4][4];
                float m_vp_float4x4[4][4];
                float m_time[4];
            };

            graphic_uhost* _m_graphic_host;
            std::vector<rendchain_branch*>  _m_rchain_pipeline;
            size_t                          _m_this_frame_allocate_rchain_pipeline_count;

            BasePipelineInterface(game_world w, const jegl_interface_config* config)
                : game_system(w)
                , _m_graphic_host(jegl_uhost_get_or_create_for_universe(w.get_universe().handle(), config))
                , _m_this_frame_allocate_rchain_pipeline_count(0)
            {
            }
            ~BasePipelineInterface()
            {
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

    }

    namespace audio
    {
        class buffer
        {
            JECS_DISABLE_MOVE_AND_COPY(buffer);

            jeal_buffer* _m_audio_buffer;
            buffer(jeal_buffer* audio_buffer)
                : _m_audio_buffer(audio_buffer)
            {
                assert(_m_audio_buffer != nullptr);
            }
        public:
            ~buffer()
            {
                jeal_close_buffer(_m_audio_buffer);
            }
            inline static basic::resource<buffer> create(
                const void* data, 
                size_t length, 
                size_t freq, 
                size_t byterate, 
                jeal_format format)
            {
                auto* buf = jeal_create_buffer(data, length, freq, byterate, format);
                if (buf != nullptr)
                    return new buffer(buf);
                return nullptr; 
            }
            inline static basic::resource<buffer> load(const std::string& path)
            {
                auto* buf = jeal_load_buffer_from_wav(path.c_str());
                if (buf != nullptr)
                    return new buffer(buf);
                return nullptr;
            }
            inline size_t get_byte_rate()const
            {
                return jeal_buffer_byte_rate(_m_audio_buffer);
            }
            inline size_t get_byte_size()const
            {
                return jeal_buffer_byte_size(_m_audio_buffer);
            }
            jeal_buffer* handle()const
            {
                return _m_audio_buffer;
            }
        };
        class source
        {
            JECS_DISABLE_MOVE_AND_COPY(source);

            jeal_source* _m_audio_source;
            basic::resource<buffer> _m_playing_buffer;

            source(jeal_source* audio_source)
                : _m_audio_source(audio_source)
                , _m_playing_buffer(nullptr)
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
                return jeal_source_get_state(_m_audio_source);
            }
            inline static basic::resource<source> create()
            {
                auto* src = jeal_open_source();
                assert(src != nullptr);
                return new source(src);
            }
            inline void set_playing_buffer(const basic::resource<buffer>& buffer)
            {
                if (_m_playing_buffer != buffer)
                {
                    _m_playing_buffer = buffer;
                    stop();
                    jeal_source_set_buffer(_m_audio_source, buffer->handle());
                }
            }
            jeal_source* handle()const
            {
                return _m_audio_source;
            }
            void play()
            {
                jeal_source_play(_m_audio_source);
            }
            void pause()
            {
                jeal_source_pause(_m_audio_source);
            }
            void stop()
            {
                jeal_source_stop(_m_audio_source);
            }
            size_t get_playing_offset() const
            {
                return jeal_source_get_byte_offset(_m_audio_source);
            }
            void set_playing_offset(size_t offset)
            {
                jeal_source_set_byte_offset(_m_audio_source, offset);
            }
            void set_pitch(float patch)
            {
                jeal_source_pitch(_m_audio_source, patch);
            }
            void set_volume(float patch)
            {
                jeal_source_volume(_m_audio_source, patch);
            }
            void set_position(const math::vec3& pos)
            {
                jeal_source_position(_m_audio_source, pos.x, pos.y, pos.z);
            }
            void set_velocity(const math::vec3& vlo)
            {
                jeal_source_velocity(_m_audio_source, vlo.x, vlo.y, vlo.z);
            }
            void set_loop(bool looping)
            {
                jeal_source_loop(_m_audio_source, looping);
            }
        };
        class listener
        {
        public:
            inline static void set_position(const math::vec3& pos)
            {
                jeal_listener_position(pos.x, pos.y, pos.z);
            }
            inline static void set_velocity(const math::vec3& pos)
            {
                jeal_listener_velocity(pos.x, pos.y, pos.z);
            }
            inline static void set_direction(const math::quat& rot)
            {
                math::vec3 euler = rot.euler_angle();
                jeal_listener_direction(euler.x, euler.y, euler.z);
            }
            inline static void set_volume(float volume)
            {
                jeal_listener_volume(volume);
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

        struct Translation
        {
            float object2world[4][4] = { };

            math::vec3 world_position = { 0,0,0 };
            math::quat world_rotation;
            math::vec3 local_scale = { 1,1,1 };

            inline void set_position(const math::vec3& _v3) noexcept
            {
                world_position = _v3;
            }
            inline void set_scale(const math::vec3& _v3) noexcept
            {
                local_scale = _v3;
            }
            inline void set_rotation(const math::quat& _quat)noexcept
            {
                world_rotation = _quat;
            }
        };

        struct LocalRotation
        {
            math::quat rot;
            inline math::quat get_parent_global_rotation(const Translation& translation)const noexcept
            {
                return translation.world_rotation * rot.inverse();
            }
            inline void set_global_rotation(const math::quat& _rot, const Translation& translation) noexcept
            {
                auto x = get_parent_global_rotation(translation).inverse();
                rot = _rot * get_parent_global_rotation(translation).inverse();
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalRotation::rot, "rot");
            }
        };

        struct LocalPosition
        {
            math::vec3 pos;

            inline math::vec3 get_parent_global_position(const Translation& translation, const LocalRotation* rotation) const noexcept
            {
                if (rotation)
                    return translation.world_position - rotation->get_parent_global_rotation(translation) * pos;
                else
                    return translation.world_position - translation.world_rotation * pos;
            }

            void set_global_position(const math::vec3& _pos, const Translation& translation, const LocalRotation* rotation) noexcept
            {
                if (rotation)
                    pos = rotation->get_parent_global_rotation(translation).inverse() * (_pos - get_parent_global_position(translation, rotation));
                else
                    pos = translation.world_rotation.inverse() * (_pos - get_parent_global_position(translation, nullptr));
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalPosition::pos, "pos");
            }
        };

        struct LocalScale
        {
            math::vec3 scale = { 1.0f, 1.0f, 1.0f };

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &LocalScale::scale, "scale");
            }
        };

        struct Anchor
        {
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
            bool left_origin = false;
            bool right_origin = false;
            bool top_origin = false;
            bool buttom_origin = false;

            // Will be update by uipipeline
            math::vec2 size = {};
            math::vec2 scale = {};
            math::vec2 global_offset = { 0.0f, 0.0f };

            bool keep_vertical_ratio = false;
            math::vec2 global_location = { 0.0f, 0.0f };

            // 用于计算ui元素的绝对坐标和大小，接受显示区域的宽度和高度，获取以屏幕左下角为原点的元素位置和大小。
            // 其中位置是ui元素中心位置，而非坐标原点位置。
            math::vec4 get_layout(float w, float h) const
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
                if (left_origin)
                    absoffset.x += abssize.x / 2.0f;
                else if (right_origin)
                    absoffset.x += w - abssize.x / 2.0f;
                else
                    absoffset.x += w / 2.0f;

                if (buttom_origin)
                    absoffset.y += abssize.y / 2.0f;
                else if (top_origin)
                    absoffset.y += h - abssize.y / 2.0f;
                else
                    absoffset.y += h / 2.0f;

                return math::vec4(absoffset.x, absoffset.y, abssize.x, abssize.y);
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Origin::left_origin, "left_origin");
                typing::register_member(guard, &Origin::right_origin, "right_origin");
                typing::register_member(guard, &Origin::top_origin, "top_origin");
                typing::register_member(guard, &Origin::buttom_origin, "buttom_origin");
            }
        };
        struct Absolute
        {
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
        struct Distortion
        {
            float angle = 0.0f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Distortion::angle, "angle");
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
            int rend_queue = 0;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Rendqueue::rend_queue, "rend_queue");
            }
        };

        struct Shape
        {
            basic::fileresource<graphic::vertex> vertex;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Shape::vertex, "vertex");
            }
        };

        struct Shaders
        {
            // NOTE: shaders should not be nullptr!
            basic::vector<basic::resource<graphic::shader>> shaders;

            template<typename T>
            void set_uniform(const std::string& name, const T& val) noexcept
            {
                for (auto& shad : shaders)
                    shad->set_uniform(name, val);
            }
        };

        struct Textures
        {
            // NOTE: textures should not be nullptr!
            struct texture_with_passid
            {
                size_t m_pass_id;
                basic::resource<graphic::texture> m_texture;

                texture_with_passid(size_t pass, const basic::resource<graphic::texture>& tex)
                    : m_pass_id(pass)
                    , m_texture(tex)
                {

                }
            };
            math::vec2 tiling = math::vec2(1.f, 1.f);
            math::vec2 offset = math::vec2(0.f, 0.f);
            basic::vector<texture_with_passid> textures;

            void bind_texture(size_t passid, const basic::resource<graphic::texture>& texture)
            {
                assert(texture != nullptr);
                for (auto& [pass, tex] : textures)
                {
                    if (pass == passid)
                    {
                        tex = texture;
                        return;
                    }
                }
                textures.push_back(texture_with_passid(passid, texture));
            }
            basic::resource<graphic::texture> get_texture(size_t passid) const
            {
                for (auto& [pass, tex] : textures)
                    if (pass == passid)
                        return tex;

                return nullptr;
            }

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Textures::tiling, "tiling");
                typing::register_member(guard, &Textures::offset, "offset");
            }
        };

        struct Color
        {
            math::vec4 color;

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
            jeecs::basic::resource<jeecs::graphic::uniformbuffer>
                default_uniform_buffer = jeecs::graphic::uniformbuffer::create(
                    0, sizeof(graphic::BasePipelineInterface::default_uniform_buffer_data_t));

            float view[4][4] = {};
            float projection[4][4] = {};
            float view_projection[4][4] = {};
            float inv_projection[4][4] = {};

            Projection() = default;
            Projection(const Projection&) {/* Do nothing */ }
            Projection(Projection&&) = default;
        };
        struct OrthoProjection
        {
            float scale = 1.0f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &OrthoProjection::scale, "scale");
            }
        };
        struct PerspectiveProjection
        {
            float angle = 75.0f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &PerspectiveProjection::angle, "angle");
            }
        };
        struct Viewport
        {
            math::vec4 viewport = math::vec4(0, 0, 1, 1);
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Viewport::viewport, "viewport");
            }
        };
        struct RendToFramebuffer
        {
            basic::resource<graphic::framebuffer> framebuffer = nullptr;
            math::vec4 clearcolor = {};
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &RendToFramebuffer::clearcolor, "clearcolor");
            }
        };
    }
    namespace Physics2D
    {
        struct World
        {
            size_t      layerid = 0;
            math::vec2  gravity = math::vec2(0.f, -9.8f);
            bool        sleepable = true;
            bool        continuous = true;

            size_t      velocity_step = 8;
            size_t      position_step = 3;

            basic::fileresource<void> group_config;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &World::layerid, "layerid");
                typing::register_member(guard, &World::gravity, "gravity");
                typing::register_member(guard, &World::sleepable, "sleepable");
                typing::register_member(guard, &World::continuous, "continuous");
                typing::register_member(guard, &World::velocity_step, "velocity_step");
                typing::register_member(guard, &World::position_step, "position_step");
                typing::register_member(guard, &World::group_config, "group_config");
            }
        };

        struct Rigidbody
        {
            void* native_rigidbody = nullptr;
            Rigidbody* _arch_updated_modify_hack = nullptr;

            bool        rigidbody_just_created = false;
            math::vec2  record_body_scale = math::vec2(0.f, 0.f);
            float       record_density = 0.f;
            float       record_friction = 0.f;
            float       record_restitution = 0.f;

            size_t      layerid = 0;

            Rigidbody() = default;
            Rigidbody(Rigidbody&&) = default;
            Rigidbody(const Rigidbody& other)
                :layerid(other.layerid)
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
            float density = 1.f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Mass::density, "density");
            }
        };
        struct Friction
        {
            float value = 1.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Friction::value, "value");
            }
        };
        struct Restitution
        {
            float value = 1.f;
            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Restitution::value, "value");
            }
        };
        struct Kinematics
        {
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

        };
        struct BoxCollider
        {
            void* native_fixture = nullptr;
            math::vec2 scale = { 1.f, 1.f };

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &BoxCollider::scale, "scale");
            }
        };
        struct CircleCollider
        {
            void* native_fixture = nullptr;
            float scale = 1.f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &CircleCollider::scale, "scale");
            }
        };

        struct CollisionResult
        {
            struct collide_result
            {
                math::vec2 position;
                math::vec2 normalize;
            };
            basic::map<Rigidbody*, collide_result> results;

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
        struct Color
        {
            math::vec4 color = math::vec4(1, 1, 1, 1);
            float gain = 1.0f;
            float decay = 2.0f;
            float range = 1.0f;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Color::color, "color");
                typing::register_member(guard, &Color::gain, "gain");
                typing::register_member(guard, &Color::decay, "decay");
                typing::register_member(guard, &Color::range, "range");
            }
        };
        struct Parallel
        {

        };
        struct Shadow
        {
            size_t resolution_width = 1024;
            size_t resolution_height = 768;
            float shape_offset = 0.f;

            basic::resource<graphic::framebuffer> shadow_buffer = nullptr;

            Shadow() = default;
            Shadow(const Shadow& another)
                : resolution_width(another.resolution_width)
                , resolution_height(another.resolution_width)
                , shape_offset(another.shape_offset)
            {}
            Shadow(Shadow&&) = default;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Shadow::resolution_width, "resolution_width");
                typing::register_member(guard, &Shadow::resolution_height, "resolution_height");
                typing::register_member(guard, &Shadow::shape_offset, "shape_offset");
            }
        };
        struct CameraPostPass
        {
            basic::resource<graphic::framebuffer> post_rend_target = nullptr;
            basic::resource<jeecs::graphic::framebuffer> post_light_target = nullptr;

            float ratio = 1.0f;

            CameraPostPass() = default;
            CameraPostPass(const CameraPostPass& another) : ratio(another.ratio) {}
            CameraPostPass(CameraPostPass&&) = default;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &CameraPostPass::ratio, "ratio");
            }
        };
        struct Block
        {
            struct block_mesh
            {
                basic::vector<math::vec2> m_block_points = {
                    math::vec2(-0.5f, 0.5f),
                    math::vec2(-0.5f, -0.5f),
                    math::vec2(0.5f, -0.5f),
                    math::vec2(0.5f, 0.5f),
                    math::vec2(-0.5f, 0.5f),
                };
                basic::resource<graphic::vertex> m_block_mesh = nullptr;

                std::string to_string()const
                {
                    std::string result = "#je_light2d_block_shape#";
                    result += "size:" + std::to_string(m_block_points.size()) + ";";
                    for (size_t id = 0; id < m_block_points.size(); ++id)
                    {
                        result += std::to_string(id) + ":" + m_block_points[id].to_string() + ";";
                    }
                    return result;
                }
                void parse(const char* databuf)
                {
                    size_t readed_length = 0;
                    size_t size = 0;
                    if (sscanf(databuf, "#je_light2d_block_shape#size:%zu;%zn", &size, &readed_length) == 1)
                    {
                        databuf += readed_length;
                        m_block_points.clear();
                        m_block_mesh = nullptr;
                        for (size_t i = 0; i < size; ++i)
                        {
                            size_t idx = 0;
                            math::vec2 pos;
                            if (sscanf(databuf, "%zu:(%f,%f);%zn", &idx, &pos.x, &pos.y, &readed_length) == 3)
                            {
                                databuf += readed_length;
                                m_block_points.push_back(pos);
                            }
                            else
                                m_block_points.push_back(math::vec2(0.f, 0.f));
                        }
                    }
                }
            };

            block_mesh mesh;

            float shadow = 1.0f;

            bool nocover = false;
            bool reverse = false;

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Block::mesh, "mesh");
                typing::register_member(guard, &Block::shadow, "shadow");
                typing::register_member(guard, &Block::nocover, "nocover");
                typing::register_member(guard, &Block::reverse, "reverse");
            }
        };
    }
    namespace Animation2D
    {
        struct FrameAnimation
        {
            struct animation_data_set_list
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
                        union value {
                            int i32;
                            float f32;
                            math::vec2 v2;
                            math::vec3 v3;
                            math::vec4 v4;
                            math::quat q4;
                        };

                        type    m_type = type::INT;
                        value   m_value = { 0 };

                        data_value() = default;
                        data_value(const data_value& val)
                        {
                            m_type = val.m_type;
                            memcpy(&m_value, &val.m_value, sizeof(value));
                        }
                        data_value(data_value&& val)
                        {
                            m_type = val.m_type;
                            memcpy(&m_value, &val.m_value, sizeof(value));
                        }
                        data_value& operator = (const data_value& val)
                        {
                            m_type = val.m_type;
                            memcpy(&m_value, &val.m_value, sizeof(value));
                            return *this;
                        }
                        data_value& operator = (data_value&& val)
                        {
                            m_type = val.m_type;
                            memcpy(&m_value, &val.m_value, sizeof(value));
                            return *this;
                        }
                    };
                    struct component_data
                    {
                        const jeecs::typing::type_info* m_component_type;
                        const jeecs::typing::member_info* m_member_info;
                        data_value                          m_member_value;
                        bool                                m_offset_mode;

                        void* m_member_addr_cache;
                        jeecs::game_entity                  m_entity_cache;
                    };
                    struct uniform_data
                    {
                        basic::string                   m_uniform_name;
                        data_value                      m_uniform_value;
                    };

                    float                         m_frame_time;
                    basic::vector<component_data> m_component_data;
                    basic::vector<uniform_data>   m_uniform_data;
                };
                struct animation_data
                {
                    basic::vector<frame_data> frames;
                };

                struct animation_data_set
                {
                    basic::map<basic::string, animation_data>  m_animations;
                    basic::string                              m_path;

                    basic::string       m_current_action = "";
                    size_t              m_current_frame_index = SIZE_MAX;
                    double              m_next_update_time = 0.0f;
                    float               m_last_speed = 1.0f;

                    bool                m_loop = false;

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
                    std::string get_action()const
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
                                                jeecs_file_read(&value.m_value.i32, sizeof(value.m_value.i32), 1, file_handle); break;
                                            case frame_data::data_value::type::FLOAT:
                                                jeecs_file_read(&value.m_value.f32, sizeof(value.m_value.f32), 1, file_handle); break;
                                            case frame_data::data_value::type::VEC2:
                                                jeecs_file_read(&value.m_value.v2, sizeof(value.m_value.v2), 1, file_handle); break;
                                            case frame_data::data_value::type::VEC3:
                                                jeecs_file_read(&value.m_value.v3, sizeof(value.m_value.v3), 1, file_handle); break;
                                            case frame_data::data_value::type::VEC4:
                                                jeecs_file_read(&value.m_value.v4, sizeof(value.m_value.v4), 1, file_handle); break;
                                            case frame_data::data_value::type::QUAT4:
                                                jeecs_file_read(&value.m_value.q4, sizeof(value.m_value.q4), 1, file_handle); break;
                                            default:
                                                jeecs::debug::logerr("Unknown value type(%d) for component frame data.", (int)value.m_type);
                                                break;
                                            }

                                            auto* component_type = jeecs::typing::type_info::of(component_name.c_str());
                                            if (component_type == nullptr)
                                                jeecs::debug::logerr("Failed to found component type named '%s'.", component_name.c_str());
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
                                                    jeecs::debug::logerr("Component '%s' donot have member named '%s'.", component_name.c_str(), member_name.c_str());
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
                                                jeecs_file_read(&value.m_value.i32, sizeof(value.m_value.i32), 1, file_handle); break;
                                            case frame_data::data_value::type::FLOAT:
                                                jeecs_file_read(&value.m_value.f32, sizeof(value.m_value.f32), 1, file_handle); break;
                                            case frame_data::data_value::type::VEC2:
                                                jeecs_file_read(&value.m_value.v2, sizeof(value.m_value.v2), 1, file_handle); break;
                                            case frame_data::data_value::type::VEC3:
                                                jeecs_file_read(&value.m_value.v3, sizeof(value.m_value.v3), 1, file_handle); break;
                                            case frame_data::data_value::type::VEC4:
                                                jeecs_file_read(&value.m_value.v4, sizeof(value.m_value.v4), 1, file_handle); break;
                                            default:
                                                jeecs::debug::logerr("Unknown value type(%d) for uniform frame data.", (int)value.m_type);
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
                std::vector<animation_data_set> m_animations;

                void active_action(size_t id, const char* act_name, bool loop)
                {
                    if (id < m_animations.size())
                    {
                        if (m_animations[id].get_action() != act_name)
                            m_animations[id].set_action(act_name);
                        m_animations[id].set_loop(loop);
                    }
                }

                std::string to_string()const
                {
                    std::string result = "#je_animation_list#";
                    result += std::to_string(m_animations.size()) + ";";
                    for (size_t id = 0; id < m_animations.size(); ++id)
                    {
                        auto& animation = m_animations[id];
                        result +=
                            std::string(animation.m_path.c_str())
                            + "|"
                            + animation.m_current_action.c_str()
                            + "|"
                            + (animation.m_loop ? "true" : "false")
                            + ";";
                    }

                    return result;
                }
                void parse(const char* databuf)
                {
                    size_t readed_length = 0;
                    size_t size = 0;
                    if (sscanf(databuf, "#je_animation_list#%zu;%zn", &size, &readed_length) == 1)
                    {
                        databuf += readed_length;
                        m_animations.clear();
                        for (size_t i = 0; i < size; ++i)
                        {
                            char animation_path[256] = {};
                            char action_name[256] = {};
                            char is_loop[8] = {};

                            auto& animation = m_animations.emplace_back(animation_data_set{});

                            size_t widx = 0;
                            while (*databuf != 0 && widx <= 255)
                            {
                                char rdch = *(databuf++);
                                if (rdch == '|')
                                    break;

                                animation_path[widx++] = rdch;
                            }
                            widx = 0;
                            while (*databuf != 0 && widx <= 255)
                            {
                                char rdch = *(databuf++);
                                if (rdch == '|')
                                    break;

                                action_name[widx++] = rdch;
                            }
                            widx = 0;
                            while (*databuf != 0 && widx <= 7)
                            {
                                char rdch = *(databuf++);
                                if (rdch == ';')
                                    break;

                                is_loop[widx++] = rdch;
                            }

                            animation.load_animation(animation_path);
                            animation.set_action(action_name);
                            animation.m_loop = (strcmp(is_loop, "true") == 0);
                        }
                    }
                }
            };

            animation_data_set_list animations;
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
            basic::resource<audio::source> source = audio::source::create();

            float pitch = 1.0f;
            float volume = 1.0f;

            math::vec3 last_position = {};

            Source() = default;
            Source(const Source& another)
                : pitch(another.pitch)
                , volume(another.volume)
                , last_position(another.last_position)
            {
                assert(source != nullptr && source != another.source);
            }
            Source(Source&& another)
                : source(std::move(another.source))
                , pitch(another.pitch)
                , volume(another.volume)
                , last_position(another.last_position)
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
            float volume = 1.0f;

            math::vec3 last_position = {};

            static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
            {
                typing::register_member(guard, &Listener::volume, "volume");
            }
        };
        struct Playing
        {
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

    inline typing::euid_t game_entity::get_euid() const noexcept
    {
        return je_ecs_entity_uid(this);
    }

    namespace math
    {
        struct ray
        {
        public:
            vec3 orgin = { 0,0,0 };
            vec3 direction = { 0,0,1 };

            ray() = default;

            ray(ray&&) = default;
            ray(const ray&) = default;

            ray& operator = (ray&&) = default;
            ray& operator = (const ray&) = default;

            ray(const vec3& _orgin, const vec3& _direction) :
                orgin(_orgin),
                direction(_direction)
            {

            }
            ray(const Transform::Translation& camera_trans, const Camera::Projection& camera_proj, const vec2& screen_pos, bool ortho)
            {
                //根据摄像机和屏幕坐标创建射线
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
                intersect_result(bool rslt, float dist = INFINITY, const vec3& plce = vec3(0, 0, 0)) :
                    intersected(rslt),
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

                *u = T.dot(P);// T.dot(P);
                if (*u < 0.0f || *u > det)
                    return false;

                // Q

                vec3 Q = T.cross(E1);// T.Cross(E1);

                // Calculate v and make sure u + v <= 1
                *v = direction.dot(Q); //direction.dot(Q);

                if (*v < 0.0f || *u + *v > det)
                    return false;

                // Calculate t, scale parameters, ray intersects triangle

                *t = E2.dot(Q);// .dot(Q);

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
                //if (vec3::dot(delta, direction) < 0.0f)
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
            intersect_result intersect_box(const vec3& size, const vec3& centerpos, const quat& rotation) const
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

                //pos
                finalBoxPos[0].x = finalBoxPos[2].x = finalBoxPos[4].x = finalBoxPos[6].x =
                    -(finalBoxPos[1].x = finalBoxPos[3].x = finalBoxPos[5].x = finalBoxPos[7].x = size.x / 2.0f);
                finalBoxPos[2].y = finalBoxPos[3].y = finalBoxPos[6].y = finalBoxPos[7].y =
                    -(finalBoxPos[0].y = finalBoxPos[1].y = finalBoxPos[4].y = finalBoxPos[5].y = size.y / 2.0f);
                finalBoxPos[0].z = finalBoxPos[1].z = finalBoxPos[2].z = finalBoxPos[3].z =
                    -(finalBoxPos[4].z = finalBoxPos[5].z = finalBoxPos[6].z = finalBoxPos[7].z = size.z / 2.0f);

                //rot and transform
                for (int i = 0; i < 8; i++)
                    finalBoxPos[i] = (rotation * finalBoxPos[i]) + centerpos;
                {
                    //front
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[3], finalBoxPos[2]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //back
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[4], finalBoxPos[5], finalBoxPos[7], finalBoxPos[6]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //left
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[2], finalBoxPos[6], finalBoxPos[4]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //right
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[1], finalBoxPos[3], finalBoxPos[7], finalBoxPos[5]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //top
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[5], finalBoxPos[4]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //buttom
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[2], finalBoxPos[3], finalBoxPos[7], finalBoxPos[6]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                }


                return minResult;
            }
            intersect_result intersect_entity(const Transform::Translation& translation, const Renderer::Shape* entity_shape, float insRange = 0.0f) const
            {
                vec3 entity_box_sz;
                if (entity_shape && entity_shape->vertex.has_resource())
                {
                    auto* vertex_dat = entity_shape->vertex.get_resource()->resouce()->m_raw_vertex_data;
                    entity_box_sz = { vertex_dat->m_size_x, vertex_dat->m_size_y ,vertex_dat->m_size_z };
                }
                else
                    entity_box_sz = vec3(1, 1, 0); // default shape size

                vec3 finalBoxPos[8];
                intersect_result minResult = false;
                minResult.distance = INFINITY;

                //pos
                finalBoxPos[0].x = finalBoxPos[2].x = finalBoxPos[4].x = finalBoxPos[6].x =
                    -(finalBoxPos[1].x = finalBoxPos[3].x = finalBoxPos[5].x = finalBoxPos[7].x = entity_box_sz.x / 2.0f);
                finalBoxPos[2].y = finalBoxPos[3].y = finalBoxPos[6].y = finalBoxPos[7].y =
                    -(finalBoxPos[0].y = finalBoxPos[1].y = finalBoxPos[4].y = finalBoxPos[5].y = entity_box_sz.y / 2.0f);
                finalBoxPos[0].z = finalBoxPos[1].z = finalBoxPos[2].z = finalBoxPos[3].z =
                    -(finalBoxPos[4].z = finalBoxPos[5].z = finalBoxPos[6].z = finalBoxPos[7].z = entity_box_sz.z / 2.0f);

                //rot and transform
                for (int i = 0; i < 8; i++)
                    finalBoxPos[i] = mat4trans(translation.object2world, translation.local_scale * finalBoxPos[i]);

                {
                    //front
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[3], finalBoxPos[2]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //back
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[4], finalBoxPos[5], finalBoxPos[7], finalBoxPos[6]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //left
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[2], finalBoxPos[6], finalBoxPos[4]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //right
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[1], finalBoxPos[3], finalBoxPos[7], finalBoxPos[5]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //top
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[5], finalBoxPos[4]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //buttom
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[2], finalBoxPos[3], finalBoxPos[7], finalBoxPos[6]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
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
            type_info::register_type<UserInterface::Distortion>(guard, "UserInterface::Distortion");
            type_info::register_type<UserInterface::Absolute>(guard, "UserInterface::Absolute");
            type_info::register_type<UserInterface::Relatively>(guard, "UserInterface::Relatively");

            type_info::register_type<Renderer::Rendqueue>(guard, "Renderer::Rendqueue");
            type_info::register_type<Renderer::Shape>(guard, "Renderer::Shape");
            type_info::register_type<Renderer::Shaders>(guard, "Renderer::Shaders");
            type_info::register_type<Renderer::Textures>(guard, "Renderer::Textures");
            type_info::register_type<Renderer::Color>(guard, "Renderer::Color");

            type_info::register_type<Animation2D::FrameAnimation>(guard, "Animation2D::FrameAnimation");

            type_info::register_type<Camera::Clip>(guard, "Camera::Clip");
            type_info::register_type<Camera::FrustumCulling>(guard, "Camera::FrustumCulling");
            type_info::register_type<Camera::Projection>(guard, "Camera::Projection");
            type_info::register_type<Camera::OrthoProjection>(guard, "Camera::OrthoProjection");
            type_info::register_type<Camera::PerspectiveProjection>(guard, "Camera::PerspectiveProjection");
            type_info::register_type<Camera::Viewport>(guard, "Camera::Viewport");
            type_info::register_type<Camera::RendToFramebuffer>(guard, "Camera::RendToFramebuffer");

            type_info::register_type<Light2D::Color>(guard, "Light2D::Color");
            type_info::register_type<Light2D::Parallel>(guard, "Light2D::Parallel");
            type_info::register_type<Light2D::Shadow>(guard, "Light2D::Shadow");
            type_info::register_type<Light2D::CameraPostPass>(guard, "Light2D::CameraPostPass");
            type_info::register_type<Light2D::Block>(guard, "Light2D::Block");

            type_info::register_type<Physics2D::World>(guard, "Physics2D::World");
            type_info::register_type<Physics2D::Rigidbody>(guard, "Physics2D::Rigidbody");
            type_info::register_type<Physics2D::Kinematics>(guard, "Physics2D::Kinematics");
            type_info::register_type<Physics2D::Mass>(guard, "Physics2D::Mass");
            type_info::register_type<Physics2D::Bullet>(guard, "Physics2D::Bullet");
            type_info::register_type<Physics2D::BoxCollider>(guard, "Physics2D::BoxCollider");
            type_info::register_type<Physics2D::CircleCollider>(guard, "Physics2D::CircleCollider");
            type_info::register_type<Physics2D::Restitution>(guard, "Physics2D::Restitution");
            type_info::register_type<Physics2D::Friction>(guard, "Physics2D::Friction");
            type_info::register_type<Physics2D::CollisionResult>(guard, "Physics2D::CollisionResult");

            type_info::register_type<Audio::Source>(guard, "Audio::Source");
            type_info::register_type<Audio::Listener>(guard, "Audio::Listener");
            type_info::register_type<Audio::Playing>(guard, "Audio::Playing");

            // 1. register basic types
            typing::register_script_parser<bool>(
                guard,
                [](wo_vm, wo_value value, const bool* v) {
                    wo_set_bool(value, *v);
                },
                [](wo_vm, wo_value value, bool* v) {
                    *v = wo_bool(value);
                }, "bool", "");

            auto integer_uniform_parser_c2w = [](wo_vm, wo_value value, const auto* v) {
                wo_set_int(value, (wo_integer_t)*v);
                };
            auto integer_uniform_parser_w2c = [](wo_vm, wo_value value, auto* v) {
                *v = (typename std::remove_reference<decltype(*v)>::type)wo_int(value);
                };
            typing::register_script_parser<int8_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int8", "alias int8 = int;");
            typing::register_script_parser<int16_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int16", "alias int16 = int;");
            typing::register_script_parser<int32_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int32", "alias int32 = int;");
            typing::register_script_parser<int64_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int64", "alias int64 = int;");
            typing::register_script_parser<uint8_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint8", "alias uint8 = int;");
            typing::register_script_parser<uint16_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint16", "alias uint16 = int;");
            typing::register_script_parser<uint32_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint32", "alias uint32 = int;");
            typing::register_script_parser<uint64_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint64", "alias uint64 = int;");

            typing::register_script_parser<float>(
                guard,
                [](wo_vm, wo_value value, const float* v) {
                    wo_set_float(value, *v);
                },
                [](wo_vm, wo_value value, float* v) {
                    *v = wo_float(value);
                }, "float", "alias float = real;");
            typing::register_script_parser<double>(
                guard,
                [](wo_vm, wo_value value, const double* v) {
                    wo_set_real(value, *v);
                },
                [](wo_vm, wo_value value, double* v) {
                    *v = wo_real(value);
                }, "real", "");

            typing::register_script_parser<jeecs::basic::string>(
                guard,
                [](wo_vm vm, wo_value value, const jeecs::basic::string* v) {
                    wo_set_string(value, vm, v->c_str());
                },
                [](wo_vm, wo_value value, jeecs::basic::string* v) {
                    *v = wo_string(value);
                }, "string", "");

            typing::register_script_parser<jeecs::math::ivec2>(
                guard,
                [](wo_vm vm, wo_value value, const jeecs::math::ivec2* v) {
                    wo_set_struct(value, vm, 2);
                    wo_value elem = wo_push_empty(vm);

                    wo_set_int(elem, (wo_integer_t)v->x);
                    wo_struct_set(value, 0, elem);

                    wo_set_int(elem, (wo_integer_t)v->y);
                    wo_struct_set(value, 1, elem);
                },
                [](wo_vm vm, wo_value value, jeecs::math::ivec2* v) {
                    wo_value elem = wo_push_empty(vm);

                    wo_struct_get(elem, value, 0);
                    v->x = (int)wo_int(elem);

                    wo_struct_get(elem, value, 1);
                    v->y = (int)wo_int(elem);
                }, "ivec2", "public using ivec2 = (int, int);");

            typing::register_script_parser<jeecs::math::vec2>(
                guard,
                [](wo_vm vm, wo_value value, const jeecs::math::vec2* v) {
                    wo_set_struct(value, vm, 2);
                    wo_value elem = wo_push_empty(vm);

                    wo_set_float(elem, v->x);
                    wo_struct_set(value, 0, elem);

                    wo_set_float(elem, v->y);
                    wo_struct_set(value, 1, elem);
                },
                [](wo_vm vm, wo_value value, jeecs::math::vec2* v) {
                    wo_value elem = wo_push_empty(vm);

                    wo_struct_get(elem, value, 0);
                    v->x = wo_float(elem);

                    wo_struct_get(elem, value, 1);
                    v->y = wo_float(elem);
                }, "vec2", "public using vec2 = (real, real);");

            typing::register_script_parser<jeecs::math::vec3>(
                guard,
                [](wo_vm vm, wo_value value, const jeecs::math::vec3* v) {
                    wo_set_struct(value, vm, 3);
                    wo_value elem = wo_push_empty(vm);

                    wo_set_float(elem, v->x);
                    wo_struct_set(value, 0, elem);

                    wo_set_float(elem, v->y);
                    wo_struct_set(value, 1, elem);

                    wo_set_float(elem, v->z);
                    wo_struct_set(value, 2, elem);
                },
                [](wo_vm vm, wo_value value, jeecs::math::vec3* v) {
                    wo_value elem = wo_push_empty(vm);

                    wo_struct_get(elem, value, 0);
                    v->x = wo_float(elem);

                    wo_struct_get(elem, value, 1);
                    v->y = wo_float(elem);

                    wo_struct_get(elem, value, 2);
                    v->z = wo_float(elem);
                }, "vec3", "public using vec3 = (real, real, real);");

            typing::register_script_parser<jeecs::math::vec4>(
                guard,
                [](wo_vm vm, wo_value value, const jeecs::math::vec4* v) {
                    wo_set_struct(value, vm, 4);
                    wo_value elem = wo_push_empty(vm);

                    wo_set_float(elem, v->x);
                    wo_struct_set(value, 0, elem);

                    wo_set_float(elem, v->y);
                    wo_struct_set(value, 1, elem);

                    wo_set_float(elem, v->z);
                    wo_struct_set(value, 2, elem);

                    wo_set_float(elem, v->w);
                    wo_struct_set(value, 2, elem);
                },
                [](wo_vm vm, wo_value value, jeecs::math::vec4* v) {
                    wo_value elem = wo_push_empty(vm);

                    wo_struct_get(elem, value, 0);
                    v->x = wo_float(elem);

                    wo_struct_get(elem, value, 1);
                    v->y = wo_float(elem);

                    wo_struct_get(elem, value, 2);
                    v->z = wo_float(elem);

                    wo_struct_get(elem, value, 3);
                    v->w = wo_float(elem);
                }, "vec4", "public using vec4 = (real, real, real, real);");

            typing::register_script_parser<jeecs::math::quat>(
                guard,
                [](wo_vm vm, wo_value value, const jeecs::math::quat* v) {
                    wo_set_struct(value, vm, 4);
                    wo_value elem = wo_push_empty(vm);

                    wo_set_float(elem, v->x);
                    wo_struct_set(value, 0, elem);

                    wo_set_float(elem, v->y);
                    wo_struct_set(value, 1, elem);

                    wo_set_float(elem, v->z);
                    wo_struct_set(value, 2, elem);

                    wo_set_float(elem, v->w);
                    wo_struct_set(value, 3, elem);
                },
                [](wo_vm vm, wo_value value, jeecs::math::quat* v) {
                    wo_value elem = wo_push_empty(vm);

                    wo_struct_get(elem, value, 0);
                    v->x = wo_float(elem);

                    wo_struct_get(elem, value, 1);
                    v->y = wo_float(elem);

                    wo_struct_get(elem, value, 2);
                    v->z = wo_float(elem);

                    wo_struct_get(elem, value, 3);
                    v->w = wo_float(elem);
                }, "quat", "public using quat = (real, real, real, real);");

            // 1. register core&graphic systems.
            jeecs_entry_register_core_systems(guard);

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
            return je_io_get_keydown(key);
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
            je_io_get_windowsize(&w, &h);
            return { ((float)x / (float)w - 0.5f) * 2.0f, ((float)y / (float)h - 0.5f) * -2.0f };
        }
        inline math::ivec2 windowsize()
        {
            int x, y;
            je_io_get_windowsize(&x, &y);
            return { x, y };
        }

        template<typing::typehash_t hash_v1, int v2>
        static bool _isUp(bool keystate)
        {
            static bool lastframekeydown;
            bool res = (!keystate) && lastframekeydown;
            lastframekeydown = keystate;
            return res;
        }
        template<typing::typehash_t hash_v1, int v2>
        static bool _firstDown(bool keystate)
        {
            static bool lastframekeydown;
            bool res = (keystate) && !lastframekeydown;
            lastframekeydown = keystate;
            return res;
        }
        template<typing::typehash_t hash_v1, int v2>
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

        static void is_up(...);
        static void first_down(...);
        static void double_click(...);// just for fool ide

#define is_up _isUp<jeecs::basic::hash_compile_time(__FILE__),__LINE__>
#define first_down _firstDown<jeecs::basic::hash_compile_time(__FILE__),__LINE__>
#define double_click _doubleClick<jeecs::basic::hash_compile_time(__FILE__),__LINE__>
    }
}

#endif
