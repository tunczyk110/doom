
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

#include "st_stuff.h"
#include "st_lib.h"

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "r_main.h"
#include "m_random.h"
#include "m_cheat.h"

#define STARTREDPALS 1
#define STARTBONUSPALS 9
#define NUMREDPALS 8
#define NUMBONUSPALS 4
#define RADIATIONPAL 13

#define ST_NUMPAINFACES 5
#define ST_NUMSTRAIGHTFACES 3
#define ST_NUMTURNFACES 2
#define ST_NUMSPECIALFACES 3

#define ST_FACESTRIDE (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES 2

#define ST_NUMFACES (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE (ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE (ST_GODFACE+1)

#define ST_FACESX 143
#define ST_FACESY 168

#define ST_EVILGRINCOUNT (2*TICRATE)
#define ST_STRAIGHTFACECOUNT (TICRATE/2)
#define ST_TURNCOUNT (1*TICRATE)
#define ST_OUCHCOUNT (1*TICRATE)
#define ST_RAMPAGEDELAY (2*TICRATE)

#define ST_MUCHPAIN 20

extern patch_t** face_patches;

player_t* player;

static int st_palette = 0;
static int lu_palette;
int st_faceindex = 0;
static int st_facecount = 0;
static int st_randomnumber;

// used to use appopriately pained face
static int st_oldhealth = -1;

// used for evil grin
static boolean oldweaponsowned[NUMWEAPONS];

#define STBAR_WIDTH SCREENWIDTH

statusbar_t* active_bar;

int statusbars_len = 0;
statusbar_t** status_bars;

numberfont_t** number_fonts;
int numberfonts_len = 0;

boolean ST_Responder (SDL_Event* ev)
{
    return check_cheat_input(ev);
}
int ST_calcPainOffset(void)
{
    int health;
    static int lastcalc;
    static int oldhealth = -1;

    health = player->health > 100 ? 100 : player->health;

    if (health != oldhealth) {
        lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
        oldhealth = health;
    }
    return lastcalc;
}

void ST_updateFaceWidget(void)
{
    int i;
    angle_t badguyangle;
    angle_t diffang;
    static int lastattackdown = -1;
    static int priority = 0;
    boolean doevilgrin;

    if (priority < 10 && !player->health) {
        // dead
        priority = 9;
        st_faceindex = ST_DEADFACE;
        st_facecount = 1;
    }

    if (priority < 9) {
        if (player->bonuscount) {
            doevilgrin = false;
            for (i=0;i<NUMWEAPONS;i++) {
                if (oldweaponsowned[i] != player->weaponowned[i]) {
                    doevilgrin = true;
                    oldweaponsowned[i] = player->weaponowned[i];
                }
            }
            if (doevilgrin) {
                // evil grin if just picked up weapon
                priority = 8;
                st_facecount = ST_EVILGRINCOUNT;
                st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
            }
        }

    }

    if (priority < 8 && player->damagecount &&
        player->attacker && player->attacker != player->mo)
    {
        // being attacked
        priority = 7;

        if (st_oldhealth - player->health > ST_MUCHPAIN) {
            st_facecount = ST_TURNCOUNT;
            st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
        } else {
            badguyangle = R_PointToAngle2(player->mo->x,
                player->mo->y,
                player->attacker->x,
                player->attacker->y);

            if (badguyangle > player->mo->angle) {
                // whether right or left
                diffang = badguyangle - player->mo->angle;
                i = diffang > ANG180;
            } else {
                // whether left or right
                diffang = player->mo->angle - badguyangle;
                i = diffang <= ANG180;
            } // confusing, aint it?


            st_facecount = ST_TURNCOUNT;
            st_faceindex = ST_calcPainOffset();

            if (diffang < ANG45) {
                // head-on
                st_faceindex += ST_RAMPAGEOFFSET;
            } else if (i) {
                // turn face right
                st_faceindex += ST_TURNOFFSET;
            } else {
                // turn face left
                st_faceindex += ST_TURNOFFSET+1;
            }
        }
    }

    if (priority < 7 && player->damagecount) {
        if (st_oldhealth - player->health > ST_MUCHPAIN) {
            priority = 7;
            st_facecount = ST_TURNCOUNT;
            st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
        } else {
            priority = 6;
            st_facecount = ST_TURNCOUNT;
            st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
        }
    }

    if (priority < 6) {
        // rapid firing
        if (player->attackdown) {
            if (lastattackdown==-1) {
                lastattackdown = ST_RAMPAGEDELAY;
            } else if (!--lastattackdown) {
                priority = 5;
                st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
                st_facecount = 1;
                lastattackdown = 1;
            }
        } else {
            lastattackdown = -1;
        }
    }

    if (priority < 5 && ((player->cheats & CF_GODMODE) || player->powers[pw_invulnerability])) {
        priority = 4;

        st_faceindex = ST_GODFACE;
        st_facecount = 1;
    }

    // look left or look right if the facecount has timed out
    if (!st_facecount) {
        st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
        st_facecount = ST_STRAIGHTFACECOUNT;
        priority = 0;
    }

    st_facecount--;

}

