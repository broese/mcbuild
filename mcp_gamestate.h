#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "mcp_packet.h"
#include "mcp_ids.h"

////////////////////////////////////////////////////////////////////////////////

#define GSOP_PRUNE_CHUNKS       1
#define GSOP_SEARCH_SPAWNERS    2
#define GSOP_TRACK_ENTITIES     3

typedef struct _gamestate {
    // options
    struct {
        int prune_chunks;
        int search_spawners;
        int track_entities;
    } opt;

} gamestate;

extern gamestate gs;

////////////////////////////////////////////////////////////////////////////////

void gs_reset();
int  gs_setopt(int optid, int value);
int  gs_getopt(int optid);

int  gs_packet(MCPacket *pkt);

