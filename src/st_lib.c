
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

#include "st_lib.h"

#include <math.h>

#include "w_wad.h"
#include "z_zone.h"
#include "v_video.h"
#include "doomstat.h"
#include "i_system.h"

extern numberfont_t* number_fonts;
extern int st_faceindex;

patch_t** face_patches = NULL;

int num_digits (int n) {
    if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
    if (n < 10) return 1;
    if (n < 100) return 2;
    if (n < 1000) return 3;
    if (n < 10000) return 4;
    if (n < 100000) return 5;
    if (n < 1000000) return 6;

    // really, how large could numbers on HUD be?
    // there isn't a mod out there that gives you 70 million ammo for your chaingun, is there?
    I_Error("Number too large %d", n);
    return 0;
}

void align_coordinates(int* x, int* y, int alignment, int width, int height) {
    // horizontal alignment - left aligned by default
    if (alignment & ST_ALIGN_HOR_CENTER) {
        *x -= width / 2;
    } else if (alignment & ST_ALIGN_HOR_RIGHT) {
        *x -= width;
    }

    // vertical alignment - top aligned by default
    if (alignment & ST_ALIGN_VERT_CENTER) {
        *y -= height / 2;
    } else if (alignment & ST_ALIGN_VERT_BOTTOM) {
        *y -= height;
    }
}

#define NUMFONT_MINUS_INDEX 10
#define NUMFONT_PRCNT_INDEX 11
#define NUMFONT_PATCHES_LEN NUMFONT_PRCNT_INDEX

numberfont_t* load_number_font(numberfont_type_t type, const char* stem)
{
    numberfont_t* font = Z_Malloc(sizeof(numberfont_t), PU_STATIC, NULL);
    font->type = type;
    font->patches = Z_Malloc(sizeof(patch_t*)*NUMFONT_PATCHES_LEN, PU_STATIC, NULL);

    char lumpname[9];
    for (int i = 0; i < 10; ++i) {
        sprintf(lumpname, "%sNUM%d", stem, i);
        font->patches[i] = W_CacheLumpName(lumpname, PU_STATIC);
    }
    sprintf(lumpname, "%sMINUS", stem);
    font->patches[NUMFONT_MINUS_INDEX] = W_CacheLumpName(lumpname, PU_STATIC);
    sprintf(lumpname, "%sPRCNT", stem);
    font->patches[NUMFONT_PRCNT_INDEX] = W_CacheLumpName(lumpname, PU_STATIC);

    return font;
}

// we're reusing this function in subsequent element types
sbarelem_t* load_canvas(const sbarelem_loadinfo_t* loadinfo)
{
    sbarelem_t* elem = Z_Malloc(sizeof(sbarelem_t), PU_STATIC, NULL);

    elem->type = SBAR_ELEM_CANVAS;

    elem->canvas.x = loadinfo->x;
    elem->canvas.y = loadinfo->y;
    elem->canvas.alignment = loadinfo->alignment;
    elem->canvas.tranmap = loadinfo->tranmap;
    elem->canvas.translation = loadinfo->translation;
    elem->canvas.conditions_len = loadinfo->conditions_len;
    elem->canvas.conditions = loadinfo->conditions;
    elem->canvas.children = NULL;
    elem->canvas.children_len = 0;

    return elem;
}

sbarelem_t* load_graphic(const sbarelem_loadinfo_t* loadinfo, char* patch_name)
{
    sbarelem_t* elem = load_canvas(loadinfo);

    elem->type = SBAR_ELEM_GRAPHIC;
    elem->graphic.patch = W_CacheLumpName(patch_name, PU_STATIC);

    return elem;
}

sbarelem_t* load_animation(const sbarelem_loadinfo_t* loadinfo)
{}

sbarelem_t* load_face(const sbarelem_loadinfo_t* loadinfo)
{
    sbarelem_t* elem = load_canvas(loadinfo);
    elem->type = SBAR_ELEM_FACE;
    return elem;
}

sbarelem_t* load_face_bg(const sbarelem_loadinfo_t* loadinfo)
{
    sbarelem_t* elem = load_canvas(loadinfo);
    elem->type = SBAR_ELEM_FACE_BG;
    return elem;
}

