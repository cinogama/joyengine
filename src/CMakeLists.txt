# librscene
#
cmake_minimum_required (VERSION 3.16)

include_directories("../include")
include_directories("../3rd/woolang/include")
include_directories("../3rd/glew/include")
include_directories("../3rd/glfw/include")
include_directories("../3rd/stb")
include_directories("../3rd/imgui")
include_directories("../3rd/imgui/backends")
include_directories("../3rd/imgui/misc/cpp")
include_directories("../3rd/box2d/include")

include_directories("../3rd/openal-soft/include")

file(GLOB_RECURSE joyecs_srcs_cpp *.cpp)
file(GLOB_RECURSE joyecs_srcs_hpp *.hpp)

set(imgui_srcs	"../3rd/imgui/imconfig.h"
				"../3rd/imgui/imgui.cpp"
				"../3rd/imgui/imgui_draw.cpp"
				"../3rd/imgui/imgui.h"
				"../3rd/imgui/imgui_internal.h"
				"../3rd/imgui/imgui_tables.cpp"
				"../3rd/imgui/imgui_widgets.cpp"
				"../3rd/imgui/misc/cpp/imgui_stdlib.cpp"
				"../3rd/imgui/misc/cpp/imgui_stdlib.h")

option(JE4_ENABLE_OPENGL330_GAPI	"Enable OpenGL 3.3 Graphic API"		OFF)
option(JE4_ENABLE_DX11_GAPI			"Enable DX11 Graphic api"			OFF)
option(JE4_ENABLE_OPENGLES320_GAPI	"Enable OpenGLES 3.2 Graphic API"	OFF)
option(JE4_ENABLE_VULKAN110_GAPI	"Enable Vulkan 1.1 Graphic API"		OFF)
option(JE4_ENABLE_METAL_GAPI		"Enable Metal Graphic API"			OFF)

if (JE4_ENABLE_OPENGL330_GAPI)
	add_definitions(-DJE_ENABLE_GL330_GAPI)
	list(APPEND imgui_srcs # OPENGL330
			"../3rd/imgui/backends/imgui_impl_glfw.cpp"
			"../3rd/imgui/backends/imgui_impl_glfw.h"
			"../3rd/imgui/backends/imgui_impl_opengl3.cpp"
			"../3rd/imgui/backends/imgui_impl_opengl3.h"
			"../3rd/imgui/backends/imgui_impl_opengl3_loader.h")
endif()
if (JE4_ENABLE_OPENGL330_GAPI AND WIN32)
	add_definitions(-DJE_ENABLE_DX11_GAPI)
	list(APPEND imgui_srcs # DX11
			"../3rd/imgui/backends/imgui_impl_win32.cpp"
			"../3rd/imgui/backends/imgui_impl_win32.h"
			"../3rd/imgui/backends/imgui_impl_dx11.cpp"
			"../3rd/imgui/backends/imgui_impl_dx11.h")
endif()
if (JE4_ENABLE_OPENGLES320_GAPI)
	add_definitions(-DJE_ENABLE_GLES320_GAPI)
endif()
if (JE4_ENABLE_VULKAN110_GAPI)
	add_definitions(-DJE_ENABLE_VK110_GAPI)
endif()
if (JE4_ENABLE_METAL_GAPI)
	add_definitions(-DJE_ENABLE_METAL_GAPI)
endif()

if (WIN32)
	list(APPEND jeecs_system_libs OpenGL32)
else()
	list(APPEND jeecs_system_libs pthread dl)
endif()

if (${BUILD_SHARED_LIBS})
	add_definitions(-DWO_SHARED_LIB)
	add_library(joyecs SHARED ${joyecs_srcs_cpp} ${joyecs_srcs_hpp} ${imgui_srcs})
	if(UNIX)
		add_compile_options(-Wall -rdynamic)
endif()
else()
	add_definitions(-DWO_STATIC_LIB)
	add_library(joyecs STATIC ${joyecs_srcs_cpp} ${joyecs_srcs_hpp} ${imgui_srcs})
endif()

set_target_properties(joyecs PROPERTIES OUTPUT_NAME libjoyecs)
set_target_properties(joyecs PROPERTIES PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set_target_properties(joyecs PROPERTIES RELEASE_POSTFIX "")
	set_target_properties(joyecs PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set_target_properties(joyecs PROPERTIES RELEASE_POSTFIX "32")
	set_target_properties(joyecs PROPERTIES DEBUG_POSTFIX "32_debug")
else()
	message(FATAL_ERROR "Unknown platform")
endif()

if (NOT WIN32)
	target_compile_options (glew_s PRIVATE -fPIC)
	target_compile_options (glfw PRIVATE -fPIC)
	target_compile_options (box2d PRIVATE -fPIC)
endif()
target_link_libraries(joyecs ${jeecs_system_libs})
target_link_libraries(joyecs woolang glew_s glfw box2d OpenAL)
