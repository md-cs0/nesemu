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

// calloc() which aborts if calloc() returns NULL.
void* safe_calloc(size_t count, size_t size);

// Wrapper around safe_calloc().
inline void* safe_malloc(size_t size)
{
    return safe_calloc(1, size);
}

// Get a timestamp using the system's high-resolution clock in nanoseconds.
uint64_t get_ns_timestamp();

// Linearly interpolate from a to b using weight t.
inline float lerp(float a, float b, float t)
{
    return a * (1.f - t) + (b * t);
}

// https://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64BitsDiv
inline uint8_t reverse_byte(uint8_t byte)
{
    return (byte * 0x0202020202ULL & 0x010884422010ULL) % 1023;
}