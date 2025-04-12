/*
; Ricoh 2A03 emulation (based on the 6502). It features the NES APU and excludes BCD support.
; (Although APU emulation will be defined in a separate translation unit.)
;
; I should note that this emulation isn't aiming to be accurate. There's many discrepancies
; that can be observed here, including the lack of T states, inaccurate interrupt timings,
; instruction cycles aren't really separate to begin with, interrupt hijackings, etc.
*/

#include <stdint.h>
#include <stdbool.h>

#include "util.h"
#include "cpu.h"

// CPU interrupt vectors.
#define NMI_VECTOR      0xFFFA
#define RESET_VECTOR    0xFFFC
#define IRQ_VECTOR      0xFFFE

// 6502 processor status flags.
enum status_flags
{
    CPUFLAG_C       = (1 << 0), // Carry
    CPUFLAG_Z       = (1 << 1), // Zero
    CPUFLAG_I       = (1 << 2), // Interrupt Disable
    CPUFLAG_D       = (1 << 3), // Decimal (shouldn't be used)
    CPUFLAG_B       = (1 << 4), // BRK (not a real flag)
    CPUFLAG_V       = (1 << 6), // Overflow
    CPUFLAG_N       = (1 << 7)  // Negative
};

// Implied: do nothing.
static bool addr_impl(struct cpu* cpu)
{
    return 0;
}

// Accumulator: the accumulator value is used as the data fetched. This does nothing as
// well and exists for semantics only.
static bool addr_a(struct cpu* cpu)
{
    return 0;
}

// Immediate: fetch the value after the opcode.
static bool addr_imm(struct cpu* cpu)
{
    cpu->addr_fetched = cpu->pc++;
    return 0;
}

// Absolute: fetch the value from address.
static bool addr_abs(struct cpu* cpu)
{
    uint8_t lo = nes_read(cpu->computer, cpu->pc++);
    uint8_t hi = nes_read(cpu->computer, cpu->pc++);
    cpu->addr_fetched = lo | (hi << 8);
    return 0;
}


// Absolute X-indexed: fetch the value from address + Y.
static bool addr_abs_x(struct cpu* cpu)
{
    uint8_t lo = nes_read(cpu->computer, cpu->pc++);
    uint8_t hi = nes_read(cpu->computer, cpu->pc++);
    uint16_t addr = lo | (hi << 8);
    cpu->addr_fetched = addr + cpu->x;
    return ((addr & 0xFF) + cpu->x) > 0xFF;
}

// Absolute Y-indexed: fetch the value from address + X.
static bool addr_abs_y(struct cpu* cpu)
{
    uint8_t lo = nes_read(cpu->computer, cpu->pc++);
    uint8_t hi = nes_read(cpu->computer, cpu->pc++);
    uint16_t addr = lo | (hi << 8);
    cpu->addr_fetched = addr + cpu->x;
    return ((addr & 0xFF) + cpu->x) > 0xFF;
}

// Zero page: fetch the value from address & 0xFF.
static bool addr_zpg(struct cpu* cpu)
{
    cpu->addr_fetched = nes_read(cpu->computer, cpu->pc++);
    return 0;
}

// Zero page X-indexed: fetch the value from (address + X) & 0xFF.
static bool addr_zpg_x(struct cpu* cpu)
{
    cpu->addr_fetched = (nes_read(cpu->computer, cpu->pc++) + cpu->x) & 0xFF;
    return 0;
}

// Zero page Y-indexed: fetch the value from (address + Y) & 0xFF.
static bool addr_zpg_y(struct cpu* cpu)
{
    cpu->addr_fetched = (nes_read(cpu->computer, cpu->pc++) + cpu->y) & 0xFF;
    return 0;
}

