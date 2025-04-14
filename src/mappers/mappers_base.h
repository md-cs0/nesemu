/*
; Base mapper class. I'm currently not interested in emulating
; bus conflicts.
*/

#pragma once

#include <stdint.h>

#define UNMAPPED (intptr_t)(-1)

// Mapper struct definition.
struct mapper
{
    // CHR/PRG ROM banks.
    size_t prg_rom_banks;
    size_t chr_rom_banks;

    // CPU mapping functions.
    intptr_t (*cpu_read)(struct mapper* mapper, uint16_t address);
    intptr_t (*cpu_write)(struct mapper* mapper, uint16_t address);

    // PPU mapping functions.
    // TODO

    // Memory de-allocation.
    void (*free)(void* mapper);
};

// Create a new base mapper instance.
struct mapper* mapper_alloc();

// Free a base mapper instance.
void mapper_free(void* mapper);