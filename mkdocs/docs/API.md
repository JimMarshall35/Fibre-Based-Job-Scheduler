# API

All functions are inside the "Jobs" namespace.

## Structs

Define a job with an ExecuteFn function ptr and a userData pointer.

```c
    typedef void (*ExecuteFn)(void*, Jobs::WorkerThread*);

    struct Job
    {
        ExecuteFn execute;
        void* userData;

        // private
        Counter* counter;

        // private
        Job* next;
    };
```

Associate a job with a priority and it is ready to send to the job system.


```c

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
```

## Functions

Run an array of JobDecls and get back an atomic counter equal to the number of jobs that each job will decrement.

```c
    Counter* RunJobs(struct JobDecl* pJobs, int numJobs);
```

Call this once at the start

```c
    void InitScheduler();
```

Call this once at the end to terminate and join all worker threads. Each thread saves their log buffer to a file on exit.

```c
    void KillScheduler();
```

Wait for a counter to reach 0, indicating all its jobs have finshed.
When this is called the calling fibre (and it must be a fibre not an OS thread) will be placed into a wait list allowing more jobs to be scheduled on this core.

```c
    void WaitForCounter(WorkerThread* pThisThread, Counter* pCtr);
    
```

Wait for a counter to reach 0, but from an OS thread not another job.
This will just spin until the counter reaches 0. Optionally pass sleepMS to sleep for sleepMS before checking the counter value.

```c
    void WaitForCounterOSThread(Counter* pCtr, int sleepMS=0);
```