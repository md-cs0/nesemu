/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"

#define MASTER_CLOCK 21477272 // Hz

// Controller struct definition.
union controller
{ 
    struct 
    {
        uint8_t right   : 1;
        uint8_t left    : 1;
        uint8_t down    : 1;
        uint8_t up      : 1;
        uint8_t start   : 1;
        uint8_t select  : 1;
        uint8_t b       : 1;
        uint8_t a       : 1;
    } vars;
    uint8_t value;
};

// NES computer struct definition.
struct nes
{
    // Connected hardware.
    struct cpu* cpu;
    struct ppu* ppu;
    struct cartridge* cartridge;
    union controller controllers[2];    // Only standard NES controllers are currently emulated.

    // Standard controller cache.
    bool controller_port_latch;         // This is technically its own register, but I am not emulating
    uint8_t controller_cache[2];        // the expansion ports.

    // Internal RAM.
    uint8_t ram[0x0800];

    // NES clock.
    uint64_t cycles;

    // OAM.
    bool oam_executing_dma;
    bool idle_cycle;
    uint8_t oam_page;
    uint8_t oam_offset;
    int16_t oam_cycle_count;
};

// Set the cartridge of the NES.
inline void nes_setcartridge(struct nes* computer, struct cartridge* cartridge)
{
    computer->cartridge = cartridge;
}

// Reset the NES.
void nes_reset(struct nes* computer);

// Read a byte from a given address.
uint8_t nes_read(struct nes* computer, uint16_t address);

// Write a byte to a given address.
void nes_write(struct nes* computer, uint16_t address, uint8_t byte);

// Clock the NES.
void nes_clock(struct nes* computer);

// Create a new NES computer instance.
struct nes* nes_alloc();

// Free a NES computer instance.
void nes_free(struct nes* computer);