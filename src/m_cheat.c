
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

#include "m_cheat.h"

#include "doomstat.h"

#include "s_sound.h"
#include "p_inter.h"
#include "g_game.h"
#include "d_englsh.h"
#include "sounds.h"


#define ST_MSGWIDTH			52

extern player_t* player;

// Massive bunches of cheat shit
//  to keep it from being easy to figure them out.
// Yeah, right...
unsigned char	cheat_mus_seq[] =
{
    0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff
};

unsigned char	cheat_choppers_seq[] =
{
    0xb2, 0x26, 0xe2, 0x32, 0xf6, 0x2a, 0x2a, 0xa6, 0x6a, 0xea, 0xff // id...
};

unsigned char	cheat_god_seq[] =
{
    0xb2, 0x26, 0x26, 0xaa, 0x26, 0xff  // iddqd
};

unsigned char	cheat_ammo_seq[] =
{
    0xb2, 0x26, 0xf2, 0x66, 0xa2, 0xff	// idkfa
};

unsigned char	cheat_ammonokey_seq[] =
{
    0xb2, 0x26, 0x66, 0xa2, 0xff	// idfa
};


// Smashing Pumpkins Into Samml Piles Of Putried Debris. 
unsigned char	cheat_noclip_seq[] =
{
    0xb2, 0x26, 0xea, 0x2a, 0xb2,	// idspispopd
    0xea, 0x2a, 0xf6, 0x2a, 0x26, 0xff
};

//
unsigned char	cheat_commercial_noclip_seq[] =
{
    0xb2, 0x26, 0xe2, 0x36, 0xb2, 0x2a, 0xff	// idclip
}; 



unsigned char	cheat_powerup_seq[7][10] =
{
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6e, 0xff }, 	// beholdv
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xea, 0xff }, 	// beholds
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xb2, 0xff }, 	// beholdi
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6a, 0xff }, 	// beholdr
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xa2, 0xff }, 	// beholda
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x36, 0xff }, 	// beholdl
    { 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xff }		// behold
};


unsigned char	cheat_clev_seq[] =
{
    0xb2, 0x26,  0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff	// idclev
};


// my position cheat
unsigned char	cheat_mypos_seq[] =
{
    0xb2, 0x26, 0xb6, 0xba, 0x2a, 0xf6, 0xea, 0xff	// idmypos
}; 


// Now what?
cheatseq_t	cheat_mus = { cheat_mus_seq, 0 };
cheatseq_t	cheat_god = { cheat_god_seq, 0 };
cheatseq_t	cheat_ammo = { cheat_ammo_seq, 0 };
cheatseq_t	cheat_ammonokey = { cheat_ammonokey_seq, 0 };
cheatseq_t	cheat_noclip = { cheat_noclip_seq, 0 };
cheatseq_t	cheat_commercial_noclip = { cheat_commercial_noclip_seq, 0 };

cheatseq_t	cheat_powerup[7] =
{
    { cheat_powerup_seq[0], 0 },
    { cheat_powerup_seq[1], 0 },
    { cheat_powerup_seq[2], 0 },
    { cheat_powerup_seq[3], 0 },
    { cheat_powerup_seq[4], 0 },
    { cheat_powerup_seq[5], 0 },
    { cheat_powerup_seq[6], 0 }
};

cheatseq_t	cheat_choppers = { cheat_choppers_seq, 0 };
cheatseq_t	cheat_clev = { cheat_clev_seq, 0 };
cheatseq_t	cheat_mypos = { cheat_mypos_seq, 0 };

//
// CHEAT SEQUENCE PACKAGE
//

static int		firsttime = 1;
static unsigned char	cheat_xlate_table[256];


//
// Called in st_stuff module, which handles the input.
// Returns a 1 if the cheat was successful, 0 if failed.
//
int cht_CheckCheat(cheatseq_t*	cht, SDL_Scancode key)
{
    char ch = SDL_GetKeyFromScancode(key, SDL_KMOD_NONE, false);

    int i;
    int rc = 0;

    if (firsttime)
    {
	firsttime = 0;
	for (i=0;i<256;i++) cheat_xlate_table[i] = SCRAMBLE(i);
    }

    if (!cht->p)
	cht->p = cht->sequence; // initialize if first time

    if (*cht->p == 0)
	*(cht->p++) = ch;
    else if
	(cheat_xlate_table[(unsigned char)ch] == *cht->p) cht->p++;
    else
	cht->p = cht->sequence;

    if (*cht->p == 1)
	cht->p++;
    else if (*cht->p == 0xff) // end of sequence character
    {
	cht->p = cht->sequence;
	rc = 1;
    }

    return rc;
}

void
cht_GetParam
( cheatseq_t*	cht,
  char*		buffer )
{

    unsigned char *p, c;

    p = cht->sequence;
    while (*(p++) != 1);
    
    do
    {
	c = *p;
	*(buffer++) = c;
	*(p++) = 0;
    }
    while (c && *p!=0xff );

    if (*p==0xff)
	*buffer = 0;

}


