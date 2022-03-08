#pragma once

#ifndef __cplusplus
#error jeecs.h only support for c++
#else

#include "rs.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>

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

#ifdef __cpp_lib_execution
#include <execution>
#endif

#define RS_FORCE_CAPI extern "C"{
#define RS_FORCE_CAPI_END }

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

namespace jeecs
{
    namespace typing
    {
        using typehash_t = size_t;
        using typeid_t = size_t;

        constexpr typeid_t INVALID_TYPE_ID = SIZE_MAX;
        constexpr size_t ALLIGN_BASE = alignof(std::max_align_t);

        struct type_info;

        using construct_func_t = void(*)(void*, void*);
        using destruct_func_t = void(*)(void*);
        using copy_func_t = void(*)(void*, const void*);
        using move_func_t = void(*)(void*, void*);
        using to_string_func_t = const char* (*)(void*);
        using parse_func_t = void(*)(void*, const char*);

        using entity_id_in_chunk_t = size_t;
        using version_t = size_t;

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
                    uint32_t x;
                    uint16_t y; // Time stamp
                    uint16_t z;

                    uint16_t w; // Inc L16
                    uint16_t u; // Inc H16
                    uint32_t v; // Random
                };
            };
        };

        using uid_t = uuid;
        using ms_stamp_t = uint64_t;
    }

    struct game_system_function;

    class game_system;

    class game_world;

    struct game_entity
    {
        enum class entity_stat
        {
            UNAVAILABLE,    // Entity is destroied or just not ready,
            READY,          // Entity is OK, and just work as normal.
        };

        void* _m_in_chunk;
        jeecs::typing::entity_id_in_chunk_t   _m_id;
        jeecs::typing::version_t              _m_version;

        template<typename T>
        inline T* get_component()noexcept;
    };
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

RS_FORCE_CAPI
JE_API void* je_mem_alloc(size_t sz);
JE_API void je_mem_free(void* ptr);

#define JE_LOG_NORMAL 0
#define JE_LOG_INFO 1
#define JE_LOG_WARNING 2
#define JE_LOG_ERROR 3
#define JE_LOG_FATAL 4
JE_API void je_log(int level, const char* format, ...);

// You should promise: different type should have different name.
JE_API bool je_typing_find_or_register(
    jeecs::typing::typeid_t* out_typeid,
    const char* _name,
    jeecs::typing::typehash_t _hash,
    size_t                    _size,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t  _destructor,
    jeecs::typing::copy_func_t      _copier,
    jeecs::typing::move_func_t      _mover,
    bool                            _is_system);

JE_API const jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id);

JE_API const jeecs::typing::type_info* je_typing_get_info_by_name(
    const char* type_name);

JE_API void je_typing_unregister(
    jeecs::typing::typeid_t _id);

////////////////////// ARCH //////////////////////

JE_API void* je_arch_get_chunk(void* archtype);

JE_API void* je_arch_next_chunk(void* chunk);

JE_API const void* je_arch_entity_meta_addr_in_chunk(void* chunk);

JE_API size_t je_arch_entity_meta_size();

JE_API size_t je_arch_entity_meta_state_offset();

JE_API size_t je_arch_entity_meta_version_offset();

////////////////////// ECS //////////////////////

JE_API void* je_ecs_universe_create();

JE_API void je_ecs_universe_destroy(void* universe);

JE_API void* je_ecs_universe_instance_system(
    void* universe,
    void* aim_world,
    const jeecs::typing::type_info* system_type
);

JE_API void* je_ecs_world_in_universe(void* world);

JE_API void* je_ecs_world_create(void* in_universe);

JE_API void je_ecs_world_destroy(void* world);

JE_API void je_ecs_world_register_system_func(void* world, jeecs::game_system_function* game_system_function);

JE_API void je_ecs_world_unregister_system_func(void* world, jeecs::game_system_function* game_system_function);

JE_API bool je_ecs_world_update(void* world);

JE_API void je_ecs_world_create_entity_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    jeecs::typing::typeid_t* component_ids);

JE_API void je_ecs_world_destroy_entity(
    void* world,
    const jeecs::game_entity* entity);

JE_API void* je_ecs_world_entity_add_component(
    void* world,
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info);

JE_API void je_ecs_world_entity_remove_component(
    void* world,
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info);

/////////////////////////// Time&Sleep /////////////////////////////////

JE_API double je_clock_time();

JE_API jeecs::typing::ms_stamp_t je_clock_time_stamp();

JE_API void je_clock_sleep_until(double time);

JE_API void je_clock_sleep_for(double time);

JE_API void je_clock_suppress_sleep(double sup_stax);

/////////////////////////// JUID /////////////////////////////////

