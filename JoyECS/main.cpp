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
    }

    namespace core
    {
        using entity_id_t = size_t;
        using version_t = size_t;
        using atomic_version_t = std::atomic_size_t;

        constexpr entity_id_t INVALID_ENTITY_ID = SIZE_MAX;

        class arch_group
        {
            JECS_DISABLE_MOVE_AND_COPY(arch_group);
        public:
            constexpr static size_t CHUNK_ENTITY_GROUP = 32;
            constexpr static size_t ALLIGN_BASE_COUNT = 8;
        public:
            using typehash_t = decltype(typeid(int).hash_code());

            friend class chunk;

            class chunk
            {
                JECS_DISABLE_MOVE_AND_COPY(chunk);

                struct _entity_inform
                {
                    std::atomic_flag m_in_used = {};
                    atomic_version_t m_version = 0;
                };

                // A chunk will store AoC, C will allign with 8 bytes.
                // 1 Chunk will store CHUNK_ENTITY_GROUP entity(s)

                std::atomic_size_t  _m_free_entity_count;
                arch_group* _m_arch_group;

                _entity_inform      _m_entity_informs[CHUNK_ENTITY_GROUP];
                void* _m_array_of_components_buffer;

            public:
                chunk(arch_group* _arch_group)
                    : last(nullptr)
                    , _m_free_entity_count(CHUNK_ENTITY_GROUP)
                    , _m_arch_group(_arch_group)
                {
                    _m_array_of_components_buffer = je_mem_alloc(_arch_group->_m_chunk_size);
                }
                ~chunk()
                {
                    // ATTENTION: NEED DO DESSTRUCT FUNCTION
                    je_mem_free(_m_array_of_components_buffer);
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
            public:

                chunk* last; // used for free list;
            };

            struct entity
            {
                chunk*      _m_storage_chunk;
                entity_id_t _m_entity_id;
                version_t   _m_version;
            };

        private:
            struct _component_info
            {
                typehash_t m_type_hash;
                size_t     m_type_size;

                size_t     m_chunk_size;
                size_t     m_offset;
            };

        private:
            _component_info* _m_components;
            size_t           _m_components_count;
            size_t           _m_chunk_size;

            std::atomic<> _m_usable_entity_count;

            basic::atomic_list<chunk> _m_chunks;

        public:
            arch_group() = default;

            template<typename ComponentT, typename ... ComponentTs>
            void init_arch_group()
            {
                _m_components_count = 1 + sizeof...(ComponentTs);

                _component_info _typeids[] = {
                    {
                        typeid(ComponentT).hash_code(),
                        sizeof(ComponentT),
                    },
                    {
                        typeid(ComponentTs).hash_code(),
                        sizeof(ComponentTs),
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
        public:
            entity new_entity()
            {
                while (true)
                {
                    if (--_m_usable_entity_count >= 0)
                    {
                        chunk* current_chunk = _m_chunks.peek();
                        while (current_chunk)
                        {
                            entity_id_t eid;
                            version_t   ever;
                            if (current_chunk->alloc_new_entity(&eid, &ever))
                                return entity{ current_chunk, eid ,ever };
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
        };
    }


    class entity
    {

    };
}

struct sz4
{
    int m1;
};

struct sz8
{
    double m1;
};

struct sz12
{
    int m1;
    int m2;
    int m3;
};

struct xx
{
    char m0;
    sz4 m1;
    sz8 m2;
    sz12 m3;
};


int main()
{
    jeecs::core::arch_group a;
    a.init_arch_group<char, sz4, sz8, sz12>();
    
    auto entity = a.new_entity();
    system("cls");

}
