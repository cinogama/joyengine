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

        jeal_using_device(jeal_get_all_devices()[0]);
        auto* source = jeal_open_source();
        auto* buffer = jeal_load_buffer_from_wav("Chipmaster.wav", true);

        jeal_source_set_buffer(source, buffer);
        jeal_source_play(source);

        enrty::module_entry();
        {
            jedbg_editor();
            je_clock_sleep_for(1);
        }
        enrty::module_leave();

        jeal_source_stop(source);
        jeal_close_source(source);
        jeal_close_buffer(buffer);
    }
    je_finish();
}
