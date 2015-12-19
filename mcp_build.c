#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include <lh_buffers.h>
#include <lh_files.h>
#include <lh_debug.h>
#include <lh_bytes.h>
#include <lh_compress.h>

#include "mcp_ids.h"
#include "mcp_build.h"
#include "mcp_bplan.h"
#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_arg.h"


#define EYEHEIGHT 52
#define YAWMARGIN 0.5

////////////////////////////////////////////////////////////////////////////////
// Helpers

// calculate the yaw and pitch values for the player looking at a specific
// dot in space
int calculate_yaw_pitch(fixp x, fixp z, fixp y, float *yaw, float *pitch) {
    // relative distance to the dot
    float dx = (float)(x-gs.own.x)/32.0;
    float dz = (float)(z-gs.own.z)/32.0;
    float dy = (float)(y-(gs.own.y+EYEHEIGHT))/32.0;
    float c  = sqrt(dx*dx+dz*dz);
    if (c==0) c=0.0001;

    float alpha = asinf(dx/c)/M_PI*180;
    *yaw = (dz<0) ? (180+alpha) : (360-alpha);
    if (*yaw >= 360.0) *yaw-=360; // normalize yaw

    *pitch = -atanf(dy/c)/M_PI*180;

    if (*yaw < 45-YAWMARGIN || *yaw > 315+YAWMARGIN) {
        return DIR_SOUTH;
    }
    else if (*yaw > 45+YAWMARGIN && *yaw < 135-YAWMARGIN) {
        return DIR_WEST;
    }
    else if (*yaw > 135+YAWMARGIN && *yaw < 225-YAWMARGIN) {
        return DIR_NORTH;
    }
    else if (*yaw > 225+YAWMARGIN && *yaw < 315-YAWMARGIN) {
        return DIR_EAST;
    }

    return DIR_ANY;
}






////////////////////////////////////////////////////////////////////////////////
// Structures

// offsets to the neighbor blocks
int32_t NOFF[6][3] = {
    //               x   z   y
    [DIR_UP]    = {  0,  0,  1 },
    [DIR_DOWN]  = {  0,  0, -1 },
    [DIR_SOUTH] = {  0,  1,  0 },
    [DIR_NORTH] = {  0, -1,  0 },
    [DIR_EAST]  = {  1,  0,  0 },
    [DIR_WEST]  = { -1,  0,  0 },
};

// mask - all dots on the face
uint16_t DOTS_ALL[15] = {
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff };

// all dots in the lower half
uint16_t DOTS_LOWER[15] = {
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0, 0,
    0, 0, 0, 0, 0, 0, 0 };

// all dots in the upper half
uint16_t DOTS_UPPER[15] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff };

// no dots on this face can be used
uint16_t DOTS_NONE[15] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0 };

// this structure is used to define an absolute block placement
// in the active building process - one block of the 'buildtask'
typedef struct {
    int32_t     x,y,z;          // coordinates of the block to place
    bid_t       b;              // block type, including the meta
    bid_t       current;        // block that is currently in the world at this position

    int         rdir;           // required placement direction
                                // one of the DIR_* constants, -1 if doesn't matter

    // state flags
    union {
        int8_t  state;
        struct {
            int8_t empty   : 1; // true if the block is free to place blocks into
                                // (contains air or some non-solid blocks)
            int8_t needadj : 1; // true if the block has correct placement, but need adjustment
            int8_t placed  : 1; // true if this block is already in place
            int8_t blocked : 1; // true if this block is obstructed by something else
            int8_t inreach : 1; // this block is close enough to place
            int8_t pending : 1; // block was placed but pending confirmation from the server
        };
    };

    // a bitmask of neighbors (6 bits only),
    // set bit means there is a neighbor in that direction
    union {
        int8_t  neigh;
        struct {
            int8_t  n_yp : 1;   // up    (y-pos)
            int8_t  n_yn : 1;   // down  (y-neg)
            int8_t  n_zp : 1;   // south (z-pos)
            int8_t  n_zn : 1;   // north (z-neg)
            int8_t  n_xp : 1;   // east  (x-pos)
            int8_t  n_xn : 1;   // west  (x-neg)
        };
    };

    bid_t nblocks[6];           // types of blocks at the neighbor positions

    uint16_t dots[6][15];       // usable dots on the 6 neighbor faces to place the block
    int ndots;                  // number of dots on this block we can use to place it correctly

    int32_t dist;               // distance to the block center (squared)

    uint64_t last;              // last timestamp when we attempted to place this block
} blk;

// maximum number of blocks in the buildable list
#define MAXBUILDABLE 1024

struct {
    int64_t lastbuild;         // timestamp of last block placement

    int active;                // if nonzero - buildtask is being built
    int recording;             // if nonzero - build recording active
    int placemode;             // 0-disabled, 1-once, 2-multiple
    int limit;                 // build height limit, disabled if zero

    lh_arr_declare(blk,task);  // current active building task
    bplan *bp;                 // currently loaded/created buildplan

    blkr brp[MAXBUILDABLE];    // records the 'pending' blocks from the build recorder
    ssize_t nbrp;              // we add blocks as we place them (CP_PlayerBlockPlacement)
                               // and remove them as we get the confirmation from the server
                               // (SP_BlockChange or SP_MultiBlockChange)

    pivot_t pv;                // pivot

    int bq[MAXBUILDABLE];      // list of buildable blocks from the task
    int nbq;                   // number of buildable blocks

    int32_t     xmin,xmax,ymin,ymax,zmin,zmax;

} build;

#define BTASK GAR(build.task)

// Options

struct {
    int init;
    int blkint;            // interval (in microseconds) between attempting to place same block
    int bldint;            // interval between placing any block
    int blkmax;            // maximum number of blocks to be places at once
    int placemode;         // automatically enter this placement mode once a buildplan is
                           // created, loaded or modified
    int wallmode;          // if nonzero - do not build blocks higher than your own position
    int sealmode;          // if nonzero - only build blocks behind your back
    int anyface;           // attempt to build on any faces, even those looking away from you
                           // this type of building will work on vanilla servers, but may be
                           // blocked on some, including 2b2t
    int bjump;             // build while jumping/flying/swimming - this is disabled by default
                           // but useful in some situations, e.g. when building under water
    int preview_retain;    // by default preview is automatically removed when the buildtask
                           // is canceled, otherwise phantom blocks will stay there until
                           // chunks are removed. This option overrides this behavior and
                           // retains the phantom blocks.
} buildopts = { 0 };

typedef struct {
    const char * name;
    const char * description;
    int * var;
    int defvalue;
} bopt_t;

#define BUILD_BLKINT 1000000
#define BUILD_BLDINT  150000
#define BUILD_BLKMAX       1

bopt_t OPTIONS[] = {
    { "blkint", "interval (us) between attempting to place same block", &buildopts.blkint, BUILD_BLKINT},
    { "bldint", "interval (us) between attempting to place any block",  &buildopts.bldint, BUILD_BLDINT},
    { "blkmax", "max number of blocks to place at once",                &buildopts.blkmax, BUILD_BLKMAX},
    { "placemode", "behavior to activate placement: 0:don't 1:once 2:many", &buildopts.placemode, 1},
    { "wm", "wall mode - limit placement to blocks beneath player",     &buildopts.wallmode, 0},
    { "sm", "seal mode - limit placement to blocks in front of player", &buildopts.sealmode, 0},
    { "anyface", "place on any faces even if they look away from player",   &buildopts.anyface, 0},
    { "bjump", "build while jumping/falling/swimming",                  &buildopts.bjump, 0},
    { "preview_retain", "Retain preview when buildtask is canceled",    &buildopts.preview_retain, 0},
    { NULL, NULL, NULL, 0 }, //list terminator
};

////////////////////////////////////////////////////////////////////////////////

static void build_update_placed();

////////////////////////////////////////////////////////////////////////////////
// Inventory

// slot range in the quickbar that can be used for material fetching
int matl=4, math=7;

// timestamps when the material slots were last accessed - used to select evictable slots
int64_t mat_last[9];

// find a suitable slot in the quickbar where we can swap in materials from the main inventory
int find_evictable_slot() {
    int i;
    int best_s = matl;
    int64_t best_ts = mat_last[matl];

    for(i=matl; i<=math; i++) {
        if (gs.inv.slots[i+36].item == -1)
            return i; // empty slot is the best candidate

        if (mat_last[i] < best_ts) {
            best_s = i;
            best_ts = mat_last[i];
        }
    }

    return best_s; // return slot that has not been used longest
}

// locate in which inventory slot do we have some specific material type
// quickbar is preferred, -1 if nothing found
static int find_material_slot(bid_t mat) {
    int i;
    for(i=44; i>8; i--)
        if (gs.inv.slots[i].item == mat.bid && gs.inv.slots[i].damage == mat.meta)
            return i;
    return -1;
}

// fetch necessary material to the quickbar
int prefetch_material(MCPacketQueue *sq, MCPacketQueue *cq, bid_t mat) {
    int mslot = find_material_slot(mat);

    if (mslot<0) return -1; // material not available in the inventory

    if (mslot>=36 && mslot<=44) return mslot-36; // found in thequickbar

    // fetch the material from main inventory to a suitable quickbar slot
    int eslot = find_evictable_slot();
    //printf("swapping slots: %d %d\n", mslot, eslot+36);
    gmi_swap_slots(sq, cq, mslot, eslot+36);

    return eslot;
}

