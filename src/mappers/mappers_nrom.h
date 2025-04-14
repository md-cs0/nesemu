/*
; Mapper 0: NROM
*/

#pragma once

#include "mappers_base.h"

// NROM mapper struct definition.
struct mapper_nrom
{
    // Base mapper struct.
    struct mapper base;
};

// Create a new NROM mapper instance.
struct mapper_nrom* mapper_nrom_alloc();

// Free an NROM mapper instance.
void mapper_nrom_free(void* mapper);