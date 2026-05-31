cmake_minimum_required(VERSION 3.16)
project(3DEngine)
project(3DEngine CXX ASM)

set (CMAKE_CXX_STANDARD 20)
set(CMAKE_ENABLE_EXPORTS ON)

add_subdirectory(scheduler)
add_subdirectory(test)