// print a table of required materials for the buildplan (if plan=1) or buildtask
void calculate_material(int plan) {
    int i;

    if (plan) {
        printf("=== Material Demand for the Buildplan ===\n");
    }
    else {
        printf("=== Material Demand for the Buildtask ===\n");
        build_update_placed();
    }

    // buildplan/task count per material type
    int bc[65536];
    lh_clear_obj(bc);

    if (plan) {
        for (i=0; i<C(build.bp->plan); i++) {
            bid_t bmat = get_base_material(P(build.bp->plan)[i].b);
            bc[bmat.raw]++;
        }
    }
    else {
        for (i=0; i<C(build.task); i++) {
            if (P(build.task)[i].placed) continue;
            bid_t bmat = get_base_material(P(build.task)[i].b);
            bc[bmat.raw]++;
        }
    }

    // inventory count, per material type
    int ic[65536];
    lh_clear_obj(ic);

    for (i=9; i<45; i++) {
        if (gs.inv.slots[i].item<0 || gs.inv.slots[i].item>=0x100) continue;
        bid_t bmat = BLOCKTYPE(gs.inv.slots[i].item, gs.inv.slots[i].damage);
        ic[bmat.raw] += gs.inv.slots[i].count;
    }

    printf("BL/MT Name                             Count   Have   Need\n");
    // print material demand
    for(i=0; i<65536; i++) {
        if (bc[i] > 0) {
            bid_t mat;
            mat.raw = (uint16_t)i;

            char buf[256];
            printf("%02x/%02x %-32s %5d  %5d  ",mat.bid,mat.meta,get_bid_name(buf, mat),bc[i],ic[i]);

            int need = bc[i]-ic[i];
            if (need <= 0)
                printf("-\n");
            else {
                if (need >= STACKSIZE(mat.bid))
                    printf("%5.1f$\n", (float)need/STACKSIZE(mat.bid));
                else
                    printf("%3d\n",need);
            }
        }
    }

    printf("=========================================\n");
}




////////////////////////////////////////////////////////////////////////////////
// Building process

// maximum reach distance for building, squared, in fixp units (1/32 block)
#define MAXREACH_COARSE SQ(5<<5)
#define MAXREACH SQ(4<<5)
#define OFF(x,z,y) (((x)-xo)+((z)-zo)*(xsz)+((y)-yo)*(xsz*zsz))

typedef struct {
    fixp x,z,y;     // position of the dot 0,0
    fixp cx,cz,cy;  // deltas to the next dot in column
    fixp rx,rz,ry;  // deltas to the next dot in row
} dotpos_t;

static dotpos_t DOTPOS[6] = {
    //               start     cols    rows
    //               x  z  y   x z y   x z y
    [DIR_UP]    = {  2, 2, 0,  2,0,0,  0,2,0, }, // X-Z
    [DIR_DOWN]  = {  2, 2,32,  2,0,0,  0,2,0, }, // X-Z
    [DIR_SOUTH] = {  2, 0, 2,  2,0,0,  0,0,2, }, // X-Y
    [DIR_NORTH] = {  2,32, 2,  2,0,0,  0,0,2, }, // X-Y
    [DIR_EAST]  = {  0, 2, 2,  0,2,0,  0,0,2, }, // Z-Y
    [DIR_WEST]  = { 32, 2, 2,  0,2,0,  0,0,2, }, // Z-Y
};

// from all the dots which can be used to place a block correctly,
// remove those out of player's reach by updating the dot masks
static void remove_distant_dots(blk *b) {
    // reset distance to the block
    // this will be now replaced with the max dot distance
    b->dist = 0;

    fixp px = gs.own.x;
    fixp pz = gs.own.z;
    fixp py = gs.own.y+EYEHEIGHT;

    int f;
    for(f=0; f<6; f++) {
        if (!((b->neigh>>f)&1) && !b->needadj) continue; // no neighbor - skip this face
        uint16_t *dots = b->dots[f];
        dotpos_t dotpos = DOTPOS[f];

        // coordinates (fixed point) of the adjacent block
        fixp nx = (b->x+NOFF[f][0])<<5;
        fixp nz = (b->z+NOFF[f][1])<<5;
        fixp ny = (b->y+NOFF[f][2])<<5;

        int dr,dc;
        for(dr=0; dr<15; dr++) {
            uint16_t drow = dots[dr];
            if (!drow) continue; // skip disabled rows

            // dot dr,0 coordinates in 3D space
            fixp rx = nx + dotpos.x + dotpos.rx*dr;
            fixp ry = ny + dotpos.y + dotpos.ry*dr;
            fixp rz = nz + dotpos.z + dotpos.rz*dr;

            for(dc=0; dc<15; dc++) {
                uint16_t mask = 1<<dc;
                if (!(drow&mask)) continue; // skip disabled dots

                fixp x = rx + dotpos.cx*dc;
                fixp y = ry + dotpos.cy*dc;
                fixp z = rz + dotpos.cz*dc;

                int32_t dist = SQ(x-px)+SQ(z-pz)+SQ(y-py);

                if (dist > MAXREACH) {
                    dots[dr] &= ~mask; // this dot is too far away - disable it
                    drow = dots[dr];
                    continue;
                }

                if (b->rdir != DIR_ANY) {
                    // this block requires a certain player look direction on placement
                    float yaw,pitch;
                    int yawdir = calculate_yaw_pitch(x, z, y, &yaw, &pitch);
                    if (yawdir != b->rdir) {
                        // direction does not match - we can't place this block
                        // as needed from player's perspective
                        dots[dr] &= ~mask;
                        drow = dots[dr];
                        continue;
                    }
                }

                // update block distance - necessary for the decision
                // which block to place first
                if (b->dist < dist)
                    b->dist = dist;
            }
        }
    }

    b->inreach = (b->dist > 0);
}

// cound how many active dots are in a row
static inline int count_dots_row(uint16_t dots) {
    int i,c=0;
    for (i=0; i<15; dots>>=1,i++)
        c += (dots&1);
    return c;
}

// count how many active dots are on all faces of the block
static inline int count_dots(blk *b) {
    int f;
    int c=0;
    for(f=0; f<6; f++) {
        int dr;
        for(dr=0; dr<15; dr++) {
            c += count_dots_row(b->dots[f][dr]);
        }
    }

    return c;
}

// predicate function to sort the blocks by their distance to the player
static int sort_blocks(const void *a, const void *b) {
    int ia = *((int *)a);
    int ib = *((int *)b);

    if (P(build.task)[ia].dist > P(build.task)[ib].dist) return -1;
    if (P(build.task)[ia].dist < P(build.task)[ib].dist) return 1;

    return 0;
}

// set all dot faces on the block
static inline void setdots(blk *b, uint16_t *u, uint16_t *d,
                           uint16_t *s, uint16_t *n, uint16_t *e, uint16_t *w) {
    if (b->n_yp) memcpy(b->dots[DIR_UP],    u, sizeof(DOTS_ALL));
    if (b->n_yn) memcpy(b->dots[DIR_DOWN],  d, sizeof(DOTS_ALL));
    if (b->n_xp) memcpy(b->dots[DIR_EAST],  e, sizeof(DOTS_ALL));
    if (b->n_xn) memcpy(b->dots[DIR_WEST],  w, sizeof(DOTS_ALL));
    if (b->n_zp) memcpy(b->dots[DIR_SOUTH], s, sizeof(DOTS_ALL));
    if (b->n_zn) memcpy(b->dots[DIR_NORTH], n, sizeof(DOTS_ALL));
}

// update placed/empty flags of a buildtask only -
// used to update state for preview or material calculation
static void build_update_placed() {
    if (C(build.task)<=0) return;

    extent_t ex = { { build.xmin, build.ymin, build.zmin },
                    { build.xmax, build.ymax, build.zmax } };
    cuboid_t c = export_cuboid_extent(ex);

    int i;
    for(i=0; i<C(build.task); i++) {
        blk *b = P(build.task)+i;
        bid_t *slice = c.data[b->y-build.ymin]+c.boff;
        bid_t *row = slice+(b->z-build.zmin)*c.sa.x;
        bid_t bl = row[b->x-build.xmin];
        int smask = (ITEMS[b->b.bid].flags&I_STATE_MASK)^15;
        b->placed = (bl.bid == b->b.bid && (bl.meta&smask)==(b->b.meta&smask) );
        b->empty  = ISEMPTY(bl.bid) && !b->placed;
        b->current = bl;
    }
    free_cuboid(c);
}

#define PLACE_EAST(b)  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL);
#define PLACE_WEST(b)  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE);
#define PLACE_NORTH(b) setdots(b, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE, DOTS_NONE, DOTS_NONE);
#define PLACE_SOUTH(b) setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE, DOTS_NONE);
#define PLACE_FLOOR(b) setdots(b, DOTS_NONE, DOTS_ALL, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE);
#define PLACE_CEIL(b)  setdots(b, DOTS_ALL, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE);
#define PLACE_NONE(b)  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE);
#define PLACE_ALL(b)   setdots(b, DOTS_ALL, DOTS_ALL, DOTS_ALL, DOTS_ALL, DOTS_ALL, DOTS_ALL);

