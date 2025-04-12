/*
; Top-level translation unit; used for invoking NES computer and outputting the
; display/audio behind the scenes via SDL.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h> // (TEMP)

#include "SDL.h"

#include "constants.h"

// Create display information for this current session.
struct display_data
{
    SDL_Window* window;     // The window itself.    
    SDL_Renderer* renderer; // The renderer used for the window.
    SDL_Texture* buffer;    // The buffer that is being manipulated by the emulator.
    int w;                  // The current display width.
    int h;                  // The current display height.
};
static struct display_data display;

// Unload SDL on process exit.
static void process_exit()
{
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
    
    // Create a grid of random pretty colours.
    const int square_size = 1;
    SDL_Color grid[NES_H * NES_W] = {0};
    for (int y = 0; y < NES_H; y += square_size)
    {
        for (int x = 0; x < NES_W; x += square_size)
        {
            int g = rand() % 256; // Don't use this in real code due to modulo bias!
            int b = rand() % 256;
            for (int y1 = 0; y1 < square_size; ++y1)
            {
                for (int x1 = 0; x1 < square_size; ++x1)
                {
                    int coord = y * NES_W + y1 * NES_W + x + x1;
                    grid[coord].a = 0;
                    grid[coord].r = 0;
                    grid[coord].g = g;
                    grid[coord].b = b;
                }
            }
        }
    }

    // (TEMP) get the performance frequency and the start timestamp.
    int frames = 0;
    LARGE_INTEGER start, end, elapsed, frequency;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    // Start the main event loop.
    SDL_AddEventWatch(watcher, NULL);
    atexit(process_exit);
    for (;;)
    {
        // Poll each event and handle process exit.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                goto quit;
            }
        }

        // Update the buffer and re-render it.
        SDL_UpdateTexture(display.buffer, NULL, grid, NES_W * sizeof(SDL_Color));
        update_render();

        // (TEMP) if at least one second has elapsed, print the framerate.
        QueryPerformanceCounter(&end);
        frames++;
        elapsed.QuadPart = end.QuadPart - start.QuadPart;
        elapsed.QuadPart = (elapsed.QuadPart) / frequency.QuadPart;
        if (elapsed.QuadPart >= 1)
        {
            printf("%dfps\n", frames);
            frames = 0;
            start = end;
        }
    }

    // Exit.
quit:
    return EXIT_SUCCESS;
}