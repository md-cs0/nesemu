/*
; Picture Processing Unit emulation. This is a 2D picture generator that generates a
; 256x240 image. Currently, only the Ricoh 2C02 is emulated.
*/

#include <stdint.h>
#include <stdbool.h>

#include "util.h"
#include "ppu.h"

// Internal enum for deciding the current timing stage.
enum timing
{
    TIMING_UNKNOWN = -1,

    TIMING_PRE_RENDER = 0,
    TIMING_VISIBLE,
    TIMING_POST_RENDER,
    TIMING_VBLANK
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

// Is rendering enabled?
// If both bits 3 and 4 are forced to be zero, this is known to be forced blanking.
static inline bool ppu_isrendering(struct ppu* ppu)
{
    return ppu->ppumask.vars.background_rendering || ppu->ppumask.vars.sprite_rendering;
}

// See above function.
static inline bool ppu_forcedblanking(struct ppu* ppu)
{
    return !ppu_isrendering(ppu);
}

// What stage of rendering is the PPU currently in?
static inline enum timing ppu_timing(struct ppu* ppu)
{
    if (ppu->scanline == -1)
        return TIMING_PRE_RENDER;
    else if (0 <= ppu->scanline && ppu->scanline <= 239)
        return TIMING_VISIBLE;
    else if (ppu->scanline == 240)
        return TIMING_POST_RENDER;
    else if (241 <= ppu->scanline && ppu->scanline <= 260)
        return TIMING_VBLANK;

    assert(false);
    return TIMING_UNKNOWN;
}

// Increment the coarse X scroll component of v. If coarse X == 31,
// switch the horizontal nametable.
static void coarse_x_increment(struct ppu* ppu)
{
    if (ppu->v.vars.coarse_x_scroll == 0b11111)
        ppu->v.vars.nametable_select = ppu->v.vars.nametable_select ^ 0b01;
    ppu->v.vars.coarse_x_scroll++;
}

// Increment the fine y scroll component of V. If fine y == 7 and
// coarse y == 29, switch the vertical nametable (29 specifically due to
// row 29 being the last row of tiles in a nametable).)
static void fine_y_increment(struct ppu* ppu)
{
    if (ppu->v.vars.fine_y_scroll == 0b111)
    {
        if (ppu->v.vars.coarse_y_scroll == 29)
        {
            ppu->v.vars.coarse_y_scroll = 0;
            ppu->v.vars.nametable_select = ppu->v.vars.nametable_select ^ 0b10;
        }
        else
            ppu->v.vars.coarse_y_scroll++;
    }
    ppu->v.vars.fine_y_scroll++;
}

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
    // Reset the timing information.
    ppu->enumerated_cycles = 0;
    ppu->cycle = 0;
    ppu->scanline = -1;
    ppu->frame_complete = false;

    // Clear public registers.
    ppu->ppuctrl.reg = 0x00;
    ppu->ppumask.reg = 0x00;
    ppu->oamaddr = 0x00;
    ppu->ppuscroll = 0x00;

    // Clear internal registers.
    ppu->w = false;

    // Reset PPU flags.
    ppu->oam_executing_dma = false;
    ppu->even_odd_frame = true; // Set to odd so that it is set to even next frame.
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
        ppu->w = false;
        return ppu->ppustatus.reg;
    }
    
    // OAMDATA
    case 0x0004:
    {
        return ppu->oam_byte_pointer[ppu->oamaddr];
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
        ppu->t.vars.nametable_select = byte & 0b11;
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
        ppu->oamaddr = byte;
        return;
    }

    // OAMDATA
    case 0x0004:
    {
        ppu->oamaddr++;
        return;
    }

    // PPUSCROLL
    case 0x0005:
    {
        if (ppu->w)
        {
            ppu->t.vars.coarse_y_scroll = (byte >> 3) & 0b11111;
            ppu->t.vars.fine_y_scroll = byte & 0b111;
        }
        else
        {
            ppu->t.vars.coarse_x_scroll = (byte >> 3) & 0b11111;
            ppu->x = byte & 0b111;
        }        
        ppu->w = !ppu->w;
        return;
    }

    // PPUADDR
    case 0x0006:
    {
        if (ppu->w)
        {
            ppu->t.reg = (ppu->t.reg & 0xFF00) | byte;
            ppu->v.reg = ppu->t.reg;
        }
        else
            ppu->t.reg = (ppu->t.reg & 0xFF) | (((byte >> 2) & 0b111111) << 8); // bit 14 is cleared
        ppu->w = !ppu->w;
        return;
    }

    // PPUDATA
    case 0x0007:
    {
        return;
    }

    // OAMDMA
    case 0x4014:
    {
        ppu->oamdma = byte;
        ppu->oam_executing_dma = true;
        return;
    }
    }

    // Open bus.
}

// Execute a PPU clock.
void ppu_clock(struct ppu* ppu)
{
    // Increment the total number of cycles.
    ppu->enumerated_cycles++;
    ppu->frame_cycles_enumerated++;

    // Toggle the even/odd frame flag.
    ppu->even_odd_frame = !ppu->even_odd_frame;

    // Determine the current PPU timing phase.
    switch ((int)ppu_timing(ppu))
    {
    // Pre-render scanline.
    case TIMING_PRE_RENDER:
    {
        // Scanline -1/261, cycle 1: clear vblank; reset sprite 0
        if (ppu->cycle == 1 && ppu->scanline == -1)
        {
            ppu->ppustatus.vars.vblank_flag = 0;
            ppu->ppustatus.vars.sprite_0_hit_flag = 0;
            ppu->ppustatus.vars.sprite_overflow_flag = 0;
        }
        break;
    }

    // Visible scanlines.
    case TIMING_VISIBLE:
    {
        // Scanline 0, cycle 0: skip on even frames.
        if (ppu->cycle == 0 && ppu->scanline == 0 && !ppu->even_odd_frame && ppu_isrendering(ppu))
            ppu->cycle = 1;
        break;
    }

    // Post-render scanline.
    case TIMING_POST_RENDER:
    {
        break;
    }

    // Vertical-blanking scanlines.
    case TIMING_VBLANK:
    {
        // Scanline 241, cycle 1: set vblank flag
        if (ppu->cycle == 1 && ppu->scanline == 241)
            ppu->ppustatus.vars.vblank_flag = 1;
        break;
    }
    }

    // Increment the cycle and scanline count.
    if (ppu->cycle == 340)
    {
        if (ppu->scanline == 260)
            ppu->frame_complete = true;
        ppu->scanline = (ppu->scanline + 2) % 262 - 1;
    }
    ppu->cycle = (ppu->cycle + 1) % 341;
}

// Create a new PPU instance. The PPU must be reset before used.
struct ppu* ppu_alloc()
{
    // Allocate a new PPU instance.
    struct ppu* ppu = safe_malloc(sizeof(struct ppu));

    // As safe_malloc() uses calloc() internally, the registers should already
    // be set to zero.

    // Set the OAM byte pointer to the address of the OAM buffer. This is necessary
    // for OAMADDR/OAMDATA.
    ppu->oam_byte_pointer = (uint8_t*)&ppu->oam;
    
    // Return the PPU.
    return ppu;
}

// Free a CPU instance.
void ppu_free(struct ppu* ppu)
{
    free(ppu);
}