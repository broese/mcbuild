#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "mcp_packet.h"
#include "mcp_ids.h"

////////////////////////////////////////////////////////////////////////////////

#define GSOP_PRUNE_CHUNKS       1
#define GSOP_SEARCH_SPAWNERS    2
#define GSOP_TRACK_ENTITIES     3

#define ENTITY_UNKNOWN  0
#define ENTITY_SELF     1
#define ENTITY_PLAYER   2
#define ENTITY_MOB      3
#define ENTITY_OBJECT   4
#define ENTITY_OTHER    5

static char * ENTITY_TYPES[] = {
    [ENTITY_UNKNOWN] = "Unknown",
    [ENTITY_SELF]    = "Self",
    [ENTITY_PLAYER]  = "Player",
    [ENTITY_MOB]     = "Mob",
    [ENTITY_OBJECT]  = "Object",
    [ENTITY_OTHER]   = "Other",
};

typedef struct _entity {
    int32_t  id;        // EID
    fixp     x,y,z;     // note: fixed-point coords, divide by 32
    int      type;      // one of the ENTITY_ variables
    int      mtype;     // mob/object type as used natively
    int      hostile;   // whether marked hostile
    uint64_t lasthit;   // timestamp when this entity was last attacked - for limiting the attack rate
    char     name[256]; // only valid for players
} entity;

////////////////////////////////////////////////////////////////////////////////

typedef struct _gamestate {
    // options
    struct {
        int prune_chunks;
        int search_spawners;
        int track_entities;
    } opt;

    // tracked entities
    lh_arr_declare(entity, entity);
} gamestate;

extern gamestate gs;

////////////////////////////////////////////////////////////////////////////////

void gs_reset();
void gs_destroy();
int  gs_setopt(int optid, int value);
int  gs_getopt(int optid);

int  gs_packet(MCPacket *pkt);

void dump_entities();
