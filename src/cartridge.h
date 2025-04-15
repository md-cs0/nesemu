/*
; Game Pak NES cartridge with basic iNES binary format support.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "mappers_base.h"

// A list of supported mappers.
enum mappers
{
    MAPPER_NROM = 0
};

// NES cartridge struct definition.
struct cartridge
{
    // Program/character ROM buffers.
    uint8_t* prg_rom;
    uint8_t* chr_rom;
    size_t prg_rom_size;    // 16384 * x bytes
    size_t chr_rom_size;    // 8192 * y bytes

    // Mapper.
    struct mapper* mapper;
};

// Read per CPU request.
bool cartridge_cpu_read(struct cartridge* cartridge, uint16_t address, uint8_t* read);

// Write per CPU request.
bool cartridge_cpu_write(struct cartridge* cartridge, uint16_t address, uint8_t data);

// Create a new cartridge instance.
struct cartridge* cartridge_alloc(uint8_t* ines_data, size_t ines_size);

// Free a cartridge instance.
void cartridge_free(struct cartridge* cartridge);