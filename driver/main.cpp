/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/

#include "jeecs.hpp"

extern "C"
{
    JE_EXPORT int NvOptimusEnablement = 0x00000001;
    JE_EXPORT int AmdPowerXpressRequestHighPerformance = 0x00000001;
}

int main(int argc, char** argv)
{
    using namespace jeecs;

    je_init(argc, argv);

    do
    {
        jeecs::typing::type_unregister_guard guard;
        entry::module_entry(&guard);
        {
            je_main_script_entry();
        }
        entry::module_leave(&guard);

    } while (0);

    je_finish();
}
