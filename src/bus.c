/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#include <memory.h>

#include "util.h"
#include "bus.h"

// Read a byte from a given address.
uint8_t bus_read(struct bus* computer, uint16_t address)
{
    // Basic memory.
    if (0x0000 <= address && address <= 0x3FFF)
        return computer->memory[address];
    else if (0xFF00 <= address && address <= 0xFFFF)
        return computer->program[address & 0xFF];

    // Registers.
    else if (0xD010)
    {
        computer->reg.kbdcr_rdy = 0;
        return computer->reg.buffer[0];
    }
    else if (address == 0xD011)
        return computer->reg.buffer[1];
    else if (address == 0xD012)
        return computer->reg.buffer[2];
    else if (address == 0xD013)
        return computer->reg.buffer[3];

    // No address lines?
    assert(false);
    return 0;
}

// Write a byte to a given address.
void bus_write(struct bus* computer, uint16_t address, uint8_t byte)
{
    // Basic memory.
    if (0x0000 <= address && address <= 0x3FFF)
    {
        computer->memory[address] = byte;
        return;
    }
    else if (0xFF00 <= address && address <= 0xFFFF)
    {
        computer->program[address & 0xFF] = byte;
        return;
    }

    // Registers.
    else if (address == 0xD010)
    {
        computer->reg.kbd = byte;
        return;
    }
    else if (address == 0xD011)
    {
        computer->reg.buffer[1] = byte;
        return;
    }
    else if (address == 0xD012)
    {
        computer->reg.buffer[2] = byte;
        return;
    }
    else if (address == 0xD013)
    {
        computer->reg.buffer[3] = byte;
        return;
    }

    // No address lines?
    assert(false);
}

// Write a program into memory.
size_t bus_writeprog(struct bus* computer, uint8_t* buffer, size_t size)
{
    size_t size_written = min(size, sizeof(computer->program));
    memcpy(computer->program, buffer, size_written);
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