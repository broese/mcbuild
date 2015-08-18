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
    int wallmode;              // if nonzero - do not build blocks higher than your own position
    int sealmode;              // if nonzero - only build blocks behind your back
    int anyface;               // attempt to build on any faces, even those looking away from you
                               // this type of building will work on vanilla servers, but may be
                               // blocked on some, including 2b2t
    int limit;                 // build height limit, disabled if zero

    lh_arr_declare(blk,task);  // current active building task
    bplan *bp;                 // currently loaded/created buildplan

    blkr brp[MAXBUILDABLE];    // records the 'pending' blocks from the build recorder
    ssize_t nbrp;              // we add blocks as we place them (CP_PlayerBlockPlacement)
                               // and remove them as we get the confirmation from the server
                               // (SP_BlockChange or SP_MultiBlockChange)

    int32_t px,py,pz;          // BREC pivot block
    int pd;                    // BREC pivot direction
    int pivotset;              // nonzero if pivot block has been set

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
    { "placemode", "default placement behavior",                        &buildopts.placemode, 1},
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
static int find_evictable_slot() {
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

#if 0
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
        for (i=0; i<C(build.plan); i++) {
            bid_t bmat = get_base_material(P(build.plan)[i].b);
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
#endif






////////////////////////////////////////////////////////////////////////////////
// Building process

#define SQ(x) ((x)*(x))
#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

// maximum reach distance for building, squared, in fixp units (1/32 block)
#define MAXREACH_COARSE SQ(5<<5)
#define MAXREACH SQ(4<<5)
#define OFF(x,z,y) (((x)-xo)+((z)-zo)*(xsz)+((y)-yo)*(xsz*zsz))

// block types that are considered 'empty' for the block placement
static inline int ISEMPTY(int bid) {
    return ( bid==0x00 ||               // air
             bid==0x08 || bid==0x09 ||  // water
             bid==0x0a || bid==0x0b ||  // lava
             bid==0x1f ||               // tallgrass
             bid==0x33 ||               // fire
             bid==0x4e                  // snow layer
    );
}

// block types we should exclude from scanning
static inline int NOSCAN(int bid) {
    return ( bid==0x00 ||               // air
             bid==0x08 || bid==0x09 ||  // water
             bid==0x0a || bid==0x0b ||  // lava
             bid==0x1f ||               // tallgrass
             bid==0x22 ||               // piston head
             bid==0x24 ||               // piston extension
             bid==0x33 ||               // fire
             bid==0x3b ||               // wheat
             bid==0x4e ||               // snow layer
             bid==0x5a ||               // portal field
             //bid==0x63 || bid==0x64 || // giant mushrooms
             bid==0x8d || bid==0x8e     // carrots, potatoes
             );
}

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
        if (!((b->neigh>>f)&1)) continue; // no neighbor - skip this face
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

    // offset coords of the cuboid
    int32_t Xo = (build.xmin-1)>>4;
    int32_t Zo = (build.zmin-1)>>4;
    int32_t xo = Xo<<4;
    int32_t zo = Zo<<4;
    int32_t yo = build.ymin-1;

    // cuboid size
    int32_t Xsz = ((build.xmax+1)>>4)-Xo+1;
    int32_t Zsz = ((build.zmax+1)>>4)-Zo+1;
    int32_t xsz = Xsz<<4;
    int32_t zsz = Zsz<<4;
    int32_t ysz = build.ymax-build.ymin+3;

    bid_t * world = export_cuboid(Xo, Xsz, Zo, Zsz, yo, ysz, NULL);

    int i;
    for(i=0; i<C(build.task); i++) {
        blk *b = P(build.task)+i;
        bid_t bl = world[OFF(b->x,b->z,b->y)];
        b->placed = (bl.raw == b->b.raw);
        b->empty  = ISEMPTY(bl.bid) && !b->placed;
        b->current = bl;
    }
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
        b->inreach = 1;

        // avoid building blocks above the player in wall and limit mode
        if ((build.wallmode && (b->y>(gs.own.y>>5)-1)) ||
            (build.limit && (b->y>build.limit))) {
                b->inreach = 0;
                continue;
        }

        // if the "seal mode" is active, only build blocks located in front of you
        if (build.sealmode) {
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

    //TODO: try to limit the extracted area to a bare minimum of
    //      the reachable blocks, to limit the amount of data

    // offset coords of the cuboid
    int32_t Xo = (build.xmin-1)>>4;
    int32_t Zo = (build.zmin-1)>>4;
    int32_t xo = Xo<<4;
    int32_t zo = Zo<<4;
    int32_t yo = build.ymin-1;

    // cuboid size
    int32_t Xsz = ((build.xmax+1)>>4)-Xo+1;
    int32_t Zsz = ((build.zmax+1)>>4)-Zo+1;
    int32_t xsz = Xsz<<4;
    int32_t zsz = Zsz<<4;
    int32_t ysz = build.ymax-build.ymin+3;

    bid_t * world = export_cuboid(Xo, Xsz, Zo, Zsz, yo, ysz, NULL);

    // 3. determine which blocks are occupied and which neighbors are available
    build.nbq = 0;
    for(i=0; i<C(build.task); i++) {
        blk *b = P(build.task)+i;
        b->rdir = DIR_ANY;
        if (!b->inreach) continue;

        const item_id *it = &ITEMS[b->b.bid];

        bid_t bl = world[OFF(b->x,b->z,b->y)];

        // check if this block is already correctly placed (including meta)
        //TODO: implement less restricted check for blocks with non-positional meta
        b->placed = (bl.raw == b->b.raw);

        // check if the block is empty, but ignore those that are already
        // placed - this way we can support "empty" blocks like water in our buildplan
        b->empty  = ISEMPTY(bl.bid) && !b->placed;

        // determine which neighbors do we have
        b->n_yp = !ISEMPTY(world[OFF(b->x,b->z,b->y+1)].bid);
        b->n_yn = !ISEMPTY(world[OFF(b->x,b->z,b->y-1)].bid);
        b->n_xp = !ISEMPTY(world[OFF(b->x+1,b->z,b->y)].bid);
        b->n_xn = !ISEMPTY(world[OFF(b->x-1,b->z,b->y)].bid);
        b->n_zp = !ISEMPTY(world[OFF(b->x,b->z+1,b->y)].bid);
        b->n_zn = !ISEMPTY(world[OFF(b->x,b->z-1,b->y)].bid);
        //TODO: skip faces looking away from us

        // skip the blocks we can't place
        if (b->placed || !b->empty || !b->neigh) continue;

        // determine usable dots on the neighbor faces
        lh_clear_obj(b->dots);

        //TODO: provide support for ALL position-dependent blocks
        if (it->flags&I_SLAB) {
            // Slabs
            if (b->b.meta&8) // upper half placement
                setdots(b, DOTS_ALL, DOTS_NONE, DOTS_UPPER, DOTS_UPPER, DOTS_UPPER, DOTS_UPPER);
            else // lower half placement
                setdots(b, DOTS_NONE, DOTS_ALL, DOTS_LOWER, DOTS_LOWER, DOTS_LOWER, DOTS_LOWER);
        }
        else if (it->flags&I_STAIR) {
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
        else if (it->flags&I_LOG) {
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
        else if (it->flags&I_TORCH) {
            switch(b->b.meta) {
                case 1: //east
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL);
                    break;
                case 2: //west
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE);
                    break;
                case 3: //south
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE, DOTS_NONE);
                    break;
                case 4: //north
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE, DOTS_NONE, DOTS_NONE);
                    break;
                case 5: //ground
                    setdots(b, DOTS_NONE, DOTS_ALL, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE);
                    break;
                default:
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE);
                    break;
            }
        }
        else if (it->flags&I_ONWALL) {
            switch(b->b.meta) {
                case 2: //north
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE, DOTS_NONE, DOTS_NONE);
                    break;
                case 3: //south
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE, DOTS_NONE);
                    break;
                case 4: //west
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL, DOTS_NONE);
                    break;
                case 5: //east
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_ALL);
                    break;
                default:
                    setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE);
                    break;
            }
        }
        else {
            // Blocks that don't have I_MPOS or not supported
            setdots(b, DOTS_ALL, DOTS_ALL, DOTS_ALL, DOTS_ALL, DOTS_ALL, DOTS_ALL);
        }

        // disable the faces where there is no neighbor
        for (f=0; f<6; f++)
            if (!((b->neigh>>f)&1))
                memset(b->dots[f], 0, sizeof(DOTS_ALL));

        // disable faces looking away from you
        if (!build.anyface) {
            if (b->y < (gs.own.y>>5)+1) memset(b->dots[DIR_UP], 0, sizeof(DOTS_ALL));
            if (b->y > (gs.own.y>>5)+2) memset(b->dots[DIR_DOWN], 0, sizeof(DOTS_ALL));
            if (b->x < (gs.own.x>>5)) memset(b->dots[DIR_EAST], 0, sizeof(DOTS_ALL));
            if (b->x > (gs.own.x>>5)) memset(b->dots[DIR_WEST], 0, sizeof(DOTS_ALL));
            if (b->z < (gs.own.z>>5)) memset(b->dots[DIR_SOUTH], 0, sizeof(DOTS_ALL));
            if (b->z > (gs.own.z>>5)) memset(b->dots[DIR_NORTH], 0, sizeof(DOTS_ALL));
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

    free(world);
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

    uint64_t ts = gettimestamp();

    if (ts < build.lastbuild+buildopts.bldint) return;

    int i, bc=0;
    int held=gs.inv.held;

    for(i=0; i<build.nbq && bc<buildopts.blkmax; i++) {
        blk *b = P(build.task)+build.bq[i];
        if (ts-b->last < buildopts.blkint) continue;

        // fetch block's material into quickbar slot
        int islot = prefetch_material(sq, cq, get_base_material(b->b));
        if (islot<0) continue; // we don't have this material
        //TODO: notify user about missing materials

        // silently switch to this slot
        gmi_change_held(sq, cq, islot, 0);
        slot_t * hslot = &gs.inv.slots[islot+36];

        char buf[4096];

        int8_t face, cx, cy, cz;
        choose_dot(b, &face, &cx, &cy, &cz);

        // dot's absolute 3D coordinates (fixed point)
        fixp tx = ((b->x+NOFF[face][0])<<5) + cx*2;
        fixp tz = ((b->z+NOFF[face][1])<<5) + cz*2;
        fixp ty = ((b->y+NOFF[face][2])<<5) + cy*2;

        float yaw, pitch;
        int ldir = calculate_yaw_pitch(tx, tz, ty, &yaw, &pitch);

        printf("Placing Block: %d,%d,%d (%s)  On: %d,%d,%d  "
               "Face:%d Cursor:%d,%d,%d  "
               "Player: %.1f,%.1f,%.1f  Dot: %.1f,%.1f,%.1f  "
               "Rot=%.2f,%.2f  Dir=%d (%s)\n",
               b->x,b->y,b->z, get_item_name(buf, hslot),
               b->x+NOFF[face][0],b->z+NOFF[face][1],b->y+NOFF[face][2],
               face, cx, cy, cz,
               (float)gs.own.x/32, (float)(gs.own.y+EYEHEIGHT)/32, (float)gs.own.z/32,
               (float)b->x+(float)cx/16,(float)b->y+(float)cy/16,(float)b->z+(float)cz/16,
               yaw, pitch, ldir, DIRNAME[ldir]);

        // turn player look to the dot
        NEWPACKET(CP_PlayerLook, pl);
        tpl->yaw = yaw;
        tpl->pitch = pitch;
        tpl->onground = gs.own.onground;
        queue_packet(pl,sq);

        // place block
        NEWPACKET(CP_PlayerBlockPlacement, pbp);
        tpbp->bpos.x = b->x+NOFF[face][0];
        tpbp->bpos.z = b->z+NOFF[face][1];
        tpbp->bpos.y = b->y+NOFF[face][2];
        tpbp->face = face;
        tpbp->cx = cx;
        tpbp->cy = cy;
        tpbp->cz = cz;
        queue_packet(pbp,sq);
        //dump_packet(pbp);

        // Wave arm
        NEWPACKET(CP_Animation, anim);
        queue_packet(anim, sq);

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
        gmi_change_held(sq, cq, held, 0);
}






////////////////////////////////////////////////////////////////////////////////
// Buildplan updates/calculations

// invoked by various functions when a buildplan is crated, so it can be placed immediately
static void buildplan_place(char *reply) {
#if 0
    switch (buildopts.placemode) {
        case 1:
            sprintf(reply, "Mark pivot position by placing a block - will be build once");
            break;
        case 2:
            sprintf(reply, "Mark pivot positions by placing block, disable with #build place cancel");
            break;
    }
#endif
    build.placemode = buildopts.placemode;
}




////////////////////////////////////////////////////////////////////////////////
// Helpers for parameter parsing

#if 0
static bid_t build_parse_material(char **words, int argpos, char *reply) {
    bid_t mat;

    if (mcparg_parse_material(words, argpos, reply, &mat, NULL)==0) {
        if (reply[0]) return BLOCKTYPE(0,0); // parsing failed

        // no material was specified on command line, so use what
        // the player is currently holding in hand
        slot_t *s = &gs.inv.slots[gs.inv.held+36];
        if (s->item > 0 && !(ITEMS[s->item].flags&I_ITEM)) {
            mat = BLOCKTYPE(s->item, s->damage);
        }
        else {
            // player holding nothing or a non-placeable item
            mat = BLOCKTYPE(0,0);
            sprintf(reply,
                    "You must specify material - either explicitly with "
                    "mat=<bid>[,<meta>] or by holding a placeable block");
        }
    }

    return mat;
}

// parse commandline to get the offset parameter
static boff_t build_arg_offset(char **words, char *reply, int argpos) {
    boff_t off = { .dx = build.bpsx, .dy = build.bpsy, .dz = build.bpsz };

    if (mcparg_parse_offset(words, argpos, reply, &off)==0) {
        if (!reply[0]) sprintf(reply, "You must specify offset as offset=<x>[,<z>[,<y>]]|<direction>");
        return (boff_t) { 0, 0, 0 };
    }

    return off;
}
#endif

// parse the commandline to get the direction parameter,
// or assume the direction the player is facing
static int build_arg_dir(char **words, char *reply, int argpos) {
    int dir = player_direction();
    if (mcparg_parse_direction(words, argpos, reply, &dir)==0) return DIR_ANY;

    reply[0] = 0;
    return dir;
}

#if 0
////////////////////////////////////////////////////////////////////////////////
// Parametric structures

static void build_floor(char **words, char *reply) {
    build_clear();

    // floor size
    int wd,hg;
    if (!mcparg_parse_size(words, 0, reply, &wd, &hg, NULL)) {
        sprintf(reply, "Usage: build floor size=<width>[,<depth>]");
        return;
    }

    bid_t mat = build_parse_material(words, 1, reply);
    if (reply[0]) return;

    int hollow = mcparg_find(words, "hollow", "rect", "empty", "e", NULL);

    int x,z;
    for(x=0; x<wd; x++) {
        for(z=0; z<hg; z++) {
            if (hollow && x!=0 && x!=wd-1 && z!=0 && z!=hg-1) continue;
            blkr *b = lh_arr_new(BPLAN);
            b->b = mat;
            b->x = x;
            b->z = -z;
            b->y = 0;
        }
    }

    char buf[256];
    int off = sprintf(reply, "Created floor %s%dx%d material=%s",
                      hollow?"(border only) ":"", wd, hg, get_bid_name(buf, mat));

    buildplan_updated();
    buildplan_place(reply);
}

// add a single block to the buildplan
#define PLACE_DOT(_x,_z)                            \
    b = lh_arr_new(BPLAN); b->b = mat; b->y = 0;    \
    b->x = (_x);  b->z = (_z);

static void build_ring(char **words, char *reply) {
    build_clear();

    // ring diameter
    //TODO: move to the disc implementation + 2D hollow to generate rings
    int diam;
    if (!mcparg_parse_size(words, 0, reply, &diam, NULL, NULL)) {
        sprintf(reply, "Usage: build ring size=<diameter>");
        return;
    }

    bid_t mat = build_parse_material(words, 1, reply);
    if (reply[0]) return;

    int x,z,o;
    blkr *b;
    if (diam&1) { // odd diameter - a block in the center
        int r=diam/2;
        for(x=0,z=r; x<z; x++) {
            int err1=abs(SQ(x)+SQ(z)-SQ(r));
            int err2=abs(SQ(x)+SQ(z-1)-SQ(r));
            //printf("x=%d z=%d err1=%d err2=%d   =>  ",x,z,err1,err2);
            if (err2<err1) z--;
            //printf("DOT %d,%d\n",x,z);

            // distribute the calculated dot to all octants
            PLACE_DOT(x,z);
            PLACE_DOT(x,-z);
            if (x!=z) {
                PLACE_DOT(z,x);
                PLACE_DOT(-z,x);
            }

            if (x!=0) {
                PLACE_DOT(-x,z);
                PLACE_DOT(-x,-z);
                if (x!=z) {
                    PLACE_DOT(z,-x);
                    PLACE_DOT(-z,-x);
                }
            }
        }
    }
    else {
        int r=diam-1;
        for(x=0,z=r/2; x<z; x++) {
            int err1=abs(SQ(2*x)+SQ(2*z)-SQ(r));
            int err2=abs(SQ(2*x)+SQ(2*(z-1))-SQ(r));
            //printf("x=%d z=%d err1=%d err2=%d   =>  ",x,z,err1,err2);
            if (err2<err1) z--;
            //printf("DOT %d,%d\n",x,z);

            // distribute the calculated dot to all octants
            PLACE_DOT(x,z);
            PLACE_DOT(x,-z-1);
            if (x!=z) {
                PLACE_DOT(z,x);
                PLACE_DOT(-z-1,x);
            }

            if (x!=0) {
                PLACE_DOT(-x-1,z);
                PLACE_DOT(-x-1,-z-1);
                if (x!=z) {
                    PLACE_DOT(z,-x-1);
                    PLACE_DOT(-z-1,-x-1);
                }
            }
        }
    }

    char buf[256];
    int off = sprintf(reply, "Created ring diam=%d material=%s",
                      diam, get_bid_name(buf, mat));

    buildplan_updated();
    buildplan_place(reply);
}

static void build_ball(char **words, char *reply) {
    build_clear();

    // ball diameter
    //TODO: ellipsoid
    int diam;
    if (!mcparg_parse_size(words, 0, reply, &diam, NULL, NULL)) {
        sprintf(reply, "Usage: build ball size=<diameter>");
        return;
    }

    bid_t mat = build_parse_material(words, 1, reply);
    if (reply[0]) return;

    int x,y,z,min,max;
    blkr *b;
    float c;

    if (diam&1) { // odd diameter - a block in the center
        max = diam/2;
        min = -max;
        c=0.0;
    }
    else {
        max = diam/2-1;
        min = -max-1;
        c=-0.5;
    }

    for(y=min; y<=max; y++) {
        for(x=min; x<=max; x++) {
            for(z=min; z<=max; z++) {
                float sqdist = SQ((float)x-c)+SQ((float)y-c)+SQ((float)z-c);
                if (sqdist < SQ(((float)diam)/2)) {
                    b = lh_arr_new(BPLAN);
                    b->b = mat;
                    b->y = y;
                    b->x = x;
                    b->z = z;
                }
            }
        }
    }

    char buf[256];
    int off = sprintf(reply, "Created ball diam=%d material=%s",
                      diam, get_bid_name(buf, mat));

    buildplan_updated();
    buildplan_place(reply);
}

static void build_disk(char **words, char *reply) {
    build_clear();

    // disc diameter
    //TODO: ellipse, ring/disk
    int diam;
    if (!mcparg_parse_size(words, 0, reply, &diam, NULL, NULL)) {
        sprintf(reply, "Usage: build disk size=<diameter>");
        return;
    }

    bid_t mat = build_parse_material(words, 1, reply);
    if (reply[0]) return;

    int x,z,min,max;
    blkr *b;
    float c;

    if (diam&1) {
        // odd diameter - pivot block is in the center
        max = diam/2;
        min = -max;
        c=0.0;
    }
    else {
        // even diameter - pivot is the SE block of the 4 in the center
        max = diam/2-1;
        min = -max-1;
        c=-0.5;
    }

    for(x=min; x<=max; x++) {
        for(z=min; z<=max; z++) {
            float sqdist = SQ((float)x-c)+SQ((float)z-c);
            if (sqdist < SQ(((float)diam)/2)) {
                b = lh_arr_new(BPLAN);
                b->b = mat;
                b->y = 0;
                b->x = x;
                b->z = z;
            }
        }
    }

    char buf[256];
    int off = sprintf(reply, "Created disk diam=%d material=%s",
                      diam, get_bid_name(buf, mat));

    buildplan_updated();
    buildplan_place(reply);
}

static int SCAFF_STAIR[5][2] = { { 1, -1}, { 2, -1 }, { 2, 0 }, { 3, 0 }, { 3, 1 } };

static void build_scaffolding(char **words, char *reply) {
    build_clear();

    int wd,hg;
    if (!mcparg_parse_size(words, 0, reply, &wd, &hg, NULL)) {
        sprintf(reply, "Usage: build stairs size=<width>[,<floors>]");
        return;
    }

    // ensure minimum width so we can connect the stairs
    if (hg==1 && wd<4) wd=4;
    if (hg>1 && wd<7) wd=7;

    bid_t mat = build_parse_material(words, 1, reply);
    if (reply[0]) {
        reply[0]=0;
        mat = BLOCKTYPE(3,0);
    }

    //TODO: use a secondary material to build stairs, set meta=0 for stair-type blocks

    int floor;
    for(floor=0; floor<hg; floor++) {
        int i;
        blkr *b;
        // bridge
        for(i=0; i<wd; i++) {
            b = lh_arr_new(BPLAN);
            b->b = mat;
            b->x = i;
            b->z = 0;
            b->y = 2+3*floor;
        }
        // column
        for(i=0; i<2; i++) {
            b = lh_arr_new(BPLAN);
            b->b = mat;
            b->x = 0;
            b->z = 0;
            b->y = i+3*floor;
        }
        // stairs
        for(i=0; i<5; i++) {
            b = lh_arr_new(BPLAN);
            b->b = mat;
            b->x = SCAFF_STAIR[i][0]+3*(floor&1);
            b->z = 1;
            b->y = SCAFF_STAIR[i][1]+3*floor;
        }
    }

    char buf[256];
    sprintf(reply, "Created scaffolding: %d floors, %d blocks wide, material=%s",
            hg, wd, get_bid_name(buf, mat));
    buildplan_updated();
    buildplan_place(reply);
}

static void build_stairs(char **words, char *reply) {
    build_clear();

    // stairs size
    int wd,hg;
    if (!mcparg_parse_size(words, 0, reply, &wd, &hg, NULL)) {
        sprintf(reply, "Usage: build stairs size=<width>[,<height>] mat=<material> [m|n|f]");
        return;
    }

    bid_t mat = build_parse_material(words, 1, reply);
    if (reply[0]) return;

    // make stairs-type block face player
    if ((ITEMS[mat.bid].flags&I_STAIR)) mat.meta=3;

    int base=1; // default=minimal base
    if (mcparg_find(words, "nobase", "none", "no", "nb", "n", NULL)) {
        base=0;
    }
    else if (mcparg_find(words, "minbase", "min", "mb", "m", NULL)) {
        base=1;
    }
    else if (mcparg_find(words, "fullbase", "full", "fb", "f", NULL)) {
        base=2;
    }
    //TODO: different material for the stairs base,
    //      or even automatically match material
    //      (e.g. Sandstone stairs => Sandstone)

    int floor;
    for(floor=0; floor<hg; floor++) {
        int x;
        blkr *b;
        for(x=0; x<wd; x++) {
            b = lh_arr_new(BPLAN);
            b->b = mat;
            b->x = x;
            b->z = -floor;
            b->y = floor;

            if (floor!=(hg-1) && (base==2 || (base==1 && x==0))) {
                b = lh_arr_new(BPLAN);
                b->b = mat;
                b->x = x;
                b->z = -floor-1;
                b->y = floor;
            }
        }
    }

    sprintf(reply, "Created stairs: %d floors, %d blocks wide, base=%s",
            hg, wd, base ? ( (base==1) ? "minimal" : "full" ) : "none");
    buildplan_updated();
    buildplan_place(reply);
}

static void build_wall(char **words, char *reply) {
    build_clear();

    // wall size
    int wd,hg;
    if (!mcparg_parse_size(words, 0, reply, &wd, &hg, NULL)) {
        sprintf(reply, "Usage: build wall size=<width>[,<height>]");
        return;
    }

    bid_t mat = build_parse_material(words, 1, reply);
    if (reply[0]) return;

    int x,y;
    for(y=0; y<hg; y++) {
        for(x=0; x<wd; x++) {
            blkr *b = lh_arr_new(BPLAN);
            b->b = mat;
            b->x = x;
            b->z = 0;
            b->y = y;
        }
    }

    char buf[256];
    int off = sprintf(reply, "Created wall %dx%d material=%s",
                      wd, hg, get_bid_name(buf, mat));

    buildplan_updated();
    buildplan_place(reply);
}

////////////////////////////////////////////////////////////////////////////////

// create a buildplan from existing map data by scanning a defined cuboid
static void build_scan(char **words, char *reply) {
    build_clear();

    // pivot block position
    int px,py,pz;
    mcpopt opt_from = {{"from","pivot","f"}, 0, {"%d,%d,%d",NULL}};
    if (mcparg_parse(words, &opt_from, &px, &pz, &py)<0) {
        if (mcparg_find(words, "here", "h", "player", "p", NULL)) {
            px = gs.own.x>>5;
            py = gs.own.y>>5;
            pz = gs.own.z>>5;
        }
        else {
            sprintf(reply, "Usage: specify pivot position either with 'from=<x,z,y>' or with 'here'");
            return;
        }
    }

    // pivot direction
    int dir = build_arg_dir(words, reply, 2);
    if (dir<0 || reply[0]) return;

    // opposize point
    int tx,ty,tz;
    mcpopt opt_to = {{"to","t"}, 0, {"%d,%d,%d",NULL}};
    if (mcparg_parse(words, &opt_to, &tx, &tz, &ty)<0) {
        // alternatively the user can specify the size
        int wd,hg,dp;
        mcpopt opt_size = {{"size","sz","s"}, -1, {"%d,%d,%d",NULL}};
        if (mcparg_parse(words, &opt_size, &wd, &dp, &hg)<0) {
            sprintf(reply, "Usage: specify the extent of the scan either with 'to=<x,z,y>' or with 'size=<width,depth,height>'");
            return;
        }

        wd--; hg--; dp--;
        switch (dir) {
            case DIR_NORTH: tx=px+wd; tz=pz-dp; break;
            case DIR_SOUTH: tx=px-wd; tz=pz+dp; break;
            case DIR_EAST:  tx=px+dp; tz=pz+wd; break;
            case DIR_WEST:  tx=px-dp; tz=pz-wd; break;
        }
        ty=py+hg;
    }

    // calculate the extent of the scan independent from direction
    int minx = MIN(px,tx); int maxx=MAX(px,tx);
    int miny = MIN(py,ty); int maxy=MAX(py,ty);
    int minz = MIN(pz,tz); int maxz=MAX(pz,tz);

    //printf("%d,%d,%d - %d,%d,%d, pivot:%d,%d,%d, dir:%s\n",minx,minz,miny,maxx,maxz,maxy,px,pz,py,DIRNAME[dir]);

    // offset coords of the cuboid
    int32_t Xo = minx>>4;
    int32_t Zo = minz>>4;
    int32_t xo = Xo<<4;
    int32_t zo = Zo<<4;
    int32_t yo = miny;

    // cuboid size
    int32_t Xsz = (maxx>>4)-Xo+1;
    int32_t Zsz = (maxz>>4)-Zo+1;
    int32_t xsz = Xsz<<4;
    int32_t zsz = Zsz<<4;
    int32_t ysz = maxy-miny+1;

    // obtain the cuboid containing our scan region
    bid_t * world = export_cuboid(Xo, Xsz, Zo, Zsz, yo, ysz, NULL);

    int x,y,z;
    for(y=miny; y<=maxy; y++) {
        for(z=minz; z<=maxz; z++) {
            for(x=minx; x<=maxx; x++) {
                bid_t bl = world[OFF(x,z,y)];
                if (NOSCAN(bl.bid)) continue;

                blkr *b = lh_arr_new(BPLAN);
                b->y = y-py;

                switch (dir) {
                    case DIR_NORTH:
                        b->x = x-px;
                        b->z = z-pz;
                        b->b = rotate_meta(bl, 0);
                        break;
                    case DIR_SOUTH:
                        b->x = px-x;
                        b->z = pz-z;
                        b->b = rotate_meta(bl, -2);
                        break;
                    case DIR_EAST:
                        b->x = z-pz;
                        b->z = px-x;
                        b->b = rotate_meta(bl, -1);
                        break;
                    case DIR_WEST:
                        b->x = pz-z;
                        b->z = x-px;
                        b->b = rotate_meta(bl, -3);
                        break;
                }
            }
        }
    }

    sprintf(reply, "Scanned %zd blocks from a %dx%dx%d area\n", C(build.plan), maxx-minx+1, maxz-minz+1, ysz);
    buildplan_updated();
    buildplan_place(reply);
}



////////////////////////////////////////////////////////////////////////////////
// Buildplan manipulation

/*
Extend the buildplan by replicating it a given number of times.

#build extend <offset> [<count>]
<offset>    := [offset=]<x>[,<z>[,<y>]]|<direction> # explicit offset, or direction
<direction> := [distance](u|d|r|l|f|b)              # offset by buildplan size in that direction or by explicit number
<count>     := [count=]<count>                      # how many times, default=1
*/
static void build_extend(char **words, char *reply) {
    boff_t o = build_arg_offset(words, reply, 0);
    if (reply[0]) return;

    mcpopt opt_count  = {{"count","cnt","c"},     1, {"%d",NULL}};
    int count;
    if (mcparg_parse(words, &opt_count, &count)<0)
        count=1;

    int i,j;
    int bc=C(build.plan);
    for(i=1; i<=count; i++) {
        for(j=0; j<bc; j++) {
            blkr *bn = lh_arr_new(BPLAN);
            blkr *bo = P(build.plan)+j;
            bn->x = bo->x+o.dx*i;
            bn->y = bo->y+o.dy*i;
            bn->z = bo->z+o.dz*i;
            bn->b = bo->b;
        }
    }

    sprintf(reply, "Extended buildplan %d times, offset=%d,%d,%d",
            count, o.dx, o.dz, o.dy);
    buildplan_updated();
    buildplan_place(reply);
}

// replace one material in the buildplan with another (including meta specification)
static void build_replace(char **words, char *reply) {
    bid_t mat1,mat2;
    int specified = 0;

    if ( mcparg_parse_material(words, 0, reply, &mat1, "1")==0 ) {
        if (reply[0]) return;
        sprintf(reply, "Usage: #build replace mat1=<mat1> mat2=<mat2>");
        return;
    }
    if ( mcparg_parse_material(words, 1, reply, &mat2, "2")==0 ) {
        if (reply[0]) return;
        sprintf(reply, "Usage: #build replace mat1=<mat1> mat2=<mat2>");
        return;
    }
    // TODO: handle material replacement for the orientation-dependent metas

    // if mat2=Air, blocks will be removed.
    // this array will store all blocks we keep
    lh_arr_declare_i(blkr, keep);
    int removed = 0;

    int i, count=0;
    for(i=0; i<C(build.plan); i++) {
        if (P(build.plan)[i].b.raw == mat1.raw) {
            if (mat2.raw != 0) {
                blkr *k = lh_arr_new(GAR(keep));
                *k = P(build.plan)[i];
                k->b = mat2;
                count++;
            }
            else {
                removed++;
            }
        }
        else {
            blkr *k = lh_arr_new(GAR(keep));
            *k = P(build.plan)[i];
        }
    }

    // replace the buildplan with the reduced list
    lh_arr_free(BPLAN);
    C(build.plan) = C(keep);
    P(build.plan) = P(keep);

    char buf1[256], buf2[256];
    sprintf(reply, "Replaced %d blocks %d:%d (%s) to %d:%d (%s), removed %d", count,
            mat1.bid, mat1.meta, get_bid_name(buf1, mat1),
            mat2.bid, mat2.meta, get_bid_name(buf2, mat2), removed);

    buildplan_updated();
    buildplan_place(reply);
}

// hollow out the structure by removing all blocks completely surrounded by other blocks.
static void build_hollow(char **words, char *reply) {
    int i,j;

    // size of a single "slice" with additional 1-block border
    int32_t size_xz = (build.bpsx+2)*(build.bpsz+2);

    // this array will hold the entire buildplan as a "voxel set"
    // with values set to 0 for empty blocks, 1 for occupied and -1 for removed
    // note - we leave an additional empty block for the border on each side
    lh_create_num(int8_t,bp,size_xz*(build.bpsy+2));

    // initialize the voxel set from the buildplan
    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;

        // offset of this block in the array
        int32_t off_x = b->x-build.bpxn+1;
        int32_t off_y = b->y-build.bpyn+1;
        int32_t off_z = b->z-build.bpzn+1;
        int32_t off   = off_x+off_z*(build.bpsx+2)+off_y*size_xz;

        // TODO: set the blocks not occupying the whole block (like slabs,
        //       stairs, torches, etc) as empty as they don't seal the surface
        bp[off] = b->b.bid ? 1 : 0;
    }

    // mark blocks completely surrounded by other blocks for removal
    int x,y,z;
    for(y=1; y<build.bpsy+1; y++) {
        int32_t off_y = size_xz*y;
        for(z=1; z<build.bpsz+1; z++) {
            for(x=1; x<build.bpsx+1; x++) {
                int32_t off = off_y + x + z*(build.bpsx+2);
                if (bp[off] &&
                    bp[off-1] && bp[off+1] &&
                    bp[off-(build.bpsx+2)] && bp[off+(build.bpsx+2)] &&
                    bp[off-size_xz] && bp[off+size_xz])
                    bp[off] = -1;
            }
        }
    }

    // new list for the build plan to hold only the blocks we keep after removal
    lh_arr_declare_i(blkr, keep);
    int removed=0;

    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;

        // offset of this block in the array
        int32_t off_x = b->x-build.bpxn+1;
        int32_t off_y = b->y-build.bpyn+1;
        int32_t off_z = b->z-build.bpzn+1;
        int32_t off   = off_x+off_z*(build.bpsx+2)+off_y*size_xz;

        if (bp[off] == 1) {
            blkr *k = lh_arr_new(GAR(keep));
            *k = *b;
        }
        else
            removed++;
    }

    // free our voxel set
    lh_free(bp);

    // replace the buildplan with the reduced list
    lh_arr_free(BPLAN);
    C(build.plan) = C(keep);
    P(build.plan) = P(keep);

    sprintf(reply, "Removed %d blocks, kept %zd", removed, C(build.plan));

    buildplan_updated();
    buildplan_place(reply);
}

