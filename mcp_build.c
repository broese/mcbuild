#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include <lh_buffers.h>
#include <lh_files.h>
#include <lh_bytes.h>

#include "mcp_ids.h"
#include "mcp_build.h"
#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_arg.h"





////////////////////////////////////////////////////////////////////////////////
// Helpers

static int scan_opt(char **words, const char *fmt, ...) {
    int i;
    
    const char * fmt_opts = index(fmt, '=');
    assert(fmt_opts);
    ssize_t optlen = fmt_opts+1-fmt; // the size of the option name with '='

    for(i=0; words[i]; i++) {
        if (!strncmp(words[i],fmt,optlen)) {
            va_list ap;
            va_start(ap,fmt);
            int res = vsscanf(words[i]+optlen,fmt+optlen,ap);
            va_end(ap);
            return res;
        }
    }

    return 0;
}

static int find_opt(char **words, const char *name) {
    int i;
    for(i=0; words[i]; i++)
        if (!strcmp(words[i], name))
            return 1;
    return 0;
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

uint16_t DOTS_ALL[15] = {
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff };

uint16_t DOTS_LOWER[15] = {
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0, 0,
    0, 0, 0, 0, 0, 0, 0 };

uint16_t DOTS_UPPER[15] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff };

uint16_t DOTS_NONE[15] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0 };

// this structure is used to define an absolute block placement
// in the active building process
typedef struct {
    int32_t     x,y,z;          // coordinates of the block to place
    bid_t       b;              // block type, including the meta

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

// this structure defines a relative block placement in a buildplan
typedef struct {
    int32_t     x,y,z;  // coordinates of the block to place (relative to pivot)
    bid_t       b;      // block type, including the meta
                        // positional meta is north-oriented
} blkr;

// maximum number of blocks in the buildable list
#define MAXBUILDABLE 1024

struct {
    int64_t lastbuild;         // timestamp of last block placement

    int active;                // if nonzero - buildtask is being built
    int recording;             // if nonzero - build recording active
    int placemode;             // 0-disabled, 1-once, 2-multiple

    lh_arr_declare(blk,task);  // current active building task
    lh_arr_declare(blkr,plan); // currently loaded/created buildplan

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

    int32_t bpsx,bpsy,bpsz;
} build;

#define BTASK GAR(build.task)
#define BPLAN GAR(build.plan)






////////////////////////////////////////////////////////////////////////////////
// Inventory

// slot range in the quickbar that can be used for material fetching
int matl=3, math=7;

// timestamps when the material slots were last accessed - used to select evictable slots
int64_t mat_last[9];

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
static int prefetch_material(MCPacketQueue *sq, MCPacketQueue *cq, bid_t mat) {
    int mslot = find_material_slot(mat);

    if (mslot<0) return -1; // material not available in the inventory

    if (mslot>=36 && mslot<=44) return mslot-36; // found in thequickbar

    // fetch the material from main inventory to a suitable quickbar slot
    int eslot = find_evictable_slot();
    //printf("swapping slots: %d %d\n", mslot, eslot+36);
    gmi_swap_slots(sq, cq, mslot, eslot+36);

    return eslot;
}

static void calculate_material(int plan) {
    int i;

    if (plan) {
        printf("=== Material Demand for the Buildplan ===\n");
    }
    else {
        printf("=== Material Demand for the Buildtask ===\n");
        build_update();
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

    // print material demand
    for(i=0; i<65536; i++) {
        if (bc[i] > 0) {
            bid_t mat;
            mat.raw = (uint16_t)i;

            printf("block:%02x/%02x need:%4d have:%4d ",mat.bid,mat.meta,bc[i],ic[i]);

            int need = bc[i]-ic[i];
            if (need <= 0)
                printf("              ");
            else
                printf("%3dS+%-2d short ", need/STACKSIZE(mat.bid), need%STACKSIZE(mat.bid));

            char buf[256];
            printf("%s\n",get_bid_name(buf, mat));
        }
    }

    printf("=========================================\n");
}







////////////////////////////////////////////////////////////////////////////////
// Building process

#define SQ(x) ((x)*(x))
#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

// maximum reach distance for building, squared, in fixp units (1/32 block)
#define EYEHEIGHT 52
#define MAXREACH_COARSE SQ(5<<5)
#define MAXREACH SQ(4<<5)
#define OFF(x,z,y) (((x)-xo)+((z)-zo)*(xsz)+((y)-yo)*(xsz*zsz))

static inline int ISEMPTY(int bid) {
    return ( bid==0x00 ||               // air
             bid==0x08 || bid==0x09 ||  // water
             bid==0x0a || bid==0x0b ||  // lava
             bid==0x1f ||               // tallgrass
             bid==0x33 );               // fire
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
                }
                else {
                    if (b->dist < dist)
                        b->dist = dist;
                }
            }
        }
    }

    b->inreach = (b->dist > 0);
}

