#pragma once

#include <stdint.h>

#include <lh_arr.h>

#include "mcp_packet.h"

// this structure defines a relative block placement in a buildplan
typedef struct {
    int32_t     x,y,z;  // coordinates of the block to place (relative to pivot)
    bid_t       b;      // block type, including the meta
                        // positional meta is north-oriented
} blkr;

typedef struct {
    lh_arr_declare(blkr,plan); // currently loaded/created buildplan

    int32_t maxx,maxy,maxz;    // max buildplan coordinate in each dimension
    int32_t minx,miny,minz;    // min buildplan coordinate in each dimension
    int32_t sx,sy,sz;          // buildplan size in each dimension
} bplan;

////////////////////////////////////////////////////////////////////////////////
// Management

// free a buildplan object
void bplan_free(bplan * bp);

// recalculate buildplan extents - called when the buildplan was modified
void bplan_update(bplan * bp);

// dump the contents of the buildplan
void bplan_dump(bplan *bp);

////////////////////////////////////////////////////////////////////////////////
// Parametric builds

bplan * bplan_floor(int32_t wd, int32_t dp, bid_t mat);
bplan * bplan_wall(int32_t wd, int32_t hg, bid_t mat);
bplan * bplan_disk(int32_t diam, bid_t mat);
bplan * bplan_ball(int32_t diam, bid_t mat);
