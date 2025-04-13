/*
; Ricoh 2A03 emulation (based on the 6502). It features the NES APU and excludes BCD support
; (although APU emulation will be defined in a separate translation unit).
;
; I should note that this emulation isn't aiming to be accurate. There's many discrepancies
; that can be observed here, including the lack of T states, no interrupt hijackings,
; instruction cycles aren't really separate to begin with, inaccurate interrupt timings, 
; illegal opcodes (may be added in the future), etc.
*/

#include <stdio.h>
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
static bool op_rol(struct cpu* cpu);
static bool op_ror(struct cpu* cpu);
static bool op_rti(struct cpu* cpu);
static bool op_rts(struct cpu* cpu);
static bool op_sbc(struct cpu* cpu);
static bool op_sec(struct cpu* cpu);
static bool op_sed(struct cpu* cpu);
static bool op_sei(struct cpu* cpu);
static bool op_sta(struct cpu* cpu);
static bool op_stx(struct cpu* cpu);
static bool op_sty(struct cpu* cpu);
static bool op_tax(struct cpu* cpu);
static bool op_tay(struct cpu* cpu);
static bool op_tsx(struct cpu* cpu);
static bool op_txa(struct cpu* cpu);
static bool op_txs(struct cpu* cpu);
static bool op_tya(struct cpu* cpu);

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
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"ORA", 3, addr_zpg,   op_ora},
    {"ASL", 5, addr_zpg,   op_asl},
    {"???", 0, NULL,       NULL},
    {"PHP", 3, addr_impl,  op_php},
    {"ORA", 2, addr_imm,   op_ora},
    {"ASL", 2, addr_a,     op_asl},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"ORA", 4, addr_abs,   op_ora},
    {"ASL", 6, addr_abs,   op_asl},
    {"???", 0, NULL,       NULL},

    // 0x10 - 0x1F
    {"BPL", 2, addr_rel,   op_bpl},
    {"ORA", 5, addr_ind_y, op_ora},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"ORA", 4, addr_zpg_x, op_ora},
    {"ASL", 6, addr_zpg_x, op_asl},
    {"???", 0, NULL,       NULL},
    {"CLC", 2, addr_impl,  op_clc},
    {"ORA", 4, addr_abs_y, op_ora},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"ORA", 4, addr_abs_x, op_ora},
    {"ASL", 7, addr_abs_x, op_asl},
    {"???", 0, NULL,       NULL},

    // 0x20 - 0x2F
    {"JSR", 6, addr_abs,   op_jsr},
    {"AND", 6, addr_x_ind, op_and},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"BIT", 3, addr_zpg,   op_bit},
    {"AND", 3, addr_zpg,   op_and},
    {"ROL", 5, addr_zpg,   op_rol},
    {"???", 0, NULL,       NULL},
    {"PLP", 4, addr_impl,  op_plp},
    {"AND", 2, addr_imm,   op_and},
    {"ROL", 2, addr_a,     op_rol},
    {"???", 0, NULL,       NULL},
    {"BIT", 4, addr_abs,   op_bit},
    {"AND", 4, addr_abs,   op_and},
    {"ROL", 6, addr_abs,   op_rol},
    {"???", 0, NULL,       NULL},

    // 0x30 - 0x3F
    {"BMI", 2, addr_rel,   op_bmi},
    {"AND", 5, addr_ind_y, op_and},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"AND", 4, addr_zpg_x, op_and},
    {"ROL", 6, addr_zpg_x, op_rol},
    {"???", 0, NULL,       NULL},
    {"SEC", 2, addr_impl,  op_sec},
    {"AND", 4, addr_abs_y, op_and},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"AND", 4, addr_abs_x, op_and},
    {"ROL", 7, addr_abs_x, op_rol},
    {"???", 0, NULL,       NULL},

    // 0x40 - 0x4F
    {"RTI", 6, addr_impl,  op_rti},
    {"EOR", 6, addr_x_ind, op_eor},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"EOR", 3, addr_zpg,   op_eor},
    {"LSR", 5, addr_zpg,   op_lsr},
    {"???", 0, NULL,       NULL},
    {"PHA", 3, addr_impl,  op_pha},
    {"EOR", 2, addr_imm,   op_eor},
    {"LSR", 2, addr_a,     op_lsr},
    {"???", 0, NULL,       NULL},
    {"JMP", 3, addr_abs,   op_jmp},
    {"EOR", 4, addr_abs,   op_eor},
    {"LSR", 6, addr_abs,   op_lsr},
    {"???", 0, NULL,       NULL},

    // 0x50 - 0x5F
    {"BVC", 2, addr_rel,   op_bvc},
    {"EOR", 5, addr_ind_y, op_eor},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"EOR", 4, addr_zpg_x, op_eor},
    {"LSR", 6, addr_zpg_x, op_lsr},
    {"???", 0, NULL,       NULL},
    {"CLI", 2, addr_impl,  op_cli},
    {"EOR", 4, addr_abs_y, op_eor},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"EOR", 4, addr_abs_x, op_eor},
    {"LSR", 7, addr_abs_x, op_lsr},
    {"???", 0, NULL,       NULL},

    // 0x60 - 0x6F
    {"RTS", 6, addr_impl,  op_rts},
    {"ADC", 6, addr_x_ind, op_adc},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"ADC", 3, addr_zpg,   op_adc},
    {"ROR", 5, addr_zpg,   op_ror},
    {"???", 0, NULL,       NULL},
    {"PLA", 4, addr_impl,  op_pla},
    {"ADC", 2, addr_imm,   op_adc},
    {"ROR", 2, addr_a,     op_ror},
    {"???", 0, NULL,       NULL},
    {"JMP", 5, addr_ind,   op_jmp},
    {"ADC", 4, addr_abs,   op_adc},
    {"ROR", 6, addr_abs,   op_ror},
    {"???", 0, NULL,       NULL},

    // 0x70 - 0x7F
    {"BVS", 2, addr_rel,   op_bvs},
    {"ADC", 5, addr_ind_y, op_adc},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"ADC", 4, addr_zpg_x, op_adc},
    {"ROR", 6, addr_zpg_x, op_ror},
    {"???", 0, NULL,       NULL},
    {"SEI", 2, addr_impl,  op_sei},
    {"ADC", 4, addr_abs_y, op_adc},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"ADC", 4, addr_abs_x, op_adc},
    {"ROR", 7, addr_abs_x, op_ror},
    {"???", 0, NULL,       NULL},

    // 0x80 - 0x8F
    {"???", 0, NULL,       NULL},
    {"STA", 6, addr_x_ind, op_sta},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"STY", 3, addr_zpg,   op_sty},
    {"STA", 3, addr_zpg,   op_sta},
    {"STX", 3, addr_zpg,   op_stx},
    {"???", 0, NULL,       NULL},
    {"DEY", 2, addr_impl,  op_dey},
    {"???", 0, NULL,       NULL},
    {"TXA", 2, addr_impl,  op_txa},
    {"???", 0, NULL,       NULL},
    {"STY", 4, addr_abs,   op_sty},
    {"STA", 4, addr_abs,   op_sta},
    {"STX", 4, addr_abs,   op_stx},
    {"???", 0, NULL,       NULL},

    // 0x90 - 0x9F
    {"BCC", 2, addr_rel,   op_bcc},
    {"STA", 6, addr_ind_y, op_sta},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"STY", 4, addr_zpg_x, op_sty},
    {"STA", 4, addr_zpg_x, op_sta},
    {"STX", 4, addr_zpg_y, op_stx},
    {"???", 0, NULL,       NULL},
    {"TYA", 2, addr_impl,  op_tya},
    {"STA", 5, addr_abs_y, op_sta},
    {"TXS", 2, addr_impl,  op_txs},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"STA", 5, addr_abs_x, op_sta},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},

    // 0xA0 - 0xAF
    {"LDY", 2, addr_imm,   op_ldy},
    {"LDA", 6, addr_x_ind, op_lda},
    {"LDX", 2, addr_imm,   op_ldx},
    {"???", 0, NULL,       NULL},
    {"LDY", 3, addr_zpg,   op_ldy},
    {"LDA", 3, addr_zpg,   op_lda},
    {"LDX", 3, addr_zpg,   op_ldx},
    {"???", 0, NULL,       NULL},
    {"TAY", 2, addr_impl,  op_tay},
    {"LDA", 2, addr_imm,   op_lda},
    {"TAX", 2, addr_impl,  op_tax},
    {"???", 0, NULL,       NULL},
    {"LDY", 4, addr_abs,   op_ldy},
    {"LDA", 4, addr_abs,   op_lda},
    {"LDX", 4, addr_abs,   op_ldx},
    {"???", 0, NULL,       NULL},

    // 0xB0 - 0xBF
    {"BCS", 2, addr_rel,   op_bcs},
    {"LDA", 5, addr_ind_y, op_lda},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"LDY", 4, addr_zpg_x, op_ldy},
    {"LDA", 4, addr_zpg_x, op_lda},
    {"LDX", 4, addr_zpg_y, op_ldx},
    {"???", 0, NULL,       NULL},
    {"CLV", 2, addr_impl,  op_clv},
    {"LDA", 4, addr_abs_y, op_lda},
    {"TSX", 2, addr_impl,  op_tsx},
    {"???", 0, NULL,       NULL},
    {"LDY", 4, addr_abs_x, op_ldy},
    {"LDA", 4, addr_abs_x, op_lda},
    {"LDX", 4, addr_abs_y, op_ldx},
    {"???", 0, NULL,       NULL},

    // 0xC0 - 0xCF
    {"CPY", 2, addr_imm,   op_cpy},
    {"CMP", 6, addr_x_ind, op_cmp},
    {"???", 0, NULL,       NULL},
    {"CPY", 3, addr_zpg,   op_cpy},
    {"???", 0, NULL,       NULL},
    {"CMP", 3, addr_zpg,   op_cmp},
    {"DEC", 5, addr_zpg,   op_dec},
    {"???", 0, NULL,       NULL},
    {"INY", 2, addr_impl,  op_iny},
    {"CMP", 2, addr_imm,   op_cmp},
    {"DEX", 2, addr_impl,  op_dex},
    {"???", 0, NULL,       NULL},
    {"CPY", 4, addr_abs,   op_cpy},
    {"CMP", 4, addr_abs,   op_cmp},
    {"DEC", 6, addr_abs,   op_dec},
    {"???", 0, NULL,       NULL},

    // 0xD0 - 0xDF
    {"BNE", 2, addr_rel,   op_bne},
    {"CMP", 5, addr_ind_y, op_cmp},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"CMP", 4, addr_zpg_x, op_cmp},
    {"DEC", 6, addr_zpg_x, op_dec},
    {"???", 0, NULL,       NULL},
    {"CLD", 2, addr_impl,  op_cld},
    {"CMP", 4, addr_zpg_y, op_cmp},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"CMP", 4, addr_abs_x, op_cmp},
    {"DEC", 7, addr_abs_x, op_dec},
    {"???", 0, NULL,       NULL},

    // 0xE0 - 0xEF
    {"CPX", 2, addr_imm,   op_cpx},
    {"SBC", 6, addr_x_ind, op_sbc},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"CPX", 3, addr_zpg,   op_cpx},
    {"SBC", 3, addr_zpg,   op_sbc},
    {"INC", 5, addr_zpg,   op_inc},
    {"???", 0, NULL,       NULL},
    {"INX", 2, addr_impl,  op_inx},
    {"SBC", 2, addr_imm,   op_sbc},
    {"NOP", 2, addr_impl,  op_nop},
    {"???", 0, NULL,       NULL},
    {"CPX", 4, addr_abs,   op_cpx},
    {"SBC", 4, addr_abs,   op_sbc},
    {"INC", 6, addr_abs,   op_inc},
    {"???", 0, NULL,       NULL},

    // 0xF0 - 0xFF
    {"BEQ", 2, addr_rel,   op_beq},
    {"SBC", 5, addr_ind_y, op_sbc},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"SBC", 4, addr_zpg_x, op_sbc},
    {"INC", 6, addr_zpg_x, op_inc},
    {"???", 0, NULL,       NULL},
    {"SED", 2, addr_impl,  op_sed},
    {"SBC", 4, addr_abs_y, op_sbc},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"???", 0, NULL,       NULL},
    {"SBC", 4, addr_abs_x, op_sbc},
    {"INC", 7, addr_abs_x, op_inc},
    {"???", 0, NULL,       NULL}
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
    uint8_t lo = bus_read(cpu->computer, cpu->pc++);
    uint8_t hi = bus_read(cpu->computer, cpu->pc++);
    cpu->addr_fetched = lo | (hi << 8);
    return false;
}

