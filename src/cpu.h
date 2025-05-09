/*
; Ricoh 2A03 emulation (based on the 6502). It features the NES APU and excludes BCD support.
; (Although APU emulation will be defined in a separate translation unit.)
*/

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "nes.h"

// CPU struct definition.
struct cpu
{
    // Bind the computer to the CPU.
    struct nes* computer;

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
    bool nmi;               // Must be set to false (i.e. held low) to invoke NMI. Because this is
                            // edge-sensitive, it must be set to true (i.e. held high) afterwards
                            // before triggering another NMI.
    bool irq;               // Must be set to false (i.e. held low) to invoke IRQ.
    bool irq_toggle;        // Only relevant to CLI/SEI/PLP/RTI.
    bool nmi_toggle;        // Internal.

    // Debug information.
    uint64_t enumerated_cycles;
};

// Bind the computer to the CPU.
inline void cpu_setnes(struct cpu* cpu, struct nes* computer)
{
    cpu->computer = computer;
}

// Reset the CPU.
void cpu_reset(struct cpu* cpu);

// Execute a CPU clock.
void cpu_clock(struct cpu* cpu);

// Create a new CPU instance. The CPU must be reset before used.
struct cpu* cpu_alloc();

// Free a CPU instance.
void cpu_free(struct cpu* cpu);

// Spew information on the current CPU status.
void cpu_spew(struct cpu* cpu, uint16_t pc, FILE* stream);