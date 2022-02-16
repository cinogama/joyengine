/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#include "jeecs.hpp"

#include <iostream>

int main()
{
    auto* tinfo = jeecs::typing::type_info::of<jeecs::typing::type_info>("type_info");

    jeecs::typing::type_info::unregister_all_type_in_shutdown();

}
