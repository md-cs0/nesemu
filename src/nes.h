/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#pragma once

#include <stdint.h>

#include "cpu.h"

// NES computer struct definition.
struct nes
{
    struct cpu* cpu;
    uint8_t memory[0x10000]; // THIS IS TEMPORARY!
};

// Read a byte from a given address.
uint8_t nes_read(struct nes* computer, uint16_t address);

// Write a byte to a given address.
void nes_write(struct nes* computer, uint16_t address, uint8_t byte);

// Create a new NES computer instance.
struct nes* nes_alloc();

// Free a NES computer instance.
void nes_free(struct nes* computer);