JE_API jeecs::typing::uid_t je_uid_generate();

/////////////////////////// CORE /////////////////////////////////

JE_API void jeecs_entry_register_core_systems();

/////////////////////////// GRAPHIC //////////////////////////////
// Here to store low-level-graphic-api.

struct jegl_interface_config
{
    size_t m_windows_width, m_windows_height;
    size_t m_resolution_x, m_resolution_y;

    size_t m_fps;
};

struct jegl_thread
{
    std::thread* _m_thread;
    void* _m_interface_handle;
};

JE_API jegl_thread* jegl_start_graphic_thread(jegl_interface_config config);

JE_API void jegl_terminate_graphic_thread(jegl_thread* thread);

RS_FORCE_CAPI_END

namespace jeecs
{
#define JECS_DISABLE_MOVE_AND_COPY(TYPE) \
    TYPE(const TYPE &)  = delete;\
    TYPE(TYPE &&)       = delete;\
    TYPE& operator = (const TYPE &) = delete;\
    TYPE& operator = (TYPE &&) = delete;

    namespace debug
    {
        template<typename ... ArgTs>
        inline void log(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_NORMAL, format, args...);
        }
        template<typename ... ArgTs>
        inline void log_info(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_INFO, format, args...);
        }
        template<typename ... ArgTs>
        inline void log_warn(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_WARNING, format, args...);
        }
        template<typename ... ArgTs>
        inline void log_error(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_ERROR, format, args...);
        }
        template<typename ... ArgTs>
        inline void log_fatal(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_FATAL, format, args...);
        }
    }

    namespace basic
    {
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
        };

        template<typename T>
        struct default_functions
        {
            template<typename U>
            struct has_default_constructor
            {
                template<typename W>
                using _true_type = std::true_type;

                template<typename V>
                static auto _tester(int)->_true_type<decltype(new T())>;
                template<typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };

            static void constructor(void* _ptr, void* arg_ptr)
            {
                if constexpr (has_default_constructor<T>::value)
                    new(_ptr)T;
                else
                    new(_ptr)T(arg_ptr);
            }
            static void destructor(void* _ptr)
            {
                ((T*)_ptr)->~T();
            }
            static void copier(void* _ptr, const void* _be_copy_ptr)
            {
                new(_ptr)T(*(const T*)_be_copy_ptr);
            }
            static void mover(void* _ptr, void* _be_moved_ptr)
            {
                new(_ptr)T(std::move(*(T*)_be_moved_ptr));
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
    }

    namespace typing
    {
        struct type_info
        {
            typeid_t    m_id;

            typehash_t  m_hash;
            const char* m_typename;   // will be free by je_typing_unregister
            size_t      m_size;
            size_t      m_chunk_size; // calc by je_typing_find_or_register

            construct_func_t    m_constructor;
            destruct_func_t     m_destructor;
            copy_func_t         m_copier;
            move_func_t         m_mover;

            bool                m_is_system;

        private:
            inline static std::atomic_bool      _m_shutdown_flag = false;

            class _type_unregister_guard
            {
                friend struct type_info;
                _type_unregister_guard() = default;
                std::mutex            _m_self_registed_typeid_mx;
                std::vector<typeid_t> _m_self_registed_typeid;

            public:
                ~_type_unregister_guard()
                {
                    std::lock_guard g1(_m_self_registed_typeid_mx);
                    for (typeid_t typeindex : _m_self_registed_typeid)
                        je_typing_unregister(typeindex);
                    _m_self_registed_typeid.clear();
                    _m_shutdown_flag = true;
                }

                template<typename T>
                typeid_t _register_or_get_type_id(const char* _typename)
                {
                    typeid_t id = INVALID_TYPE_ID;
                    if (je_typing_find_or_register(
                        &id,
                        _typename,
                        basic::type_hash<T>(),
                        sizeof(T),
                        basic::default_functions<T>::constructor,
                        basic::default_functions<T>::destructor,
                        basic::default_functions<T>::copier,
                        basic::default_functions<T>::mover,
                        std::is_base_of<game_system, T>::value))
                    {
                        // store to list for unregister
                        std::lock_guard g1(_m_self_registed_typeid_mx);
                        _m_self_registed_typeid.push_back(id);
                    }
                    return id;
                }
            };
            inline static _type_unregister_guard _type_guard;

        public:
            static void unregister_all_type_in_shutdown()
            {
                _type_guard.~_type_unregister_guard();
            }
            template<typename T>
            inline static typeid_t id(const char* _typename = typeid(T).name())
            {
                assert(!_m_shutdown_flag);
                static typeid_t registed_typeid = _type_guard._register_or_get_type_id<T>(_typename);
                return registed_typeid;
            }
            template<typename T>
            inline static const type_info* of(const char* _typename = typeid(T).name())
            {
                assert(!_m_shutdown_flag);
                static typeid_t registed_typeid = id<T>(_typename);
                static const type_info* registed_typeinfo = je_typing_get_info_by_id(registed_typeid);

                return registed_typeinfo;
            }
            inline static const type_info* of(typeid_t _tid)
            {
                assert(!_m_shutdown_flag);
                return je_typing_get_info_by_id(_tid);
            }
            inline static const type_info* of(const char* name)
            {
                assert(!_m_shutdown_flag);
                return je_typing_get_info_by_name(name);
            }

            void construct(void* addr, void* arg = nullptr) const
            {
                m_constructor(addr, arg);
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
        };
    }

    struct game_system_function
    {
        enum dependence_type : uint8_t
        {
            // Operation
            READ_FROM_LAST_FRAME,
            WRITE,
            READ_AFTER_WRITE,

            // Constraint
            EXCEPT,
            CONTAIN,
            ANY,
        };

        using system_function_pak_t = std::function<void(const game_system_function*)>;
        using destructor_t = void(*)(game_system_function*);
        using invoker_t = void(*)(const game_system_function*, system_function_pak_t*);

        struct arch_index_info
        {
            void* m_archtype;
            size_t m_entity_count_per_arch_chunk;

            size_t* m_component_mem_begin_offsets;
            size_t* m_component_sizes;

            template<typename T>
            inline T get_component_accessor(void* chunk_addr, size_t eid, size_t cid) const noexcept
            {
                static_assert(sizeof(T) == sizeof(uint8_t*));

                uint8_t* ptr = (uint8_t*)chunk_addr
                    + m_component_mem_begin_offsets[cid]
                    + m_component_sizes[cid] * eid;

                return *(T*)(&ptr);
            }

            //template<typename T, typename IndexerT>
            //inline T get_component_accessor_expander(void* chunk_addr, size_t eid, IndexerT & indexer) const noexcept
            //{
            //    return get_component_accessor<T>(chunk_addr, eid,  indexer.template index_of<T>());
            //}

            inline static jeecs::game_entity::entity_stat get_entity_state(const void* entity_meta, size_t eid)
            {
                static const size_t meta_size = je_arch_entity_meta_size();
                static const size_t meta_entity_stat_offset = je_arch_entity_meta_state_offset();

                uint8_t* _addr = ((uint8_t*)entity_meta) + eid * meta_size + meta_entity_stat_offset;
                return *(const jeecs::game_entity::entity_stat*)_addr;
            }

            inline static jeecs::typing::version_t get_entity_version(const void* entity_meta, size_t eid)
            {
                static const size_t meta_size = je_arch_entity_meta_size();
                static const size_t meta_entity_version_offset = je_arch_entity_meta_version_offset();

                uint8_t* _addr = ((uint8_t*)entity_meta) + eid * meta_size + meta_entity_version_offset;
                return *(const jeecs::typing::version_t*)_addr;
            }
        };

        struct typeid_dependence_pair
        {
            typing::typeid_t m_tid;
            dependence_type  m_depend;
        };

        arch_index_info* m_archs;
        size_t m_arch_count;
        typeid_dependence_pair* m_dependence;
        size_t m_dependence_count;
        system_function_pak_t* m_function;
        invoker_t m_invoker;
        size_t m_rw_component_count;

    private:
        destructor_t _m_destructor;

        static void _updater(const game_system_function* _this, system_function_pak_t* pak)
        {
            (*pak)(_this);
        }

        static void _destructor(game_system_function* _this)
        {
            _this->~game_system_function();
            je_mem_free(_this);
        }

        ~game_system_function()
        {
            basic::destroy_free(m_function);

            if (m_dependence)
            {
                je_mem_free(m_dependence);
            }
        }
        game_system_function(const system_function_pak_t& sys_function, size_t rw_func_count)
            : m_archs(nullptr)
            , m_arch_count(0)
            , m_function(basic::create_new<system_function_pak_t>(sys_function))
            , _m_destructor(_destructor)
            , m_invoker(_updater)
            , m_dependence(nullptr)
            , m_dependence_count(0)
            , m_rw_component_count(rw_func_count)
        {

        }
    public:
        void set_depends(const std::vector<typeid_dependence_pair>& depends)
        {
            assert(nullptr == m_dependence && 0 == m_dependence_count);
            if (!depends.empty())
            {
                m_dependence_count = depends.size();
                m_dependence = (typeid_dependence_pair*)je_mem_alloc(sizeof(typeid_dependence_pair) * m_dependence_count);
                memcpy(m_dependence, depends.data(), sizeof(typeid_dependence_pair) * m_dependence_count);
            }
        }

        void update() const
        {
            m_invoker(this, m_function);
        }

    public:
        static game_system_function* create(const system_function_pak_t& sys_function, size_t rw_func_count)
        {
            return new(je_mem_alloc(sizeof(game_system_function)))game_system_function(sys_function, rw_func_count);
        }
        static void destory(game_system_function* _this)
        {
            _this->_m_destructor(_this);
        }
    };

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
        jeecs::game_system_function* register_system_func_to_world(jeecs::game_system_function* sys_func)
        {
            je_ecs_world_register_system_func(_m_ecs_world_addr, sys_func);
            return sys_func;
        }
    public:
        inline void* handle()const noexcept
        {
            return _m_ecs_world_addr;
        }

        inline void* add_system(const typing::type_info * sys_type)
        {
            assert(sys_type->m_is_system);
            return je_ecs_universe_instance_system(
                je_ecs_world_in_universe(handle()),
                handle(),
                sys_type
            );
        }

        template<typename T>
        inline T* add_system()
        {
            return (T*)add_system(typing::type_info::of<T>());
        }

        template<typename ... CompTs>
        inline game_entity add_entity()
        {
            typing::typeid_t component_ids[] = {
                typing::type_info::id<CompTs>()...,
                typing::INVALID_TYPE_ID
            };
            game_entity gentity;
            je_ecs_world_create_entity_with_components(
                handle(), &gentity, component_ids);

            return gentity;
        }
        inline operator bool()const noexcept
        {
            return handle() != nullptr;
        }
    };

    class game_system
    {
        struct accessor_base {};

        struct read_last_frame_base :accessor_base {};
        struct read_updated_base :accessor_base {};
        struct write_base :accessor_base {};

    public:

        template<typename T>
        struct read : read_last_frame_base {
        public:
            void* _m_component_addr;
        public:
            inline const T* operator ->() const noexcept { return (const T*)_m_component_addr; }
            inline const T& operator * () const noexcept { return *(const T*)_m_component_addr; }
            inline const T* operator &() const noexcept { return (const T*)_m_component_addr; };
            inline operator const T* () const noexcept { return (const T*)_m_component_addr; };
        };
        template<typename T>
        struct read_updated : read_updated_base {
        public:
            void* _m_component_addr;
        public:
            inline const T* operator ->() const noexcept { return (const T*)_m_component_addr; }
            inline const T& operator * () const noexcept { return *(const T*)_m_component_addr; }
            inline const T* operator &() const noexcept { return (const T*)_m_component_addr; };
            inline operator const T* () const noexcept { return (const T*)_m_component_addr; };
        };
        template<typename T>
        struct write : write_base {
        public:
            void* _m_component_addr;
        public:
            inline T* operator ->() const noexcept { return (T*)_m_component_addr; }
            inline T& operator * () const noexcept { return *(T*)_m_component_addr; }
            inline T* operator &() const noexcept { return (T*)_m_component_addr; };
            inline operator T* () const noexcept { return (T*)_m_component_addr; };
        };

    private:
        game_world _m_game_world;

    public:
        game_system(game_world world)
            : _m_game_world(world)
        {

        }

        const game_world* get_world() const noexcept
        {
            return &_m_game_world;
        }

    public:
        template<typename T>
        static constexpr jeecs::game_system_function::dependence_type depend_type()
        {
            if constexpr (std::is_pointer<T>::value || std::is_base_of<accessor_base, T>::value)
            {
                if constexpr (std::is_pointer<T>::value)
                {
                    if constexpr (std::is_const<typename std::remove_pointer<T>::type>::value)
                        return jeecs::game_system_function::dependence_type::READ_AFTER_WRITE;
                    else
                        return jeecs::game_system_function::dependence_type::WRITE;
                }
                else
                {
                    if constexpr (std::is_base_of<read_last_frame_base, T>::value)
                        return jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME;
                    else if constexpr (std::is_base_of<write_base, T>::value)
                        return jeecs::game_system_function::dependence_type::WRITE;
                    else /* if constexpr (std::is_base_of<read_updated_base, T>::value) */
                        return jeecs::game_system_function::dependence_type::READ_AFTER_WRITE;
                }
            }
            else
            {
                static_assert(std::is_pointer<T>::value || std::is_base_of<accessor_base, T>::value,
                    "Unknown accessor type: should be pointer/read/write/read_newest.");
            }
        }

        template<typename T>
        struct origin_component
        {

        };

        template<typename T>
        struct origin_component<T*>
        {
            using type = typename std::remove_cv<T>::type;
        };

        template<typename T>
        struct origin_component<read<T>>
        {
            using type = typename std::remove_cv<T>::type;
        };

        template<typename T>
        struct origin_component<write<T>>
        {
            using type = typename std::remove_cv<T>::type;
        };

        template<typename T>
        struct origin_component<read_updated<T>>
        {
            using type = typename std::remove_cv<T>::type;
        };

        template<typename ... ArgTs>
        struct is_need_game_entity
        {
            template<typename T, typename ... Ts>
            struct is_game_entity
            {
                static constexpr bool value = std::is_same<T, jeecs::game_entity>::value;
            };

            static constexpr bool value =
                0 != sizeof...(ArgTs) && is_game_entity<ArgTs...>::value;
        };

        struct requirement
        {
            game_system_function::dependence_type m_depend;
            typing::typeid_t m_required_id;
        };

        template<typename T>
        inline static constexpr requirement except()
        {
            return requirement{ game_system_function::dependence_type::EXCEPT, jeecs::typing::type_info::id<T>() };
        }

        template<typename T>
        inline static constexpr requirement any_of()
        {
            return requirement{ game_system_function::dependence_type::ANY, jeecs::typing::type_info::id<T>() };
        }

        inline static requirement system_read(const void* offset)
        {
            return requirement{ game_system_function::dependence_type::READ_FROM_LAST_FRAME, reinterpret_cast<typing::typeid_t>(offset) };
        }

        inline static requirement system_write(void* offset)
        {
            return requirement{ game_system_function::dependence_type::WRITE, reinterpret_cast<typing::typeid_t>(offset) };
        }

        inline static requirement system_read_updated(const void* offset)
        {
            return requirement{ game_system_function::dependence_type::READ_AFTER_WRITE, reinterpret_cast<typing::typeid_t>(offset) };
        }

        template<typename T>
        inline static constexpr requirement pre(T val)
        {
            return requirement{ game_system_function::dependence_type::READ_FROM_LAST_FRAME,
                *reinterpret_cast<typing::typeid_t*>(&val) };
        }

        template<typename T>
        inline static constexpr requirement after(T val)
        {
            return requirement{ game_system_function::dependence_type::READ_AFTER_WRITE,
                *reinterpret_cast<typing::typeid_t*>(&val) };
        }

        template<typename T>
        inline static constexpr requirement current(T val)
        {
            return requirement{ game_system_function::dependence_type::WRITE,
                *reinterpret_cast<typing::typeid_t*>(&val) };
        }

        template<typename ReturnT, typename ThisT, typename ... ArgTs>
        inline auto pack_normal_invoker(ReturnT(ThisT::* system_func)(ArgTs ...), const std::vector<requirement>& requirement)
        {
            auto invoker = [this, system_func](const jeecs::game_system_function* sysfunc) {

                if constexpr (0 == sizeof...(ArgTs))
                {
                    ((static_cast<ThisT*>(this))->*system_func)();
                }
                else
                {
                    jeecs::basic::type_index_in_varargs<ArgTs...> tindexer;

                    for (size_t arch_index = 0; arch_index < sysfunc->m_arch_count; ++arch_index)
                    {
                        void* current_chunk = je_arch_get_chunk(sysfunc->m_archs[arch_index].m_archtype);
                        while (current_chunk)
                        {
                            auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(current_chunk);

                            for (size_t entity_index = 0;
                                entity_index < sysfunc->m_archs[arch_index].m_entity_count_per_arch_chunk;
                                entity_index++)
                            {
                                if (jeecs::game_entity::entity_stat::READY
                                    == jeecs::game_system_function::arch_index_info::get_entity_state(entity_meta_addr, entity_index))
                                {
                                    ((static_cast<ThisT*>(this))->*system_func)(

                                        sysfunc->m_archs[arch_index].get_component_accessor<ArgTs>(
                                            current_chunk, entity_index, tindexer.template index_of<ArgTs>()
                                            )...
                                        );
                                }
                            }
                            current_chunk = je_arch_next_chunk(current_chunk);
                        }
                    }
                }
            };

            auto* gsys = jeecs::game_system_function::create(invoker, sizeof...(ArgTs));
            std::vector<jeecs::game_system_function::typeid_dependence_pair> depends
                = { {
                        jeecs::typing::type_info::id<typename origin_component<ArgTs>::type>(),
                        depend_type<ArgTs>(),
                    }... };

            for (auto& req : requirement)
                depends.push_back({ req.m_required_id,req.m_depend });

            depends.push_back({ current(system_func).m_required_id, current(system_func).m_depend });

            gsys->set_depends(depends);

            return gsys;
        }

        template<typename ReturnT, typename ThisT, typename ET, typename ... ArgTs>
        inline auto pack_normal_invoker_with_entity(ReturnT(ThisT::* system_func)(ET, ArgTs ...), const std::vector<requirement>& requirement)
        {
            auto invoker = [this, system_func](const jeecs::game_system_function* sysfunc) {

                jeecs::basic::type_index_in_varargs<ArgTs...> tindexer;

                for (size_t arch_index = 0; arch_index < sysfunc->m_arch_count; ++arch_index)
                {
                    void* current_chunk = je_arch_get_chunk(sysfunc->m_archs[arch_index].m_archtype);
                    while (current_chunk)
                    {
                        auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(current_chunk);

                        for (size_t entity_index = 0;
                            entity_index < sysfunc->m_archs[arch_index].m_entity_count_per_arch_chunk;
                            entity_index++)
                        {
                            if (jeecs::game_entity::entity_stat::READY
                                == jeecs::game_system_function::arch_index_info::get_entity_state(entity_meta_addr, entity_index))
                            {
                                jeecs::game_entity gentity;
                                gentity._m_id = entity_index;
                                gentity._m_in_chunk = current_chunk;
                                gentity._m_version = jeecs::game_system_function::arch_index_info::get_entity_version(entity_meta_addr, entity_index);

                                ((static_cast<ThisT*>(this))->*system_func)(
                                    gentity,
                                    sysfunc->m_archs[arch_index].get_component_accessor<ArgTs>(
                                        current_chunk, entity_index, tindexer.template index_of<ArgTs>()
                                        )...
                                    );
                            }
                        }
                        current_chunk = je_arch_next_chunk(current_chunk);
                    }
                }
            };

            auto* gsys = jeecs::game_system_function::create(invoker, sizeof...(ArgTs));
            std::vector<jeecs::game_system_function::typeid_dependence_pair> depends
                = { {
                        jeecs::typing::type_info::id<origin_component<ArgTs>::type>(),
                        depend_type<ArgTs>(),
                    }... };

            for (auto& req : requirement)
                depends.push_back({ req.m_required_id,req.m_depend });

            depends.push_back({ current(system_func).m_required_id, current(system_func).m_depend });

            gsys->set_depends(depends);

            return gsys;
        }

        template<typename ReturnT, typename ThisT, typename ... ArgTs>
        inline jeecs::game_system_function* register_system_func(ReturnT(ThisT::* sysf)(ArgTs ...), const std::vector<requirement>& requirement = {})
        {
            if (_m_game_world)
            {
                if constexpr (0 != sizeof...(ArgTs))
                {
                    if constexpr (is_need_game_entity<ArgTs...>::value)
                        return _m_game_world.register_system_func_to_world(pack_normal_invoker_with_entity(sysf, requirement));
                    else
                        return _m_game_world.register_system_func_to_world(pack_normal_invoker(sysf, requirement));
                }
                else
                {
                    return _m_game_world.register_system_func_to_world(pack_normal_invoker(sysf, requirement));
                }
            }
            else
            {
                debug::log_warn("Unable to register system function with 'register_system_func' in independence system.");
                return nullptr;
            }
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

        game_world create_world()
        {
            return je_ecs_world_create(_m_universe_addr);
        }

        inline void* add_shared_system(const typing::type_info* typeinfo)
        {
            assert(typeinfo->m_is_system);
            return je_ecs_universe_instance_system(
                handle(),
                nullptr,
                typeinfo
            );
        }

    public:
        static game_universe create_universe()
        {
            return game_universe(je_ecs_universe_create());
        }
        static void destroy_universe(game_universe universe)
        {
            return je_ecs_universe_destroy(universe._m_universe_addr);
        }
    };

    template<typename T>
    inline T* game_entity::get_component() noexcept
    {
        const jeecs::typing::type_info info
            = jeecs::typing::type_info::of<T>();

    }

    namespace math
    {
        static float clamp(float src, float min, float max)
        {
            return (src < min) ? (
                min
                ) :
                (
                    (src > max) ?
                    (max) :
                    (src)
                    );
        }
        template<typename T>
        T lerp(const T& va, const T& vb, float deg)
        {
            return va * (1.0f - deg) + vb * deg;
        }

        template<typename T>
        T random(T from, T to)
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

        };
        inline static constexpr vec2 operator * (float _f, const vec2& _v2) noexcept
        {
            return vec2(_v2.x * _f, _v2.y * _f);
        }

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

        };
        inline static constexpr vec4 operator * (float _f, const vec4& _v4) noexcept
        {
            return vec4(_v4.x * _f, _v4.y * _f, _v4.z * _f, _v4.w * _f);
        }

        struct quat
        {
            constexpr static float RAD2DEG = 57.29578f;
            float x, y, z, w;

            inline constexpr bool operator == (const quat& q) const noexcept
            {
                return x == q.x && y == q.y && z == q.z && w == q.w;
            }
            inline constexpr bool operator != (const quat& q) const noexcept
            {
                return x != q.x || y != q.y || z != q.z || w != q.w;
            }

            constexpr quat(float _x = 0.f, float _y = 0.f, float _z = 0.f, float _w = 1.f) noexcept
                :x(_x / (_x * _x + _y * _y + _z * _z + _w * _w))
                , y(_y / (_x * _x + _y * _y + _z * _z + _w * _w))
                , z(_z / (_x * _x + _y * _y + _z * _z + _w * _w))
                , w(_w / (_x * _x + _y * _y + _z * _z + _w * _w)) { }

            quat(float yaw, float pitch, float roll) noexcept
            {
                set_euler_angle(yaw, pitch, roll);
            }

            inline void create_matrix(float* pMatrix) const noexcept
            {
                // 转换为矩阵
                if (!pMatrix) return;
                float m_x = x;
                float m_y = y;
                float m_z = z;
                float m_w = w;
                //第一行
                pMatrix[0] = 1.0f - 2.0f * (m_y * m_y + m_z * m_z);
                pMatrix[1] = 2.0f * (m_x * m_y + m_z * m_w);
                pMatrix[2] = 2.0f * (m_x * m_z - m_y * m_w);
                pMatrix[3] = 0.0f;

                // 第二行
                pMatrix[4] = 2.0f * (m_x * m_y - m_z * m_w);
                pMatrix[5] = 1.0f - 2.0f * (m_x * m_x + m_z * m_z);
                pMatrix[6] = 2.0f * (m_z * m_y + m_x * m_w);
                pMatrix[7] = 0.0f;

                //第三行
                pMatrix[8] = 2.0f * (m_x * m_z + m_y * m_w);
                pMatrix[9] = 2.0f * (m_y * m_z - m_x * m_w);
                pMatrix[10] = 1.0f - 2.0f * (m_x * m_x + m_y * m_y);
                pMatrix[11] = 0.0f;

                //第四行
                pMatrix[12] = 0;
                pMatrix[13] = 0;
                pMatrix[14] = 0;
                pMatrix[15] = 1.0f;
            }
            inline void create_inv_matrix(float* pMatrix) const noexcept
            {
                // 转换为矩阵
                if (!pMatrix) return;
                float m_x = x;
                float m_y = y;
                float m_z = z;
                float m_w = -w;
                //第一行
                pMatrix[0] = 1.0f - 2.0f * (m_y * m_y + m_z * m_z);
                pMatrix[1] = 2.0f * (m_x * m_y + m_z * m_w);
                pMatrix[2] = 2.0f * (m_x * m_z - m_y * m_w);
                pMatrix[3] = 0.0f;

                // 第二行
                pMatrix[4] = 2.0f * (m_x * m_y - m_z * m_w);
                pMatrix[5] = 1.0f - 2.0f * (m_x * m_x + m_z * m_z);
                pMatrix[6] = 2.0f * (m_z * m_y + m_x * m_w);
                pMatrix[7] = 0.0f;

                //第三行
                pMatrix[8] = 2.0f * (m_x * m_z + m_y * m_w);
                pMatrix[9] = 2.0f * (m_y * m_z - m_x * m_w);
                pMatrix[10] = 1.0f - 2.0f * (m_x * m_x + m_y * m_y);
                pMatrix[11] = 0.0f;

                //第四行
                pMatrix[12] = 0;
                pMatrix[13] = 0;
                pMatrix[14] = 0;
                pMatrix[15] = 1.0f;
            }

            inline constexpr float dot(const quat& _quat) const noexcept
            {
                return x * _quat.x + y * _quat.y + z * _quat.z + w * _quat.w;
            }
            inline void set_euler_angle(float yaw, float pitch, float roll) noexcept
            {
                yaw = yaw / RAD2DEG;
                pitch = pitch / RAD2DEG;
                roll = roll / RAD2DEG;

                float angle;
                float sinRoll, sinPitch, sinYaw, cosRoll, cosPitch, cosYaw;

                angle = yaw * 0.5f;
                sinYaw = sin(angle);
                cosYaw = cos(angle);
                angle = pitch * 0.5f;
                sinPitch = sin(angle);
                cosPitch = cos(angle);
                angle = roll * 0.5f;
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
                auto sv = sin(arg / 2.0f / RAD2DEG);
                auto cv = cos(arg / 2.0f / RAD2DEG);
                return quat(a.x * sv, a.y * sv, a.z * sv, cv);
            }

            static inline constexpr quat lerp(const quat& a, const quat& b, float t)
            {
                return quat((1 - t) * a.x + t * b.x,
                    (1 - t) * a.y + t * b.y,
                    (1 - t) * a.z + t * b.z,
                    (1 - t) * a.w + t * b.w);
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
                if (cos_theta > 1.f - FLT_EPSILON)
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
                // if B is on opposite hemisphere from A, use -B instead
                if (cos_theta < 0.f)
                {
                    cos_theta = -cos_theta;
                }
                float theta = acos(cos_theta);
                return 2 * RAD2DEG * theta;
            }

            inline constexpr quat conjugate() const noexcept
            {
                return quat(-x, -y, -z, w);
            }
            inline constexpr quat inverse() const noexcept
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

            inline constexpr quat operator * (const quat& _quat) const noexcept
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
        };
    }

    namespace Transform
    {
        // An entity without childs and parent will contain these components:
        // LocalPosition/LocalRotation/LocalScale and using LocalToWorld to apply
        // local transform to Translation
        // If an entity have childs, it will have ChildAnchor 
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

        struct LocalPosition
        {
            math::vec3 pos;
        };
        struct LocalRotation
        {
            math::quat rot;
        };
        struct LocalScale
        {
            math::vec3 scale;
        };

        struct ChildAnchor
        {
            typing::uid_t anchor_id = je_uid_generate();
        };

        struct LocalToParent
        {
            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;

            typing::uid_t parent_uid;
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

        struct Translation
        {
            float object2world[16] =
            {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1, };
            float objectrotation[16] = {
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1, };
            math::quat rotation;

            inline void set_position(const math::vec3& _v3) noexcept
            {
                object2world[0 + 3 * 4] = _v3.x;
                object2world[1 + 3 * 4] = _v3.y;
                object2world[2 + 3 * 4] = _v3.z;
            }

            inline void set_scale(const math::vec3& _v3) noexcept
            {
                object2world[0 + 0 * 4] = _v3.x;
                object2world[1 + 1 * 4] = _v3.y;
                object2world[2 + 2 * 4] = _v3.z;
                object2world[3 + 3 * 4] = 1.f;
            }
            inline void set_rotation(const math::quat& _quat)noexcept
            {
                rotation = _quat;
                _quat.create_matrix(objectrotation);
            }

            inline constexpr math::vec3 get_position() const noexcept
            {
                return math::vec3(
                    object2world[0 + 3 * 4],
                    object2world[1 + 3 * 4],
                    object2world[2 + 3 * 4]);
            }
            inline constexpr math::vec3 get_scale() const noexcept
            {
                return math::vec3(
                    object2world[0 + 0 * 4],
                    object2world[1 + 1 * 4],
                    object2world[2 + 2 * 4]);
            }
            inline constexpr math::quat get_rotation() const noexcept
            {
                return rotation;
            }
        };
    }
    namespace Renderer
    {
        // An entity need to be rendered, must have Transform::Translation and 
        // Renderer, 
        /*
        Camera----------\
                         >------> Cameras
                        /          / | \
        Translation ---<          /  |  \ (for each..)
                        \        /   |   \
                         >--  The entities need
        Material--------/         be rend..
                       /
        Shape---------/

        */
        struct OrthoCamera
        {
            float znear = 0;
            float zfar = 1000;
        };

        struct Shape
        {

        };

        struct Material
        {

        };
    }

    namespace enrty
    {
        inline void module_entry()
        {
            // 0. register built-in components
            jeecs::typing::type_info::of<Transform::LocalPosition>("Transform::LocalPosition");
            jeecs::typing::type_info::of<Transform::LocalRotation>("Transform::LocalRotation");
            jeecs::typing::type_info::of<Transform::LocalScale>("Transform::LocalScale");
            jeecs::typing::type_info::of<Transform::ChildAnchor>("Transform::ChildAnchor");
            jeecs::typing::type_info::of<Transform::LocalToWorld>("Transform::LocalToWorld");
            jeecs::typing::type_info::of<Transform::LocalToParent>("Transform::LocalToParent");
            jeecs::typing::type_info::of<Transform::Translation>("Transform::Translation");

            jeecs::typing::type_info::of<Renderer::OrthoCamera>("Renderer::OrthoCamera");
            jeecs::typing::type_info::of<Renderer::Shape>("Renderer::Shape");
            jeecs::typing::type_info::of<Renderer::Material>("Renderer::Material");

            // 1. register core&graphic systems.
            jeecs_entry_register_core_systems();
        }

        inline void module_leave()
        {
            // 0. ungister this module components
            typing::type_info::unregister_all_type_in_shutdown();
        }
    }

    namespace graphic
    {

    }
}

#endif