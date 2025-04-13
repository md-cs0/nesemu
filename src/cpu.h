/*
; Ricoh 2A03 emulation (based on the 6502). It features the NES APU and excludes BCD support.
; (Although APU emulation will be defined in a separate translation unit.)
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "bus.h"

// Forward the computer struct.
struct nes;

// CPU struct definition.
struct cpu
{
    // Bind the computer to the CPU.
    struct bus* computer;

    // Registers.
    uint8_t a;              // Accumulator.
    uint8_t x;              // X index.
    uint8_t y;              // Y index.
    uint8_t p;              // Processor status flags.
    uint8_t s;              // Stack pointer; must be OR'd with 0x100!
    uint16_t pc;            // Program counter.

    // Opcode data.
    uint8_t opcode;
    uint8_t cycles;
    uint16_t addr_fetched;

    // Interrupts.
    bool irq;               // Must be set to false (i.e. held low) to invoke IRQ.
    bool irq_toggle;        // Only relevant to CLI/SEI/PLP/RTI.
};

// Bind the computer to the CPU.
inline void cpu_setbus(struct cpu* cpu, struct bus* computer)
{
    cpu->computer = computer;
}

// Reset the CPU.
void cpu_reset(struct cpu* cpu);

// Trigger a non-maskable interrupt (falling edge-sensitive).
void cpu_nmi(struct cpu* cpu);

// Execute a CPU clock.
void cpu_clock(struct cpu* cpu);

// Create a new CPU instance. The CPU must be reset before used.
struct cpu* cpu_alloc();

// Free a CPU instance.
void cpu_free(struct cpu* cpu);