static inline int count_dots_row(uint16_t dots) {
    int i,c=0;
    for (i=0; i<15; dots>>=1,i++)
        c += (dots&1);
    return c;
}

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
            int rdir = (b->b.meta&2) ?
                ((b->b.meta&1) ? DIR_NORTH : DIR_SOUTH ) :
                ((b->b.meta&1) ? DIR_WEST  : DIR_EAST );

            int pdir = player_direction();
            if (pdir != rdir) {
                // placement not possible
                setdots(b, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE, DOTS_NONE);
            }
            else {
                if (b->b.meta&4) // upside-down placement
                    setdots(b, DOTS_ALL, DOTS_NONE, DOTS_UPPER, DOTS_UPPER, DOTS_UPPER, DOTS_UPPER);
                else // straight placement
                    setdots(b, DOTS_NONE, DOTS_ALL, DOTS_LOWER, DOTS_LOWER, DOTS_LOWER, DOTS_LOWER);
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

        // calculate exact distance to each of the dots and remove those out of reach
        remove_distant_dots(b);

        b->ndots = count_dots(b);
        if (b->ndots>0 && build.nbq<MAXBUILDABLE)
            build.bq[build.nbq++] = i;
    }
    build.bq[build.nbq] = -1;

    qsort(build.bq, build.nbq, sizeof(build.bq[0]), sort_blocks);

    /* Further strategy:
       - skip those neighbor faces looking away from you

       - skip those neighbor faces unsuitable for the block orientation
         you want to achieve - for now we can skip that for the plain
         blocks - they can be placed on any neighbor. Later we'll need
         to determine this properly for the stairs, slabs, etc.

       + for each neighbor face, calculate which from 15x15 points can be
         'clicked' to be able to place the block we want the way we want.
         For now, we can just say "all of them" - this will work with plain
         blocks. Later we can introduce support for slabs, stairs etc., e.g.
         in order to place the upper slab we will have to choose only the
         upper 15x7 block from the matrix.

       - for each of the remaining points, calculate their direct visibility
         this is optional for now, because it's obviously very difficult
         to achieve and possibly not even checked properly.

       + for each of the remaining points, calculate their exact distance,
         skip those farther away than 4.0 blocks (this is now the proper
         in-reach calculation)

       + for each of the remaining points, store the one with the largest
         distance in the blk - this will serve as the selector for the
         build-the-most-distant-blocks-first strategy to avoid isolating blocks

       + store the suitable dots (as a bit array in a 16xshorts?) in the
         blk struct

       + when building, select the first suitable block for building,
         and choose a random dot from the stored set

       + automatically fetch materials into a quickbar slot

    */

    free(world);
}

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
                        *cx = (dotpos.rx*dr+dotpos.cx*dc)/2;
                        *cy = (dotpos.ry*dr+dotpos.cy*dc)/2;
                        *cz = (dotpos.rz*dr+dotpos.cz*dc)/2;
                        return;
                    }
                }
            }
        }
    }
    printf("Fail: i=%d\n",i);
    assert(0);
}

