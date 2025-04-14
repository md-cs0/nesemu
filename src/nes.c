/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#include "util.h"
#include "nes.h"
#include "cartridge.h"

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

    // Open bus: while not accurate, just return zero.
    return 0;
}

// Write a byte to a given address.
void nes_write(struct nes* computer, uint16_t address, uint8_t byte)
{
    // Attempt to write to the cartridge. Usually $4020-$FFFF.
    if (cartridge_cpu_write(computer->cartridge, address, byte))
        return;

    // $0000-$1FFF: internal RAM.
    else if (0x0000 <= address && address <= 0x1FFF)
        computer->ram[address & 0x7FF] = byte;

    // Open bus.
}

// Create a new NES computer instance.
struct nes* nes_alloc()
{
    struct nes* computer = safe_malloc(sizeof(struct nes));
    computer->cpu = cpu_alloc();
    return computer;
}

// Free a NES computer instance.
void nes_free(struct nes* computer)
{
    if (computer == NULL)
        return;
    cpu_free(computer->cpu);
    free(computer);
}