sbarelem_t* load_number(const sbarelem_loadinfo_t* loadinfo,
    int font,
    sbar_number_type_t number_type,
    int parameter,
    int max_length)
{
    sbarelem_t* elem = load_canvas(loadinfo);
    elem->type = SBAR_ELEM_NUMBER;
    elem->number.font = font;
    elem->number.number_type = number_type;
    elem->number.parameter = parameter;
    elem->number.max_length = max_length;
    return elem;
}

sbarelem_t* load_percent(const sbarelem_loadinfo_t* loadinfo,
    int font,
    sbar_number_type_t number_type,
    int parameter,
    int max_length)
{
    sbarelem_t* elem = load_number(loadinfo, font, number_type, parameter, max_length);
    elem->type = SBAR_ELEM_PERCENT;
    return elem;
}

boolean check_conditions(sbar_condition_t* conditions, int conditions_len)
{
    player_t* player = &players[consoleplayer];
    for (int i = 0; i < conditions_len; ++i) {
        switch(conditions[i].condition) {
        case SBAR_COND_PARAM_WEAPON_OWNED:
            if (!player->weaponowned[conditions[i].param]) return false;
            continue;
        case SBAR_COND_PARAM_WEAPON_SELECTED:
            if (player->readyweapon != conditions[i].param) return false;
            continue;
        case SBAR_COND_PARAM_WEAPON_NOT_SELECTED:
            if (player->readyweapon == conditions[i].param) return false;
            continue;
        case SBAR_COND_PARAM_WEAPON_HAS_VALID_AMMO_TYPE:
            if (conditions[i].param == wp_fist || conditions[i].param == wp_chainsaw) return false;
            continue;
        case SBAR_COND_SELECTED_WEAPON_HAS_VALID_AMMO_TYPE:
            if (player->readyweapon == wp_fist || player->readyweapon == wp_chainsaw) return false;
            continue;
        case SBAR_COND_PARAM_AMMO_MATCHES_SELECTED_WEAPON:
        case SBAR_COND_PARAM_SLOT_ANY_WEAPON_OWNED:
        case SBAR_COND_PARAM_SLOT_ANY_WEAPON_NOT_OWNED:
        case SBAR_COND_PARAM_SLOT_ANY_WEAPON_SELECTED:
        case SBAR_COND_PARAM_SLOT_ANY_WEAPON_NOT_SELECTED:
            // todo when arbitrary weapons and slots are implemented with dehacked
            return false;
        case SBAR_COND_PARAM_ITEM_OWNED:
        case SBAR_COND_PARAM_ITEM_NOT_OWNED:
            // todo: uses id24hacked indices
            return false;
        case SBAR_COND_PARAM_FEATURE_LEVEL_GREATER_EQUAL_GAME_VERSION:
        case SBAR_COND_PARAM_FEATURE_LEVEL_LESSER_GAME_VERSION:
            // todo when gameconf is implemented
            return false;
        case SBAR_COND_PARAM_SESSION_TYPE_EQUAL_CURRENT_SESSION:
        case SBAR_COND_PARAM_SESSION_TYPE_NOT_EQUAL_CURRENT_SESSION:
        case SBAR_COND_PARAM_GAME_MODE_EQUAL_CURRENT_MODE:
        case SBAR_COND_17:
        case SBAR_COND_PARAM_HUD_MODE_EQUAL_CURRENT_MODE:
            // todo
            return false;
        default:
            continue;
        }
    }
    return true;
}

void st_draw_graphic(sbar_graphic_t* widget, int parent_x, int parent_y)
{
    int x = widget->x + parent_x,
        y = widget->y + parent_y;
    align_coordinates(&x, &y, widget->alignment, widget->patch->width, widget->patch->height);
    V_DrawPatch(x, y, 4, widget->patch);
}

