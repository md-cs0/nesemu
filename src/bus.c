/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#include <memory.h>

#include "util.h"
#include "bus.h"

// Read a byte from a given address.
uint8_t bus_read(struct bus* computer, uint16_t address)
{
    if (0xFF00 <= address && address <= 0xFFFF)
        return computer->memory[address & 0xFF];
    assert(false);
    return 0;
}

// Write a byte to a given address.
void bus_write(struct bus* computer, uint16_t address, uint8_t byte)
{
    if (0xFF00 <= address && address <= 0xFFFF)
        computer->memory[address & 0xFF] = byte;
    assert(false);
}

// Write a program into memory.
size_t bus_writeprog(struct bus* computer, uint8_t* buffer, size_t size)
{
    size_t size_written = min(size, sizeof(computer->memory));
    memcpy(computer->memory, buffer, size_written);
    return size_written;
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