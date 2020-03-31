cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 11)
if (ANDROID)
    add_definitions("-D__android__=1")
endif ()

message("build type is ${CMAKE_BUILD_TYPE}")
if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "enable debug option")
    add_definitions("-D__debug__=1")
endif ()

set(LIBJOYCON "..")
include_directories("${LIBJOYCON}/include")
aux_source_directory("${LIBJOYCON}/src" src)
add_library(joycon SHARED joycon.cc ${src})
find_library(log-lib log)
target_link_libraries(joycon ${log-lib})