/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <iostream>

#ifdef _WIN32
#   ifndef NDEBUG
//      Woolang's compiler is too complex, need more stack space in debug.
#       pragma comment(linker, "/STACK:268435456")
#   endif
#endif

int main(int argc, char** argv)
{
    je_init(argc, argv);
    atexit(je_finish);
    at_quick_exit(je_finish);

    using namespace jeecs;
    using namespace std;

    jeecs::enrty::module_entry();

    atexit(jeecs::enrty::module_leave);
    at_quick_exit(jeecs::enrty::module_leave);

    jedbg_editor();

    je_clock_sleep_for(1);
}
