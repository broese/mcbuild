/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

/*
  mcp_types : complex data types used in the protocol and internal data
*/

#include <stdint.h>

#include "nbt.h"

////////////////////////////////////////////////////////////////////////////////
// Useful macros

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define SGN(x)   (((x)>=0)?1:-1)
#define SQ(x)    ((x)*(x))

////////////////////////////////////////////////////////////////////////////////
// various constants

#define MCP_MAXSTR 4096
#define MCP_MAXPLEN (4*1024*1024)

////////////////////////////////////////////////////////////////////////////////
// protocol

typedef uint8_t uuid_t[16];
typedef int32_t fixp;
typedef uint8_t angle_t;

typedef struct {
    union {
        struct {
            int64_t z : 26;
            int64_t y : 12;
            int64_t x : 26;
        };
        uint64_t p;
    };
} pos_t;

#define POS(X,Y,Z) (pos_t){ { { .x=X, .y=Y, .z=Z } } }

////////////////////////////////////////////////////////////////////////////////
// coordinates

typedef struct {
    int32_t x,y,z;
} off3_t;               // block offset/coordinate

typedef struct {
    int32_t x,y,z;
} size3_t;              // cuboid size in blocks

typedef struct {
    off3_t  pos;
    int     dir;
} pivot_t;              // pivot position and orientation

typedef struct {
    off3_t  min;        // minimum coordinates (lower NW corner)
    off3_t  max;        // maximum coordinates (upper SE corner)
} extent_t;             // extent/cuboid in the 3D coordinates

// calculate an extent from a pivot and a size
extent_t ps2extent(pivot_t pv, size3_t sz);

////////////////////////////////////////////////////////////////////////////////
// Slots and inventory

typedef struct {
    int16_t item;
    int16_t count;  // actually int8_t, but we need to have a larger capacity to deal with crafting
    int16_t damage;
    nbt_t   *nbt;   // auxiliary data - enchantments etc.
} slot_t;

void dump_slot(slot_t *s);
void clear_slot(slot_t *s);
slot_t * clone_slot(slot_t *src, slot_t *dst);

////////////////////////////////////////////////////////////////////////////////
// Metadata

// single metadata key-value pair
typedef struct {
    union {
        struct {
            unsigned char key  : 5;
            unsigned char type : 3;
        };
        uint8_t h;
    };
    union {
        int8_t  b;
        int16_t s;
        int32_t i;
        float   f;
        char    str[MCP_MAXSTR];
        slot_t  slot;
        struct {
            int32_t x;
            int32_t y;
            int32_t z;
        };
        struct {
            float   pitch;
            float   yaw;
            float   roll;
        };
    };
} metadata;

metadata * clone_metadata(metadata *meta);
void free_metadata(metadata *meta);

////////////////////////////////////////////////////////////////////////////////
// Map data

typedef struct {
    union {
        struct {
            uint16_t meta : 4;
            uint16_t bid  : 12;
        };
        uint16_t raw;
    };
} bid_t;

// used for skylight and blocklight
typedef struct {
    union {
        struct {
            uint8_t l : 4;
            uint8_t h : 4;
        };
        uint8_t b;
    };
} light_t;

// represents a single 16x16x16 cube of blocks within a chunk
typedef struct {
    bid_t   blocks[4096];
    light_t skylight[2048];
    light_t light[2048];
} cube_t;

typedef struct {
    int32_t  X;
    int32_t  Z;
    uint16_t mask;
    cube_t   *cubes[16];    // pointers to cubes. The pointers may be NULL meaning air
    uint8_t  biome[256];
} chunk_t;

typedef struct {
    union {
        struct {
            uint8_t z : 4;
            uint8_t x : 4;
        };
        uint8_t pos;
    };
    uint8_t y;
    bid_t bid;
} blkrec;

typedef struct {
    int32_t dx,dy,dz;
} boff_t;

typedef struct {
    bid_t * data[256];  // array of horizontal slices of size sx*sz
    size3_t sr;    // size of the requested block
    size3_t sa;    // actual size of the data (aligned to chunk size)
    int32_t boff;  // block offset where the xmin-zmin corner of the requested block is located
                   // access to a block 0,0,dy : cuboid.data[y][cuboid.boff];
} cuboid_t;

void free_cuboid(cuboid_t c);
