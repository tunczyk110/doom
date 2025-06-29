
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

#include <jansson.h>

#include "w_wad.h"
#include "z_zone.h"
#include "v_video.h"
#include "doomstat.h"
#include "i_system.h"

extern int st_faceindex;

patch_t** face_patches = NULL;

extern int statusbars_len;
extern statusbar_t** status_bars;

extern int numberfonts_len;
extern numberfont_t** number_fonts;

char** numfont_names;

int get_numfont_index_from_name(const char* name) {
    for (int i = 0; i < numberfonts_len; ++i) {
        if (!strcmp(name, numfont_names[i])) {
            return i;
        }
    }
    return -1;
}

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

// json_string_value returns a pointer json owns
// it gets freed when we decref json, therefore
// we need to copy it if we wish to unload json
// after parsing the entire tree
char* copy_string_value(json_t* json) {
    int len = json_string_length(json);
    if (!len) return NULL;
    char* ret = Z_Malloc(len, PU_STATIC, NULL);
    strcpy(ret, json_string_value(json));
    return ret;
}

const char* elem_type_names[] = {
    "canvas",
    "graphic",
    "animation",
    "face",
    "facebackground",
    "number",
    "percent"
};

void load_conditions(sbar_condition_t** conditions, int* conditions_len, json_t* json_arr) {
    *conditions_len = json_array_size(json_arr);
    if (!conditions_len) {
        *conditions = NULL;
        return;
    }

    sbar_condition_t* conds = *conditions = Z_Malloc(sizeof(sbar_condition_t) * *conditions_len, PU_STATIC, NULL);
    for (int i = 0; i < *conditions_len; ++i) {
        json_t* curr_cond = json_array_get(json_arr, i);
        conds[i].condition = json_integer_value(json_object_get(curr_cond, "condition"));
        conds[i].param = json_integer_value(json_object_get(curr_cond, "param"));
    }
}

void load_children(sbarelem_t*** children, int* children_len, json_t* json_arr) {
    *children_len = json_array_size(json_arr);
    if (!children_len) {
        *children = NULL;
        return;
    }

    sbarelem_t** children_arr = *children = Z_Malloc(sizeof(sbarelem_t*) * (*children_len), PU_STATIC, NULL);
    for (int i = 0; i < *children_len; ++i) {
        sbarelem_type_t elem_type;
        json_t* child_data = json_array_get(json_arr, i);
        for (int type = 0; type < SBAR_ELEM_TOTAL; ++type) {
            json_t* element_data = json_object_get(child_data, elem_type_names[type]);
            if (json_is_object(element_data)) {
                child_data = element_data;
                elem_type = type;
                break;
            }
        }

        int cond_len = 0;
        sbar_condition_t* conds = NULL;
        load_conditions(&conds, &cond_len, json_object_get(child_data, "conditions"));

        sbarelem_loadinfo_t loadinfo = {
            .x = json_integer_value(json_object_get(child_data, "x")),
            .y = json_integer_value(json_object_get(child_data, "y")),
            .alignment = json_integer_value(json_object_get(child_data, "alignment")),
            .tranmap = copy_string_value(json_object_get(child_data, "tranmap")),
            .translation = copy_string_value(json_object_get(child_data, "translation")),
            .conditions_len = cond_len,
            .conditions = conds
        };

        switch(elem_type) {
        case SBAR_ELEM_CANVAS:
            children_arr[i] = load_canvas(&loadinfo);
            break;
        case SBAR_ELEM_GRAPHIC:
            children_arr[i] = load_graphic(&loadinfo,
                json_string_value(json_object_get(child_data, "patch")));
            break;
        case SBAR_ELEM_ANIMATION:
            I_Error("Unimplemented SBAR_ELEM_ANIMATION");
        case SBAR_ELEM_FACE:
            children_arr[i] = load_face(&loadinfo);
            break;
        case SBAR_ELEM_FACE_BG:
            children_arr[i] = load_face_bg(&loadinfo);
            break;
        case SBAR_ELEM_NUMBER:
            children_arr[i] = load_number(&loadinfo,
                get_numfont_index_from_name(json_string_value(json_object_get(child_data, "font"))),
                json_integer_value(json_object_get(child_data, "type")),
                json_integer_value(json_object_get(child_data, "param")),
                json_integer_value(json_object_get(child_data, "maxlength"))
            );
            break;
        case SBAR_ELEM_PERCENT:
            children_arr[i] = load_percent(&loadinfo,
                get_numfont_index_from_name(json_string_value(json_object_get(child_data, "font"))),
                json_integer_value(json_object_get(child_data, "type")),
                json_integer_value(json_object_get(child_data, "param")),
                json_integer_value(json_object_get(child_data, "maxlength"))
            );
            break;
        case SBAR_ELEM_TOTAL:
        default:
            I_Error("man something fucked up happened");
        }

        load_children(
            &(children_arr[i]->canvas.children),
            &(children_arr[i]->canvas.children_len),
            json_object_get(child_data, "children"));
    }
}

