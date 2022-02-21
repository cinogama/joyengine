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

struct ecs_system
{
    template<typename ReturnT , typename ThisT, typename ... ArgTs>
    void register_system_func(ReturnT(ThisT::* system_func)(ArgTs ...))
    {

    }
};

struct graphic_system : public ecs_system
{
    void xx1(const position* pos, const renderer * renderer)
    {
        
    }

    graphic_system(void * world)
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