// Called by main loop.
boolean check_cheat_input (SDL_Event* ev)
{
    int i;
    if (ev->type == SDL_EVENT_KEY_DOWN) {
        if (!netgame && gameskill != sk_nightmare) {
            // 'dqd' cheat for toggleable god mode
            if (cht_CheckCheat(&cheat_god, ev->key.scancode)) {
                player->cheats ^= CF_GODMODE;
                if (player->cheats & CF_GODMODE) {
                    if (player->mo)
                        player->mo->health = 100;

                    player->health = 100;
                    player->message = STSTR_DQDON;
                }
                else
                player->message = STSTR_DQDOFF;
            }
            // 'fa' cheat for killer fucking arsenal
            else if (cht_CheckCheat(&cheat_ammonokey, ev->key.scancode))
            {
            player->armorpoints = 200;
            player->armortype = 2;

            for (i=0;i<NUMWEAPONS;i++)
            player->weaponowned[i] = true;

            for (i=0;i<NUMAMMO;i++)
            player->ammo[i] = player->maxammo[i];

            player->message = STSTR_FAADDED;
            }
            // 'kfa' cheat for key full ammo
            else if (cht_CheckCheat(&cheat_ammo, ev->key.scancode))
            {
            player->armorpoints = 200;
            player->armortype = 2;

            for (i=0;i<NUMWEAPONS;i++)
            player->weaponowned[i] = true;

            for (i=0;i<NUMAMMO;i++)
            player->ammo[i] = player->maxammo[i];

            for (i=0;i<NUMCARDS;i++)
            player->cards[i] = true;

            player->message = STSTR_KFAADDED;
            }
            // 'mus' cheat for changing music
            else if (cht_CheckCheat(&cheat_mus, ev->key.scancode))
            {
            
            char	buf[3];
            int		musnum;
            
            player->message = STSTR_MUS;
            cht_GetParam(&cheat_mus, buf);
            
            if (gamemode == commercial)
            {
            musnum = mus_runnin + (buf[0]-'0')*10 + buf[1]-'0' - 1;
            
            if (((buf[0]-'0')*10 + buf[1]-'0') > 35)
                player->message = STSTR_NOMUS;
            else
                S_ChangeMusic(musnum, 1);
            }
            else
            {
            musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');
            
            if (((buf[0]-'1')*9 + buf[1]-'1') > 31)
                player->message = STSTR_NOMUS;
            else
                S_ChangeMusic(musnum, 1);
            }
            }
            // Simplified, accepting both "noclip" and "idspispopd".
            // no clipping mode cheat
            else if ( cht_CheckCheat(&cheat_noclip, ev->key.scancode) 
                || cht_CheckCheat(&cheat_commercial_noclip,ev->key.scancode) )
            {	
            player->cheats ^= CF_NOCLIP;
            
            if (player->cheats & CF_NOCLIP)
            player->message = STSTR_NCON;
            else
            player->message = STSTR_NCOFF;
            }
            // 'behold?' power-up cheats
            for (i=0;i<6;i++)
            {
            if (cht_CheckCheat(&cheat_powerup[i], ev->key.scancode))
            {
            if (!player->powers[i])
                P_GivePower( player, i);
            else if (i!=pw_strength)
                player->powers[i] = 1;
            else
                player->powers[i] = 0;
            
            player->message = STSTR_BEHOLDX;
            }
            }
            
            // 'behold' power-up menu
            if (cht_CheckCheat(&cheat_powerup[6], ev->key.scancode))
            {
            player->message = STSTR_BEHOLD;
            }
            // 'choppers' invulnerability & chainsaw
            else if (cht_CheckCheat(&cheat_choppers, ev->key.scancode))
            {
            player->weaponowned[wp_chainsaw] = true;
            player->powers[pw_invulnerability] = true;
            player->message = STSTR_CHOPPERS;
            }
            // 'mypos' for player position
            else if (cht_CheckCheat(&cheat_mypos, ev->key.scancode))
            {
            static char	buf[ST_MSGWIDTH];
            sprintf(buf, "ang=0x%x;x,y=(0x%x,0x%x)",
                players[consoleplayer].mo->angle,
                players[consoleplayer].mo->x,
                players[consoleplayer].mo->y);
            player->message = buf;
            }
        }
        
        // 'clev' change-level cheat
        if (cht_CheckCheat(&cheat_clev, ev->key.scancode))
        {
        char		buf[3];
        int		epsd;
        int		map;
        
        cht_GetParam(&cheat_clev, buf);
        
        if (gamemode == commercial)
        {
        epsd = 1;
        map = (buf[0] - '0')*10 + buf[1] - '0';
        }
        else
        {
        epsd = buf[0] - '0';
        map = buf[1] - '0';
        }

        // Catch invalid maps.
        if (epsd < 1)
        return false;

        if (map < 1)
        return false;
        
        // Ohmygod - this is not going to work.
        if ((gamemode == retail)
        && ((epsd > 4) || (map > 9)))
        return false;

        if ((gamemode == registered)
        && ((epsd > 3) || (map > 9)))
        return false;

        if ((gamemode == shareware)
        && ((epsd > 1) || (map > 9)))
        return false;

        if ((gamemode == commercial)
        && (( epsd > 1) || (map > 34)))
        return false;

        // So be it.
        player->message = STSTR_CLEV;
        G_DeferedInitNew(gameskill, epsd, map);
        }    
  }
  return false;
}