#define TRIM_UNK -1
#define TRIM_EQ   0
#define TRIM_L    1
#define TRIM_LE   2
#define TRIM_G    3
#define TRIM_GE   4

// trim the buildplan by erasing block not fitting the criteria
static void build_trim(char **words, char *reply) {
    int i,j,f;
    int removed=0;

    lh_arr_declare_i(blkr, keep);

    int maxx=INT32_MAX, minx=INT32_MIN;
    int maxy=INT32_MAX, miny=INT32_MIN;
    int maxz=INT32_MAX, minz=INT32_MIN;

    // parse the parameters to determine the desired trim setting
    for(i=0; words[i]; i++) {
        char *arg = words[i];

        // parse the dimension
        int *maxc,*minc;
        char dim;
        switch(*arg) {
            case 'x': maxc=&maxx; minc=&minx; dim='x'; break;
            case 'y': maxc=&maxy; minc=&miny; dim='y'; break;
            case 'z': maxc=&maxz; minc=&minz; dim='z'; break;
            default :
                sprintf(reply, "Cannot parse parameter '%s'",arg);
                return;
        }
        arg++;

        // parse the operator
        int op=TRIM_UNK;
        switch(*arg) {
            case '=': op=TRIM_EQ; break;
            case '<': if (arg[1]=='=') { op=TRIM_LE; arg++; } else op=TRIM_L; break;
            case '>': if (arg[1]=='=') { op=TRIM_GE; arg++; } else op=TRIM_G; break;
            default :
                sprintf(reply, "Cannot parse parameter2 '%s'",arg);
                return;
        }
        arg++;

        // parse the value
        int val;
        if (sscanf(arg,"%d",&val)!=1) {
            sprintf(reply, "Cannot parse value '%s'",arg);
            return;
        }

        switch (op) {
            case TRIM_EQ: *maxc=val; *minc=val; break;
            case TRIM_L:  *maxc=val-1; break;
            case TRIM_LE: *maxc=val; break;
            case TRIM_G:  *minc=val-1; break;
            case TRIM_GE: *minc=val; break;
        }
    }

    // select blocks matching the trim criteria
    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;

        if (b->x>=minx && b->x<=maxx &&
            b->y>=miny && b->y<=maxy &&
            b->z>=minz && b->z<=maxz) {

            // this block is within boundaries and should be kept
            blkr *k = lh_arr_new(GAR(keep));
            *k = *b;
        }
        else
            removed++;
    }

    // replace the buildplan with the list of blocks that matched trim criteria
    lh_arr_free(BPLAN);
    C(build.plan) = C(keep);
    P(build.plan) = P(keep);

    sprintf(reply, "Removed %d blocks, kept %zd", removed, C(build.plan));

    buildplan_updated();
    buildplan_place(reply);
}

