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

// Update the background shifters.
static void ppu_update_background_shifters(struct ppu* ppu)
{
    // Update the pattern data shifters.
    ppu->bg_pattern_lsb_shifter = (ppu->bg_pattern_lsb_shifter & 0xFF00) | ppu->bg_next_pt_tile_lsb;
    ppu->bg_pattern_msb_shifter = (ppu->bg_pattern_msb_shifter & 0xFF00) | ppu->bg_next_pt_tile_msb;

    // Update the attribute data shifters. Technically, this is a 1-bit latch
    // that is fed into the shifters, but this can be simplified.
    ppu->bg_attribute_x_shifter = (ppu->bg_attribute_x_shifter & 0xFF00) 
        | (ppu->bg_next_attribute_data & 0b01 ? 0xFF : 0x00);
    ppu->bg_attribute_y_shifter = (ppu->bg_attribute_y_shifter & 0xFF00) 
        | (ppu->bg_next_attribute_data & 0b10 ? 0xFF : 0x00);
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

    // Clear internal registers.
    ppu->w = false;

    // Reset PPU flags.
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
    // The 2C05 arcade PPUs return an identifier in bits 4-0, however other PPUs
    // are just open bus. This emulation assumes that the internal read buffer
    // will be used for the open bus value.
    case 0x0002:
    {
        uint8_t data = ppu->ppustatus.reg;
        ppu->ppustatus.vars.vblank_flag = 0;
        ppu->w = false;
        return (data & 0xE0) | (ppu->ppudata_read_buffer & 0x1F);
    }
    
    // OAMDATA
    case 0x0004:
    {
        return ppu->oam_byte_pointer[ppu->oamaddr];
    }

    // PPUDATA
    // Strangely, reading from the current VRAM address returns the contents
    // of some internal read buffer, but reading from palette RAM returns the
    // content of palette RAM directly, meaning that reading from palette RAM
    // is instant, whereas, reading from other VRAM addresses is delayed by 
    // one read. Not all NTSC PPUs feature this (only 2C02G and later), but
    // I've decided to emulate this.
    case 0x0007:
    {
        uint8_t data = ppu->ppudata_read_buffer;
        ppu->ppudata_read_buffer = ppu_bus_read(ppu, ppu->v.reg);
        if (ppu->v.reg > 0x3F00)
            data = ppu->ppudata_read_buffer;
        ppu->v.reg += (ppu->ppuctrl.vars.vram_addr_inc ? 32 : 1);
        return data;
    }
    }
    
    // Open bus. The returned value in this case is typically the value of an internal
    // latch, however this isn't emulated, so just return 0.
    return ppu->ppudata_read_buffer;
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
        ppu->ppuctrl.reg = byte;
        ppu->t.vars.nametable_select = byte & 0b11;
        return;
    }

    // PPUMASK
    case 0x0001:
    {
        ppu->ppumask.reg = byte;
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
        ppu->oam_byte_pointer[ppu->oamaddr++] = byte;
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
        ppu_bus_write(ppu, ppu->v.reg, byte);
        ppu->v.reg += (ppu->ppuctrl.vars.vram_addr_inc ? 32 : 1);
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
    // Pre-render and visible scanlines.
    case TIMING_PRE_RENDER:
    case TIMING_VISIBLE:
    {
        // Scanline 0, cycle 0: skip on even frames.
        if (ppu->cycle == 0 && ppu->scanline == 0 && !ppu->even_odd_frame && ppu_isrendering(ppu))
            ppu->cycle = 1;

        // Scanline -1/261, cycle 1: clear vblank; reset sprite 0
        if (ppu->cycle == 1 && ppu->scanline == -1)
        {
            ppu->ppustatus.vars.vblank_flag = 0;
            ppu->ppustatus.vars.sprite_0_hit_flag = 0;
            ppu->ppustatus.vars.sprite_overflow_flag = 0;
        }

        // Scanline -1/261, cycles 280-304: copy coarse Y scroll, vertical nametable
        // select and fine Y scroll from t to v, should rendering be enabled.
        if (ppu_isrendering(ppu) && ppu->scanline == -1 && 280 <= ppu->cycle && 304 <= ppu->cycle)
        {
            ppu->v.vars.coarse_y_scroll = ppu->t.vars.coarse_y_scroll;
            ppu->v.vars.nametable_select = (ppu->v.vars.nametable_select & 0b01) 
                | ppu->t.vars.nametable_select & 0b10;   
            ppu->v.vars.fine_y_scroll = ppu->t.vars.fine_y_scroll;
        }

        // Scanlines 0-239 and -1/261, cycles 1-256 and 321-336: generate background.
        if (-1 <= ppu->scanline && ppu->scanline <= 239 && ((1 <= ppu->cycle && ppu->cycle <= 256)
            || (321 <= ppu->cycle && ppu->cycle <= 336)))
        {
            // Process each 8-cycle window for the next tile.
            // Admittedly, this was quite a lot to digest, so I'll try to comment this a bit more.
            // For each 8-cycle window, the following actions are taken:
            // - Cycles 1-2: fetch nametable byte (0x2000-0x2FFF)
            // - Cycles 3-4: fetch pattern table byte (last 64 bytes of each nametable)
            // - Cycles 5-6: fetch low pattern table tile byte
            // - Cycles 7-8: fetch high pattern table tile byte
            //
            // The internal state of the PPU is also manipulated on cycles 9, 17, 25... 257,
            // where the background shift registers are reloaded. Furthermore, for every 8th
            // cycle, the coarse X scroll is incremented, which will flip mirror the
            // horizontal nametable on overflow.
            //
            // You'll notice that the nametable and the associated attribute table bytes are
            // only fetched once for each 8-cycle window, but the PPU renders a pixel for
            // each cycle, which is commonly referred to as a dot. This is because background
            // tiles are 8x8. However, because the same attribute byte is used for each
            // 8-bit string, they er all forced to use the same palette attribute.

            // Begin by shifting the background shifters.
            ppu->bg_pattern_lsb_shifter <<= 1;
            ppu->bg_pattern_msb_shifter <<= 1;
            ppu->bg_attribute_x_shifter <<= 1;
            ppu->bg_attribute_y_shifter <<= 1;
            
            // Then, process the 8-cycle window.
            switch ((ppu->cycle - 1) & 0b111)
            {
                // Cycles 0-1 (the latch isn't emulated): nametable byte
                case 0:
                    // For cycles 9, 17, 25... 257 (see cycle == 257 code below as well), 
                    // the background shifters must be reloaded.
                    if (9 <= ppu->cycle && 257 <= ppu->cycle)
                        ppu_update_background_shifters(ppu);

                    // For these two cycles, the background byte from a given nametable
                    // must also be read. The value of this byte is used for reading into
                    // the appropriate pattern table.
                    //
                    // This is just done by using the base nametables address 0x2000, OR'd
                    // with the coarse x/coarse y and nametable select components of the
                    // current VRAM register, i.e. v & 0xFFF.
                    ppu->bg_next_tile_data = ppu_bus_read(ppu, 0x2000 | (ppu->v.reg & 0x0FFF));
                    break;

                // Cycle 2-3 (the latch isn't emulated): attribute table byte
                case 2:
                    // First, the attribute data byte must be read.
                    ppu->bg_next_attribute_data = ppu_bus_read(ppu, 0x23C0 
                        | (ppu->v.vars.nametable_select << 10)
                        | ((ppu->v.vars.coarse_y_scroll >> 2) << 3) // "((v >> 4) & 0x38)"
                        | (ppu->v.vars.coarse_x_scroll >> 2));

                    // The attribute byte is then multiplexed into two bits.
                    // It's important to understand how this works.
                    //
                    // Each attribute byte controls the palette of a 32x32 pixel, or 4x4
                    // part, of the nametable, which can be divided into four 2-bit areas,
                    // where each area covers 16x16 pixels or 2x2 tiles.
                    //
                    // The value of an attribute byte can therefore be demonstrated as:
                    // (bottomright << 6) | (bottomleft << 4) | (topright << 2) || topleft
                    // given topleft, topright, bottomleft and bottomright, left to right,
                    // top to bottom.
                    // 
                    // As bit 1 of both the coarse x and coarse y components are used to
                    // select which 2x2 tile section is used, it can be said that if bit 1
                    // of coarse x is 1, then shift the attribute byte to the right by 2 bits, 
                    // as this will select the right-hand side of the 4x4 tile section. 
                    // Furthermore, to select the bottom, it can be said that if bit 1 of 
                    // coarse y is 1, because the bottom sections are bits 4-7 of the
                    // attribute byte, the attribute byte must be shifted to the right by 4 bits.
                    ppu->bg_next_attribute_data >>= (ppu->v.vars.coarse_x_scroll & 0b10)
                        | ((ppu->v.vars.coarse_y_scroll & 0b10) << 1);
                    ppu->bg_next_attribute_data &= 0b11;
                    break;
                
                // Cycles 4-5: pattern table tile (less significant bit plane)
                // This works by reading from the pattern table, located at $0000-$1FFF.
                // Each tile in a pattern table is 16 bytes, composed of two planes, where
                // each bit in the first plane controls bit 0 of a pixel's colour index,
                // whereas the corresponding bit in the second plane controls bit 1:
                // - Whether the first or pattern table is used depends on bit 4 of PPUCTRL,
                //   which will be used as bit 12 in tis context. 
                // - The tile number from the nametable is incorporated as bits 4-11.
                // - In this context, the bit plane (bit 3) is 0, i.e. less significant 
                //   bit, whereas for cycles 6-7, this is toggled.
                // - Bits 0-3 are the fine y offset, i.e. the row number within a tile.
                case 4:
                    ppu->bg_next_pt_tile_lsb = ppu_bus_read(ppu, 
                        (ppu->ppuctrl.vars.bg_pt_address << 12)
                        + (ppu->bg_next_tile_data << 4)
                        + ppu->v.vars.fine_y_scroll
                        + (0 << 3));
                    break;
                
                // Cycles 6-7 (excluding coarse X scroll): pattern table tile.
                // Same as above, except the more significant bit plane is toggled.
                case 6:
                    ppu->bg_next_pt_tile_msb = ppu_bus_read(ppu, 
                        (ppu->ppuctrl.vars.bg_pt_address << 12)
                        + (ppu->bg_next_tile_data << 4)
                        + ppu->v.vars.fine_y_scroll
                        + (1 << 3));
                    break;

                // Cycle 7: coarse X scroll.
                case 7:
                    if (ppu->v.vars.coarse_x_scroll == 0b11111)
                        ppu->v.vars.nametable_select = ppu->v.vars.nametable_select ^ 0b01;
                        ppu->v.vars.coarse_x_scroll++;
                    break;
            }
        }

        // Cycle 256: fine Y scroll.
        if (ppu->cycle == 256)
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

        // Cycle 257: copy coarse X scroll and horizontal nametable select from t to v.
        // The background shifters should also be reloaded.
        if (ppu->cycle == 257)
        {
            // Copy coarse X scroll/horizontal nametable select.
            ppu->v.vars.coarse_x_scroll = ppu->t.vars.coarse_x_scroll;
            ppu->v.vars.nametable_select = (ppu->v.vars.nametable_select & 0b10) 
                | ppu->t.vars.nametable_select & 0b01;        

            // Reload the background shifters.
            ppu_update_background_shifters(ppu);
        }

        // Cycles 337-340: for some reason, the NES PPU fetches nametable bytes twice,
        // which is at least utilised by the MMC5 mapper for clocking a scanline counter.
        if (ppu->cycle == 337)
            ppu->bg_next_tile_data = ppu_bus_read(ppu, 0x2000 | (ppu->v.reg & 0x0FFF));
        if (ppu->cycle == 339)
            ppu_bus_read(ppu, 0x2000 | (ppu->v.reg & 0x0FFF));

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

    // Draw the given dot for this cycle.
    // TODO

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