#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "mcp_ids.h"
#include "mcp_build.h"
#include "mcp_gamestate.h"

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

#define SQ(x) ((x)*(x))
#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

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

uint16_t DOTS_UPPER[15] = {
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0,
    0, 0, 0, 0, 0, 0, 0 };

uint16_t DOTS_LOWER[15] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff };

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
    int ndots;

    int32_t dist; // distance to the block center (squared)

    uint64_t last;              // last timestamp when we attempted to place this block
} blk;

// this structure defines a relative block placement 
typedef struct {
    int32_t     x,y,z;  // coordinates of the block to place (relative to pivot)
    bid_t       b;      // block type, including the meta
                        // positional meta is north-oriented
} blkr;

// maximum number of blocks in the buildable list
#define MAXBUILDABLE 1024

struct {
    int active;
    lh_arr_declare(blk,task);  // current active building task
    lh_arr_declare(blkr,plan); // currently loaded/created buildplan

    int bq[MAXBUILDABLE];      // list of buildable blocks from the task
    int nbq;                   // number of buildable blocks

    int32_t     xmin,xmax,ymin,ymax,zmin,zmax;
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
    int best_s;
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
    gmi_swap_slots(sq, cq, mslot, eslot+36);

    return eslot;
}

////////////////////////////////////////////////////////////////////////////////

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
    fixp rx,rz,ry;  // deltas to the next dot in row
    fixp cx,cz,cy;  // deltas to the next dot in column
} dotpos_t;

static dotpos_t DOTPOS[6] = {
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

static int count_dots(blk *b) {
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

void build_update() {
    // player position or look have changed - update our placeable blocks list
    if (!build.active) return;

    int i;

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

        // determine usable dots on the neighbr faces
        lh_clear_obj(b->dots);
        int n;
        for (n=0; n<6; n++) {
            if (!((b->neigh>>n)&1)) continue;
            //TODO: provide support for position-dependent blocks
            memcpy(b->dots[n], DOTS_ALL, sizeof(DOTS_ALL));
        }

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
#define BUILD_BLKINT 50000

// maximum number of blocks to attempt to place in one go
#define BUILD_BLKMAX 3

void build_progress(MCPacketQueue *sq, MCPacketQueue *cq) {
    // time update - try to build any blocks from the placeable blocks list
    if (!build.active) return;

    uint64_t ts = gettimestamp();
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
        mat_last[islot] = ts;
        bc++;
    }

    // switch back to whatever the client was holding
    if (held != gs.inv.held)
        gmi_change_held(sq, cq, held, 0);
}

////////////////////////////////////////////////////////////////////////////////

static bid_t build_arg_material(char **words, char *reply) {
    bid_t mat;

    // determine the building material
    int bid=0, meta=0;
    // first option: BID,meta specified
    if (scan_opt(words, "mat=%d,%d", &bid, &meta)!=2) {
        // second option: just block ID, assume meta=0
        meta = 0;
        if (scan_opt(words, "mat=%d", &bid)!=1) {
            // third option: take the same material the player is currently holding
            slot_t *s = &gs.inv.slots[gs.inv.held+36];
            if (s->item < 0 || s->item==0 || ITEMS[s->item].flags&I_ITEM) {
                sprintf(reply, "You must specify material - either explicitly with "
                               "mat=<bid>[,<meta>] or by holding a placeable block");
                return mat;
            }
            else {
                bid = s->item;
                meta = s->damage;
                if (!(ITEMS[s->item].flags&I_MTYPE) && meta) {
                    printf("Warning: item %d is not I_MTYPE but has a non-zero damage value (%d)\n",
                           s->item, meta);
                    meta = 0;
                }
            }
        }
    }

    mat.bid = bid;
    mat.meta = meta;
    return mat;
}

static void build_floor(char **words, char *reply) {
    build_clear();

    // floor size
    int xsize,zsize;
    if (scan_opt(words, "size=%d,%d", &xsize, &zsize)!=2) {
        sprintf(reply, "Usage: build floor size=<xsize>,<zsize>");
        return;
    }
    if (xsize<=0 || zsize<=0) {
        sprintf(reply, "Usage: illegal floor size %d,%d",xsize,zsize);
        return;
    }

    bid_t mat = build_arg_material(words, reply);
    if (reply[0]) return;

    //TODO: for slab-type blocks, parse an additional option to select upper/lower placement

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
    sprintf(reply, "Created floor %dx%d material=%s\n",
            xsize, zsize, get_bid_name(buf, mat));
}

//TODO: orientation and rotate brec accordingly
static void build_place(char **words, char *reply) {
    // check if we have a plan
    if (!C(build.plan)) {
        sprintf(reply, "You have no active buildplan!\n");
        return;
    }

    // parse coords
    // TODO: placement at player's position
    int px,py,pz;
    if (scan_opt(words, "coord=%d,%d,%d", &px, &pz, &py)!=3) {
        sprintf(reply, "Usage: build place coord=<x>,<z>,<y>");
        return;
    }
    sprintf(reply, "Place pivot at %d,%d (%d)\n",px,pz,py);

    // parse placement direction
    int dir;
    if (scan_opt(words, "dir=%d", &dir)!=1) {
        // if not specified, derive from the player's look direction
        dir = player_direction();
    }

    if (dir<DIR_SOUTH || dir>DIR_WEST) {
        sprintf(reply, "incorrect direction code, use: SOUTH=2,NORTH=3,EAST=4,WEST=5");
        return;
    }

    // abort current buildtask
    build_cancel();

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

        //TODO: correct the I_MPOS-dependent metas
        bt->b = bp->b;
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

////////////////////////////////////////////////////////////////////////////////

//TODO: print needed material amounts
void build_dump_plan() {
    int i;
    char buf[256];
    for(i=0; i<C(build.plan); i++) {
        blkr *b = &P(build.plan)[i];
        printf("%3d %+4d,%+4d,%3d %3x/%02x (%s)\n",
               i, b->x, b->z, b->y, b->b.bid, b->b.meta, get_bid_name(buf, b->b));
    }
}

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

               get_bid_name(buf, b->b));
    }
}

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

               get_bid_name(buf, b->b));
    }
}

void build_clear() {
    build_cancel();
    lh_arr_free(BPLAN);
}

void build_cancel() {
    build.active = 0;
    lh_arr_free(BTASK);
    build.bq[0] = -1;
}

void build_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq) {
    char reply[32768];
    reply[0]=0;

    if (!words[1]) {
        sprintf(reply, "Usage: build <type> [ parameters ... ] or build cancel");
    }
    else if (!strcmp(words[1], "floor")) {
        build_floor(words+2, reply);
    }
    else if (!strcmp(words[1], "place")) {
        build_place(words+2, reply);
    }
    else if (!strcmp(words[1], "cancel")) {
        build_cancel();
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

    if (reply[0]) chat_message(reply, cq, "green", 0);
}
