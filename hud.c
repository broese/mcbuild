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

#define HUDMODE_TEST            0
#define HUDMODE_NAV             1
#define HUDMODE_TUNNEL          2

#define DEFAULT_MAP_ID 32767

int hud_mode        = HUDMODE_TEST;
uint64_t hud_inv    = HUDINV_NONE;
int hud_autoid      = DEFAULT_MAP_ID;
int hud_id          = -1;

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

void hud_unbind(char *reply, MCPacketQueue *cq);

// returns True if the slot contains the bogus map item
int hud_bogus_map(slot_t *s) {
    return (s->item == 358 && s->damage == hud_autoid);
}

// create a new bogus map item for the client
int hud_new(char * reply, MCPacketQueue *cq) {
    hud_unbind(reply, cq);
    reply[0] = 0;

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
    tss->slot.damage = hud_autoid;
    tss->slot.nbt = NULL;

    gs_packet(ss);
    queue_packet(ss,cq);

    return hud_autoid;
}

// bind the HUD to a specific map ID - from now on updates will be sent to that ID
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

    hud_id = id;
    return id;
}

// unbind the HUD
void hud_unbind(char *reply, MCPacketQueue *cq) {
    int sid;
    for(sid=0; sid<=45; sid++) {
        if (hud_bogus_map(&gs.inv.slots[sid])) {
            NEWPACKET(SP_SetSlot, ss);
            tss->wid = 0;
            tss->sid = sid;
            clear_slot(&tss->slot);
            queue_packet(ss,cq);
            clear_slot(&gs.inv.slots[sid]);
        }
    }

    if (hud_bogus_map(&gs.inv.drag)) {
        NEWPACKET(SP_SetSlot, ss);
        tss->wid = 255;
        tss->sid = -1;
        clear_slot(&tss->slot);
        queue_packet(ss,cq);
        clear_slot(&gs.inv.drag);
    }

    hud_id = -1;
}

// workaround for bug MC-46345 - renew map ID when changing dimension
void hud_renew(MCPacketQueue *cq) {
    int sid;
    int hud_newid = hud_autoid-1;

    for(sid=0; sid<=45; sid++) {
        if (hud_bogus_map(&gs.inv.slots[sid])) {
            gs.inv.slots[sid].damage = hud_newid;
            NEWPACKET(SP_SetSlot, ss);
            tss->wid = 0;
            tss->sid = sid;
            clone_slot(&gs.inv.slots[sid], &tss->slot);
            queue_packet(ss,cq);
        }
    }
    if (hud_bogus_map(&gs.inv.drag)) {
        gs.inv.drag.damage = hud_newid;
        NEWPACKET(SP_SetSlot, ss);
        tss->wid = 255;
        tss->sid = -1;
        clone_slot(&gs.inv.drag, &tss->slot);
        queue_packet(ss,cq);
    }

    if (hud_id == hud_autoid) hud_id = hud_newid;
    hud_autoid = hud_newid;
}

////////////////////////////////////////////////////////////////////////////////

#define HUDINVMASK_TUNNEL   (HUDINV_BLOCKS|HUDINV_POSITION|HUDINV_ENTITIES)
#define HUDINVMASK_NAV      (HUDINV_POSITION)
#define HUDINVMASK_MAP      (HUDINV_POSITION|HUDINV_BLOCKS|HUDINV_ENTITIES)
#define HUDINVMASK_BUILD    (HUDINV_INVENTORY|HUDINV_BUILD)

int huddraw_test() {
    int i;
    for (i=1; i<=21; i++) {
        uint8_t color = i*4+2;
        int row = (i-1)*6+1;
        char text[256];
        sprintf(text, "Color %d\n",color);
        draw_color = color;
        draw_text(5, row, text);
    }
    return 1;
}

int huddraw_nav() {
    if (!(hud_inv & HUDINVMASK_NAV)) return 0;

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

    return 1;
}

