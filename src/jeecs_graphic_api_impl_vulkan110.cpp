#define JE_IMPL
#include "jeecs.h"

#ifdef JE_ENABLE_VK110_GAPI

namespace jeecs::graphic::api::vk110
{

}

void jegl_using_vulkan110_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("Vulkan Graphic API not support now.");
}
#else
void jegl_using_vulkan110_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("VK110 not available.");
}
#endif