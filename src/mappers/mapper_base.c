/*
; Base mapper class. I'm currently not interested in emulating
; bus conflicts.
*/

#include "util.h"
#include "mappers_base.h"

// Create a new base mapper instance.
struct mapper* mapper_alloc()
{
    struct mapper* mapper = safe_malloc(sizeof(struct mapper));
    mapper->free = mapper_free;
    return mapper;
}

// Free a base mapper instance.
void mapper_free(void* mapper)
{
    free(mapper);
}