// Indirect: fetch the value from *ptr, or in theory it would.
// In reality, due to a bug with the NMOS 6502 where the pointer is
// $xxFF, the address at pointer $xxFF is read as *($xxFF) | *($xx00) << 8,
// not *($xxFF) | *($xxFF + 1) << 8.
static bool addr_ind(struct cpu* cpu)
{
    // Read the pointer.
    uint8_t ptr_lo = nes_read(cpu->computer, cpu->pc++);
    uint8_t ptr_hi = nes_read(cpu->computer, cpu->pc++);

    // Get the address at the pointer.
    uint8_t lo = nes_read(cpu->computer, ptr_lo | (ptr_hi << 8));
    uint16_t hi = nes_read(cpu->computer, ((ptr_lo + 1) & 0xFF) | (ptr_hi << 8));
    cpu->addr_fetched = lo | (hi << 8);
    return 0;
}

// X-indexed indirect: fetch the value from *(ptr + X).
static bool addr_x_ind(struct cpu* cpu)
{
    // Read the pointer.
    uint8_t ptr_lo = nes_read(cpu->computer, cpu->pc++);
    uint8_t ptr_hi = nes_read(cpu->computer, cpu->pc++);
    uint16_t ptr = ptr_lo | (ptr_hi << 8) + cpu->x;

    // Get the address at the pointer.
    uint8_t lo = nes_read(cpu->computer, ptr);
    uint16_t hi = nes_read(cpu->computer, ptr + 1);
    cpu->addr_fetched = lo | (hi << 8);
    return 0;
}

// Indirect Y-indexed: fetch the value from *ptr + Y.
static bool addr_ind_y(struct cpu* cpu)
{
    // Read the pointer.
    uint8_t ptr_lo = nes_read(cpu->computer, cpu->pc++);
    uint8_t ptr_hi = nes_read(cpu->computer, cpu->pc++);
    uint16_t ptr = ptr_lo | (ptr_hi << 8);

    // Get the address at the pointer.
    uint8_t lo = nes_read(cpu->computer, ptr);
    uint16_t hi = nes_read(cpu->computer, ptr + 1);
    uint16_t addr = lo | (hi << 8);
    cpu->addr_fetched = addr + cpu->y;
    return ((addr & 0xFF) + cpu->y) > 0xFF;
}

// Relative: fetch the value from PC + signed imm8.
static bool addr_rel(struct cpu* cpu)
{
    int8_t imm8 = nes_read(cpu->computer, cpu->pc++);
    cpu->addr_fetched = cpu->pc + imm8;
    return ((cpu->pc & 0xFF) + imm8) > 0xFF;
}

// Set a CPU flag.
static inline void cpu_setflag(struct cpu* cpu, enum status_flags flag, bool toggle)
{
    if (toggle)
        cpu->p |= flag;
    else
        cpu->p &= ~flag;
}

// Get a CPU flag.
static inline bool cpu_getflag(struct cpu* cpu, enum status_flags flag)
{
    return !!(cpu->p & flag);
}

// Push a byte onto the stack.
static inline void cpu_push(struct cpu* cpu, uint8_t byte)
{
    nes_write(cpu->computer, 0x100 | (cpu->s--), byte);
}

// Pop a byte off the stack.
static inline uint8_t cpu_pop(struct cpu* cpu)
{
    return nes_read(cpu->computer, 0x100 | (cpu->s++));
}

// 6502 opcode struct.
struct opcode
{
    const char* name;
    uint8_t byte;
    uint8_t cycles;
    bool (*addr_mode)(struct cpu* cpu);
    bool (*op)(struct cpu* cpu);
};

// 6502 opcode table.
struct opcode op_lookup[] = 
{
    // 0x00 - 0x0F
    {"???", 0x00, 3, addr_zpg, NULL},
    {"???", 0x01, 3, addr_zpg, NULL},
    {"???", 0x02, 3, addr_zpg, NULL},
    {"???", 0x03, 3, addr_zpg, NULL},
    {"???", 0x04, 3, addr_zpg, NULL},
    {"???", 0x05, 3, addr_zpg, NULL},
    {"???", 0x06, 3, addr_zpg, NULL},
    {"???", 0x07, 3, addr_zpg, NULL},
    {"???", 0x08, 3, addr_zpg, NULL},
    {"???", 0x09, 3, addr_zpg, NULL},
    {"???", 0x0A, 3, addr_zpg, NULL},
    {"???", 0x0B, 3, addr_zpg, NULL},
    {"???", 0x0C, 3, addr_zpg, NULL},
    {"???", 0x0D, 3, addr_zpg, NULL},
    {"???", 0x0E, 3, addr_zpg, NULL},
    {"???", 0x0F, 3, addr_zpg, NULL},

