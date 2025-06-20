// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"
#include "w_wad.h"
#include "z_zone.h"

#include "doomdef.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_pixels.h>

// SDL display number
int video_display = 0;

SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;

SDL_Surface* screen_buffer = NULL;
SDL_Palette* screen_palette = NULL;

SDL_Surface* argb_buffer = NULL;
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
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

void I_StartTic (void)
{
    if (!renderer) {
        return;
    }

    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event)) {
        event_t event = TranslateEvent(&sdl_event);
        D_PostEvent(&event);
    }
}

void I_FinishUpdate (void)
{
    if (!SDL_LockTextureToSurface(render_texture, NULL, &argb_buffer)) {
        I_Error("Failed to lock texture for rendering: %s", SDL_GetError());
    }
    SDL_BlitSurfaceUnchecked(screen_buffer, &blit_rect, argb_buffer, &blit_rect);
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
    int i;

    if (!screen_palette) {
        return;
    }

    SDL_Color* palette = screen_palette->colors;

    for (i=0; i<256; ++i)
    {
        palette[i].a = 0xFFu;
        palette[i].r = gammatable[usegamma][*doompalette++];
        palette[i].g = gammatable[usegamma][*doompalette++];
        palette[i].b = gammatable[usegamma][*doompalette++];
    }
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
}
