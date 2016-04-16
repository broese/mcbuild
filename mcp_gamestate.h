/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "mcp_packet.h"
#include "mcp_ids.h"
#include "mcp_arg.h"
#include "mcp_types.h"

////////////////////////////////////////////////////////////////////////////////

#define GSOP_PRUNE_CHUNKS       1
#define GSOP_SEARCH_SPAWNERS    2
#define GSOP_TRACK_ENTITIES     3
#define GSOP_TRACK_INVENTORY    4

////////////////////////////////////////////////////////////////////////////////
// entity tracking

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
    double   x,y,z;     // position
    int      type;      // one of the ENTITY_ variables
    EntityType mtype;   // mob/object type as used natively
    int      hostile;   // whether marked hostile
    uint64_t lasthit;   // timestamp when this entity was last attacked - for limiting the attack rate
    char     name[256]; // only valid for players
    metadata *mdata;    // entity metadata
} entity;

////////////////////////////////////////////////////////////////////////////////
// player list

typedef struct {
    uuid_t      uuid;
    char       *name;
    char       *dispname;
} pli;

////////////////////////////////////////////////////////////////////////////////
// chunk storage

typedef struct {
    bid_t blocks[65536];
    light_t light[32768];
    light_t skylight[32768];
    uint8_t  biome[256];
} gschunk;

// chunk coord -> offset within region (1x1 regions, 32x32 chunks, 512x512 blocks)
#define CC_0(X,Z)   (uint32_t)((((uint64_t)(X))&0x1f)|((((uint64_t)(Z))&0x1f)<<5))

// chunk coord -> region offset within a super-region (256x256 regions, 8kx8k chunks, 128kx128k blocks)
#define CC_1(X,Z)   (uint32_t)(((((uint64_t)(X))>>5)&0xff)|(((((uint64_t)(Z))>>5)&0xff)<<8))

// chunk coord -> super-region offset within world (512x512 superregions, 128kx128k regions, 4Mx4M chunks, 64Mx64M blocks)
#define CC_2(X,Z)   (uint32_t)(((((uint64_t)(X))>>13)&0x1ff)|(((((uint64_t)(Z))>>13)&0x1ff)<<9))

// offsets in world, super-region, region -> chunk coord
#define SIGNEXT(X) ((X)|(((X)&0x200000)?0xffc00000:0))
#define CC_X(S,R,C) SIGNEXT(  (((S)&0x1ff)<<13)  | (((R)&0xff)<<5)   | ((C)&0x1f)        )
#define CC_Z(S,R,C) SIGNEXT(  (((S)&0x3fe00)<<4) | (((R)&0xff00)>>3) | (((C)&0x3e0)>>5)  )

typedef struct {
    gschunk *chunk[32*32];
} gsregion;

typedef struct {
    gsregion *region[256*256];
} gssreg;

typedef struct {
    gssreg *sreg[512*512];
} gsworld;

////////////////////////////////////////////////////////////////////////////////

typedef struct _gamestate {
    // game
    int64_t time; // timestamp from the last received server tick

    // options
    struct {
        int prune_chunks;
        int search_spawners;
        int track_entities;
        int track_inventory;
    } opt;

    struct {
        uint32_t    eid;
        double      x,y,z;
        uint8_t     onground;
        float       yaw,pitch;

        int         crouched;

        // position change tracking for functions like holeradar
        int32_t     lx,ly,lz,lo;
        int         pos_change;

        float       health;
        int32_t     food;
    } own;

    struct {
        uint8_t     held;
        slot_t      slots[64];
        slot_t      drag;

        // set when the inventory tracking fails to keep the inventory consistent
        int         inconsistent;

        int16_t     pslots[256];
        int         pcount;
        int         ptype;

        int         windowopen;         // nonzero if the client has an open window
        int         wid;                // ID of the currently opened window
        int         woffset;            // main inventory starts in the window from this offset
    } inv;

    // tracked entities
    lh_arr_declare(entity, entity);

    lh_arr_declare(pli, players);

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

void gs_packet(MCPacket *pkt);

void dump_entities();
void dump_inventory();

gschunk * find_chunk(gsworld *w, int32_t X, int32_t Z, int allocate);
cuboid_t export_cuboid_extent(extent_t ex);
bid_t get_block_at(int32_t x, int32_t z, int32_t y);

int player_direction();
int sameitem(slot_t *a, slot_t *b);
