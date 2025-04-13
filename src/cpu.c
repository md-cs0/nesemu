/*
; Ricoh 2A03 emulation (based on the 6502). It features the NES APU and excludes BCD support
; (although APU emulation will be defined in a separate translation unit).
;
; I should note that this emulation isn't aiming to be accurate. There's many discrepancies
; that can be observed here, including the lack of T states, no interrupt hijackings,
; instruction cycles aren't really separate to begin with, inaccurate interrupt timings, 
; illegal opcodes (may be added in the future), etc.
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
static bool op_bcc(struct cpu* cpu);
static bool op_bcs(struct cpu* cpu);
static bool op_beq(struct cpu* cpu);
static bool op_bit(struct cpu* cpu);
static bool op_bmi(struct cpu* cpu);
static bool op_bne(struct cpu* cpu);
static bool op_bpl(struct cpu* cpu);
static bool op_brk(struct cpu* cpu);
static bool op_bvc(struct cpu* cpu);
static bool op_bvs(struct cpu* cpu);
static bool op_clc(struct cpu* cpu);
static bool op_cld(struct cpu* cpu);
static bool op_cli(struct cpu* cpu);
static bool op_clv(struct cpu* cpu);
static bool op_cmp(struct cpu* cpu);
static bool op_cpx(struct cpu* cpu);
static bool op_cpy(struct cpu* cpu);
static bool op_dec(struct cpu* cpu);
static bool op_dex(struct cpu* cpu);
static bool op_dey(struct cpu* cpu);
static bool op_eor(struct cpu* cpu);
static bool op_inc(struct cpu* cpu);
static bool op_inx(struct cpu* cpu);
static bool op_iny(struct cpu* cpu);
static bool op_jmp(struct cpu* cpu);
static bool op_jsr(struct cpu* cpu);
static bool op_lda(struct cpu* cpu);
static bool op_ldx(struct cpu* cpu);
static bool op_ldy(struct cpu* cpu);
static bool op_lsr(struct cpu* cpu);
static bool op_nop(struct cpu* cpu);
static bool op_ora(struct cpu* cpu);
static bool op_pha(struct cpu* cpu);
static bool op_php(struct cpu* cpu);
static bool op_pla(struct cpu* cpu);
static bool op_plp(struct cpu* cpu);

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
    uint8_t cycles;
    bool (*addr_mode)(struct cpu* cpu);
    bool (*op)(struct cpu* cpu);
};

// 6502 opcode table.
static struct opcode op_lookup[] = 
{
    // 0x00 - 0x0F
    {"BRK", 7, addr_impl,  op_brk},
    {"ORA", 6, addr_x_ind, op_ora},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"ORA", 3, addr_zpg,   op_ora},
    {"ASL", 5, addr_zpg,   op_asl},
    {"???", 0, addr_zpg,   NULL},
    {"PHP", 3, addr_impl,  op_php},
    {"ORA", 2, addr_imm,   op_ora},
    {"ASL", 2, addr_a,     op_asl},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"ORA", 4, addr_abs,   op_ora},
    {"ASL", 6, addr_abs,   op_asl},
    {"???", 0, addr_zpg,   NULL},

    // 0x10 - 0x1F
    {"BPL", 2, addr_rel,   op_bpl},
    {"ORA", 5, addr_ind_y, op_ora},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"ORA", 4, addr_zpg_x, op_ora},
    {"ASL", 6, addr_zpg_x, op_asl},
    {"???", 0, addr_zpg,   NULL},
    {"CLC", 2, addr_impl,  op_clc},
    {"ORA", 4, addr_abs_y, op_ora},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"ORA", 4, addr_abs_x, op_ora},
    {"ASL", 7, addr_abs_x, op_asl},
    {"???", 0, addr_zpg,   NULL},

    // 0x20 - 0x2F
    {"JSR", 6, addr_abs,   op_jsr},
    {"AND", 6, addr_x_ind, op_and},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"BIT", 3, addr_zpg,   op_bit},
    {"AND", 3, addr_zpg,   op_and},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"PLP", 4, addr_impl,  op_plp},
    {"AND", 2, addr_imm,   op_and},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"BIT", 0, addr_abs,   op_bit},
    {"AND", 4, addr_abs,   op_and},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},

    // 0x30 - 0x3F
    {"BMI", 2, addr_rel,   op_bmi},
    {"AND", 5, addr_ind_y, op_and},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"AND", 4, addr_zpg_x, op_and},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"AND", 4, addr_abs_y, op_and},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"AND", 4, addr_abs_x, op_and},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},

    // 0x40 - 0x4F
    {"???", 0, addr_zpg,   NULL},
    {"EOR", 6, addr_x_ind, op_eor},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"EOR", 3, addr_zpg,   op_eor},
    {"LSR", 5, addr_zpg,   op_lsr},
    {"???", 0, addr_zpg,   NULL},
    {"PHA", 3, addr_impl,  op_pha},
    {"EOR", 2, addr_imm,   op_eor},
    {"LSR", 2, addr_a,     op_lsr},
    {"???", 0, addr_zpg,   NULL},
    {"JMP", 3, addr_abs,   op_jmp},
    {"EOR", 4, addr_abs,   op_eor},
    {"LSR", 6, addr_abs,   op_lsr},
    {"???", 0, addr_zpg,   NULL},

    // 0x50 - 0x5F
    {"BVC", 2, addr_rel,   op_bvc},
    {"EOR", 5, addr_ind_y, op_eor},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"EOR", 4, addr_zpg_x, op_eor},
    {"LSR", 6, addr_zpg_x, op_lsr},
    {"???", 0, addr_zpg,   NULL},
    {"CLI", 2, addr_impl,  op_cli},
    {"EOR", 4, addr_abs_y, op_eor},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"EOR", 4, addr_abs_x, op_eor},
    {"LSR", 7, addr_abs_x, op_lsr},
    {"???", 0, addr_zpg,   NULL},

    // 0x60 - 0x6F
    {"???", 0, addr_zpg,   NULL},
    {"ADC", 6, addr_x_ind, op_adc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"ADC", 3, addr_zpg,   op_adc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"PLA", 4, addr_impl,  op_pla},
    {"ADC", 2, addr_imm,   op_adc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"JMP", 5, addr_ind,   op_jmp},
    {"ADC", 4, addr_abs,   op_adc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},

    // 0x70 - 0x7F
    {"BVS", 2, addr_rel,   op_bvs},
    {"ADC", 5, addr_ind_y, op_adc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"ADC", 4, addr_zpg_x, op_adc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"ADC", 4, addr_abs_y, op_adc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"ADC", 4, addr_abs_x, op_adc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},

    // 0x80 - 0x8F
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"DEY", 2, addr_impl,  op_dey},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},

    // 0x90 - 0x9F
    {"BCC", 2, addr_rel,   op_bcc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},

    // 0xA0 - 0xAF
    {"LDY", 2, addr_imm,   op_ldy},
    {"LDA", 6, addr_x_ind, op_lda},
    {"LDX", 2, addr_imm,   op_ldx},
    {"???", 0, addr_zpg,   NULL},
    {"LDY", 3, addr_zpg,   op_ldy},
    {"LDA", 3, addr_zpg,   op_lda},
    {"LDX", 3, addr_zpg,   op_ldx},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"LDA", 2, addr_imm,   op_lda},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"LDY", 4, addr_abs,   op_ldy},
    {"LDA", 4, addr_abs,   op_lda},
    {"LDX", 4, addr_abs,   op_ldx},
    {"???", 0, addr_zpg,   NULL},

    // 0xB0 - 0xBF
    {"BCS", 2, addr_rel,   op_bcs},
    {"LDA", 5, addr_ind_y, op_lda},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"LDY", 4, addr_zpg_x, op_ldy},
    {"LDA", 4, addr_zpg_x, op_lda},
    {"LDX", 4, addr_zpg_y, op_ldx},
    {"???", 0, addr_zpg,   NULL},
    {"CLV", 2, addr_impl,  op_clv},
    {"LDA", 4, addr_abs_y, op_lda},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"LDY", 4, addr_abs_x, op_ldy},
    {"LDA", 4, addr_abs_x, op_lda},
    {"LDX", 4, addr_abs_y, op_ldx},
    {"???", 0, addr_zpg,   NULL},

    // 0xC0 - 0xCF
    {"CPY", 2, addr_imm,   op_cpy},
    {"CMP", 6, addr_x_ind, op_cmp},
    {"???", 0, addr_zpg,   NULL},
    {"CPY", 3, addr_zpg,   op_cpy},
    {"???", 0, addr_zpg,   NULL},
    {"CMP", 3, addr_zpg,   op_cmp},
    {"DEC", 5, addr_zpg,   op_dec},
    {"???", 0, addr_zpg,   NULL},
    {"INY", 2, addr_impl,  op_iny},
    {"CMP", 2, addr_imm,   op_cmp},
    {"DEX", 2, addr_impl,  op_dex},
    {"???", 0, addr_zpg,   NULL},
    {"CPY", 4, addr_abs,   op_cpy},
    {"CMP", 4, addr_abs,   op_cmp},
    {"DEC", 6, addr_abs,   op_dec},
    {"???", 0, addr_zpg,   NULL},

    // 0xD0 - 0xDF
    {"BNE", 2, addr_rel,   op_bne},
    {"CMP", 5, addr_ind_y, op_cmp},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"CMP", 4, addr_zpg_x, op_cmp},
    {"DEC", 6, addr_zpg_x, op_dec},
    {"???", 0, addr_zpg,   NULL},
    {"CLD", 2, addr_impl,  op_cld},
    {"CMP", 4, addr_zpg_y, op_cmp},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"CMP", 4, addr_abs_x, op_cmp},
    {"DEC", 7, addr_abs_x, op_dec},
    {"???", 0, addr_zpg,   NULL},

    // 0xE0 - 0xEF
    {"CPX", 2, addr_imm,   op_cpx},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"CPX", 3, addr_zpg,   op_cpx},
    {"???", 0, addr_zpg,   NULL},
    {"INC", 5, addr_zpg,   op_inc},
    {"???", 0, addr_zpg,   NULL},
    {"INX", 2, addr_impl,  op_inx},
    {"???", 0, addr_zpg,   NULL},
    {"NOP", 2, addr_impl,  op_nop},
    {"???", 0, addr_zpg,   NULL},
    {"CPX", 4, addr_abs,   op_cpx},
    {"???", 0, addr_zpg,   NULL},
    {"INC", 6, addr_abs,   op_inc},
    {"???", 0, addr_zpg,   NULL},

    // 0xF0 - 0xFF
    {"BEQ", 2, addr_rel,   op_beq},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"INC", 6, addr_zpg_x, op_inc},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"???", 0, addr_zpg,   NULL},
    {"INC", 7, addr_abs_x, op_inc},
    {"???", 0, addr_zpg,   NULL}
};

// Implied: do nothing.
static bool addr_impl(struct cpu* cpu)
{
    return false;
}

// Accumulator: the accumulator value is used as the data fetched. This does nothing as
// well and exists for semantics only.
static bool addr_a(struct cpu* cpu)
{
    return false;
}

// Immediate: fetch the value after the opcode.
static bool addr_imm(struct cpu* cpu)
{
    cpu->addr_fetched = cpu->pc++;
    return false;
}

// Absolute: fetch the value from address.
static bool addr_abs(struct cpu* cpu)
{
    uint8_t lo = nes_read(cpu->computer, cpu->pc++);
    uint8_t hi = nes_read(cpu->computer, cpu->pc++);
    cpu->addr_fetched = lo | (hi << 8);
    return false;
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
    return false;
}

// Zero page X-indexed: fetch the value from (address + X) & 0xFF.
static bool addr_zpg_x(struct cpu* cpu)
{
    cpu->addr_fetched = (nes_read(cpu->computer, cpu->pc++) + cpu->x) & 0xFF;
    return false;
}

// Zero page Y-indexed: fetch the value from (address + Y) & 0xFF.
static bool addr_zpg_y(struct cpu* cpu)
{
    cpu->addr_fetched = (nes_read(cpu->computer, cpu->pc++) + cpu->y) & 0xFF;
    return false;
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
    return false;
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
    return false;
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
    cpu->a &= nes_read(cpu->computer, cpu->addr_fetched);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->a == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->a & 0x80);

    // Return.
    return true;
}

// ASL: arithmetic shift left.
static bool op_asl(struct cpu* cpu)
{
    // Calculate the new value.
    uint8_t memory;
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        memory = cpu->a;
    else
        memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = memory << 1;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, memory & 0x80);
    cpu_setflag(cpu, CPUFLAG_Z, (result & 0xFF) == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        cpu->a = result;
    else
    {
        // This looks strange, but the 6502 tends to write the original value
        // back to memory before the modified value. This distinction does
        // actually matter, because writing to addresses that are used by
        // other hardware registers can trigger specific functions.
        nes_write(cpu->computer, cpu->addr_fetched, memory);
        nes_write(cpu->computer, cpu->addr_fetched, result);
    }
    return false;
}

// BCC: branch if carry clear (will take extra cycle if branch taken, may take
// another extra cycle if page crossed).
static bool op_bcc(struct cpu* cpu)
{
    // If the carry flag is set, continue.
    if (cpu_getflag(cpu, CPUFLAG_C))
        return false;

    // Take the branch.
    cpu->cycles++;
    cpu->pc = cpu->addr_fetched;
    return true;
}

// BCS: branch if carry set (will take extra cycle if branch taken, may take
// another extra cycle if page crossed).
static bool op_bcs(struct cpu* cpu)
{
    // If the carry flag is not set, continue.
    if (!cpu_getflag(cpu, CPUFLAG_C))
        return false;

    // Take the branch.
    cpu->cycles++;
    cpu->pc = cpu->addr_fetched;
    return true;
}

// BEQ: branch if equal (will take extra cycle if branch taken, may take
// another extra cycle if page crossed).
static bool op_beq(struct cpu* cpu)
{
    // If the zero flag is not set, continue.
    if (!cpu_getflag(cpu, CPUFLAG_Z))
        return false;

    // Take the branch.
    cpu->cycles++;
    cpu->pc = cpu->addr_fetched;
    return true;
}

// BIT: bit test.
static bool op_bit(struct cpu* cpu)
{
    // Get the result of accumulator & memory.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = cpu->a & memory;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, (result & 0xFF) == 0);
    cpu_setflag(cpu, CPUFLAG_V, result & 0x40);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Return.
    return false;
}

// BMI: branch if minus (will take extra cycle if branch taken, may take
// another extra cycle if page crossed).
static bool op_bmi(struct cpu* cpu)
{
    // If the negative flag is not set, continue.
    if (!cpu_getflag(cpu, CPUFLAG_N))
        return false;

    // Take the branch.
    cpu->cycles++;
    cpu->pc = cpu->addr_fetched;
    return true;
}

// BNE: branch if not equal (will take extra cycle if branch taken, may take
// another extra cycle if page crossed).
static bool op_bne(struct cpu* cpu)
{
    // If the zero flag is set, continue.
    if (cpu_getflag(cpu, CPUFLAG_Z))
        return false;

    // Take the branch.
    cpu->cycles++;
    cpu->pc = cpu->addr_fetched;
    return true;
}

// BPL: branch if plus (will take extra cycle if branch taken, may take
// another extra cycle if page crossed).
static bool op_bpl(struct cpu* cpu)
{
    // If the negative flag is set, continue.
    if (cpu_getflag(cpu, CPUFLAG_N))
        return false;

    // Take the branch.
    cpu->cycles++;
    cpu->pc = cpu->addr_fetched;
    return true;
}

// BRK: break (software IRQ). This works the same as an IRQ, except the break
// flag is pushed and the IRQ disable flag is ignored. Because PC + 2 (where 
// PC = the address that the BRK instruction is located at) is pushed to the stack,
// this is technically a 2-byte instruction. BRK suffers from interrupt hijacks,
// however this is not emulated here.
static bool op_brk(struct cpu* cpu)
{
    // Push the PC and processor status.
    cpu_push(cpu, (cpu->pc++) >> 8);
    cpu_push(cpu, cpu->pc);
    cpu_push(cpu, cpu->p | 0b00010000); // The break flag is pushed.

    // Fetch the new PC.
    cpu->pc = IRQ_VECTOR;
    addr_ind(cpu);
    cpu->pc = cpu->addr_fetched;

    // Toggle the IRQ disable flag.
    cpu_setflag(cpu, CPUFLAG_I, true);

    // Return.
    return false;
}

// BVC: branch if overflow clear (will take extra cycle if branch taken, may take
// another extra cycle if page crossed).
static bool op_bvc(struct cpu* cpu)
{
    // If the overflow flag is set, continue.
    if (cpu_getflag(cpu, CPUFLAG_V))
        return false;

    // Take the branch.
    cpu->cycles++;
    cpu->pc = cpu->addr_fetched;
    return true;
}

// BVS: branch if overflow set (will take extra cycle if branch taken, may take
// another extra cycle if page crossed).
static bool op_bvs(struct cpu* cpu)
{
    // If the overflow flag is not set, continue.
    if (!cpu_getflag(cpu, CPUFLAG_V))
        return false;

    // Take the branch.
    cpu->cycles++;
    cpu->pc = cpu->addr_fetched;
    return true;
}

// CLC: clear the carry flag.
static bool op_clc(struct cpu* cpu)
{
    cpu_setflag(cpu, CPUFLAG_C, false);
    return false;
}

// CLD: clear the decimal flag.
static bool op_cld(struct cpu* cpu)
{
    cpu_setflag(cpu, CPUFLAG_D, false);
    return false;
}

// CLI: clear the interrupt disable flag. If IRQ is held low, the IRQ isn't triggered
// until after the next instruction following this one.
static bool op_cli(struct cpu* cpu)
{
    cpu_setflag(cpu, CPUFLAG_I, false);
    cpu->irq_cli_disable = true;
    return false;
}

// CLV: clear the overflow flag.
static bool op_clv(struct cpu* cpu)
{
    cpu_setflag(cpu, CPUFLAG_V, false);
    return false;
}

// CMP: compare A to memory (may take extra cycle if page crossed).
static bool op_cmp(struct cpu* cpu)
{
    // Get the result of accumulator - memory.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = cpu->a - memory;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, cpu->a >= memory);
    cpu_setflag(cpu, CPUFLAG_Z, cpu->a == memory);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Return.
    return true;
}

// CPX: compare X to memory.
static bool op_cpx(struct cpu* cpu)
{
    // Get the result of accumulator - memory.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = cpu->x - memory;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, cpu->x >= memory);
    cpu_setflag(cpu, CPUFLAG_Z, cpu->x == memory);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Return.
    return false;
}

// CPY: compare Y to memory.
static bool op_cpy(struct cpu* cpu)
{
    // Get the result of accumulator - memory.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = cpu->y - memory;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, cpu->y >= memory);
    cpu_setflag(cpu, CPUFLAG_Z, cpu->y == memory);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Return.
    return false;
}

// DEC: decrement memory.
static bool op_dec(struct cpu* cpu)
{
    // Calculate the new value.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = memory - 1;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    nes_write(cpu->computer, cpu->addr_fetched, memory);
    nes_write(cpu->computer, cpu->addr_fetched, result);
    return false;
}

// DEX: decrement X.
static bool op_dex(struct cpu* cpu)
{
    // Calculate the new X value.
    cpu->x--;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->y == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->y & 0x80);

    // Return.
    return false;
}

// DEY: decrement Y.
static bool op_dey(struct cpu* cpu)
{
    // Calculate the new Y value.
    cpu->y--;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->y == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->y & 0x80);

    // Return.
    return false;
}

// EOR: bitwise exclusive or (may take extra cycle if page crossed).
// A ^ $FF can be used to achieve NOT.
static bool op_eor(struct cpu* cpu)
{
    // Calculate the new accumulator value.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = cpu->a ^ memory;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, (result & 0xFF) == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new accumulator value.
    cpu->a = result;
    return true;
}

// INC: increment memory.
static bool op_inc(struct cpu* cpu)
{
    // Calculate the new value.
    uint8_t memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = memory + 1;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    nes_write(cpu->computer, cpu->addr_fetched, memory);
    nes_write(cpu->computer, cpu->addr_fetched, result);
    return false;
}

// INX: increment X.
static bool op_inx(struct cpu* cpu)
{
    // Calculate the new X value.
    cpu->x++;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->y == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->y & 0x80);

    // Return.
    return false;
}

// INY: increment Y.
static bool op_iny(struct cpu* cpu)
{
    // Calculate the new Y value.
    cpu->y++;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->y == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->y & 0x80);

    // Return.
    return false;
}

// JMP: jump to a specific memory location.
static bool op_jmp(struct cpu* cpu)
{
    cpu->pc = cpu->addr_fetched;
    return false;
}

// JSR: jump to a subroutine (same as JMP, but PC + 2 is pushed 
// to stack too).
static bool op_jsr(struct cpu* cpu)
{
    cpu_push(cpu, (cpu->pc - 1) >> 8);
    cpu_push(cpu, (cpu->pc - 1));
    cpu->pc = cpu->addr_fetched;
    return false;
}

// LDA: load a memory value into the accumulator (may take extra 
// cycle if page crossed).
static bool op_lda(struct cpu* cpu)
{
    // Load the memory value into the accumulator.
    cpu->a = nes_read(cpu->computer, cpu->addr_fetched);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->a == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->a & 0x80);

    // Return.
    return true;
}

// LDX: load a memory value into the X register (may take extra 
// cycle if page crossed).
static bool op_ldx(struct cpu* cpu)
{
    // Load the memory value into the accumulator.
    cpu->x = nes_read(cpu->computer, cpu->addr_fetched);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->x == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->x & 0x80);

    // Return.
    return true;
}

// LDY: load a memory value into the Y register (may take extra 
// cycle if page crossed).
static bool op_ldy(struct cpu* cpu)
{
    // Load the memory value into the accumulator.
    cpu->y = nes_read(cpu->computer, cpu->addr_fetched);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->y == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->y & 0x80);

    // Return.
    return true;
}

// LSR: logical shift right.
static bool op_lsr(struct cpu* cpu)
{
    // Calculate the new value.
    uint8_t memory;
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        memory = cpu->a;
    else
        memory = nes_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = memory >> 1;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, memory & 0x01);
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        cpu->a = result;
    else
    {
        nes_write(cpu->computer, cpu->addr_fetched, memory);
        nes_write(cpu->computer, cpu->addr_fetched, result);
    }
    return false;
}

// NOP: no operation.
static bool op_nop(struct cpu* cpu)
{
    return false;
}

// ORA: bitwise ORA (may take extra cycle if page crossed).
static bool op_ora(struct cpu* cpu)
{
    // Calculate the new accumulator value.
    cpu->a |= nes_read(cpu->computer, cpu->addr_fetched);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->a == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->a & 0x80);

    // Return.
    return true;
}

// PHA: push the accumulator onto the stack.
static bool op_pha(struct cpu* cpu)
{
    cpu_push(cpu, cpu->a);
    return false;
}

// PHP: push the processor status flags onto the stack. The break flag
// is set to 1 for the pushed flags.
static bool op_php(struct cpu* cpu)
{
    cpu_push(cpu, cpu->p | 0b00010000);
    return false;
}

// PLA: pull the accumulator off the stack.
static bool op_pla(struct cpu* cpu)
{
    cpu->a = cpu_pop(cpu);
    return false;
}

// PLP: pull the processor status flags off the stack. The break flag
// is ignored.
static bool op_plp(struct cpu* cpu)
{
    cpu->p = (cpu_pop(cpu) & 0b11001111) | (cpu->p & 0b00110000);
    cpu->irq_cli_disable = true;
    return false;
}

// Trigger an IRQ (low level-sensitive).
static void cpu_irq(struct cpu* cpu)
{
    // If the IRQ disable flag is set, continue.
    if (cpu_getflag(cpu, CPUFLAG_I))
        return;

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

// Execute a CPU clock.
void cpu_clock(struct cpu* cpu)
{
    // Check if there are any pending cycles still.
    if (cpu->cycles)
    {
        cpu->cycles--;
        return;
    }

    // If the IRQ signal is held low and irq_cli_disable is false,
    // trigger an IRQ.
    if (!cpu->irq && !cpu->irq_cli_disable)
    {
        cpu_irq(cpu);
        return;
    }
    cpu->irq_cli_disable = false;
    
    // Seems like we are ready to execute a new instruction. Read the given
    // opcode data.
    cpu->opcode = nes_read(cpu->computer, cpu->pc++);
    assert(op_lookup[cpu->opcode].cycles);
    cpu->cycles = op_lookup[cpu->opcode].cycles - 1;

    // Read the appropriate address before executing the opcode itself. Depending
    // on the address mode and the opcode, an extra cycle may be used. This is because
    // the 6502 has an 8-bit ALU where the low byte of the address to read from is
    // calculated while the high byte is fetched. However, if there's a carry, the high
    // byte must be re-fetched with the carry added.
    bool page_crossed = op_lookup[cpu->opcode].addr_mode(cpu);
    cpu->cycles += (page_crossed & op_lookup[cpu->opcode].op(cpu));
    assert(cpu->cycles > 6);
}

// Create a new CPU instance. The CPU must be reset before used.
struct cpu* cpu_alloc()
{
    struct cpu* cpu = safe_malloc(sizeof(struct cpu));
    cpu->a = cpu->x = cpu->y = cpu->s = 0x00;
    cpu->p = 0b00100000;
    cpu->pc = RESET_VECTOR;
    cpu->irq = true;
    return cpu;
}

// Free a CPU instance.
void cpu_free(struct cpu* cpu)
{
    free(cpu);
}