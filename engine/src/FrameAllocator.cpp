#include "FrameAllocator.h"
#include <atomic>
#include <cstddef>
#include <cassert>

namespace FrameAllocator
{
    static char* gAllocation = nullptr;
    static size_t gAllocationSize = 0;
    std::atomic<int> gTopIndex = 0;

    size_t Align16(size_t x) 
    {
        return (x + 15) & ~size_t(15);
    }

    void BeginFrame()
    {
        gTopIndex.store(0);
    }
    
    void* Allocate(size_t size)
    {
        size = Align16(size);
        int index = gTopIndex.fetch_add(size);
        if (index >= gAllocationSize - 1)
        {
            assert(false && "frame allocator out of memory, increase size");
        }
        return &gAllocation[index];
    }

    void Init(size_t sizeBytes)
    {
        gAllocationSize = sizeBytes;
        gAllocation = new char[gAllocationSize];
    }
}