// Absolute X-indexed: fetch the value from address + Y.
static bool addr_abs_x(struct cpu* cpu)
{
    uint8_t lo = bus_read(cpu->computer, cpu->pc++);
    uint8_t hi = bus_read(cpu->computer, cpu->pc++);
    uint16_t addr = lo | (hi << 8);
    cpu->addr_fetched = addr + cpu->x;
    return ((addr & 0xFF) + cpu->x) > 0xFF;
}

// Absolute Y-indexed: fetch the value from address + X.
static bool addr_abs_y(struct cpu* cpu)
{
    uint8_t lo = bus_read(cpu->computer, cpu->pc++);
    uint8_t hi = bus_read(cpu->computer, cpu->pc++);
    uint16_t addr = lo | (hi << 8);
    cpu->addr_fetched = addr + cpu->x;
    return ((addr & 0xFF) + cpu->x) > 0xFF;
}

// Zero page: fetch the value from address & 0xFF.
static bool addr_zpg(struct cpu* cpu)
{
    cpu->addr_fetched = bus_read(cpu->computer, cpu->pc++);
    return false;
}

// Zero page X-indexed: fetch the value from (address + X) & 0xFF.
static bool addr_zpg_x(struct cpu* cpu)
{
    cpu->addr_fetched = (bus_read(cpu->computer, cpu->pc++) + cpu->x) & 0xFF;
    return false;
}

