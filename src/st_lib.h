
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

#include "doomtype.h"
#include "r_defs.h"

typedef union sbarelem_t sbarelem_t;

typedef enum sbarelem_type_t {
    SBAR_ELEM_CANVAS,
    SBAR_ELEM_GRAPHIC,
    SBAR_ELEM_ANIMATION,
    SBAR_ELEM_FACE,
    SBAR_ELEM_FACE_BG,
    SBAR_ELEM_NUMBER,
    SBAR_ELEM_PERCENT
} sbarelem_type_t;

typedef enum sbar_condition_enum_t {
    SBAR_COND_PARAM_WEAPON_OWNED,
    SBAR_COND_PARAM_WEAPON_SELECTED,
    SBAR_COND_PARAM_WEAPON_NOT_SELECTED,
    SBAR_COND_PARAM_WEAPON_HAS_VALID_AMMO_TYPE,
    SBAR_COND_SELECTED_WEAPON_HAS_VALID_AMMO_TYPE,
    SBAR_COND_PARAM_AMMO_MATCHES_SELECTED_WEAPON,
    SBAR_COND_PARAM_SLOT_ANY_WEAPON_OWNED,
    SBAR_COND_PARAM_SLOT_ANY_WEAPON_NOT_OWNED,
    SBAR_COND_PARAM_SLOT_ANY_WEAPON_SELECTED,
    SBAR_COND_PARAM_SLOT_ANY_WEAPON_NOT_SELECTED,
    SBAR_COND_PARAM_ITEM_OWNED,
    SBAR_COND_PARAM_ITEM_NOT_OWNED,
    SBAR_COND_PARAM_FEATURE_LEVEL_GREATER_EQUAL_GAME_VERSION,
    SBAR_COND_PARAM_FEATURE_LEVEL_LESSER_GAME_VERSION,
    SBAR_COND_PARAM_SESSION_TYPE_EQUAL_CURRENT_SESSION,
    SBAR_COND_PARAM_SESSION_TYPE_NOT_EQUAL_CURRENT_SESSION,
    SBAR_COND_PARAM_GAME_MODE_EQUAL_CURRENT_MODE,
    SBAR_COND_17, // 0.99.2 specification copies the description from condition 16 ??
    SBAR_COND_PARAM_HUD_MODE_EQUAL_CURRENT_MODE
} sbar_condition_enum_t;

typedef struct sbar_condition_t {
    sbar_condition_enum_t condition;
    int param;
} sbar_condition_t;

// Number font

typedef enum numberfont_type_t {
    SBAR_NUMFONT_TYPE_MONO_SPACED_ZERO, // based on the width of the 0 glyph, vanilla sbar default
    SBAR_NUMFONT_TYPE_MONO_SPACED_WIDEST, // based on the widest glyph
    SBAR_NUMFONT_TYPE_PROPORTIONAL
} numberfont_type_t;

typedef struct numberfont_t {
    numberfont_type_t type;
    patch_t** patches;
} numberfont_t;

// Status bar elements

#define ST_ALIGN_HOR_LEFT // 0
#define ST_ALIGN_HOR_CENTER 1
#define ST_ALIGN_HOR_RIGHT (1 << 1)

#define ST_ALIGN_VERT_TOP // 0
#define ST_ALIGN_VERT_CENTER (1 << 2)
#define ST_ALIGN_VERT_BOTTOM (1 << 3)


typedef struct sbar_canvas_t {
    sbarelem_type_t type;
    int x, y;
    int alignment; // which point is defined by the x,y coords - top left corner, bottom right, center right, etc
    char* tranmap; // boom-style translucency map lump
    char* translation;

    int conditions_len;
    sbar_condition_t* conditions;

    int children_len;
    sbarelem_t** children;
} sbar_canvas_t;

typedef sbar_canvas_t sbar_face_t;
typedef sbar_canvas_t sbar_face_bg_t;

