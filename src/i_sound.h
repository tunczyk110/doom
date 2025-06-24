
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

#pragma once

#include "doomdef.h"

#include "doomstat.h"
#include "sounds.h"

#define NUM_CHANNELS 32


// Init at program start...
void I_InitSound();

// ... shut down and relase at program termination.
void I_ShutdownSound(void);


//
//  SFX I/O
//

boolean update_sound_params(int channel, int vol, int sep);

// Get raw data lump index for sound descriptor.
int I_GetSfxLumpNum (sfxinfo_t* sfxinfo );

// Starts a sound, returns a channel on which it's played
int I_StartSound(int id, int vol, int sep);

// Stops a sound channel.
void I_StopSound(int channel);


// Updates the volume, separation,
//  and pitch of a sound channel.
void
I_UpdateSoundParams
( int		handle,
  int		vol,
  int		sep,
  int		pitch );


//
//  MUSIC I/O
//
void I_InitMusic(void);
void I_ShutdownMusic(void);
// Volume.
void I_SetMusicVolume(int volume);
// PAUSE game handling.
void I_PauseSong(int handle);
void I_ResumeSong(int handle);
// Registers a song handle to song data.
int I_RegisterSong(void *data);
// Called by anything that wishes to start music.
//  plays a song, and when the song is done,
//  starts playing it again in an endless loop.
// Horrible thing to do, considering.
void
I_PlaySong
( int		handle,
  int		looping );
// Stops a song over 3 seconds.
void I_StopSong(int handle);
// See above (register), then think backwards
void I_UnRegisterSong(int handle);
