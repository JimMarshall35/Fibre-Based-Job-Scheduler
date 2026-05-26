cmake_minimum_required(VERSION 3.16)
project(3DEngine)
project(3DEngine CXX ASM)

set (CMAKE_CXX_STANDARD 20)

add_subdirectory(engine)
add_subdirectory(test)
add_subdirectory(mandelbrot_test)