// minimum interval between attempting to build the same block
#define BUILD_BLKINT 200000
#define BUILD_BLDINT 50000

// maximum number of blocks to attempt to place in one go
#define BUILD_BLKMAX 3

void build_progress(MCPacketQueue *sq, MCPacketQueue *cq) {
    // time update - try to build any blocks from the placeable blocks list
    if (!build.active) return;

    uint64_t ts = gettimestamp();

    if (ts < build.lastbuild+BUILD_BLDINT) return;

    int i, bc=0;
    int held=gs.inv.held;

    for(i=0; i<build.nbq && bc<BUILD_BLKMAX; i++) {
        blk *b = P(build.task)+build.bq[i];
        if (ts-b->last < BUILD_BLKINT) continue;

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
        printf("Placing block %d,%d,%d (%s)\n",
               b->x,b->y,b->z, get_item_name(buf, hslot));

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

// recalculate the size of the buildplan
static void buildplan_updated() {
    if (C(build.plan)==0) {
        // we have no buildplan
        build.bpsx=build.bpsy=build.bpsz=0;
        return;
    }

    int32_t xn,xx,yn,yx,zn,zx;

    xn=xx=P(build.plan)[0].x;
    yn=yx=P(build.plan)[0].y;
    zn=zx=P(build.plan)[0].z;

    int i;
    for(i=0; i<C(build.plan); i++) {
        xn = MIN(P(build.plan)[i].x, xn);
        xx = MAX(P(build.plan)[i].x, xx);
        yn = MIN(P(build.plan)[i].y, yn);
        yx = MAX(P(build.plan)[i].y, yx);
        zn = MIN(P(build.plan)[i].z, zn);
        zx = MAX(P(build.plan)[i].z, zx);
    }

    build.bpsx=xx-xn+1;
    build.bpsy=yx-yn+1;
    build.bpsz=zx-zn+1;
}





////////////////////////////////////////////////////////////////////////////////
// Parametric structures

static bid_t build_arg_material(char **words, char *reply) {
    mcpopt opt_mat = {{"material","mat","m",NULL}, -1, {"%d:%d","%d/%d","%d,%d","%d",NULL}};

    bid_t mat;

    // determine the building material
    int bid=-1, meta=0;
    switch(mcparg_parse(words, &opt_mat, &bid, &meta)) {
        case 0:
        case 1:
        case 2:
            // bid+meta specified explicitly
            break;
        case 3:
            // no meta specified - assume 0
            meta=0;
            break;
        default: {
            // nothing specified, take the same material the player is currently holding
            slot_t *s = &gs.inv.slots[gs.inv.held+36];
            if (s->item > 0 && !(ITEMS[s->item].flags&I_ITEM)) {
                bid = s->item;
                meta = s->damage;
            }
            else {
                bid = -1;
            }
        }
    }

    if (bid<0)
        sprintf(reply, "You must specify material - either explicitly with "
                "mat=<bid>[,<meta>] or by holding a placeable block");

    mat.bid = bid;
    mat.meta = meta;

    if (ITEMS[mat.bid].flags&I_SLAB) {
        // for slab blocks additionally parse the upper/lower placement
        if (mcparg_find(words,"upper","up","u","high","h",NULL))
            mat.meta |= 8;
        else
            mat.meta &= 7;
    }

    return mat;
}

static void build_floor(char **words, char *reply) {
    build_clear();

    // floor size
    int xsize,zsize;
    mcpopt opt_size = {{"size","sz","s",NULL}, 0, {"%d,%d","%dx%d","%d",NULL}};
    switch(mcparg_parse(words, &opt_size, &xsize, &zsize)) {
        case 0:
        case 1:
            break;
        case 2:
            zsize=xsize; // square floor
            break;
        default:
            sprintf(reply, "Usage: build floor size=<xsize>,<zsize>");
            return;
    }

    if (xsize<=0 || zsize<=0) {
        sprintf(reply, "Usage: illegal floor size %d,%d",xsize,zsize);
        return;
    }

    bid_t mat = build_arg_material(words, reply);
    if (reply[0]) return;


    int x,z;
    for(x=0; x<xsize; x++) {
        for(z=0; z<zsize; z++) {
            blkr *b = lh_arr_new(BPLAN);
            b->b = mat;
            b->x = x;
            b->z = -z;
            b->y = 0;
        }
    }

    char buf[256];
    int off = sprintf(reply, "Created floor %dx%d material=%s",
                      xsize, zsize, get_bid_name(buf, mat));

    buildplan_updated();
}

// add a single block to the buildplan
#define PLACE_DOT(_x,_z)                            \
    b = lh_arr_new(BPLAN); b->b = mat; b->y = 0;    \
    b->x = (_x);  b->z = (_z);

static void build_ring(char **words, char *reply) {
    build_clear();

    // ring diameter
    int diam;
    if (scan_opt(words, "size=%d", &diam)!=1) {
        sprintf(reply, "Usage: build ring size=<diameter>");
        return;
    }
    if (diam<=0) {
        sprintf(reply, "Usage: illegal ring size %d",diam);
        return;
    }

    bid_t mat = build_arg_material(words, reply);
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
}

static int SCAFF_STAIR[5][2] = { { 1, -1}, { 2, -1 }, { 2, 0 }, { 3, 0 }, { 3, 1 } };

static void build_scaffolding(char **words, char *reply) {
    build_clear();

    int wd,hg;
    if (scan_opt(words, "size=%d,%d", &wd, &hg)!=2) {
        sprintf(reply, "Usage: build scaffolding size=<width>,<floors>");
        return;
    }
    if (wd<=0 || hg<=0) {
        sprintf(reply, "Usage: illegal scaffolding size %d,%d",wd,hg);
        return;
    }

    // ensure minimum width so we can connect the stairs
    if (hg==1 && wd<4) wd=4;
    if (hg>1 && wd<7) wd=7;

    // determine the building material
    int bid=0, meta=0;
    // first option: BID,meta specified
    if (scan_opt(words, "mat=%d,%d", &bid, &meta)!=2) {
        // second option: just block ID, assume meta=0
        meta = 0;
        if (scan_opt(words, "mat=%d", &bid)!=1) {
            // third option: use dirt
            bid = 0x03; // Dirt
        }
    }
    bid_t mat = BLOCKTYPE(bid, meta);

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

    sprintf(reply, "Created scaffolding: %d floors, %d blocks wide", hg, wd);
    buildplan_updated();
}

static void build_stairs(char **words, char *reply) {
    build_clear();

    int wd,hg;
    if (scan_opt(words, "size=%d,%d", &wd, &hg)!=2) {
        sprintf(reply, "Usage: build stairs size=<width>,<floors>");
        return;
    }
    if (wd<=0 || hg<=0) {
        sprintf(reply, "Usage: illegal stairs size %d,%d",wd,hg);
        return;
    }

    bid_t mat = build_arg_material(words, reply);
    if (reply[0]) return;

    // make stairs-type block face player
    if ((ITEMS[mat.bid].flags&I_STAIR)) mat.meta=3;

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

            //TODO: make this optional?
            //TODO: make it match the stairs block
            b = lh_arr_new(BPLAN);
            b->b = mat;
            b->x = x;
            b->z = -floor-1;
            b->y = floor;
        }
    }

    sprintf(reply, "Created stairs: %d floors, %d blocks wide", hg, wd);
    buildplan_updated();
}





