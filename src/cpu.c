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

// Forward-declare the addressing functions.
static bool addr_impl(struct cpu* cpu);
static bool addr_a(struct cpu* cpu);
static bool addr_imm(struct cpu* cpu);
static bool addr_abs(struct cpu* cpu);
static bool addr_abs_x(struct cpu* cpu);
static bool addr_abs_y(struct cpu* cpu);
static bool addr_zpg(struct cpu* cpu);
static bool addr_zpg_x(struct cpu* cpu);
static bool addr_zpg_y(struct cpu* cpu);
static bool addr_ind(struct cpu* cpu);
static bool addr_x_ind(struct cpu* cpu);
static bool addr_ind_y(struct cpu* cpu);
static bool addr_rel(struct cpu* cpu);

// Forward-declare the opcode functions.
static bool op_adc(struct cpu* cpu);
static bool op_and(struct cpu* cpu);
static bool op_asl(struct cpu* cpu);

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
static struct opcode op_lookup[] = 
{
    // 0x00 - 0x0F
    {"???", 0x00, 0, addr_zpg,   NULL},
    {"???", 0x01, 0, addr_zpg,   NULL},
    {"???", 0x02, 0, addr_zpg,   NULL},
    {"???", 0x03, 0, addr_zpg,   NULL},
    {"???", 0x04, 0, addr_zpg,   NULL},
    {"???", 0x05, 0, addr_zpg,   NULL},
    {"ASL", 0x06, 5, addr_zpg,   op_asl},
    {"???", 0x07, 0, addr_zpg,   NULL},
    {"???", 0x08, 0, addr_zpg,   NULL},
    {"???", 0x09, 0, addr_zpg,   NULL},
    {"ASL", 0x0A, 2, addr_a,     op_asl},
    {"???", 0x0B, 0, addr_zpg,   NULL},
    {"???", 0x0C, 0, addr_zpg,   NULL},
    {"???", 0x0D, 0, addr_zpg,   NULL},
    {"ASL", 0x0E, 6, addr_abs,   op_asl},
    {"???", 0x0F, 0, addr_zpg,   NULL},

    // 0x10 - 0x1F
    {"???", 0x10, 0, addr_zpg,   NULL},
    {"???", 0x11, 0, addr_zpg,   NULL},
    {"???", 0x12, 0, addr_zpg,   NULL},
    {"???", 0x13, 0, addr_zpg,   NULL},
    {"???", 0x14, 0, addr_zpg,   NULL},
    {"???", 0x15, 0, addr_zpg,   NULL},
    {"ASL", 0x16, 6, addr_zpg_x, op_asl},
    {"???", 0x17, 0, addr_zpg,   NULL},
    {"???", 0x18, 0, addr_zpg,   NULL},
    {"???", 0x19, 0, addr_zpg,   NULL},
    {"???", 0x1A, 0, addr_zpg,   NULL},
    {"???", 0x1B, 0, addr_zpg,   NULL},
    {"???", 0x1C, 0, addr_zpg,   NULL},
    {"???", 0x1D, 0, addr_zpg,   NULL},
    {"ASL", 0x1E, 7, addr_abs_x, op_asl},
    {"???", 0x1F, 0, addr_zpg,   NULL},

    // 0x20 - 0x2F
    {"???", 0x20, 0, addr_zpg,   NULL},
    {"AND", 0x21, 6, addr_x_ind, op_and},
    {"???", 0x22, 0, addr_zpg,   NULL},
    {"???", 0x23, 0, addr_zpg,   NULL},
    {"???", 0x24, 0, addr_zpg,   NULL},
    {"AND", 0x25, 3, addr_zpg,   op_and},
    {"???", 0x26, 0, addr_zpg,   NULL},
    {"???", 0x27, 0, addr_zpg,   NULL},
    {"???", 0x28, 0, addr_zpg,   NULL},
    {"AND", 0x29, 2, addr_imm,   op_and},
    {"???", 0x2A, 0, addr_zpg,   NULL},
    {"???", 0x2B, 0, addr_zpg,   NULL},
    {"???", 0x2C, 0, addr_zpg,   NULL},
    {"AND", 0x2D, 4, addr_abs,   op_and},
    {"???", 0x2E, 0, addr_zpg,   NULL},
    {"???", 0x2F, 0, addr_zpg,   NULL},