void set_block_dots(blk *b) {
    // determine usable dots on the neighbor faces
    lh_clear_obj(b->dots);

    //TODO: provide support for ALL position-dependent blocks

    const item_id *it = &ITEMS[b->b.bid];

    if (it->flags&I_SLAB) { // Halfslabs
        // Slabs
        if (b->b.meta&8) // upper half placement
            setdots(b, DOTS_ALL, DOTS_NONE, DOTS_UPPER, DOTS_UPPER, DOTS_UPPER, DOTS_UPPER);
        else // lower half placement
            setdots(b, DOTS_NONE, DOTS_ALL, DOTS_LOWER, DOTS_LOWER, DOTS_LOWER, DOTS_LOWER);
    }

    else if (it->flags&I_STAIR) { // Stairs
        // Stairs

        // determine the required look direction for the correct block placement
        b->rdir = (b->b.meta&2) ?
            ((b->b.meta&1) ? DIR_NORTH : DIR_SOUTH ) :
            ((b->b.meta&1) ? DIR_WEST  : DIR_EAST );
        // the direction will be checked for each dot in remove_distant_dots()

        if (b->b.meta&4) // upside-down placement
            setdots(b, DOTS_ALL, DOTS_NONE, DOTS_UPPER, DOTS_UPPER, DOTS_UPPER, DOTS_UPPER);
        else // straight placement
            setdots(b, DOTS_NONE, DOTS_ALL, DOTS_LOWER, DOTS_LOWER, DOTS_LOWER, DOTS_LOWER);
    }

    else if (it->flags&I_LOG) { // Wood Logs and Hay Bales
        switch((b->b.meta>>2)&3) {
            case 0: // Up-Down
            case 3: // All-bark (not possible, but we just assume up-down)
                setdots(b, DOTS_ALL, DOTS_ALL, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE);
                break;
            case 1: // East-West
                setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_ALL);
                break;
            case 2: // North-South
                setdots(b, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_ALL, DOTS_NONE, DOTS_NONE);
                break;
        }
    }

    else if (it->flags&I_TORCH) { // Torches and Redstone Torches
        switch(b->b.meta) {
            case 1: PLACE_EAST(b); break;
            case 2: PLACE_WEST(b); break;
            case 3: PLACE_SOUTH(b); break;
            case 4: PLACE_NORTH(b); break;
            case 5: PLACE_FLOOR(b); break;
            default: PLACE_NONE(b); break;
        }
    }

    else if (it->flags&I_ONWALL) { // Ladder, Wall Signs and Banners
        switch(b->b.meta) {
            case 2: PLACE_NORTH(b); break;
            case 3: PLACE_SOUTH(b); break;
            case 4: PLACE_WEST(b); break;
            case 5: PLACE_EAST(b); break;
            default: PLACE_NONE(b); break;
        }
    }

    else if (it->flags&I_RSRC) { // Redstone Repeater / Comparator
        // required look direction for the correct block placement
        b->rdir = (b->b.meta&1) ?
            ((b->b.meta&2) ? DIR_WEST  : DIR_EAST ) :
            ((b->b.meta&2) ? DIR_SOUTH : DIR_NORTH);
        PLACE_ALL(b);
    }

    else if (b->b.bid == 0x9a) { // Hopper
        switch(b->b.meta&7) { // ignore state bit
            case 0: PLACE_FLOOR(b); break;
            case 2: PLACE_SOUTH(b); break;
            case 3: PLACE_NORTH(b); break;
            case 4: PLACE_EAST(b); break;
            case 5: PLACE_WEST(b); break;
            default: PLACE_NONE(b); break;
        }
    }

    else if (it->flags&I_RSDEV) { // Pistons, Dispensers and Droppers
        int px = gs.own.x>>5;
        int pz = gs.own.z>>5;
        int py = (gs.own.y>>5)+((gs.own.y>>4)&1); // rounded

        int dx = b->x-px;
        int dz = b->z-pz;

        if (dx>-2 && dx<3 && dz>-2 && dz<3) {
            // placing in a 4x4 block zone around the player
            // (1 block to the north and west, 2 blocks to south and east)
            // printf("Dead zone dx=%d, dz=%d, by=%d py=%d\n", dx, dz, b->y, py);
            switch(b->b.meta&7) {
                case 0: if (b->y>=py+2) { PLACE_ALL(b); } else { PLACE_NONE(b); } break;
                case 1: if (b->y<py) { PLACE_ALL(b); } else { PLACE_NONE(b); } break;
                case 2: if (b->y>=py && b->y<py+2) { PLACE_ALL(b); b->rdir=DIR_SOUTH; } else { PLACE_NONE(b); } break;
                case 3: if (b->y>=py && b->y<py+2) { PLACE_ALL(b); b->rdir=DIR_NORTH; } else { PLACE_NONE(b); } break;
                case 4: if (b->y>=py && b->y<py+2) { PLACE_ALL(b); b->rdir=DIR_EAST; } else { PLACE_NONE(b); } break;
                case 5: if (b->y>=py && b->y<py+2) { PLACE_ALL(b); b->rdir=DIR_WEST; } else { PLACE_NONE(b); } break;
                default: PLACE_NONE(b); break;
            }
        }
        else {
            switch(b->b.meta&7) {
                case 2:  PLACE_ALL(b);  b->rdir=DIR_SOUTH; break;
                case 3:  PLACE_ALL(b);  b->rdir=DIR_NORTH; break;
                case 4:  PLACE_ALL(b);  b->rdir=DIR_EAST;  break;
                case 5:  PLACE_ALL(b);  b->rdir=DIR_WEST;  break;
                default: PLACE_NONE(b); break;
            }
        }
    }

    else if (it->flags&I_DOOR) { // Doors
        if (b->b.meta&8) {
            // top half can't be placed
            PLACE_NONE(b);
        }
        else {
            b->rdir = (b->b.meta&1) ?
                ((b->b.meta&2) ? DIR_NORTH : DIR_SOUTH) :
                ((b->b.meta&2) ? DIR_WEST  : DIR_EAST );

            PLACE_FLOOR(b);
        }

        //TODO: check if the door we are trying to place is right-hinged
        // in that case we need to place it only if there is a block
        // on the right side, otherwise the door will become left-hinged
        // by default. For this we need access to the top half meta value though
    }

    else if (it->flags&I_TDOOR) {
        switch (b->b.meta&11) { // ignore the open state bit
            case 0:  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_LOWER, DOTS_NONE, DOTS_NONE, DOTS_NONE); break;
            case 1:  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_LOWER, DOTS_NONE, DOTS_NONE); break;
            case 2:  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_LOWER, DOTS_NONE); break;
            case 3:  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_LOWER); break;
            case 8:  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_UPPER, DOTS_NONE, DOTS_NONE, DOTS_NONE); break;
            case 9:  setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_UPPER, DOTS_NONE, DOTS_NONE); break;
            case 10: setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_UPPER, DOTS_NONE); break;
            case 11: setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_UPPER); break;
        };
    }

    else if (b->b.bid == 26) { // bed
        if (b->b.meta < 8) { // only place the foot of the bed
            switch (b->b.meta&3) { // ignore the occupied bit
                case 0: b->rdir = DIR_SOUTH; break;
                case 1: b->rdir = DIR_WEST; break;
                case 2: b->rdir = DIR_NORTH; break;
                case 3: b->rdir = DIR_EAST; break;
            }
            PLACE_FLOOR(b);
        }
        else {
            PLACE_NONE(b);
        }
    }

    else if (it->flags&I_PLANT) {
        PLACE_FLOOR(b);
        int fl = b->nblocks[DIR_DOWN].bid;

        switch (b->b.bid) {
            case 0x06: // Sapling
            case 0x26: // Flower
            case 0xaf: // Large flower
                if (fl!=0x02 && fl!=0x03 && fl!=0x3c)
                    PLACE_NONE(b);
                break;

            case 0x3b: // Wheat
            case 0x68: // Pumpkin stem
            case 0x69: // Melon stem
            case 0x8d: // Crrot plant
            case 0x8e: // Potato plant
                if (fl!=0x3c)
                    PLACE_NONE(b);
                break;

            case 0x51: // Cactus
                if (fl!=0x0c)
                    PLACE_NONE(b);
                break;

            case 0x53: // Sugar Cane
                if (fl!=0x0c && fl!=0x02 && fl!=0x03)
                    PLACE_NONE(b);
                break;
        }
    }

    else if (it->flags&I_CHEST) {
        switch (b->b.meta&7) {
            case 2: b->rdir = DIR_SOUTH; break;
            case 3: b->rdir = DIR_NORTH; break;
            case 4: b->rdir = DIR_EAST; break;
            case 5: b->rdir = DIR_WEST; break;
        }
        PLACE_ALL(b);
    }

    else if (b->b.bid == 69) { // Lever
        switch(b->b.meta&7) {
            case 0: PLACE_CEIL(b); b->rdir=DIR_SOUTH; break;
            case 7: PLACE_CEIL(b); b->rdir=DIR_EAST; break;

            case 5: PLACE_FLOOR(b); b->rdir=DIR_SOUTH; break;
            case 6: PLACE_FLOOR(b); b->rdir=DIR_EAST; break;

            case 1: PLACE_EAST(b); break;
            case 2: PLACE_WEST(b); break;
            case 3: PLACE_SOUTH(b); break;
            case 4: PLACE_NORTH(b); break;
        }
    }

    else if (b->b.bid==77 || b->b.bid==143) { // Buttons
        switch(b->b.meta) {
            case 0: PLACE_CEIL(b); break;
            case 1: PLACE_EAST(b); break;
            case 2: PLACE_WEST(b); break;
            case 3: PLACE_SOUTH(b); break;
            case 4: PLACE_NORTH(b); break;
            case 5: PLACE_FLOOR(b); break;
            default: PLACE_NONE(b); break;
        }
    }

    else if (it->flags&I_GATE) {
        switch (b->b.meta&3) {
            case 0: b->rdir = DIR_SOUTH; break;
            case 1: b->rdir = DIR_WEST;  break;
            case 2: b->rdir = DIR_NORTH; break;
            case 3: b->rdir = DIR_EAST;  break;
        }
        PLACE_ALL(b);
    }

    else {
        // Blocks that don't have I_MPOS or not supported
        PLACE_ALL(b);
    }

    // disable the faces where there is no neighbor
    int f;
    for (f=0; f<6; f++)
        if (!((b->neigh>>f)&1))
            memset(b->dots[f], 0, sizeof(DOTS_ALL));
}