// Zero page Y-indexed: fetch the value from (address + Y) & 0xFF.
static bool addr_zpg_y(struct cpu* cpu)
{
    cpu->addr_fetched = (bus_read(cpu->computer, cpu->pc++) + cpu->y) & 0xFF;
    return false;
}

// Indirect: fetch the value from *ptr, or in theory it would.
// In reality, due to a bug with the NMOS 6502 where the pointer is
// $xxFF, the address at pointer $xxFF is read as *($xxFF) | *($xx00) << 8,
// not *($xxFF) | *($xxFF + 1) << 8.
static bool addr_ind(struct cpu* cpu)
{
    // Read the pointer.
    uint8_t ptr_lo = bus_read(cpu->computer, cpu->pc++);
    uint8_t ptr_hi = bus_read(cpu->computer, cpu->pc++);

    // Get the address at the pointer.
    uint8_t lo = bus_read(cpu->computer, ptr_lo | (ptr_hi << 8));
    uint8_t hi = bus_read(cpu->computer, ((ptr_lo + 1) & 0xFF) | (ptr_hi << 8));
    cpu->addr_fetched = lo | (hi << 8);
    return false;
}

// X-indexed indirect: fetch the value from *(ptr + X).
static bool addr_x_ind(struct cpu* cpu)
{
    // Read the pointer.
    uint8_t ptr_lo = bus_read(cpu->computer, cpu->pc++);
    uint8_t ptr_hi = bus_read(cpu->computer, cpu->pc++);
    uint16_t ptr = ptr_lo | (ptr_hi << 8) + cpu->x;

    // Get the address at the pointer.
    uint8_t lo = bus_read(cpu->computer, ptr);
    uint8_t hi = bus_read(cpu->computer, ptr + 1);
    cpu->addr_fetched = lo | (hi << 8);
    return false;
}

