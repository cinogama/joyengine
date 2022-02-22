/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#include "jeecs.hpp"

#include <iostream>



struct custom
{
    int random_value;
    custom()
    {
        srand(time(0));
        random_value = rand();

        std::cout << "random_value : " << random_value << std::endl;
    }
};

struct position
{

};

struct renderer
{

};

struct game_world
{
    void register_system_func_to_world()
    {

    }
};

struct game_system
{
    template<typename T>
    struct accessor_base
    {
    private:
        T* _m_component_addr;
    public:
        T* operator ->() const noexcept { return _m_component_addr; }
        T& operator * () const noexcept { return *_m_component_addr; }
    };

    struct read_last_frame_base {};
    struct read_updated_base {};
    struct write_base {};

    template<typename T>
    struct read : read_last_frame_base, accessor_base<T> { static_assert(sizeof(read) == sizeof(T*)); };
    template<typename T>
    struct read_newest : read_updated_base, accessor_base<T> { static_assert(sizeof(read_newest) == sizeof(T*)); };
    template<typename T>
    struct write : write_base, accessor_base<T> { static_assert(sizeof(write) == sizeof(T*)); };
public:
    struct game_system_function
    {
        struct arch_index_info
        {
            size_t m_rw_component_count;
            size_t m_entity_count_per_arch_chunk;

            void** m_component_mem_begin_offsets;
            size_t* m_component_sizes;
            void** m_entity_meta_indexer;

            template<typename T>
            T get_component_accessor(size_t eid, size_t cid) const noexcept
            {
                return reinterpret_cast<T>((uint8_t*)(m_component_mem_begin_offsets[cid])
                    + m_component_sizes[cid] * eid);
            }
        };

        arch_index_info* m_archs;
        size_t m_arch_count;
    };

private:
    game_world* _m_game_world;

public:
    game_system(game_world* world)
        : _m_game_world(_m_game_world)
    {

    }

    template<typename ReturnT, typename ThisT, typename ... ArgTs>
    void register_system_func(ReturnT(ThisT::* system_func)(ArgTs ...))
    {
        // 1. Anylize depend
        auto invoker = [system_func](void* _this) {
            game_system_function gsf;

            jeecs::basic::type_index_in_varargs<ArgTs...> tindexer;
            static_assert(sizeof...(ArgTs) == 2);

            for (size_t arch_index = 0; arch_index < gsf.m_arch_count; ++arch_index)
            {
                for (size_t entity_index = 0;
                    entity_index < gsf.m_archs[arch_index].m_entity_count_per_arch_chunk;
                    entity_index++)
                {
                    // TODO: GET CHUNK! HERE IS NO CHUNK GETTER!!!
                    (((ThisT*)_this)->*system_func)(
                        gsf.m_archs[arch_index]
                        .get_component_accessor<ArgTs>(entity_index, tindexer.index_of<ArgTs>())...
                    );

                }
            }
        };
    }
};

struct graphic_system : public game_system
{
    void xx1(const position* pos, const renderer* renderer)
    {

    }

    graphic_system(game_world* world)
        :game_system(world)
    {
        register_system_func(&graphic_system::xx1);
    }
};

int main()
{
    using namespace jeecs;


    /*
    auto* tinfo = jeecs::typing::type_info::of<jeecs::typing::type_info>("type_info");

    arch::arch_manager am;
    arch::command_buffer cb;

    auto entity_1 = am.create_an_entity_with_component({ typing::type_info::id<int>(), typing::type_info::id<std::string>() });
    cb.append_component(entity_1, typing::type_info::of<custom>());
    cb.update();
    */

}
