/*
; Utility library.
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#if defined (__unix__) || defined(__APPLE__)
#define POSIX
#endif

#define NANOSECOND 1000000000ULL

void* safe_calloc(size_t count, size_t size);

inline void* safe_malloc(size_t size)
{
    return safe_calloc(1, size);
}

uint64_t get_ns_timestamp();