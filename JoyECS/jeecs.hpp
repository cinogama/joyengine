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
#include <map>
#include <algorithm>
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

        struct type_info;

        using construct_func_t = void(*)(void*);
        using destruct_func_t = void(*)(void*);
        using copy_func_t = void(*)(void*, const void*);
        using move_func_t = void(*)(void*, void*);
    }

    namespace arch
    {
        using entity_id_in_chunk_t = size_t;
        using types_set = std::set<typing::typeid_t>;
        using version_t = size_t;

        constexpr entity_id_in_chunk_t INVALID_ENTITY_ID = SIZE_MAX;
        constexpr size_t ALLIGN_BASE = alignof(std::max_align_t);
        constexpr size_t CHUNK_SIZE = 64 * 1024; // 64K
    }
}

RS_FORCE_CAPI
JE_API void* je_mem_alloc(size_t sz);
JE_API void je_mem_free(void* ptr);

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


RS_FORCE_CAPI_END

namespace jeecs
{
#define JECS_DISABLE_MOVE_AND_COPY(TYPE) \
    TYPE(const TYPE &)  = delete;\
    TYPE(TYPE &&)       = delete;\
    TYPE& operator = (const TYPE &) = delete;\
    TYPE& operator = (TYPE &&) = delete;

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

            void add_one(NodeT* node)
            {
                NodeT* last_last_node = last_node;// .exchange(node);
                do
                {
                    node->last = last_last_node;
                } while (!last_node.compare_exchange_weak(last_last_node, node));
            }

            NodeT* pick_all()
            {
                NodeT* result = nullptr;
                result = last_node.exchange(nullptr);

                return result;
            }

            NodeT* peek()
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

    namespace arch
    {
        class command_buffer;
        class arch_manager;

        class arch_type
        {
            JECS_DISABLE_MOVE_AND_COPY(arch_type);
            // ahahahah, arch_type is coming!

            struct arch_type_info
            {
                const typing::type_info* m_typeinfo;
                size_t m_begin_offset_in_chunk;
            };

            using types_list = std::vector<const typing::type_info*>;
            using archtypes_map = std::unordered_map<typing::typeid_t, arch_type_info>;

            const types_list      _m_arch_typeinfo;
            const types_set       _m_types_set;
            const archtypes_map   _m_arch_typeinfo_mapping;
            const size_t    _m_entity_size;
            const size_t    _m_entity_count_per_chunk;

            arch_manager* _m_arch_manager;

        public:
            class arch_chunk
            {
                JECS_DISABLE_MOVE_AND_COPY(arch_chunk);

                using byte_t = uint8_t;
                static_assert(sizeof(byte_t) == 1, "sizeof(uint8_t) should be 1.");

                byte_t _m_chunk_buffer[CHUNK_SIZE];
                const types_set& _m_types;
                const archtypes_map& _m_arch_typeinfo_mapping;
                const size_t _m_entity_count;
                const size_t _m_entity_size;
                struct _entity_meta
                {
                    std::atomic_flag m_in_used = {};
                    version_t m_version = 0;

                    enum class entity_stat
                    {
                        UNAVAILABLE,    // Entity is destroied or just not ready,
                        READY,          // Entity is OK, and just work as normal.
                    };
                    entity_stat m_stat = entity_stat::UNAVAILABLE;
                };

                _entity_meta* _m_entities_meta;
                std::atomic_size_t _m_free_count;
                arch_type* _m_arch_type;
            public:
                arch_chunk* last; // for atomic_list;
            public:
                arch_chunk(arch_type* _arch_type)
                    : _m_entity_count(_arch_type->_m_entity_count_per_chunk)
                    , _m_entity_size(_arch_type->_m_entity_size)
                    , _m_free_count(_arch_type->_m_entity_count_per_chunk)
                    , _m_arch_typeinfo_mapping(_arch_type->_m_arch_typeinfo_mapping)
                    , _m_types(_arch_type->_m_types_set)
                    , _m_arch_type(_arch_type)
                {
                    _m_entities_meta = basic::create_new_n<_entity_meta>(_m_entity_count);
                }
                ~arch_chunk()
                {
                    // All entity in chunk should be free.
                    assert(_m_free_count == _m_entity_count);
                    basic::destroy_free_n(_m_entities_meta, _m_entity_count);
                }

