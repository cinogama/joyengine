# librscene
#
cmake_minimum_required (VERSION 3.8)

include_directories("../include")
include_directories("../3rd/woolang/include")
include_directories("../3rd/glew/include")
include_directories("../3rd/glfw/include")
include_directories("../3rd/stb")
include_directories("../3rd/imgui")
include_directories("../3rd/imgui/backends")
include_directories("../3rd/imgui/misc/cpp")

file(GLOB_RECURSE joyecs_srcs_cpp *.cpp)
file(GLOB_RECURSE joyecs_srcs_hpp *.hpp)


set(imgui_glfw_gl3_src	"../3rd/imgui/backends/imgui_impl_glfw.cpp"
						"../3rd/imgui/backends/imgui_impl_glfw.h"
						"../3rd/imgui/backends/imgui_impl_opengl3.cpp"
						"../3rd/imgui/backends/imgui_impl_opengl3.h"
						"../3rd/imgui/backends/imgui_impl_opengl3_loader.h"
						"../3rd/imgui/imconfig.h"
						"../3rd/imgui/imgui.cpp"
						"../3rd/imgui/imgui_draw.cpp"
						"../3rd/imgui/imgui.h"
						"../3rd/imgui/imgui_internal.h"
						"../3rd/imgui/imgui_tables.cpp"
						"../3rd/imgui/imgui_widgets.cpp"
						"../3rd/imgui/misc/cpp/imgui_stdlib.cpp"
						"../3rd/imgui/misc/cpp/imgui_stdlib.h")

if (WIN32)
	list(APPEND jeecs_system_libs OpenGL32)
else()
	list(APPEND jeecs_system_libs pthread dl)
endif()

if (${BUILD_SHARED_LIBS})
	add_definitions(-DRS_SHARED_LIB)
	add_library(joyecs SHARED ${joyecs_srcs_cpp} ${joyecs_srcs_hpp} ${imgui_glfw_gl3_src})
	if(UNIX)
		add_compile_options(-Wall -rdynamic)
endif()
else()
	add_definitions(-DRS_STATIC_LIB)
	add_library(joyecs STATIC ${joyecs_srcs_cpp} ${joyecs_srcs_hpp} ${imgui_glfw_gl3_src})
endif()


set_target_properties(joyecs PROPERTIES OUTPUT_NAME libjoyecs)
set_target_properties(joyecs PROPERTIES PREFIX "")

target_link_libraries(joyecs ${jeecs_system_libs})
target_link_libraries(joyecs woolang glew_s glfw)
