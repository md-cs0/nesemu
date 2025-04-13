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
    uint8_t memory[0x10000];
};

// Read a byte from a given address.
uint8_t bus_read(struct bus* computer, uint16_t address);

// Write a byte to a given address.
void bus_write(struct bus* computer, uint16_t address, uint8_t byte);

// Create a new bus instance.
struct bus* bus_alloc();

// Free a bus instance.
void bus_free(struct bus* computer);