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

// add a block to the buildplan
int bplan_add(bplan *bp, blkr block);

////////////////////////////////////////////////////////////////////////////////
// Helpers

blkr abs2rel(pivot_t pv, blkr b);
blkr rel2abs(pivot_t pv, blkr b);

////////////////////////////////////////////////////////////////////////////////
// Parametric builds

bplan * bplan_floor(int32_t wd, int32_t dp, bid_t mat);
bplan * bplan_wall(int32_t wd, int32_t hg, bid_t mat);
bplan * bplan_disk(int32_t diam, bid_t mat);
bplan * bplan_ball(int32_t diam, bid_t mat);
bplan * bplan_scaffolding(int wd, int hg, bid_t mat, int ladder);
bplan * bplan_stairs(int32_t wd, int32_t hg, bid_t mat, int base);

////////////////////////////////////////////////////////////////////////////////
// Buildplan manipulations

#define TRIM_UNK  -1
#define TRIM_END  0

#define TRIM_XE   1
#define TRIM_XL   2
#define TRIM_XG   3

#define TRIM_YE   4
#define TRIM_YL   5
#define TRIM_YG   6

#define TRIM_ZE   7
#define TRIM_ZL   8
#define TRIM_ZG   9

int  bplan_hollow(bplan *bp, int flat);
void bplan_extend(bplan *bp, int ox, int oz, int oy, int count);
int  bplan_replace(bplan *bp, bid_t mat1, bid_t mat2);
int  bplan_trim(bplan *bp, int type, int32_t value);
void bplan_flip(bplan *bp, char mode);
void bplan_tilt(bplan *bp, char mode);
void bplan_normalize(bplan *bp);
void bplan_shrink(bplan *bp);

////////////////////////////////////////////////////////////////////////////////
// Save/load/import

int bplan_save(bplan *bp, const char *name);
bplan * bplan_load(const char *name);
int bplan_ssave(bplan *bp, const char *name);
bplan * bplan_sload(const char *name);
