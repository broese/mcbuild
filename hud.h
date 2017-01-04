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

#define HUDINV_NONE             0LL
#define HUDINV_ANY              (HUDINV_NONE-1)
#define HUDINV_POSITION         (1LL<<1)
#define HUDINV_HEALTH           (1LL<<2)
#define HUDINV_INVENTORY        (1LL<<3)
#define HUDINV_ENTITIES         (1LL<<4)
#define HUDINV_BLOCKS           (1LL<<5)
#define HUDINV_BUILD            (1LL<<6)
#define HUDINV_HELP             (1LL<<7)

int  hud_bogus_map(slot_t *s);
void hud_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq);
void hud_renew(MCPacketQueue *cq);
void hud_update(MCPacketQueue *cq);
void hud_invalidate(uint64_t flags);
