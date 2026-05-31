#pragma once
#include <cstddef>

namespace FrameAllocator
{
    void BeginFrame();
    void EndFrame();
    void* Allocate(size_t size);
    void Init(size_t sizeBytes);
    void DeInit();
}