void st_draw_number(sbar_number_t* widget, boolean percent, int parent_x, int parent_y)
{
    player_t* player = &players[consoleplayer];
    int display_value = -999;

    switch(widget->number_type) {
    case SBAR_NUMTYPE_HEALTH:
        display_value = player->mo->health;
        break;
    case SBAR_NUMTYPE_ARMOR:
        display_value = player->armorpoints;
        break;
    case SBAR_NUMTYPE_FRAGS:
        // no deathmatch in this port (yet?)
        I_Error("Unimplemented SBAR_NUMTYPE_FRAGS");
    case SBAR_NUMTYPE_AMMO_PARAM_AMMO:
        display_value = player->ammo[widget->parameter];
        break;
    case SBAR_NUMTYPE_AMMO_CURRENT_WEAP:
        display_value = player->ammo[weaponinfo[player->readyweapon].ammo];
        break;
    case SBAR_NUMTYPE_MAX_AMMO_PARAM_AMMO:
        display_value = player->maxammo[widget->parameter];
        break;
    case SBAR_NUMTYPE_AMMO_PARAM_WEAP:
        display_value = player->ammo[weaponinfo[widget->parameter].ammo];
        break;
    case SBAR_NUMTYPE_MAX_AMMO_PARAM_WEAP:
        display_value = player->maxammo[weaponinfo[widget->parameter].ammo];
        break;
    }

    numberfont_t* font = &number_fonts[widget->font];

    // total width of all displayed characters - for calculating aligned position
    int width = 0;
    switch(font->type) {
    case SBAR_NUMFONT_TYPE_MONO_SPACED_ZERO:
        const int zero_width = font->patches[0]->width;
        width = num_digits(display_value) * zero_width;
        if (percent) width += zero_width;
        if (display_value < 0) width += zero_width;
        break;
    case SBAR_NUMFONT_TYPE_MONO_SPACED_WIDEST:
        I_Error("unimplemented SBAR_NUMFONT_TYPE_MONO_SPACED_WIDEST");
    case SBAR_NUMFONT_TYPE_PROPORTIONAL:
        I_Error("unimplemented SBAR_NUMFONT_TYPE_PROPORTIONAL");
    }

    int height = font->patches[0]->height;

    int x = widget->x + parent_x,
        y = widget->y + parent_y;
    align_coordinates(&x, &y, widget->alignment, width, height);

    int x_shift = 0;
    switch(font->type) {
    case SBAR_NUMFONT_TYPE_MONO_SPACED_ZERO:
        x_shift = font->patches[0]->width;
        break;
    case SBAR_NUMFONT_TYPE_MONO_SPACED_WIDEST:
        I_Error("unimplemented SBAR_NUMFONT_TYPE_MONO_SPACED_WIDEST");
    case SBAR_NUMFONT_TYPE_PROPORTIONAL:
        I_Error("unimplemented SBAR_NUMFONT_TYPE_PROPORTIONAL");
    }

    if (display_value < 0) {
        V_DrawPatch(x, widget->y, 4, font->patches[NUMFONT_MINUS_INDEX]);
        x += x_shift;
        display_value = -display_value;
    }
    boolean found_first_digit = false;
    for (int i = widget->max_length-1; i >= 0; --i) {
        int order = pow(10, i);
        int digit;

        if (display_value >= order) {
            digit = display_value / order;
        } else if (found_first_digit || (display_value == 0 && i == 0)) {
            digit = 0;
        } else continue;

        V_DrawPatch(x, y, 4, font->patches[digit]);
        display_value = display_value % order;
        x += x_shift;
        found_first_digit = true;
    }
    if (percent) {
        V_DrawPatch(x, y, 4, font->patches[NUMFONT_PRCNT_INDEX]);
    }
}


void st_draw_face(sbar_face_t* widget, int parent_x, int parent_y)
{
    int x = widget->x + parent_x,
        y = widget->y + parent_y;
    align_coordinates(&x, &y, widget->alignment, face_patches[st_faceindex]->width, face_patches[st_faceindex]->height);

    V_DrawPatch(x, y, 4, face_patches[st_faceindex]);
}

void st_draw_face_bg(sbar_face_bg_t* widget, int parent_x, int parent_y)
{
    char lump_name[9];
    sprintf(lump_name, "STFB%d", consoleplayer);
    patch_t* patch = W_CacheLumpName(lump_name, PU_CACHE);
    int x = widget->x + parent_x,
        y = widget->y + parent_y;
    align_coordinates(&x, &y, widget->alignment, patch->width, patch->height);
    V_DrawPatch(widget->x + parent_x, widget->y + parent_y, 4, patch);
}