// Indirect Y-indexed: fetch the value from *ptr + Y.
static bool addr_ind_y(struct cpu* cpu)
{
    // Read the pointer.
    uint8_t ptr_lo = bus_read(cpu->computer, cpu->pc++);
    uint8_t ptr_hi = bus_read(cpu->computer, cpu->pc++);
    uint16_t ptr = ptr_lo | (ptr_hi << 8);

    // Get the address at the pointer.
    uint8_t lo = bus_read(cpu->computer, ptr);
    uint8_t hi = bus_read(cpu->computer, ptr + 1);
    uint16_t addr = lo | (hi << 8);
    cpu->addr_fetched = addr + cpu->y;
    return ((addr & 0xFF) + cpu->y) > 0xFF;
}

// Relative: fetch the value from PC + signed imm8.
static bool addr_rel(struct cpu* cpu)
{
    int8_t imm8 = bus_read(cpu->computer, cpu->pc++);
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
    bus_write(cpu->computer, 0x100 | (cpu->s--), byte);
}

// Pop a byte off the stack.
static inline uint8_t cpu_pop(struct cpu* cpu)
{
    return bus_read(cpu->computer, 0x100 | (++cpu->s));
}

// ADC: add with carry (may take extra cycle if page crossed).
static bool op_adc(struct cpu* cpu)
{
    // Calculate the new accumulator value.
    uint8_t memory = bus_read(cpu->computer, cpu->addr_fetched);
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
    cpu->a &= bus_read(cpu->computer, cpu->addr_fetched);

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
        memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = memory << 1;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, memory & 0x80);
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        cpu->a = result;
    else
    {
        // This looks strange, but the 6502 tends to write the original value
        // back to memory before the modified value. This distinction does
        // actually matter, because writing to addresses that are used by
        // hardware registers can trigger specific functions.
        bus_write(cpu->computer, cpu->addr_fetched, memory);
        bus_write(cpu->computer, cpu->addr_fetched, result);
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
    uint8_t memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = cpu->a & memory;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
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
    addr_abs(cpu);
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
    uint8_t memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = cpu->a - memory;

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
    uint8_t memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = cpu->x - memory;

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
    uint8_t memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = cpu->y - memory;

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
    uint8_t memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = memory - 1;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    bus_write(cpu->computer, cpu->addr_fetched, memory);
    bus_write(cpu->computer, cpu->addr_fetched, result);
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
    uint8_t memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = cpu->a ^ memory;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new accumulator value.
    cpu->a = result;
    return true;
}