////////////////////////////////////////////////////////////////////////////////
// Buildplan manipulation

/*
Extend the buildplan by replicating it a given number of times.

#build extend <offset|direction> [<count>]
<offset>    := [offset=]<dx>[,<dz>[,<dy>]]      # explicit offset, default=0
<direction> := [direction=](u|d|r|l|f|b)        # offset by buildplan size
<count>     := [count=]<count>                  # how many times, default=1
*/
static void build_extend(char **words, char *reply) {
    mcpopt opt_offset = {{"offset","off","o",NULL},    0, {"%d,%d,%d","%d,%d","%d",NULL}};
    mcpopt opt_dir    = {{"direction","dir","d",NULL}, 0, {"%s",NULL}};
    mcpopt opt_count  = {{"count","cnt","c",NULL},     1, {"%d",NULL}};

    int ox,oy,oz;

    switch(mcparg_parse(words, &opt_offset, &ox, &oz, &oy)) {
        case 0: break;
        case 1: oy=0; break;
        case 2: oz=0; oy=0; break;
        default: {
            char dir[256];
            if (mcparg_parse(words, &opt_dir, dir)<0) {
                sprintf(reply, "Usage: #build extend offset=x[,z[,y]]|u|d|r|l|f|b");
                return;
            }

            switch(dir[0]) {
                case 'u': ox=0; oz=0; oy=build.bpsy; break;
                case 'd': ox=0; oz=0; oy=-build.bpsy; break;
                case 'r': ox=build.bpsx; oz=0; oy=0; break;
                case 'l': ox=-build.bpsx; oz=0; oy=0; break;
                case 'f': ox=0; oz=build.bpsz; oy=0; break;
                case 'b': ox=0; oz=-build.bpsz; oy=0; break;
                default:
                    sprintf(reply, "Usage: #build extend offset=x[,z[,y]]|u|d|r|l|f|b");
                    return;
            }
        }
    }

    int count;
    if (mcparg_parse(words, &opt_count, &count)<0)
        count=1;

    int i,j;
    int bc=C(build.plan);
    for(i=1; i<=count; i++) {
        for(j=0; j<bc; j++) {
            blkr *bo = P(build.plan)+j;
            blkr *bn = lh_arr_new(BPLAN);
            bn->x = bo->x+ox*i;
            bn->y = bo->y+oy*i;
            bn->z = bo->z+oz*i;
            bn->b = bo->b;
        }
    }

    sprintf(reply, "Extended buildplan %d times, offset=%d,%d,%d",
            count, ox,oz,oy);
    buildplan_updated();
}






