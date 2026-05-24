#include <atomic>
#include "Fibre_x64_systemv.h"
#include "FrameAllocator.h"
#include "MPSCQueue.hpp"
#include "CircularWSDeque.hpp"
#include "ObjectPool.hpp"
#include <vector>
#include <thread>
#include <optional>

// LINUX SPECIFIC BEGIN

#include <pthread.h>
#include <sched.h>

// LINUX SPECIFIC END


namespace Jobs
{
#if defined(__linux__)

    void PinThreadToCore(int core_id) 
    {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        pthread_t thread = pthread_self();

        int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (result != 0) 
        {
            assert(false && "failed to set thread affinity");
        }
    }

#endif
    
    struct Counter
    {
        std::atomic<int> value;

        // Wait list of fibers waiting on this counter
        Fiber* waitListHead;
    };

    struct Job
    {
        ExecuteFn execute;;
        void* userData;

        Counter* counter;

        Job* next;
    };

    enum class JobPriority
    {
        High,
        Medium,
        Low
    };

    struct JobDecl
    {
        JobPriority priority;
        Job job;
    };

    

    using JobQueue = mpsc_queue_t<Job>;
    using LocalJobQueue = deque::Worker<Fiber*>;

    
    struct JobScheduler;

    enum class WorkerThreadState
    {
        Unstarted,
        Idle,
        Active,
        Finished,
        Waiting,
    };

    struct WorkerThread
    {
        WorkerThread(LocalJobQueue& q, deque::Stealer<Fiber*>& stealer, int coreID, struct JobScheduler* pSched)
            : localQueue(std::move(q)), myStealer(std::move(stealer)), pScheduler(pSched), state(WorkerThreadState::Unstarted)
        {
            pSchedulerFibre = fiberPool.allocate();
        }
        LocalJobQueue localQueue;
        deque::Stealer<Fiber*> myStealer; /* used to allow other worker threads to steal */
        ObjectPool<Fiber, 64> fiberPool;
        int coreID;
        std::optional<std::thread> thread = std::nullopt; /* no thread created to begin with*/
        Fiber* pSchedulerFibre;
        Fiber* pActiveJobFibre = nullptr;
        struct JobScheduler* pScheduler;

        /*
            Honest, law abiding worker threads who are just trying to make an honest buck processing game simulation data.
            A fair days work, for a fair days pay, but if they don't process it fast enough "Johnny foreigner" will get there first
            and steal the jobs from right out of their local job queue.
        */
        std::vector<deque::Stealer<Fiber*>> myVictims;

        JobQueue highPriority;
        JobQueue mediumPriority;
        JobQueue lowPriority;
        WorkerThreadState state;
    };

    void JobWrapper(ExecuteFn execute, void* pUser, Counter* pCounter, WorkerThread* pThread)
    {
        pThread->state = WorkerThreadState::Active;

        // do work
        execute(pUser, pThread);

        // decrement counter
        auto v = pCounter->value.fetch_sub(1);
        if(v == 1)
        {
            Fiber* h = pCounter->waitListHead;
            while(h)
            {
                pThread->localQueue.push(h);
            }
        }
        pThread->state = WorkerThreadState::Finished;
    }

    void ScheduleWorker(WorkerThread* pThread)
    {
        pThread->state = WorkerThreadState::Idle;
        while (true)
        {
            auto job = pThread->localQueue.pop();
            Job j;
            bool bNewJobDequeued = false;
            if(job != std::nullopt)
            {
                FibreSwitch(&pThread->pSchedulerFibre->ctx, &job.value()->ctx);
            }
            else if (pThread->highPriority.dequeue(j))
            {
                bNewJobDequeued = true;
            }
            else if (pThread->mediumPriority.dequeue(j))
            {
                bNewJobDequeued = true;
            }
            else if (pThread->lowPriority.dequeue(j))
            {
                bNewJobDequeued = true;
            }
            else
            {
                for(auto& victim : pThread->myVictims)
                {
                    auto job = victim.steal();
                    if(job != std::nullopt)
                    {
                        FibreSwitch(&pThread->pSchedulerFibre->ctx, &job.value()->ctx);
                    }
                }
            }
            if(bNewJobDequeued)
            {
                Fiber* pF = pThread->fiberPool.allocate();
                pThread->pActiveJobFibre = pF;
                ResetFibreStack(pF);
                LoadNewJobIntoFiber(&pF->ctx, j.execute, j.userData, &JobWrapper, j.counter, pThread);
                FibreSwitchNewJob(&pThread->pSchedulerFibre->ctx, &pF->ctx);
            }
            if(pThread->state == WorkerThreadState::Finished)
            {
                pThread->fiberPool.deallocate(pThread->pActiveJobFibre);
            }
        }
    }

