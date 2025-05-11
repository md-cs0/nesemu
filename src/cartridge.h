/*
; Game Pak NES cartridge with basic iNES binary format support.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "constants.h"
#include "mappers_base.h"

// NES cartridge struct definition.
struct cartridge
{
    // Program/character ROM buffers.
    uint8_t* prg_rom;
    uint8_t* chr_rom;
    size_t prg_rom_size;    // 16384 * x bytes
    size_t chr_rom_size;    // 8192 * y bytes

    // Mirror type.
    enum mirror_type mirror_type;

    // Mapper.
    struct mapper* mapper;
};

// Return the current nametable mirroring used.
enum mirror_type cartridge_mirror_type(struct cartridge* cartridge);

// Read per CPU request.
bool cartridge_cpu_read(struct cartridge* cartridge, uint16_t address, uint8_t* byte);

// Write per CPU request.
bool cartridge_cpu_write(struct cartridge* cartridge, uint16_t address, uint8_t byte);

// Read per PPU request.
bool cartridge_ppu_read(struct cartridge* cartridge, uint16_t address, uint8_t* byte);

// Write per PPU request.
bool cartridge_ppu_write(struct cartridge* cartridge, uint16_t address, uint8_t byte);

// Create a new cartridge instance.
struct cartridge* cartridge_alloc(uint8_t* ines_data, size_t ines_size);

// Free a cartridge instance.
void cartridge_free(struct cartridge* cartridge);

// Get the cartridge error message.
const char* cartridge_error_msg();