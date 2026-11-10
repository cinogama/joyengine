#pragma once

// ============================================================================
// jeecs/typing.hpp —— 类型子系统：je_typing_* / je_towoo_* C API、je_uid_generate，以及 jeecs::typing C++ 层（uuid/uid、类型注册 register_type/register_member/register_script_parser、type_unregister_guard、traits）
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/basic.hpp"
#include "jeecs/logging.hpp"

// je_uuid_t：全局唯一标识符（平凡布局）。C++ 端 jeecs::typing::uuid 以公开继承方式扩展它（加方法）。
typedef struct je_uuid_t {
    union {
        struct {
            uint64_t a;
            uint64_t b;
        };
        struct {
            uint32_t x; // Time stamp
            uint16_t y; // Time stamp
            uint16_t z; // Random

            uint16_t w; // Inc L16
            uint16_t u; // Inc H16
            uint32_t v; // Random
        };
    };
} je_uuid_t;
JE_FORCE_CAPI

typedef enum je_typing_class_t
{
    JE_BASIC_TYPE,
    JE_COMPONENT,
    JE_SYSTEM,
} je_typing_class_t;

// ==========================================================================
// 类型子系统（typing）C 类型
// je_type_info_t 及其关联类型拥有公开的平凡数据布局（非不透明）。C++ 包装层
// （jeecs::typing）直接使用这些 C 类型；原 je_type_info_t 上的便捷方法以
// jeecs::typing 命名空间下的自由函数形式提供。
// ==========================================================================

/* 前向声明：函数指针类型与 je_member_info_t 需要引用 je_type_info_t */
typedef struct je_type_info_t je_type_info_t;

/* 类型子系统使用的函数指针类型（C ABI 签名直接使用） */
typedef void (*je_construct_func_t)(void* addr, void* arg, const je_type_info_t* tinfo);
typedef void (*je_destruct_func_t)(void* addr);
typedef void (*je_copy_construct_func_t)(void* dst, const void* src);
typedef void (*je_move_construct_func_t)(void* dst, void* src);
typedef void (*je_on_enable_or_disable_func_t)(void* sys);
typedef void (*je_update_func_t)(void* sys);
typedef void (*je_parse_c2w_func_t)(const void* c_data, woort_value w_value);
typedef void (*je_parse_w2c_func_t)(void* c_data, woort_value w_value);

/* 组件成员信息节点 */
typedef struct je_member_info_t {
    const je_type_info_t* m_class_type;
    const char* m_member_name;
    const char* m_woovalue_type_may_null;
    woort_GCPin* m_woovalue_init_may_null;
    const je_type_info_t* m_member_type;
    ptrdiff_t m_member_offset;
    struct je_member_info_t* m_next_member;
} je_member_info_t;

/* 组件成员信息集合 */
typedef struct je_typeinfo_member_t {
    size_t m_member_count;
    je_member_info_t* m_members;
} je_typeinfo_member_t;

/* woolang 转换信息 */
typedef struct je_typeinfo_script_parser_t {
    je_parse_c2w_func_t m_script_parse_c2w;
    je_parse_w2c_func_t m_script_parse_w2c;
    const char* m_woolang_typename;
    const char* m_woolang_typedecl;
} je_typeinfo_script_parser_t;

/* 系统更新方法集 */
typedef struct je_typeinfo_system_updater_t {
    je_on_enable_or_disable_func_t m_on_enable;
    je_on_enable_or_disable_func_t m_on_disable;
    je_update_func_t m_pre_update;
    je_update_func_t m_state_update;
    je_update_func_t m_update;
    je_update_func_t m_physics_update;
    je_update_func_t m_transform_update;
    je_update_func_t m_late_update;
    je_update_func_t m_commit_update;
    je_update_func_t m_graphic_update;
} je_typeinfo_system_updater_t;

