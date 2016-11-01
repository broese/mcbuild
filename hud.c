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

#define COLOR_TRANSPARENT       0
#define COLOR_GRASS_GREEN       1
#define COLOR_SAND_YELLOW       2
#define COLOR_COBWEB_GRAY       3
#define COLOR_REDSTONE_RED      4
#define COLOR_ICE_BLUE          5
#define COLOR_IRON_GRAY         6
#define COLOR_LEAF_GREEN        7
#define COLOR_WHITE             8
#define COLOR_CLAY_GRAY         9
#define COLOR_DIRT_BROWN        10
#define COLOR_STONE_GRAY        11
#define COLOR_WATER_BLUE        12
#define COLOR_OAK_BROWN         13
#define COLOR_QUARTZ_WHITE      14
#define COLOR_ORANGE            15
#define COLOR_MAGENTA           16
#define COLOR_LIGHT_BLUE        17
#define COLOR_YELLOW            18
#define COLOR_LIME              19
#define COLOR_PINK              20
#define COLOR_GRAY              21
#define COLOR_LIGHT_GRAY        22
#define COLOR_CYAN              23
#define COLOR_PURPLE            24
#define COLOR_BLUE              25
#define COLOR_BROWN             26
#define COLOR_GREEN             27
#define COLOR_RED               28
#define COLOR_BLACK             29
#define COLOR_GOLD_YELLOW       30
#define COLOR_DIAMOND_BLUE      31
#define COLOR_LAPIS_BLUE        32
#define COLOR_EMERALD_GREEN     33
#define COLOR_SPRUCE_BROWN      34
#define COLOR_NETHER_RED        35

#define B3(x)   ((x)*4+2)
#define B2(x)   ((x)*4+1)
#define B1(x)   ((x)*4+0)
#define B0(x)   ((x)*4+3)

#define SAMECOLOR(x) {x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x}
#define WOOLCOLOR    {8,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29}

