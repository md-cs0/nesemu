/*
; NES cartridge using the iNES binary format.
*/

#include <stdio.h>
#include <memory.h>
#include <stdint.h>
#include <stdbool.h>

#include "util.h"
#include "cartridge.h"

#include "mappers_nrom.h"

#define INES_MAGIC 0x1A53454E

// iNES file format header.
struct ines_header
{
    // Initial data.
    int32_t magic;              // $43 $45 $5 $1A; see INES_MAGIC macro
    uint8_t prg_rom_size;       // in 16KB units
    uint8_t chr_rom_size;       // in 8KB units (if present, but CHR RAM is not currently emulated)
    
    // Flags 6.
    uint8_t mirror      : 1;    // 0: horizontal mirroed, 1: vertically mirrored
    uint8_t has_prg_ram : 1;    // 0: no prg ram, 1: has prg ram (not currently emulated)
    uint8_t has_trainer : 1;    // 0: no 512-byte trainer, 1: has trainer (not currently emulated)
    uint8_t nt_layout   : 1;    // if 1, use alternative nametable layout (not currently emulated)
    uint8_t mapper_lo   : 4;    // lower nybble of the mapper number

    // Flags 7.
    uint8_t vt_unisys   : 1;    // not currently emulated
    uint8_t playchoice  : 1;    // not currently emulated
    uint8_t is_nes_2    : 2;    // not currently emulated
    uint8_t mapper_hi   : 4;    // upper nybble of the mapper number

    // Flags 8.
    uint8_t prg_ram_size;       // in 16KB units; currently not emulated

    // Flags 9.
    uint8_t tv_system   : 1;    // not currently emulated
    uint8_t padding0    : 7;

    // Flags 10.
    uint8_t padding1;           // unofficial extension

    // Unused padding.
    uint8_t padding2[5];
};

// Read per CPU request.
bool cartridge_cpu_read(struct cartridge* cartridge, uint16_t address, uint8_t* read)
{
    // Check if the cartridge mapper returns a mapped address.
    intptr_t mapped_address = cartridge->mapper->cpu_read(cartridge->mapper, address);
    if (mapped_address == UNMAPPED)
        return false;

    // Read from the given mapped address.
    *read = cartridge->prg_rom[mapped_address];
    return true;
}

// Write per CPU request.
bool cartridge_cpu_write(struct cartridge* cartridge, uint16_t address, uint8_t data)
{
    return false;
}

// Create a new cartridge instance.
struct cartridge* cartridge_alloc(uint8_t* ines_data, size_t ines_size)
{
    // If the given size is smaller than the size of the header, we can't
    // even begin to read the header, so exit immediately.
    if (ines_size < sizeof(struct ines_header))
        return NULL;

    // Allocate a new cartridge instance.
    struct cartridge* cartridge = safe_malloc(sizeof(struct cartridge));
    struct ines_header* header = (struct ines_header*)ines_data;

    // Validate the magic of the cartridge data.
    if (header->magic != INES_MAGIC)
        goto corrupt;

    // Validate the overall size of the cartridge data.
    size_t calculated_size = sizeof(struct ines_header);
    if (header->has_trainer)
        calculated_size += 0x200;
    size_t offset = calculated_size;
    calculated_size += (cartridge->prg_rom_size = header->prg_rom_size * 0x4000);
    calculated_size += (cartridge->chr_rom_size = header->chr_rom_size * 0x2000);
    if (ines_size < calculated_size)
        goto corrupt;

    // Read the PRG ROM.
    cartridge->prg_rom = safe_malloc(cartridge->prg_rom_size);
    memcpy(cartridge->prg_rom, ines_data + offset, cartridge->prg_rom_size);

    // Read the CHR ROM.
    offset += cartridge->prg_rom_size;
    cartridge->chr_rom = safe_malloc(cartridge->chr_rom_size);
    memcpy(cartridge->chr_rom, ines_data + offset, cartridge->chr_rom_size);

    // Initialize the mapper.
    enum mappers mapper_id = header->mapper_lo | (header->mapper_hi << 4);
    switch (mapper_id)
    {
    // Mapper 0: NROM
    case MAPPER_NROM:
        cartridge->mapper = (struct mapper*)mapper_nrom_alloc();
        break;
    default:
        fprintf(stderr, "mapper ID %d is currently not supported", mapper_id);
        goto corrupt;
    }
    cartridge->mapper->prg_rom_banks = header->prg_rom_size;
    cartridge->mapper->chr_rom_banks = header->chr_rom_size;

    // Return the cartridge.
    return cartridge;

corrupt:
    cartridge_free(cartridge);
    return NULL;
}

// Free a cartridge instance.
void cartridge_free(struct cartridge* cartridge)
{
    if (cartridge == NULL)
        return;
    if (cartridge->mapper)
        cartridge->mapper->free(cartridge->mapper); // This is fine because &mapper->base = &mapper.
    free(cartridge->prg_rom);
    free(cartridge->chr_rom);
    free(cartridge);
}