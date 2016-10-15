/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include "hud.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include <lh_buffers.h>
#include <lh_files.h>
#include <lh_debug.h>
#include <lh_bytes.h>
#include <lh_image.h>

#include "mcp_ids.h"
#include "mcp_build.h"
#include "mcp_bplan.h"
#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_arg.h"

////////////////////////////////////////////////////////////////////////////////

int hud_mode  = HUDMODE_NONE;
int hud_valid = 1;
int hud_id=-1;

uint8_t hud_image[16384];

// TODO: color constants
uint8_t draw_color = 34;

lhimage * fonts = NULL;

#define FONTS_OFFSET 0
#define FONTS_W      4
#define FONTS_H      6

// TODO: selectable fonts
int font_w = FONTS_W;
int font_h = FONTS_H;
int font_o = FONTS_OFFSET;

////////////////////////////////////////////////////////////////////////////////

void draw_clear() {
    memset(hud_image, draw_color, sizeof(hud_image));
}

void draw_glyph(int col, int row, char l) {
    if (l<0x20 || l>0x7f) return;

    uint32_t *glyph = &IMGDOT(fonts, (l&15)*font_w, ((l>>4)-2)*font_h+font_o);
    uint8_t  *hud   = hud_image+col+row*128;

    int r,c;
    for(r=0; r<font_h; r++) {
        uint32_t *glyr = glyph+r*fonts->stride;
        uint8_t  *hudr = hud+r*128;
        for(c=0; c<font_w; c++) {
            if ((glyr[c]&0xffffff) == 0xffffff)
                hudr[c] = draw_color;
        }
    }
}

void draw_text(int col, int row, char *s) {
    int i;
    for(i=0; s[i]; i++)
        draw_glyph(col+i*FONTS_W, row, s[i]);
}

////////////////////////////////////////////////////////////////////////////////

int hud_new(char * reply, MCPacketQueue *cq) {
    // find a free slot in user's inventory (including the off-hand slot)
    // search in descending order, so the hotbar gets preference
    int sid=(currentProtocol>=PROTO_1_9)?45:44;
    while(sid>=9) {
        if (gs.inv.slots[sid].item <= 0) break;
        sid--;
    }
    if (sid<9) {
        sprintf(reply, "no free inventory slot to give you a new item");
        return -1;
    }

    NEWPACKET(SP_SetSlot, ss);
    tss->wid = 0;
    tss->sid = sid;
    tss->slot.item = 358;
    tss->slot.count = 1;
    tss->slot.damage = DEFAULT_MAP_ID;
    tss->slot.nbt = NULL;

    gs_packet(ss);
    queue_packet(ss,cq);

    return DEFAULT_MAP_ID;
}

int hud_bind(char *reply, int id) {
    if (id<0 || id>32767) {
        sprintf(reply, "Map ID must be in range 0..32767");
        return -1;
    }

    if (!fonts)
        fonts = import_png_file("hudfonts.png");

    if (!fonts) {
        sprintf(reply, "Failed to load fonts");
        return -1;
    }

    sprintf(reply, "Binding HUD to Map ID %d", id);
    hud_id = id;
    return id;
}

void hud_unbind(char *reply, MCPacketQueue *cq) {
    int sid;
    for(sid=0; sid<=45; sid++) {
        if (gs.inv.slots[sid].item == 358 && gs.inv.slots[sid].damage == DEFAULT_MAP_ID) {
            NEWPACKET(SP_SetSlot, ss);
            tss->wid = 0;
            tss->sid = sid;
            clear_slot(&tss->slot);
            queue_packet(ss,cq);
            clear_slot(&gs.inv.slots[sid]);
        }
    }

    if (gs.inv.drag.item == 358 && gs.inv.drag.damage == DEFAULT_MAP_ID) {
        NEWPACKET(SP_SetSlot, ss);
        tss->wid = 255;
        tss->sid = -1;
        clear_slot(&tss->slot);
        queue_packet(ss,cq);
        clear_slot(&gs.inv.drag);
    }

    hud_id = -1;
    sprintf(reply, "Unbinding HUD");
}

