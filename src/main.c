/*
; A test of my 6502 emulation using a fork of Wozmon.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "SDL.h"

#include "util.h"
#include "bus.h"

// Top-level function.
int main(int argc, char** argv)
{
    // Init SDL events.
    int exit = EXIT_SUCCESS;
    SDL_Init(SDL_INIT_EVERYTHING);

    // Create a window for input. This is ugly, but this isn't meant to be a proper program.
    SDL_Window* window = SDL_CreateWindow("KEYBOARD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        300, 30, 0);
    if (window == NULL)
    {
        fprintf(stderr, "Failed to create SDL window: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    // Set up the 6502 computer.
    struct bus* computer = bus_alloc();
    cpu_setbus(computer->cpu, computer);

    // Read wozmon.bin into a buffer.
    FILE* file = fopen("wozmon.bin", "rb");
    if (file == NULL)
    {
        fprintf(stderr, "could not find wozmon.bin!");
        exit = EXIT_FAILURE;
        goto exit;
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
        exit = EXIT_FAILURE;
        goto exit;
    }

    // Reset the CPU and begin execution.
    cpu_reset(computer->cpu);
    for (;;)
    {
        // Poll SDL events.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            // Handle process exit.
            case SDL_QUIT:
                goto exit;

            // Check for the keydown event.
            case SDL_KEYDOWN:
            {
                // Read the given input. Only accept it if this is an ASCII character.
                SDL_Keycode c = event.key.keysym.sym;
                if (c < 0x80)
                {
                    // Capitalize the character if it is a letter.
                    if ('a' <= c && c <= 'z')
                        c &= ~0b00100000;

                    // If shift is held, send a colon instead of a semi-colon.
                    if (c == ';' && event.key.keysym.mod & KMOD_SHIFT)
                        c = ':';

                    // Queue the given input.
                    struct key* key = safe_malloc(sizeof(struct key));
                    key->c = c;
                    if (computer->buffer_front == NULL)
                        computer->buffer_front = key;
                    if (computer->buffer_rear != NULL)
                        computer->buffer_rear->next = key;
                    computer->buffer_rear = key;
                }
            }
            }
        }

        // Execute a new CPU clock cycle.
        //if (!computer->cpu->cycles && computer->cpu->enumerated_cycles > 7)
        //    cpu_spew(computer->cpu, stdout);
        cpu_clock(computer->cpu);
    }

    // Exit.
exit:
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit;
}