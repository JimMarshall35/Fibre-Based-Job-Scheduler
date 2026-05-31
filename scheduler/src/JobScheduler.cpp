#include "JobScheduler.h"
#include <atomic>
#include "FrameAllocator.h"
#include "MPSCQueue.hpp"
#include "CircularWSDeque.hpp"
#include "ObjectPool.hpp"
#include <vector>
#include <thread>
#include <optional>

#if defined(__linux__)

#include <pthread.h>
#include <sched.h>
#include <time.h> 

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

#endif

#include <cstring>
#include <stdio.h>
#include <string>
#include <sstream>
#include <cstdarg>
#include <cinttypes>
#include <chrono>

#include <iostream>
#include <mutex>
#define TEST_MODE 0

#define LOG_SCHEDULING 1

#define LOG_TASK_FUNCTION_NAMES 1

#if LOG_SCHEDULING == 1
#define SCHED_LOG(pThread, fmt, ...) pThread->logBuffer.Writef(fmt, __VA_ARGS__)
#else
#define SCHED_LOG(pThread, fmt, ...)
#endif


namespace Jobs
{

////////////////////////////////////////////////////////////////////////////////////////// DATA STRUCTURES

    class CircularLogBuffer
    {
    public:
        CircularLogBuffer(size_t size, size_t id)
            :m_pData(new char[size]),
            m_nDataCapacity(size),
            m_pWrite(m_pData),
            m_nID(id)
        {}
        ~CircularLogBuffer()
        {
            delete[] m_pData;
        }
        void Write(const char* str)
        {
            char* pEnd = m_pData + m_nDataCapacity;
            size_t len = strlen(str);
            char* pWriteEnd = m_pWrite + len;
            if(pWriteEnd > pEnd)
            {
                size_t excess = pWriteEnd - pEnd;
                memcpy(m_pWrite, str, len - excess);
                m_pWrite = m_pData;
                memcpy(m_pWrite, str + len - excess, excess);
                m_pWrite += excess;
            }
            else
            {
                memcpy(m_pWrite, str, len);
                m_pWrite += len;
            }
            m_nSize += len;
        }
        void WriteToFile(const char* filePath)
        {
            FILE* pFile = fopen(filePath, "w+");
            assert(pFile);
            if(m_nSize <= m_nDataCapacity)
            {
                fwrite(m_pData, m_nSize, 1, pFile);
            }
            else
            {
                char* pEnd = m_pData + m_nDataCapacity;
                size_t toEnd = pEnd - m_pWrite;
                fwrite(m_pWrite, toEnd, 1, pFile);
                size_t remainder = m_nDataCapacity - toEnd;
                fwrite(m_pData, remainder, 1, pFile);
            }
            fclose(pFile);
        }
        void Writef(const char* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            VWritef(fmt, args);
            va_end(args);
        }
    private:
        void VWritef(const char* pFmt, va_list args)
        {
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            auto now = (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
            char sBuf[2048];
            const char* idFmt = "[%"PRIu64 "] ";
            int num = sprintf(sBuf, idFmt, m_nID);
            const char* timeFmt = "[%" PRIu64 "] " ;
            num += sprintf(sBuf + num, timeFmt, now);

            int numChars = vsnprintf(sBuf + num,2048, pFmt, args);
            numChars += num;
            sBuf[numChars++] = '\n';
            sBuf[numChars] = '\0';
#if TEST_MODE == 1
            printf("%s",sBuf);
            
#endif
            Write(sBuf);
        }
    private:
        char* m_pData = nullptr;
        size_t m_nDataCapacity = 0;
        char* m_pWrite = nullptr;
        size_t m_nSize = 0;
        size_t m_nID = 0;
    };


    std::atomic<bool> gWorkersContinue;
    
    struct Counter
    {
        std::atomic<int> value;

        // Wait list of fibSchedulerers waiting on this counter
        std::atomic<Fiber*> waitListHead = nullptr;
        size_t id = 0;
    };

    using JobQueue = MPSCQueue<Job>;
    using LocalJobQueue = wsq::BoundedWSQ<Fiber*,10>;

