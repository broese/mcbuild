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
#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Useful macros

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define SGN(x)   (((x)>=0)?1:-1)
#define SQ(x)    ((x)*(x))

////////////////////////////////////////////////////////////////////////////////
// various constants and helpers

#define MCP_MAXSTR 4096
#define MCP_MAXPLEN (4*1024*1024)

uint8_t * read_string(uint8_t *p, char *s);
uint8_t * write_string(uint8_t *w, const char *s);
const char * limhex(uint8_t *data, ssize_t len, ssize_t maxbyte);

////////////////////////////////////////////////////////////////////////////////
// Directions

#define DIR_ANY    -1
#define DIR_UP      0
#define DIR_DOWN    1
#define DIR_SOUTH   2
#define DIR_NORTH   3
#define DIR_EAST    4
#define DIR_WEST    5

extern const char ** DIRNAME;

static inline int ltodir(char c) {
    switch (c) {
        case 'n':
        case 'N': return DIR_NORTH;
        case 's':
        case 'S': return DIR_SOUTH;
        case 'w':
        case 'W': return DIR_WEST;
        case 'e':
        case 'E': return DIR_EAST;
        case 'u':
        case 'U': return DIR_UP;
        case 'd':
        case 'D': return DIR_DOWN;
    }
    return DIR_ANY;
}

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
    uint32_t mask;
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

