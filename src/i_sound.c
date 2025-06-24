
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"

#include <SDL3_mixer/SDL_mixer.h>

void channel_finished_callback(int channel);

int allocated_channels = -1;

SDL_AudioSpec mixer_spec = {
    .freq = MIX_DEFAULT_FREQUENCY,
    .format = MIX_DEFAULT_FORMAT,
    .channels = MIX_DEFAULT_CHANNELS
};

// MUSIC API - dummy. Some code from DOS version.
void I_SetMusicVolume(int volume)
{
  // Internal state variable.
  snd_MusicVolume = volume;
  // Now set volume on output device.
  // Whatever( snd_MusciVolume );
}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

boolean update_sound_params(int channel, int vol, int sep)
{
    vol *= MIX_MAX_VOLUME / 15;

    int left = ((254 - sep) * vol) / 127;
    int right = (sep * vol) / 127;

    if (left < 0) left = 0;
    else if (left > 255) left = 255;

    if (right < 0) right = 0;
    else if (right > 255) right = 255;

    return Mix_SetPanning(channel, left, right);
}

int I_StartSound(int id, int vol, int sep)
{
    int channel = Mix_PlayChannel(-1, S_sfx[id].data, 0);
    if (channel < 0) {
        printf("******************* no free channel for sound %s!\n", S_sfx[id].name);
        return channel;
    }

    vol *= MIX_MAX_VOLUME / 15;

    int left = ((254 - sep) * vol) / 127;
    int right = (sep * vol) / 127;

    if (left < 0) left = 0;
    else if (left > 255) left = 255;

    if (right < 0) right = 0;
    else if (right > 255) right = 255;

    if (!update_sound_params(channel, left, right)) {
        printf("Failed to set parameters for sound %s: %s\n", S_sfx[id].name, SDL_GetError());
    }
    return channel;
}

void I_StopSound (int channel)
{
    Mix_HaltChannel(channel);
}

void I_ShutdownSound(void)
{
    Mix_CloseAudio();
    Mix_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void resample_pcm(sfxinfo_t* sfx_info, byte* data, int sample_rate, unsigned num_samples)
{
    SDL_AudioSpec dmx_spec = {
        .freq = sample_rate,
        .format = SDL_AUDIO_U8,
        .channels = 1
    };

    Mix_Chunk chunk = {
        .allocated = 1,
        .volume = MIX_MAX_VOLUME
    };

    if (!SDL_ConvertAudioSamples(&dmx_spec, data, num_samples, &mixer_spec, &chunk.abuf, &chunk.alen)) {
        I_Error("Failed to convert %s from DMX format to SDL mixer: %s", sfx_info->name, SDL_GetError());
    }

    // todo: do you need to sdl_free buffer written to by convertaudiosamples?

    sfx_info->data = Z_Malloc(sizeof(Mix_Chunk), PU_STATIC, 0);
    memcpy(sfx_info->data, &chunk, sizeof(Mix_Chunk));
}

boolean cache_sound(sfxinfo_t* sfx_info)
{
    byte* dmx_data = W_CacheLumpNum(sfx_info->lumpnum, PU_STATIC);
    unsigned lump_len = W_LumpLength(sfx_info->lumpnum);

    if (lump_len < 8 || dmx_data[0] != 0x03 || dmx_data[1] != 0x00)
    {
        printf("Invalid sound %s", sfx_info->name);
        return false;
    }

    int sample_rate = (dmx_data[3] << 8) | dmx_data[2];
    unsigned num_samples = (dmx_data[7] << 24) | (dmx_data[6] << 16) | (dmx_data[5] << 8) | dmx_data[4];

    resample_pcm(sfx_info, dmx_data + 0x18, sample_rate, num_samples-16);
    return true;
}

void preallocate_sounds(void)
{
    for (int i = 1; i < NUMSFX; ++i) {
        sfxinfo_t* sfx_info = &S_sfx[i];
        if (sfx_info->link != NULL) {
            sfx_info = sfx_info->link;
        }
        sfx_info->lumpnum = I_GetSfxLumpNum(sfx_info);
        cache_sound(sfx_info);
    }
}

void I_InitSound(void) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        I_Error("Failed to init SDL audio: %s", SDL_GetError());
    }

    if (Mix_Init(MIX_INIT_WAVPACK) != MIX_INIT_WAVPACK) {
        I_Error("Failed to init SDL mixer");
    }

    if (!Mix_OpenAudio(0, &mixer_spec)) {
        I_Error("Failed to open audio: %s", SDL_GetError());
    }

    Mix_QuerySpec(&mixer_spec.freq, &mixer_spec.format, &mixer_spec.channels);
    printf("I_InitSound: Opened audio at %d Hz %d bit%s %s\n",
        mixer_spec.freq,
        (mixer_spec.format&0xFF),
        (SDL_AUDIO_ISFLOAT(mixer_spec.format) ? " (float)" : ""),
        (mixer_spec.channels > 2) ? "surround" :
        (mixer_spec.channels > 1) ? "stereo" : "mono");

    allocated_channels = Mix_AllocateChannels(NUM_CHANNELS);
    if (allocated_channels < 8) {
        I_Error("SDL allocated too few channels: %d", allocated_channels);
    }
    printf("SDL allocated channels: %d\n", allocated_channels);

    Mix_ChannelFinished(channel_finished_callback);

    preallocate_sounds();
}

//
// MUSIC API.
// Still no music done.
// Remains. Dummies.
//
void I_InitMusic(void)		{ }
void I_ShutdownMusic(void)	{ }

static int	looping=0;
static int	musicdies=-1;

void I_PlaySong(int handle, int looping)
{
  // UNUSED.
  handle = looping = 0;
  musicdies = gametic + TICRATE*30;
}

void I_PauseSong (int handle)
{
  // UNUSED.
  handle = 0;
}

void I_ResumeSong (int handle)
{
  // UNUSED.
  handle = 0;
}

void I_StopSong(int handle)
{
  // UNUSED.
  handle = 0;
  
  looping = 0;
  musicdies = 0;
}

void I_UnRegisterSong(int handle)
{
  // UNUSED.
  handle = 0;
}

int I_RegisterSong(void* data)
{
  // UNUSED.
  data = NULL;
  
  return 1;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
  // UNUSED.
  handle = 0;
  return looping || musicdies > gametic;
}