    // 0x30 - 0x3F
    {"???", 0x30, 0, addr_zpg,   NULL},
    {"AND", 0x31, 5, addr_ind_y, op_and},
    {"???", 0x32, 0, addr_zpg,   NULL},
    {"???", 0x33, 0, addr_zpg,   NULL},
    {"???", 0x34, 0, addr_zpg,   NULL},
    {"AND", 0x35, 4, addr_zpg_x, op_and},
    {"???", 0x36, 0, addr_zpg,   NULL},
    {"???", 0x37, 0, addr_zpg,   NULL},
    {"???", 0x38, 0, addr_zpg,   NULL},
    {"AND", 0x39, 4, addr_abs_y, op_and},
    {"???", 0x3A, 0, addr_zpg,   NULL},
    {"???", 0x3B, 0, addr_zpg,   NULL},
    {"???", 0x3C, 0, addr_zpg,   NULL},
    {"AND", 0x3D, 4, addr_abs_x, op_and},
    {"???", 0x3E, 0, addr_zpg,   NULL},
    {"???", 0x3F, 0, addr_zpg,   NULL},

    // 0x40 - 0x4F
    {"???", 0x40, 0, addr_zpg,   NULL},
    {"???", 0x41, 0, addr_zpg,   NULL},
    {"???", 0x42, 0, addr_zpg,   NULL},
    {"???", 0x43, 0, addr_zpg,   NULL},
    {"???", 0x44, 0, addr_zpg,   NULL},
    {"???", 0x45, 0, addr_zpg,   NULL},
    {"???", 0x46, 0, addr_zpg,   NULL},
    {"???", 0x47, 0, addr_zpg,   NULL},
    {"???", 0x48, 0, addr_zpg,   NULL},
    {"???", 0x49, 0, addr_zpg,   NULL},
    {"???", 0x4A, 0, addr_zpg,   NULL},
    {"???", 0x4B, 0, addr_zpg,   NULL},
    {"???", 0x4C, 0, addr_zpg,   NULL},
    {"???", 0x4D, 0, addr_zpg,   NULL},
    {"???", 0x4E, 0, addr_zpg,   NULL},
    {"???", 0x4F, 0, addr_zpg,   NULL},

    // 0x50 - 0x5F
    {"???", 0x50, 0, addr_zpg,   NULL},
    {"???", 0x51, 0, addr_zpg,   NULL},
    {"???", 0x52, 0, addr_zpg,   NULL},
    {"???", 0x53, 0, addr_zpg,   NULL},
    {"???", 0x54, 0, addr_zpg,   NULL},
    {"???", 0x55, 0, addr_zpg,   NULL},
    {"???", 0x56, 0, addr_zpg,   NULL},
    {"???", 0x57, 0, addr_zpg,   NULL},
    {"???", 0x58, 0, addr_zpg,   NULL},
    {"???", 0x59, 0, addr_zpg,   NULL},
    {"???", 0x5A, 0, addr_zpg,   NULL},
    {"???", 0x5B, 0, addr_zpg,   NULL},
    {"???", 0x5C, 0, addr_zpg,   NULL},
    {"???", 0x5D, 0, addr_zpg,   NULL},
    {"???", 0x5E, 0, addr_zpg,   NULL},
    {"???", 0x5F, 0, addr_zpg,   NULL},