/* 类型信息（完整平凡布局，非不透明） */
typedef struct je_type_info_t {
    je_type_id_t m_id;
    const char* m_typename; // will be free by je_typing_unregister
    size_t m_size;
    size_t m_align;
    je_type_hash_t m_hash;
    je_construct_func_t m_constructor;
    je_destruct_func_t m_destructor;
    je_copy_construct_func_t m_copier;
    je_move_construct_func_t m_mover;
    je_typing_class_t m_type_class;
    const je_typeinfo_member_t* m_member_types;
    const je_typeinfo_script_parser_t* m_script_parsers;
    const je_typeinfo_system_updater_t* m_system_updaters;
    const je_type_info_t* m_next;
} je_type_info_t;

/*
je_typing_register [基本接口]
向引擎的类型管理器注册一个类型及其基本信息，返回记录当前类型的类型地址
* 类型的名称是区分的唯一标记符，不同类型必须使用不同的名字。
* 必须通过 je_typing_unregister 在适当时机释放
请参见：
    je_type_id_t
    je_typing_unregister
*/
JE_API const je_type_info_t* je_typing_register(
    const char* _name,
    je_type_hash_t _hash,
    size_t _size,
    size_t _align,
    je_typing_class_t _typecls,
    je_construct_func_t _constructor,
    je_destruct_func_t _destructor,
    je_copy_construct_func_t _copy_constructor,
    je_move_construct_func_t _move_constructor);

/*
je_typing_reset [基本接口]
被设计用于重置指定类型的大小、对齐、成员和构造函数等
    * 成员字段将被重置，请重新注册
*/
JE_API void je_typing_reset(
    const je_type_info_t* _tinfo,
    size_t _size,
    size_t _align,
    je_construct_func_t _constructor,
    je_destruct_func_t _destructor,
    je_copy_construct_func_t _copy_constructor,
    je_move_construct_func_t _move_constructor);

/*
je_typing_info_get_by_id [基本接口]
通过类型id获取类型信息，若给定的id不合法，返回nullptr
请参见：
    je_type_id_t
    je_type_info_t
*/
JE_API /* OPTIONAL: If _id is invalid or unregistered */ const je_type_info_t* je_typing_info_get_by_id(
    je_type_id_t _id);

/*
je_typing_info_get_by_hash [基本接口]
通过类型的哈希值获取类型信息，若给定的类型哈希不合法，返回nullptr
请参见：
    je_type_hash_t
    je_type_info_t
*/
JE_API /* OPTIONAL: If _hash is not registered */ const je_type_info_t* je_typing_info_get_by_hash(
    je_type_hash_t _hash);

/*
je_typing_info_get_by_name [基本接口]
通过类型的名称获取类型信息，若给定的类型名不合法或不存在，返回nullptr
请参见：
    je_type_info_t
*/
JE_API /* OPTIONAL: If type_name is not found */ const je_type_info_t* je_typing_info_get_by_name(
    const char* type_name);

/*
je_typing_unregister [基本接口]
向引擎的类型管理器要求解除注册指定的类型信息
需要注意的是，一般而言引擎推荐遵循“谁注册谁释放”原则，请确保释放的
类型是当前模块通过 je_typing_register 成功注册的类型。
若释放的类型不合法，则给出级别错误的日志信息。
请参见：
    je_type_info_t
    je_typing_register
*/
JE_API void je_typing_unregister(const je_type_info_t* tinfo);

/*
je_typing_member_register [基本接口]
向引擎的类型管理器注册指定类型的成员信息。
* 使用本地typeinfo，而非全局通用typeinfo
请参见：
    je_type_info_t
*/
JE_API void je_typing_member_register(
    const je_type_info_t* _classtype,
    const je_type_info_t* _membertype,
    const char* _member_name,
    /* OPTIONAL */ const char* _woovalue_type_may_null,
    woort_value _boxed_woovalue_init_may_ignored /* Use WOORT_IGNORE as none init */,
    ptrdiff_t _member_offset);

/*
je_typing_script_parser_register [基本接口]
向引擎的类型管理器注册指定类型的脚本转换方法。
* 使用本地typeinfo，而非全局通用typeinfo
请参见：
    je_type_info_t
*/
JE_API void je_typing_script_parser_register(
    const je_type_info_t* _type,
    je_parse_c2w_func_t c2w,
    je_parse_w2c_func_t w2c,
    const char* woolang_typename,
    const char* woolang_typedecl);