    struct JobScheduler;

    enum class WorkerThreadState
    {
        Unstarted,
        Idle,
        Active,
        Finished,
        Waiting,
    };

    struct JobScheduler
    {
        std::vector<WorkerThread*> workers;
    };

    struct WorkerThread
    {
        WorkerThread(int coreID, struct JobScheduler* pSched)
            :pScheduler(pSched), 
            state(WorkerThreadState::Unstarted),
            logBuffer(1024 * 64, coreID),
            coreID(coreID)
        {
            pSchedulerFibre = fiberPool.allocate();
            pSchedulerFibre->pOwner = this;
        }
        LocalJobQueue localQueue;

        ObjectPool<Fiber, 64> fiberPool;
        int coreID;
        std::optional<std::thread> thread = std::nullopt; /* no thread created to begin with*/
        Fiber* pSchedulerFibre;
        Fiber* pActiveJobFibre = nullptr;
        FibreContext OSThreadCtx;
        struct JobScheduler* pScheduler;

        /*
            Honest, law abiding worker threads who are just trying to make an honest buck processing game simulation data.
            A fair days work, for a fair days pay, but if they don't process it fast enough "Johnny foreigner" will get there first
            and steal the jobs from right out of their local job queue.
        */
        std::vector<LocalJobQueue*> myVictims;

        JobQueue highPriority;
        JobQueue mediumPriority;
        JobQueue lowPriority;
        WorkerThreadState state;
        CircularLogBuffer logBuffer;
    };

////////////////////////////////////////////////////////////////////////////////////////// GLOBAL VARS

    struct JobScheduler gJobScheduler;
    thread_local WorkerThread* gTLSThread = nullptr;

////////////////////////////////////////////////////////////////////////////////////////// STATIC FUNCTIONS


#if defined(__linux__)
    static void PinThreadToCore(int core_id) 
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

    static void JobWrapper(ExecuteFn execute, void* pUser, Counter* pCounter)
    {
        gTLSThread->state = WorkerThreadState::Active;
        //pThread->logBuffer.Writef("thread %zu in state Active", pThread);
        // do work
        execute(pUser, gTLSThread);

        // decrement counter
        auto v = pCounter->value.fetch_sub(1);
        
        SCHED_LOG(gTLSThread, "decrementing counter %zu. old value: %i", pCounter->id, v);
        if(v == 1)
        {
            Fiber* h = pCounter->waitListHead;
            while(h) 
            {
                SCHED_LOG(gTLSThread, "removing fiber %zu from waitlist", h->id);

                while(!gTLSThread->localQueue.try_push(h))
                {
                }
                pCounter->waitListHead = pCounter->waitListHead.load()->nextWaiter;
                h = pCounter->waitListHead;
            }
        }
        gTLSThread->state = WorkerThreadState::Finished;
        FibreContext c;
        FibreSwitch(&c, &gTLSThread->pSchedulerFibre->ctx);
    }

