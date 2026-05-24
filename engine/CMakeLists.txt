add_library(engine SHARED
    src/JobScheduler.cpp
    src/Fibre_x64_systemv.S
    src/Fibre_x64_systemv_c.cpp
    src/FrameAllocator.cpp
)
target_include_directories(engine PRIVATE
    include
)