////////////////////////////////////////////////////////////////////////////////

void huddraw_test() {
    int i;
    for (i=1; i<=21; i++) {
        uint8_t color = i*4+2;
        int row = (i-1)*6+1;
        char text[256];
        sprintf(text, "Color %d\n",color);
        draw_color = color;
        draw_text(5, row, text);
    }
}

void huddraw_nav() {
    char text[256];

    draw_color = 18;

    sprintf(text, "X:%7d Z:%7d", (int32_t)floor(gs.own.x), (int32_t)floor(gs.own.z));
    draw_text(4, 4, text);

    if (gs.world==&gs.nether)
        sprintf(text, "X:%7d Z:%7d Overworld", (int32_t)floor(gs.own.x)*8, (int32_t)floor(gs.own.z)*8);
    else
        sprintf(text, "X:%7d Z:%7d Nether", (int32_t)floor(gs.own.x)/8, (int32_t)floor(gs.own.z)/8);
    draw_text(4, 12, text);

    char * dir = "UNKNOWN";
    switch(player_direction()) {
        case DIR_NORTH : dir = "NORTH"; break;
        case DIR_SOUTH : dir = "SOUTH"; break;
        case DIR_EAST  : dir = "EAST"; break;
        case DIR_WEST  : dir = "WEST"; break;
    }

    sprintf(text, "Y:%7d D:%s", (int32_t)floor(gs.own.y), dir);
    draw_text(4, 20, text);
}

////////////////////////////////////////////////////////////////////////////////

/*
 * hud new  # give player a new fake map (32767) and bind HUD to it
 * hud NNN  # bind HUD to a specific map ID
 * hud -    # unbind HUD and remove the fake map from player's inventory
 *
 * hud test         # test picture
 */

void hud_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq) {
    char reply[32768];
    reply[0] = 0;
    int rpos = 0;

    int id;
    int oldmode = hud_mode;

    if (!words[1] || !strcmp(words[1],"new")) {
        id = hud_new(reply, cq);
        if (id < 0) goto Error;
        hud_bind(reply, id);
        hud_valid = 0;
    }
    else if (sscanf(words[1], "%u", &id)==1) {
        hud_bind(reply, id);
        hud_valid = 0;
    }
    else if (!strcmp(words[1],"-")) {
        hud_unbind(reply, cq);
    }

    else if (!strcmp(words[1],"test")) {
        hud_mode = HUDMODE_TEST;
    }

    else if (!strcmp(words[1],"nav")) {
        hud_mode = HUDMODE_NAV;
    }

    if (oldmode != hud_mode) hud_valid = 0;

 Error:
    if (reply[0]) chat_message(reply, cq, "green", rpos);
}

void hud_update(MCPacketQueue *cq) {
    if (hud_id < 0 || hud_valid) return;

    draw_color = 34;
    draw_clear();

    switch(hud_mode) {
        case HUDMODE_TEST:
            huddraw_test();
            break;

        case HUDMODE_NAV:
            huddraw_nav();
            break;

        case HUDMODE_NONE:
        default:
            break;
    }

    NEWPACKET(SP_Map, map);
    tmap->mapid    = hud_id;
    tmap->scale    = 0;
    tmap->trackpos = 0;
    tmap->nicons   = 0;
    tmap->icons    = NULL;
    tmap->ncols    = 128;
    tmap->nrows    = 128;
    tmap->X        = 0;
    tmap->Z        = 0;
    tmap->len      = sizeof(hud_image);
    lh_alloc_num(tmap->data, sizeof(hud_image));
    memmove(tmap->data, hud_image, sizeof(hud_image));

    queue_packet(map, cq);

    hud_valid = 1;
}

void hud_invalidate(int mode) {
    if (mode == hud_mode) hud_valid = 0;
}
