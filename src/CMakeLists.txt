# librscene
#
cmake_minimum_required (VERSION 3.8)

include_directories("../include")
include_directories("../3rd/restorablescene/include")
include_directories("../3rd/glew/include")
include_directories("../3rd/glfw/include")
include_directories("../3rd/stb")

file(GLOB_RECURSE joyecs_srcs *.cpp)

if (WIN32)
	list(APPEND jeecs_system_libs OpenGL32)
else()
	list(APPEND jeecs_system_libs pthread dl)
endif()

if (${BUILD_SHARED_LIBS})
	add_definitions(-DRS_SHARED_LIB)
	add_library(joyecs SHARED ${joyecs_srcs})
	if(UNIX)
		add_compile_options(-Wall -rdynamic)
endif()
else()
	add_definitions(-DRS_STATIC_LIB)
	add_library(joyecs STATIC ${joyecs_srcs})
endif()



set_target_properties(joyecs PROPERTIES OUTPUT_NAME libjoyecs)
set_target_properties(joyecs PROPERTIES PREFIX "")

target_link_libraries(joyecs ${jeecs_system_libs})
target_link_libraries(joyecs rscene glew_s glfw)
