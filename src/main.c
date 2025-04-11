/*
; Top-level translation unit; used for invoking NES computer and outputting the
; display/audio behind the scenes via SDL.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "SDL.h"

#include "constants.h"

int main(int argc, char** argv)
{
    SDL_Init(SDL_INIT_EVERYTHING);
    srand((unsigned)time(NULL));

    SDL_Window* window = SDL_CreateWindow("nesemu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          640, 480, 0);
    assert(window);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    assert(renderer);

    SDL_Texture* buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                                            NES_W, NES_H);
    assert(buffer);
    
    SDL_Color grid[NES_H * NES_W];
    for (int i = 0; i < sizeof(grid) / sizeof(grid[0]); ++i)
    {
        grid[i].a = 0;
        grid[i].r = 255;
        grid[i].g = 0;//#rand() % 256; // Obviously don't do this in real code, due to modulo bias!
        grid[i].b = 0;//rand() % 256;
    }

    for (;;)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                goto exit;
            }
        }

        SDL_RenderClear(renderer);
        SDL_UpdateTexture(buffer, NULL, grid, NES_W * sizeof(SDL_Color));
        SDL_RenderCopy(renderer, buffer, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

exit:
    SDL_DestroyTexture(buffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return 0;
}