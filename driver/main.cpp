/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/

#include "jeecs/engine.hpp"
#include "jeecs/file.hpp"

extern "C"
{
    JE_EXPORT int NvOptimusEnablement = 0x00000001;
    JE_EXPORT int AmdPowerXpressRequestHighPerformance = 0x00000001;
}

int main(int argc, char** argv)
{
    using namespace jeecs;

    game_engine_context context(argc, argv);

    // The engine only records the user data path during bootup; the entry
    // point creates it once the final path is settled (Android/WebGL ports
    // do the same after setting their own path).
    (void)je_file_user_path_assure();

    context.loop();
}