    // 0x10 - 0x1F
    {"???", 0x10, 3, addr_zpg, NULL},
    {"???", 0x11, 3, addr_zpg, NULL},
    {"???", 0x12, 3, addr_zpg, NULL},
    {"???", 0x13, 3, addr_zpg, NULL},
    {"???", 0x14, 3, addr_zpg, NULL},
    {"???", 0x15, 3, addr_zpg, NULL},
    {"???", 0x16, 3, addr_zpg, NULL},
    {"???", 0x17, 3, addr_zpg, NULL},
    {"???", 0x18, 3, addr_zpg, NULL},
    {"???", 0x19, 3, addr_zpg, NULL},
    {"???", 0x1A, 3, addr_zpg, NULL},
    {"???", 0x1B, 3, addr_zpg, NULL},
    {"???", 0x1C, 3, addr_zpg, NULL},
    {"???", 0x1D, 3, addr_zpg, NULL},
    {"???", 0x1E, 3, addr_zpg, NULL},
    {"???", 0x1F, 3, addr_zpg, NULL},

    // 0x20 - 0x2F
    {"???", 0x20, 3, addr_zpg, NULL},
    {"???", 0x21, 3, addr_zpg, NULL},
    {"???", 0x22, 3, addr_zpg, NULL},
    {"???", 0x23, 3, addr_zpg, NULL},
    {"???", 0x24, 3, addr_zpg, NULL},
    {"???", 0x25, 3, addr_zpg, NULL},
    {"???", 0x26, 3, addr_zpg, NULL},
    {"???", 0x27, 3, addr_zpg, NULL},
    {"???", 0x28, 3, addr_zpg, NULL},
    {"???", 0x29, 3, addr_zpg, NULL},
    {"???", 0x2A, 3, addr_zpg, NULL},
    {"???", 0x2B, 3, addr_zpg, NULL},
    {"???", 0x2C, 3, addr_zpg, NULL},
    {"???", 0x2D, 3, addr_zpg, NULL},
    {"???", 0x2E, 3, addr_zpg, NULL},
    {"???", 0x2F, 3, addr_zpg, NULL},

    // 0x30 - 0x3F
    {"???", 0x30, 3, addr_zpg, NULL},
    {"???", 0x31, 3, addr_zpg, NULL},
    {"???", 0x32, 3, addr_zpg, NULL},
    {"???", 0x33, 3, addr_zpg, NULL},
    {"???", 0x34, 3, addr_zpg, NULL},
    {"???", 0x35, 3, addr_zpg, NULL},
    {"???", 0x36, 3, addr_zpg, NULL},
    {"???", 0x37, 3, addr_zpg, NULL},
    {"???", 0x38, 3, addr_zpg, NULL},
    {"???", 0x39, 3, addr_zpg, NULL},
    {"???", 0x3A, 3, addr_zpg, NULL},
    {"???", 0x3B, 3, addr_zpg, NULL},
    {"???", 0x3C, 3, addr_zpg, NULL},
    {"???", 0x3D, 3, addr_zpg, NULL},
    {"???", 0x3E, 3, addr_zpg, NULL},
    {"???", 0x3F, 3, addr_zpg, NULL},

