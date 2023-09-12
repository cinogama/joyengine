#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_OS_WINDOWS

#include <D3D11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "winmm.lib")

#include "imgui_impl_dx11.h"

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("DirectX 11 Graphic API not support now.");
}

#else

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("Current platform not support dx11, try using opengl3 instead.");
}

#endif