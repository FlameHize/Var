cmake_minimum_required(VERSION 3.0)
project(Var)
set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_FLAGS "-std=c++17 -g -Wall -fno-rtti")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

include_directories(${PROJECT_SOURCE_DIR})

add_subdirectory(metric)
add_subdirectory(test)
add_subdirectory(net)