            public:
                // ATTENTION: move_component_to WILL INVOKE DESTRUCT FUNCTION OF from_component
                inline void move_component_to(entity_id_in_chunk_t eid, typing::typeid_t tid, void* from_component)const
                {
                    const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping.at(tid);
                    void* component_addr = get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);
                    arch_typeinfo.m_typeinfo->move(component_addr, from_component);
                    arch_typeinfo.m_typeinfo->destruct(from_component);
                }

                bool alloc_entity_id(entity_id_in_chunk_t* out_id, version_t* out_version)
                {
                    size_t free_entity_count = _m_free_count;
                    while (free_entity_count)
                    {
                        if (_m_free_count.compare_exchange_weak(free_entity_count, free_entity_count - 1))
                        {
                            // OK There is a usable place for entity
                            for (size_t id = 0; id < _m_entity_count; id++)
                            {
                                if (!_m_entities_meta[id].m_in_used.test_and_set())
                                {
                                    *out_id = id;
                                    *out_version = ++(_m_entities_meta[id].m_version);
                                    return true;
                                }
                            }
                            assert(false); // entity count is ok, but there is no free place. that should not happend.
                        }
                        free_entity_count = _m_free_count; // Fail to compare, update the count and retry.
                    }
                    return false;
                }
                inline void* get_component_addr(entity_id_in_chunk_t _eid, size_t _chunksize, size_t _offset)const noexcept
                {
                    return (void*)(_m_chunk_buffer + _offset + _eid * _chunksize);
                }
                inline bool is_entity_valid(entity_id_in_chunk_t eid, version_t eversion) const noexcept
                {
                    if (_m_entities_meta[eid].m_version != eversion)
                        return false;
                    return true;
                }
                inline void* get_component_addr_with_typeid(entity_id_in_chunk_t eid, typing::typeid_t tid) const noexcept
                {
                    const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping.at(tid);
                    return get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);

                }
                inline void destruct_component_addr_with_typeid(entity_id_in_chunk_t eid, typing::typeid_t tid) const noexcept
                {
                    const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping.at(tid);
                    auto* component_addr = get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);

                    arch_typeinfo.m_typeinfo->destruct(component_addr);

                }
                inline const types_set& types()const noexcept
                {
                    return _m_types;
                }
                inline arch_type* get_arch_type()const noexcept
                {
                    return _m_arch_type;
                }
            private:
                // Following function only invoke by command_buffer
                friend class command_buffer;

                void command_active_entity(entity_id_in_chunk_t eid) noexcept
                {
                    _m_entities_meta[eid].m_stat = _entity_meta::entity_stat::READY;
                }