// flip the buildplan along the specified axis
static void build_flip(char **words, char *reply) {
    int i;
    char mode = 'x';
    if (words[0]) {
        switch(words[0][0]) {
            case 'x':
            case 'y':
            case 'z':
                mode=words[0][0]; break;
            default:
                sprintf(reply, "Usage: #build flip [x|y|z]");
                return;
        }
    }

    // TODO: handle meta-values
    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;
        switch (mode) {
            case 'x': b->x = -b->x; break;
            case 'y': b->y = -b->y; break;
            case 'z': b->z = -b->z; break;
        }
    }
    buildplan_updated();
    buildplan_place(reply);
}

// tilt the buildplan by 90 degrees
// the plan is rotated clockwise around specified axis (default: x),
// as when looking from the positive axis direction
static void build_tilt(char **words, char *reply) {
    int i;

    char mode = 'x';
    if (words[0]) {
        switch(words[0][0]) {
            case 'x':
            case 'y':
            case 'z':
                mode=words[0][0]; break;
            default:
                sprintf(reply, "Usage: #build tilt [x|y|z]");
                return;
        }
    }

    //TODO: rotate meta-values as far as possible
    int x,y,z;
    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;
        switch (mode) {
            case 'x':
                y = b->z;
                z = -b->y;
                b->y = y;
                b->z = z;
                break;
            case 'y':
                x = -b->z;
                z = b->x;
                b->x = x;
                b->z = z;
                break;
            case 'z':
                x = b->y;
                y = -b->x;
                b->x = x;
                b->y = y;
                break;
        }
    }

    buildplan_updated();
    buildplan_place(reply);
}

