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
    float x, y, z;
};

struct renderer
{

};

namespace jeecs
{
    struct game_world
    {
        void* m_ecs_world_addr;
        game_world(void* ecs_world_addr)
            :m_ecs_world_addr(ecs_world_addr)
        {

        }

        jeecs::game_system_function* register_system_func_to_world(jeecs::game_system_function* sys_func)
        {
            je_ecs_world_register_system_func(m_ecs_world_addr, sys_func);
            return sys_func;
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

        template<typename ReturnT, typename ThisT, typename ... ArgTs>
        inline auto register_normal_invoker(ReturnT(ThisT::* system_func)(ArgTs ...))
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
                                    sysfunc->m_archs[arch_index]
                                    .get_component_accessor<ArgTs>(
                                        current_chunk, entity_index, tindexer.index_of<ArgTs>()
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

            gsys->set_depends(depends);

            return gsys;
        }

        template<typename ReturnT, typename ThisT, typename ET, typename ... ArgTs>
        inline auto register_normal_invoker_with_entity(ReturnT(ThisT::* system_func)(ET, ArgTs ...))
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
                                    sysfunc->m_archs[arch_index]
                                    .get_component_accessor<ArgTs>(
                                        current_chunk, entity_index, tindexer.index_of<ArgTs>()
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

            gsys->set_depends(depends);

            return gsys;
        }

        template<typename ReturnT, typename ThisT, typename ... ArgTs>
        inline jeecs::game_system_function* register_system_func(ReturnT(ThisT::* sysf)(ArgTs ...))
        {
            static_assert(sizeof...(ArgTs));

            jeecs::game_system_function::system_function_pak_t invoker;

            if constexpr (is_need_game_entity<ArgTs...>::value)
                return _m_game_world.register_system_func_to_world(register_normal_invoker_with_entity(sysf));
            else
                return _m_game_world.register_system_func_to_world(register_normal_invoker(sysf));

        }
    };

}
struct graphic_system : public jeecs::game_system
{
    void rend_work_1(jeecs::game_entity e, const position* pos)
    {
        jeecs::debug::log("rend_work_1=====");
        je_ecs_world_entity_add_component(get_world()->m_ecs_world_addr, &e, jeecs::typing::type_info::of<renderer>());
        je_ecs_world_entity_remove_component(get_world()->m_ecs_world_addr, &e, jeecs::typing::type_info::of<position>());

    }
    void rend_work_2(jeecs::game_entity e, const renderer* rend)
    {
        jeecs::debug::log("rend_work_2=====");
        je_ecs_world_entity_add_component(get_world()->m_ecs_world_addr, &e, jeecs::typing::type_info::of<position>());
        je_ecs_world_entity_remove_component(get_world()->m_ecs_world_addr, &e, jeecs::typing::type_info::of<renderer>());
    }

    graphic_system(jeecs::game_world world)
        :game_system(world)
    {
        while (true)
        {
            auto sys1 = register_system_func(&graphic_system::rend_work_1);
            auto sys2 = register_system_func(&graphic_system::rend_work_2);

            world.update();
            world.update();
            world.update();

            je_ecs_world_unregister_system_func(world.m_ecs_world_addr, sys1);
            je_ecs_world_unregister_system_func(world.m_ecs_world_addr, sys2);

            world.update();
            world.update();
            world.update();
        }
    }
};

int main()
{
    using namespace jeecs;

    constexpr bool xx = game_system::is_need_game_entity<jeecs::game_entity>::value;

    game_world gw(je_ecs_world_create());
   
    jeecs::typing::typeid_t x[3] = {
        typing::type_info::id<position>(),
        typing::INVALID_TYPE_ID
    };

    jeecs::game_entity ge;
    je_ecs_world_create_entity_with_components(gw.m_ecs_world_addr, &ge, x);

    graphic_system gs(gw);

    size_t index = 0;
    bool flag = true;
    while (true)
    {
        gw.update();
    }


    /*
    auto* tinfo = jeecs::typing::type_info::of<jeecs::typing::type_info>("type_info");

    arch::arch_manager am;
    arch::command_buffer cb;

    auto entity_1 = am.create_an_entity_with_component({ typing::type_info::id<int>(), typing::type_info::id<std::string>() });
    cb.append_component(entity_1, typing::type_info::of<custom>());
    cb.update();
    */

}