    static void ScheduleWorker(WorkerThread* pThread)
    {
        std::cout << "started "<< pThread<< "\n";
        gTLSThread = pThread;
        gTLSThread->state = WorkerThreadState::Idle;
        while (gWorkersContinue.load())
        {
            Fiber* job = pThread->localQueue.pop();
            std::optional<Job> j;
            bool bNewJobDequeued = false;
            JobPriority prioritydequeued = JobPriority::Undefined; // only used for logging
            if(job)
            {
                SCHED_LOG(gTLSThread, "scheduling fiber %i from local queue", job->id);
                gTLSThread->pActiveJobFibre = job;
                FibreSwitch(&gTLSThread->pSchedulerFibre->ctx, &job->ctx);
            }
            else if (j = gTLSThread->highPriority.pop())
            {
                prioritydequeued = JobPriority::High;
                bNewJobDequeued = true;
            }
            else if (j = gTLSThread->mediumPriority.pop())
            {
                prioritydequeued = JobPriority::Medium;
                bNewJobDequeued = true;
            }
            else if (j = gTLSThread->lowPriority.pop())
            {
                prioritydequeued = JobPriority::Low;
                bNewJobDequeued = true;
            }
            else if (!bNewJobDequeued)
            {
                for(auto& victim : gTLSThread->myVictims)
                {
                    if(!gWorkersContinue.load()) continue;

                    Fiber* job = victim->steal();
                    if(job != nullptr)
                    {
                        SCHED_LOG(gTLSThread, "stealing fiber %i ", job->id);
                        pThread->pActiveJobFibre = job;
                        FibreSwitch(&gTLSThread->pSchedulerFibre->ctx, &gTLSThread->pActiveJobFibre->ctx);
                    }
                }
            }
            if(bNewJobDequeued)
            {
                Fiber* pF = gTLSThread->fiberPool.allocate();
                pF->pOwner = gTLSThread;
                gTLSThread->pActiveJobFibre = pF;
                ResetFibreStack(pF);
                LoadNewJobIntoFiber(&pF->ctx, j.value().execute, j.value().userData, &JobWrapper, j.value().counter, pThread);

#if LOG_TASK_FUNCTION_NAMES == 1  
                /* don't know how computationally expensive this is so keep it optional */
                Dl_info info;
                dladdr((const void*)j.value().execute, &info);
                SCHED_LOG(gTLSThread, "new job dequeued %s with user data %p onto fiber %i with priority %s pF %p", 
                   info.dli_sname, j.value().userData, pF->id, gJobPriorityEnumNames[static_cast<size_t>(prioritydequeued)], pF
                );
#else
                /* 
                    print the raw fn pointer, if we log the base address of a module as well we can determine the 
                    function name after the fact. But different shared libs + exes may be used in the final game.
                */
                SCHED_LOG(gTLSThread, "new job dequeued %p with user data %p onto fiber %i with priority %s pF %p", 
                   j.value().execute, j.value().userData, pF->id, gJobPriorityEnumNames[static_cast<size_t>(prioritydequeued)], pF
                );
#endif
                FibreSwitchNewJob(&gTLSThread->pSchedulerFibre->ctx, &pF->ctx);
            }
            if(gTLSThread->state == WorkerThreadState::Finished && gTLSThread->pActiveJobFibre->pOwner == gTLSThread)
            {
                SCHED_LOG(gTLSThread, "deallocating fiber %i", gTLSThread->pActiveJobFibre->id);
                gTLSThread->fiberPool.deallocate(gTLSThread->pActiveJobFibre);
                gTLSThread->state = WorkerThreadState::Idle;
            }
            else if(gTLSThread->state == WorkerThreadState::Finished && gTLSThread->pActiveJobFibre->pOwner != gTLSThread)
            {
                SCHED_LOG(gTLSThread, "NOT deallocating fiber %i", gTLSThread->pActiveJobFibre->id);
                gTLSThread->state = WorkerThreadState::Idle;
            }
            if(gTLSThread->state == WorkerThreadState::Waiting)
            {
                gTLSThread->pActiveJobFibre = nullptr;
                gTLSThread->state = WorkerThreadState::Idle;
            }
        }
#ifdef LOG_SCHEDULING
        char buf[267];
        sprintf(buf, "worker_thread_log_%i.txt", gTLSThread->coreID);
        gTLSThread->logBuffer.WriteToFile(buf);
#endif
        FibreContext old;
        FibreSwitch(&old, &gTLSThread->OSThreadCtx);
    }

    static void RunJob(struct JobDecl* pDecl, Counter* pCounter)
    {
        static std::atomic<int> sCounter = 0;
        // cycle through worker threads, each one gets its turn to be assigned
        // jobs by this global function
        int i = sCounter.fetch_add(1) % gJobScheduler.workers.size(); 
        printf("Running job on worker %i\n", i);
        pDecl->job.counter = pCounter;
        switch (pDecl->priority)
        {
        case JobPriority::High:
            gJobScheduler.workers[i]->highPriority.push(pDecl->job);
            break;
        case JobPriority::Medium:
            gJobScheduler.workers[i]->mediumPriority.push(pDecl->job);
            break;
        case JobPriority::Low:
            gJobScheduler.workers[i]->lowPriority.push(pDecl->job);
            break;
        }
    }

////////////////////////////////////////////////////////////////////////////////////////// PUBLIC FUNCTIONS

