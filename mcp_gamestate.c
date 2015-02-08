#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LH_DECLARE_SHORT_NAMES 1

#include <lh_debug.h>
#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_buffers.h>
#include <lh_compress.h>

#include "mcp_gamestate.h"

////////////////////////////////////////////////////////////////////////////////

gamestate gs;
static int gs_used = 0;

void gs_reset() {
    int i;

    if (gs_used)
        gs_destroy();

    CLEAR(gs);

#if 0
    // set all items in the inventory to -1 to define them as empty
    for(i=0; i<64; i++) {
        gs.inventory.slots[i].id = 0xffff;
    }
    gs.drag.id = 0xffff;
#endif

    gs_used = 1;
}

void gs_destroy() {
    // delete tracked entities
    lh_free(P(gs.entity));

#if 0
        // delete cached chunks
        for(i=0; i<gs.C(chunk); i++)
            if (gs.P(chunk)[i].c)
                free(gs.P(chunk)[i].c);
        free(gs.P(chunk));
#endif
}

int gs_setopt(int optid, int value) {
    switch (optid) {
        case GSOP_PRUNE_CHUNKS:
            gs.opt.prune_chunks = value;
            break;
        case GSOP_SEARCH_SPAWNERS:
            gs.opt.search_spawners = value;
            break;
        case GSOP_TRACK_ENTITIES:
            gs.opt.track_entities = value;
            break;

        default:
            LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }

    return 0;
}

int gs_getopt(int optid) {
    switch (optid) {
        case GSOP_PRUNE_CHUNKS:
            return gs.opt.prune_chunks;
        case GSOP_SEARCH_SPAWNERS:
            return gs.opt.search_spawners;
        case GSOP_TRACK_ENTITIES:
            return gs.opt.track_entities;

        default:
            LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }
}

////////////////////////////////////////////////////////////////////////////////

int gs_packet(MCPacket *pkt) {
    switch (pkt->type) {
    }
}
