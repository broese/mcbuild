/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "mcp_packet.h"

typedef struct {
    bid_t   material;   // base material, i.e. meta reduced to material only
    int     total;      // number of such blocks in the buildtask/plan
    int     placed;     // number of blocks already placed
    int     available;  // number of blocks in the inventory
} build_info_material;

typedef struct {
    int     total;
    int     placed;
    int     available;
    lh_arr_declare(build_info_material,mat);
} build_info;

void build_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq);
void build_clear(MCPacketQueue *sq, MCPacketQueue *cq);
void build_cancel(MCPacketQueue *sq, MCPacketQueue *cq);
void build_pause();
void build_update();
void build_progress(MCPacketQueue *sq, MCPacketQueue *cq);
int  build_packet(MCPacket *pkt, MCPacketQueue *sq, MCPacketQueue *cq);
void build_preview_transmit(MCPacketQueue *cq);

void build_sload(const char *name, char *reply);
void build_dump_plan();
build_info * get_build_info(int plan);
void calculate_material(int plan);
int prefetch_material(MCPacketQueue *sq, MCPacketQueue *cq, bid_t mat);
int find_evictable_slot();
