#ifndef ASYNC_RUNTIME_WINDOWS_CLONE_PROCESS_H
#define ASYNC_RUNTIME_WINDOWS_CLONE_PROCESS_H

#if defined(_WIN32)
#include <windows.h>
#include <sys/types.h>

typedef struct async_windows_process_stats {
    HANDLE process_handle;
    pid_t  process_id;
} async_windows_process_stats;

async_windows_process_stats async_runtime_windows_clone_process(void);

#endif

#endif