// called when player position or look have changed - update our placeable blocks list
void build_update() {
    if (!build.active) return;

    int i,f;

    // 1. Update 'inreach' flag for all blocks and set the block distance
    // inreach=1 does not necessarily mean the block is really reachable -
    // this will be determined later in more detail, but those with
    // inreach=0 are definitely too far away to bother.
    int num_inreach = 0;
    for(i=0; i<C(build.task); i++) {
        blk *b = P(build.task)+i;
        b->state = 0;
        b->inreach = 1;

        // avoid building blocks above the player in wall and limit mode
        if ((buildopts.wallmode && (b->y>(gs.own.y>>5)-1)) ||
            (build.limit && (b->y>build.limit))) {
                b->inreach = 0;
                continue;
        }

        // if the "seal mode" is active, only build blocks located in front of you
        if (buildopts.sealmode) {
            switch (player_direction()) {
                case DIR_NORTH: if (b->z >= (gs.own.z>>5)) b->inreach=0; break;
                case DIR_SOUTH: if (b->z <= (gs.own.z>>5)) b->inreach=0; break;
                case DIR_WEST:  if (b->x >= (gs.own.x>>5)) b->inreach=0; break;
                case DIR_EAST:  if (b->x <= (gs.own.x>>5)) b->inreach=0; break;
            }
            if (b->inreach==0) continue;
        }

        int32_t dx = gs.own.x-(b->x<<5)+16;
        int32_t dy = gs.own.y-(b->y<<5)+16+EYEHEIGHT;
        int32_t dz = gs.own.z-(b->z<<5)+16;
        b->dist = SQ(dx)+SQ(dy)+SQ(dz);

        b->inreach = (b->dist<MAXREACH_COARSE);
        num_inreach += b->inreach;
    }
    if (num_inreach==0) {
        // no potentially buildable blocks nearby - don't bother with the rest
        build.nbq = 0;
        build.bq[0] = -1;
        return;
    }

    // 2. extract the cuboid with the existing world blocks

    int32_t xmin = (gs.own.x>>5)-12;
    int32_t xmax = (gs.own.x>>5)+12;
    int32_t ymin = (gs.own.y>>5)-12;
    if (ymin<0) ymin=0;
    int32_t ymax = (gs.own.y>>5)+12;
    if (ymax>255) ymax=255;
    int32_t zmin = (gs.own.z>>5)-12;
    int32_t zmax = (gs.own.z>>5)+12;

    extent_t ex = { { xmin, ymin, zmin }, { xmax, ymax, zmax } };
    cuboid_t c = export_cuboid_extent(ex);

    // 3. determine which blocks are occupied and which neighbors are available
    build.nbq = 0;
    for(i=0; i<C(build.task); i++) {
        blk *b = P(build.task)+i;
        b->rdir = DIR_ANY;
        if (!b->inreach) continue;

        int x = b->x-xmin;
        int y = b->y-ymin;
        int z = b->z-zmin;

        bid_t *row   = c.data[y]+c.boff+z*c.sa.x;
        bid_t *row_u = c.data[y+1]+c.boff+z*c.sa.x;
        bid_t *row_d = c.data[y-1]+c.boff+z*c.sa.x;
        bid_t *row_n = c.data[y]+c.boff+(z-1)*c.sa.x;
        bid_t *row_s = c.data[y]+c.boff+(z+1)*c.sa.x;

        bid_t bl = row[x];

        // check if this block is already correctly placed (including meta)
        //TODO: implement less restricted check for blocks with non-positional meta
        b->placed = (bl.raw == b->b.raw);


        // check if the block is empty, but ignore those that are already
        // placed - this way we can support "empty" blocks like water in our buildplan
        b->empty  = ISEMPTY(bl.bid) && !b->placed;

        const item_id *it = &ITEMS[b->b.bid];
        if (it->flags&I_DSLAB) {
            // we want to place doubleslab here and the block contains
            // a suitable slab - mark it as empty, so we can place the second slab
            bid_t bm = get_base_material(b->b);
            if (bm.bid==bl.bid && bm.meta==(bl.meta&7))
                b->empty = 1;
        }

        // check if this block needs state adjustment (e.g. repeaters)
        int smask = (it->flags&I_STATE_MASK)^15;
        if (bl.bid==b->b.bid && (bl.meta&smask)!=(b->b.meta&smask) && (it->flags&I_ADJ))
            b->needadj = 1;

        //TODO: when placing a double slab, prevent obstruction - place the slab further away first
        //TODO: take care when placing a slab over a slab - prevent a doubleslab creation

        // determine which neighbors do we have
        bid_t nbl;
        nbl = b->nblocks[DIR_UP] = row_u[x];
        b->n_yp = !ISEMPTY(nbl.bid);
        nbl = b->nblocks[DIR_DOWN] = row_d[x];
        b->n_yn = !ISEMPTY(nbl.bid);
        nbl = b->nblocks[DIR_SOUTH] = row_s[x];
        b->n_zp = !ISEMPTY(nbl.bid);
        nbl = b->nblocks[DIR_NORTH] = row_n[x];
        b->n_zn = !ISEMPTY(nbl.bid);
        nbl = b->nblocks[DIR_EAST]  = row[x+1];
        b->n_xp = !ISEMPTY(nbl.bid);
        nbl = b->nblocks[DIR_WEST]  = row[x-1];
        b->n_xn = !ISEMPTY(nbl.bid);

        if (b->needadj) {
            // we can handle adjustment clicks on the block itself in similar fashion
            // but instead of clicking on the neightbors, we need to click on the
            // block itself. We set all dots on all faces as clickable.
            b->neigh = 0x3f; // pretend we have all neighbors
            PLACE_ALL(b);

            // disable faces looking away from you
            if (!buildopts.anyface) {
                if (b->y > (gs.own.y>>5)+1) memset(b->dots[DIR_UP], 0, sizeof(DOTS_ALL));
                if (b->y < (gs.own.y>>5)+2) memset(b->dots[DIR_DOWN], 0, sizeof(DOTS_ALL));
                if (b->x > (gs.own.x>>5)) memset(b->dots[DIR_EAST], 0, sizeof(DOTS_ALL));
                if (b->x < (gs.own.x>>5)) memset(b->dots[DIR_WEST], 0, sizeof(DOTS_ALL));
                if (b->z > (gs.own.z>>5)) memset(b->dots[DIR_SOUTH], 0, sizeof(DOTS_ALL));
                if (b->z < (gs.own.z>>5)) memset(b->dots[DIR_NORTH], 0, sizeof(DOTS_ALL));
            }
        }
        else {
            // skip the blocks we can't place
            if (b->placed || !b->empty || !b->neigh) continue;

            set_block_dots(b);

            // disable faces looking away from you
            if (!buildopts.anyface) {
                if (b->y < (gs.own.y>>5)+1) memset(b->dots[DIR_UP], 0, sizeof(DOTS_ALL));
                if (b->y > (gs.own.y>>5)+2) memset(b->dots[DIR_DOWN], 0, sizeof(DOTS_ALL));
                if (b->x < (gs.own.x>>5)) memset(b->dots[DIR_EAST], 0, sizeof(DOTS_ALL));
                if (b->x > (gs.own.x>>5)) memset(b->dots[DIR_WEST], 0, sizeof(DOTS_ALL));
                if (b->z < (gs.own.z>>5)) memset(b->dots[DIR_SOUTH], 0, sizeof(DOTS_ALL));
                if (b->z > (gs.own.z>>5)) memset(b->dots[DIR_NORTH], 0, sizeof(DOTS_ALL));
            }
        }

        // calculate exact distance to each of the dots and remove those out of reach
        remove_distant_dots(b);

        b->ndots = count_dots(b);
        if (b->ndots>0 && build.nbq<MAXBUILDABLE)
            build.bq[build.nbq++] = i;
    }
    build.bq[build.nbq] = -1;

    qsort(build.bq, build.nbq, sizeof(build.bq[0]), sort_blocks);

    /* Further things TODO:
       - remove the dots obstructed by other blocks
       - make the various obstruction checking options configurable
    */
    free_cuboid(c);
}

// randomly choose which of the suitable dots we are going to use to place the block
static void choose_dot(blk *b, int8_t *face, int8_t *cx, int8_t *cy, int8_t *cz) {
    int f,dr,dc;
    int i=random()%b->ndots;

    for(f=0; f<6; f++) {
        if (!((b->neigh>>f)&1)) continue;
        for(dr=0; dr<15; dr++) {
            uint16_t dots = b->dots[f][dr];
            if (!dots) continue;
            for(dc=0; dc<15; dc++) {
                if ((dots>>dc)&1) {
                    i--;
                    if (i<0) {
                        dotpos_t dotpos = DOTPOS[f];
                        *face = f;
                        *cx = (dotpos.x+dotpos.rx*dr+dotpos.cx*dc)/2;
                        *cy = (dotpos.y+dotpos.ry*dr+dotpos.cy*dc)/2;
                        *cz = (dotpos.z+dotpos.rz*dr+dotpos.cz*dc)/2;
                        //printf("face=%d dot=%d,%d, cur=%d,%d,%d\n",*face,dr,dc,*cx,*cy,*cz);
                        return;
                    }
                }
            }
        }
    }
    printf("Fail: i=%d\n",i);
    assert(0);
}