    // 0x40 - 0x4F
    {"???", 0x40, 3, addr_zpg, NULL},
    {"???", 0x41, 3, addr_zpg, NULL},
    {"???", 0x42, 3, addr_zpg, NULL},
    {"???", 0x43, 3, addr_zpg, NULL},
    {"???", 0x44, 3, addr_zpg, NULL},
    {"???", 0x45, 3, addr_zpg, NULL},
    {"???", 0x46, 3, addr_zpg, NULL},
    {"???", 0x47, 3, addr_zpg, NULL},
    {"???", 0x48, 3, addr_zpg, NULL},
    {"???", 0x49, 3, addr_zpg, NULL},
    {"???", 0x4A, 3, addr_zpg, NULL},
    {"???", 0x4B, 3, addr_zpg, NULL},
    {"???", 0x4C, 3, addr_zpg, NULL},
    {"???", 0x4D, 3, addr_zpg, NULL},
    {"???", 0x4E, 3, addr_zpg, NULL},
    {"???", 0x4F, 3, addr_zpg, NULL},

    // 0x50 - 0x5F
    {"???", 0x50, 3, addr_zpg, NULL},
    {"???", 0x51, 3, addr_zpg, NULL},
    {"???", 0x52, 3, addr_zpg, NULL},
    {"???", 0x53, 3, addr_zpg, NULL},
    {"???", 0x54, 3, addr_zpg, NULL},
    {"???", 0x55, 3, addr_zpg, NULL},
    {"???", 0x56, 3, addr_zpg, NULL},
    {"???", 0x57, 3, addr_zpg, NULL},
    {"???", 0x58, 3, addr_zpg, NULL},
    {"???", 0x59, 3, addr_zpg, NULL},
    {"???", 0x5A, 3, addr_zpg, NULL},
    {"???", 0x5B, 3, addr_zpg, NULL},
    {"???", 0x5C, 3, addr_zpg, NULL},
    {"???", 0x5D, 3, addr_zpg, NULL},
    {"???", 0x5E, 3, addr_zpg, NULL},
    {"???", 0x5F, 3, addr_zpg, NULL},

    // 0x60 - 0x6F
    {"???", 0x60, 3, addr_zpg, NULL},
    {"???", 0x61, 3, addr_zpg, NULL},
    {"???", 0x62, 3, addr_zpg, NULL},
    {"???", 0x63, 3, addr_zpg, NULL},
    {"???", 0x64, 3, addr_zpg, NULL},
    {"???", 0x65, 3, addr_zpg, NULL},
    {"???", 0x66, 3, addr_zpg, NULL},
    {"???", 0x67, 3, addr_zpg, NULL},
    {"???", 0x68, 3, addr_zpg, NULL},
    {"???", 0x68, 3, addr_zpg, NULL},
    {"???", 0x6A, 3, addr_zpg, NULL},
    {"???", 0x6B, 3, addr_zpg, NULL},
    {"???", 0x6C, 3, addr_zpg, NULL},
    {"???", 0x6D, 3, addr_zpg, NULL},
    {"???", 0x6E, 3, addr_zpg, NULL},
    {"???", 0x6F, 3, addr_zpg, NULL},

    // 0x70 - 0x7F
    {"???", 0x70, 3, addr_zpg, NULL},
    {"???", 0x71, 3, addr_zpg, NULL},
    {"???", 0x72, 3, addr_zpg, NULL},
    {"???", 0x73, 3, addr_zpg, NULL},
    {"???", 0x74, 3, addr_zpg, NULL},
    {"???", 0x75, 3, addr_zpg, NULL},
    {"???", 0x76, 3, addr_zpg, NULL},
    {"???", 0x77, 3, addr_zpg, NULL},
    {"???", 0x78, 3, addr_zpg, NULL},
    {"???", 0x79, 3, addr_zpg, NULL},
    {"???", 0x7A, 3, addr_zpg, NULL},
    {"???", 0x7B, 3, addr_zpg, NULL},
    {"???", 0x7C, 3, addr_zpg, NULL},
    {"???", 0x7D, 3, addr_zpg, NULL},
    {"???", 0x7E, 3, addr_zpg, NULL},
    {"???", 0x7F, 3, addr_zpg, NULL},

