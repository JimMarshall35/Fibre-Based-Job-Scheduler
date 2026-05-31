#include "Fibre_x64_systemv.h"
#include <iostream>
#include <cassert>

void LoadNewJobIntoFiber(FibreContext* pCtx, ExecuteFn execute, void* pUser, JobWrapperFn pWrapper, Jobs::Counter* pCounter, Jobs::WorkerThread* pThread)
{
    pCtx->rip = reinterpret_cast<uint64_t>(pWrapper);
    pCtx->rdi = reinterpret_cast<uint64_t>(execute);
    pCtx->rsi = reinterpret_cast<uint64_t>(pUser);
    pCtx->rdx = reinterpret_cast<uint64_t>(pCounter);
    pCtx->rcx = reinterpret_cast<uint64_t>(pThread);
}

void ResetFibreStack(Fiber* pCtx)
{
    char* stackTop = reinterpret_cast<char*>(pCtx->stack) + FiberStackSize;
    pCtx->ctx.rsp = reinterpret_cast<uint64_t>(stackTop);
    assert(pCtx->ctx.rsp % 16 == 0);
}