    // 0x60 - 0x6F
    {"???", 0x60, 0, addr_zpg,   NULL},
    {"ADC", 0x61, 6, addr_x_ind, op_adc},
    {"???", 0x62, 0, addr_zpg,   NULL},
    {"???", 0x63, 0, addr_zpg,   NULL},
    {"???", 0x64, 0, addr_zpg,   NULL},
    {"ADC", 0x65, 3, addr_zpg,   op_adc},
    {"???", 0x66, 0, addr_zpg,   NULL},
    {"???", 0x67, 0, addr_zpg,   NULL},
    {"???", 0x68, 0, addr_zpg,   NULL},
    {"ADC", 0x69, 2, addr_imm,   op_adc},
    {"???", 0x6A, 0, addr_zpg,   NULL},
    {"???", 0x6B, 0, addr_zpg,   NULL},
    {"???", 0x6C, 0, addr_zpg,   NULL},
    {"ADC", 0x6D, 4, addr_abs,   op_adc},
    {"???", 0x6E, 0, addr_zpg,   NULL},
    {"???", 0x6F, 0, addr_zpg,   NULL},

    // 0x70 - 0x7F
    {"???", 0x70, 0, addr_zpg,   NULL},
    {"ADC", 0x71, 5, addr_ind_y, op_adc},
    {"???", 0x72, 0, addr_zpg,   NULL},
    {"???", 0x73, 0, addr_zpg,   NULL},
    {"???", 0x74, 0, addr_zpg,   NULL},
    {"ADC", 0x75, 4, addr_zpg_x, op_adc},
    {"???", 0x76, 0, addr_zpg,   NULL},
    {"???", 0x77, 0, addr_zpg,   NULL},
    {"???", 0x78, 0, addr_zpg,   NULL},
    {"ADC", 0x79, 4, addr_abs_y, op_adc},
    {"???", 0x7A, 0, addr_zpg,   NULL},
    {"???", 0x7B, 0, addr_zpg,   NULL},
    {"???", 0x7C, 0, addr_zpg,   NULL},
    {"ADC", 0x7D, 4, addr_abs_x, op_adc},
    {"???", 0x7E, 0, addr_zpg,   NULL},
    {"???", 0x7F, 0, addr_zpg,   NULL},

    // 0x80 - 0x8F
    {"???", 0x80, 0, addr_zpg,   NULL},
    {"???", 0x81, 0, addr_zpg,   NULL},
    {"???", 0x82, 0, addr_zpg,   NULL},
    {"???", 0x83, 0, addr_zpg,   NULL},
    {"???", 0x84, 0, addr_zpg,   NULL},
    {"???", 0x85, 0, addr_zpg,   NULL},
    {"???", 0x86, 0, addr_zpg,   NULL},
    {"???", 0x87, 0, addr_zpg,   NULL},
    {"???", 0x88, 0, addr_zpg,   NULL},
    {"???", 0x89, 0, addr_zpg,   NULL},
    {"???", 0x8A, 0, addr_zpg,   NULL},
    {"???", 0x8B, 0, addr_zpg,   NULL},
    {"???", 0x8C, 0, addr_zpg,   NULL},
    {"???", 0x8D, 0, addr_zpg,   NULL},
    {"???", 0x8E, 0, addr_zpg,   NULL},
    {"???", 0x8F, 0, addr_zpg,   NULL},

    // 0x90 - 0x9F
    {"???", 0x90, 0, addr_zpg,   NULL},
    {"???", 0x91, 0, addr_zpg,   NULL},
    {"???", 0x92, 0, addr_zpg,   NULL},
    {"???", 0x93, 0, addr_zpg,   NULL},
    {"???", 0x94, 0, addr_zpg,   NULL},
    {"???", 0x95, 0, addr_zpg,   NULL},
    {"???", 0x96, 0, addr_zpg,   NULL},
    {"???", 0x97, 0, addr_zpg,   NULL},
    {"???", 0x98, 0, addr_zpg,   NULL},
    {"???", 0x99, 0, addr_zpg,   NULL},
    {"???", 0x9A, 0, addr_zpg,   NULL},
    {"???", 0x9B, 0, addr_zpg,   NULL},
    {"???", 0x9C, 0, addr_zpg,   NULL},
    {"???", 0x9D, 0, addr_zpg,   NULL},
    {"???", 0x9E, 0, addr_zpg,   NULL},
    {"???", 0x9F, 0, addr_zpg,   NULL},

