/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

int main(int argc, char** argv)
{
    je_init(argc, argv);
    {
        using namespace jeecs;
        using namespace std;
        enrty::module_entry();
        {
            jedbg_main_script_entry();
        }
        enrty::module_leave();
    }
    je_finish();
}
