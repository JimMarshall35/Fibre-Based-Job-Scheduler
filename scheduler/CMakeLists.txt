add_library(scheduler SHARED
    src/JobScheduler.cpp
    src/Fibre_x64_systemv.S
    src/Fibre_x64_systemv_c.cpp
    src/FrameAllocator.cpp
)
target_include_directories(scheduler PUBLIC
    include
)

set_property(TARGET scheduler PROPERTY CXX_STANDARD 20)

