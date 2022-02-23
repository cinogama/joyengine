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
    void* m_ecs_world_addr;
    game_world(void* ecs_world_addr)
        :m_ecs_world_addr(ecs_world_addr)
    {

    }

    void register_system_func_to_world(jeecs::game_system_function* sys_func)
    {
        je_ecs_world_register_system_func(m_ecs_world_addr, sys_func);
    }

    void update()
    {
        je_ecs_world_update(m_ecs_world_addr);
    }
};

struct game_system
{

    struct accessor_base {};

    struct read_last_frame_base :accessor_base {};
    struct read_updated_base :accessor_base {};
    struct write_base :accessor_base {};

    template<typename T>
    struct read : read_last_frame_base {
    public:
        void* _m_component_addr;
    public:
        T* operator ->() const noexcept { return (T*)_m_component_addr; }
        T& operator * () const noexcept { return *(T*)_m_component_addr; }
    };
    template<typename T>
    struct read_newest : read_updated_base {
    public:
        void* _m_component_addr;
    public:
        T* operator ->() const noexcept { return (T*)_m_component_addr; }
        T& operator * () const noexcept { return *(T*)_m_component_addr; }
    };
    template<typename T>
    struct write : write_base {
    public:
        void* _m_component_addr;
    public:
        T* operator ->() const noexcept { return (T*)_m_component_addr; }
        T& operator * () const noexcept { return *(T*)_m_component_addr; }
    };
public:


private:
    game_world* _m_game_world;

public:
    game_system(game_world* world)
        : _m_game_world(world)
    {

    }

    template<typename T>
    static constexpr jeecs::game_system_function::dependence_type depend_type()
    {
        if constexpr (std::is_pointer<T>::value || std::is_base_of<accessor_base, T>::value)
        {
            if constexpr (std::is_pointer<T>::value)
            {
                if constexpr (std::is_const<std::remove_pointer<T>::type>::value)
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

    template<typename ReturnT, typename ThisT, typename ... ArgTs>
    void register_system_func(ReturnT(ThisT::* system_func)(ArgTs ...))
    {
        auto per_entity_invoker = [this, system_func](const jeecs::game_system_function* sysfunc) {

            jeecs::basic::type_index_in_varargs<ArgTs...> tindexer;

            for (size_t arch_index = 0; arch_index < sysfunc->m_arch_count; ++arch_index)
            {
                void* current_chunk = je_arch_get_chunk(sysfunc->m_archs[arch_index].m_archtype);
                while (current_chunk)
                {
                    for (size_t entity_index = 0;
                        entity_index < sysfunc->m_archs[arch_index].m_entity_count_per_arch_chunk;
                        entity_index++)
                    {
                        ((static_cast<ThisT*>(this))->*system_func)(
                            sysfunc->m_archs[arch_index]
                            .get_component_accessor<ArgTs>(
                                current_chunk, entity_index, tindexer.index_of<ArgTs>()
                                )...
                            );
                    }

                    current_chunk = je_arch_next_chunk(current_chunk);
                }
            }
        };

        auto* game_system = jeecs::game_system_function::create(per_entity_invoker, sizeof...(ArgTs));
        std::vector<jeecs::game_system_function::typeid_dependence_pair> depends
            = { {
                    jeecs::typing::type_info::id<origin_component<ArgTs>::type>(),
                    depend_type<ArgTs>(),
                }... };

        game_system->set_depends(depends);
        _m_game_world->register_system_func_to_world(game_system);
    }
};

struct graphic_system : public game_system
{
    void rend_work(const position* pos, renderer* rend)
    {

    }

    graphic_system(game_world* world)
        :game_system(world)
    {
        register_system_func(&graphic_system::rend_work);
    }
};

int main()
{
    using namespace jeecs;

    game_world gw(je_ecs_world_create());
    graphic_system gs(&gw);

    jeecs::typing::typeid_t x[3] = {
        typing::type_info::id<position>(),
        typing::type_info::id<renderer>(),
        typing::INVALID_TYPE_ID
    };

    je_ecs_world_create_entity_with_components(gw.m_ecs_world_addr, nullptr, nullptr, nullptr,
        x);

    while (true)
        gw.update();

    /*
    auto* tinfo = jeecs::typing::type_info::of<jeecs::typing::type_info>("type_info");

    arch::arch_manager am;
    arch::command_buffer cb;

    auto entity_1 = am.create_an_entity_with_component({ typing::type_info::id<int>(), typing::type_info::id<std::string>() });
    cb.append_component(entity_1, typing::type_info::of<custom>());
    cb.update();
    */

}
