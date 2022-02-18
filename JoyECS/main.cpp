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

int main()
{
    using namespace jeecs;

    auto* tinfo = jeecs::typing::type_info::of<jeecs::typing::type_info>("type_info");

    arch::arch_manager am;
    auto entity_2 = am.create_an_entity_with_component({ typing::type_info::id<int>() });
    auto entity_3 = am.create_an_entity_with_component({ typing::type_info::id<std::string>() });
}