void ST_doPaletteStuff(void)
{
    int palette;
    byte* pal;
    int cnt;
    int bzc;

    cnt = player->damagecount;

    if (player->powers[pw_strength]) {
        bzc = 12 - (player->powers[pw_strength]>>6);
        if (bzc > cnt) {
            cnt = bzc;
        }
    }

    if (cnt) {
        palette = (cnt+7)>>3;

        if (palette >= NUMREDPALS)
            palette = NUMREDPALS-1;

        palette += STARTREDPALS;
    } else if (player->bonuscount) {
        palette = (player->bonuscount+7)>>3;

        if (palette >= NUMBONUSPALS)
            palette = NUMBONUSPALS-1;

        palette += STARTBONUSPALS;
    } else if (player->powers[pw_ironfeet] > 4*32 || player->powers[pw_ironfeet]&8) {
        palette = RADIATIONPAL;
    } else {
        palette = 0;
    }

    if (palette != st_palette) {
        st_palette = palette;
        pal = (byte *) W_CacheLumpNum (lu_palette, PU_CACHE)+palette*768;
        I_SetPalette (pal);
    }
}

// Called by main loop.
void ST_Ticker (void)
{
    st_randomnumber = M_Random();
    ST_updateFaceWidget();
    st_oldhealth = player->health;
}

void st_draw_elements(sbarelem_t** elems, int len, int parent_x, int parent_y)
{
    for (int i = 0; i < len; ++i) {
        sbarelem_t* current_elem = elems[i];
        if (!check_conditions(current_elem->canvas.conditions, current_elem->canvas.conditions_len)) continue;
        switch(current_elem->type) {
        case SBAR_ELEM_CANVAS:
            break;
        case SBAR_ELEM_GRAPHIC:
            st_draw_graphic(&current_elem->graphic, parent_x, parent_y);
            break;
        case SBAR_ELEM_ANIMATION:
            // todo
            break;
        case SBAR_ELEM_FACE:
            st_draw_face(&current_elem->face, parent_x, parent_y);
            break;
        case SBAR_ELEM_FACE_BG:
            st_draw_face_bg(&current_elem->face_bg, parent_x, parent_y);
            break;
        case SBAR_ELEM_NUMBER:
            st_draw_number(&current_elem->number, false, parent_x, parent_y);
            break;
        case SBAR_ELEM_PERCENT:
            st_draw_number(&current_elem->percent, true, parent_x, parent_y);
            break;
        default:
            I_Error("Unhandled stbar element %d", current_elem->type);
        }

        // kinda ugly to use canvas here regardless of the actual element type,
        // but sbarelem_t is a union where every member shares same members as canvas, so it works fine
        if (current_elem->canvas.children_len > 0) {
            st_draw_elements(current_elem->canvas.children, current_elem->canvas.children_len,
                current_elem->canvas.x + parent_x, current_elem->canvas.y + parent_y);
        }
    }
}

// Called by main loop.
void ST_Drawer (boolean fullscreen, boolean refresh)
{
    ST_doPaletteStuff();

    int bar_y = active_bar->fullscreen_render ? 0 : SCREENHEIGHT - active_bar->height;

    st_draw_elements(active_bar->children, active_bar->children_len, 0, bar_y);
}

// Called when the console player is spawned on each level.
void ST_Start (void)
{
    player = &players[consoleplayer];
}

void st_load_face_patches(void)
{
    const char* face_patch_names[] = {
        "STFST01", "STFST00", "STFST02", "STFTR00", "STFTL00", "STFOUCH0", "STFEVL0", "STFKILL0",
        "STFST11", "STFST10", "STFST12", "STFTR10", "STFTL10", "STFOUCH1", "STFEVL1", "STFKILL1",
        "STFST21", "STFST20", "STFST22", "STFTR20", "STFTL20", "STFOUCH2", "STFEVL2", "STFKILL2",
        "STFST31", "STFST30", "STFST32", "STFTR30", "STFTL30", "STFOUCH3", "STFEVL3", "STFKILL3",
        "STFST41", "STFST40", "STFST42", "STFTR40", "STFTL40", "STFOUCH4", "STFEVL4", "STFKILL4",
        "STFGOD0", "STFDEAD0"};

    face_patches = Z_Malloc(sizeof(patch_t*)*ST_NUMFACES, PU_STATIC, NULL);
    for (int i = 0; i < ST_NUMFACES; ++i) {
        face_patches[i] = W_CacheLumpName(face_patch_names[i], PU_STATIC);
    }
}

// Called by startup code.
void ST_Init (void)
{
    st_load_face_patches();

    parse_sbardef();

    lu_palette = W_GetNumForName ("PLAYPAL");

    active_bar = status_bars[0];

    screens[4] = (byte *) Z_Malloc(STBAR_WIDTH*active_bar->height, PU_STATIC, 0);
}