void parse_sbardef()
{
    char* sbardef_lump = cache_text_lump_name("SBARDEF", PU_CACHE);

    json_t* root = NULL;
    json_error_t json_error;

    root = json_loads(sbardef_lump, 0, &json_error);
    if (!root) {
        I_Error("Error parsing SBARDEF on line %d: %s", json_error.line, json_error.text);
    }

    json_t* json_type = json_object_get(root, "type");
    if (!json_is_string(json_type)) {
        I_Error("Error parsing SBARDEF: \"type\" isn't a string");
    }
    const char* type_data = json_string_value(json_type);
    if (strcmp(type_data, "statusbar")) {
        I_Error("Error parsing SBARDEF: type isn't \"statusbar\", it's \"%s\"", type_data);
    }

    json_t* data = json_object_get(root, "data");

    json_t* numberfonts_json = json_object_get(data, "numberfonts");
    if (!json_is_array(numberfonts_json)) {
        I_Error("Error parsing SBARDEF: numberfonts isn't an array");
    }

    numberfonts_len = json_array_size(numberfonts_json);
    number_fonts = Z_Malloc(sizeof(numberfont_t*) * numberfonts_len, PU_STATIC, NULL);
    numfont_names = Z_Malloc(sizeof(char*) * numberfonts_len, PU_STATIC, NULL);

    for (int i = 0; i < numberfonts_len; ++i) {
        json_t* numfont_data = json_array_get(numberfonts_json, i);
        number_fonts[i] = load_number_font(
            json_integer_value(json_object_get(numfont_data, "type")),
            json_string_value(json_object_get(numfont_data, "stem"))
        );
        numfont_names[i] = copy_string_value(json_object_get(numfont_data, "name"));
    }

    json_t* statusbars_json = json_object_get(data, "statusbars");
    if (!json_is_array(statusbars_json)) {
        I_Error("Error parsing SBARDEF: statusbars isn't an array");
    }

    statusbars_len = json_array_size(statusbars_json);
    status_bars = Z_Malloc(sizeof(statusbar_t*) * statusbars_len, PU_STATIC, NULL);

    for (int i = 0; i < statusbars_len; ++i) {
        json_t* statusbar_data = json_array_get(statusbars_json, i);
        statusbar_t* bar = status_bars[i] = Z_Malloc(sizeof(statusbar_t), PU_STATIC, NULL);
        bar->height = json_integer_value(json_object_get(statusbar_data, "height"));
        bar->fill_flat = copy_string_value(json_object_get(statusbar_data, "fillflat"));
        bar->fullscreen_render = json_boolean_value(json_object_get(statusbar_data, "fullscreenrender"));
        load_children(&bar->children, &bar->children_len, json_object_get(statusbar_data, "children"));
    }

    // free the now unneeded number font names
    for (int i = 0; i < numberfonts_len; ++i) {
        Z_Free(numfont_names[i]);
    }
    Z_Free(numfont_names);
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
    // trust the modders that they won't try to render minus or percent
    // with a font that doesn't support it
    if (W_CheckNumForName(lumpname) > 0) {
        font->patches[NUMFONT_MINUS_INDEX] = W_CacheLumpName(lumpname, PU_STATIC);
    } else {
        font->patches[NUMFONT_MINUS_INDEX] = NULL;
    }
    sprintf(lumpname, "%sPRCNT", stem);
    if (W_CheckNumForName(lumpname) > 0) {
        font->patches[NUMFONT_PRCNT_INDEX] = W_CacheLumpName(lumpname, PU_STATIC);
    } else {
        font->patches[NUMFONT_PRCNT_INDEX] = NULL;
    }

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

sbarelem_t* load_graphic(const sbarelem_loadinfo_t* loadinfo, const char* patch_name)
{
    sbarelem_t* elem = load_canvas(loadinfo);

    elem->type = SBAR_ELEM_GRAPHIC;
    if (W_GetNumForName(patch_name) < 0) {
        elem->graphic.patch = NULL;
    } else {
        elem->graphic.patch = W_CacheLumpName(patch_name, PU_STATIC);
    }

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
        int param = conditions[i].param;
        switch(conditions[i].condition) {
        case SBAR_COND_PARAM_WEAPON_OWNED:
            if (!player->weaponowned[param]) return false;
            continue;
        case SBAR_COND_PARAM_WEAPON_SELECTED:
            if (player->readyweapon != param) return false;
            continue;
        case SBAR_COND_PARAM_WEAPON_NOT_SELECTED:
            if (player->readyweapon == param) return false;
            continue;
        case SBAR_COND_PARAM_WEAPON_HAS_VALID_AMMO_TYPE:
            if (param == wp_fist || param == wp_chainsaw) return false;
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
            if (get_session_type() != param) return false;
            continue;
        case SBAR_COND_PARAM_SESSION_TYPE_NOT_EQUAL_CURRENT_SESSION:
            if (get_session_type() == param) return false;
            continue;
        case SBAR_COND_PARAM_GAME_MODE_EQUAL_CURRENT_MODE:
        case SBAR_COND_17:
        case SBAR_COND_PARAM_HUD_MODE_EQUAL_CURRENT_MODE:
            // todo when gameconf is implemented
            return false;
        default:
            continue;
        }
    }
    return true;
}

void st_draw_graphic(sbar_graphic_t* widget, int parent_x, int parent_y)
{
    if (!widget->patch) return;

    int x = widget->x + parent_x,
        y = widget->y + parent_y;
    align_coordinates(&x, &y, widget->alignment, widget->patch->width, widget->patch->height);
    V_DrawPatch(x, y, 4, widget->patch);
}

void st_draw_number(sbar_number_t* widget, boolean percent, int parent_x, int parent_y)
{
    player_t* player = &players[consoleplayer];
    int display_value = 0;

    switch(widget->number_type) {
    case SBAR_NUMTYPE_HEALTH:
        display_value = player->mo->health;
        break;
    case SBAR_NUMTYPE_ARMOR:
        display_value = player->armorpoints;
        break;
    case SBAR_NUMTYPE_FRAGS:
        // no deathmatch in this port (yet?)
        return;
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

    numberfont_t* font = number_fonts[widget->font];

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

    // all the glyphs in a font are the same height I think?
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
