cmake_minimum_required(VERSION 3.0)
project(Var)
set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_FLAGS "-std=c++17 -g -Wall")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

include_directories(${PROJECT_SOURCE_DIR})
# include_directories(${PROJECT_SOURCE_DIR}/net)

add_subdirectory(src)
add_subdirectory(test)
# add_subdirectory(util)
add_subdirectory(net)
# add_subdirectory(service)