// INC: increment memory.
static bool op_inc(struct cpu* cpu)
{
    // Calculate the new value.
    uint8_t memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = memory + 1;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    bus_write(cpu->computer, cpu->addr_fetched, memory);
    bus_write(cpu->computer, cpu->addr_fetched, result);
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
    cpu->a = bus_read(cpu->computer, cpu->addr_fetched);

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
    cpu->x = bus_read(cpu->computer, cpu->addr_fetched);

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
    cpu->y = bus_read(cpu->computer, cpu->addr_fetched);

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
        memory = bus_read(cpu->computer, cpu->addr_fetched);
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
        bus_write(cpu->computer, cpu->addr_fetched, memory);
        bus_write(cpu->computer, cpu->addr_fetched, result);
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
    cpu->a |= bus_read(cpu->computer, cpu->addr_fetched);

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
    return false;
}

// ROL: rotate left.
static bool op_rol(struct cpu* cpu)
{
    // Calculate the new value.
    uint8_t memory;
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        memory = cpu->a;
    else
        memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = (memory << 1) | cpu_getflag(cpu, CPUFLAG_C);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, memory & 0x80);
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        cpu->a = result;
    else
    {
        bus_write(cpu->computer, cpu->addr_fetched, memory);
        bus_write(cpu->computer, cpu->addr_fetched, result);
    }
    return false;
}

