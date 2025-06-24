
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

#include "i_system.h"
#include "i_sound.h"
#include "sounds.h"
#include "s_sound.h"

#include "z_zone.h"
#include "m_random.h"
#include "w_wad.h"

#include "doomdef.h"
#include "p_local.h"

#include "doomstat.h"

extern int allocated_channels;

// when to clip out sounds
// Does not fit the large outdoor areas.
#define S_CLIPPING_DIST (1200*FRACUNIT)

// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).
#define S_CLOSE_DIST (200*FRACUNIT)


#define S_ATTENUATOR ((S_CLIPPING_DIST-S_CLOSE_DIST)>>FRACBITS)

#define NORM_SEP 128

#define S_STEREO_SWING (96*FRACUNIT)

typedef struct
{
    // sound information (if null, channel avail.)
    sfxinfo_t*	sfxinfo;

    // origin of sound
    void*	origin;
} channel_t;

// the set of channels available
static channel_t* channels;

// These are not used, but should be (menu).
// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
int snd_SfxVolume = 15;

// Maximum volume of music. Useless so far.
int snd_MusicVolume = 15;

// whether songs are mus_paused
static boolean mus_paused;

// music currently being played
static musicinfo_t*	mus_playing = NULL;

static int nextcleanup;


//
// Internals.
//

int
S_AdjustSoundParams
( mobj_t*	listener,
  mobj_t*	source,
  int*		vol,
  int*		sep,
  int*		pitch );

void S_StopChannel(int cnum);

