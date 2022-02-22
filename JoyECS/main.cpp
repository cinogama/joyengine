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
        register_system_func(xx1);
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
