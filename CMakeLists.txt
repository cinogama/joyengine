# JoyECS
#
cmake_minimum_required (VERSION 3.8)

project ("JoyECS")
set(CMAKE_CXX_STANDARD 17)

if(MSVC)
	add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/source-charset:utf-8>")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

add_subdirectory("3rd/glfw")
add_subdirectory("3rd/glew/build/cmake")
add_subdirectory("3rd/restorablescene")
add_subdirectory("src")