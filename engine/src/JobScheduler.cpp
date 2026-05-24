#include "JobScheduler.h"
#include <atomic>
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

#include <cstring>
#include <stdio.h>
#include <string>
#include <sstream>
#include <cstdarg>

#define LOG_SCHEDULING 1
namespace Jobs
{

    class CircularLogBuffer
    {
    public:
        CircularLogBuffer(size_t size)
            :m_pData(new char[size]),
            m_nDataCapacity(size),
            m_pWrite(m_pData)
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
            static char sBuf[1024];
            int numChars = vsnprintf(sBuf,2048, pFmt, args);
            sBuf[numChars++] = '\n';
            sBuf[numChars] = '\0';
            Write(sBuf);
        }
    private:
        char* m_pData = nullptr;
        size_t m_nDataCapacity = 0;
        char* m_pWrite = nullptr;
        size_t m_nSize = 0;

    };


    std::atomic<bool> gWorkersContinue;

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
            : localQueue(std::move(q)), 
            myStealer(std::move(stealer)), 
            pScheduler(pSched), 
            state(WorkerThreadState::Unstarted),
            logBuffer(1024 * 64)
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
        CircularLogBuffer logBuffer;
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
        while (gWorkersContinue.load())
        {
            auto job = pThread->localQueue.pop();
            Job j;
            bool bNewJobDequeued = false;
            JobPriority prioritydequeued = JobPriority::Undefined; // only used for logging
            if(job != std::nullopt)
            {
#ifdef LOG_SCHEDULING
                pThread->logBuffer.Writef("(scheduling fiber) scheduling fibre %i from local queue", job.value()->id);
#endif
                FibreSwitch(&pThread->pSchedulerFibre->ctx, &job.value()->ctx);
            }
            else if (pThread->highPriority.dequeue(j))
            {
                prioritydequeued = JobPriority::High;
                bNewJobDequeued = true;
            }
            else if (pThread->mediumPriority.dequeue(j))
            {
                prioritydequeued = JobPriority::Medium;
                bNewJobDequeued = true;
            }
            else if (pThread->lowPriority.dequeue(j))
            {
                prioritydequeued = JobPriority::Low;
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
#ifdef LOG_SCHEDULING
        std::stringstream ss;
        ss << "worker_thread_log_";
        ss << pThread->coreID;
        ss << ".txt";
        auto s = ss.str();
        pThread->logBuffer.WriteToFile(s.c_str());
#endif
    }

    struct JobScheduler
    {
        std::vector<WorkerThread*> workers;
    };

    struct JobScheduler gJobScheduler;

    void InitScheduler()
    {
        gWorkersContinue.store(true);
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
    
    void WaitForCounter(WorkerThread* pThisThread, Counter* pCtr)
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
#ifdef LOG_SCHEDULING
        pThisThread->logBuffer.Writef("Fiber [%i] waiting\n");
#endif
        FibreSwitch(&pThisThread->pActiveJobFibre->ctx, &pThisThread->pSchedulerFibre->ctx);
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
