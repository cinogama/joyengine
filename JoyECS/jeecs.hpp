#pragma once

#ifndef __cplusplus
#error jeecs.h only support for c++
#else

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

        using construct_func_t = void(*)(void*);
        using destruct_func_t = void(*)(void*);
        using copy_func_t = void(*)(void*, const void*);
        using move_func_t = void(*)(void*, void*);
        using to_string_func_t = const char* (*)(void*);
        using parse_func_t = void(*)(void*, const char*);

        using entity_id_in_chunk_t = size_t;
        using version_t = size_t;
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
    jeecs::typing::move_func_t      _mover);

JE_API const jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id);

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

JE_API void je_ecs_universe_store_world_system_instance(
    void* universe,
    void* world,
    jeecs::game_system* gsystem_instance,
    void(*gsystem_destructor)(jeecs::game_system*)
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

JE_API double je_clock_time();

JE_API void je_clock_sleep_until(double time);

JE_API void je_clock_sleep_for(double time);

JE_API void je_clock_suppress_sleep(double sup_stax);

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
            static void constructor(void* _ptr)
            {
                new(_ptr)T;
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
                        basic::default_functions<T>::mover))
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

            void construct(void* addr) const
            {
                m_constructor(addr);
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
        template<typename T, typename ... ArgTs>
        inline T* add_system(ArgTs&&... args)
        {
            T* created_system = basic::create_new<T>(*this, args...);

            je_ecs_universe_store_world_system_instance(
                je_ecs_world_in_universe(handle()),
                handle(),
                created_system,
                (void(*)(jeecs::game_system*)) & basic::destroy_free<T>
            );

            return created_system;
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
            const T* operator ->() const noexcept { return (const T*)_m_component_addr; }
            const T& operator * () const noexcept { return *(const T*)_m_component_addr; }
        };
        template<typename T>
        struct read_newest : read_updated_base {
        public:
            void* _m_component_addr;
        public:
            const T* operator ->() const noexcept { return (const T*)_m_component_addr; }
            const T& operator * () const noexcept { return *(const T*)_m_component_addr; }
        };
        template<typename T>
        struct write : write_base {
        public:
            void* _m_component_addr;
        public:
            T* operator ->() const noexcept { return (T*)_m_component_addr; }
            T& operator * () const noexcept { return *(T*)_m_component_addr; }
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
        struct origin_component<read_newest<T>>
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
        inline static requirement except()
        {
            return requirement{ game_system_function::dependence_type::EXCEPT, jeecs::typing::type_info::id<T>() };
        }

        template<typename T>
        inline static requirement any_of()
        {
            return requirement{ game_system_function::dependence_type::ANY, jeecs::typing::type_info::id<T>() };
        }

        template<typename ReturnT, typename ThisT, typename ... ArgTs>
        inline auto pack_normal_invoker(ReturnT(ThisT::* system_func)(ArgTs ...), const std::vector<requirement>& requirement)
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
            };

            auto* gsys = jeecs::game_system_function::create(invoker, sizeof...(ArgTs));
            std::vector<jeecs::game_system_function::typeid_dependence_pair> depends
                = { {
                        jeecs::typing::type_info::id<typename origin_component<ArgTs>::type>(),
                        depend_type<ArgTs>(),
                    }... };

            for (auto& req : requirement)
            {
                depends.push_back({ req.m_required_id,req.m_depend });
            }

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
            {
                depends.push_back({ req.m_required_id,req.m_depend });
            }

            gsys->set_depends(depends);

            return gsys;
        }

        template<typename ReturnT, typename ThisT, typename ... ArgTs>
        inline jeecs::game_system_function* register_system_func(ReturnT(ThisT::* sysf)(ArgTs ...), const std::vector<requirement>& requirement = {})
        {
            static_assert(sizeof...(ArgTs));

            jeecs::game_system_function::system_function_pak_t invoker;

            if constexpr (is_need_game_entity<ArgTs...>::value)
                return _m_game_world.register_system_func_to_world(pack_normal_invoker_with_entity(sysf, requirement));
            else
                return _m_game_world.register_system_func_to_world(pack_normal_invoker(sysf, requirement));

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


    namespace enrty
    {
        inline void module_entry()
        {
            // 0. register built-in components
        }

        inline void module_leave()
        {
            // 0. ungister this module components
            typing::type_info::unregister_all_type_in_shutdown();
        }
    }

}

#endif