                void command_close_entity(entity_id_in_chunk_t eid) noexcept
                {
                    // TODO: MULTI-THREAD PROBLEM
                    _m_entities_meta[eid].m_stat = _entity_meta::entity_stat::UNAVAILABLE;
                    ++_m_entities_meta[eid].m_version;
                    _m_entities_meta[eid].m_in_used.clear();

                    ++_m_free_count;
                    ++_m_arch_type->_m_free_count;
                }
            };

        private:

            std::atomic_size_t _m_free_count;
            basic::atomic_list<arch_chunk> _m_chunks;

        public:
            struct entity
            {
                arch_chunk* _m_in_chunk;
                entity_id_in_chunk_t   _m_id;
                version_t              _m_version;

                // Do not invoke this function if possiable, you should get component by arch_type & system.
                template<typename CT>
                inline CT* get_component() const
                {
                    assert(_m_in_chunk->is_entity_valid(_m_id, _m_version));
                    return (CT*)_m_in_chunk->get_component_addr_with_typeid(_m_id, typing::type_info::id<CT>());
                }

                inline arch_chunk* chunk()const noexcept
                {
                    return _m_in_chunk;
                }

                inline bool operator < (const entity& another) const noexcept
                {
                    if (_m_in_chunk < another._m_in_chunk)
                        return true;
                    if (_m_in_chunk > another._m_in_chunk)
                        return false;
                    if (_m_id < another._m_id)
                        return true;
                    return false;
                }

                inline bool valid() const noexcept
                {
                    return _m_in_chunk->is_entity_valid(_m_id, _m_version);
                }
            };

        public:
            arch_type(arch_manager* _arch_manager, const types_set& _types_set)
                : _m_entity_size(0)
                , _m_free_count(0)
                , _m_entity_count_per_chunk(0)
                , _m_types_set(_types_set)
                , _m_arch_manager(_arch_manager)
            {
                for (typing::typeid_t tid : _types_set)
                    const_cast<types_list&>(_m_arch_typeinfo).push_back(typing::type_info::of(tid));

                std::sort(const_cast<types_list&>(_m_arch_typeinfo).begin(),
                    const_cast<types_list&>(_m_arch_typeinfo).end(),
                    [](const typing::type_info* a, const typing::type_info* b) {
                        return a->m_size < b->m_size;
                    });

                const_cast<size_t&>(_m_entity_size) = 0;
                for (auto* typeinfo : _m_arch_typeinfo)
                {
                    const_cast<size_t&>(_m_entity_size) += typeinfo->m_chunk_size;
                }

                const_cast<size_t&>(_m_entity_count_per_chunk) = CHUNK_SIZE / _m_entity_size;

                size_t mem_offset = 0;
                for (auto* typeinfo : _m_arch_typeinfo)
                {
                    const_cast<archtypes_map&>(_m_arch_typeinfo_mapping)[typeinfo->m_id]
                        = arch_type_info{ typeinfo, mem_offset };
                    mem_offset += typeinfo->m_chunk_size * _m_entity_count_per_chunk;
                }
            }

            ~arch_type()
            {
                arch_chunk* chunk = _m_chunks.pick_all();
                while (chunk)
                {
                    auto* next_chunk = chunk->last;
                    basic::destroy_free(chunk);

                    chunk = next_chunk;
                }
            }

            arch_chunk* _create_new_chunk()
            {
                arch_chunk* new_chunk = basic::create_new<arch_chunk>(this);
                _m_chunks.add_one(new_chunk);
                _m_free_count += _m_entity_count_per_chunk;
                return new_chunk;
            }

            void alloc_entity(arch_chunk** out_chunk, entity_id_in_chunk_t* out_eid, version_t* out_eversion)
            {
                while (true)
                {
                    size_t free_entity_count = _m_free_count;
                    while (free_entity_count)
                    {
                        if (_m_free_count.compare_exchange_weak(free_entity_count, free_entity_count - 1))
                        {
                            // OK There is a usable place for entity
                            arch_chunk* peek_chunk = _m_chunks.peek();
                            while (peek_chunk)
                            {
                                if (peek_chunk->alloc_entity_id(out_eid, out_eversion))
                                {
                                    *out_chunk = peek_chunk;
                                    return;
                                }
                                peek_chunk = peek_chunk->last;
                            }

                            assert(false); // entity count is ok, but there is no free place. that should not happend.
                        }
                        free_entity_count = _m_free_count; // Fail to compare, update the count and retry.
                    }
                    _create_new_chunk();
                }
            }

            entity instance_entity()
            {
                arch_chunk* chunk;
                entity_id_in_chunk_t entity_id;
                version_t            entity_version;

                alloc_entity(&chunk, &entity_id, &entity_version);
                for (auto& arch_typeinfo : _m_arch_typeinfo_mapping)
                {
                    void* component_addr = chunk->get_component_addr(entity_id,
                        arch_typeinfo.second.m_typeinfo->m_chunk_size,
                        arch_typeinfo.second.m_begin_offset_in_chunk);

                    arch_typeinfo.second.m_typeinfo->construct(component_addr);
                }

                return entity{ chunk ,entity_id, entity_version };
            }

            arch_manager* get_arch_mgr()const noexcept
            {
                return _m_arch_manager;
            }

        };

        class arch_manager
        {
            JECS_DISABLE_MOVE_AND_COPY(arch_manager);

            using arch_map_t = std::map<types_set, arch_type*>;

            arch_map_t _m_arch_types_mapping;
            std::shared_mutex _m_arch_types_mapping_mx;
        public:
            arch_manager() = default;
            ~arch_manager()
            {
                for (auto& [types, archtype] : _m_arch_types_mapping)
                    basic::destroy_free(archtype);
            }

            arch_type* find_or_add_arch(const types_set& _types)
            {
                do
                {
                    std::shared_lock sg1(_m_arch_types_mapping_mx);
                    auto fnd = _m_arch_types_mapping.find(_types);
                    if (fnd != _m_arch_types_mapping.end())
                    {
                        return fnd->second;
                    }
                } while (0);

                std::lock_guard g1(_m_arch_types_mapping_mx);
                jeecs::arch::arch_type*& atype = _m_arch_types_mapping[_types];
                if (nullptr == atype)
                    atype = basic::create_new<jeecs::arch::arch_type>(this, _types);

                return atype;
            }
            arch_type::entity create_an_entity_with_component(const types_set& _types)
            {
                assert(!_types.empty());
                return find_or_add_arch(_types)->instance_entity();
            }
        };

        class command_buffer
        {
            // command_buffer used to store operations happend in a entity.
            JECS_DISABLE_MOVE_AND_COPY(command_buffer);

            struct _entity_command_buffer
            {
                JECS_DISABLE_MOVE_AND_COPY(_entity_command_buffer);

                struct typed_component
                {
                    const typing::type_info* m_typeinfo;
                    void* m_component_addr;

                    typed_component* last;

                    typed_component(const typing::type_info* id, void* addr)
                        : m_typeinfo(id)
                        , m_component_addr(addr)
                    {
                        // Do nothing else
                    }
                };

                basic::atomic_list<typed_component> m_removed_components;
                basic::atomic_list<typed_component> m_append_components;
                bool                                m_entity_removed_flag;

                _entity_command_buffer() = default;
            };

            std::shared_mutex _m_entity_command_buffer_mx;
            std::map<arch_type::entity, _entity_command_buffer> _m_entity_command_buffer;

            _entity_command_buffer& _find_or_create_buffer_for(const arch_type::entity& e)
            {
                do
                {
                    std::shared_lock sg1(_m_entity_command_buffer_mx);
                    auto fnd = _m_entity_command_buffer.find(e);
                    if (fnd != _m_entity_command_buffer.end())
                        return fnd->second;

                } while (0);

                std::lock_guard g1(_m_entity_command_buffer_mx);
                return _m_entity_command_buffer[e];
            }
        public:
            command_buffer() = default;

            void init_new_entity(const arch_type::entity& e)
            {
                _find_or_create_buffer_for(e);
            }

            void remove_entity(const arch_type::entity& e)
            {
                _find_or_create_buffer_for(e).m_entity_removed_flag = true;
            }

            void* append_component(const arch_type::entity& e, const typing::type_info* component_type)
            {
                // Instance component
                void* created_component = je_mem_alloc(component_type->m_size);
                component_type->construct(created_component);

                _find_or_create_buffer_for(e).m_append_components.add_one(
                    basic::create_new<_entity_command_buffer::typed_component>(component_type, created_component)
                );

                return created_component;
            }

            void remove_component(const arch_type::entity& e, const typing::type_info* component_type)
            {
                _find_or_create_buffer_for(e).m_removed_components.add_one(
                    basic::create_new<_entity_command_buffer::typed_component>(component_type, nullptr)
                );
            }

        public:
            void update()
            {
                // Update all operate in this buffer
                std::for_each(
#ifdef __cpp_lib_execution
                    std::execution::par_unseq,
#endif
                    _m_entity_command_buffer.begin(), _m_entity_command_buffer.end(),
                    [](std::pair<const arch_type::entity, _entity_command_buffer>& _buf_in_entity)
                    {
                        //  If entity not valid, skip component append&remove&active, but need 
                        // free temp components.
                        arch_type::entity current_entity = _buf_in_entity.first;

                        if (current_entity.valid())
                        {
                            if (_buf_in_entity.second.m_entity_removed_flag)
                            {
                                // Remove all new component;
                                auto* append_typed_components = _buf_in_entity.second.m_append_components.pick_all();
                                while (append_typed_components)
                                {
                                    // Free template component
                                    auto current_typed_component = append_typed_components;
                                    append_typed_components = append_typed_components->last;

                                    current_typed_component->m_typeinfo
                                        ->m_destructor(current_typed_component->m_component_addr);
                                    je_mem_free(current_typed_component->m_component_addr);

                                    basic::destroy_free(current_typed_component);
                                }

                                auto* removed_typed_components = _buf_in_entity.second.m_removed_components.pick_all();
                                while (removed_typed_components)
                                {
                                    auto current_typed_component = removed_typed_components;
                                    removed_typed_components = removed_typed_components->last;

                                    basic::destroy_free(current_typed_component);
                                }

                                // Remove all component
                                types_set origin_chunk_types = current_entity.chunk()->types();
                                for (typing::typeid_t type_id : origin_chunk_types)
                                {
                                    current_entity.chunk()->destruct_component_addr_with_typeid(
                                        current_entity._m_id,
                                        type_id
                                    );
                                }

                                // OK, Mark old entity chunk is freed, 
                                current_entity.chunk()->command_close_entity(current_entity._m_id);
                            }
                            else
                            {
                                // 1. Mark entity as active..
                                current_entity.chunk()->command_active_entity(current_entity._m_id);

                                types_set new_chunk_types = current_entity.chunk()->types();

                                // 2. Destroy removed component..
                                auto* removed_typed_components = _buf_in_entity.second.m_removed_components.pick_all();
                                while (removed_typed_components)
                                {
                                    auto current_typed_component = removed_typed_components;
                                    removed_typed_components = removed_typed_components->last;

                                    assert(current_typed_component->m_component_addr == nullptr);
                                    if (new_chunk_types.erase(current_typed_component->m_typeinfo->m_id))
                                    {
                                        current_entity.chunk()
                                            ->destruct_component_addr_with_typeid(current_entity._m_id,
                                                current_typed_component->m_typeinfo->m_id);
                                    }

                                    basic::destroy_free(current_typed_component);
                                }

                                // 3. Prepare append component..(component may be repeated, so we using last one and give warning)
                                std::unordered_map<typing::typeid_t, void*> append_component_type_addr_set;
                                auto* append_typed_components = _buf_in_entity.second.m_append_components.pick_all();
                                while (append_typed_components)
                                {
                                    // Free template component
                                    auto current_typed_component = append_typed_components;
                                    append_typed_components = append_typed_components->last;

                                    assert(new_chunk_types.find(current_typed_component->m_typeinfo->m_id) == new_chunk_types.end());

                                    auto& addr_place = append_component_type_addr_set[current_typed_component->m_typeinfo->m_id];
                                    if (addr_place)
                                    {
                                        // This type of component already in list, destruct/free it and give warning
                                        // TODO: WARNING~
                                        current_typed_component->m_typeinfo->m_destructor(addr_place);
                                        je_mem_free(addr_place);
                                    }
                                    addr_place = current_typed_component->m_component_addr;
                                    new_chunk_types.insert(current_typed_component->m_typeinfo->m_id);

                                    basic::destroy_free(current_typed_component);
                                }

                                // 5. Almost done! get new arch type:
                                auto* current_arch_type = current_entity.chunk()->get_arch_type();
                                auto* new_arch_type = current_arch_type->get_arch_mgr()->find_or_add_arch(new_chunk_types);

                                if (new_arch_type == current_arch_type)
                                {
                                    // New & old arch is same, rebuilt in place.
                                    for (auto [type_id, component_addr] : append_component_type_addr_set)
                                    {
                                        current_entity.chunk()->move_component_to(current_entity._m_id, type_id, component_addr);
                                        je_mem_free(component_addr);
                                    }
                                }
                                else
                                {
                                    arch_type::arch_chunk* chunk;
                                    entity_id_in_chunk_t entity_id;
                                    version_t entity_version;

                                    new_arch_type->alloc_entity(&chunk, &entity_id, &entity_version);
                                    // Entity alloced, move component to here..

                                    for (typing::typeid_t type_id : new_chunk_types)
                                    {
                                        auto fnd = append_component_type_addr_set.find(type_id);
                                        if (fnd == append_component_type_addr_set.end())
                                        {
                                            // 1. Move old component
                                            chunk->move_component_to(entity_id, type_id,
                                                current_entity.chunk()->get_component_addr_with_typeid(
                                                    current_entity._m_id, type_id));
                                        }
                                        else
                                        {
                                            // 2. Move new component
                                            chunk->move_component_to(entity_id, type_id, fnd->second);
                                            je_mem_free(fnd->second);
                                        }
                                    }

                                    // OK, Mark old entity chunk is freed, 
                                    current_entity.chunk()->command_close_entity(current_entity._m_id);
                                }

                            }// End component modify
                        }
                        else
                        {
                            auto* append_typed_components = _buf_in_entity.second.m_append_components.pick_all();
                            while (append_typed_components)
                            {
                                // Free template component
                                auto current_typed_component = append_typed_components;
                                append_typed_components = append_typed_components->last;

                                current_typed_component->m_typeinfo
                                    ->destruct(current_typed_component->m_component_addr);
                                je_mem_free(current_typed_component->m_component_addr);

                                basic::destroy_free(current_typed_component);
                            }

                            auto* removed_typed_components = _buf_in_entity.second.m_removed_components.pick_all();
                            while (removed_typed_components)
                            {
                                auto current_typed_component = removed_typed_components;
                                removed_typed_components = removed_typed_components->last;

                                basic::destroy_free(current_typed_component);
                            }
                        }
                    });

                // Finish! clear buffer.
                _m_entity_command_buffer.clear();
            }
        };
    }
}

#endif