    // 0x80 - 0x8F
    {"???", 0x80, 3, addr_zpg, NULL},
    {"???", 0x81, 3, addr_zpg, NULL},
    {"???", 0x82, 3, addr_zpg, NULL},
    {"???", 0x83, 3, addr_zpg, NULL},
    {"???", 0x84, 3, addr_zpg, NULL},
    {"???", 0x85, 3, addr_zpg, NULL},
    {"???", 0x86, 3, addr_zpg, NULL},
    {"???", 0x87, 3, addr_zpg, NULL},
    {"???", 0x88, 3, addr_zpg, NULL},
    {"???", 0x89, 3, addr_zpg, NULL},
    {"???", 0x8A, 3, addr_zpg, NULL},
    {"???", 0x8B, 3, addr_zpg, NULL},
    {"???", 0x8C, 3, addr_zpg, NULL},
    {"???", 0x8D, 3, addr_zpg, NULL},
    {"???", 0x8E, 3, addr_zpg, NULL},
    {"???", 0x8F, 3, addr_zpg, NULL},

    // 0x90 - 0x9F
    {"???", 0x90, 3, addr_zpg, NULL},
    {"???", 0x91, 3, addr_zpg, NULL},
    {"???", 0x92, 3, addr_zpg, NULL},
    {"???", 0x93, 3, addr_zpg, NULL},
    {"???", 0x94, 3, addr_zpg, NULL},
    {"???", 0x95, 3, addr_zpg, NULL},
    {"???", 0x96, 3, addr_zpg, NULL},
    {"???", 0x97, 3, addr_zpg, NULL},
    {"???", 0x98, 3, addr_zpg, NULL},
    {"???", 0x99, 3, addr_zpg, NULL},
    {"???", 0x9A, 3, addr_zpg, NULL},
    {"???", 0x9B, 3, addr_zpg, NULL},
    {"???", 0x9C, 3, addr_zpg, NULL},
    {"???", 0x9D, 3, addr_zpg, NULL},
    {"???", 0x9E, 3, addr_zpg, NULL},
    {"???", 0x9F, 3, addr_zpg, NULL},

    // 0xA0 - 0xAF
    {"???", 0xA0, 3, addr_zpg, NULL},
    {"???", 0xA1, 3, addr_zpg, NULL},
    {"???", 0xA2, 3, addr_zpg, NULL},
    {"???", 0xA3, 3, addr_zpg, NULL},
    {"???", 0xA4, 3, addr_zpg, NULL},
    {"???", 0xA5, 3, addr_zpg, NULL},
    {"???", 0xA6, 3, addr_zpg, NULL},
    {"???", 0xA7, 3, addr_zpg, NULL},
    {"???", 0xA8, 3, addr_zpg, NULL},
    {"???", 0xA9, 3, addr_zpg, NULL},
    {"???", 0xAA, 3, addr_zpg, NULL},
    {"???", 0xAB, 3, addr_zpg, NULL},
    {"???", 0xAC, 3, addr_zpg, NULL},
    {"???", 0xAD, 3, addr_zpg, NULL},
    {"???", 0xAE, 3, addr_zpg, NULL},
    {"???", 0xAF, 3, addr_zpg, NULL},

    // 0xB0 - 0xBF
    {"???", 0xB0, 3, addr_zpg, NULL},
    {"???", 0xB1, 3, addr_zpg, NULL},
    {"???", 0xB2, 3, addr_zpg, NULL},
    {"???", 0xB3, 3, addr_zpg, NULL},
    {"???", 0xB4, 3, addr_zpg, NULL},
    {"???", 0xB5, 3, addr_zpg, NULL},
    {"???", 0xB6, 3, addr_zpg, NULL},
    {"???", 0xB7, 3, addr_zpg, NULL},
    {"???", 0xB8, 3, addr_zpg, NULL},
    {"???", 0xB9, 3, addr_zpg, NULL},
    {"???", 0xBA, 3, addr_zpg, NULL},
    {"???", 0xBB, 3, addr_zpg, NULL},
    {"???", 0xBC, 3, addr_zpg, NULL},
    {"???", 0xBD, 3, addr_zpg, NULL},
    {"???", 0xBE, 3, addr_zpg, NULL},
    {"???", 0xBF, 3, addr_zpg, NULL},

