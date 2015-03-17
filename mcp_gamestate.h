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

typedef struct {
    bid_t blocks[65536];
    light_t light[32768];
    light_t skylight[32768];
    uint8_t  biome[256];
} gschunk;

#define CHUNKIDX(w,X,Z) ((X)-(w)->Xo)+((Z)-(w)->Zo)*((w)->Xs)

typedef struct {
    int32_t Xo, Zo;     // chunk coordinate offset
    int32_t Xs, Zs;     // size of the chunk storage
    gschunk **chunks;   // chunk storage
} gsworld;

typedef struct {
    int8_t   mode;
    int8_t   button;
    int16_t  sid;
    int16_t  aid;
} invact;

////////////////////////////////////////////////////////////////////////////////

#define DIR_UP      0
#define DIR_DOWN    1
#define DIR_SOUTH   2
#define DIR_NORTH   3
#define DIR_EAST    4
#define DIR_WEST    5

////////////////////////////////////////////////////////////////////////////////

typedef struct _gamestate {
    // game
    int64_t time; // timestamp from the last received server tick

    // options
    struct {
        int prune_chunks;
        int search_spawners;
        int track_entities;
    } opt;

    struct {
        uint32_t    eid;
        fixp        x,y,z;
        uint8_t     onground;
        float       yaw,pitch;

        // position change tracking for functions like holeradar
        int32_t     lx,ly,lz,lo;
        int         pos_change;
    } own;

    struct {
        uint8_t     held;
        slot_t      slots[64];
        slot_t      drag;

        lh_arr_declare(invact, iaq);    // window action queue

        // set when the inventory tracking fails to keep the inventory consistent
        int         inconsistent;

        int16_t     pslots[256];
        int         pcount;
        int         ptype;
    } inv;

    // tracked entities
    lh_arr_declare(entity, entity);

    gsworld         overworld;
    gsworld         end;
    gsworld         nether;
    gsworld        *world;
} gamestate;

extern gamestate gs;

////////////////////////////////////////////////////////////////////////////////

void gs_reset();
void gs_destroy();
int  gs_setopt(int optid, int value);
int  gs_getopt(int optid);

int  gs_packet(MCPacket *pkt);

void dump_entities();
void dump_inventory();

bid_t * export_cuboid(int32_t Xl, int32_t Xs, int32_t Zl, int32_t Zs,
                      int32_t yl, int32_t ys, bid_t *data);

int player_direction();
