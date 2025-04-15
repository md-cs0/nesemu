/*
; Base mapper class. I'm currently not interested in emulating
; bus conflicts.
*/

#pragma once

#include <stdint.h>

#include "constants.h"
#include "cartridge.h"

// Forward the cartridge struct.
struct cartridge;

// Mapper struct definition.
struct mapper
{
    // Bind the cartridge to the mapper.
    struct cartridge* cartridge;

    // Mapper ID.
    enum mappers mapper_id;
    
    // CHR/PRG ROM banks.
    size_t prg_rom_banks;
    size_t chr_rom_banks;

    // CPU mapping functions.
    bool (*cpu_read)(struct mapper* mapper, uint16_t address, uint8_t* byte);
    bool (*cpu_write)(struct mapper* mapper, uint16_t address, uint8_t byte);

    // PPU mapping functions.
    bool (*ppu_read)(struct mapper* mapper, uint16_t address, uint8_t* byte);
    bool (*ppu_write)(struct mapper* mapper, uint16_t address, uint8_t byte);

    // Mapper mirror type.
    enum mirror_type (*mirror_type)(struct mapper* mapper);

    // Memory de-allocation.
    void (*free)(void* mapper);
};

// Create a new base mapper instance.
struct mapper* mapper_alloc();

// Free a base mapper instance.
void mapper_free(void* mapper);