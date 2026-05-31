# Fibre Scheduler for games

- based on the talk [Parallelizing the Naughty Dog Engine Using Fibres](https://www.youtube.com/watch?v=HIVBhKj7gQU&t=578s) , but, I think, a slightly different design

- works on x86/64 linux

- uses other peoples implementations of lock free queues
    - https://github.com/taskflow/work-stealing-queue/tree/master
    - https://github.com/bowtoyourlord/MPSCQueue/blob/main/MPSCQueue.h


