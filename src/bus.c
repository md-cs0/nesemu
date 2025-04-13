/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#include "util.h"
#include "bus.h"

// Read a byte from a given address.
uint8_t bus_read(struct bus* computer, uint16_t address)
{
    return computer->memory[address];
}

// Write a byte to a given address.
void bus_write(struct bus* computer, uint16_t address, uint8_t byte)
{
    computer->memory[address] = byte;
}

// Create a new NES computer instance.
struct bus* bus_alloc()
{
    struct bus* computer = safe_malloc(sizeof(struct bus));
    computer->cpu = cpu_alloc();
    return computer;
}

// Free a NES computer instance.
void bus_free(struct bus* computer)
{
    cpu_free(computer->cpu);
    free(computer);
}