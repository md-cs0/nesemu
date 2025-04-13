/*
; A test of my 6502 emulation using a fork of Wozmon.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "util.h"
#include "bus.h"

// Top-level function.
int main(int argc, char** argv)
{
    // Set up the 6502 computer.
    struct bus* computer = bus_alloc();
    cpu_setbus(computer->cpu, computer);

    // Read wozmon.bin into a buffer.
    FILE* file = fopen("wozmon.bin", "rb");
    if (file == NULL)
    {
        fprintf(stderr, "could not find wozmon.bin!");
        return EXIT_FAILURE;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t* buffer = safe_malloc(size);
    fread(buffer, 1, size, file);
    fclose(file);

    // Read wozmon.bin into the 6502 computer.
    if (bus_writeprog(computer, buffer, size) != size)
    {
        fprintf(stderr, "provided wozmon.bin is longer than 256 bytes!");
        return EXIT_FAILURE;
    }

    // Reset the CPU and begin execution.
    computer->reg.kbd_5v = 1;
    cpu_reset(computer->cpu);
    for (;;)
    {
        if (!computer->cpu->cycles && computer->cpu->enumerated_cycles > 7)
            cpu_spew(computer->cpu, stdout);
        cpu_clock(computer->cpu);
    }

    // Exit.
    return EXIT_SUCCESS;
}