typedef struct sbar_graphic_t {
    sbarelem_type_t type;
    int x, y;
    int alignment;
    char* tranmap;
    char* translation;

    int conditions_len;
    sbar_condition_t* conditions;

    int children_len;
    sbarelem_t** children;

    patch_t* patch;
} sbar_graphic_t;

// animation

typedef struct sbar_anim_frame_t {
    char* patch;
    int duration; // in seconds
} sbar_anim_frame_t;

typedef struct sbar_anim_t {
    sbarelem_type_t type;
    int x, y;
    int alignment;
    char* tranmap;
    char* translation;

    int conditions_len;
    sbar_condition_t* conditions;

    int children_len;
    sbarelem_t** children;

    int frames_len;
    sbar_anim_frame_t* frames;
} sbar_anim_t;

// number & percent number

typedef enum sbar_number_type_t {
    SBAR_NUMTYPE_HEALTH,                // 0
    SBAR_NUMTYPE_ARMOR,                 // 1
    SBAR_NUMTYPE_FRAGS,                 // 2
    SBAR_NUMTYPE_AMMO_PARAM_AMMO,       // 3
    SBAR_NUMTYPE_AMMO_CURRENT_WEAP,     // 4
    SBAR_NUMTYPE_MAX_AMMO_PARAM_AMMO,   // 5
    SBAR_NUMTYPE_AMMO_PARAM_WEAP,       // 6
    SBAR_NUMTYPE_MAX_AMMO_PARAM_WEAP    // 7
} sbar_number_type_t;

typedef struct sbar_number_t {
    sbarelem_type_t type;
    int x, y;
    int alignment;
    char* tranmap;
    char* translation;

    int conditions_len;
    sbar_condition_t* conditions;

    int children_len;
    sbarelem_t** children;

    int font; // index to an array of fonts read from SBARDEF; json refers to them by name
    sbar_number_type_t number_type;
    int parameter;
    int max_length; // max number of glyphs to render, excluding percent sign
} sbar_number_t;

typedef sbar_number_t sbar_percent_t;

union sbarelem_t {
    sbarelem_type_t type;
    sbar_canvas_t canvas;
    sbar_graphic_t graphic;
    sbar_anim_t animation;
    sbar_face_t face;
    sbar_face_bg_t face_bg;
    sbar_number_t number;
    sbar_percent_t percent;
};

// Status bar

typedef struct statusbar_t {
    int height;
    boolean fullscreen_render;
    char* fill_flat;

    int children_len;
    sbarelem_t** children;
} statusbar_t;

// Loading functions

// common sbar elem fields to shorten function signatures
typedef struct sbarelem_loadinfo_t {
    int x;
    int y;
    int alignment;
    char* tranmap;
    char* translation;
    int conditions_len;
    sbar_condition_t* conditions;
} sbarelem_loadinfo_t;


boolean check_conditions(sbar_condition_t* conditions, int conditions_len);

numberfont_t* load_number_font(numberfont_type_t type, const char* stem);

sbarelem_t* load_canvas(const sbarelem_loadinfo_t* loadinfo);
sbarelem_t* load_graphic(const sbarelem_loadinfo_t* loadinfo, char* patch_name);
sbarelem_t* load_animation(const sbarelem_loadinfo_t* loadinfo);
sbarelem_t* load_face(const sbarelem_loadinfo_t* loadinfo);
sbarelem_t* load_face_bg(const sbarelem_loadinfo_t* loadinfo);
sbarelem_t* load_number(const sbarelem_loadinfo_t* loadinfo,
    int font,
    sbar_number_type_t number_type,
    int parameter,
    int max_length);
sbarelem_t* load_percent(const sbarelem_loadinfo_t* loadinfo,
    int font,
    sbar_number_type_t number_type,
    int parameter,
    int max_length);

// Drawing functions

void st_draw_graphic(sbar_graphic_t* widget, int parent_x, int parent_y);
void st_draw_number(sbar_number_t* widget, boolean percent, int parent_x, int parent_y);
void st_draw_face(sbar_face_t* widget, int parent_x, int parent_y);
void st_draw_face_bg(sbar_face_bg_t* widget, int parent_x, int parent_y);
