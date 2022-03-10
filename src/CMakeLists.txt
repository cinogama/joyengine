# librscene
#
cmake_minimum_required (VERSION 3.8)

include_directories("../include")
include_directories("../3rd/restorablescene/include")
include_directories("../3rd/glew/include")
include_directories("../3rd/glfw/include")

file(GLOB_RECURSE joyecs_srcs *.cpp)

if (WIN32)
	list(APPEND jeecs_system_libs OpenGL32)
else()
	list(APPEND jeecs_system_libs pthread dl)
endif()

if (${BUILD_SHARED_LIBS})
	add_definitions(-DRS_SHARED_LIB)
else()
	add_definitions(-DRS_STATIC_LIB)
endif()

add_executable(joyecs ${joyecs_srcs})
target_link_libraries(joyecs ${jeecs_system_libs})
target_link_libraries(joyecs rscene glew_s glfw)
