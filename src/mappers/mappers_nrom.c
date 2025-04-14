/*
; Mapper 0: NROM
;
; PRG RAM is currently not emulated, meaning $6000-$7FFF will be unmapped.
; As a consequence, Family Basic will not run correctly on this emulator.
*/

#include <stdint.h>

#include "util.h"
#include "mappers_nrom.h"

// Map CPU read/write requests.
static intptr_t cpu_map(struct mapper* mapper, uint16_t address)
{
    // $8000-$FFFF: PRG ROM banks.
    if (0x8000 <= address && address <= 0xFFFF)
        return address & ((mapper->prg_rom_banks == 2) ? 0x7FFF : 0x3FFF);

    // The address has not been mapped to internal cartridge data.
    return UNMAPPED;
}

// Create a new NROM mapper instance.
struct mapper_nrom* mapper_nrom_alloc()
{
    // Allocate a new NROM mapper instance.
    struct mapper_nrom* mapper = safe_malloc(sizeof(struct mapper_nrom));

    // Assign the CPU read/write function pointers.
    mapper->base.cpu_read = cpu_map;
    mapper->base.cpu_write = cpu_map;

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