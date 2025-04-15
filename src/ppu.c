/*
; Picture Processing Unit emulation. This is a 2D picture generator that generates a
; 256x240 image. Currently, only the Ricoh 2C02 is emulated.
*/

#include <stdint.h>
#include <stdbool.h>

#include "util.h"
#include "ppu.h"

// ABGR8888 colour type, so that the NES code is independent of SDL.
struct agbr8888
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

// Ricoh 2C02 palette table (ABGR8888).
static struct agbr8888 palette_lookup[] =
{    // 0x00 - 0x0F
    {0x62, 0x62, 0x62, 0xFF},
    {0x00, 0x1F, 0xB2, 0xFF},
    {0x24, 0x04, 0xC8, 0xFF},
    {0x52, 0x00, 0xB2, 0xFF},
    {0x73, 0x00, 0x76, 0xFF},
    {0x80, 0x00, 0x24, 0xFF},
    {0x73, 0x0B, 0x00, 0xFF},
    {0x52, 0x28, 0x00, 0xFF},
    {0x24, 0x44, 0x00, 0xFF},
    {0x00, 0x57, 0x00, 0xFF},
    {0x00, 0x5C, 0x00, 0xFF},
    {0x00, 0x53, 0x24, 0xFF},
    {0x00, 0x3C, 0x76, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},

    // 0x10 - 0x1F
    {0xAB, 0xAB, 0xAB, 0xFF},
    {0x0D, 0x57, 0xFF, 0xFF},
    {0x4B, 0x30, 0xFF, 0xFF},
    {0x8A, 0x13, 0xFF, 0xFF},
    {0xBC, 0x08, 0xD6, 0xFF},
    {0xD2, 0x12, 0x69, 0xFF},
    {0xC7, 0x2E, 0x00, 0xFF},
    {0x9D, 0x54, 0x00, 0xFF},
    {0x60, 0x7B, 0x00, 0xFF},
    {0x20, 0x98, 0x00, 0xFF},
    {0x00, 0xA3, 0x00, 0xFF},
    {0x00, 0x99, 0x42, 0xFF},
    {0x00, 0x7D, 0xB4, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},

    // 0x20 - 0x2F
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0x53, 0xAE, 0xFF, 0xFF},
    {0x90, 0x85, 0xFF, 0xFF},
    {0xD3, 0x65, 0xFF, 0xFF},
    {0xFF, 0x57, 0xFF, 0xFF},
    {0xFF, 0x5D, 0xCF, 0xFF},
    {0xFF, 0x77, 0x57, 0xFF},
    {0xFA, 0x9E, 0x00, 0xFF},
    {0xBD, 0xC7, 0x00, 0xFF},
    {0x7A, 0xE7, 0x00, 0xFF},
    {0x43, 0xF6, 0x11, 0xFF},
    {0x26, 0xEF, 0x7E, 0xFF},
    {0x2C, 0xD5, 0xF6, 0xFF},
    {0x4E, 0x4E, 0x4E, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},

    // 0x30 - 0x3F
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xB6, 0xE1, 0xFF, 0xFF},
    {0xCE, 0xD1, 0xFF, 0xFF},
    {0xE9, 0xC3, 0xFF, 0xFF},
    {0xFF, 0xBC, 0xFF, 0xFF},
    {0xFF, 0xBD, 0xF4, 0xFF},
    {0xFF, 0xC6, 0xC3, 0xFF},
    {0xFF, 0xD5, 0x9A, 0xFF},
    {0xE9, 0xE6, 0x81, 0xFF},
    {0xCE, 0xF4, 0x81, 0xFF},
    {0xB6, 0xFB, 0x9A, 0xFF},
    {0xA9, 0xFA, 0xC3, 0xFF},
    {0xA9, 0xF0, 0xF4, 0xFF},
    {0xB8, 0xB8, 0xB8, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},
    {0x00, 0x00, 0x00, 0xFF}
};

// Internal address mapping for accessing the PPU's VRAM for nametable accessing
// depending on the given mapper's mirror type.
static uint16_t vram_mirror(struct ppu* ppu, uint16_t address)
{
    enum mirror_type mirror = cartridge_mirror_type(ppu->computer->cartridge);
    address &= 0x0FFF;
    if (mirror == MIRROR_HORIZONTAL)
    {
        if (0x0000 <= address && address <= 0x07FF)
            return address & 0x03FF;
        else
            return 0x400 + (address & 0x03FF);
    }
    else // Vertical mirroring.
    {
        if (0x0000 <= address && address <= 0x03FF || 0x0800 <= address && address <= 0x0BFF)
            return address & 0x03FF;
        else
            return 0x400 + (address & 0x03FF);
    }
}