/*
je_typing_system_updater_register [基本接口]
向引擎的类型管理器注册指定类型的系统更新方法。
* 使用本地typeinfo，而非全局通用typeinfo
请参见：
    je_type_info_t
*/
JE_API void je_typing_system_updater_register(
    const je_type_info_t* _type,
    je_on_enable_or_disable_func_t _on_enable,
    je_on_enable_or_disable_func_t _on_disable,
    je_update_func_t _pre_update,
    je_update_func_t _state_update,
    je_update_func_t _update,
    je_update_func_t _physics_update,
    je_update_func_t _transform_update,
    je_update_func_t _late_update,
    je_update_func_t _commit_update,
    je_update_func_t _graphic_update);

////////////////////// ToWoo //////////////////////
/*
je_towoo_api_update [基本接口]
根据类型信息重新生成 towoo 的接口脚本
请参见：
    je_type_info_t
*/
JE_API void je_towoo_api_update();

/*
je_towoo_system_register [基本接口]
注册一个 Woolang 脚本系统，返回系统对应的类型信息
    * script_path 为系统脚本路径；读取或注册失败时返回 nullptr
请参见：
    je_towoo_system_unregister
    je_towoo_api_update
*/
JE_API /* OPTIONAL: If registration failed */ const je_type_info_t* je_towoo_system_register(
    const char* system_name,
    const char* script_path);

/*
je_towoo_system_unregister [基本接口]
注销一个之前注册的 Woolang 脚本系统
    * tinfo 为 nullptr 时不做任何事
请参见：
    je_towoo_system_register
*/
JE_API void je_towoo_system_unregister(/* OPTIONAL */ const je_type_info_t* tinfo);
/////////////////////////// JUID /////////////////////////////////

/*
je_uid_generate [基本接口]
生成一个uuid
请参见：
    jeecs::typing::uid_t
*/
JE_API void je_uid_generate(je_uuid_t* out_uid);
typedef je_type_hash_t je_TypeHash;
typedef je_type_id_t je_TypeId;
typedef je_uuid_t je_Uuid;
typedef je_typing_class_t je_TypingClass;
typedef je_type_info_t je_TypeInfo;
typedef je_construct_func_t je_ConstructFunc;
typedef je_destruct_func_t je_DestructFunc;
typedef je_copy_construct_func_t je_CopyConstructFunc;
typedef je_move_construct_func_t je_MoveConstructFunc;
typedef je_on_enable_or_disable_func_t je_OnEnableOrDisableFunc;
typedef je_update_func_t je_UpdateFunc;
typedef je_parse_c2w_func_t je_ParseC2WFunc;
typedef je_parse_w2c_func_t je_ParseW2CFunc;
typedef je_member_info_t je_MemberInfo;
typedef je_typeinfo_member_t je_TypeinfoMember;
typedef je_typeinfo_script_parser_t je_TypeinfoScriptParser;
typedef je_typeinfo_system_updater_t je_TypeinfoSystemUpdater;

JE_FORCE_CAPI_END
#ifdef __cplusplus
namespace jeecs
{
// game_system 完整定义见 jeecs/game.hpp；注册层模板体内以 is_base_of<game_system, T> 区分 JE_SYSTEM / JE_COMPONENT，
// 实例化点所在编译单元均已包含 game.hpp，此处前置声明足够。
    /*
    jeecs::typing [命名空间]
    此处定义引擎使用的常用类型和类型相关的常量值
    */
    namespace typing
    {
        /*
        je_type_hash_t [类型别名]
        用于储存哈希值结果的类型
        */

        /*
        je_type_id_t [类型别名]
        用于储存引擎的类型工厂管理的类型ID，规定的无效值是 JE_INVALID_TYPE_ID
            请参见：
            JE_INVALID_TYPE_ID
        */

        /*
        JE_INVALID_TYPE_ID [常量]
        je_type_id_t 类型的无效值
        请参见：
            je_type_id_t
        */
        constexpr je_type_id_t INVALID_TYPE_ID = SIZE_MAX;

        using module_entry_t = void (*)(woort_Dylib*);
        using module_leave_t = void (*)(void);