////////////////////////////////////////////////////////////////////////////////
// Pivot placement

// rotation mapping for the stairs-type blocks (2 low bits in the meta)
static uint8_t ROTATE_STAIR[][4] = {
    [DIR_NORTH] = { 0, 1, 2, 3 },
    [DIR_SOUTH] = { 1, 0, 3, 2 },
    [DIR_EAST]  = { 2, 3, 1, 0 },
    [DIR_WEST]  = { 3, 2, 0, 1 },
};

// a pivot block has been placed (using whatever method)
void place_pivot(int32_t px, int32_t py, int32_t pz, int dir) {
    // create a new buildtask from our buildplan
    int i;
    for(i=0; i<C(build.plan); i++) {
        blkr *bp = P(build.plan)+i;
        blk  *bt = lh_arr_new_c(BTASK); // new element in the buildtask

        switch (dir) {
            case DIR_SOUTH:
                bt->x = px-bp->x;
                bt->z = pz-bp->z;
                break;
            case DIR_NORTH:
                bt->x = px+bp->x;
                bt->z = pz+bp->z;
                break;
            case DIR_EAST:
                bt->x = px-bp->z;
                bt->z = pz+bp->x;
                break;
            case DIR_WEST:
                bt->x = px+bp->z;
                bt->z = pz-bp->x;
                break;
        }
        bt->y = py+bp->y;

        bt->b = bp->b;

        //correct the I_MPOS-dependent metas
        if (ITEMS[bt->b.bid].flags&I_STAIR) {
            // rotate stair-type blocks
            uint8_t stair_rot = ROTATE_STAIR[dir][bt->b.meta&3];
            bt->b.meta = (bt->b.meta&4)|(stair_rot&3);
        }
        //TODO: other I_MPOS blocks
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
    printf("Buildtask boundary: X: %d - %d   Z: %d - %d   Y: %d - %d\n",
           build.xmin, build.xmax, build.zmin, build.zmax, build.ymin, build.ymax);

    build_update();
}

// chat command "#build place cancel|once|many|coord=<x,z,y> [dir=<d>]"
static void build_place(char **words, char *reply) {
    if (find_opt(words, "cancel")) {
        sprintf(reply, "Placement canceled\n");
        build.placemode = 0;
        return;
    }

    // check if we have a plan
    if (!C(build.plan)) {
        sprintf(reply, "You have no active buildplan!\n");
        return;
    }

    // pivot coordinates and direction
    int px,py,pz;
    int dir = -1;

    // parse coords
    if (scan_opt(words, "coord=%d,%d,%d", &px, &pz, &py)!=3) {
        // second possibility - place at player's position (in front of him)
        if (find_opt(words, "here")) {
            px = gs.own.x>>5;
            py = gs.own.y>>5;
            pz = gs.own.z>>5;

            dir = player_direction();
            switch (dir) {
                case DIR_SOUTH: pz++; break;
                case DIR_NORTH: pz--; break;
                case DIR_EAST:  px++; break;
                case DIR_WEST:  px--; break;
            }
        }

        // next possibility - place by placing the pivot block manually
        else if (find_opt(words, "once")) {
            sprintf(reply, "Mark pivot position by placing a block - will be build once");
            build.placemode = 1; // only once (gets cleared after first placement)
            return;
        }
        else if (find_opt(words, "many")) {
            sprintf(reply, "Mark pivot positions by placing block, disable with #build place cancel");
            build.placemode = 2; // many times (cancel explicitly)
            return;
        }

        sprintf(reply, "Usage: build place coord=<x>,<z>,<y>|here|once|many|cancel");
    }

    // parse placement direction
    if (dir<0) {
        if (scan_opt(words, "dir=%d", &dir)!=1) {
            // if not specified, derive from the player's look direction
            dir = player_direction();
        }
    }

    if (dir<DIR_SOUTH || dir>DIR_WEST) {
        sprintf(reply, "incorrect direction code, use: SOUTH=2,NORTH=3,EAST=4,WEST=5");
        return;
    }

    // abort current buildtask
    build_cancel();

    sprintf(reply, "Place pivot at %d,%d (%d), dir=%d\n",px,pz,py,dir);
    place_pivot(px,pz,py,dir);

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

    //TODO: send a SetSlot to the client, so it does not decrement the block count ?
    //TODO: detect when player accesses chests/etc
}





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
                    break;
                case DIR_NORTH:
                    bp->x = x-build.px;
                    bp->z = z-build.pz;
                    break;
                case DIR_EAST:
                    bp->x = z-build.pz;
                    bp->z = build.px-x;
                    break;
                case DIR_WEST:
                    bp->x = build.pz-z;
                    bp->z = x-build.px;
                    break;
            }
            bp->y = y-build.py;

            bp->b = block;

            //correct the I_MPOS-dependent metas
            int rot=build.pd;
            switch(build.pd) {
                case DIR_EAST: rot=DIR_WEST; break;
                case DIR_WEST: rot=DIR_EAST; break;
                default: rot=build.pd;
            }

            if (ITEMS[block.bid].flags&I_STAIR) { //stair-type blocks
                // we can use the same table, but switch east and west
                uint8_t stair_rot = ROTATE_STAIR[rot][bp->b.meta&3];
                bp->b.meta = (bp->b.meta&4)|(stair_rot&3);
            }
            //TODO: other I_MPOS blocks

            dump_brec_pending();
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





