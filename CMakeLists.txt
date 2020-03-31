cmake_minimum_required(VERSION 3.0)
project(libjoycon)

set(CMAKE_CXX_STANDARD 11)

if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
message(STATUS "enable debug option")
add_definitions("-D__debug__=1")
endif()

option(WITH_HIDAPI "use hidapi" off)

set(INCLUDE
    include
)
set(SOURCE
    src/session2.cc
    src/controller.cc
)
set(LINKS
    pthread
)

if(WITH_HIDAPI)
    #list(APPEND INCLUDE third_party/hidapi)
    #list(APPEND SOURCE src/hidapi/controller)
    #list(APPEND LINKS hidapi)
    #add_subdirectory(third_party/hidapi)
endif(WITH_HIDAPI)

include_directories(${INCLUDE})
add_compile_options("-fPIC")

add_library(joycon SHARED ${SOURCE})
target_link_libraries(joycon ${LINKS})

add_executable(test_cpp test.cc ${SOURCE})
target_link_libraries(test_cpp ${LINKS})
