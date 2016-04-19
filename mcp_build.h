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
void calculate_material(int plan);
int prefetch_material(MCPacketQueue *sq, MCPacketQueue *cq, bid_t mat);
int find_evictable_slot();
