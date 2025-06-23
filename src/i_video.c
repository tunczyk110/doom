
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2025 by Michał Tomczyk
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include <signal.h>

#include "i_system.h"
#include "v_video.h"
#include "d_main.h"
#include "w_wad.h"
#include "z_zone.h"
#include "d_event.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_pixels.h>

// SDL display number
int video_display = 0;

SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;

SDL_Surface* screen_buffer = NULL;
SDL_Palette* screen_palette = NULL;

SDL_Texture* render_texture = NULL;

pixel_t* screen_pixels = NULL;

static SDL_Rect blit_rect = {
    0,
    0,
    SCREENWIDTH,
    SCREENHEIGHT
};

void I_ShutdownGraphics(void)
{
    SDL_DestroySurface(screen_buffer);
    SDL_DestroyTexture(render_texture);
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

void I_StartTic (void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            I_Quit();
        }
        if (event.type != SDL_EVENT_MOUSE_MOTION) {
            D_PostEvent(&event);
        }
    }
    float x, y;
    SDL_GetRelativeMouseState(&x, &y);
    if (x > 0.0005f || x < -0.0005f || y > 0.0005f || y < -0.0005f) {
        SDL_Event mouse_motion = {
            .motion = {
                .type = SDL_EVENT_MOUSE_MOTION,
                .xrel = x,
                .yrel = y
            }
        };
        D_PostEvent(&mouse_motion);
    }
}

void I_FinishUpdate (void)
{
    SDL_Surface* lock_surface = NULL;
    if (!SDL_LockTextureToSurface(render_texture, NULL, &lock_surface)) {
        I_Error("Failed to lock texture for rendering: %s", SDL_GetError());
    }
    SDL_BlitSurfaceUnchecked(screen_buffer, &blit_rect, lock_surface, &blit_rect);
    SDL_UnlockTexture(render_texture);

    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderTexture(renderer, render_texture, NULL, NULL);

    SDL_RenderPresent(renderer);
}

void I_ReadScreen (byte* scr)
{
    memcpy (scr, screen_pixels, SCREENWIDTH*SCREENHEIGHT);
}

void I_SetPalette (byte* doompalette)
{
    if (!screen_palette) {
        I_Error("Error when trying to set palette; the ptr is NULL");
    }

    SDL_Color colors[256];

    for (int i=0; i<256; ++i)
    {
        colors[i].a = 0xFFu;
        colors[i].r = gammatable[usegamma][*doompalette++];
        colors[i].g = gammatable[usegamma][*doompalette++];
        colors[i].b = gammatable[usegamma][*doompalette++];
    }

    SDL_SetPaletteColors(screen_palette, colors, 0, 256);
}

void I_InitGraphics(void)
{
    signal(SIGINT, (void (*)(int)) I_Quit);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        I_Error("Failed to init SDL video: %s", SDL_GetError());
    }

    if (!SDL_CreateWindowAndRenderer("Doom", SCREENWIDTH, SCREENHEIGHT, 0, &window, &renderer)) {
        I_Error("Failed to create window: %s", SDL_GetError());
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    screen_buffer = SDL_CreateSurface(SCREENWIDTH, SCREENHEIGHT, SDL_PIXELFORMAT_INDEX8);
    if (!screen_buffer) {
        I_Error("Failed to create screen buffer surface: %s", SDL_GetError());
    }
    screen_palette = SDL_CreateSurfacePalette(screen_buffer);
    if (!screen_palette) {
        I_Error("Failed to create palette for screen buffer: %s", SDL_GetError());
    }

    render_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREENWIDTH, SCREENHEIGHT);
    if (!render_texture) {
        I_Error("Failed to create rendering texture: %s", SDL_GetError());
    }

    screen_pixels = screen_buffer->pixels;

    byte* doompal = W_CacheLumpName("PLAYPAL", PU_CACHE);
    I_SetPalette(doompal);

    SDL_SetWindowMouseGrab(window, true);
    SDL_HideCursor();
}
