add_executable(MandelbrotTest main.cpp)
find_package(SDL2 REQUIRED)

target_link_libraries(MandelbrotTest PRIVATE engine)
target_link_libraries(MandelbrotTest PRIVATE SDL2::SDL2)