int huddraw_tunnel() {
    if (!(hud_inv & HUDINVMASK_TUNNEL)) return 0;

    draw_color = 140;
    draw_clear();
    draw_color = 122;

    int32_t x = (int32_t)floor(gs.own.x);
    int32_t y = (int32_t)floor(gs.own.y);
    int32_t z = (int32_t)floor(gs.own.z);
    extent_t ex = { { x-80, y-2, z-80 }, { x+80, y+2, z+80 } };
    cuboid_t cb = export_cuboid_extent(ex);

    int r,c,i,j;
    int32_t off = cb.boff + 16*cb.sa.x + 16;
    for(r=0; r<128; r++) {
        for(c=0; c<128; c++) {
            int poff = off+r*cb.sa.x+c;
            int s1=0,s2=0;
            for(i=-8; i<8; i++) {
                for(j=0; j<5; j++) {
                    s1+= (cb.data[j][poff-1+i*cb.sa.x].bid != 0)
                      -2*(cb.data[j][poff+0+i*cb.sa.x].bid != 0)
                      +  (cb.data[j][poff+1+i*cb.sa.x].bid != 0);
                    s2+= (cb.data[j][poff+i  -cb.sa.x].bid != 0)
                      -2*(cb.data[j][poff+i          ].bid != 0)
                      +  (cb.data[j][poff+i  +cb.sa.x].bid != 0);
                }
            }
            int s = MAX(s1,s2);
            if (s>10) hud_image[r*128+c] = 114;
            if (s>30) hud_image[r*128+c] = 62;
            if (s>60) hud_image[r*128+c] = 74;
            if (s>80) hud_image[r*128+c] = 34;
        }
    }

    for(i=0; i<256; i++) lh_free(cb.data[i]);

    hud_image[64*128+64] = 126;

    char text[256];
    sprintf(text, "X:%6d", x);
    draw_text(2, 2, text);
    sprintf(text, "Z:%6d", z);
    draw_text(2, 9, text);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////

void hud_prune() {
    if (hud_id < 0) return;

    int sid;
    for(sid=0; sid<=45; sid++)
        if (hud_bogus_map(&gs.inv.slots[sid]))
            return;

    hud_id = -1; // HUD item not found in the inventory - invalidate ID
}

/*
 * hud new  # give player a new fake map (32767) and bind HUD to it
 * hud -    # unbind HUD and remove the fake map from player's inventory
 *
 * hud test         # test picture
 * hud nav          # basic navigation info
 * hud tunnel       # tunnel radar
 */

void hud_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq) {
    hud_prune();

    char reply[32768];
    reply[0] = 0;
    int rpos = 0;
    int bind_needed = 1;

    if (!words[1] || !strcmp(words[1],"toggle")) {
        if (hud_id<0) {
            bind_needed = 1;
        }
        else {
            hud_unbind(reply, cq);
            bind_needed = 0;
        }
    }

    else if (!strcmp(words[1],"test")) {
        hud_mode = HUDMODE_TEST;
    }

    else if (!strcmp(words[1],"nav")) {
        hud_mode = HUDMODE_NAV;
    }

    else if (!strcmp(words[1],"tunnel") || !strcmp(words[1],"tun")) {
        hud_mode = HUDMODE_TUNNEL;
    }

    else {
        bind_needed = 0;
        goto Error;
    }

    if (bind_needed && hud_id<0) {
        int id = hud_new(reply, cq);
        if (id < 0) goto Error;
        hud_bind(reply, id);
    }

    hud_inv = HUDINV_ANY;

 Error:
    if (reply[0]) chat_message(reply, cq, "green", rpos);
}

void hud_update(MCPacketQueue *cq) {
    hud_prune();
    if (hud_id < 0 || !hud_inv) return;

    draw_color = 34;
    draw_clear();

    int updated = 0;

    switch(hud_mode) {
        case HUDMODE_TEST:      updated = huddraw_test(); break;
        case HUDMODE_NAV:       updated = huddraw_nav(); break;
        case HUDMODE_TUNNEL:    updated = huddraw_tunnel(); break;
        default:                break;
    }

    if (updated) {
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
    }

    hud_inv = HUDINV_NONE;
}

void hud_invalidate(uint64_t flags) {
    hud_inv |= flags;
}
