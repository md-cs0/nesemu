/*
; Mapper 0: NROM
;
; PRG RAM is currently not emulated, meaning $6000-$7FFF will be unmapped.
; As a consequence, Family Basic will not run correctly on this emulator.
*/

#include <stdint.h>

#include "constants.h"
#include "util.h"
#include "mappers_nrom.h"

// Map CPU read requests.
static bool cpu_map_read(struct mapper* mapper, uint16_t address, uint8_t* byte)
{
    assert(byte);

    // $8000-$FFFF: PRG ROM banks.
    if (0x8000 <= address && address <= 0xFFFF)
    {
        *byte = mapper->cartridge->prg_rom[address & ((mapper->prg_rom_banks == 2) ? 0x7FFF : 0x3FFF)];
        return true;
    }

    // The address has not been mapped to internal cartridge data.
    return false;
}

// Map CPU write requests.
static bool cpu_map_write(struct mapper* mapper, uint16_t address, uint8_t byte)
{
    // PRG RAM is currently not emulated.
    return false;
}

// Map PPU read requests.
static bool ppu_map_read(struct mapper* mapper, uint16_t address, uint8_t* byte)
{
    assert(byte);

    // $0000-$1FFF: CHR ROM banks.
    if (0x0000 <= address && address <= 0x1FFF)
    {
        *byte = mapper->cartridge->chr_rom[address];
        return true;
    }

    // The address has not been mapped to internal cartridge data.
    return false;
}

// Map PPU write requests.
static bool ppu_map_write(struct mapper* mapper, uint16_t address, uint8_t byte)
{
    return false;
}

// Return the mapper's mirror type.
static enum mirror_type mirror_type(struct mapper* mapper)
{
    return MIRROR_CARTRIDGE;
}

// Create a new NROM mapper instance.
struct mapper_nrom* mapper_nrom_alloc()
{
    // Allocate a new NROM mapper instance.
    struct mapper_nrom* mapper = safe_malloc(sizeof(struct mapper_nrom));

    // Assign the CPU read/write function pointers.
    mapper->base.cpu_read = cpu_map_read;
    mapper->base.cpu_write = cpu_map_write;

    // Assign the PPU read/write function pointers.
    mapper->base.ppu_read = ppu_map_read;
    mapper->base.ppu_write = ppu_map_write;

    // Assign the mapper's mirror type function pointer.
    mapper->base.mirror_type = mirror_type;

    // Assign the memory de-allocation function pointer.
    mapper->base.free = mapper_nrom_free;

    // Return the mapper.
    return mapper;
}

// Free an NROM mapper instance.
void mapper_nrom_free(void* mapper)
{
    free(mapper);
}