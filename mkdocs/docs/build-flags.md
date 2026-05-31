# Build Flags

- `LOG_SCHEDULING`
    - Each Thread logs to its own circular buffer if `LOG_SCHEDULING == 1`. When 
- `LOG_TASK_FUNCTION_NAMES`
    - If `LOG_TASK_FUNCTION_NAMES == ` and `LOG_SCHEDULING == 1` then it will try and log the name of the function that comprises the tasks by calling dladdr 
- `TEST_MODE`
    - if `TEST_MODE == 1` it will print logs to the console as the program runs (access to printf not behind any kind of lock)