        /*
        jeecs::typing::uuid [类型]
        UUID，一般作为全局唯一标识符使用
        请参见：
            jeecs::typing::uid_t
        */
        struct uuid : public je_uuid_t
        {
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
            void JEParseFromScriptType(woort_value v)
            {
                unsigned long long aa, bb;
                ((void)sscanf(woort_string(v), "%llX-%llX", &aa, &bb));
                a = (uint64_t)aa;
                b = (uint64_t)bb;
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_set_string_fmt(v, "%016llX-%016llX",
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
        je_timestamp_ms_t [类型别名]
        用于储存以毫秒为单位的时间戳的类型别名
        请参见：
            je_clock_time_stamp
        */

        /*
        je_debug_eid_t [类型别名]
        实体的调试 ID，对于任何实体，确保在引擎的生命周期内唯一
        仅编辑器环境下有效。
        ----------------------------------
        * 0 是无效值
        ----------------------------------
        * 正计划废弃，待编辑器重构计划推进。
        */

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
                    std::is_reference_v<T>
                    || std::is_pointer_v<T>
                    || std::is_const_v<T>
                    || std::is_volatile_v<T>)
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

        /*
        jeecs::typing::pack_index_v<Target, Ts...> [编译期常量]
        在类型序列 Ts... 中查找 Target 首次出现的位置；若未找到则为 SIZE_MAX。
        替代旧的 _const_type_index 递归实现，编译期展开更友好。
        */
        namespace _detail
        {
            template <typename Target, size_t I, typename... Ts>
            struct _pack_index_helper;

            template <typename Target, size_t I, typename First, typename... Rest>
            struct _pack_index_helper<Target, I, First, Rest...>
            {
                static constexpr size_t value =
                    std::is_same_v<Target, First>
                    ? I
                    : _pack_index_helper<Target, I + 1, Rest...>::value;
            };

            template <typename Target, size_t I>
            struct _pack_index_helper<Target, I>
            {
                static constexpr size_t value = SIZE_MAX;
            };
        }
        template <typename Target, typename... Ts>
        inline constexpr size_t pack_index_v = _detail::_pack_index_helper<Target, 0, Ts...>::value;

        class type_unregister_guard;
    }
    namespace typing
    {
        namespace traits
        {
            template<typename T>
            concept has_JERefRegsiter = requires(type_unregister_guard * guard)
            {
                { T::JERefRegsiter(guard) } -> std::same_as<void>;
            };
            template<typename T>
            concept has_JEScriptTypeInterface = requires(
                T t, const T ct, woort_value val)
            {
                { T::JEScriptTypeName() } -> std::same_as<const char*>;
                { T::JEScriptTypeDeclare() } -> std::same_as<const char*>;
                { t.JEParseFromScriptType(val) } -> std::same_as<void>;
                { ct.JEParseToScriptType(val) } -> std::same_as<void>;
            };
            template <typename U>
            concept has_pointer_typeinfo_constructor_function = requires(void* ptr, const je_type_info_t * tinfo)
            {
                new U(ptr, tinfo);
            };
            template <typename U>
            concept has_pointer_constructor_function = requires(void* ptr)
            {
                new U(ptr);
            };
            template <typename U>
            concept has_game_system_typeinfo_constructor_function = requires(je_game_world_t * ptr, const je_type_info_t * tinfo)
            {
                new U(ptr, tinfo);
            };
            template <typename U>
            concept has_game_system_constructor_function = requires(je_game_world_t * ptr)
            {
                new U(ptr);
            };
            template <typename U>
            concept has_typeinfo_constructor_function = requires(const je_type_info_t * tinfo)
            {
                new U(tinfo);
            };
            template <typename U>
            concept has_default_constructor_function = requires()
            {
                new U();
            };
            template <typename T>
            concept is_reference_or_pointer = std::is_reference_v<T> || std::is_pointer_v<T>;

            namespace system
            {
                template<typename T>
                concept has_OnEnable = requires(T t)
                {
                    { t.OnEnable() } -> std::same_as<void>;
                };
                template<typename T>
                concept has_OnDisable = requires(T t)
                {
                    { t.OnDisable() } -> std::same_as<void>;
                };