static int8_t BLOCK_COLORMAP[256][16] = {
    [0] = SAMECOLOR(0),
    [1] = SAMECOLOR(11),
    [2] = SAMECOLOR(1),
    [3] = SAMECOLOR(1),
    [4] = SAMECOLOR(11),
    [5] = { 13, 34, 2, 10, 15, 26 }, // Woodplanks
    [6] = SAMECOLOR(7),
    [7] = SAMECOLOR(11),
    [8] = SAMECOLOR(12),
    [9] = SAMECOLOR(12),
    [10] = SAMECOLOR(4),
    [11] = SAMECOLOR(4),
    [12] = { 2, 10 },       // Sand
    [13] = SAMECOLOR(11),
    [14] = SAMECOLOR(11),
    [15] = SAMECOLOR(11),

    [16] = SAMECOLOR(11),
    [17] = { 13, 34, 2, 10, 13, 34, 2, 10, 13, 34, 2, 10, 13, 34, 2, 10 }, // Woodlogs
    [18] = SAMECOLOR(7),
    [19] = SAMECOLOR(18),   // Sponge
    [20] = SAMECOLOR(14),   // Glass
    [21] = SAMECOLOR(11),
    [22] = SAMECOLOR(32),
    [23] = SAMECOLOR(11),
    [24] = SAMECOLOR(2),
    [25] = SAMECOLOR(13),
    [26] = SAMECOLOR(3),
    [27] = SAMECOLOR(6),    // Powered rail
    [28] = SAMECOLOR(6),    // Detector rail
    [29] = SAMECOLOR(11),
    [30] = SAMECOLOR(3),    // Cobweb
    [31] = SAMECOLOR(7),

    [32] = SAMECOLOR(7),
    [33] = SAMECOLOR(11),
    [34] = SAMECOLOR(11),
    [35] = WOOLCOLOR,       // Wool
    [36] = SAMECOLOR(11),
    [37] = SAMECOLOR(7),
    [38] = SAMECOLOR(7),
    [39] = SAMECOLOR(7),
    [40] = SAMECOLOR(7),
    [41] = SAMECOLOR(30),
    [42] = SAMECOLOR(6),
    [43] = SAMECOLOR(11),
    [44] = SAMECOLOR(11),
    [45] = SAMECOLOR(28),
    [46] = SAMECOLOR(4),
    [47] = SAMECOLOR(13),

    [48] = SAMECOLOR(11),
    [49] = SAMECOLOR(29),
    [50] = SAMECOLOR(18),   // Torch
    [51] = SAMECOLOR(4),
    [52] = SAMECOLOR(11),
    [53] = SAMECOLOR(13),
    [54] = SAMECOLOR(11),
    [55] = { 35, 35, 35, 35, 28, 28, 28, 28, 4, 4, 4, 4, 4, 4, 4, 4 },  // Redstone wire
    [56] = SAMECOLOR(11),
    [57] = SAMECOLOR(31),
    [58] = SAMECOLOR(11),
    [59] = SAMECOLOR(7),
    [60] = SAMECOLOR(10),
    [61] = SAMECOLOR(11),
    [62] = SAMECOLOR(11),
    [63] = SAMECOLOR(13),

    [64] = SAMECOLOR(13),
    [65] = SAMECOLOR(13),
    [66] = SAMECOLOR(6),
    [67] = SAMECOLOR(11),
    [68] = SAMECOLOR(13),
    [69] = SAMECOLOR(11),
    [70] = SAMECOLOR(11),
    [71] = SAMECOLOR(6),
    [72] = SAMECOLOR(13),
    [73] = SAMECOLOR(11),
    [74] = SAMECOLOR(11),
    [75] = SAMECOLOR(35),
    [76] = SAMECOLOR(4),
    [77] = SAMECOLOR(11),
    [78] = SAMECOLOR(8),
    [79] = SAMECOLOR(5),

    [80] = SAMECOLOR(8),
    [81] = SAMECOLOR(7),
    [82] = SAMECOLOR(9),
    [83] = SAMECOLOR(7),
    [84] = SAMECOLOR(10),
    [85] = SAMECOLOR(13),
    [86] = SAMECOLOR(15),
    [87] = SAMECOLOR(35),
    [88] = SAMECOLOR(26),
    [89] = SAMECOLOR(2),
    [90] = SAMECOLOR(16),
    [91] = SAMECOLOR(15),
    [92] = SAMECOLOR(11),
    [93] = SAMECOLOR(35),
    [94] = SAMECOLOR(4),
    [95] = WOOLCOLOR,

    [96] = SAMECOLOR(13),
    [97] = SAMECOLOR(9),
    [98] = SAMECOLOR(11),
    [99] = SAMECOLOR(2),
    [100] = SAMECOLOR(2),
    [101] = SAMECOLOR(6),
    [102] = SAMECOLOR(11),
    [103] = SAMECOLOR(19),
    [104] = SAMECOLOR(7),
    [105] = SAMECOLOR(7),
    [106] = SAMECOLOR(7),
    [107] = SAMECOLOR(13),
    [108] = SAMECOLOR(28),
    [109] = SAMECOLOR(11),
    [110] = SAMECOLOR(11),
    [111] = SAMECOLOR(7),

    [112] = SAMECOLOR(35),
    [113] = SAMECOLOR(35),
    [114] = SAMECOLOR(35),
    [115] = SAMECOLOR(28),
    [116] = SAMECOLOR(28),
    [117] = SAMECOLOR(6),
    [118] = SAMECOLOR(11),
    [119] = SAMECOLOR(29),
    [120] = SAMECOLOR(27),
    [121] = SAMECOLOR(2),
    [122] = SAMECOLOR(29),
    [123] = SAMECOLOR(10),
    [124] = SAMECOLOR(18),
    [125] = SAMECOLOR(13),
    [126] = SAMECOLOR(13),
    [127] = SAMECOLOR(10),

    [128] = SAMECOLOR(2),
    [129] = SAMECOLOR(11),
    [130] = SAMECOLOR(11),
    [131] = SAMECOLOR(11),
    [132] = SAMECOLOR(11),
    [133] = SAMECOLOR(33),
    [134] = SAMECOLOR(34),
    [135] = SAMECOLOR(2),
    [136] = SAMECOLOR(10),
    [137] = SAMECOLOR(26),
    [138] = SAMECOLOR(31),
    [139] = SAMECOLOR(11),
    [140] = SAMECOLOR(10),
    [141] = SAMECOLOR(7),
    [142] = SAMECOLOR(7),
    [143] = SAMECOLOR(13),

    [144] = SAMECOLOR(11),
    [145] = SAMECOLOR(6),
    [146] = SAMECOLOR(11),
    [147] = SAMECOLOR(30),
    [148] = SAMECOLOR(6),
    [149] = SAMECOLOR(35),
    [150] = SAMECOLOR(4),
    [151] = SAMECOLOR(13),
    [152] = SAMECOLOR(4),
    [153] = SAMECOLOR(35),
    [154] = SAMECOLOR(6),
    [155] = SAMECOLOR(14),
    [156] = SAMECOLOR(14),
    [157] = SAMECOLOR(6),
    [158] = SAMECOLOR(11),
    [159] = WOOLCOLOR,

    [160] = WOOLCOLOR,
    [161] = SAMECOLOR(7),
    [162] = { 15, 26, 0, 0, 15, 26, 0, 0, 15, 26, 0, 0, 15, 26, 0, 0 },
    [163] = SAMECOLOR(15),
    [164] = SAMECOLOR(26),
    [165] = SAMECOLOR(1),
    [166] = SAMECOLOR(11),
    [167] = SAMECOLOR(6),
    [168] = SAMECOLOR(23),
    [169] = SAMECOLOR(14),
    [170] = SAMECOLOR(18),
    [171] = WOOLCOLOR,
    [172] = SAMECOLOR(15),
    [173] = SAMECOLOR(29),
    [174] = SAMECOLOR(11),
    [175] = SAMECOLOR(7),

    [176] = SAMECOLOR(13),
    [177] = SAMECOLOR(13),
    [178] = SAMECOLOR(13),
    [179] = SAMECOLOR(15),
    [180] = SAMECOLOR(15),
    [181] = SAMECOLOR(15),
    [182] = SAMECOLOR(15),
    [183] = SAMECOLOR(34),
    [184] = SAMECOLOR(2),
    [185] = SAMECOLOR(10),
    [186] = SAMECOLOR(26),
    [187] = SAMECOLOR(15),
    [188] = SAMECOLOR(34),
    [189] = SAMECOLOR(2),
    [190] = SAMECOLOR(10),
    [191] = SAMECOLOR(26),

    [192] = SAMECOLOR(15),
    [193] = SAMECOLOR(34),
    [194] = SAMECOLOR(2),
    [195] = SAMECOLOR(10),
    [196] = SAMECOLOR(26),
    [197] = SAMECOLOR(15),
    [198] = SAMECOLOR(20),
    [199] = SAMECOLOR(24),
    [200] = SAMECOLOR(24),
    [201] = SAMECOLOR(24),
    [202] = SAMECOLOR(24),
    [203] = SAMECOLOR(24),
    [204] = SAMECOLOR(24),
    [205] = SAMECOLOR(24),
    [206] = SAMECOLOR(2),
    [207] = SAMECOLOR(7),

    [208] = SAMECOLOR(10),
    [209] = SAMECOLOR(29),
    [210] = SAMECOLOR(24),
    [211] = SAMECOLOR(27),
    [212] = SAMECOLOR(5),
    [213] = SAMECOLOR(15),
    [214] = SAMECOLOR(28),
    [215] = SAMECOLOR(35),
    [216] = SAMECOLOR(3),
    [217] = SAMECOLOR(11),
    [218] = SAMECOLOR(11),
    [219] = SAMECOLOR(24), // Shulker boxes
    [220] = SAMECOLOR(24),
    [221] = SAMECOLOR(24),
    [222] = SAMECOLOR(24),
    [223] = SAMECOLOR(24),

    [224] = SAMECOLOR(24),
    [225] = SAMECOLOR(24),
    [226] = SAMECOLOR(24),
    [227] = SAMECOLOR(24),
    [228] = SAMECOLOR(24),
    [229] = SAMECOLOR(24),
    [230] = SAMECOLOR(24),
    [231] = SAMECOLOR(24),
    [232] = SAMECOLOR(24),
    [233] = SAMECOLOR(24),
    [234] = SAMECOLOR(24),
    [255] = SAMECOLOR(22),
};

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

    bg_color = B3(COLOR_WHITE);
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

