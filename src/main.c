/*
; Top-level translation unit; used for invoking NES computer and outputting the
; display/audio behind the scenes via SDL.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "SDL.h"

#include "constants.h"
#include "util.h"
#include "nes.h"

// Create display information for this current session.
struct nes_display_data
{
    // SDL data.
    SDL_Window* window;     // The window itself.    
    SDL_Renderer* renderer; // The renderer used for the window.
    SDL_Texture* buffer;    // The buffer that is being manipulated by the emulator.
    unsigned w;             // The current display width.
    unsigned h;             // The current display height.

    // NES data.
    struct nes* computer;
    struct cartridge* cartridge;
};
static struct nes_display_data display;

// Unload SDL on process exit.
static void process_exit()
{
    // Clear up the NES emulator.
    nes_free(display.computer);
    cartridge_free(display.cartridge);

    // Clear up SDL.
    SDL_DestroyTexture(display.buffer);
    SDL_DestroyRenderer(display.renderer);
    SDL_DestroyWindow(display.window);
    SDL_Quit();
}

// Catch SDL error and exit abruptly.
static void sdl_error()
{
    char error_msg[256];
    printf("SDL ERROR CAUGHT: %s\n", SDL_GetErrorMsg(error_msg, sizeof(error_msg)));
    exit(EXIT_FAILURE);
}

// Update the renderer.
static void update_render()
{
    // Get the current window size. We need this in order to calculate the position
    // of the current display.
    int w, h;
    SDL_GetWindowSize(display.window, &w, &h);

    // Calculate the rect that the display should be drawn onto and re-draw the display.
    struct SDL_Rect rect = {(w - display.w) / 2, (h - display.h) / 2, display.w, display.h};
    SDL_RenderClear(display.renderer);
    SDL_RenderCopy(display.renderer, display.buffer, NULL, &rect);
    SDL_RenderPresent(display.renderer);
}

// A separate event watch is needed to handle window resizing.
static int watcher(void* userdata, SDL_Event* event)
{
    // If this is not a window event, continue.
    if (event->type != SDL_WINDOWEVENT)
        return 0;

    // Get the current window event type.
    switch (event->window.event)
    {
    // The window is being resized - re-scale the display and update the rendered output.
    case SDL_WINDOWEVENT_RESIZED:
        float scale = min((float)event->window.data1 / NES_W, (float)event->window.data2 / NES_H);
        display.w = NES_W * scale;
        display.h = NES_H * scale;
        update_render();
        break;
    }

    // Exit.
    return 0;
}

// Top-level function.
int main(int argc, char** argv)
{
    // Before anything is initialized, the cartridge file should be read into
    // memory first. Check if it actually exists first.
    if (argc < 2)
        goto no_cartridge;
    uint8_t* ines_data;
    size_t ines_size;
    FILE* ines = fopen(argv[1], "rb");
    if (ines == NULL)
        goto no_cartridge;

    // Read from the cartridge file into memory.
    fseek(ines, 0, SEEK_END);
    ines_size = ftell(ines);
    ines_data = safe_malloc(ines_size);
    fseek(ines, 0, SEEK_SET);
    fread(ines_data, ines_size, 1, ines);
    fclose(ines);

    // Initialize SDL and the time seed.
    SDL_Init(SDL_INIT_EVERYTHING);
    srand((unsigned)time(NULL));

    // Create the SDL window, renderer and the render texture.
    display.window = SDL_CreateWindow("nesemu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        NES_W, NES_H, SDL_WINDOW_RESIZABLE);
    if (display.window == NULL)
        sdl_error();
    display.renderer = SDL_CreateRenderer(display.window, -1, SDL_RENDERER_ACCELERATED);
    if (display.renderer == NULL)
        sdl_error();
    display.buffer = SDL_CreateTexture(display.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
        NES_W, NES_H);
    if (display.buffer == NULL)
        sdl_error();
    SDL_SetWindowMinimumSize(display.window, NES_W, NES_H);

    // Set the display width/height to the default NES emulator's resolution.
    display.w = NES_W;
    display.h = NES_H;
    
    // Set up the NES computer.
    display.computer = nes_alloc();
    if ((display.cartridge = cartridge_alloc(ines_data, ines_size)) == NULL)
    {
        fprintf(stderr, "NES ROM FILE IS CORRUPT");
        exit(EXIT_FAILURE);
    }
    nes_setcartridge(display.computer, display.cartridge);
    nes_reset(display.computer);

    // Start the main event loop.
    SDL_AddEventWatch(watcher, NULL);
    atexit(process_exit);
    uint64_t timestamp = 0;
    float cached_framerate = 0.f;
    for (;;)
    {
        // Busywait until the time length of a PPU frame has been complete.
        static float ns_per_ppu_cycle = (float)NANOSECOND / ((float)MASTER_CLOCK / 4);
        uint64_t time_wait = display.computer->ppu->frame_cycles_enumerated * ns_per_ppu_cycle;
        uint64_t new_timestamp;
        while (display.computer->ppu->frame_complete 
            && (new_timestamp = get_ns_timestamp()) - timestamp < time_wait)
            continue;

        // Calculate the framerate and change the window title.
        if (new_timestamp > timestamp)
        {
            cached_framerate = lerp(cached_framerate, 
                1 / ((float)(new_timestamp - timestamp) / NANOSECOND), 
                min(max(1 / cached_framerate * 2, 0), 1));
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "nesemu: %dfps", (int)cached_framerate);
            SDL_SetWindowTitle(display.window, buffer);
        }

        // Reset the PPU.
        timestamp = new_timestamp;
        display.computer->ppu->frame_complete = false;
        display.computer->ppu->frame_cycles_enumerated = 0;

        // Poll each event.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            // Handle process exit.
            case SDL_QUIT:
                goto quit;

            // Set a controller input.
            case SDL_KEYDOWN:
                switch ((int)event.key.keysym.scancode)
                {
                // A key.
                case SDL_SCANCODE_X:
                    display.computer->controllers[0].vars.a = 1;
                    break;

                // B key.
                case SDL_SCANCODE_Z:
                    display.computer->controllers[0].vars.b = 1;
                    break;

                // Select key.
                case SDL_SCANCODE_A:
                    display.computer->controllers[0].vars.select = 1;
                    break;

                // Start key.
                case SDL_SCANCODE_S:
                    display.computer->controllers[0].vars.start = 1;
                    break;
                
                // Up arrow key.
                case SDL_SCANCODE_UP:
                    display.computer->controllers[0].vars.up = 1;
                    break;

                // Down arrow key.
                case SDL_SCANCODE_DOWN:
                    display.computer->controllers[0].vars.down = 1;
                    break;

                // Left arrow key.
                case SDL_SCANCODE_LEFT:
                    display.computer->controllers[0].vars.left = 1;
                    break;

                // Right arrow key.
                case SDL_SCANCODE_RIGHT:
                    display.computer->controllers[0].vars.right = 1;
                    break;
                }
                break;

            // Release a controller input.
            case SDL_KEYUP:
                switch ((int)event.key.keysym.scancode)
                {
                // A key.
                case SDL_SCANCODE_X:
                    display.computer->controllers[0].vars.a = 0;
                    break;

                // B key.
                case SDL_SCANCODE_Z:
                    display.computer->controllers[0].vars.b = 0;
                    break;

                // Select key.
                case SDL_SCANCODE_A:
                    display.computer->controllers[0].vars.select = 0;
                    break;

                // Start key.
                case SDL_SCANCODE_S:
                    display.computer->controllers[0].vars.start = 0;
                    break;
                
                // Up arrow key.
                case SDL_SCANCODE_UP:
                    display.computer->controllers[0].vars.up = 0;
                    break;

                // Down arrow key.
                case SDL_SCANCODE_DOWN:
                    display.computer->controllers[0].vars.down = 0;
                    break;

                // Left arrow key.
                case SDL_SCANCODE_LEFT:
                    display.computer->controllers[0].vars.left = 0;
                    break;

                // Right arrow key.
                case SDL_SCANCODE_RIGHT:
                    display.computer->controllers[0].vars.right = 0;
                    break;
                }
                break;
            }
        }

        // Clock the NES enough times to render a whole frame.
        while (!display.computer->ppu->frame_complete)
            nes_clock(display.computer);
        
        // Update the buffer and re-render it.
        SDL_UpdateTexture(display.buffer, NULL, &display.computer->ppu->screen, 
            NES_W * sizeof(struct agbr8888));
        update_render();
    }

    // Exit.
no_cartridge:
    puts("usage: nesemu game.nes");
quit:
    return EXIT_SUCCESS;
}