    // 0xC0 - 0xCF
    {"???", 0xC0, 3, addr_zpg, NULL},
    {"???", 0xC1, 3, addr_zpg, NULL},
    {"???", 0xC2, 3, addr_zpg, NULL},
    {"???", 0xC3, 3, addr_zpg, NULL},
    {"???", 0xC4, 3, addr_zpg, NULL},
    {"???", 0xC5, 3, addr_zpg, NULL},
    {"???", 0xC6, 3, addr_zpg, NULL},
    {"???", 0xC7, 3, addr_zpg, NULL},
    {"???", 0xC8, 3, addr_zpg, NULL},
    {"???", 0xC9, 3, addr_zpg, NULL},
    {"???", 0xCA, 3, addr_zpg, NULL},
    {"???", 0xCB, 3, addr_zpg, NULL},
    {"???", 0xCC, 3, addr_zpg, NULL},
    {"???", 0xCD, 3, addr_zpg, NULL},
    {"???", 0xCE, 3, addr_zpg, NULL},
    {"???", 0xCF, 3, addr_zpg, NULL},

    // 0xD0 - 0xDF
    {"???", 0xD0, 3, addr_zpg, NULL},
    {"???", 0xD1, 3, addr_zpg, NULL},
    {"???", 0xD2, 3, addr_zpg, NULL},
    {"???", 0xD3, 3, addr_zpg, NULL},
    {"???", 0xD4, 3, addr_zpg, NULL},
    {"???", 0xD5, 3, addr_zpg, NULL},
    {"???", 0xD6, 3, addr_zpg, NULL},
    {"???", 0xD7, 3, addr_zpg, NULL},
    {"???", 0xD8, 3, addr_zpg, NULL},
    {"???", 0xD9, 3, addr_zpg, NULL},
    {"???", 0xDA, 3, addr_zpg, NULL},
    {"???", 0xDB, 3, addr_zpg, NULL},
    {"???", 0xDC, 3, addr_zpg, NULL},
    {"???", 0xDD, 3, addr_zpg, NULL},
    {"???", 0xDE, 3, addr_zpg, NULL},
    {"???", 0xDF, 3, addr_zpg, NULL},

    // 0xE0 - 0xEF
    {"???", 0xE0, 3, addr_zpg, NULL},
    {"???", 0xE1, 3, addr_zpg, NULL},
    {"???", 0xE2, 3, addr_zpg, NULL},
    {"???", 0xE3, 3, addr_zpg, NULL},
    {"???", 0xE4, 3, addr_zpg, NULL},
    {"???", 0xE5, 3, addr_zpg, NULL},
    {"???", 0xE6, 3, addr_zpg, NULL},
    {"???", 0xE7, 3, addr_zpg, NULL},
    {"???", 0xE8, 3, addr_zpg, NULL},
    {"???", 0xE9, 3, addr_zpg, NULL},
    {"???", 0xEA, 3, addr_zpg, NULL},
    {"???", 0xEB, 3, addr_zpg, NULL},
    {"???", 0xEC, 3, addr_zpg, NULL},
    {"???", 0xED, 3, addr_zpg, NULL},
    {"???", 0xEE, 3, addr_zpg, NULL},
    {"???", 0xEF, 3, addr_zpg, NULL},