// ROR: rotate left.
static bool op_ror(struct cpu* cpu)
{
    // Calculate the new value.
    uint8_t memory;
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        memory = cpu->a;
    else
        memory = bus_read(cpu->computer, cpu->addr_fetched);
    uint8_t result = (memory >> 1) | (cpu_getflag(cpu, CPUFLAG_C) << 7);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, memory & 0x01);
    cpu_setflag(cpu, CPUFLAG_Z, result == 0);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new value in the given memory location.
    if (op_lookup[cpu->opcode].addr_mode == addr_a)
        cpu->a = result;
    else
    {
        bus_write(cpu->computer, cpu->addr_fetched, memory);
        bus_write(cpu->computer, cpu->addr_fetched, result);
    }
    return false;
}

// RTI: return from interrupt. The IRQ disable flag toggle is effective
// immediately after this instruction.
static bool op_rti(struct cpu* cpu)
{
    // Pull the processor status flags.
    op_plp(cpu);
    cpu->irq_toggle = cpu_getflag(cpu, CPUFLAG_I);

    // Pull the program counter from the stack.
    op_rts(cpu);
    cpu->pc--;
    
    // Return.
    return false;
}

// RTS: return from subroutine.
static bool op_rts(struct cpu* cpu)
{
    cpu->pc = (cpu_pop(cpu) | (cpu_pop(cpu) << 8)) + 1;
    return false;
}

// SBC: subtract with carry (may take extra cycle if page crossed).
static bool op_sbc(struct cpu* cpu)
{
    // Calculate the new accumulator value.
    uint8_t memory = ~bus_read(cpu->computer, cpu->addr_fetched);
    uint16_t result = cpu->a + memory + cpu_getflag(cpu, CPUFLAG_C);

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_C, result < 0x00);
    cpu_setflag(cpu, CPUFLAG_Z, (result & 0xFF) == 0);
    cpu_setflag(cpu, CPUFLAG_V, (result ^ cpu->a) & (result ^ memory) & 0x80);
    cpu_setflag(cpu, CPUFLAG_N, result & 0x80);

    // Set the new accumulator value.
    cpu->a = result;
    return true;
}

// SEC: clear the carry flag.
static bool op_sec(struct cpu* cpu)
{
    cpu_setflag(cpu, CPUFLAG_C, true);
    return false;
}

// SED: clear the decimal flag.
static bool op_sed(struct cpu* cpu)
{
    cpu_setflag(cpu, CPUFLAG_D, true);
    return false;
}

// SEI: set the interrupt disable flag. If IRQ is held low, the IRQ is still
// triggered next instruction anyway, as the flag setting is delayed by one
// instruction.
static bool op_sei(struct cpu* cpu)
{
    cpu_setflag(cpu, CPUFLAG_I, true);
    return false;
}

// STA: store the accumulator into a given memory address.
static bool op_sta(struct cpu* cpu)
{
    bus_write(cpu->computer, cpu->addr_fetched, cpu->a);
    return false;
}

// STA: store the X register into a given memory address.
static bool op_stx(struct cpu* cpu)
{
    bus_write(cpu->computer, cpu->addr_fetched, cpu->x);
    return false;
}

// STA: store the Y register into a given memory address.
static bool op_sty(struct cpu* cpu)
{
    bus_write(cpu->computer, cpu->addr_fetched, cpu->y);
    return false;
}

