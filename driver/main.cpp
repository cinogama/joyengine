/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#ifdef JE_OS_WINDOWS
extern "C" __declspec(dllexport) int NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0x00000001;
#endif

int main(int argc, char** argv)
{
    je_init(argc, argv);
    {
        using namespace jeecs;
        using namespace std;

        jeecs::enrty::module_entry();
        {
            jedbg_editor();
            je_clock_sleep_for(1);
        }
        jeecs::enrty::module_leave();
    }
    je_finish();
}
