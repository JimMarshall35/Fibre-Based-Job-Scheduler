# Welcome

This is the documentation page for a fibre based job system inspired by the talk [Parallelizing the Naughty Dog Engine Using Fibres](https://www.youtube.com/watch?v=HIVBhKj7gQU&t=578s).

It differs from that particular design in several ways, including adding "work stealing". For a detailed explaination see [Architecture page](architecture.md).

## Known limitiations

It is inherently x64 linux only as context switch code is written in assembly and obeys the linux system v ABI.

When I have access to a windows machine I will make a windows version as well.

The system described in "[Parallelizing the Naughty Dog Engine Using Fibres](https://www.youtube.com/watch?v=HIVBhKj7gQU&t=578s)." seems to be capable of waiting for the counter to reach any aribtrary value, but with mine you can only wait until the counter value is zero. If I think of a use case for the other way I can add that.

## Lock free queue implementations

Thanks to these github users who's code I have used:

- https://github.com/taskflow/work-stealing-queue/tree/master
- https://github.com/bowtoyourlord/MPSCQueue/tree/main

## Quick links

- [Getting Started](getting-started.md)
- [Architecture](architecture.md)
- [API](API.md)