// TAX: copy the accumulator to the X register.
static bool op_tax(struct cpu* cpu)
{
    // Copy the value over.
    cpu->x = cpu->a;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->x == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->x & 0x80);

    // Return.
    return false;
}

// TAY: copy the accumulator to the Y register.
static bool op_tay(struct cpu* cpu)
{
    // Copy the value over.
    cpu->y = cpu->a;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->y == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->y & 0x80);

    // Return.
    return false;
}

// TSX: copy the stack pointer to the X register.
static bool op_tsx(struct cpu* cpu)
{
    // Copy the value over.
    cpu->x = cpu->s;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->x == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->x & 0x80);

    // Return.
    return false;
}

// TXA: copy the X register to the accumulator.
static bool op_txa(struct cpu* cpu)
{
    // Copy the value over.
    cpu->a = cpu->x;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->a == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->a & 0x80);

    // Return.
    return false;
}

// TXS: copy the X register to the stack pointer.
static bool op_txs(struct cpu* cpu)
{
    cpu->s = cpu->x;
    return false;
}

// TYA: copy the Y register to the accumulatr.
static bool op_tya(struct cpu* cpu)
{
    // Copy the value over.
    cpu->a = cpu->y;

    // Calculate the new flags.
    cpu_setflag(cpu, CPUFLAG_Z, cpu->a == 0);
    cpu_setflag(cpu, CPUFLAG_N, cpu->a & 0x80);

    // Return.
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
    addr_abs(cpu);
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
    addr_abs(cpu);
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
    addr_abs(cpu);
    cpu->pc = cpu->addr_fetched;

    // Wait 7 cycles.
    cpu->cycles = 7;
}

