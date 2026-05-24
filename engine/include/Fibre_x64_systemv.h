#pragma once
#include <stdint.h>
#include <cstddef>
#define FiberStackSize (1024 * 64)

struct Job;
namespace Jobs
{
    struct  Counter;
    struct WorkerThread;
}

typedef void (*ExecuteFn)(void*, Jobs::WorkerThread*);

typedef void (*JobWrapperFn)(ExecuteFn, void* pUser, Jobs::Counter* pCounter, Jobs::WorkerThread*);

struct FibreContext
{
    uint64_t rsp;       // Stack Pointer
    uint64_t rbx;       // Callee-saved registers
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;       // Instruction Pointer / Return Address

    // only used on intial "kick off" of job
    uint64_t rdi;       
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
};

struct Fiber
{
    size_t id;
    char* stack;
    FibreContext ctx;
    Job* currentJob = nullptr;
    Fiber* nextWaiter = nullptr; // for wait list linking
    Fiber()
        :stack(new char[FiberStackSize])
    {
        static size_t sId = 0;
        id = sId++;
    }
    ~Fiber()
    {
        delete[] stack;
    }
};

extern "C" {
    void FibreSwitch(FibreContext* pOld, FibreContext* pNew);
    void FibreSwitchNewJob(FibreContext* pOld, FibreContext* pNew);
}

void LoadNewJobIntoFiber(FibreContext* pCtx, 
    ExecuteFn, 
    void* pUser, 
    JobWrapperFn pWrapper, 
    Jobs::Counter* pCounter,
    Jobs::WorkerThread* pThread
);
void ResetFibreStack(Fiber* pCtx);

