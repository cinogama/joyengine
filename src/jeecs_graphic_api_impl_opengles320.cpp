#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_GLES320_GAPI

#include "GLES3/gl32.h"

namespace jeecs::graphic::api::gles320
{

}

void jegl_using_opengles320_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("OpenglES v320 Graphic API not support now.");
}
#else
void jegl_using_opengles320_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("GLES320 not available.");
}
#endif