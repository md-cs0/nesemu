/*
; Utility library.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"

void* safe_calloc(size_t count, size_t size)
{
    assert(count);
    assert(size);
    void* ptr = calloc(1, size);
    if (ptr == NULL)
    {
        fprintf(stderr, "*ERROR* MEMORY ALLOCATION FAILED!");
        abort();
    }
    return ptr;
}