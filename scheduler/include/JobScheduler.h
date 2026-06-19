#pragma once
#include "Fibre_x64_systemv.h"
#include <array>

namespace Jobs {
    struct Counter;
    struct WorkerThread;
    
    struct Job
    {
        ExecuteFn execute;;
        void* userData;

        Counter* counter;

        Job* next;
    };

    enum class JobPriority
    {
        Undefined,
        High,
        Medium,
        Low,
        NumberOfPriorities
    };

    struct JobDecl
    {
        JobPriority priority;
        Job job;
    };

    void InitScheduler(int numWorkers);
    void KillScheduler();
    void WaitForCounter(WorkerThread* pThisThread, Counter* pCtr);
    Counter* RunJobs(struct JobDecl* pJobs, int numJobs);
    void WaitForCounterOSThread(Counter* pCtr, int sleepMS=0);
};