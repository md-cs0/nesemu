/*
; Utility library.
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void* safe_calloc(size_t count, size_t size);

inline void* safe_malloc(size_t size)
{
    return safe_calloc(1, size);
}