////////////////////////////////////////////////////////////////////////////////
// Debugging

// dump our buildplan to console
void build_dump_plan() {
    int i;
    char buf[256];
    for(i=0; i<C(build.plan); i++) {
        blkr *b = &P(build.plan)[i];
        printf("%3d %+4d,%+4d,%3d %3x/%02x (%s)\n",
               i, b->x, b->z, b->y, b->b.bid, b->b.meta,
               get_bid_name(buf, get_base_material(b->b)));
    }
    printf("Buildplan size: W:%d D:%d H:%d\n",build.bpsx,build.bpsz,build.bpsy);
}

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

// load a buildplan from a file
void build_load(const char * name, char * reply) {
    if (!name) {
        sprintf(reply, "Usage: #build load <name> (w/o extension)");
        return;
    }

    char fname[256];
    sprintf(fname, "%s.bplan", name);

    build_clear();

    uint8_t *buf;
    ssize_t sz = lh_load_alloc(fname, &buf);

    if (sz <= 0) {
        sprintf(reply, "Error loading from %s", fname);
        return;
    }

    uint8_t *p = buf;
    while(p<buf+sz-13) {
        blkr *b = lh_arr_new(BPLAN);
        b->x = lh_read_int_be(p);
        b->y = lh_read_int_be(p);
        b->z = lh_read_int_be(p);
        b->b.raw = lh_read_short_be(p);
    }

    sprintf(reply, "Loaded a buildplan with %zd blocks from %s",
            C(build.plan), fname);

    lh_free(buf);
    buildplan_updated();
}