// make sure that the pivot block is at the bottom level of the buildplan
static void build_normalize(char **words, char *reply) {
    int i,
        minx=P(build.plan)[0].x,
        miny=P(build.plan)[0].y,
        maxz=P(build.plan)[0].z;

    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;
        if (b->x < minx) minx = b->x;
        if (b->y < miny) miny = b->y;
        if (b->z > maxz) maxz = b->z;
    }

    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;
        b->x += -minx;
        b->y += -miny;
        b->z -= maxz;
    }

    buildplan_updated();
    buildplan_place(reply);
}

// shrink the buildplan to half size in each dimension
static void build_shrink(char **words, char *reply) {
    int i,j;

    int32_t size_xz = build.bpsx*build.bpsz;
    lh_create_num(bid_t,bp,size_xz*build.bpsy);

    // initialize the voxel set from the buildplan
    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;

        // offset of this block in the array
        int32_t off_x = b->x-build.bpxn;
        int32_t off_y = b->y-build.bpyn;
        int32_t off_z = b->z-build.bpzn;
        int32_t off   = off_x+off_z*build.bpsx+off_y*size_xz;
        bp[off] = b->b;
    }

    // new list for the build plan to hold the blocks from the shrinked model
    lh_arr_declare_i(blkr, keep);

    int x,y,z;
    for(y=build.bpyn; y<build.bpyx-1; y+=2) {
        for(x=build.bpxn; x<build.bpxx-1; x+=2) {
            for(z=build.bpzn; z<build.bpzx-1; z+=2) {
                int32_t off_x = x-build.bpxn;
                int32_t off_y = y-build.bpyn;
                int32_t off_z = z-build.bpzn;

                int32_t off = off_x+off_z*build.bpsx+off_y*size_xz;
                int32_t offs[8] = {
                    off,
                    off+1,
                    off+build.bpsx,
                    off+1+build.bpsx,
                    off+size_xz,
                    off+1+size_xz,
                    off+build.bpsx+size_xz,
                    off+1+build.bpsx+size_xz
                };

                int blk[256]; lh_clear_obj(blk);
                for (i=0; i<8; i++) {
                    int bid = bp[offs[i]].bid;
                    blk[bid]++;
                }
                bid_t mat = BLOCKTYPE(0,0);
                for(i=1; i<256; i++)
                    if (blk[i]>4)
                        mat = BLOCKTYPE(i,0);

                if (mat.bid) {
                    blkr *k = lh_arr_new(GAR(keep));
                    k->b = mat;
                    k->x = build.bpxn + (x-build.bpxn)/2;
                    k->y = build.bpyn + (y-build.bpyn)/2;
                    k->z = build.bpzn + (z-build.bpzn)/2;
                }
            }
        }
    }

    // free our voxel set
    lh_free(bp);

    // replace the buildplan with the reduced list
    lh_arr_free(BPLAN);
    C(build.plan) = C(keep);
    P(build.plan) = P(keep);

    sprintf(reply, "Shrinked to %zd blocks", C(build.plan));

    buildplan_updated();
    buildplan_place(reply);
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Pivot placement