// Execute a CPU clock.
void cpu_clock(struct cpu* cpu)
{
    // Increment the total number of cycles.
    cpu->enumerated_cycles++;

    // Check if there are any pending cycles still.
    if (cpu->cycles)
    {
        cpu->cycles--;
        return;
    }

    // If the IRQ signal is held low and the interrupt flag matches the cached toggle,
    // trigger an IRQ.
    if (!cpu->irq && cpu_getflag(cpu, CPUFLAG_I) == cpu->irq_toggle)
    {
        cpu_irq(cpu);
        return;
    }
    cpu->irq_toggle = cpu_getflag(cpu, CPUFLAG_I);
    
    // Seems like we are ready to execute a new instruction. Read the given
    // opcode data.
    cpu->last_pc = cpu->pc;
    cpu->opcode = bus_read(cpu->computer, cpu->pc++);
    assert(op_lookup[cpu->opcode].cycles);
    cpu->cycles = op_lookup[cpu->opcode].cycles - 1;

    // Read the appropriate address before executing the opcode itself. Depending
    // on the address mode and the opcode, an extra cycle may be used. This is because
    // the 6502 has an 8-bit ALU where the low byte of the address to read from is
    // calculated while the high byte is fetched. However, if there's a carry, the high
    // byte must be re-fetched with the carry added.
    bool page_crossed = op_lookup[cpu->opcode].addr_mode(cpu);
    cpu->cycles += (page_crossed & op_lookup[cpu->opcode].op(cpu));
    assert(cpu->cycles < 7);
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

// Spew information on the current CPU status.
void cpu_spew(struct cpu* cpu, FILE* stream)
{
    // Print the last PC and the bytes for the instruction.
    fprintf(stream, "%04X  ", cpu->last_pc);
    struct opcode op = op_lookup[cpu->opcode];
    uint8_t bytes = 0;
    if (op.addr_mode == addr_impl || op.addr_mode == addr_a)
        bytes = 1;
    else if (op.addr_mode == addr_imm || op.addr_mode == addr_zpg
        || op.addr_mode == addr_zpg_x || op.addr_mode == addr_zpg_y
        || op.addr_mode == addr_rel)
        bytes = 2;
    else
        bytes = 3;
    for (int i = 0; i < bytes; ++i)
    {
        fprintf(stream, "%02X ", bus_read(cpu->computer, cpu->last_pc + i));
    }
    fprintf(stream, "%*s", (3 - bytes) * 3 + 1, "");
    
    // Print the opcode itself.
    fprintf(stream, "%s ", op.name);
    if (op.addr_mode == addr_impl)
        fprintf(stream, "                            ");
    else if (op.addr_mode == addr_a)
        fprintf(stream, "A                           ");
    else if (op.addr_mode == addr_imm)
        fprintf(stream, "#$%02X                        ", bus_read(cpu->computer, cpu->last_pc + 1));
    else if (op.addr_mode == addr_abs)
    {
        uint8_t lo = bus_read(cpu->computer, cpu->last_pc + 1);
        uint8_t hi = bus_read(cpu->computer, cpu->last_pc + 2);
        fprintf(stream, "$%04X                       ", lo | (hi << 8));
    }
    else if (op.addr_mode == addr_abs_x)
    {
        uint8_t lo = bus_read(cpu->computer, cpu->last_pc + 1);
        uint8_t hi = bus_read(cpu->computer, cpu->last_pc + 2);
        fprintf(stream, "$%04X,X                     ", lo | (hi << 8));
    }
    else if (op.addr_mode == addr_abs_y)
    {
        uint8_t lo = bus_read(cpu->computer, cpu->last_pc + 1);
        uint8_t hi = bus_read(cpu->computer, cpu->last_pc + 2);
        fprintf(stream, "$%04X,Y                     ", lo | (hi << 8));
    }
    else if (op.addr_mode == addr_zpg)
        fprintf(stream, "$%02X                         ", bus_read(cpu->computer, cpu->last_pc + 1));
    else if (op.addr_mode == addr_zpg_x)
        fprintf(stream, "$%02X,X                       ", bus_read(cpu->computer, cpu->last_pc + 1));
    else if (op.addr_mode == addr_zpg_y)
        fprintf(stream, "$%02X,Y                       ", bus_read(cpu->computer, cpu->last_pc + 1));
    else if (op.addr_mode == addr_ind)
    {
        uint8_t lo = bus_read(cpu->computer, cpu->last_pc + 1);
        uint8_t hi = bus_read(cpu->computer, cpu->last_pc + 2);
        fprintf(stream, "($%04X)                     ", lo | (hi << 8));
    }
    else if (op.addr_mode == addr_x_ind)
    {
        uint8_t lo = bus_read(cpu->computer, cpu->last_pc + 1);
        uint8_t hi = bus_read(cpu->computer, cpu->last_pc + 2);
        fprintf(stream, "($%04X,X)                   ", lo | (hi << 8));
    }
    else if (op.addr_mode == addr_ind_y)
    {
        uint8_t lo = bus_read(cpu->computer, cpu->last_pc + 1);
        uint8_t hi = bus_read(cpu->computer, cpu->last_pc + 2);
        fprintf(stream, "($%04X),Y                   ", lo | (hi << 8));
    }
    else if (op.addr_mode == addr_rel)
    {
        int8_t imm8 = bus_read(cpu->computer, cpu->last_pc + 1);
        uint16_t address = cpu->last_pc + 2 + imm8;
        fprintf(stream, "$%02X                       ", address);
    }

    // Print register information.
    fprintf(stream, "A:%02X ", cpu->a);
    fprintf(stream, "X:%02X ", cpu->x);
    fprintf(stream, "Y:%02X ", cpu->y);
    fprintf(stream, "P:%02X ", cpu->p);
    fprintf(stream, "SP:%02X             ", cpu->s);

    // Print the number of enumerated cycles.
    fprintf(stream, "CYC:%llu", cpu->enumerated_cycles - 7);

    // Finish.
    fprintf(stream, "\n");
}