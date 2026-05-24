#pragma once

namespace FrameAllocator
{
    void BeginFrame();
    void EndFrame();
    void* Allocate(int size);
    void Init();
}
