/*
; Picture Processing Unit emulation. This is a 2D picture generator that generates a
; 256x240 image. Currently, only the Ricoh 2C02 is emulated.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "constants.h"
#include "nes.h"

// ABGR8888 colour type, so that the NES code is independent of SDL.
struct agbr8888
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

// Internal VRAM address register union.
union internal_vram_register
{
    struct
    {
        uint8_t coarse_x_scroll         : 5;    // coarse X scroll
        uint8_t coarse_y_scroll         : 5;    // coarse Y scroll
        uint8_t nametable_select        : 2;    // nametable select
        uint8_t fine_y_scroll           : 3;    // fine Y scroll
        uint8_t padding                 : 1;    // unused bit 15
    } vars;
    uint16_t reg;
};

// PPU struct definition.
struct ppu
{
    // Bind the computer to the PPU.
    struct nes* computer;

    // PPU RAM.
    uint8_t palette_ram[0x20];
    uint8_t vram[0x800];

    // PPU screen.
    struct agbr8888 screen[NES_H][NES_W];

    // PPU OAM.
    struct oamdata
    {
        // Y position of the sprite (top-left).
        uint8_t y;

        // Tile index number.
        union
        {   
            // 8x16
            struct 
            {
                uint8_t bank                : 1;    // 0: segment $0000; 1: segment $1000
                uint8_t tile_of_top         : 7;    // tile number for top of sprite
            } vars;

            // 8x8
            uint8_t value;
        } tile_index;

        // Sprite attributes.
        union
        {
            struct
            {
                uint8_t palette             : 2;    // palette (4 to 7) of sprite
                uint8_t unimplemented       : 3;    // unimplemented
                uint8_t priority            : 1;    // 0: in front of background; 1: behind background
                uint8_t flip_horizontally   : 1;    // if 1, flip horizontally
                uint8_t flip_vertically     : 1;    // if 1, flip vertically
            } vars;
            uint8_t value;
        } attributes;

        // X position of the sprite (top-left).
        uint8_t x;
    } oam[0x40], oam_memory[0x8];
    uint8_t* oam_byte_pointer;
    bool oam_executing_dma;

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
    // Rendering is assumed to be disabled if both bits 3 and 4 are 0.
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

    // Internal registers (used for scrolling)
    union internal_vram_register v;                 // current VRAM address
    union internal_vram_register t;                 // temporary VRAM address or of top-left onscreen tile
    uint8_t x                               : 3;    // fine X scroll
    bool w                                  : 1;    // $2005/$2006 write latch

    // PPU flags.
    bool even_odd_frame;                            // 0: even; 1: odd
    bool vbl;                                       // if set, hold CPU NMI pin low

    // Timing information.
    int16_t cycle;
    int16_t scanline;
    uint32_t frame_cycles_enumerated;
    bool frame_complete;

    // Debug information.
    uint64_t enumerated_cycles;
};

// Bind the computer to the PPU.
inline void ppu_setnes(struct ppu* ppu, struct nes* computer)
{
    ppu->computer = computer;
}

// Reset the PPU.
void ppu_reset(struct ppu* ppu);

// Read a byte from a given address on the internal PPU bus.
uint8_t ppu_bus_read(struct ppu* ppu, uint16_t address);

// Write a byte to a given address on the internal PPU bus.
void ppu_bus_write(struct ppu* ppu, uint16_t address, uint8_t byte);

// Handle CPU read requests from the PPU here.
uint8_t ppu_cpu_read(struct ppu* ppu, uint16_t address);

// Handle CPU write requests to the PPU here.
void ppu_cpu_write(struct ppu* ppu, uint16_t address, uint8_t byte);

// Execute a PPU clock.
void ppu_clock(struct ppu* ppu);

// Create a new PPU instance. The PPU must be reset before used.
struct ppu* ppu_alloc();

// Free a CPU instance.
void ppu_free(struct ppu* ppu);