#define FONTS_DIAL      0,36
#define FONTS_DIAL_SZ   31

void huddraw_compass(int center_c, int center_r, int color_dial, int color_needle) {
    int old_color = fg_color;

    fg_color = color_dial;
    draw_blit(fonts, FONTS_DIAL, FONTS_DIAL_SZ, FONTS_DIAL_SZ,
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

void huddraw_info_nav(int r) {
    char text[256];

    fg_color = B0(COLOR_BLACK);
    bg_color = B3(COLOR_SAND_YELLOW);
    draw_rect(0,r,128,35,1);

    fg_color = B2(COLOR_SAND_YELLOW);
    draw_rect(35,r+2,12,14,0);
    draw_rect(76,r+2,12,14,0);

    // coordinates
    fg_color = B3(COLOR_REDSTONE_RED);
    bg_color = COLOR_TRANSPARENT;

    int32_t x = (int32_t)floor(gs.own.x);
    int32_t z = (int32_t)floor(gs.own.z);
    int32_t x_= (gs.world==&gs.nether) ? x*8 : x/8;
    int32_t z_= (gs.world==&gs.nether) ? z*8 : z/8;
    char *  n_= (gs.world==&gs.nether) ? "Overworld" : "Nether";

    draw_text(3, r+ 3, "X");
    draw_text(3, r+10, "Z");
    draw_text(3, r+19, "Y");
    draw_text(3, r+26, "DIR");

    sprintf(text, "%9d", x);  draw_text(11,r+3,text);
    sprintf(text, "%9d", x_); draw_text(53,r+3,text);
    sprintf(text, "%9d", z);  draw_text(11,r+10,text);
    sprintf(text, "%9d", z_); draw_text(53,r+10,text);
    sprintf(text, "%9d", (int32_t)floor(gs.own.y)); draw_text(11,r+19,text);

    char * dir = "UNKNOWN";
    switch(player_direction()) {
        case DIR_NORTH : dir = "NORTH"; break;
        case DIR_SOUTH : dir = "SOUTH"; break;
        case DIR_EAST  : dir = "EAST"; break;
        case DIR_WEST  : dir = "WEST"; break;
    }
    sprintf(text, "%9s", dir); draw_text(11,r+26,text);

    int pos = (42-(strlen(n_)*4-1))/2+49;
    draw_text(pos, r+26, n_);

    // compass
    huddraw_compass(108,r+17,B0(COLOR_BLACK), B3(COLOR_REDSTONE_RED));
}

#define FONTS_ICON_HEART  32,36
#define FONTS_ICON_FOOD   40,36
#define FONTS_ICON_SAT    48,36

void huddraw_info_health(int r) {
    char text[256];

    fg_color = B0(COLOR_BLACK);
    bg_color = B3(COLOR_GRASS_GREEN);
    draw_rect(0,r,128,12,1);

    bg_color = COLOR_TRANSPARENT;
    fg_color = B3(COLOR_REDSTONE_RED);
    draw_blit(fonts, FONTS_ICON_HEART, 8, 8, 2, r+2);
    fg_color = B0(COLOR_BLACK);
    sprintf(text, "%.1f", gs.own.health);
    draw_text(12, r+3, text);

    fg_color = B2(COLOR_ORANGE);
    draw_blit(fonts, FONTS_ICON_FOOD, 8, 8, 33, r+2);
    fg_color = B0(COLOR_BLACK);
    sprintf(text, "%d (%.1f)", gs.own.food, gs.own.saturation);
    draw_text(43, r+3, text);
}

#define FONTS_ICON_EQ_C 32
#define FONTS_ICON_EQ_R 44

void huddraw_info_inv_item(int id, float damage, int r) {
    int col = 4+31*(id%4);
    int row = r+2+10*(id/4);

    int fcol = FONTS_ICON_EQ_C+8*(id%4);
    int frow = FONTS_ICON_EQ_R+8*(id/4);

    bg_color = COLOR_TRANSPARENT;
    fg_color = (damage<0) ? B0(COLOR_CLAY_GRAY) : B3(COLOR_DIAMOND_BLUE);
    draw_blit(fonts, fcol, frow, 8, 8, col, row);

    fg_color = B2(COLOR_CLAY_GRAY);
    draw_rect(col+10, row+1, 16, 6, 1);

    if (damage >= 0) {
        fg_color = B3(COLOR_LAPIS_BLUE);
        if (damage < 1.0)  fg_color = B3(COLOR_EMERALD_GREEN);
        if (damage < 0.5)  fg_color = B3(COLOR_GOLD_YELLOW);
        if (damage < 0.25) fg_color = B3(COLOR_ORANGE);
        if (damage < 0.1)  fg_color = B3(COLOR_REDSTONE_RED);

        draw_rect(col+10, row+1, 16*damage, 6, 0);
    }
}

typedef struct {
    int pos;
    int iid;
    int ssid;
    int lsid;
    int dur;
} hudinvitem;

hudinvitem hii[] = {
    { 0, 298,  5,  5,  56 }, // Leather Helmet
    { 0, 314,  5,  5,  78 }, // Gold Helmet
    { 0, 302,  5,  5, 166 }, // Chainmail Helmet
    { 0, 306,  5,  5, 166 }, // Iron Helmet
    { 0, 310,  5,  5, 364 }, // Diamond Helmet

    { 1, 299,  6,  6,  81 }, // Leather Chestplate
    { 1, 315,  6,  6, 113 }, // Gold Chestplate
    { 1, 303,  6,  6, 241 }, // Chainmail Chestplate
    { 1, 307,  6,  6, 241 }, // Iron Chestplate
    { 1, 311,  6,  6, 529 }, // Diamond Chestplate

    { 2, 300,  7,  7,  76 }, // Leather Leggings
    { 2, 316,  7,  7, 106 }, // Gold Leggings
    { 2, 304,  7,  7, 226 }, // Chainmail Leggings
    { 2, 308,  7,  7, 226 }, // Iron Leggings
    { 2, 312,  7,  7, 496 }, // Diamond Leggings

    { 3, 301,  8,  8,  66 }, // Leather Boots
    { 3, 317,  8,  8,  92 }, // Gold Boots
    { 3, 305,  8,  8, 196 }, // Chainmail Boots
    { 3, 309,  8,  8, 196 }, // Iron Boots
    { 3, 313,  8,  8, 430 }, // Diamond Boots

    { 4, 442, 36, 45, 337 }, // Shield

    { 5, 267, 36, 45,  60 }, // Wooden Sword
    { 5, 268, 36, 45, 251 }, // Iron Sword
    { 5, 272, 36, 45, 132 }, // Stone Sword
    { 5, 283, 36, 45,  33 }, // Gold Sword
    { 5, 276, 36, 45,1562 }, // Diamond Sword

    { 6, 270, 36, 45,  60 }, // Wooden Pickaxe
    { 6, 257, 36, 45, 251 }, // Iron Pickaxe
    { 6, 274, 36, 45, 132 }, // Stone Pickaxe
    { 6, 285, 36, 45,  33 }, // Gold Pickaxe
    { 6, 278, 36, 45,1562 }, // Diamond Pickaxe

    { 7, 269, 36, 45,  60 }, // Wooden Shovel
    { 7, 256, 36, 45, 251 }, // Iron Shovel
    { 7, 273, 36, 45, 132 }, // Stone Shovel
    { 7, 284, 36, 45,  33 }, // Gold Shovel
    { 7, 277, 36, 45,1562 }, // Diamond Shovel

    { -1, 0, 0, 0, 0 },
};

void huddraw_info_inv(int r) {
    bg_color = B3(COLOR_PINK);
    fg_color = B0(COLOR_BLACK);
    draw_rect(0,r,128,22,1);

    int present[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    int i,j;
    for(j=0; hii[j].pos>=0; j++) {
        hudinvitem *h = hii+j;
        for(i=h->ssid; i<=h->lsid; i++) {
            slot_t *s = gs.inv.slots+i;
            if (s->item == h->iid) {
                float damage = ((float)h->dur-(float)s->damage)/(float)h->dur;
                huddraw_info_inv_item(h->pos, damage, r);
                present[h->pos] = 1;
            }
        }
    }

    for(i=0; i<8; i++)
        if (!present[i])
            huddraw_info_inv_item(i, -1, r);
}

int huddraw_info() {
    if (!(hud_inv & HUDINVMASK_INFO)) return 0;

    huddraw_info_nav(0);
    huddraw_info_health(34);
    huddraw_info_inv(45);
    // server

    return 1;
}

int huddraw_map() {
    int shading[6] = { 3, 0, 0, 1, 2, 2 };

    if (!(hud_inv & HUDINVMASK_TUNNEL)) return 0;

    bg_color = B0(COLOR_BLACK);
    draw_clear();

    int32_t x = (int32_t)floor(gs.own.x);
    int32_t y = (int32_t)floor(gs.own.y);
    int32_t z = (int32_t)floor(gs.own.z);
    extent_t ex = { { x-80, y-4, z-80 }, { x+80, y+1, z+80 } };
    cuboid_t cb = export_cuboid_extent(ex);

    int r,c,i,j;
    int32_t off = cb.boff + 16*cb.sa.x + 16;
    for(r=0; r<128; r++) {
        for(c=0; c<128; c++) {
            int poff = off+r*cb.sa.x+c;
            for(j=0; j<6; j++) {
                if ( cb.data[j][poff].bid ) {
                    int8_t color = BLOCK_COLORMAP[cb.data[j][poff].bid][cb.data[j][poff].meta];
                    hud_image[r*128+c] = color*4 + shading[j];
                }
            }
        }
    }

    for(i=0; i<256; i++) lh_free(cb.data[i]);

    hud_image[64*128+64] = 126;

    char text[256];
    bg_color = B3(COLOR_WHITE);
    fg_color = B3(COLOR_REDSTONE_RED);
    sprintf(text, "X:%6d", x);
    draw_text(2, 2, text);
    sprintf(text, "Z:%6d", z);
    draw_text(2, 9, text);

    return 1;
}

int huddraw_tunnel() {
    if (!(hud_inv & HUDINVMASK_TUNNEL)) return 0;

    bg_color = B1(COLOR_NETHER_RED);
    draw_clear();
    fg_color = B3(COLOR_GOLD_YELLOW);

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
            if (s>10) hud_image[r*128+c] = B3(COLOR_RED);
            if (s>30) hud_image[r*128+c] = B3(COLOR_ORANGE);
            if (s>60) hud_image[r*128+c] = B3(COLOR_YELLOW);
            if (s>80) hud_image[r*128+c] = B3(COLOR_WHITE);
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

    else if (!strcmp(words[1],"map")) {
        hud_mode = HUDMODE_MAP;
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
        case HUDMODE_MAP:       updated = huddraw_map(); break;
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