// a pivot block has been placed (using whatever method)
void place_pivot(int32_t px, int32_t py, int32_t pz, int dir) {
    assert(build.bp);

    // create a new buildtask from our buildplan
    int i;
    for(i=0; i<C(build.bp->plan); i++) {
        blkr *bp = P(build.bp->plan)+i;
        blk  *bt = lh_arr_new_c(BTASK); // new element in the buildtask

        switch (dir) {
            case DIR_SOUTH:
                bt->x = px-bp->x;
                bt->z = pz-bp->z;
                bt->b = rotate_meta(bp->b, 2);
                break;
            case DIR_NORTH:
                bt->x = px+bp->x;
                bt->z = pz+bp->z;
                bt->b = rotate_meta(bp->b, 0);
                break;
            case DIR_EAST:
                bt->x = px-bp->z;
                bt->z = pz+bp->x;
                bt->b = rotate_meta(bp->b, 1);
                break;
            case DIR_WEST:
                bt->x = px+bp->z;
                bt->z = pz-bp->x;
                bt->b = rotate_meta(bp->b, 3);
                break;
        }
        bt->y = py+bp->y;
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
    build.px = px;
    build.py = py;
    build.pz = pz;
    build.pd = dir;
    build.pivotset = 1;

    printf("Buildtask boundary: X: %d - %d   Z: %d - %d   Y: %d - %d\n",
           build.xmin, build.xmax, build.zmin, build.zmax, build.ymin, build.ymax);

    build_update();
}

// chat command "#build place cancel|once|many|coord=<x,z,y> [dir=<d>]"
static void build_place(char **words, char *reply) {
    if (mcparg_find(words, "cancel", NULL)) {
        sprintf(reply, "Placement canceled\n");
        build.placemode = 0;
        return;
    }

    // check if we have a plan
    if (!build.bp) {
        sprintf(reply, "You have no active buildplan!\n");
        return;
    }

    // pivot coordinates and direction
    int px,py,pz;
    int dir = -1;

    // parse coords
    mcpopt opt_coord = {{"coordinates","coords","coord","c"}, 0, {"%d,%d,%d","%d,%d","%d",NULL}};
    switch(mcparg_parse(words, &opt_coord, &px, &pz, &py)) {
        case 0:
            build.placemode = 0;
            break;
        case 1:
            py=gs.own.y>>5;
            build.placemode = 0;
            break;
        case 2: {
            int dist=px;
            px = gs.own.x>>5;
            py = gs.own.y>>5;
            pz = gs.own.z>>5;
            switch (player_direction()) {
                case DIR_SOUTH: pz+=dist; break;
                case DIR_NORTH: pz-=dist; break;
                case DIR_EAST:  px+=dist; break;
                case DIR_WEST:  px-=dist; break;
            }
            build.placemode = 0;
            break;
        }
        default:
            if (mcparg_find(words, "here", "h", "player", "p", NULL)) {
                px = gs.own.x>>5;
                py = gs.own.y>>5;
                pz = gs.own.z>>5;
                build.placemode = 0;
            }
            else if (mcparg_find(words, "many", NULL)) {
                sprintf(reply, "Mark pivot positions by placing block, disable with #build place cancel");
                build.placemode = 2; 
                return;
            }
            else if (mcparg_find(words, "again", NULL)) {
                if (!build.pivotset) {
                    sprintf(reply, "No pivot was set previously");
                }
                else {
                    px = build.px;
                    py = build.py;
                    pz = build.pz;
                    dir = build.pd;
                }
            }
            else {
                sprintf(reply, "Mark pivot position by placing a block - will be built once");
                build.placemode = 1; // only once (gets cleared after first placement)
                return;
            }
    }

    if (dir<0)
        dir = build_arg_dir(words, reply, 1);
    if (dir<0 || reply[0]) return;

    // abort current buildtask
    build_cancel();

    sprintf(reply, "Place pivot at %d,%d (%d), dir=%d (%s)\n",px,pz,py,dir,DIRNAME[dir]);
    place_pivot(px,py,pz,dir);
}

// handler for placing the pivot block in-game ("place once" or "place many")
void build_placemode(MCPacket *pkt, MCPacketQueue *sq, MCPacketQueue *cq) {
    CP_PlayerBlockPlacement_pkt *tpkt = &pkt->_CP_PlayerBlockPlacement;

    if (tpkt->face < 0) return; // ignore "fake" block placements
    int32_t x = tpkt->bpos.x - NOFF[tpkt->face][0];
    int32_t z = tpkt->bpos.z - NOFF[tpkt->face][1];
    int32_t y = tpkt->bpos.y - NOFF[tpkt->face][2];

    int dir = player_direction();

    if (build.placemode == 1) {
        build.placemode = 0;
        build_cancel(); // don't cancel the task in "multiple placement" mode
    }

    place_pivot(x,y,z,dir);

    char reply[4096];
    sprintf(reply, "Place pivot at %d,%d (%d), dir=%d\n",x,z,y,dir);
    chat_message(reply, cq, "green", 0);

    //TODO: detect when player accesses chests/etc

    // send a BlockChange to the client to remove the fake block it thinks it just placed
    bid_t blocks[256];
    export_cuboid(x>>4,1,z>>4,1,y,1,blocks);

    NEWPACKET(SP_BlockChange, bc);
    tbc->pos.x = x;
    tbc->pos.y = y;
    tbc->pos.z = z;
    tbc->block = blocks[(x&15)+((z&15)<<4)];
    queue_packet(bc, cq);

    // send a SetSlot to the client for the held item to restore item count
    NEWPACKET(SP_SetSlot, hi);
    thi->wid = 0;
    thi->sid = gs.inv.held+36;
    clone_slot(&gs.inv.slots[thi->sid], &thi->slot);
    queue_packet(hi, cq);
}




#if 0
////////////////////////////////////////////////////////////////////////////////
// Build Recorder

// chat command "#build rec start|stop|add"
static void build_rec(char **words, char *reply) {
    build_cancel(); // stop current building process if there was any

    if (!words[0] || !strcmp(words[0],"start")) {
        build_clear();
        build.recording = 1;
        build.pivotset = 0;
        sprintf(reply, "BREC started, buildplan cleared");
    }
    else if (!strcmp(words[0],"stop")) {
        build.recording = 0;
        sprintf(reply, "BREC stopped");
        buildplan_updated();
        buildplan_place(reply);
    }
    else if (!strcmp(words[0],"add") || !strcmp(words[0],"cont") || !strcmp(words[0],"continue")) {
        build.recording = 1;
        if (build.pivotset)
            sprintf(reply, "BREC continued, pivot at %d,%d,%d",build.px,build.py,build.pz);
        else
            sprintf(reply, "BREC continued, pivot not set");
    }
}

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
static void brec_blockupdate_blk(int32_t x, int32_t y, int32_t z, bid_t block) {
    block.meta &= ~(get_state_mask(block.bid));

    int i;
    for(i=0; i<build.nbrp; i++) {
        blkr *bl = &build.brp[i];
        if (bl->x==x && bl->y==y && bl->z==z) {
            //printf("Found pending block idx=%d %d,%d,%d %02x/%02x => %02x/%02x\n",
            //       i, x, y, z, bl->b.bid, bl->b.meta, block.bid, block.meta);

            // remove block from the pending queue
            blkr *brp = build.brp;
            lh_arr_delete(brp, build.nbrp, 1, i);

            // add block to the buildplan
            int j;
            for(j=0; j<C(build.plan); j++) {
                blkr *bp = P(build.plan)+j;
                if (bp->x==x && bp->y==y && bp->z==z) {
                    printf("Warning: block %d,%d,%d is already in the buildplan, updating bid  %02x/%02x => %02x/%02x\n",
                           x, y, z, bp->b.bid, bp->b.meta, block.bid, block.meta);
                    bp->b.raw = block.raw;
                    return;
                }
            }

            // this block was not in the buildplan, add it
            blkr *bp = lh_arr_new(BPLAN);

            switch (build.pd) {
                case DIR_SOUTH:
                    bp->x = build.px-x;
                    bp->z = build.pz-z;
                    bp->b = rotate_meta(block, -2);
                    break;
                case DIR_NORTH:
                    bp->x = x-build.px;
                    bp->z = z-build.pz;
                    bp->b = rotate_meta(block, 0);
                    break;
                case DIR_EAST:
                    bp->x = z-build.pz;
                    bp->z = build.px-x;
                    bp->b = rotate_meta(block, -1);
                    break;
                case DIR_WEST:
                    bp->x = build.pz-z;
                    bp->z = x-build.px;
                    bp->b = rotate_meta(block, -3);
                    break;
            }
            bp->y = y-build.py;

            //dump_brec_pending();
            return;
        }
    }
}

// handler for the SP_BlockChange and SP_MultiBlockChange messages from the server
// dispatch each updated block to brec_blockupdate_blk()
static void brec_blockupdate(MCPacket *pkt) {
    assert(pkt->pid == SP_BlockChange || pkt->pid == SP_MultiBlockChange);

    if (pkt->pid == SP_BlockChange) {
        SP_BlockChange_pkt *tpkt = &pkt->_SP_BlockChange;
        brec_blockupdate_blk(tpkt->pos.x,tpkt->pos.y,tpkt->pos.z,tpkt->block);
    }
    else if (pkt->pid == SP_MultiBlockChange) {
        SP_MultiBlockChange_pkt *tpkt = &pkt->_SP_MultiBlockChange;
        int i;
        for(i=0; i<tpkt->count; i++) {
            blkrec *br = tpkt->blocks+i;
            brec_blockupdate_blk(((tpkt->X)<<4)+br->x,br->y,((tpkt->Z)<<4)+br->z,br->bid);
        }
    }
    buildplan_updated();
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

    bl->x = x;
    bl->y = y;
    bl->z = z;
    bl->b = b;

    // if this is the first block being recorded, set it as pivot
    if (!build.pivotset) {
        build.pivotset = 1;
        build.px = x;
        build.py = y;
        build.pz = z;
        build.pd = player_direction();

        printf("Set pivot block at %d,%d,%d dir=%d\n",
               build.px,build.py,build.pz,build.pd);
    }

    dump_brec_pending();
}
#endif


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

#define PREVIEW_BLOCK BLOCKTYPE(0x98,0)

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
        if (b->placed) continue;

        int32_t X=b->x>>4;
        int32_t Z=b->z>>4;

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




#if 0
////////////////////////////////////////////////////////////////////////////////
// Save/Load

// save buildplan to a file
void build_save(const char * name, char * reply) {
    if (!name) {
        sprintf(reply, "Usage: #build save <name> (w/o extension)");
        return;
    }

    char fname[256];
    sprintf(fname, "%s.bplan", name);

    if (C(build.plan) <= 0) {
        sprintf(reply, "Cannot save - your buildplan is empty");
        return;
    }

    lh_create_buf(buf, sizeof(blkr)*C(build.plan));
    int i;
    uint8_t *w = buf;
    for(i=0; i<C(build.plan); i++) {
        blkr *b = P(build.plan)+i;
        lh_write_int_be(w, b->x);
        lh_write_int_be(w, b->y);
        lh_write_int_be(w, b->z);
        lh_write_short_be(w, b->b.raw);
    }

    ssize_t sz = lh_save(fname, buf, w-buf);
    if (sz > 0)
        sprintf(reply, "Saved buildplan to %s", fname);
    else
        sprintf(reply, "Error saving to %s", fname);

    lh_free(buf);
}

// append a buildplan from a file to existing buildplan
void build_loadappend(const char * name, char * reply, int ox, int oz, int oy) {
    char fname[256];
    sprintf(fname, "%s.bplan", name);

    uint8_t *buf;
    ssize_t sz = lh_load_alloc(fname, &buf);

    if (sz <= 0) {
        sprintf(reply, "Error loading from %s", fname);
        return;
    }

    uint8_t *p = buf;
    while(p<buf+sz-13) {
        blkr *b = lh_arr_new(BPLAN);
        b->x = lh_read_int_be(p)+ox;
        b->y = lh_read_int_be(p)+oy;
        b->z = lh_read_int_be(p)+oz;
        b->b.raw = lh_read_short_be(p);
    }

    sprintf(reply, "Loaded a buildplan with %zd blocks from %s",
            C(build.plan), fname);

    lh_free(buf);
    buildplan_updated();
    buildplan_place(reply);
}

// load a buildplan from a .bplan file
void build_load(const char * name, char * reply) {
    if (!name) {
        sprintf(reply, "Usage: #build load <name> (name w/o extension)");
        return;
    }
    build_clear();
    build_loadappend(name, reply, 0, 0, 0);
}

// load a buildplan from a .bplan file and append to existing buildplan (with a given offset)
// TODO: check for overlapping blocks and remove old ones
void build_append(char ** words, char * reply) {
    mcpopt opt_name = {{"name","n"}, 0, {"%s",NULL}};
    char name[256];
    if (mcparg_parse(words, &opt_name, name)!=0) {
        sprintf(reply, "Usage: #build append <name> [offset=ox,oz,oy] (name w/o extension)");
        return;
    }

    boff_t o = build_arg_offset(words, reply, 1);
    if (reply[0]) return;

    build_loadappend(name, reply, o.dx, o.dz, o.dy);
}

#define SCHEMATICS_DIR "schematic"

// import a buildplan from a .schematic file
void build_sload(const char *name, char *reply) {
    // generate filename
    if (!name) {
        sprintf(reply, "Usage: #build sload <name> (name w/o extension and path)");
        return;
    }

    char fname[256];
    sprintf(fname, "%s/%s.schematic", SCHEMATICS_DIR,name);

    // load the compressed .schematic file
    uint8_t *buf;
    ssize_t sz = lh_load_alloc(fname, &buf);

    if (sz <= 0) {
        sprintf(reply, "Error loading schematic from %s", fname);
        return;
    }

    // uncompress
    ssize_t dlen;
    uint8_t *dbuf = lh_gzip_decode(buf, sz, &dlen);

    if (!dbuf) {
        sprintf(reply, "Error loading schematic from %s", fname);
        return;
    }

    // parse the NBT structure
    uint8_t *p = dbuf;
    nbt_t *n = nbt_parse(&p);
    if (!n || (p-dbuf)!=dlen) {
        sprintf(reply, "Error parsing schematic from %s", fname);
        return;
    }

    //nbt_dump(n);

    // extract the NBT elements relevant for us
    nbt_t *Blocks = nbt_hget(n,"Blocks");
    nbt_t *Metas  = nbt_hget(n,"Data");
    nbt_t *Height = nbt_hget(n,"Height");
    nbt_t *Length = nbt_hget(n,"Length");
    nbt_t *Width  = nbt_hget(n,"Width");

    uint8_t *blocks = (uint8_t *)Blocks->ba;
    uint8_t *metas  = (uint8_t *)Metas->ba;
    int hg = Height->s;
    int wd = Width->s;
    int ln = Length->s;

    // scan the Blocks data for solid blocks
    build_clear();

    int x,y,z,i=0;
    for(y=0; y<hg; y++) {
        for (z=0; z<ln; z++) {
            for (x=0; x<wd; x++) {
                if (!NOSCAN(blocks[i])) {
                    blkr *b = lh_arr_new(BPLAN);
                    b->b.bid = blocks[i];
                    b->b.meta = metas[i]&0xf;
                    b->x = x;
                    b->z = z-ln+1;
                    b->y = y;
                }
                i++;
            }
        }
    }

    sprintf(reply, "Loaded schematic %s with %zd blocks, size=%dx%dx%d\n",
            fname, C(build.plan), wd, ln, hg);

    // cleanup
    nbt_free(n);
    lh_free(dbuf);
    lh_free(buf);

    buildplan_updated();
    buildplan_place(reply);
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Canceling Build

// cancel building and completely erase the buildplan
void build_clear() {
    build_cancel();
    bplan_free(build.bp);
    lh_clear_obj(build);

    if (!buildopts.init)
        buildopt_setdefault();
}

// cancel and delete the buildtask, leaving the buildplan
void build_cancel() {
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

void build_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq) {
    char reply[32768];
    reply[0]=0;
    int rpos = 0;

    char *cmd = words[1];
    words+=2;

    arg_defaults argdefaults;                                           \
    get_argdefaults(&argdefaults);                                      \
    int ARG_NOTFOUND=0;

    // possible arguments for the commands
    size3_t sz;
    bid_t mat;

    char buf[256];

    if (!cmd) {
        sprintf(reply, "Usage: build <type> [ parameters ... ] or build cancel");
    }

    // Parametric builds
    else if (!strcmp(cmd, "floor")) {
        ARG(size,NULL,sz);
        ARGREQUIRE(size);

        ARG(mat,NULL,mat);
        ARGREQUIRE(mat);

        build.bp = bplan_floor(sz.x, sz.z, mat);
        bplan_update(build.bp);

        sprintf(reply, "Floor size=%dx%d material=%s",sz.x,sz.z,get_bid_name(buf, mat));
    }
    else if (!strcmp(cmd, "wall")) {
        ARG(size,NULL,sz);
        ARGREQUIRE(size);

        ARG(mat,NULL,mat);
        ARGREQUIRE(mat);

        build.bp = bplan_wall(sz.x, sz.z, mat);
        bplan_update(build.bp);

        sprintf(reply, "Wall size=%dx%d material=%s",sz.x,sz.z,get_bid_name(buf, mat));
    }
#if 0
    else if (!strcmp(cmd, "ring")) {
        build_ring(words, reply);
    }
    else if (!strcmp(cmd, "ball")) {
        build_ball(words, reply);
    }
    else if (!strcmp(cmd, "disk")) {
        build_disk(words, reply);
    }
    else if (!strcmp(cmd, "scaf") || !strcmp(cmd, "scaffolding")) {
        build_scaffolding(words, reply);
    }
    else if (!strcmp(cmd, "stairs")) {
        build_stairs(words, reply);
    }

    // Buildplan manipulation
    else if (!strcmp(cmd, "ext") || !strcmp(cmd, "extend")) {
        build_extend(words, reply);
    }
    else if (!strcmp(cmd, "hollow")) {
        build_hollow(words, reply);
    }
    else if (!strcmp(cmd, "replace")) {
        build_replace(words, reply);
    }
    else if (!strcmp(cmd, "trim")) {
        build_trim(words, reply);
    }
    else if (!strcmp(cmd, "flip")) {
        build_flip(words, reply);
    }
    else if (!strcmp(cmd, "tilt")) {
        build_tilt(words, reply);
    }
    else if (!strcmp(cmd, "normalize") || !strcmp(cmd, "norm")) {
        build_normalize(words, reply);
    }
    else if (!strcmp(cmd, "shrink") || !strcmp(cmd, "sh")) {
        build_shrink(words, reply);
    }

    // Save/load/import
    else if (!strcmp(cmd, "save")) {
        build_save(words[0], reply);
    }
    else if (!strcmp(cmd, "load")) {
        build_load(words[0], reply);
    }
    else if (!strcmp(cmd, "sload")) {
        build_sload(words[0], reply);
    }
    else if (!strcmp(cmd, "append")) {
        build_append(words, reply);
    }
    else if (!strcmp(cmd, "rec")) {
        build_rec(words, reply);
    }
    else if (!strcmp(cmd, "scan")) {
        build_scan(words, reply);
    }
#endif

    // Build control
    else if (!strcmp(cmd, "place")) {
        build_place(words, reply);
    }
    else if (!strcmp(cmd, "cancel")) {
        build_cancel();
        build.placemode=0;
    }
    else if (!strcmp(cmd, "pause")) {
        if (P(build.task)) {
            build.active = !build.active;
            sprintf(reply, "Buildtask is %s", build.active?"unpaused":"paused");
        }
        else {
            sprintf(reply, "You need an existing buildtask to pause/unpause");
        }
        rpos=2;
    }

    // Preview
    else if (!strcmp(cmd, "preview")) {
        int mode = PREVIEW_MISSING;

        if (words[0]) {
            if (!strcmp(words[0], "true") || !strcmp(words[0], "t"))
                mode = PREVIEW_TRUE;
            else if (!strcmp(words[0], "remove") || !strcmp(words[0], "-"))
                mode = PREVIEW_REMOVE;
        }
        build_show_preview(sq, cq, mode);
    }

    // Debug
    else if (!strcmp(cmd, "dumpplan")) {
        bplan_dump(build.bp);
    }
    else if (!strcmp(cmd, "dumptask")) {
        build_dump_task();
    }
    else if (!strcmp(cmd, "dumpqueue")) {
        build_dump_queue();
    }
#if 0
    else if (!strcmp(cmd, "dumpmat")) {
        calculate_material(words[0] && !strcmp(words[0], "plan"));
    }
#endif

    // Build options
    else if (!strcmp(cmd, "wallmode") || !strcmp(cmd, "wm")) {
        build.wallmode = !build.wallmode;
        sprintf(reply, "Wall mode is %s",build.wallmode?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(cmd, "sealmode") || !strcmp(cmd, "sm")) {
        build.sealmode = !build.sealmode;
        sprintf(reply, "Seal mode is %s",build.sealmode?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(cmd, "limit") || !strcmp(cmd, "li")) {
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
    }
    else if (!strcmp(cmd, "anyface") || !strcmp(cmd, "af")) {
        build.anyface = !build.anyface;
        sprintf(reply, "Anyface buildng is %s",build.anyface?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(cmd, "opt") || !strcmp(cmd, "set")) {
        buildopt(words, cq);
    }

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
#if 0
        if (pkt->pid == SP_BlockChange || pkt->pid == SP_MultiBlockChange) {
            brec_blockupdate(pkt);
        }
        else if (pkt->pid == CP_PlayerBlockPlacement) {
            brec_blockplace(pkt);
        }
#endif
        return 1;
    }
    else if (build.placemode && pkt->pid == CP_PlayerBlockPlacement) {
        build_placemode(pkt, sq, cq);
        return 0; // do not forward the packet to the server
    }

    return 1;
}
