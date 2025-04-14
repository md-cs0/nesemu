/*
; A made-up 6502 computer bus.
*/

#pragma once

#include <stdint.h>

#include "cpu.h"

// Key node for the keyboard buffer.
struct key
{
    char c;
    struct key* next;
};

// Test 6502 computer bus.
struct bus
{
    // 6502 CPU.
    struct cpu* cpu;

    // Memory buffers specific to Wozmon.
    uint8_t memory[0x4000]; // Basic 16K RAM (should be enough for ZP/SP/input buffer). 
    uint8_t program[0x100]; // Should be large enough to load Wozmon.

    // "Registers" (there isn't really any emphasis on 6820 emulation).
    uint8_t kbd;
    uint8_t kbdcr;
    uint8_t dsp;
    uint8_t dspcr;

    // Keyboard queue buffer.
    struct key* buffer_front;
    struct key* buffer_rear;
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