    void InitScheduler()
    {
        gWorkersContinue.store(true);
        auto concurr = std::thread::hardware_concurrency();
        int numWorkers = concurr - 2;
        gJobScheduler.workers.resize(numWorkers); /* game engine will use platform and render threads as well */
        
        // create worker thread objects
        for(int i=0; i<numWorkers; i++)
        {
            gJobScheduler.workers[i] = new WorkerThread(i, &gJobScheduler);
            std::cout << "created thread object " << gJobScheduler.workers[i] << "\n";
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
                thread.myVictims.push_back(&gJobScheduler.workers[j]->localQueue);
            }
        }

        // start an actual thread for each thread object
        for(int i=0; i<numWorkers; i++)
        {
            gJobScheduler.workers[i]->thread = std::thread([i](){
                /*
                    pin to core.
                    The naughty dog presentation said if you don't do this, when an OS thread gets scheduled,
                    and evicts a worker thread from the core, it can then be scheduled immediately on another worker
                    thread core as they have used a lot of cpu time, evicting that worker thread and causing a ripple effect.
                */
                PinThreadToCore(i);
                gJobScheduler.workers[i]->pSchedulerFibre->ctx.rdi = reinterpret_cast<uint64_t>(gJobScheduler.workers[i]);
                gJobScheduler.workers[i]->pSchedulerFibre->ctx.rip = reinterpret_cast<uint64_t>(&ScheduleWorker);
                ResetFibreStack(gJobScheduler.workers[i]->pSchedulerFibre);
                FibreSwitchNewJob(&gJobScheduler.workers[i]->OSThreadCtx, &gJobScheduler.workers[i]->pSchedulerFibre->ctx);
            });

        }
    }

    Counter* RunJobs(struct JobDecl* pJobs, int numJobs)
    {
        auto pCounter = static_cast<Counter*>(FrameAllocator::Allocate(sizeof(Counter)));
        static size_t sID = 1;
        pCounter->id = sID++;
        pCounter->value.store(numJobs);
        
        pCounter->waitListHead = nullptr;
        for(int i=0; i<numJobs; i++)
        {
            RunJob(&pJobs[i], pCounter);
        }
        return pCounter;
    }
    
    void WaitForCounter(WorkerThread* pThisThread, Counter* pCtr)
    {
        /* 
            TODO: this is probably not thread safe, but it might be OK. 
        */
        pThisThread->state = WorkerThreadState::Waiting;
        
        // CAS loop to push fiber onto wait list head
        Fiber* pFiber = pThisThread->pActiveJobFibre;
        Fiber* oldHead = pCtr->waitListHead.load(std::memory_order_relaxed);
        do {
            pFiber->nextWaiter = oldHead;
        } while (!pCtr->waitListHead.compare_exchange_weak(
            oldHead, pFiber,
            std::memory_order_release,
            std::memory_order_relaxed));

        // maybe counter was zero before we were added to the list
        if (pCtr->value.load(std::memory_order_acquire) == 0)
        {
            // Pull ourselves back off - nobody will wake us
            pCtr->waitListHead.compare_exchange_strong(pFiber, pFiber->nextWaiter,
                std::memory_order_relaxed, std::memory_order_relaxed);
            return; // counter already done, no need to suspend
        }

        SCHED_LOG(pThisThread, "fiber %zu waiting for counter %zu", pThisThread->pActiveJobFibre->id, pCtr->id);
        FibreSwitch(&pThisThread->pActiveJobFibre->ctx, &pThisThread->pSchedulerFibre->ctx);
    }


    void WaitForCounterOSThread(Counter* pCtr, int sleepMS/* = 0 */)
    {
        while(pCtr->value.load() != 0)
        {
            if(sleepMS)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMS));
            }
        }
    }

    void KillScheduler()
    {
        gWorkersContinue.store(false);
        for(auto w : gJobScheduler.workers)
        {
            w->thread.value().join();
            delete w;
        }
    }
};
