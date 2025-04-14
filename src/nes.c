/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#include "util.h"
#include "nes.h"

// Read a byte from a given address.
uint8_t nes_read(struct nes* computer, uint16_t address)
{
    return computer->memory[address];
}

// Write a byte to a given address.
void nes_write(struct nes* computer, uint16_t address, uint8_t byte)
{
    computer->memory[address] = byte;
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