// asynchronous building method - check the buildqueue and try to build up to maxbld blocks
void build_progress(MCPacketQueue *sq, MCPacketQueue *cq) {
    // time update - try to build any blocks from the placeable blocks list
    if (!build.active) return;

    // do not attempt to build while jumping or falling (i.e. feet not on ground)
    if (!(gs.own.onground || buildopts.bjump)) return;

    uint64_t ts = gettimestamp();

    if (ts < build.lastbuild+buildopts.bldint) return;

    int i, bc=0;
    int held=gs.inv.held;

    for(i=0; i<build.nbq && bc<buildopts.blkmax; i++) {
        char buf[4096];
        char buf2[4096];

        blk *b = P(build.task)+build.bq[i];
        if (ts-b->last < buildopts.blkint) continue;

        // fetch block's material into quickbar slot
        int islot = prefetch_material(sq, cq, get_base_material(b->b));
        if (islot<0) continue; // we don't have this material
        //TODO: notify user about missing materials

        // silently switch to this slot
        gmi_change_held(sq, cq, islot, 1);
        slot_t * hslot = &gs.inv.slots[islot+36];

        int8_t face, cx, cy, cz;
        choose_dot(b, &face, &cx, &cy, &cz);

        // dot's absolute 3D coordinates (fixed point)
        fixp tx = ((b->x+NOFF[face][0])<<5) + cx*2;
        fixp tz = ((b->z+NOFF[face][1])<<5) + cz*2;
        fixp ty = ((b->y+NOFF[face][2])<<5) + cy*2;

        float yaw, pitch;
        int ldir = calculate_yaw_pitch(tx, tz, ty, &yaw, &pitch);

        int needcrouch=0;

        if (b->needadj) {
            printf("Adjusting Block: %d,%d,%d (%s)\n",
                   b->x,b->y,b->z, get_item_name(buf, hslot));
        }
        else {
            const item_id *nit = &ITEMS[b->nblocks[face].bid];
            if (nit->flags&(I_CONT|I_ADJ) && !gs.own.crouched)
                needcrouch=1;

            printf("Placing Block: %d,%d,%d (%s)  On: %d,%d,%d (%02x, %s) "
                   "Face:%d Cursor:%d,%d,%d  "
                   "Player: %.1f,%.1f,%.1f  Dot: %.1f,%.1f,%.1f  "
                   "Rot=%.2f,%.2f  Dir=%d (%s) %s\n",
                   b->x,b->y,b->z, get_item_name(buf, hslot),
                   b->x+NOFF[face][0],b->z+NOFF[face][1],b->y+NOFF[face][2],
                   b->nblocks[face].bid, get_bid_name(buf2, b->nblocks[face]),
                   face, cx, cy, cz,
                   (float)gs.own.x/32, (float)(gs.own.y+EYEHEIGHT)/32, (float)gs.own.z/32,
                   (float)b->x+(float)cx/16,(float)b->y+(float)cy/16,(float)b->z+(float)cz/16,
                   yaw, pitch, ldir, DIRNAME[ldir],
                   needcrouch?"(need to crouch)":"");
        }

        // crouch if we have to place block on a block that reacts to right-click
        if (needcrouch) {
            NEWPACKET(CP_EntityAction, crouch);
            tcrouch->eid = gs.own.eid;
            tcrouch->action = 0;
            tcrouch->jumpboost = 0;
            queue_packet(crouch,sq);
        }

        // turn player look to the dot
        NEWPACKET(CP_PlayerLook, pl);
        tpl->yaw = yaw;
        tpl->pitch = pitch;
        tpl->onground = gs.own.onground;
        queue_packet(pl,sq);

        // place block
        NEWPACKET(CP_PlayerBlockPlacement, pbp);
        if (b->needadj) {
            // When adjusting a block, click on the block itself, not on a neighbor
            tpbp->bpos = POS(b->x, b->y, b->z);
            switch(face) {
                case DIR_UP: face=DIR_DOWN; break;
                case DIR_DOWN: face=DIR_UP; break;
                case DIR_NORTH: face=DIR_SOUTH; break;
                case DIR_SOUTH: face=DIR_NORTH; break;
                case DIR_EAST: face=DIR_WEST; break;
                case DIR_WEST: face=DIR_EAST; break;
            }
        }
        else {
            tpbp->bpos = POS(b->x+NOFF[face][0],b->y+NOFF[face][2],b->z+NOFF[face][1]);
        }
        tpbp->face = face;
        tpbp->cx = cx;
        tpbp->cy = cy;
        tpbp->cz = cz;
        clone_slot(hslot,&tpbp->item);
        queue_packet(pbp,sq);
        //dump_packet(pbp);

        // Wave arm
        NEWPACKET(CP_Animation, anim);
        queue_packet(anim, sq);

        // uncrouch again
        if (needcrouch) {
            NEWPACKET(CP_EntityAction, uncrouch);
            tuncrouch->eid = gs.own.eid;
            tuncrouch->action = 1;
            tuncrouch->jumpboost = 0;
            queue_packet(uncrouch,sq);
        }

        // restore the former look direction
        NEWPACKET(CP_PlayerLook, pl2);
        tpl2->yaw = gs.own.yaw;
        tpl2->pitch = gs.own.pitch;
        tpl2->onground = gs.own.onground;
        queue_packet(pl2,sq);

        b->last = ts;
        build.lastbuild = ts;
        mat_last[islot] = ts;
        bc++;
    }

    // switch back to whatever the client was holding
    if (held != gs.inv.held)
        gmi_change_held(sq, cq, held, 1);
}




////////////////////////////////////////////////////////////////////////////////
// Pivot placement

// a pivot block has been placed (using whatever method)
void place_pivot(pivot_t pv, MCPacketQueue *sq, MCPacketQueue *cq) {
    assert(build.bp);

    // cancel the placement mode, unless "place many" is active
    if (build.placemode == 1) {
        build.placemode = 0;
        build_cancel(sq, cq);
    }

    // create a new buildtask from our buildplan
    int i, trimmed=0;
    for(i=0; i<C(build.bp->plan); i++) {
        blkr bp=rel2abs(pv, P(build.bp->plan)[i]);
        if (bp.y<0 || bp.y >255) {
            // exclude blocks that exceed vertical range to avoid issues
            // with placement and preview
            trimmed++;
            continue;
        }
        blk  *bt = lh_arr_new_c(BTASK); // new element in the buildtask
        bt->x = bp.x;
        bt->y = bp.y;
        bt->z = bp.z;
        bt->b = bp.b;
    }
    build.active = 1;

    // calculate buildtask boundary
    build.xmin = build.xmax = P(build.task)[0].x;
    build.zmin = build.zmax = P(build.task)[0].z;
    build.ymin = build.ymax = P(build.task)[0].y;
    for(i=0; i<C(build.task); i++) {
        build.xmin = MIN(build.xmin, P(build.task)[i].x);
        build.xmax = MAX(build.xmax, P(build.task)[i].x);
        build.ymin = MIN(build.ymin, P(build.task)[i].y);
        build.ymax = MAX(build.ymax, P(build.task)[i].y);
        build.zmin = MIN(build.zmin, P(build.task)[i].z);
        build.zmax = MAX(build.zmax, P(build.task)[i].z);
    }

    // store the coordinates and direction so they can be reused for 'place again'
    build.pv = pv;

    printf("Buildtask boundary: X: %d - %d   Z: %d - %d   Y: %d - %d\n",
           build.xmin, build.xmax, build.zmin, build.zmax, build.ymin, build.ymax);
    if (trimmed > 0)
        printf("Warning: %d blocks exceeded the y range and were trimmed\n", trimmed);

    build_update();
}

// handler for placing the pivot block in-game ("place once" or "place many")
void build_placemode(MCPacket *pkt, MCPacketQueue *sq, MCPacketQueue *cq) {
    CP_PlayerBlockPlacement_pkt *tpkt = &pkt->_CP_PlayerBlockPlacement;

    if (tpkt->face < 0) return; // ignore "fake" block placements
    int32_t x = tpkt->bpos.x - NOFF[tpkt->face][0];
    int32_t z = tpkt->bpos.z - NOFF[tpkt->face][1];
    int32_t y = tpkt->bpos.y - NOFF[tpkt->face][2];

    int dir = player_direction();

    pivot_t pv = { {x,y,z}, dir};
    place_pivot(pv, sq, cq);

    char reply[4096];
    sprintf(reply, "Placing pivot at %d,%d (%d), dir=%s\n",x,z,y,DIRNAME[dir]);
    chat_message(reply, cq, "green", 0);

    //TODO: detect when player accesses chests/etc

    // send a BlockChange to the client to remove the fake block it thinks it just placed
    NEWPACKET(SP_BlockChange, bc);
    tbc->pos.x = x;
    tbc->pos.y = y;
    tbc->pos.z = z;
    tbc->block = get_block_at(x,z,y);
    queue_packet(bc, cq);

    // send a SetSlot to the client for the held item to restore item count
    NEWPACKET(SP_SetSlot, hi);
    thi->wid = 0;
    thi->sid = gs.inv.held+36;
    clone_slot(&gs.inv.slots[thi->sid], &thi->slot);
    queue_packet(hi, cq);
}




////////////////////////////////////////////////////////////////////////////////
// Build Recorder

// dump BREC blocks that are pending block update from the server
static void dump_brec_pending() {
    //printf("BREC pending queue: %zd entries\n",build.nbrp);

    int i;
    char buf[256];
    for(i=0; i<build.nbrp; i++) {
        blkr *bl = &build.brp[i];
        printf("%2d : %d,%d,%d (%s)\n", i,
               bl->x, bl->y, bl->z, get_bid_name(buf, bl->b));
    }
}

// received a block update from the server - check if we have this block in the
// BREC pending queue and record it in the buildplan
static void brec_blockupdate_blk(blkr b) {
    // keep only orientation and material-related meta bits
    // clear the state-related bits
    b.b.meta &= ~(ITEMS[b.b.bid].flags&I_STATE_MASK);

    int i;
    for(i=0; i<build.nbrp; i++) {
        blkr *bl = &build.brp[i];
        if (bl->x==b.x && bl->y==b.y && bl->z==b.z) {
            // remove block from the pending queue
            blkr *brp = build.brp;
            lh_arr_delete(brp, build.nbrp, 1, i);

            // add the block to the buildplan
            bplan_add(build.bp, abs2rel(build.pv, b));
            return;
        }
    }
}

// handler for the SP_BlockChange and SP_MultiBlockChange messages from the server
// dispatch each updated block to brec_blockupdate_blk()
static void brec_blockupdate(MCPacket *pkt) {
    switch(pkt->pid) {
        case SP_BlockChange:
            if (pkt->pid == SP_BlockChange) {
                SP_BlockChange_pkt *tpkt = &pkt->_SP_BlockChange;
                blkr b = { tpkt->pos.x,tpkt->pos.y,tpkt->pos.z,tpkt->block };
                brec_blockupdate_blk(b);
            }
            bplan_update(build.bp);
            break;
        case SP_MultiBlockChange: {
            SP_MultiBlockChange_pkt *tpkt = &pkt->_SP_MultiBlockChange;
            int i;
            for(i=0; i<tpkt->count; i++) {
                blkrec *br = tpkt->blocks+i;
                blkr b = { ((tpkt->X)<<4)+br->x,br->y,((tpkt->Z)<<4)+br->z,br->bid };
                brec_blockupdate_blk(b);
            }
            bplan_update(build.bp);
            break;
        }
    }
}

