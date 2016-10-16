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

#define DEFAULT_MAP_ID 32767

#define HUDMODE_NONE            0
#define HUDMODE_TEST            1
#define HUDMODE_NAV             2
#define HUDMODE_TUNNEL          3

void hud_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq);
void hud_update(MCPacketQueue *cq);
void hud_invalidate(int mode);
