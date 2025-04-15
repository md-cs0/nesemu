/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#pragma once

#include <stdint.h>

#include "cartridge.h"
#include "cpu.h"

// NES computer struct definition.
struct nes
{
    // Connected hardware.
    struct cpu* cpu;
    struct ppu* ppu;
    struct cartridge* cartridge;

    // Internal RAM.
    uint8_t ram[0x0800];
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

// Create a new NES computer instance.
struct nes* nes_alloc();

// Free a NES computer instance.
void nes_free(struct nes* computer);