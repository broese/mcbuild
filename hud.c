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
#define HUDMODE_INFO            1
#define HUDMODE_TUNNEL          2
#define HUDMODE_MAP             3

#define DEFAULT_MAP_ID 32767

int hud_mode        = HUDMODE_TEST;
uint64_t hud_inv    = HUDINV_NONE;
int hud_autoid      = DEFAULT_MAP_ID;
int hud_id          = -1;

uint8_t hud_image[16384];

// TODO: color constants
uint8_t fg_color    = 119; // Black
uint8_t bg_color    = 0;   // Transparent

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
    memset(hud_image, bg_color, sizeof(hud_image));
}

void draw_blit(lhimage *img, int ic, int ir, int wd, int hg, int col, int row) {
    uint32_t *bm  = &IMGDOT(img, ic, ir);
    uint8_t  *hud = hud_image+col+row*128;

    int r,c;
    for(r=0; r<hg; r++) {
        uint32_t *bmr  = bm+r*img->stride;
        uint8_t  *hudr = hud+r*128;
        for(c=0; c<wd; c++) {
            if ((bmr[c]&0xffffff) == 0xffffff) {
                hudr[c] = fg_color;
            }
            else if (bg_color != 0) {
                hudr[c] = bg_color;
            }
        }
    }
}

void draw_line(int c1, int r1, int c2, int r2) {
    uint8_t  *hud = hud_image+c1+r1*128;

    int dc=(c2-c1), dr=(r2-r1);
    int cs=SGN(dc), rs=SGN(dr)*128;

    int h = abs(dc) > abs(dr); // line is more horizontal
    int ml = h ? abs(dc) : abs(dr);
    int ol = h ? abs(dr) : abs(dc);
    int ms = h ? cs : rs;
    int os = h ? rs : cs;

    int state = ml/2;
    int pl = ml;
    while (pl>=0) {
        hud[0] = fg_color;
        hud+=ms;
        state -= ol;
        if (state < 0) {
            state += ml;
            hud+=os;
        }
        pl --;
    }
}

void draw_glyph(int col, int row, char l) {
    if (l<0x20 || l>0x7f) return;
    draw_blit(fonts, (l&15)*font_w, ((l>>4)-2)*font_h+font_o, font_w, font_h, col, row);
}

void draw_text(int col, int row, char *s) {
    int i;
    for(i=0; s[i]; i++)
        draw_glyph(col+i*FONTS_W, row, s[i]);
}

