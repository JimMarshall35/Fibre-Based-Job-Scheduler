#include <gtest/gtest.h>
#include "../engine/include/JobScheduler.h"
#include "../engine/include/FrameAllocator.h"
#include <cmath>
#include <thread>
#include <chrono>


// monotonic clock in nanoseconds
static inline uint64_t now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

// burns CPU for roughly `ms` milliseconds
void simulate_work_ms(double ms) {
    
    const uint64_t duration_ns = (uint64_t)(ms * 1000000.0);

    uint64_t start = now_ns();

    double x = 1.0;

    while ((now_ns() - start) < duration_ns) 
    {

        // numerical junk work
        for (int i = 0; i < 1000; i++) {
            x += sin((double)i) * cos(x);
            x = sqrt(fabs(x) + 1.0);
        }
    }
    printf("%s\n", "simulate_work_ms");
}
void ChildTask2(void* pUser, Jobs::WorkerThread* pWorker)
{
    simulate_work_ms(2);
}

void ChildTask(void* pUser, Jobs::WorkerThread* pWorker)
{
    int* pOutInt = static_cast<int*>(pUser);
    int val = *pOutInt;
    Jobs::JobDecl decl[100];
    for(int i=0; i<val; i++)
    {
        decl[i] = {
            Jobs::JobPriority::High,
            {
                &ChildTask2,
                nullptr,
                nullptr,
                nullptr
            }
        };
    }
    Jobs::Counter* pCounter = Jobs::RunJobs(decl, val);
    Jobs::WaitForCounter(pWorker, pCounter);
}


static int num = 3; 

void TopLevelTask(void* pUser, Jobs::WorkerThread* pWorker)
{
    /*
            struct Job
            {
                ExecuteFn execute;;
                void* userData;

                Counter* counter;

                Job* next;
            };

    */
    int outputs[3];
    Jobs::JobDecl decl[3];
    for(int i=0; i<3; i++)
    {
        outputs[i] = i;
        decl[i] = {
            Jobs::JobPriority::Medium,
            {
                &ChildTask,
                &num,
                nullptr,
                nullptr
            }
        };
    }
    Jobs::Counter* pCounter = Jobs::RunJobs(&decl[0], 3);
    Jobs::WaitForCounter(pWorker, pCounter);
    printf("done\n");
}

TEST(Scheduler, Basic)
{
    FrameAllocator::Init(1024 * 100);
    FrameAllocator::BeginFrame();
    Jobs::InitScheduler();
    Jobs::JobDecl d = {
        Jobs::JobPriority::High,
        {
            &TopLevelTask,
            nullptr,
            nullptr,
            nullptr
        }
    };
    Jobs::Counter* pCtr = Jobs::RunJobs(&d, 1);
    Jobs::WaitForCounterOSThread(pCtr, 0);
    Jobs::KillScheduler();
    FrameAllocator::DeInit();
}