    // 0xA0 - 0xAF
    {"???", 0xA0, 0, addr_zpg,   NULL},
    {"???", 0xA1, 0, addr_zpg,   NULL},
    {"???", 0xA2, 0, addr_zpg,   NULL},
    {"???", 0xA3, 0, addr_zpg,   NULL},
    {"???", 0xA4, 0, addr_zpg,   NULL},
    {"???", 0xA5, 0, addr_zpg,   NULL},
    {"???", 0xA6, 0, addr_zpg,   NULL},
    {"???", 0xA7, 0, addr_zpg,   NULL},
    {"???", 0xA8, 0, addr_zpg,   NULL},
    {"???", 0xA9, 0, addr_zpg,   NULL},
    {"???", 0xAA, 0, addr_zpg,   NULL},
    {"???", 0xAB, 0, addr_zpg,   NULL},
    {"???", 0xAC, 0, addr_zpg,   NULL},
    {"???", 0xAD, 0, addr_zpg,   NULL},
    {"???", 0xAE, 0, addr_zpg,   NULL},
    {"???", 0xAF, 0, addr_zpg,   NULL},

    // 0xB0 - 0xBF
    {"???", 0xB0, 0, addr_zpg,   NULL},
    {"???", 0xB1, 0, addr_zpg,   NULL},
    {"???", 0xB2, 0, addr_zpg,   NULL},
    {"???", 0xB3, 0, addr_zpg,   NULL},
    {"???", 0xB4, 0, addr_zpg,   NULL},
    {"???", 0xB5, 0, addr_zpg,   NULL},
    {"???", 0xB6, 0, addr_zpg,   NULL},
    {"???", 0xB7, 0, addr_zpg,   NULL},
    {"???", 0xB8, 0, addr_zpg,   NULL},
    {"???", 0xB9, 0, addr_zpg,   NULL},
    {"???", 0xBA, 0, addr_zpg,   NULL},
    {"???", 0xBB, 0, addr_zpg,   NULL},
    {"???", 0xBC, 0, addr_zpg,   NULL},
    {"???", 0xBD, 0, addr_zpg,   NULL},
    {"???", 0xBE, 0, addr_zpg,   NULL},
    {"???", 0xBF, 0, addr_zpg,   NULL},

    // 0xC0 - 0xCF
    {"???", 0xC0, 0, addr_zpg,   NULL},
    {"???", 0xC1, 0, addr_zpg,   NULL},
    {"???", 0xC2, 0, addr_zpg,   NULL},
    {"???", 0xC3, 0, addr_zpg,   NULL},
    {"???", 0xC4, 0, addr_zpg,   NULL},
    {"???", 0xC5, 0, addr_zpg,   NULL},
    {"???", 0xC6, 0, addr_zpg,   NULL},
    {"???", 0xC7, 0, addr_zpg,   NULL},
    {"???", 0xC8, 0, addr_zpg,   NULL},
    {"???", 0xC9, 0, addr_zpg,   NULL},
    {"???", 0xCA, 0, addr_zpg,   NULL},
    {"???", 0xCB, 0, addr_zpg,   NULL},
    {"???", 0xCC, 0, addr_zpg,   NULL},
    {"???", 0xCD, 0, addr_zpg,   NULL},
    {"???", 0xCE, 0, addr_zpg,   NULL},
    {"???", 0xCF, 0, addr_zpg,   NULL},

    // 0xD0 - 0xDF
    {"???", 0xD0, 0, addr_zpg,   NULL},
    {"???", 0xD1, 0, addr_zpg,   NULL},
    {"???", 0xD2, 0, addr_zpg,   NULL},
    {"???", 0xD3, 0, addr_zpg,   NULL},
    {"???", 0xD4, 0, addr_zpg,   NULL},
    {"???", 0xD5, 0, addr_zpg,   NULL},
    {"???", 0xD6, 0, addr_zpg,   NULL},
    {"???", 0xD7, 0, addr_zpg,   NULL},
    {"???", 0xD8, 0, addr_zpg,   NULL},
    {"???", 0xD9, 0, addr_zpg,   NULL},
    {"???", 0xDA, 0, addr_zpg,   NULL},
    {"???", 0xDB, 0, addr_zpg,   NULL},
    {"???", 0xDC, 0, addr_zpg,   NULL},
    {"???", 0xDD, 0, addr_zpg,   NULL},
    {"???", 0xDE, 0, addr_zpg,   NULL},
    {"???", 0xDF, 0, addr_zpg,   NULL},