    // 0xF0 - 0xFF
    {"???", 0xF0, 3, addr_zpg, NULL},
    {"???", 0xF1, 3, addr_zpg, NULL},
    {"???", 0xF2, 3, addr_zpg, NULL},
    {"???", 0xF3, 3, addr_zpg, NULL},
    {"???", 0xF4, 3, addr_zpg, NULL},
    {"???", 0xF5, 3, addr_zpg, NULL},
    {"???", 0xF6, 3, addr_zpg, NULL},
    {"???", 0xF7, 3, addr_zpg, NULL},
    {"???", 0xF8, 3, addr_zpg, NULL},
    {"???", 0xF9, 3, addr_zpg, NULL},
    {"???", 0xFA, 3, addr_zpg, NULL},
    {"???", 0xFB, 3, addr_zpg, NULL},
    {"???", 0xFC, 3, addr_zpg, NULL},
    {"???", 0xFD, 3, addr_zpg, NULL},
    {"???", 0xFE, 3, addr_zpg, NULL},
    {"???", 0xFF, 3, addr_zpg, NULL},
};

// Reset the CPU. Because the RESET sequence is the hardware just forcing in a
// software BRK, the PC/processor status write sequences are still present, meaning
// the stack pointer still decrements by 3. However, the R/W line is held high,
// meaning that it "reads" instead, meaning that the PC/processor status registers
// are not pushed onto the stack.
void cpu_reset(struct cpu* cpu)
{
    // Hack the stack pointer to be S - 3.
    cpu->s -= 3;

    // Read from the reset vector.
    cpu->pc = RESET_VECTOR;
    addr_ind(cpu);
    cpu->pc = cpu->addr_fetched;
    
    // The reset sequence requires 7 cycles.
    cpu->cycles = 7;
}

// Trigger a non-maskable interrupt (falling edge-sensitive).
void cpu_nmi(struct cpu* cpu)
{
    // Push the PC and processor status.
    cpu_push(cpu, cpu->pc >> 8);
    cpu_push(cpu, cpu->pc);
    cpu_push(cpu, cpu->p);

    // Fetch the new PC.
    cpu->pc = NMI_VECTOR;
    addr_ind(cpu);
    cpu->pc = cpu->addr_fetched;

    // Wait 7 cycles.
    cpu->cycles = 7;
}

// Trigger an IRQ (low level-sensitive).
void cpu_irq(struct cpu* cpu)
{
    // Push the PC and processor status.
    cpu_push(cpu, cpu->pc >> 8);
    cpu_push(cpu, cpu->pc);
    cpu_push(cpu, cpu->p);

    // Fetch the new PC.
    cpu->pc = IRQ_VECTOR;
    addr_ind(cpu);
    cpu->pc = cpu->addr_fetched;

    // Toggle the IRQ disable flag.
    cpu_setflag(cpu, CPUFLAG_I, true);

    // Wait 7 cycles.
    cpu->cycles = 7;
}

// Execute a CPU clock.
void cpu_clock(struct cpu* cpu)
{
    // Check if there are any pending cycles still.
    if (cpu->cycles)
    {
        cpu->cycles--;
        return;
    }
    
    // Seems like we are ready to execute a new instruction. Read the given
    // opcode data.
    cpu->opcode = nes_read(cpu->computer, cpu->pc++);
    cpu->cycles = op_lookup[cpu->opcode].cycles - 1;

    // Read the appropriate address before executing the opcode itself. Depending
    // on the address mode and the opcode, an extra cycle may be used. This is because
    // the 6502 has an 8-bit ALU where the low byte of the address to read from is
    // calculated while the high byte is fetched. However, if there's a carry, the high
    // byte must be re-fetched with the carry added.
    bool page_crossed = op_lookup[cpu->opcode].addr_mode(cpu);
    cpu->cycles += (page_crossed & op_lookup[cpu->opcode].op(cpu));
}

// Create a new CPU instance. The CPU must be reset before used.
struct cpu* cpu_alloc()
{
    struct cpu* cpu = safe_malloc(sizeof(struct cpu));
    cpu->a = cpu->x = cpu->y = cpu->s = 0x00;
    cpu->p = 0b00100000;
    cpu->pc = RESET_VECTOR;
    return cpu;
}

// Free a CPU instance.
void cpu_free(struct cpu* cpu)
{
    free(cpu);
}