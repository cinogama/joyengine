/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/

#include <iostream>
#include <algorithm>
#include <typeinfo>

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

    namespace core
    {
        class entity
        {
        public:
            using entity_id_t = size_t;
            using version_t = size_t;
        private:
            entity_id_t _m_entity_id_in_pool;
            version_t   _m_version_id;
        };

        class entity_pool;

        class arch_group_list
        {
            JECS_DISABLE_MOVE_AND_COPY(arch_group_list);
        public:
            using typehash_t = decltype(typeid(int).hash_code());

        private:
            struct _component_info
            {
                typehash_t m_type_hash;
                size_t     m_type_size;
                size_t     m_offset;
            };

        private:
            _component_info* _m_components;
            size_t           _m_components_count;
            size_t           _m_entity_size;
        public:
            arch_group_list() = default;

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
                constexpr size_t ALLIGN_BASE_COUNT = 8;

                size_t current_offset = 0;

                for (_component_info& _typeid : _typeids)
                {
                    size_t align_base = std::min(_typeid.m_type_size, ALLIGN_BASE_COUNT);

                    size_t allign_place = (current_offset / align_base) * align_base;
                    _typeid.m_offset =
                        allign_place + ((allign_place == current_offset) ? 0 : 1) * align_base;
                    current_offset = _typeid.m_offset + _typeid.m_type_size;
                }

                // OK, Ready, store it to index.
                _m_components = new (je_mem_alloc(sizeof(_typeids)))_component_info[_m_components_count];
                memcpy(_m_components, _typeids, sizeof(_typeids));

                _m_entity_size = (current_offset / ALLIGN_BASE_COUNT) * ALLIGN_BASE_COUNT;
                _m_entity_size += ((_m_entity_size == current_offset) ? 0 : 1) * ALLIGN_BASE_COUNT;
            }

            ~arch_group_list()
            {
                for (size_t index = 0; index < _m_components_count; index++)
                    _m_components[index].~_component_info();
                je_mem_free(_m_components);
            }

            size_t get_mem_offset_of_component_in_entity(typehash_t typehash) const noexcept
            {

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
    jeecs::core::arch_group_list a;
    a.init_arch_group<char, sz4, sz8, sz12>();

}
