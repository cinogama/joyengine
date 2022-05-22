cmake_minimum_required (VERSION 3.8)

include_directories("../include")
include_directories("../3rd/restorablescene/include")

file(GLOB_RECURSE jedriver_src *.cpp)

add_executable(jedriver ${jedriver_src})

target_link_libraries(jedriver joyecs)
