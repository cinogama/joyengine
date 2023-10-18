/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

extern "C" 
{
    JE_EXPORT int NvOptimusEnablement = 0x00000001;
    JE_EXPORT int AmdPowerXpressRequestHighPerformance = 0x00000001;
}


int main(int argc, char** argv)
{
    je_init(argc, argv);
    {
        using namespace jeecs;
        using namespace std;
        enrty::module_entry();
        {
            je_main_script_entry();
        }
        enrty::module_leave();
    }
    je_finish();
}
