/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/

#include <iostream>
#include <algorithm>
#include <typeinfo>
#include <atomic>
#include <cstdint>
#include <cassert>

#include <string>
#include <set>

#include <shared_mutex>
#include <unordered_map>

void* je_mem_alloc(size_t sz)
{
    return malloc(sz);
}
void je_mem_free(void* ptr)
{
    return free(ptr);
}

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
            return new(je_mem_alloc(sizeof(T)))T(args...);
        }
        template<typename T, typename  ... ArgTs>
        T* create_new_n(size_t n)
        {
            return new(je_mem_alloc(sizeof(T) * n))T[n];
        }

        template<typename T>
        void destroy_free(T* address)
        {
            address->~T();
            je_mem_free(address);
        }
        template<typename T>
        void destroy_free_n(T* address, size_t n)
        {
            for (size_t i = 0; i < n; i++)
                address[i].~T();

            je_mem_free(address);
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
        struct default_construct_function
        {
            static void constructor(void* _ptr)
            {
                new(_ptr)T;
            }
        };
        template<typename T>
        struct destruct_function
        {
            static void destructor(void* _ptr)
            {
                ((T*)_ptr)->~T();
            }
        };
        template<typename T>
        struct copy_construct_function
        {
            static void copy(void* _ptr, const void* _be_copy_ptr)
            {
                new(_ptr)T(*(const T*)_be_copy_ptr);
            }
        };
        template<typename T>
        struct move_construct_function
        {
            static void move(void* _ptr, void* _be_moved_ptr)
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

    namespace core
    {
        using entity_id_t = size_t;
        using version_t = size_t;
        using atomic_version_t = std::atomic_size_t;
        using typehash_t = decltype(basic::type_hash<int>());
        using rw_mutex_t = std::shared_mutex;
        using orderd_type_set_t = std::set<typehash_t>;

        constexpr entity_id_t INVALID_ENTITY_ID = SIZE_MAX;

        class arch_group
        {
            JECS_DISABLE_MOVE_AND_COPY(arch_group);
        public:
            constexpr static size_t CHUNK_ENTITY_GROUP = 64;
            constexpr static size_t ALLIGN_BASE_COUNT = 8;
        public:
            friend class chunk;

            class entity;

            class chunk
            {
                friend class entity;

                JECS_DISABLE_MOVE_AND_COPY(chunk);

                struct _entity_inform
                {
                    std::atomic_flag m_in_used = {};
                    atomic_version_t m_version = 0;

                    enum entity_state_mask :uint8_t
                    {
                        NOT_ACTIVE = 1 << 0,            // entity has been created/destroied, need active
                        COMPONENT_REMOVING = 1 << 1,    // some component is removing from this entity
                        COMPONENT_ADDING = 1 << 2,      // some component is adding to this entity
                        DESTROYING = 1 << 3,            // this entity will be destroy
                    };
                    std::atomic_uint8_t m_state = entity_state_mask::NOT_ACTIVE;

                    static_assert(sizeof(m_state) == sizeof(entity_state_mask),
                        "size of 'm_state' and 'entity_state_mask' must be equal.");
                };

                // A chunk will store AoC, C will allign with 8 bytes.
                // 1 Chunk will store CHUNK_ENTITY_GROUP entity(s)

                std::atomic_size_t  _m_free_entity_count;
                arch_group* _m_arch_group;

                _entity_inform      _m_entity_informs[CHUNK_ENTITY_GROUP];
                void* _m_array_of_components_buffer;

                // NOTE: In arch update, component-adding/removing should be update first,
                //       
                std::atomic_size_t _m_adding_component_entity_count;
                std::atomic_size_t _m_removing_component_entity_count;
                std::atomic_size_t _m_destroying_entity_count;

            public:
                chunk(arch_group* _arch_group)
                    : last(nullptr)
                    , _m_free_entity_count(CHUNK_ENTITY_GROUP)
                    , _m_arch_group(_arch_group)
                    , _m_destroying_entity_count(0)
                    , _m_adding_component_entity_count(0)
                    , _m_removing_component_entity_count(0)
                {
                    _m_array_of_components_buffer = je_mem_alloc(_arch_group->_m_chunk_size);
                }
                ~chunk()
                {
                    // ATTENTION: NEED DO DESSTRUCT FUNCTION
                    je_mem_free(_m_array_of_components_buffer);

                    // Before chunk destruct, all entity in chunk must be free.
                    if (_m_free_entity_count != CHUNK_ENTITY_GROUP)
                        abort();
                }

            public:
                bool alloc_new_entity(entity_id_t* out_id, version_t* out_version) noexcept
                {
                    if (chunk_full())
                        return false;

                    for (size_t eid = 0; eid < CHUNK_ENTITY_GROUP; eid++)
                    {
                        if (!_m_entity_informs[eid].m_in_used.test_and_set())
                        {
                            --_m_free_entity_count;
                            *out_version = ++_m_entity_informs[eid].m_version;
                            *out_id = eid;
                            return true;
                        }
                    }
                    return false;
                }
                inline bool chunk_full()const noexcept
                {
                    return _m_free_entity_count == 0;
                }
                inline void* chunk_buffer() const noexcept
                {
                    return _m_array_of_components_buffer;
                }
                void destroy(entity_id_t eid, version_t ever)
                {
                    if (_m_entity_informs[eid].m_version == ever)
                    {
                        uint8_t old_mask = _m_entity_informs[eid].m_state.fetch_or(_entity_inform::entity_state_mask::DESTROYING);
                        if (!(old_mask & _entity_inform::entity_state_mask::DESTROYING))
                        {
                            ++_m_destroying_entity_count;
                            ++_m_arch_group->_m_destroying_entity_count;
                        }
                    }
                }
            public:
                chunk* last; // used for free list;
            };

            class entity
            {
                friend class arch_group;

                chunk* _m_storage_chunk;
                entity_id_t _m_entity_id;
                version_t   _m_version;
            public:
                inline void* _get_component_addr(size_t _component_offset)const noexcept
                {
                    static_assert(sizeof(size_t) == sizeof(void*), "size_t should have same length as void*");

                    return
                        reinterpret_cast<void*>(
                            reinterpret_cast<size_t>(
                                _m_storage_chunk->chunk_buffer()
                                ) + _component_offset);
                }

                template<typename ComponentT>
                inline ComponentT* get_component() const noexcept
                {
                    size_t offset =
                        _m_storage_chunk->_m_arch_group->get_component_offset_in_chunk<ComponentT>(_m_entity_id);

                    return (ComponentT*)_get_component_addr(offset);
                }

                void destroy()
                {
                    _m_storage_chunk->destroy(_m_entity_id, _m_version);
                }
            };

            struct type_info
            {
                typehash_t m_type_hash;
                size_t     m_type_size;

                using construct_func_t = void(*)(void*);
                using destruct_func_t = void(*)(void*);
                using copy_construct_func_t = void(*)(void*, const void*);
                using move_construct_func_t = void(*)(void*, void*);

                construct_func_t    m_construct_func;
                destruct_func_t     m_destruct_func;
                move_construct_func_t m_move_construct_func;

                size_t     m_chunk_size;
                size_t     m_offset;

                inline size_t get_component_offset(entity_id_t eid) const noexcept
                {
                    return m_offset + m_chunk_size * eid;
                }

                template<typename T>

            };

        private:


        private:
            std::atomic_size_t _m_adding_component_entity_count;
            std::atomic_size_t _m_removing_component_entity_count;
            std::atomic_size_t _m_destroying_entity_count;

            type_info* _m_components;
            size_t           _m_components_count;
            size_t           _m_chunk_size;

            std::atomic_int64_t _m_usable_entity_count;

            basic::atomic_list<chunk> _m_chunks;

        public:
            arch_group() = default;

            template<typename ComponentT, typename ... ComponentTs>
            void init_arch_group()
            {
                _m_components_count = 1 + sizeof...(ComponentTs);

                _component_info _typeids[] = {
                    {
                        basic::type_hash<ComponentT>(),
                        sizeof(ComponentT),
                        basic::default_construct_function<ComponentT>::constructor,
                        basic::destruct_function<ComponentT>::destructor,
                        basic::move_construct_function<ComponentT>::move,
                    },
                    {
                        basic::type_hash<ComponentTs>(),
                        sizeof(ComponentTs),
                        basic::default_construct_function<ComponentTs>::constructor,
                        basic::destruct_function<ComponentTs>::destructor,
                        basic::move_construct_function<ComponentTs>::move,
                    }...
                };
                std::sort(&_typeids[0], &_typeids[_m_components_count],
                    [](const _component_info& a, const _component_info& b)
                    {
                        return a.m_type_size < b.m_type_size;
                    }
                );

                // OK, Calculate offset now.
                size_t offset_in_chunk = 0;

                for (_component_info& _typeid : _typeids)
                {
                    _typeid.m_offset = basic::allign_size(offset_in_chunk, ALLIGN_BASE_COUNT);
                    _typeid.m_chunk_size = basic::allign_size(_typeid.m_type_size, ALLIGN_BASE_COUNT);

                    offset_in_chunk = _typeid.m_offset + _typeid.m_chunk_size * CHUNK_ENTITY_GROUP;
                }

                // OK, Ready, store it to index.
                _m_components = basic::create_new_n<_component_info>(_m_components_count);
                memcpy(_m_components, _typeids, sizeof(_typeids));

                _m_chunk_size = offset_in_chunk;
                _m_usable_entity_count = 0;
            }

            ~arch_group()
            {
                chunk* current_chunk = _m_chunks.pick_all();
                while (current_chunk)
                    current_chunk = _destroy_chunk(current_chunk);

                basic::destroy_free_n(_m_components, _m_components_count);
            }

        private:
            chunk* _create_new_chunk()
            {
                chunk* new_chunk = basic::create_new<chunk>(this);
                _m_chunks.add_one(new_chunk);

                _m_usable_entity_count += CHUNK_ENTITY_GROUP;
                return new_chunk;
            }
            chunk* _destroy_chunk(chunk* current)
            {
                chunk* last_chunk = current->last;
                basic::destroy_free(current);
                return last_chunk;
            }

            inline void _new_empty_entity(entity* entity)
            {
                while (true)
                {
                    if (--_m_usable_entity_count >= 0)
                    {
                        chunk* current_chunk = _m_chunks.peek();
                        while (current_chunk)
                        {
                            if (current_chunk->alloc_new_entity(&entity->_m_entity_id, &entity->_m_version))
                            {
                                entity->_m_storage_chunk = current_chunk;
                                return;
                            }
                        }

                        //  Should not execute here, because '_m_usable_entity_count'
                        // is not 0, but there is no free place for new_entity.
                        abort();
                    }
                    else
                        _m_usable_entity_count++;

                    _create_new_chunk();
                }
            }

            template<typename ComponentT>
            inline type_info* _find_component_info() const
            {
                for (size_t ci = 0; ci < _m_components_count; ci++)
                {
                    if (_m_components[ci].m_type_hash == basic::type_hash<ComponentT>())
                        return &_m_components[ci];
                }

                abort();
                return nullptr;
            }

        public:
            inline entity new_entity()
            {
                entity created_entity;
                _new_empty_entity(&created_entity);

                for (size_t ci = 0; ci < _m_components_count; ci++)
                {
                    size_t component_addr_offset =
                        _m_components[ci].get_component_offset(created_entity._m_entity_id);

                    _m_components[ci].m_construct_func(
                        created_entity._get_component_addr(component_addr_offset)
                    );
                }
                return created_entity;
            }

            template<typename ComponentT>
            inline size_t get_component_offset_in_chunk(entity_id_t eid) const noexcept
            {
                auto* component_info = _find_component_info<ComponentT>();
                return component_info->get_component_offset(eid);
            }
        };
        class type_factory
        {
            rw_mutex_t _m_arch_group_map_mx;
            std::unordered_map<typehash_t, arch_group*> _m_arch_group_map;

            rw_mutex_t _m_arch_group_map_mx;
            std::unordered_map<typehash_t, typehash_t> _m_arch_group_map;

            template<typename T>
            static arch_group::type_info find_or_register_type(const std::string_view & name)
            {

            }
        };
        class arch_group_manager
        {
            template<typename ... HashTs>
            inline static constexpr typehash_t _sorted_arch_hash(HashTs...hasht) noexcept
            {
                std::set<typehash_t> hashs = { ((typehash_t)hasht) ... };
                typehash_t result = 0;
                for (typehash_t hash : hashs)
                {
                    result *= result;
                    result += hash;
                }

                return result;
            }

            template<typename ... ComponentTs>
            inline static constexpr typehash_t _sorted_arch_hash() noexcept
            {
                // TODO: AVOID HASH COLLISION
                return _sorted_arch_hash(basic::type_hash<ComponentTs>()...);
            }
        private:
            rw_mutex_t _m_arch_group_map_mx;
            std::unordered_map<typehash_t, arch_group*> _m_arch_group_map;

        private:
            arch_group* _find_or_add_arch_group(typehash_t _typeshash)
            {
                do
                {
                    std::shared_lock sg1(_m_arch_group_map_mx);
                    auto fnd = _m_arch_group_map.find(_typeshash);
                    if (fnd != _m_arch_group_map.end())
                        return fnd->second;
                } while (0);

                jeecs::core::arch_group* new_arch = basic::create_new<jeecs::core::arch_group>();
                new_arch->init_arch_group<std::string, heyman>();

                std::lock_guard g1(_m_arch_group_map_mx);
            }
        public:

        };

    }

}

struct heyman
{
    heyman()
    {
        std::cout << "Helloworld~:" << this << std::endl;
    }
};

int main()
{
    while (true)
    {
        entity.destroy();
    }
    system("cls");

}
