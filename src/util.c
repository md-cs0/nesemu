/*
; Utility library.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "util.h"

#if defined (_WIN32)
#include <Windows.h>
#elif defined(POSIX)
#include <time.h>
#endif

void* safe_calloc(size_t count, size_t size)
{
    assert(count);
    assert(size);
    void* ptr = calloc(1, size);
    if (ptr == NULL)
    {
        fprintf(stderr, "*ERROR* MEMORY ALLOCATION FAILED!\n");
        abort();
    }
    return ptr;
}

uint64_t get_ns_timestamp()
{
#if defined(_WIN32)
    static LARGE_INTEGER frequency;
    if (!frequency.QuadPart && !QueryPerformanceFrequency(&frequency))
        return 0;
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * NANOSECOND) / frequency.QuadPart;
#elif defined (POSIX)
    struct timespec timestamp;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timestamp);
    return timestamp.tv_sec * NANOSECOND + timestamp.tv_nsec;
#endif

    fprintf(stderr, "get_ns_timestamp(): target platform is not supported\n");
    return 0;
}