// handler for the CP_PlayerBlockPlacement message from the client
// place this block in the pending queue - it will be recorded once
// we get a confirmation from the server that it's been placed
static void brec_blockplace(MCPacket *pkt) {
    assert(pkt->pid == CP_PlayerBlockPlacement);
    CP_PlayerBlockPlacement_pkt *tpkt = &pkt->_CP_PlayerBlockPlacement;

    if (tpkt->face < 0) return; // ignore "fake" block placements
    bid_t b = BLOCKTYPE(tpkt->item.item, tpkt->item.damage);

    int32_t x = tpkt->bpos.x - NOFF[tpkt->face][0];
    int32_t z = tpkt->bpos.z - NOFF[tpkt->face][1];
    int32_t y = tpkt->bpos.y - NOFF[tpkt->face][2];

    //printf("Recording block at %d,%d,%d\n",x,y,z);

    // check if the placed block will become a double slab
    if (ITEMS[b.bid].flags&I_SLAB) {
        bid_t on = get_block_at(tpkt->bpos.x,tpkt->bpos.z,tpkt->bpos.y);
        if (get_base_material(b).raw==get_base_material(on).raw &&
            ((tpkt->face==DIR_UP && (on.meta&8)) ||
             (tpkt->face==DIR_DOWN && !(on.meta&8)) ) ) {
                y = tpkt->bpos.y;
                b.bid--; // the doubleslab's block ID is 1 below the
                         // corresponding slab, meta is the same
        }
    }

    // verify if this block is already in the pending queue
    int i;
    for(i=0; i<build.nbrp; i++) {
        blkr *bl = &build.brp[i];
        if (bl->x == x && bl->y == y && bl->z == z) {
            printf("BREC: warning, block %d,%d,%d already in the pending queue\n",x,y,z);
            bl->b = b; // update the block ID just in case
            return;
        }
    }

    if (build.nbrp >= MAXBUILDABLE) {
        printf("Warning: ignoring block placement, pending block queue full!\n");
        return;
    }

    // create a new record in the pending queue
    blkr *bl = &build.brp[build.nbrp];
    build.nbrp++;
    *bl = (blkr) {x,y,z,b};

    // if this is the first block being recorded, set it as pivot
    if (!build.pv.dir) {
        build.pv = (pivot_t) { {x,y,z}, player_direction() };
        printf("Set pivot block at %d,%d,%d dir=%d\n",
               build.pv.pos.x,build.pv.pos.y,build.pv.pos.z,build.pv.dir);
    }

    dump_brec_pending();
}


////////////////////////////////////////////////////////////////////////////////
// Options

#define BUILDOPT_STDOUT 1
#define BUILDOPT_CHAT   2

// print out current settings in the chat
void buildopt_print(int where, MCPacketQueue *cq) {
    char buf[4096];
    int i;
    for (i=0; OPTIONS[i].name; i++) {
        sprintf(buf, "%s=%d : %s", OPTIONS[i].name, *OPTIONS[i].var, OPTIONS[i].description);
        if (where&BUILDOPT_STDOUT)
            printf("%s\n",buf);
        if (where&BUILDOPT_CHAT)
            chat_message(buf, cq, "green", 0);
    }
}

// reset build options to default values
void buildopt_setdefault() {
    int i;
    for(i=0; OPTIONS[i].name; i++) {
        *OPTIONS[i].var = OPTIONS[i].defvalue;
    }
    buildopts.init = 1;
    buildopt_print(BUILDOPT_STDOUT, NULL);
}

// entry point for all command lines involving build options
void buildopt(char **words, MCPacketQueue *cq) {
    if (!words[0] || !strcmp(words[0],"list")) {
        buildopt_print(BUILDOPT_STDOUT|BUILDOPT_CHAT, cq);
        return;
    }
    else if (!strcmp(words[0],"reset")) {
        buildopt_setdefault();
        return;
    }

    char name[256];
    int  val;
    int  set=0;

    char *eq = index(words[0],'=');
    if (eq) {
        *eq=0;
        strcpy(name, words[0]);
        if (sscanf(eq+1,"%d",&val)==1)
            set=1;
    }
    else if (words[1] && sscanf(words[1], "%d", &val)==1) {
        strcpy(name, words[0]);
        set=1;
    }
    else {
        strcpy(name, words[0]);
    }

    int i;
    for(i=0; OPTIONS[i].name; i++) {
        if (!strcmp(OPTIONS[i].name, name)) {
            break;
        }
    }

    char reply[256]; reply[0]=0;

    if (!OPTIONS[i].name) {
        sprintf(reply, "No such variable %s", name);
    }
    else {
        if (set) {
            *OPTIONS[i].var = val;
            sprintf(reply, "set %s=%d",name,val);
        }
        else {
            sprintf(reply, "%s=%d",name,*OPTIONS[i].var);
        }
    }

    if (reply[0])
        chat_message(reply, cq, "green", 0);
}



////////////////////////////////////////////////////////////////////////////////
// Debugging

// dump our buildtask to console
void build_dump_task() {
    int i;
    char buf[256];
    for(i=0; i<C(build.task); i++) {
        blk *b = &P(build.task)[i];
        printf("%3d %+5d,%+5d,%3d %3x/%02x dist=%-5d (%.2f) %c%c%c %c%c%c%c%c%c (%3d) material=%s\n",
               i, b->x, b->z, b->y, b->b.bid, b->b.meta,
               b->dist, sqrt((float)b->dist)/32,
               b->inreach?'R':'.',
               b->empty  ?'E':'.',
               b->placed ?'P':'.',

               b->n_yp ? '*':'.',
               b->n_yn ? '*':'.',
               b->n_zp ? '*':'.',
               b->n_zn ? '*':'.',
               b->n_xp ? '*':'.',
               b->n_xn ? '*':'.',
               b->ndots,

               get_bid_name(buf, get_base_material(b->b)));
    }
}

// dump our buildqueue to console
void build_dump_queue() {
    int i;
    char buf[256];
    for(i=0; i<build.nbq; i++) {
        blk *b = P(build.task)+build.bq[i];
        printf("%3d %+5d,%+5d,%3d %3x/%02x dist=%-5d (%.4f) %c%c%c %c%c%c%c%c%c (%3d) material=%s\n",
               build.bq[i], b->x, b->z, b->y, b->b.bid, b->b.meta,
               b->dist, sqrt((float)b->dist)/32,
               b->inreach?'R':'.',
               b->empty  ?'E':'.',
               b->placed ?'P':'.',

               b->n_yp ? '*':'.',
               b->n_yn ? '*':'.',
               b->n_zp ? '*':'.',
               b->n_zn ? '*':'.',
               b->n_xp ? '*':'.',
               b->n_xn ? '*':'.',
               b->ndots,

               get_bid_name(buf, get_base_material(b->b)));
    }
}




////////////////////////////////////////////////////////////////////////////////
// In-game preview

//#define PREVIEW_BLOCK BLOCKTYPE(0x98,0)
#define PREVIEW_BLOCK BLOCKTYPE(83,0)

#define PREVIEW_REMOVE  0
#define PREVIEW_MISSING 1
#define PREVIEW_TRUE    2

void build_show_preview(MCPacketQueue *sq, MCPacketQueue *cq, int mode) {
    if (C(build.task)<=0) return;
    build_update_placed();

    int npackets=0;
    MCPacket *packets[1000];
    lh_clear_obj(packets);

    int i,j;
    for(i=0; i<C(build.task); i++) {
        blk *b = &P(build.task)[i];
        if (build.limit && b->y>build.limit) continue;
        if (b->placed) continue;

        int32_t X=b->x>>4;
        int32_t Z=b->z>>4;

        // skip blocks located in unloaded chunks
        int32_t idx = find_chunk(gs.world, X, Z);
        if (!gs.world->chunks[idx]) continue;

        // see if we already have a packet for this chunk prepared
        SP_MultiBlockChange_pkt *tpkt=NULL;
        for(j=0; j<npackets; j++) {
            if (packets[j]->_SP_MultiBlockChange.X==X &&
                packets[j]->_SP_MultiBlockChange.Z==Z) {
                tpkt = &packets[j]->_SP_MultiBlockChange;
                break;
            }
        }

        // no such packet - make new
        if (!tpkt) {
            if (npackets >= 1000) continue; // too many packets already - skip it
            NEWPACKET(SP_MultiBlockChange, mbc);
            packets[npackets++] = mbc;
            tpkt = tmbc;
            tmbc->X = X;
            tmbc->Z = Z;
        }

        // depending on the preview mode, select which block will be shown
        bid_t bid;
        switch (mode) {
            case PREVIEW_REMOVE:
                bid = b->current;
                break;
            case PREVIEW_MISSING:
                bid = PREVIEW_BLOCK;
                break;
            case PREVIEW_TRUE:
                bid = b->b;
                break;
        }

        // add new block to the packet
        lh_resize(tpkt->blocks, tpkt->count+1);
        tpkt->blocks[tpkt->count].x = b->x&15;
        tpkt->blocks[tpkt->count].z = b->z&15;
        tpkt->blocks[tpkt->count].y = b->y;
        tpkt->blocks[tpkt->count].bid = bid;
        tpkt->count++;
    }

    for(i=0; i<npackets; i++)
        queue_packet(packets[i], cq);

    printf("Created %d packets\n",npackets);
}





////////////////////////////////////////////////////////////////////////////////
// Canceling Build

// cancel building and completely erase the buildplan
void build_clear(MCPacketQueue *sq, MCPacketQueue *cq) {
    build_cancel(sq, cq);
    bplan_free(build.bp);
    lh_clear_obj(build);

    if (!buildopts.init)
        buildopt_setdefault();
}

// cancel and delete the buildtask, leaving the buildplan
void build_cancel(MCPacketQueue *sq, MCPacketQueue *cq) {
    if (!buildopts.preview_retain)
        build_show_preview(sq, cq, PREVIEW_REMOVE);
    build.active = 0;
    lh_arr_free(BTASK);
    build.bq[0] = -1;
    build.nbrp = 0; // clear the pending queue
}






////////////////////////////////////////////////////////////////////////////////
// Main build command dispatch

// fill the argdefaults struct with values that can be used as default parameters
static void get_argdefaults(arg_defaults *ad) {
    lh_clear_ptr(ad);

    ad->px = gs.own.x>>5;
    ad->pz = gs.own.z>>5;
    ad->py = gs.own.y>>5;
    ad->pd = player_direction();

    ad->mat = BLOCKTYPE(0,0);
    ad->mat2 = BLOCKTYPE(0,0);

    slot_t *s1 = &gs.inv.slots[gs.inv.held+36];
    slot_t *s2 = &gs.inv.slots[(gs.inv.held+1)%9+36];
    if (s1->item > 0 && !(ITEMS[s1->item].flags&I_ITEM))
        ad->mat = BLOCKTYPE(s1->item, s1->damage);
    if (s2->item > 0 && !(ITEMS[s2->item].flags&I_ITEM))
        ad->mat2 = BLOCKTYPE(s2->item, s2->damage);

    if (build.bp) {
        ad->bpsx = build.bp->sx;
        ad->bpsz = build.bp->sz;
        ad->bpsy = build.bp->sy;
    }
}

#define CMD(name) if (!strcmp(cmd, #name))
#define CMD2(name1,name2) if (!strcmp(cmd, #name1) || !strcmp(cmd, #name2))