// Reset the PPU.
void ppu_reset(struct ppu* ppu)
{
    // Reset the enumerated cycles count.
    ppu->enumerated_cycles = 0;

    // Clear public registers.
    ppu->ppuctrl.reg = 0x00;
    ppu->ppumask.reg = 0x00;
    ppu->oamaddr = 0x00;
    ppu->ppuscroll = 0x00;

    // Clear internal registers.
    ppu->w = false;
}

// Read a byte from a given address on the internal PPU bus.
uint8_t ppu_bus_read(struct ppu* ppu, uint16_t address)
{
    // The PPU only has a 14-bit address bus, so & it with 0x3FFF.
    address &= 0x3FFF;

    // Attempt to read from the cartridge.
    uint8_t byte;
    if (cartridge_cpu_read(ppu->computer->cartridge, address, &byte))
        return byte;

    // $2000-$2FFF: nametables 0-3.
    // $3000-$3EFF: usually a mirror of this region of memory.
    else if (0x2000 <= address && address <= 0x3EFF)
        return ppu->vram[vram_mirror(ppu, address)];

    // $3F00-$3FFF: palette RAM.
    else if (0x3F00 <= address && address <= 0x3FFF)
    {
        // Map the address first. Entry 0 of each palette is shared between
        // the background and sprite palettes. Entry 0 of palette 0 is
        // exclusively used as the backdrop colour.
        address &= 0x001F;
        if (address & 0x0010 && !(address & 0x0003))
            address &= ~0x0010;

        // Return the palette RAM index.
        return ppu->palette_ram[address];
    }

    // Open bus.
    return 0;
}

// Write a byte to a given address on the internal PPU bus.
void ppu_bus_write(struct ppu* ppu, uint16_t address, uint8_t byte)
{
    // The PPU only has a 14-bit address bus, so & it with 0x3FFF.
    address &= 0x3FFF;

    // Attempt to write to the cartridge.
    if (cartridge_ppu_write(ppu->computer->cartridge, address, byte))
        return;

    // $2000-$2FFF: nametables 0-3.
    // $3000-$3EFF: usually a mirror of this region of memory.
    else if (0x2000 <= address && address <= 0x3EFF)
        ppu->vram[vram_mirror(ppu, address)] = byte;

    // $3F00-$3FFF: palette RAM.
    else if (0x3F00 <= address && address <= 0x3FFF)
    {
        // Map the address first. Entry 0 of each palette is shared between
        // the background and sprite palettes.
        address &= 0x001F;
        if (address & 0x0010 && !(address & 0x0003))
            address &= ~0x0010;

        // Set the palette RAM index.
        ppu->palette_ram[address] = byte;
    }

    // Open bus.
}

// Handle CPU read requests from the PPU here.
uint8_t ppu_cpu_read(struct ppu* ppu, uint16_t address)
{
    // Find which PPU register is being accessed.
    switch (address)
    {
    // PPUSTATUS
    case 0x0002:
    {
        return 0;
    }
    
    // OAMDATA
    case 0x0004:
    {
        return 0;
    }

    // PPUDATA
    case 0x0007:
    {
        return 0;
    }
    }
    
    // Open bus. The returned value in this case is typically the value of an internal
    // latch, however this isn't emulated, so just return 0.
    return 0;
}

// Handle CPU write requests to the PPU here.
void ppu_cpu_write(struct ppu* ppu, uint16_t address, uint8_t byte)
{
    // Find which PPU register is being accessed.
    switch (address)
    {
    // PPUCTRL
    case 0x0000:
    {
        return;
    }

    // PPUMASK
    case 0x0001:
    {
        return;
    }

    // OAMADDR
    case 0x0003:
    {
        return;
    }

    // OAMDATA
    case 0x0004:
    {
        return;
    }

    // PPUSCROLL
    case 0x0005:
    {
        return;
    }

    // PPUADDR
    case 0x0006:
    {
        return;
    }

    // PPUDATA
    case 0x0007:
    {
        return;
    }
    }

    // Open bus.
}

// Create a new PPU instance. The PPU must be reset before used.
struct ppu* ppu_alloc()
{
    // Allocate a new PPU instance.
    struct ppu* ppu = safe_malloc(sizeof(struct ppu));

    // As safe_malloc() uses calloc() internally, the registers should already
    // be set to zero.
    
    // Return the PPU.
    return ppu;
}

// Free a CPU instance.
void ppu_free(struct ppu* ppu)
{
    free(ppu);
}