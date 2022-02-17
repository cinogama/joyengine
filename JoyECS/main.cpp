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
    auto* tinfo = jeecs::typing::type_info::of<jeecs::typing::type_info>("type_info");

    jeecs::arch::arch_type at({
        jeecs::typing::type_info::id<jeecs::typing::type_info>(),
        jeecs::typing::type_info::id<std::string>(),
        jeecs::typing::type_info::id<custom>(),
        });

    auto entity = at.instance_entity();
    custom * cs = entity.get_component<custom>();
    std::string* css = entity.get_component<std::string>();

    jeecs::typing::type_info::unregister_all_type_in_shutdown();

}
