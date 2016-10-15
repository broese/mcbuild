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

#include <lh_buffers.h>
#include <lh_files.h>
#include <lh_debug.h>
#include <lh_bytes.h>

#include "mcp_ids.h"
#include "mcp_build.h"
#include "mcp_bplan.h"
#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_arg.h"

////////////////////////////////////////////////////////////////////////////////

#define CMD(name) if (!strcmp(cmd, #name))

int hud_id=-1;

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

            gs_packet(ss);
            queue_packet(ss,cq);
        }
    }

    if (gs.inv.drag.item == 358 && gs.inv.drag.damage == DEFAULT_MAP_ID) {
        NEWPACKET(SP_SetSlot, ss);
        tss->wid = 255;
        tss->sid = -1;
        clear_slot(&tss->slot);

        gs_packet(ss);
        queue_packet(ss,cq);
    }

    hud_id = -1;
    sprintf(reply, "Unbinding HUD");
}

////////////////////////////////////////////////////////////////////////////////

/*
 * hud new  # give player a new fake map (32767) and bind HUD to it
 * hud NNN  # bind HUD to a specific map ID
 */

void hud_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq) {
    char reply[32768];
    reply[0] = 0;
    int rpos = 0;

    int id;

    if (!words[1] || !strcmp(words[1],"new")) {
        id = hud_new(reply, cq);
        if (id < 0) goto Error;
        hud_bind(reply, id);
    }
    else if (sscanf(words[1], "%u", &id)==1) {
        hud_bind(reply, id);
    }
    else if (!strcmp(words[1],"-")) {
        hud_unbind(reply, cq);
    }

 Error:
    if (reply[0]) chat_message(reply, cq, "green", rpos);
}
