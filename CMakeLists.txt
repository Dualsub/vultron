
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Vultron)

add_subdirectory(Vultron)
add_subdirectory(Testbed)
add_subdirectory(Vultron/tools/env_map_gen)