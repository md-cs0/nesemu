/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#include "util.h"
#include "nes.h"
#include "cpu.h"
#include "ppu.h"
#include "cartridge.h"

// Reset the NES.
void nes_reset(struct nes* computer)
{
    cpu_reset(computer->cpu);
    ppu_reset(computer->ppu);
}

// Read a byte from a given address.
uint8_t nes_read(struct nes* computer, uint16_t address)
{
    // Attempt to read from the cartridge. Usually $4020-$FFFF.
    uint8_t byte;
    if (cartridge_cpu_read(computer->cartridge, address, &byte))
        return byte;

    // $0000-$1FFF: internal RAM.
    else if (0x0000 <= address && address <= 0x1FFF)
        return computer->ram[address & 0x7FF];

    // $2000-$3FFF: NES PPU registers.
    else if (0x2000 <= address && address <= 0x3FFF)
        return ppu_cpu_read(computer->ppu, address & 0x0007);

    // Open bus: while not accurate, just return zero.
    return 0;
}

// Write a byte to a given address.
void nes_write(struct nes* computer, uint16_t address, uint8_t byte)
{
    // Attempt to write to the cartridge. Usually $0000-$1FFF.
    if (cartridge_cpu_write(computer->cartridge, address, byte))
        return;

    // $0000-$1FFF: internal RAM.
    else if (0x0000 <= address && address <= 0x1FFF)
        computer->ram[address & 0x7FF] = byte;

    // $2000-$3FFF: NES PPU registers.
    else if (0x2000 <= address && address <= 0x3FFF)
        ppu_cpu_write(computer->ppu, address & 0x0007, byte);

    // $4014: NES OAM direct memory access.
    else if (address == 0x4014)
        return;

    // Open bus.
}

// Create a new NES computer instance.
struct nes* nes_alloc()
{
    // Allocate a new NES computer instance.
    struct nes* computer = safe_malloc(sizeof(struct nes));

    // Create the NES computer's CPU.
    computer->cpu = cpu_alloc();
    cpu_setnes(computer->cpu, computer);

    // Create the NES computer's PPU.
    computer->ppu = ppu_alloc();
    ppu_setnes(computer->ppu, computer);

    // Return the NES computer.
    return computer;
}

// Free a NES computer instance.
void nes_free(struct nes* computer)
{
    if (computer == NULL)
        return;
    ppu_free(computer->ppu);
    cpu_free(computer->cpu);
    free(computer);
}