    // 0xE0 - 0xEF
    {"???", 0xE0, 0, addr_zpg,   NULL},
    {"???", 0xE1, 0, addr_zpg,   NULL},
    {"???", 0xE2, 0, addr_zpg,   NULL},
    {"???", 0xE3, 0, addr_zpg,   NULL},
    {"???", 0xE4, 0, addr_zpg,   NULL},
    {"???", 0xE5, 0, addr_zpg,   NULL},
    {"???", 0xE6, 0, addr_zpg,   NULL},
    {"???", 0xE7, 0, addr_zpg,   NULL},
    {"???", 0xE8, 0, addr_zpg,   NULL},
    {"???", 0xE9, 0, addr_zpg,   NULL},
    {"???", 0xEA, 0, addr_zpg,   NULL},
    {"???", 0xEB, 0, addr_zpg,   NULL},
    {"???", 0xEC, 0, addr_zpg,   NULL},
    {"???", 0xED, 0, addr_zpg,   NULL},
    {"???", 0xEE, 0, addr_zpg,   NULL},
    {"???", 0xEF, 0, addr_zpg,   NULL},

    // 0xF0 - 0xFF
    {"???", 0xF0, 0, addr_zpg,   NULL},
    {"???", 0xF1, 0, addr_zpg,   NULL},
    {"???", 0xF2, 0, addr_zpg,   NULL},
    {"???", 0xF3, 0, addr_zpg,   NULL},
    {"???", 0xF4, 0, addr_zpg,   NULL},
    {"???", 0xF5, 0, addr_zpg,   NULL},
    {"???", 0xF6, 0, addr_zpg,   NULL},
    {"???", 0xF7, 0, addr_zpg,   NULL},
    {"???", 0xF8, 0, addr_zpg,   NULL},
    {"???", 0xF9, 0, addr_zpg,   NULL},
    {"???", 0xFA, 0, addr_zpg,   NULL},
    {"???", 0xFB, 0, addr_zpg,   NULL},
    {"???", 0xFC, 0, addr_zpg,   NULL},
    {"???", 0xFD, 0, addr_zpg,   NULL},
    {"???", 0xFE, 0, addr_zpg,   NULL},
    {"???", 0xFF, 0, addr_zpg,   NULL}
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

// ADC: add with carry (may take extra cycle if page crossed).
static bool op_adc(struct cpu* cpu)
{
    // Calculate the new accumulator value.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = cpu->a + memory + cpu_getflag(cpu, CPUFLAG_C);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, result > 0xFF);
    cpu_setflag(cpu, CPUFLAG_Z, (result & 0xFF) == 0);
    cpu_setflag(cpu, CPUFLAG_V, (result ^ cpu->a) & (result ^ memory) & 0x80);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new accumulator value.
    cpu->a = result;
    return true;
}

// AND: bitwise AND (may take extra cycle if page crossed).
static bool op_and(struct cpu* cpu)
{
    // Calculate the new accumulator value.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = cpu->a & memory;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, (result & 0xFF) == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new accumulator value.
    cpu->a = result;
    return true;
}

// ASL: arithmetic shift left.
static bool op_asl(struct cpu* cpu)
{
    // Calculate the new value.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = memory << 1;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, memory & 0x80);
    cpu_setflag(cpu, CPUFLAG_Z, (result & 0xFF) == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        cpu->a = result;
    else
        nes_write(cpu->computer, cpu->addr_fetched, result);
    return false;
}

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