void draw_rect(int col, int row, int wd, int hg, int hollow) {
    uint8_t  *hud   = hud_image+col+row*128;
    int c,r;
    for(r=0; r<hg && r<(128-row); r++) {
        uint8_t  *hudr = hud+r*128;
        for(c=0; c<wd && c<(128-col); c++) {
            int inside = hollow && (c>0 && c<wd-1) && (r>0 && r<hg-1);
            if (inside && bg_color>0)
                hudr[c] = bg_color;
            else
                hudr[c] = fg_color;
        }
    }
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
#define HUDINVMASK_INFO     (HUDINV_POSITION|HUDINV_HEALTH|HUDINV_INVENTORY)
#define HUDINVMASK_MAP      (HUDINV_POSITION|HUDINV_BLOCKS|HUDINV_ENTITIES)
#define HUDINVMASK_BUILD    (HUDINV_INVENTORY|HUDINV_BUILD)

int huddraw_test() {
    int i;

    bg_color = 34; // white
    draw_clear();

    fg_color = 119;
    for (i=0; i<36; i++) {
        int c=(i%6)*10+2;
        int r=(i/6)*10+2;
        bg_color = i*4+2;
        draw_rect(c, r, 8, 8, 1);
    }

    int x1=95, y1=95, r=29;
    double a;
    for (a=0; a<2*M_PI; a+=M_PI/6) {
        int x2 = x1+(int)(sin(a)*r);
        int y2 = y1+(int)(cos(a)*r);
        draw_line(x1,y1,x2,y2);
    }

    return 1;
}

#define FONTS_DIAL_C    0
#define FONTS_DIAL_R    36
#define FONTS_DIAL_SZ   31

void huddraw_compass(int center_c, int center_r, int color_dial, int color_needle) {
    int old_color = fg_color;

    fg_color = color_dial;
    draw_blit(fonts, FONTS_DIAL_C, FONTS_DIAL_R, FONTS_DIAL_SZ, FONTS_DIAL_SZ,
        center_c-FONTS_DIAL_SZ/2, center_r-FONTS_DIAL_SZ/2);

    fg_color = color_needle;
    double yaw = gs.own.yaw/180*M_PI;
    int x1 = -sin(yaw)*13;
    int y1 = cos(yaw)*13;
    int x2 = -sin(yaw-M_PI/2)*1.5;
    int y2 = cos(yaw-M_PI/2)*1.5;
    int x3 = -sin(yaw+M_PI/2)*1.5;
    int y3 = cos(yaw+M_PI/2)*1.5;
    draw_line(center_c, center_r, center_c+x1, center_r+y1);
    draw_line(center_c+x2, center_r+y2, center_c+x1, center_r+y1);
    draw_line(center_c+x3, center_r+y3, center_c+x1, center_r+y1);

    fg_color = old_color;
}

int huddraw_info() {
    if (!(hud_inv & HUDINVMASK_INFO)) return 0;

    char text[256];

    // draw section rectangles

    fg_color = 119; // black
    bg_color = 10;  // sandstone yellow
    draw_rect(0,0,128,35,1);
    draw_rect(0,0,9,35,1);
    draw_rect(8,0,42,35,1);
    draw_rect(49,0,42,35,1);

    bg_color = 6;   // grass green
    draw_rect(0,34,128,30,1);
    bg_color = 70;  // light blue
    draw_rect(0,63,128,33,1);
    bg_color = 62;  // orange
    draw_rect(0,95,128,33,1);

    fg_color = 9;   // medium sandstone yellow
    draw_rect(35,2,12,14,0);
    draw_rect(76,2,12,14,0);

    // coordinates

    fg_color = 18;  // redstone red
    bg_color = 0;   // transparent

    int32_t x = (int32_t)floor(gs.own.x);
    int32_t z = (int32_t)floor(gs.own.z);
    int32_t x_= (gs.world==&gs.nether) ? x*8 : x/8;
    int32_t z_= (gs.world==&gs.nether) ? z*8 : z/8;
    char *  n_= (gs.world==&gs.nether) ? "Overworld" : "Nether";

    draw_text(3,  3, "X");
    draw_text(3, 10, "Z");
    draw_text(3, 19, "Y");
    draw_text(3, 26, "D");

    sprintf(text, "%9d", x);  draw_text(11,3,text);
    sprintf(text, "%9d", x_); draw_text(53,3,text);
    sprintf(text, "%9d", z);  draw_text(11,10,text);
    sprintf(text, "%9d", z_); draw_text(53,10,text);
    sprintf(text, "%9d", (int32_t)floor(gs.own.y)); draw_text(11,19,text);

    char * dir = "UNKNOWN";
    switch(player_direction()) {
        case DIR_NORTH : dir = "NORTH"; break;
        case DIR_SOUTH : dir = "SOUTH"; break;
        case DIR_EAST  : dir = "EAST"; break;
        case DIR_WEST  : dir = "WEST"; break;
    }
    sprintf(text, "%9s", dir); draw_text(11,26,text);

    int pos = (42-(strlen(n_)*4-1))/2+49;
    draw_text(pos, 23, n_);

    // compass
    huddraw_compass(108,17,119,18);

    // health

    // inventory

    // server

    return 1;
}

int huddraw_tunnel() {
    if (!(hud_inv & HUDINVMASK_TUNNEL)) return 0;

    bg_color = 140; // neterrack dark red
    draw_clear();
    fg_color = 122; // gold yellow

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
 * hud              # toggle HUD
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

    else if (!strcmp(words[1],"info")) {
        hud_mode = HUDMODE_INFO;
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

    bg_color = 34;
    draw_clear();
    bg_color = 0;

    int updated = 0;

    switch(hud_mode) {
        case HUDMODE_TEST:      updated = huddraw_test(); break;
        case HUDMODE_INFO:      updated = huddraw_info(); break;
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
