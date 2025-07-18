#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_METAL_GAPI
namespace jeecs::graphic::api::metal
{

}

void jegl_using_metal_apis(jegl_graphic_api *write_to_apis)
{
    jeecs::debug::logfatal("METAL Graphic API not support now.");
}
#else
void jegl_using_metal_apis(jegl_graphic_api *write_to_apis)
{
    jeecs::debug::logfatal("METAL not available.");
}
#endif