                template<typename T>
                concept has_PreUpdate = requires(T t)
                {
                    { t.PreUpdate() } -> std::same_as<void>;
                };
                template<typename T>
                concept has_StateUpdate = requires(T t)
                {
                    { t.StateUpdate() } -> std::same_as<void>;
                };
                template<typename T>
                concept has_Update = requires(T t)
                {
                    { t.Update() } -> std::same_as<void>;
                };
                template<typename T>
                concept has_PhysicsUpdate = requires(T t)
                {
                    { t.PhysicsUpdate() } -> std::same_as<void>;
                };
                template<typename T>
                concept has_TransformUpdate = requires(T t)
                {
                    { t.TransformUpdate() } -> std::same_as<void>;
                };
                template<typename T>
                concept has_LateUpdate = requires(T t)
                {
                    { t.LateUpdate() } -> std::same_as<void>;
                };
                template<typename T>
                concept has_CommitUpdate = requires(T t)
                {
                    { t.CommitUpdate() } -> std::same_as<void>;
                };
                template<typename T>
                concept has_GraphicUpdate = requires(T t)
                {
                    { t.GraphicUpdate() } -> std::same_as<void>;
                };
            }
        }

        template <typename T>
        struct default_functions
        {
            static void constructor(void* _ptr, void* arg_ptr, const je_type_info_t* tinfo)
            {
                if constexpr (traits::has_pointer_typeinfo_constructor_function<T>)
                    new (_ptr) T(arg_ptr, tinfo);
                else if constexpr (traits::has_pointer_constructor_function<T>)
                    new (_ptr) T(arg_ptr);
                else if constexpr (traits::has_game_system_typeinfo_constructor_function<T>)
                    new (_ptr) T(static_cast<je_game_world_t*>(arg_ptr), tinfo);
                else if constexpr (traits::has_game_system_constructor_function<T>)
                    new (_ptr) T(static_cast<je_game_world_t*>(arg_ptr));
                else if constexpr (traits::has_typeinfo_constructor_function<T>)
                    new (_ptr) T(tinfo);
                else
                {
                    static_assert(traits::has_default_constructor_function<T>);
                    new (_ptr) T{};
                }
            }
            static void destructor(void* _ptr)
            {
                static_cast<T*>(_ptr)->~T();
            }
            static void copier(void* _ptr, const void* _be_copy_ptr)
            {
                if constexpr (std::is_copy_constructible_v<T>)
                    new (_ptr) T(*static_cast<const T*>(_be_copy_ptr));
                else
                    debug::logerr("This type: '%s' is not copy-constructible but you try to do it.", typeid(T).name());
            }
            static void mover(void* _ptr, void* _be_moved_ptr)
            {
                if constexpr (std::is_move_constructible_v<T>)
                    new (_ptr) T(std::move(*static_cast<T*>(_be_moved_ptr)));
                else
                    debug::logerr("This type: '%s' is not move-constructible but you try to do it.", typeid(T).name());
            }
            static void on_enable(void* _ptr)
            {
                if constexpr (traits::system::has_OnEnable<T>)
                    static_cast<T*>(_ptr)->OnEnable();
            }
            static void on_disable(void* _ptr)
            {
                if constexpr (traits::system::has_OnDisable<T>)
                    static_cast<T*>(_ptr)->OnDisable();
            }
            static void pre_update(void* _ptr)
            {
                if constexpr (traits::system::has_PreUpdate<T>)
                    static_cast<T*>(_ptr)->PreUpdate();
            }
            static void state_update(void* _ptr)
            {
                if constexpr (traits::system::has_StateUpdate<T>)
                    static_cast<T*>(_ptr)->StateUpdate();
            }
            static void update(void* _ptr)
            {
                if constexpr (traits::system::has_Update<T>)
                    static_cast<T*>(_ptr)->Update();
            }
            static void physics_update(void* _ptr)
            {
                if constexpr (traits::system::has_PhysicsUpdate<T>)
                    static_cast<T*>(_ptr)->PhysicsUpdate();
            }
            static void transform_update(void* _ptr)
            {
                if constexpr (traits::system::has_TransformUpdate<T>)
                    static_cast<T*>(_ptr)->TransformUpdate();
            }
            static void late_update(void* _ptr)
            {
                if constexpr (traits::system::has_LateUpdate<T>)
                    static_cast<T*>(_ptr)->LateUpdate();
            }
            static void commit_update(void* _ptr)
            {
                if constexpr (traits::system::has_CommitUpdate<T>)
                    static_cast<T*>(_ptr)->CommitUpdate();
            }
            static void graphic_update(void* _ptr)
            {
                if constexpr (traits::system::has_GraphicUpdate<T>)
                    static_cast<T*>(_ptr)->GraphicUpdate();
            }