#define NEEDBP if (!build.bp || !C(build.bp->plan)) {                       \
        sprintf(reply, "You need a non-empty buildplan for this command");  \
        goto Error;                                                         \
    }

static inline int arg_trim(char **words, int *value) {
    if (!words[0]) return TRIM_END;

    int type = 0;

    // parse coordinate name
    switch (words[0][0]) {
        case 'X': case 'x': type=0; break;
        case 'Y': case 'y': type=3; break;
        case 'Z': case 'z': type=6; break;
        default: return TRIM_UNK;
    }

    // parse comparison sign
    int pos=2, off=0;
    switch (words[0][1]) {
        case '=':
            type+=TRIM_XE;
            break;
        case '<':
            type+=TRIM_XL;
            if (words[0][2] == '=') {
                pos++;
                off=1;
            }
            break;
        case '>':
            type+=TRIM_XG;
            if (words[0][2] == '=') {
                pos++;
                off=-1;
            }
            break;
        default:
            return TRIM_UNK;
    }

    // parse value
    if (sscanf(words[0]+pos, "%d", value) != 1) return TRIM_UNK;
    *value += off;

    // remove the string from the wordlist if parsed successfully
    int i;
    for(i=0; words[i]; i++) words[i]=words[i+1];

    return type;
}

void build_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq) {
    char reply[32768];
    reply[0]=0;
    int rpos = 0; // reply position - 0:chat 2:pop-up

    char *cmd = words[1];
    words+=2;

    arg_defaults ad;
    get_argdefaults(&ad);
    int ARG_NOTFOUND=0;

    // possible arguments for the commands
    off3_t      off;
    pivot_t     pv;
    size3_t     sz;
    bid_t       mat,mat1,mat2;
    int         count;
    float       diam;

    char buf[256];

    if (!cmd) {
        sprintf(reply, "Usage: build <type> [ parameters ... ] or build cancel");
        goto Error;
    }

    // Parametric builds
    CMD2(floor,fl) {
        ARGREQ(size, NULL, sz);
        ARGMAT(NULL, mat, ad.mat);
        build_clear(sq, cq);
        build.bp = bplan_floor(sz.x, sz.z, mat);
        sprintf(reply, "Floor size=%d,%d material=%s",sz.x,sz.z,get_bid_name(buf, mat));
        goto Place;
    }

    CMD2(wall,wa) {
        ARGREQ(size, NULL, sz);
        ARGMAT(NULL, mat, ad.mat);
        build_clear(sq, cq);
        build.bp = bplan_wall(sz.x, sz.z, mat);
        sprintf(reply, "Wall size=%d,%d material=%s",sz.x,sz.z,get_bid_name(buf, mat));
        goto Place;
    }

    CMD2(disk,di) {
        ARGREQ(diam, NULL, diam);
        ARGMAT(NULL, mat, ad.mat);
        int edge = argflag(words, WORDLIST("edge","e"));
        build_clear(sq, cq);
        build.bp = bplan_disk(diam, mat, edge);
        sprintf(reply, "Disk diam=%f%s material=%s",
                diam,edge?"(edge) ":"",get_bid_name(buf, mat));
        goto Place;
    }

    CMD2(ball,ba) {
        ARGREQ(diam, NULL, diam);
        ARGMAT(NULL, mat, ad.mat);
        int edge = argflag(words, WORDLIST("edge","e"));
        build_clear(sq, cq);
        build.bp = bplan_ball(diam, mat, edge);
        sprintf(reply, "Ball diam=%f%s material=%s",
                diam,edge?"(edge) ":"",get_bid_name(buf, mat));
        goto Place;
    }

    CMD2(ring,ri) {
        ARGREQ(diam, NULL, diam);
        ARGMAT(NULL, mat, ad.mat);
        int edge = argflag(words, WORDLIST("edge","e"));
        build_clear(sq, cq);
        build.bp = bplan_disk(diam, mat, edge);
        bplan_hollow(build.bp, 1, 0);
        sprintf(reply, "Ring diam=%f%s material=%s",
                diam,edge?"(edge) ":"",get_bid_name(buf, mat));
        goto Place;
    }

    CMD2(sphere,sp) {
        ARGREQ(diam, NULL, diam);
        ARGMAT(NULL, mat, ad.mat);
        int edge = argflag(words, WORDLIST("edge","e"));
        build_clear(sq, cq);
        build.bp = bplan_ball(diam, mat, edge);
        bplan_hollow(build.bp, 0, 0);
        sprintf(reply, "Sphere diam=%f%s material=%s",
                diam,edge?"(edge) ":"",get_bid_name(buf, mat));
        goto Place;
    }

    CMD2(rectangle,rect) {
        ARGREQ(size, NULL, sz);
        ARGMAT(NULL, mat, ad.mat);
        build_clear(sq, cq);
        build.bp = bplan_floor(sz.x, sz.z, mat);
        bplan_hollow(build.bp, 1, 0);
        sprintf(reply, "Rectangle size=%dx%d material=%s",sz.x,sz.z,get_bid_name(buf, mat));
        goto Place;
    }

    CMD2(scaffolding,scaf) {
        bid_t dirt = BLOCKTYPE(3,0);
        ARGREQ(size, NULL, sz);
        ARGMAT(NULL, mat, dirt);
        build_clear(sq, cq);
        if (argflag(words, WORDLIST("ladder","l","2"))) {
            build.bp = bplan_scaffolding(sz.x, sz.z, mat,1);
            sprintf(reply, "Scaffolding (ladder) width=%d floors=%d material=%s",
                    sz.x,sz.z,get_bid_name(buf, mat));
        }
        else {
            if (sz.z==1 && sz.x<4) sz.x = 4;
            if (sz.z>1  && sz.x<7) sz.x = 7;
            build.bp = bplan_scaffolding(sz.x, sz.z, mat,0);
            sprintf(reply, "Scaffolding width=%d floors=%d material=%s",
                    sz.x,sz.z,get_bid_name(buf, mat));
        }
        goto Place;
    }

    CMD2(stairs,stair) {
        ARGREQ(size, NULL, sz);
        ARGMAT(NULL, mat, ad.mat);
        build_clear(sq, cq);
        int base = 1,ex=1;
        if (argflag(words, WORDLIST("none","n","bn"))) base=0;
        if (argflag(words, WORDLIST("minimal","min","m","bm"))) base=1;
        if (argflag(words, WORDLIST("full","f","bf"))) base=2;
        if (argflag(words, WORDLIST("exact","e"))) ex=-1;
        build.bp = bplan_stairs(sz.x, sz.z, mat, base*ex);
        char **BASE = WORDLIST("none","minimal","full");
        sprintf(reply, "Stairs width=%d floors=%d material=%s base=%s%s",
                sz.x,sz.z,get_bid_name(buf, mat),BASE[base],(ex<0)?" (exact)":"");
        goto Place;
    }

    // Buildplan manipulation
    CMD2(extend,ext) {
        NEEDBP;
        ARGREQ(offset, NULL, off);
        ARGDEF(count, NULL, count, 1);
        bplan_extend(build.bp, off.x, off.z, off.y, count);
        //TODO: clean up overlapping blocks
        sprintf(reply, "Extend offset=%d,%d,%d count=%d", off.x, off.z, off.y, count);
        goto Place;
    }

    CMD2(hollow,ho) {
        NEEDBP;
        int flat = argflag(words, WORDLIST("flat","2d","2","f","xz"));
        int opaque = !argflag(words, WORDLIST("opaque","o"));
        int removed = bplan_hollow(build.bp, flat, opaque);
        sprintf(reply, "Removed %d blocks, kept %zd",removed,C(build.bp->plan));
        goto Place;
    }

    CMD2(replace,re) {
        NEEDBP;
        char **mw1 = WORDLIST("material1","mat1","m1");
        char **mw2 = WORDLIST("material2","mat2","m2");
        ARGREQ(mat, mw1, mat1);
        ARGREQ(mat, mw2, mat2);
        int anymeta = argflag(words, WORDLIST("anymeta","a"));
        int count = bplan_replace(build.bp, mat1, mat2, anymeta);
        char buf2[256];
        if (mat2.bid == 0) {
            // blocks were removed
            sprintf(reply, "Removed %d blocks of %s\n", count,
                    get_bid_name(buf, mat1));
        }
        else {
            // blocks were replaced
            sprintf(reply, "Replaced %d blocks of %s with %s\n", count,
                    get_bid_name(buf, mat1), get_bid_name(buf2, mat2));
        }

        goto Place;
    }

    CMD(trim) {
        NEEDBP;
        int32_t value;
        int type;
        int removed = 0;
        while (type = arg_trim(words, &value)) {
            if (type < 0) {
                sprintf(reply, "Cannot parse trim constraint %s", words[0]);
                goto Error;
            }

            removed += bplan_trim(build.bp, type, value);
        }

        sprintf(reply, "Trim: removed %d blocks, retained %zd",
                removed, C(build.bp->plan));

        goto Place;
    }

    CMD(flip) {
        NEEDBP;
        char mode='x';
        if (words[0]) {
            switch (words[0][0]) {
                case 'x':
                case 'y':
                case 'z':
                    mode = words[0][0];
                    break;
                default:
                    sprintf(reply, "Usage: #build flip [x|y|z]");
                    goto Error;
            }
        }
        bplan_flip(build.bp, mode);
        sprintf(reply, "Buildplan flipped about %c axis",mode);
        goto Place;
    }

    CMD(tilt) {
        NEEDBP;
        char mode='x';
        if (words[0]) {
            switch (words[0][0]) {
                case 'x':
                case 'y':
                case 'z':
                    mode = words[0][0];
                    break;
                default:
                    sprintf(reply, "Usage: #build tilt [x|y|z]");
                    goto Error;
            }
        }
        bplan_tilt(build.bp, mode);
        sprintf(reply, "Buildplan tilted about %c axis",mode);
        goto Place;
    }

    CMD2(normalize,norm) {
        NEEDBP;
        bplan_normalize(build.bp);
        sprintf(reply, "Buildplan normalized");
        goto Place;
    }

    CMD(shrink) {
        NEEDBP;
        bplan_shrink(build.bp);
        sprintf(reply, "Shrunk to %zd blocks in a %dx%dx%d area\n",
                C(build.bp->plan), build.bp->sx, build.bp->sz, build.bp->sy);
        goto Place;
    }

    // Save/load/import
    CMD(save) {
        NEEDBP;
        if (!words[0]) {
            sprintf(reply, "You must specify a filename (w/o .bplan extension)");
            goto Error;
        }

        if (!bplan_save(build.bp, words[0]))
            sprintf(reply, "Error saving to %s.bplan",words[0]);
        else
            sprintf(reply, "Saved %zd blocks to %s.bplan\n",
                    C(build.bp->plan),words[0]);

        goto Error;
    }

    CMD(load) {
        if (!words[0]) {
            sprintf(reply, "You must specify a filename (w/o .bplan extension)");
            goto Error;
        }

        bplan *bp = bplan_load(words[0]);

        if (bp) {
            build_clear(sq, cq);
            build.bp = bp;
            sprintf(reply, "Loaded %zd blocks from %s.bplan\n", C(bp->plan),words[0]);
            goto Place;
        }

        sprintf(reply, "Failed to load %s.bplan\n",words[0]);
        goto Error;
    }

    CMD(ssave) {
        NEEDBP;
        if (!words[0]) {
            sprintf(reply, "You must specify a filename (w/o .schematic extension)");
            goto Error;
        }

        if (!bplan_ssave(build.bp, words[0]))
            sprintf(reply, "Error saving to %s.schematic",words[0]);
        else
            sprintf(reply, "Saved %zd blocks to %s.schematic\n",
                    C(build.bp->plan),words[0]);

        goto Error;
    }

    CMD(sload) {
        if (!words[0]) {
            sprintf(reply, "You must specify a filename (w/o .schematic extension)");
            goto Error;
        }

        bplan *bp = bplan_sload(words[0]);

        if (bp) {
            build_clear(sq, cq);
            build.bp = bp;
            sprintf(reply, "Loaded %zd blocks from %s.schematic\n", C(bp->plan),words[0]);
            goto Place;
        }

        sprintf(reply, "Failed to load %s.schematic\n",words[0]);
        goto Error;
    }

    CMD(csvsave) {
        NEEDBP;
        if (!words[0]) {
            sprintf(reply, "You must specify a filename (w/o .csv extension)");
            goto Error;
        }

        if (!bplan_csvsave(build.bp, words[0]))
            sprintf(reply, "Error saving to %s.csv",words[0]);
        else
            sprintf(reply, "Saved %zd blocks to %s.csv\n",
                    C(build.bp->plan),words[0]);

        goto Error;
    }

    CMD(csvload) {
        if (!words[0]) {
            sprintf(reply, "You must specify a filename (w/o .csv extension)");
            goto Error;
        }

        bplan *bp = bplan_csvload(words[0]);

        if (bp) {
            build_clear(sq, cq);
            build.bp = bp;
            sprintf(reply, "Loaded %zd blocks from %s.csv\n", C(bp->plan),words[0]);
            goto Place;
        }

        sprintf(reply, "Failed to load %s.csv\n",words[0]);
        goto Error;
    }

    CMD2(pngload,png) {
        if (!words[0]) {
            sprintf(reply, "You must specify a filename (w/o .png extension)");
            goto Error;
        }

        bplan *bp = bplan_pngload(words[0],words[1]);

        if (bp) {
            build_clear(sq, cq);
            build.bp = bp;
            sprintf(reply, "Loaded %zd blocks from %s.png\n", C(bp->plan),words[0]);
            goto Place;
        }

        sprintf(reply, "Failed to load %s.png\n",words[0]);
        goto Error;
    }

    CMD2(record,rec) {
        char *rcmd = words[0];
        if (!rcmd || !strcmp(rcmd, "start")) {
            build_clear(sq, cq);
            lh_alloc_obj(build.bp);
            build.recording = 1;
            lh_clear_obj(build.pv);
            sprintf(reply, "recording started, buildplan cleared");
            goto Error;
        }
        if (!strcmp(words[0],"stop")) {
            build.recording = 0;
            sprintf(reply, "recording stopped");
            goto Place;
        }
        if (!strcmp(words[0],"resume")) {
            build.recording = 1;
            if (build.pv.dir)
                sprintf(reply, "recording resumed, pivot at %d,%d,%d",
                        build.pv.pos.x,build.pv.pos.y,build.pv.pos.z);
            else {
                lh_alloc_obj(build.bp);
                sprintf(reply, "recording resumed, pivot not set");
            }
        }
    }

    CMD(scan) {
        ARGREQ(pivot, NULL, pv);
        ARGREQ(size, NULL,sz);
        //TODO: scanning from mask
        //TODO: specify the scan extend with to=<x,z,y>

        extent_t ex = ps2extent(pv, sz);
        cuboid_t c = export_cuboid_extent(ex);

        build_clear(sq, cq);
        lh_alloc_obj(build.bp);

        int x,y,z;
        for(y=0; y<c.sr.y; y++) {
            bid_t *slice = c.data[y]+c.boff;
            for(z=0; z<c.sr.z; z++) {
                bid_t *row = slice+z*c.sa.x;
                for(x=0; x<c.sr.x; x++) {
                    if (NOSCAN(row[x].bid)) continue;
                    blkr b = { x+ex.min.x, y+ex.min.y, z+ex.min.z, row[x] };
                    bplan_add(build.bp, abs2rel(pv, b));
                }
            }
        }
        sprintf(reply, "Scanned %zd blocks from %dx%dx%d area\n",
                C(build.bp->plan), sz.x, sz.z, sz.y);

        free_cuboid(c);

        goto Place;
    }

    // Build control
    CMD(place) {
        NEEDBP;
        if (words[0]) {
            if (!strcmp(words[0],"cancel")) {
                sprintf(reply, "Placement canceled");
                build.placemode = 0;
                goto Error;
            }
            if (!strcmp(words[0],"many")) {
                sprintf(reply, "Mark pivot positions by placing block, disable with #build place cancel");
                build.placemode = 2;
                goto Error;
            }
            if (!strcmp(words[0],"once")) {
                sprintf(reply, "Mark pivot position by placing block, will build once");
                build.placemode = 1;
                goto Error;
            }
            if (!strcmp(words[0],"again")) {
                if (!build.pv.dir)
                    sprintf(reply, "No pivot was set previously");
                else
                    place_pivot(build.pv, sq, cq);
                goto Error;
            }
        }
        ARG(pivot, NULL, pv);
        if (reply[0]) goto Error;
        if (ARG_NOTFOUND) {
            sprintf(reply, "Mark pivot position by placing block, will build once");
            build.placemode = 1;
            goto Error;
        }

        // abort current buildtask
        build_cancel(sq, cq);

        sprintf(reply, "Placing pivot at %d,%d (%d), dir=%s\n",
                pv.pos.x,pv.pos.z,pv.pos.y,DIRNAME[pv.dir]);
        place_pivot(pv, sq, cq);
        goto Error;
    }

    CMD(cancel) {
        build_cancel(sq, cq);
        build.placemode=0;
        goto Error;
    }

    CMD(pause) {
        if (P(build.task)) {
            build.active = !build.active;
            sprintf(reply, "Buildtask is %s", build.active?"unpaused":"paused");
        }
        else {
            sprintf(reply, "You need an existing buildtask to pause/unpause");
        }
        rpos=2;
        goto Error;
    }

    // Preview
    CMD(preview) {
        int mode = PREVIEW_MISSING;
        if (argflag(words, WORDLIST("true","t"))) mode=PREVIEW_TRUE;
        if (argflag(words, WORDLIST("remove","r"))) mode=PREVIEW_REMOVE;
        if (argflag(words, WORDLIST("missing","m"))) mode=PREVIEW_MISSING;
        build_show_preview(sq, cq, mode);
        goto Error;
    }

    // Debug
    CMD(dumpplan) {
        bplan_dump(build.bp);
        goto Error;
    }

    CMD(dumptask) {
        build_dump_task();
        goto Error;
    }

    CMD(dumpqueue) {
        build_dump_queue();
        goto Error;
    }

    CMD2(material,mat) {
        calculate_material(argflag(words, WORDLIST("plan","p")));
        goto Error;
    }

    CMD2(limit,li) {
        if (!words[0]) {
            // set the limit to player's current y position
            build.limit = gs.own.y>>5;
        }
        else {
            if (!strcmp(words[0],"none") || !strcmp(words[0],"-")) {
                build.limit = 0;
            }
            else {
                if (sscanf(words[0], "%d", &build.limit)!=1) {
                    sprintf(reply, "Usage: #build limit [y|-]");
                    build.limit = 0;
                }
            }
        }
        goto Error;
    }

    CMD2(opt,set) {
        buildopt(words, cq);
        goto Error;
    }

    sprintf(reply, "Unrecognized command");
    goto Error;

 Place:
    bplan_update(build.bp);
    build.placemode = buildopts.placemode; // initiate placing
    size_t rlen = strlen(reply);
    switch(build.placemode) {
        case 1:
            sprintf(reply+rlen, ",placing once");
            break;
        case 2:
            sprintf(reply+rlen, ",placing many");
            break;
    }

 Error:
    if (reply[0]) chat_message(reply, cq, "green", rpos);
}

////////////////////////////////////////////////////////////////////////////////

// dispatch for the building-relevant packets we get from mcp_game
int build_packet(MCPacket *pkt, MCPacketQueue *sq, MCPacketQueue *cq) {
    if (pkt->pid == SP_UpdateHealth && gs.own.health < 20) {
        if (build.active) {
            build.active = 0;
            chat_message("INJURY!!! Buildtask paused!", cq, "green", 2);
        }
    }

    if (build.recording) {
        switch (pkt->pid) {
            case SP_BlockChange:
            case SP_MultiBlockChange:
                brec_blockupdate(pkt);
                break;
            case CP_PlayerBlockPlacement:
                brec_blockplace(pkt);
                break;
        }
        return 1;
    }
    else if (build.placemode && pkt->pid == CP_PlayerBlockPlacement) {
        build_placemode(pkt, sq, cq);
        return 0; // do not forward the packet to the server
    }

    return 1;
}
