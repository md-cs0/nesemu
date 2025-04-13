/*
; A made-up 6502 computer bus.
*/

#pragma once

#include <stdint.h>

#include "cpu.h"

// Test 6502 computer bus.
struct bus
{
    struct cpu* cpu;
    uint8_t memory[0x100]; // Should be large enough to load Wozmon.
};

// Read a byte from a given address.
uint8_t bus_read(struct bus* computer, uint16_t address);

// Write a byte to a given address.
void bus_write(struct bus* computer, uint16_t address, uint8_t byte);

// Write a program into memory.
size_t bus_writeprog(struct bus* computer, uint8_t* buffer, size_t size);

// Create a new bus instance.
struct bus* bus_alloc();

// Free a bus instance.
void bus_free(struct bus* computer);