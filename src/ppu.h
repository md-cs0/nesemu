/*
; Picture Processing Unit emulation. This is a 2D picture generator that generates a
; 256x240 image. Currently, only the Ricoh 2C02 is emulated.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "nes.h"

// Forward the computer struct.
struct nes;

// PPU struct definition.
struct ppu
{
    // Bind the computer to the PPU.
    struct nes* computer;

    // Register - PPUCTRL ($2000 write)
    union
    {
        struct
        {
            uint8_t nametable_addr          : 2;    // 0: $2000; 1: $2400; 2: $2800; 3: $2C00
            uint8_t vram_addr_inc           : 1;    // 0: add 1, going across; 1: add 32, going down
            uint8_t sprite_pt_address_8x8   : 1;    // 0: $0000; 1: $1000; ignored in 8x16 mode
            uint8_t bg_pt_address           : 1;    // 0: 0: $0000; 1: $1000
            uint8_t sprite_size             : 1;    // 0: 8x8 pixels; 1: 8x16 pixels
            uint8_t ppu_master_slave_select : 1;    // 0: read backdrop; 1: output colour
            uint8_t vblank_nmi_enable       : 1;    // 0: off; 1: on
        } vars;
        uint8_t reg;
    } ppuctrl;

    // Register - PPUMASK ($2001 write)
    union
    {
        struct
        {
            uint8_t greyscale               : 1;    // 0: normal colour; 1: greyscale
            uint8_t show_background_left_8p : 1;    // 0: hide; 1: show background in leftmost 8 pixels
            uint8_t show_sprites_left_8p    : 1;    // 0: hide; 1: show sprites in leftmost 8 pixels
            uint8_t background_rendering    : 1;    // 0: disable background rendering; 1: enable
            uint8_t sprite_rendering        : 1;    // 0: disable sprite rendering; 1: enable
            uint8_t emphasize_red           : 1;    // emphasize red (green on PAL/Dendy)
            uint8_t emphasize_green         : 1;    // emphasize green (red on PAL/Dendy)
            uint8_t emphasize_blue          : 1;    // emphasize blue
        } vars;
        uint8_t reg;
    } ppumask;

    // Register - PPUSTATUS ($2002 read)
    union
    {
        struct
        {
            uint8_t ppu_open_bus            : 5;    // PPU open bus or 2C05 PPU identifier
            uint8_t sprite_overflow_flag    : 1;    // sprite overflow flag
            uint8_t sprite_0_hit_flag       : 1;    // sprite 0 hit flag
            uint8_t vblank_flag             : 1;    // vblank flag, cleared on read; unreliable, use NMI
        } vars;
        uint8_t reg;
    } ppustatus;

    // Other registers.
    uint8_t oamaddr;                                // OAMADDR - sprite RAM address ($2003 write)
    uint8_t oamdata;                                // OAMDATA - sprite RAM data ($2004 read/write)
    uint8_t ppuscroll;                              // PPUSCROLL - X and Y scroll ($2005 write)
    uint8_t ppuaddr;                                // PPUADDR - VRAM address ($2006 write)
    uint8_t ppudata;                                // PPUDATA - VRAM data ($2007 read/write)
    uint8_t oamdma;                                 // OAMDMA - sprite direct memory access ($4014 write)

    // Register latches.
    bool ppuscroll_latch;
    bool ppuaddr_latch;
};

// Reset the PPU.
void ppu_reset(struct ppu* ppu);

// Handle CPU read requests from the PPU here.
uint8_t ppu_cpu_read(struct ppu* ppu, uint16_t address);

// Handle CPU write requests to the PPU here.
void ppu_cpu_write(struct ppu* ppu, uint16_t address, uint8_t byte);

// Create a new PPU instance. The PPU must be reset before used.
struct ppu* ppu_alloc();

// Free a CPU instance.
void ppu_free(struct ppu* ppu);