////////////////////////////////////////////////////////////////////////////////
// Canceling Build

void build_clear() {
    build_cancel();
    lh_arr_free(BPLAN);
    lh_clear_obj(build);
    buildplan_updated();
}

void build_cancel() {
    build.active = 0;
    lh_arr_free(BTASK);
    build.bq[0] = -1;
    build.nbrp = 0; // clear the pending queue
}






////////////////////////////////////////////////////////////////////////////////
// Main build command dispatch

void build_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq) {
    char reply[32768];
    reply[0]=0;

    if (!words[1]) {
        sprintf(reply, "Usage: build <type> [ parameters ... ] or build cancel");
    }
    else if (!strcmp(words[1], "floor")) {
        build_floor(words+2, reply);
    }
    else if (!strcmp(words[1], "ring")) {
        build_ring(words+2, reply);
    }
    else if (!strcmp(words[1], "scaf") || !strcmp(words[1], "scaffolding")) {
        build_scaffolding(words+2, reply);
    }
    else if (!strcmp(words[1], "stairs")) {
        build_stairs(words+2, reply);
    }
    else if (!strcmp(words[1], "place")) {
        build_place(words+2, reply);
    }
    else if (!strcmp(words[1], "cancel")) {
        build_cancel();
        build.placemode=0;
    }
    else if (!strcmp(words[1], "dumpplan")) {
        build_dump_plan();
    }
    else if (!strcmp(words[1], "dumptask")) {
        build_dump_task();
    }
    else if (!strcmp(words[1], "dumpqueue")) {
        build_dump_queue();
    }
    else if (!strcmp(words[1], "dumpmat")) {
        calculate_material(find_opt(words+2, "plan"));
    }
    else if (!strcmp(words[1], "save")) {
        build_save(words[2], reply);
    }
    else if (!strcmp(words[1], "load")) {
        build_load(words[2], reply);
    }
    else if (!strcmp(words[1], "rec")) {
        build_rec(words+2, reply);
    }
    else if (!strcmp(words[1], "ext") || !strcmp(words[1], "extend")) {
        build_extend(words+2, reply);
    }

    if (reply[0]) chat_message(reply, cq, "green", 0);
}

////////////////////////////////////////////////////////////////////////////////

// dispatch for the building-relevant packets we get from mcp_game
int build_packet(MCPacket *pkt, MCPacketQueue *sq, MCPacketQueue *cq) {
    if (build.recording) {
        if (pkt->pid == SP_BlockChange || pkt->pid == SP_MultiBlockChange) {
            brec_blockupdate(pkt);
        }
        else if (pkt->pid == CP_PlayerBlockPlacement) {
            brec_blockplace(pkt);
        }
        return 1;
    }
    else if (build.placemode && pkt->pid == CP_PlayerBlockPlacement) {
        build_placemode(pkt, sq, cq);
        return 0; // do not forward the packet to the server
    }

    return 1;
}