            static void parse_from_script_type(void* _ptr, woort_value val)
            {
                if constexpr (traits::has_JEScriptTypeInterface<T>)
                    static_cast<T*>(_ptr)->JEParseFromScriptType(val);
            }
            static void parse_to_script_type(const void* _ptr, woort_value val)
            {
                if constexpr (traits::has_JEScriptTypeInterface<T>)
                    static_cast<const T*>(_ptr)->JEParseToScriptType(val);
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

        /* je_typeinfo_member_t / je_typeinfo_script_parser_t / je_typeinfo_system_updater_t
           已迁移为 C ABI 类型 je_typeinfo_member_t / je_typeinfo_script_parser_t /
           je_typeinfo_system_updater_t（见文件顶部 C ABI 段），C++ 直接使用。 */
        class type_unregister_guard
        {

            using id_typeinfo_map_t = std::unordered_map<
                je_type_id_t,
                const je_type_info_t*>;
            using registered_type_hash_map_t = std::unordered_map<
                je_type_hash_t,
                je_type_id_t>;

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
            bool _register_or_get_local_type_info(const char* _typename, const je_type_info_t** out_typeinfo)
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

                je_typing_class_t current_type =
                    is_basic_type
                    ? je_typing_class_t::JE_BASIC_TYPE
                    : (std::is_base_of_v<game_system, T>
                        ? je_typing_class_t::JE_SYSTEM
                        : je_typing_class_t::JE_COMPONENT);

                // store to list for unregister
                auto* local_type_info = je_typing_register(
                    _typename,
                    basic::type_hash<T>(),
                    sizeof(T),
                    alignof(T),
                    current_type,
                    typing::default_functions<T>::constructor,
                    typing::default_functions<T>::destructor,
                    typing::default_functions<T>::copier,
                    typing::default_functions<T>::mover);

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
            const je_type_info_t* get_local_type_info(je_type_id_t id) const
            {
                std::lock_guard g1(_m_mx);
                return _m_self_registed_id_typeinfo.at(id);
            }
        };

        /*
        jeecs::typing 自由函数（原 je_type_info_t 上的便捷方法）
        这些函数操作 const je_type_info_t*；je_type_info_t 的平凡数据布局见文件顶部 C ABI 段的 je_type_info_t。
        */
        template <typename T>
        inline const je_type_info_t* of()
        {
            return je_typing_info_get_by_hash(typeid(T).hash_code());
        }
        inline const je_type_info_t* of(je_type_id_t _tid)
        {
            return je_typing_info_get_by_id(_tid);
        }
        inline const je_type_info_t* of(const char* name)
        {
            return je_typing_info_get_by_name(name);
        }

        template <typename T>
        inline je_type_id_t id()
        {
            return of<T>()->m_id;
        }
        inline je_type_id_t id(const je_type_info_t* _tinfo)
        {
            return _tinfo->m_id;
        }
        inline je_type_id_t id(const char* name)
        {
            return of(name)->m_id;
        }

        template <typename T>
        inline const je_type_info_t* register_type(
            type_unregister_guard* guard, const char* _typename)
        {
            const je_type_info_t* local_type = nullptr;
            if (guard->_register_or_get_local_type_info<T>(_typename, &local_type))
            {
                if constexpr (traits::has_JERefRegsiter<T>)
                {
                    T::JERefRegsiter(guard);
                }
            }

            if (local_type->m_type_class == je_typing_class_t::JE_SYSTEM)
            {
                je_typing_system_updater_register(
                    local_type,
                    default_functions<T>::on_enable,
                    default_functions<T>::on_disable,
                    default_functions<T>::pre_update,
                    default_functions<T>::state_update,
                    default_functions<T>::update,
                    default_functions<T>::physics_update,
                    default_functions<T>::transform_update,
                    default_functions<T>::late_update,
                    default_functions<T>::commit_update,
                    default_functions<T>::graphic_update);
            }

            if constexpr (traits::has_JEScriptTypeInterface<T>)
            {
                je_typing_script_parser_register(local_type,
                    default_functions<T>::parse_to_script_type,
                    default_functions<T>::parse_from_script_type,
                    T::JEScriptTypeName(),
                    T::JEScriptTypeDeclare());
            }

            return local_type;
        }

        inline void construct(const je_type_info_t* t, void* addr, void* arg)
        {
            t->m_constructor(addr, arg, t);
        }
        inline void destruct(const je_type_info_t* t, void* addr)
        {
            t->m_destructor(addr);
        }
        inline void copy(const je_type_info_t* t, void* dst_addr, const void* src_addr)
        {
            t->m_copier(dst_addr, src_addr);
        }
        inline void move(const je_type_info_t* t, void* dst_addr, void* src_addr)
        {
            t->m_mover(dst_addr, src_addr);
        }

        inline bool is_system(const je_type_info_t* t) noexcept
        {
            return t->m_type_class == je_typing_class_t::JE_SYSTEM;
        }

        inline bool is_component(const je_type_info_t* t) noexcept
        {
            return t->m_type_class == je_typing_class_t::JE_COMPONENT;
        }

        inline const je_member_info_t* find_member_by_name(const je_type_info_t* t, const char* name) noexcept
        {
            if (t->m_member_types == nullptr)
            {
                if (t->m_next != nullptr)
                    return find_member_by_name(t->m_next, name);
            }
            else
            {
                auto* member_info_ptr = t->m_member_types->m_members;
                while (member_info_ptr != nullptr)
                {
                    if (strcmp(member_info_ptr->m_member_name, name) == 0)
                        return member_info_ptr;

                    member_info_ptr = member_info_ptr->m_next_member;
                }
            }
            jeecs::debug::logerr("Failed to find member named: '%s' in '%s'.", name, t->m_typename);
            return nullptr;
        }
        inline const je_typeinfo_script_parser_t* get_script_parser(const je_type_info_t* t)
        {
            if (t->m_script_parsers == nullptr && t->m_next != nullptr)
                return get_script_parser(t->m_next);

            return t->m_script_parsers;
        }

        template <typename ClassT, typename MemberT>
        inline void register_member(
            jeecs::typing::type_unregister_guard* guard,
            ptrdiff_t member_offset,
            const char* membname)
        {
            const je_type_info_t* membt = jeecs::typing::register_type<MemberT>(guard, nullptr);
            assert(membt->m_type_class == je_typing_class_t::JE_BASIC_TYPE);

            je_typing_member_register(
                guard->get_local_type_info(jeecs::typing::id<ClassT>()),
                membt,
                membname,
                nullptr,
                WOORT_IGNORE,
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
            void (*c2w)(const T*, woort_value),
            void (*w2c)(T*, woort_value),
            const std::string& woolang_typename,
            const std::string& woolang_typedecl)
        {
            const je_type_info_t* local_typeinfo = nullptr;
            guard->_register_or_get_local_type_info<T>(nullptr, &local_typeinfo);

            je_typing_script_parser_register(
                local_typeinfo,
                reinterpret_cast<je_parse_c2w_func_t>(c2w),
                reinterpret_cast<je_parse_w2c_func_t>(w2c),
                woolang_typename.c_str(),
                woolang_typedecl.c_str());
        }
    }
}
#endif // __cplusplus
#ifdef __cplusplus
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
        inline constexpr size_t operator()(
            const jeecs::typing::uid_t& a, const jeecs::typing::uid_t& b) const noexcept
        {
            return a.a == b.a && a.b == b.b;
        }
    };
}
#endif // __cplusplus