void channel_finished_callback(int channel) {
    channels[channel].sfxinfo = NULL;
    channels[channel].origin = NULL;
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void S_Init(int sfxVolume, int musicVolume)
{
    int i;

    fprintf( stderr, "S_Init: default sfx volume %d\n", sfxVolume);

    S_SetSfxVolume(sfxVolume);
    S_SetMusicVolume(musicVolume);

    // Allocating the internal channels for mixing
    // (the maximum numer of sounds rendered
    // simultaneously) within zone memory.
    channels = (channel_t *) Z_Malloc(allocated_channels*sizeof(channel_t), PU_STATIC, 0);

    // Free all channels for use
    memset(channels, 0, allocated_channels*sizeof(channel_t));

    mus_paused = 0;
}

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void S_Start(void)
{
  int cnum;
  int mnum;

  // kill all playing sounds at start of level
  //  (trust me - a good idea)
    for (cnum=0; cnum < allocated_channels; ++cnum)
        if (channels[cnum].sfxinfo)
            S_StopChannel(cnum);
  
  // start new music for the level
  mus_paused = 0;
  
  if (gamemode == commercial)
    mnum = mus_runnin + gamemap - 1;
  else
  {
    int spmus[]=
    {
      // Song - Who? - Where?
      
      mus_e3m4,	// American	e4m1
      mus_e3m2,	// Romero	e4m2
      mus_e3m3,	// Shawn	e4m3
      mus_e1m5,	// American	e4m4
      mus_e2m7,	// Tim 	e4m5
      mus_e2m4,	// Romero	e4m6
      mus_e2m6,	// J.Anderson	e4m7 CHIRON.WAD
      mus_e2m5,	// Shawn	e4m8
      mus_e1m9	// Tim		e4m9
    };
    
    if (gameepisode < 4)
      mnum = mus_e1m1 + (gameepisode-1)*9 + gamemap-1;
    else
      mnum = spmus[gamemap-1];
    }	
  
  // HACK FOR COMMERCIAL
  //  if (commercial && mnum > mus_e3m9)	
  //      mnum -= mus_e3m9;
  
  S_ChangeMusic(mnum, true);
  
  nextcleanup = 15;
}	

// returns false if sound is inaudible
boolean calculate_sound_params(sfxinfo_t* sfx, mobj_t* origin, int* volume, int* sep)
{
    // Initialize sound parameters
    if (sfx->link) {
        *volume += sfx->volume;

        if (*volume < 1)
        return false;

        if (*volume > snd_SfxVolume)
        *volume = snd_SfxVolume;
    }

    // Check to see if it is audible,
    //  and if not, modify the params
    if (origin && origin != players[consoleplayer].mo) {
        int rc = S_AdjustSoundParams(players[consoleplayer].mo,
                    origin,
                    volume,
                    sep,
                    0);

        if ( origin->x == players[consoleplayer].mo->x
            && origin->y == players[consoleplayer].mo->y)
        {
            *sep = NORM_SEP;
        }

        if (!rc)
            return false;
    } else {
        *sep = NORM_SEP;
    }
    return true;
}

void S_StartSound(void* origin_p, int sfx_id)
{
    int rc;
    int sep;
    sfxinfo_t* sfx;

    mobj_t* origin = (mobj_t *) origin_p;
    int volume = snd_SfxVolume;

    // check for bogus sound #
    if (sfx_id < 1 || sfx_id > NUMSFX)
        I_Error("Bad sfx #: %d", sfx_id);

    sfx = &S_sfx[sfx_id];

    if (!calculate_sound_params(sfx, origin, &volume, &sep)) {
        return;
    }

    // kill old sound
    S_StopSound(origin);

    int channel = I_StartSound(sfx_id, volume, sep);
    if (channel < 0) return;
    channels[channel].sfxinfo = sfx;
    channels[channel].origin = origin;
}

void S_StopSound(void *origin)
{
    for (int i=0; i < allocated_channels; i++) {
        if (channels[i].sfxinfo && channels[i].origin == origin) {
            S_StopChannel(i);
            break;
        }
    }
}

void S_UpdateSounds(void* listener)
{
    int volume, sep;
    for (int i = 0; i < allocated_channels; ++i) {
        if (channels[i].sfxinfo && channels[i].origin && listener != channels[i].origin) {
            if (!calculate_sound_params(channels[i].sfxinfo, channels[i].origin, &volume, &sep)) {
                I_StopSound(i);
            } else {
                update_sound_params(i, volume, sep);
            }
        }
    }
}

//
// Stop and resume music, during game PAUSE.
//
void S_PauseMusic(void)
{
    if (mus_playing && !mus_paused)
    {
	I_PauseSong(mus_playing->handle);
	mus_paused = true;
    }
}

void S_ResumeMusic(void)
{
    if (mus_playing && mus_paused)
    {
	I_ResumeSong(mus_playing->handle);
	mus_paused = false;
    }
}

void S_SetMusicVolume(int volume)
{
    if (volume < 0 || volume > 127)
    {
	I_Error("Attempt to set music volume at %d",
		volume);
    }    

    I_SetMusicVolume(127);
    I_SetMusicVolume(volume);
    snd_MusicVolume = volume;
}



void S_SetSfxVolume(int volume)
{

    if (volume < 0 || volume > 127)
	I_Error("Attempt to set sfx volume at %d", volume);

    snd_SfxVolume = volume;

}

//
// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(int m_id)
{
    S_ChangeMusic(m_id, false);
}

void
S_ChangeMusic
( int			musicnum,
  int			looping )
{
    musicinfo_t*	music;
    char		namebuf[9];

    if ( (musicnum <= mus_None)
	 || (musicnum >= NUMMUSIC) )
    {
	I_Error("Bad music number %d", musicnum);
    }
    else
	music = &S_music[musicnum];

    if (mus_playing == music)
	return;

    // shutdown old music
    S_StopMusic();

    // get lumpnum if neccessary
    if (!music->lumpnum)
    {
	sprintf(namebuf, "d_%s", music->name);
	music->lumpnum = W_GetNumForName(namebuf);
    }

    // load & register it
    music->data = (void *) W_CacheLumpNum(music->lumpnum, PU_MUSIC);
    music->handle = I_RegisterSong(music->data);

    // play it
    I_PlaySong(music->handle, looping);

    mus_playing = music;
}


void S_StopMusic(void)
{
    if (mus_playing)
    {
	if (mus_paused)
	    I_ResumeSong(mus_playing->handle);

	I_StopSong(mus_playing->handle);
	I_UnRegisterSong(mus_playing->handle);
	Z_ChangeTag(mus_playing->data, PU_CACHE);
	
	mus_playing->data = 0;
	mus_playing = 0;
    }
}

void S_StopChannel(int cnum)
{
    I_StopSound(cnum);
    // does SDL_mixer call that for you when you halt a channel?
    channel_finished_callback(cnum);
}

//
// Changes volume, stereo-separation, and pitch variables
//  from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//
int
S_AdjustSoundParams
( mobj_t*	listener,
  mobj_t*	source,
  int*		vol,
  int*		sep,
  int*		pitch )
{
    fixed_t	approx_dist;
    fixed_t	adx;
    fixed_t	ady;
    angle_t	angle;

    // calculate the distance to sound origin
    //  and clip it if necessary
    adx = abs(listener->x - source->x);
    ady = abs(listener->y - source->y);

    // From _GG1_ p.428. Appox. eucledian distance fast.
    approx_dist = adx + ady - ((adx < ady ? adx : ady)>>1);
    
    if (gamemap != 8
	&& approx_dist > S_CLIPPING_DIST)
    {
	return 0;
    }
    
    // angle of source to listener
    angle = R_PointToAngle2(listener->x,
			    listener->y,
			    source->x,
			    source->y);

    if (angle > listener->angle)
	angle = angle - listener->angle;
    else
	angle = angle + (0xffffffff - listener->angle);

    angle >>= ANGLETOFINESHIFT;

    // stereo separation
    *sep = 128 - (FixedMul(S_STEREO_SWING,finesine[angle])>>FRACBITS);

    // volume calculation
    if (approx_dist < S_CLOSE_DIST)
    {
	*vol = snd_SfxVolume;
    }
    else if (gamemap == 8)
    {
	if (approx_dist > S_CLIPPING_DIST)
	    approx_dist = S_CLIPPING_DIST;

	*vol = 15+ ((snd_SfxVolume-15)
		    *((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
	    / S_ATTENUATOR;
    }
    else
    {
	// distance effect
	*vol = (snd_SfxVolume
		* ((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
	    / S_ATTENUATOR; 
    }
    
    return (*vol > 0);
}
