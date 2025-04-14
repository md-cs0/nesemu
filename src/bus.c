/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#include <stdio.h>
#include <memory.h>
#include <conio.h> // TEMPORARY, I'LL RE-INTRODUCE SDL LATER!

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
    else if (address == 0xD010)
        return computer->kbd;
    else if (address == 0xD011)
    {
        // If no keys are buffered, ignore.
        if (computer->buffer_front == NULL)
            return 0;

        // Dequeue the first buffered key.
        struct key* temp = computer->buffer_front;
        computer->kbd = temp->c | 0x80;
        computer->buffer_front = temp->next;
        if (computer->buffer_front == NULL)
            computer->buffer_rear = NULL;
        free(temp);

        // Return.
        return 0x80;
    }
    else if (address == 0xD012)
        return computer->dsp;
    else if (address == 0xD013)
        return computer->dspcr;

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
        return;
    else if (address == 0xD011)
        return;
    else if (address == 0xD012)
    {
        // Convert the current byte to an ASCII character.
        computer->dsp = byte & 0x7F;

        // Handle printing the ASCII character.
        switch (computer->dsp)
        {
        case '\r':
            putc('\n', stdout);
            break;
        case '\b':
            printf("\b \b");
            break;
        case '\x1B':
            break;
        default:
            putc(computer->dsp, stdout);
            fflush(stdout);
            break;
        }
        return;
    }
    else if (address == 0xD013)
        return;

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