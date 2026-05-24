#include "Fibre_x64_systemv.h"

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
        Low
    };

    struct JobDecl
    {
        JobPriority priority;
        Job job;
    };

    void InitScheduler();
    void KillScheduler();
    void WaitForCounter(WorkerThread* pThisThread, Counter* pCtr);
    Counter* RunJobs(struct JobDecl* pJobs, int numJobs);
};