    struct JobScheduler
    {
        std::vector<WorkerThread*> workers;
    };

    struct JobScheduler gJobScheduler;

    void InitScheduler()
    {
        auto concurr = std::thread::hardware_concurrency();
        int numWorkers = concurr - 2;
        gJobScheduler.workers.resize(numWorkers); /* game engine will use platform and render threads as well */
        
        // create worker thread objects
        for(int i=0; i<numWorkers; i++)
        {
            auto pair = deque::deque<Fiber*>();
            gJobScheduler.workers[i] = new WorkerThread(pair.first, pair.second, i, &gJobScheduler);
        }

        // populate each threads theft victim array
        for(int i=0; i<numWorkers; i++)
        {
            WorkerThread& thread = *gJobScheduler.workers[i];
            for(int j=0; j<numWorkers; j++)
            {
                if(j == i)
                {
                    continue;
                }
                thread.myVictims.push_back(gJobScheduler.workers[j]->myStealer);
            }
        }

        // start an actual thread for each thread object
        for(int i=0; i<numWorkers; i++)
        {
            gJobScheduler.workers[i]->thread = std::thread([&](){
                /*
                    pin to core.
                    The naughty dog presentation said if you don't do this, when an OS thread gets scheduled,
                    and evicts a worker thread from the core, it can then be scheduled immediately on another worker
                    thread core as they have used a lot of cpu time, evicting that worker thread and causing a ripple effect.
                */
                PinThreadToCore(i);
                gJobScheduler.workers[i]->pSchedulerFibre->ctx.rdi = reinterpret_cast<uint64_t>(&gJobScheduler.workers[i]);
                gJobScheduler.workers[i]->pSchedulerFibre->ctx.rip = reinterpret_cast<uint64_t>(&ScheduleWorker);
                ResetFibreStack(gJobScheduler.workers[i]->pSchedulerFibre);
                FibreContext old;
                FibreSwitchNewJob(&old, &gJobScheduler.workers[i]->pSchedulerFibre->ctx);
            });

        }
    }

    void RunJob(struct JobDecl* pDecl, Counter* pCounter)
    {
        static std::atomic<int> sCounter = 0;
        // cycle through worker threads, each one gets its turn to be assigned
        // jobs by this global function
        int i = sCounter.fetch_add(1) % gJobScheduler.workers.size(); 
        pDecl->job.counter = pCounter;
        switch (pDecl->priority)
        {
        case JobPriority::High:
            gJobScheduler.workers[i]->highPriority.enqueue(pDecl->job);
            break;
        case JobPriority::Medium:
            gJobScheduler.workers[i]->mediumPriority.enqueue(pDecl->job);
            break;
        case JobPriority::Low:
            gJobScheduler.workers[i]->lowPriority.enqueue(pDecl->job);
            break;
        }
    }


    Counter* RunJobs(struct JobDecl* pJobs, int numJobs)
    {
        auto pCounter = static_cast<Counter*>(FrameAllocator::Allocate(sizeof(Counter)));
        pCounter->value.store(0);
        pCounter->waitListHead = nullptr;
        for(int i=0; i<numJobs; i++)
        {
            RunJob(&pJobs[i], pCounter);
        }
        return pCounter;
    }
    
    void WaitForCounter(WorkerThread* pThisThread, Counter* pCtr, int waitForVal=0)
    {
        /* 
            TODO: this is probably not thread safe, but it might be OK. 
        */
        pThisThread->state = WorkerThreadState::Waiting;
        if(pCtr->waitListHead)
        {
            pThisThread->pActiveJobFibre->nextWaiter = pCtr->waitListHead;
        }
        pCtr->waitListHead = pThisThread->pActiveJobFibre;
        FibreSwitch(&pThisThread->pActiveJobFibre->ctx, &pThisThread->pSchedulerFibre->ctx);
    }


};
