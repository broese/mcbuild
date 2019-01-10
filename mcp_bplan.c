/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include <assert.h>
#include <errno.h>
#include <math.h>

#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_compress.h>
#include <lh_image.h>
#include <lh_debug.h>

#include "mcp_bplan.h"
#include "mcp_ids.h"

#define BPP P(bp->plan)
#define BPC C(bp->plan)
#define BP  GAR(bp->plan)

void bplan_free(bplan * bp) {
    if (!bp) return;
    lh_arr_free(BP);
}

void bplan_update(bplan * bp) {
    assert(bp);

    if (BPC==0) {
        // no blocks in the buildplan
        bp->maxx=bp->maxy=bp->maxz = 0;
        bp->minx=bp->miny=bp->minz = 0;
        bp->sx=bp->sy=bp->sz = 0;
        return;
    }

    bp->maxx=bp->minx = BPP[0].x;
    bp->maxy=bp->miny = BPP[0].y;
    bp->maxz=bp->minz = BPP[0].z;

    int i;
    for(i=0; i<BPC; i++) {
        bp->maxx = MAX(BPP[i].x, bp->maxx);
        bp->minx = MIN(BPP[i].x, bp->minx);
        bp->maxy = MAX(BPP[i].y, bp->maxy);
        bp->miny = MIN(BPP[i].y, bp->miny);
        bp->maxz = MAX(BPP[i].z, bp->maxz);
        bp->minz = MIN(BPP[i].z, bp->minz);
    }

    bp->sx = bp->maxx-bp->minx+1;
    bp->sy = bp->maxy-bp->miny+1;
    bp->sz = bp->maxz-bp->minz+1;
}

void bplan_dump(bplan *bp) {
    if (!bp || BPC==0) {
        printf("Buildplan is empty\n");
        return;
    }

    printf("Buildplan: %zd blocks, W:%d D:%d H:%d\n",
           BPC, bp->sx, bp->sz, bp->sy);
    int i;
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;
        char buf[256];
        printf("%3d %+4d,%+4d,%3d   %4d (%s)\n",
               i, b->x, b->z, b->y,

               b->b.raw, db_get_blk_name(b->b.raw));
    }
}

// add a new block to the buildplan. If a block with this coordinates
// is already in the buildplan, it will be replaced
// returns the index of the newly added block
int bplan_add(bplan *bp, blkr block) {
    assert(bp);

    int i;
    for(i=0; i<BPC; i++) {
        blkr *ob = BPP+i;
        if (ob->x==block.x && ob->y==block.y && ob->z==block.z) {
            ob->b = block.b;
            return i;
        }
    }

    blkr *nb = lh_arr_new(BP);
    *nb = block;
    return BPC-1;
}

////////////////////////////////////////////////////////////////////////////////
// Helpers

// translate and rotate the block between absolute and relative positions
// around a pivot block

blkr abs2rel(pivot_t pv, blkr b) {
    blkr r;
    switch(pv.dir) {
        case DIR_SOUTH:
            r.x=pv.pos.x-b.x;
            r.z=pv.pos.z-b.z;
            r.b=rotate_meta(b.b,-2);
            break;
        case DIR_NORTH:
            r.x=b.x-pv.pos.x;
            r.z=b.z-pv.pos.z;
            r.b=rotate_meta(b.b,0);
            break;
        case DIR_EAST:
            r.x=b.z-pv.pos.z;
            r.z=pv.pos.x-b.x;
            r.b=rotate_meta(b.b,-1);
            break;
        case DIR_WEST:
            r.x=pv.pos.z-b.z;
            r.z=b.x-pv.pos.x;
            r.b=rotate_meta(b.b,-3);
            break;
        default: assert(0);
    }
    r.y = b.y-pv.pos.y;
    return r;
}

blkr rel2abs(pivot_t pv, blkr b) {
    blkr r;
    switch(pv.dir) {
        case DIR_SOUTH:
            r.x=pv.pos.x-b.x;
            r.z=pv.pos.z-b.z;
            r.b=rotate_meta(b.b,2);
            break;
        case DIR_NORTH:
            r.x=pv.pos.x+b.x;
            r.z=pv.pos.z+b.z;
            r.b=rotate_meta(b.b,0);
            break;
        case DIR_EAST:
            r.x=pv.pos.x-b.z;
            r.z=pv.pos.z+b.x;
            r.b=rotate_meta(b.b,1);
            break;
        case DIR_WEST:
            r.x=pv.pos.x+b.z;
            r.z=pv.pos.z-b.x;
            r.b=rotate_meta(b.b,3);
            break;
        default: assert(0);
    }
    r.y = pv.pos.y+b.y;
    return r;
}


////////////////////////////////////////////////////////////////////////////////
// Parametric builds

bplan * bplan_floor(int32_t wd, int32_t dp, bid_t mat) {
    lh_create_obj(bplan, bp);
    int x,z;
    for(z=0; z<dp; z++) {
        for(x=0; x<wd; x++) {
            blkr *b = lh_arr_new(BP);
            b->b = mat;
            b->x = x;
            b->z = -z;
            b->y = 0;
        }
    }
    return bp;
}

bplan * bplan_wall(int32_t wd, int32_t hg, bid_t mat) {
    lh_create_obj(bplan, bp);
    int x,y;
    for(x=0; x<wd; x++) {
        for(y=0; y<hg; y++) {
            blkr *b = lh_arr_new(BP);
            b->b = mat;
            b->x = x;
            b->z = 0;
            b->y = y;
        }
    }
    return bp;
}

bplan * bplan_disk(float diam, bid_t mat, int edge) {
    lh_create_obj(bplan, bp);

    int x,z,min,max;
    blkr *b;
    float c;

    max = (int)ceilf(diam/2);
    min = -max;
    c=edge?-0.5:0.0;

    for(x=min; x<=max; x++) {
        for(z=min; z<=max; z++) {
            float sqdist = SQ((float)x-c)+SQ((float)z-c);
            if (sqdist <= SQ(diam/2)) {
                b = lh_arr_new(BP);
                b->b = mat;
                b->y = 0;
                b->x = x;
                b->z = z;
            }
        }
    }
    return bp;
}

bplan * bplan_ball(float diam, bid_t mat, int edge) {
    lh_create_obj(bplan, bp);

    int x,y,z,min,max;
    blkr *b;
    float c;

    max = (int)ceilf(diam/2);
    min = -max;
    c=edge?-0.5:0.0;

    for(y=min; y<=max; y++) {
        for(x=min; x<=max; x++) {
            for(z=min; z<=max; z++) {
                float sqdist = SQ((float)x-c)+SQ((float)y-c)+SQ((float)z-c);
                if (sqdist <= SQ(diam/2)) {
                    b = lh_arr_new(BP);
                    b->b = mat;
                    b->y = y;
                    b->x = x;
                    b->z = z;
                }
            }
        }
    }
    return bp;
}

bplan * bplan_scaffolding(int wd, int hg, bid_t mat, int ladder) {
    int SCAFF_STAIR[5][2] = { { 1, -1}, { 2, -1 }, { 2, 0 }, { 3, 0 }, { 3, 1 } };

    lh_create_obj(bplan, bp);

    int floor;
    for(floor=0; floor<hg; floor++) {
        int i;
        blkr *b;
        // bridge
        for(i=0; i<wd; i++) {
            b = lh_arr_new(BP);
            b->b = mat;
            b->x = i;
            b->z = 0;
            b->y = 2+3*floor;
        }
        // column
        for(i=0; i<2; i++) {
            b = lh_arr_new(BP);
            b->b = mat;
            b->x = 0;
            b->z = 0;
            b->y = i+3*floor;
        }
        // stairs
        if (ladder) {
            for(i=0; i<3; i++) {
                b = lh_arr_new(BP);
                b->b = BLOCKTYPE(65,3);
                b->x = 0;
                b->z = 1;
                b->y = i+3*floor;
            }
            b = lh_arr_new(BP);
            b->b = mat;
            b->x = 1;
            b->z = 1;
            b->y = 2+3*floor;
        }
        else {
            for(i=0; i<5; i++) {
                b = lh_arr_new(BP);
                b->b = mat;
                b->x = SCAFF_STAIR[i][0]+3*(floor&1);
                b->z = 1;
                b->y = SCAFF_STAIR[i][1]+3*floor;
            }
        }
    }

    return bp;
}

bplan * bplan_stairs(int32_t wd, int32_t hg, bid_t mat, int base) {
    lh_create_obj(bplan, bp);

    // make stairs-type blocks face player
    if ((ITEMS[mat.bid].flags&I_STAIR)) mat.meta=3;

    // unless -exact flag was specified, we choose a suitable material for the base
    bid_t bmat = mat;
    if (base > 0)
        bmat = get_base_block_material(mat);
    else
        base = -base;

    int floor;
    for(floor=0; floor<hg; floor++) {
        int x;
        blkr *b;
        for(x=0; x<wd; x++) {
            b = lh_arr_new(BP);
            b->b = mat;
            b->x = x;
            b->z = -floor;
            b->y = floor;

            if (floor!=(hg-1) && (base==2 || (base==1 && x==0))) {
                b = lh_arr_new(BP);
                b->b = bmat;
                b->x = x;
                b->z = -floor-1;
                b->y = floor;
            }
        }
    }
    return bp;
}

bplan * bplan_seal() {
    lh_create_obj(bplan, bp);
    int i;
    for(i=0; i<1000; i++) {
        blkr *b;
        b = lh_arr_new(BP);
        b->b = BLOCKTYPE(0x57,0);
        b->x = 0;
        b->y = 0;
        b->z = i;
        b = lh_arr_new(BP);
        b->b = BLOCKTYPE(0x57,0);
        b->x = 0;
        b->y = 1;
        b->z = i;
    }

    return bp;
}

////////////////////////////////////////////////////////////////////////////////
// Buildplan manipulation

int bplan_hollow(bplan *bp, int flat, int opaque) {
    assert(bp);
    bplan_update(bp);

    int i;

    // size of a single "slice" with additional 1-block border
    int32_t size_xz = (bp->sx+2)*(bp->sz+2);

    // this array will hold the entire buildplan as a "voxel set"
    // with values set to 0 for empty blocks, 1 for occupied and -1 for removed
    // note - we leave an additional empty block for the border on each side
    lh_create_num(int8_t,v,size_xz*(bp->sy+2));

    // initialize the voxel set from the buildplan
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;

        // offset of this block in the array
        int32_t off_x = b->x-bp->minx+1;
        int32_t off_y = b->y-bp->miny+1;
        int32_t off_z = b->z-bp->minz+1;
        int32_t off   = off_x+off_z*(bp->sx+2)+off_y*size_xz;

        if (b->b.bid) {
            // block is present
            v[off] = 1;
            if (!opaque || (ITEMS[b->b.bid].flags&I_OPAQUE))
                v[off] += 2; // Bit 1 - block is considered opaque
        }
    }

    // mark the blocks completely surrounded by other blocks for removal
    int x,y,z;
    for(y=1; y<bp->sy+1; y++) {
        int32_t off_y = size_xz*y;
        for(z=1; z<bp->sz+1; z++) {
            for(x=1; x<bp->sx+1; x++) {
                int32_t off = off_y + x + z*(bp->sx+2);
                if ( (v[off]&1) &&
                     (v[off-1]&2) && (v[off+1]&2) &&
                     (v[off-(bp->sx+2)]&2) && (v[off+(bp->sx+2)]&2) &&
                     (((v[off-size_xz]&2) && (v[off+size_xz]&2))||flat) )
                     v[off]&=0xfe; // disable "present" bit
            }
        }
    }

    // new list for the build plan to hold only the blocks we keep after removal
    lh_arr_declare_i(blkr, keep);
    int removed=0;

    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;

        // offset of this block in the array
        int32_t off_x = b->x-bp->minx+1;
        int32_t off_y = b->y-bp->miny+1;
        int32_t off_z = b->z-bp->minz+1;
        int32_t off   = off_x+off_z*(bp->sx+2)+off_y*size_xz;

        if (v[off]&1) {
            blkr *k = lh_arr_new(GAR(keep));
            *k = *b;
        }
        else
            removed++;
    }

    // free our voxel set
    lh_free(v);

    // replace the buildplan with the reduced list
    lh_arr_free(BP);
    BPC = C(keep);
    BPP = P(keep);

    return removed;
}

void bplan_extend(bplan *bp, int ox, int oz, int oy, int count) {
    int i,j;
    int bc=BPC;
    for(i=1; i<=count; i++) {
        for(j=0; j<bc; j++) {
            blkr *bn = lh_arr_new(BP);
            blkr *bo = BPP+j;
            bn->x = bo->x+ox*i;
            bn->y = bo->y+oy*i;
            bn->z = bo->z+oz*i;
            bn->b = bo->b;
        }
    }
}

int bplan_replace(bplan *bp, bid_t mat1, bid_t mat2, int anymeta) {
    // TODO: handle material replacement for the orientation-dependent metas

    // if mat2=Air, blocks will be removed.
    // this array will store all blocks we keep
    lh_arr_declare_i(blkr, keep);

    int i, count=0;
    for(i=0; i<BPC; i++) {
        int match = anymeta ? (BPP[i].b.bid == mat1.bid) : (BPP[i].b.raw == mat1.raw);

        if (match) {
            count++;
            if (mat2.bid != 0) {
                blkr *k = lh_arr_new(GAR(keep));
                *k = BPP[i];
                k->b.bid = mat2.bid;
                k->b.meta = anymeta ? BPP[i].b.meta : mat2.meta;
            }
            // else - block will be removed
        }
        else {
            blkr *k = lh_arr_new(GAR(keep));
            *k = BPP[i];
        }
    }

    lh_arr_free(BP);
    BPC = C(keep);
    BPP = P(keep);

    return count;
}

// trim the buildplan by erasing block not fitting the criteria
int bplan_trim(bplan *bp, int type, int32_t value) {
    lh_arr_declare_i(blkr, keep);

    int i, count=0;
    for(i=0; i<BPC; i++) {
        int k=0;
        int32_t x=BPP[i].x;
        int32_t y=BPP[i].y;
        int32_t z=BPP[i].z;
        switch (type) {
            case TRIM_XE: k=(x==value); break;
            case TRIM_XL: k=(x<value); break;
            case TRIM_XG: k=(x>value); break;
            case TRIM_YE: k=(y==value); break;
            case TRIM_YL: k=(y<value); break;
            case TRIM_YG: k=(y>value); break;
            case TRIM_ZE: k=(z==value); break;
            case TRIM_ZL: k=(z<value); break;
            case TRIM_ZG: k=(z>value); break;
        }
        if (k) {
            blkr *k = lh_arr_new(GAR(keep));
            *k = BPP[i];
        }
        else {
            count++; // count removed blocks
        }
    }

    lh_arr_free(BP);
    BPC = C(keep);
    BPP = P(keep);

    return count;
}

// flip the buildplan across one of the axis
void bplan_flip(bplan *bp, char mode) {
    int i;
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;
        switch (mode) {
            case 'x':
                b->x = -b->x;
                b->b = flip_meta(b->b, 'x');
                break;
            case 'y':
                b->y = -b->y;
                b->b = flip_meta_y(b->b);
                break;
            case 'z':
                b->z = -b->z;
                b->b = flip_meta(b->b, 'z');
                break;
        }
    }
}

// flip the buildplan across one of the axis
void bplan_tilt(bplan *bp, char mode) {
    int i;
    int32_t x,y,z;
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;
        switch (mode) {
            case 'x':
                y = b->z;
                z = -b->y;
                b->y = y;
                b->z = z;
                //TODO: support meta correction for x/z-tilt if possible
                break;
            case 'y':
                x = -b->z;
                z = b->x;
                b->x = x;
                b->z = z;
                b->b = rotate_meta(b->b, 1);
                break;
            case 'z':
                x = b->y;
                y = -b->x;
                b->x = x;
                b->y = y;
                break;
        }
    }
}

// shift the buildplan so that the pivot is at the bottom leftmost near corner
void bplan_normalize(bplan *bp) {
    bplan_update(bp);
    int i;
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;
        b->x += -bp->minx;
        b->y += -bp->miny;
        b->z -= bp->maxz;
    }
    bplan_update(bp);
}

// shrink the buildplan to half size in each dimension
void bplan_shrink(bplan *bp) {
    assert(bp);
    bplan_update(bp);

    int i;

    // size of a single "slice" with additional 1-block border
    int32_t size_xz = bp->sx*bp->sz;
    lh_create_num(bid_t,v,size_xz*bp->sy);

    // initialize the voxel set from the buildplan
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;

        // offset of this block in the array
        int32_t off_x = b->x-bp->minx;
        int32_t off_y = b->y-bp->miny;
        int32_t off_z = b->z-bp->minz;
        int32_t off   = off_x+off_z*bp->sx+off_y*size_xz;
        v[off] = b->b;
    }

    // new list for the build plan to hold the blocks from the shrinked model
    lh_arr_declare_i(blkr, keep);

    int x,y,z;
    for(y=bp->miny; y<bp->maxy-1; y+=2) {
        for(x=bp->minx; x<bp->maxx-1; x+=2) {
            for(z=bp->minz; z<bp->maxz-1; z+=2) {
                int32_t off_x = x-bp->minx;
                int32_t off_y = y-bp->miny;
                int32_t off_z = z-bp->minz;
                int32_t off   = off_x+off_z*bp->sx+off_y*size_xz;

                int32_t offs[8] = {
                    off,
                    off+1,
                    off+bp->sx,
                    off+1+bp->sx,
                    off+size_xz,
                    off+1+size_xz,
                    off+bp->sx+size_xz,
                    off+1+bp->sx+size_xz
                };

                int blk[256]; lh_clear_obj(blk);
                for (i=0; i<8; i++) {
                    int bid = v[offs[i]].bid;
                    blk[bid]++;
                }
                bid_t mat = BLOCKTYPE(0,0);
                for(i=1; i<256; i++)
                    if (blk[i]>4)
                        mat = BLOCKTYPE(i,0);

                if (mat.bid) {
                    blkr *k = lh_arr_new(GAR(keep));
                    k->b = mat;
                    k->x = bp->minx + (x-bp->minx)/2;
                    k->y = bp->miny + (y-bp->miny)/2;
                    k->z = bp->minz + (z-bp->minz)/2;
                }
            }
        }
    }

    // free our voxel set
    lh_free(v);

    // replace the buildplan with the reduced list
    lh_arr_free(BP);
    BPC = C(keep);
    BPP = P(keep);

    bplan_update(bp);
}

// scale up the buildplan
void bplan_scale(bplan *bp, int scale) {
    assert(bp);

    // new list for the build plan to hold the blocks from the scaled model
    lh_arr_declare_i(blkr, keep);

    int i,x,y,z;
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;
        for(x=0; x<scale; x++) {
            for(y=0; y<scale; y++) {
                for(z=0; z<scale; z++) {
                    blkr *k = lh_arr_new(GAR(keep));
                    k->x = b->x*scale+x;
                    k->y = b->y*scale+y;
                    k->z = b->z*scale+z;
                    k->b = b->b;
                }
            }
        }
    }

    // replace the buildplan with the reduced list
    lh_arr_free(BP);
    BPC = C(keep);
    BPP = P(keep);

    bplan_update(bp);
}

////////////////////////////////////////////////////////////////////////////////

int bplan_save(bplan *bp, const char *name) {
    char fname[256];
    sprintf(fname, "bplan/%s.bplan", name);

    // write all encoded blocks from the buildplan to the buffer
    lh_create_buf(buf, sizeof(blkr)*BPC);
    int i;
    uint8_t *w = buf;
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;
        lh_write_int_be(w, b->x);
        lh_write_int_be(w, b->y);
        lh_write_int_be(w, b->z);
        lh_write_short_be(w, b->b.raw);
    }

    // write out to file
    ssize_t sz = lh_save(fname, buf, w-buf);
    lh_free(buf);

    // return nonzero on success
    return (sz>0) ? 1 : 0;
}

bplan * bplan_load(const char *name) {
    char fname[256];
    sprintf(fname, "bplan/%s.bplan", name);

    // load the file into a buffer
    uint8_t *buf;
    ssize_t sz = lh_load_alloc(fname, &buf);
    if (sz <= 0) return NULL; // error reading file

    // create a new buildplan
    lh_create_obj(bplan, bp);

    // read the blocks and add them to the buildplan
    uint8_t *p = buf;
    while(p<buf+sz-13) {
        blkr *b = lh_arr_new(BP);
        b->x = lh_read_int_be(p);
        b->y = lh_read_int_be(p);
        b->z = lh_read_int_be(p);
        b->b.raw = lh_read_short_be(p);
    }

    lh_free(buf);

    bplan_update(bp);
    return bp;
}

int bplan_ssave(bplan *bp, const char *name) {
    assert(bp);
    bplan_update(bp);

    char fname[256];
    sprintf(fname, "schematic/%s.schematic", name);

    int i;

    // create Blocks and Data arrays from the buildplan
    int32_t size_xz = bp->sx*bp->sz;
    int32_t size = size_xz*bp->sy;
    lh_create_num(uint8_t,blocks,size);
    lh_create_num(uint8_t,data,size);

    // initialize the voxel set from the buildplan
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;

        // offset of this block in the array
        int32_t off_x = b->x-bp->minx;
        int32_t off_y = b->y-bp->miny;
        int32_t off_z = b->z-bp->minz;
        int32_t off   = off_x+off_z*bp->sx+off_y*size_xz;

        blocks[off] = b->b.bid;
        data[off]   = b->b.meta;
    }

    // Construct the schematic NBT structure
    nbt_t * Schematic = nbt_new(NBT_COMPOUND, "Schematic", 8,
        nbt_new(NBT_SHORT, "Height", bp->sy),
        nbt_new(NBT_SHORT, "Length", bp->sz),
        nbt_new(NBT_SHORT, "Width", bp->sx),
        nbt_new(NBT_STRING, "Materials", "Alpha"),
        nbt_new(NBT_LIST, "Entities", 0),
        nbt_new(NBT_LIST, "TileEntities", 0),
        nbt_new(NBT_BYTE_ARRAY, "Blocks", blocks, size),
        nbt_new(NBT_BYTE_ARRAY, "Data", data, size));

    // Serialize the NBT data
    ssize_t ssize = 2*size + 65536;
    lh_create_num(uint8_t,sdata,ssize);
    uint8_t *w = sdata;
    nbt_write(&w, Schematic);
    ssize_t ssize2 = w-sdata;
    printf("Estimated %zd, serialized %zd\n",ssize,ssize2);

    // Compress the data
    ssize_t clen;
    uint8_t * cdata = lh_gzip_encode(sdata, ssize2, &clen);
    if (!cdata) return 0;
    printf("Compressed: %zd\n",clen);

    // Write to file
    ssize_t wbytes = lh_save(fname, cdata, clen);
    return wbytes>0;
}

bplan * bplan_sload(const char *name) {
    char fname[256];
    sprintf(fname, "schematic/%s.schematic", name);

    // load the file into a buffer
    uint8_t *buf;
    ssize_t sz = lh_load_alloc(fname, &buf);
    if (sz <= 0) return NULL; // error reading file

    // uncompress
    ssize_t dlen;
    uint8_t *dbuf = lh_gzip_decode(buf, sz, &dlen);
    if (!dbuf) {
        printf("Failed to uncompress %s\n",fname);
        return NULL;
    }

    // parse the NBT structure
    uint8_t *p = dbuf;
    nbt_t *n = nbt_parse(&p);
    if (!n || (p-dbuf)!=dlen) {
        printf("Error parsing NBT data from %s", fname);
        return NULL;
    }

    // extract the NBT elements relevant for us
    //nbt_dump(n);
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

    // create a new buildplan
    lh_create_obj(bplan, bp);
    // scan the Blocks data for solid blocks
    int x,y,z,i=0;
    for(y=0; y<hg; y++) {
        for (z=0; z<ln; z++) {
            for (x=0; x<wd; x++) {
                if (!db_blk_is_noscan(blocks[i])) { //FIXME: check if this is the correct block type
                    blkr *b = lh_arr_new(BP);
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

    // cleanup
    nbt_free(n);
    lh_free(dbuf);
    lh_free(buf);

    return bp;
}

int bplan_csvsave(bplan *bp, const char *name) {
    assert(bp);
    bplan_update(bp);

    char fname[256];
    sprintf(fname, "csv/%s.csv", name);

    FILE * csv = fopen(fname, "w");
    if (!csv) {
        printf("Error opening %s for writing : %s\n",fname, strerror(errno));
        return 0;
    }
    fprintf(csv,"x,y,z,bid,meta\n");

    int i;
    for(i=0; i<BPC; i++) {
        blkr *b = BPP+i;
        if (fprintf(csv, "%d,%d,%d,%d,%d\n",b->x,b->y,b->z,b->b.bid,b->b.meta)<0) {
            printf("Error writing to %s : %s\n",fname, strerror(errno));
            fclose(csv);
            return 0;
        }
    }
    fclose(csv);

    return 1;
}

bplan * bplan_csvload(const char *name) {
    char fname[256];
    sprintf(fname, "csv/%s.csv", name);

    FILE * csv = fopen(fname, "r");
    if (!csv) {
        printf("Error opening %s for reading : %s\n",fname, strerror(errno));
        return 0;
    }

    char buf[4096];

    // skip header
    if (!fgets(buf, sizeof(buf), csv)) {
        printf("Error reading from %s : %s\n",fname, strerror(errno));
        fclose(csv);
        return NULL;
    }

    // read blocks
    lh_create_obj(bplan, bp);
    while(1) {
        char * r = fgets(buf, sizeof(buf), csv);
        if (!r) {
            if (feof(csv)) break;
            printf("Error reading from %s : %s\n",fname, strerror(errno));
            lh_free(bp);
            fclose(csv);
            return NULL;
        }

        blkr *b = lh_arr_new(BP);
        int bid,meta;
        if (sscanf(r, "%d,%d,%d,%d,%d", &b->x, &b->y, &b->z, &bid, &meta)!=5) {
            printf("Error parsing line %s from %s\n",r,fname);
            lh_free(bp);
            fclose(csv);
            return NULL;
        }
        b->b.bid = bid;
        b->b.meta = meta;
    }
    fclose(csv);

    bplan_update(bp);
    return bp;
}

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    int32_t color;
    bid_t    b;
} cmap_t;

static inline bid_t match_color(uint32_t c, cmap_t * set) {
    c &= 0x00ffffff;
    int i, bi=0, bdiff=0x7fffffff;

    for(i=0; set[i].b.bid; i++) {
        uint32_t mc = set[i].color;
        int rd = ((c>>16)&0xff)-((mc>>16)&0xff);
        int gd = ((c>>8)&0xff) -((mc>>8)&0xff);
        int bd = (c&0xff)      -(mc&0xff);
        int diff = rd*rd+gd*gd+bd*bd;
        if (diff < bdiff) { bi = i; bdiff = diff; }
    }

    return set[bi].b;
}

bplan * bplan_pngload(const char *name, const char *setname) {
    cmap_t CMAP_WOOL[] = {
        { 0xDEDEDE, BLOCKTYPE(35,0) },  // white
        { 0xdb7d3f, BLOCKTYPE(35,1) },  // orange
        { 0xb451bd, BLOCKTYPE(35,2) },  // magenta
        { 0x6b8ac9, BLOCKTYPE(35,3) },  // lblue
        { 0xB1A627, BLOCKTYPE(35,4) },  // yellow
        { 0x42ae39, BLOCKTYPE(35,5) },  // lime
        { 0xd08499, BLOCKTYPE(35,6) },  // pink
        { 0x404040, BLOCKTYPE(35,7) },  // gray
        { 0x9BA1A1, BLOCKTYPE(35,8) },  // lgray
        { 0x2f6f89, BLOCKTYPE(35,9) },  // cyan
        { 0x7f3eb6, BLOCKTYPE(35,10) }, // purple
        { 0x2e398e, BLOCKTYPE(35,11) }, // blue
        { 0x4f321f, BLOCKTYPE(35,12) }, // brown
        { 0x35471b, BLOCKTYPE(35,13) }, // green
        { 0x973431, BLOCKTYPE(35,14) }, // red
        { 0x1a1616, BLOCKTYPE(35,15) }, // black
        { 0x000000, BLOCKTYPE(0,0) },   // terminator
    };

    cmap_t CMAP_GLASS[] = {
        { 0xDBF2F5, BLOCKTYPE(20,0) },  // plain
        { 0xFFFFFF, BLOCKTYPE(95,0) },  // white
        { 0xD87F33, BLOCKTYPE(95,1) },  // orange
        { 0xB24CD8, BLOCKTYPE(95,2) },  // magenta
        { 0x6699D8, BLOCKTYPE(95,3) },  // lblue
        { 0xE5E533, BLOCKTYPE(95,4) },  // yellow
        { 0x7FCC19, BLOCKTYPE(95,5) },  // lime
        { 0xF27FA5, BLOCKTYPE(95,6) },  // pink
        { 0x4C4C4C, BLOCKTYPE(95,7) },  // gray
        { 0x999999, BLOCKTYPE(95,8) },  // lgray
        { 0x4C7F99, BLOCKTYPE(95,9) },  // cyan
        { 0x7F3FB2, BLOCKTYPE(95,10) }, // purple
        { 0x334CB2, BLOCKTYPE(95,11) }, // blue
        { 0x664C33, BLOCKTYPE(95,12) }, // brown
        { 0x667F33, BLOCKTYPE(95,13) }, // green
        { 0x993333, BLOCKTYPE(95,14) }, // red
        { 0x191919, BLOCKTYPE(95,15) }, // black
        { 0x000000, BLOCKTYPE(0,0) },   // terminator
    };

    cmap_t CMAP_CLAY[] = {
        { 0x975d43, BLOCKTYPE(172,0) },  // plain
        { 0xd2b2a1, BLOCKTYPE(159,0) },  // white
        { 0xa25426, BLOCKTYPE(159,1) },  // orange
        { 0x96586d, BLOCKTYPE(159,2) },  // magenta
        { 0x716d8a, BLOCKTYPE(159,3) },  // lblue
        { 0xba8523, BLOCKTYPE(159,4) },  // yellow
        { 0x687635, BLOCKTYPE(159,5) },  // lime
        { 0xa24e4f, BLOCKTYPE(159,6) },  // pink
        { 0x3a2a24, BLOCKTYPE(159,7) },  // gray
        { 0x876b62, BLOCKTYPE(159,8) },  // lgray
        { 0x575b5b, BLOCKTYPE(159,9) },  // cyan
        { 0x764656, BLOCKTYPE(159,10) }, // purple
        { 0x4a3c5b, BLOCKTYPE(159,11) }, // blue
        { 0x4d3324, BLOCKTYPE(159,12) }, // brown
        { 0x4c532a, BLOCKTYPE(159,13) }, // green
        { 0x8f3d2f, BLOCKTYPE(159,14) }, // red
        { 0x251710, BLOCKTYPE(159,15) }, // black
        { 0x000000, BLOCKTYPE(0,0) },   // terminator
    };

    // all colors
    cmap_t CMAP_MAP_ALL[] = {
        { 0x6e9a30, BLOCKTYPE(  2,  0) },   // Grass
        { 0xd5c98d, BLOCKTYPE( 44,  1) },   // Sandtone slab
        { 0x909090, BLOCKTYPE( 30,  0) },   // Cobweb
        { 0xdc0000, BLOCKTYPE(152,  0) },   // Redstone block
        { 0x8a8adc, BLOCKTYPE(174,  0) },   // Packed ice
        { 0x909090, BLOCKTYPE(148,  0) },   // Iron pressure plate
        { 0x006b00, BLOCKTYPE( 18,  0) },   // Oak leaves
        { 0xdcdcdc, BLOCKTYPE(171,  0) },   // White carpet
        { 0x8d919f, BLOCKTYPE( 82,  0) },   // Clay block
        { 0x9e5b29, BLOCKTYPE(126,  3) },   // Jungle wood slab
        { 0x616161, BLOCKTYPE( 44,  3) },   // Cobblestone slab
        { 0x3737dc, BLOCKTYPE(  9,  0) },   // Water
        { 0x5a482b, BLOCKTYPE(126,  0) },   // Oak wood slab
        { 0xdcd9d3, BLOCKTYPE(  1,  3) },   // Diorite
        { 0xba6e2c, BLOCKTYPE(171,  1) },   // Orange carpet
        { 0x9a42ba, BLOCKTYPE(171,  2) },   // Magenta carpet
        { 0x5884ba, BLOCKTYPE(171,  3) },   // Light blue carpet
        { 0xc6c62c, BLOCKTYPE(171,  4) },   // Yellow carpet
        { 0x6eb016, BLOCKTYPE(171,  5) },   // Lime carpet
        { 0xd16e8e, BLOCKTYPE(171,  6) },   // Pink carpet
        { 0x424242, BLOCKTYPE(171,  7) },   // Gray carpet
        { 0x848484, BLOCKTYPE(171,  8) },   // Light gray carpet
        { 0x426e84, BLOCKTYPE(171,  9) },   // Cyan carpet
        { 0x6e369a, BLOCKTYPE(171, 10) },   // Purple carpet
        { 0x2c429a, BLOCKTYPE(171, 11) },   // Blue carpet
        { 0x58422c, BLOCKTYPE(171, 12) },   // Brown carpet
        { 0x586e2c, BLOCKTYPE(171, 13) },   // Green carpet
        { 0x842c2c, BLOCKTYPE(171, 14) },   // Red carpet
        { 0x161616, BLOCKTYPE(171, 15) },   // Black carpet
        { 0xd8cd42, BLOCKTYPE(147,  0) },   // Gold pressure plate
        { 0x4fbdb8, BLOCKTYPE(168,  1) },   // Prismarine bricks
        { 0x406edc, BLOCKTYPE( 22,  0) },   // Lapis lazuli block
        { 0x00bb32, BLOCKTYPE(133,  0) },   // Emerald block
        { 0x12111b, BLOCKTYPE(126,  1) },   // Spruce Wood Slab
        { 0x610200, BLOCKTYPE( 87,  0) },   // Netherrack
        { 0x000000, BLOCKTYPE(0,0) },       // terminator
    };

    // all colors except inconvenient ones (cobweb/bed and water)
    cmap_t CMAP_MAP_DEFAULT[] = {
        { 0x6e9a30, BLOCKTYPE(  2,  0) },   // Grass
        { 0xd5c98d, BLOCKTYPE( 44,  1) },   // Sandtone slab
        { 0xdc0000, BLOCKTYPE(152,  0) },   // Redstone block
        { 0x8a8adc, BLOCKTYPE(174,  0) },   // Packed ice
        { 0x909090, BLOCKTYPE(148,  0) },   // Iron pressure plate
        { 0x006b00, BLOCKTYPE( 18,  0) },   // Oak leaves
        { 0xdcdcdc, BLOCKTYPE(171,  0) },   // White carpet
        { 0x8d919f, BLOCKTYPE( 82,  0) },   // Clay block
        { 0x9e5b29, BLOCKTYPE(126,  3) },   // Jungle wood slab
        { 0x616161, BLOCKTYPE( 44,  3) },   // Cobblestone slab
        { 0x5a482b, BLOCKTYPE(126,  0) },   // Oak wood slab
        { 0xdcd9d3, BLOCKTYPE(  1,  3) },   // Diorite
        { 0xba6e2c, BLOCKTYPE(171,  1) },   // Orange carpet
        { 0x9a42ba, BLOCKTYPE(171,  2) },   // Magenta carpet
        { 0x5884ba, BLOCKTYPE(171,  3) },   // Light blue carpet
        { 0xc6c62c, BLOCKTYPE(171,  4) },   // Yellow carpet
        { 0x6eb016, BLOCKTYPE(171,  5) },   // Lime carpet
        { 0xd16e8e, BLOCKTYPE(171,  6) },   // Pink carpet
        { 0x424242, BLOCKTYPE(171,  7) },   // Gray carpet
        { 0x848484, BLOCKTYPE(171,  8) },   // Light gray carpet
        { 0x426e84, BLOCKTYPE(171,  9) },   // Cyan carpet
        { 0x6e369a, BLOCKTYPE(171, 10) },   // Purple carpet
        { 0x2c429a, BLOCKTYPE(171, 11) },   // Blue carpet
        { 0x58422c, BLOCKTYPE(171, 12) },   // Brown carpet
        { 0x586e2c, BLOCKTYPE(171, 13) },   // Green carpet
        { 0x842c2c, BLOCKTYPE(171, 14) },   // Red carpet
        { 0x161616, BLOCKTYPE(171, 15) },   // Black carpet
        { 0xd8cd42, BLOCKTYPE(147,  0) },   // Gold pressure plate
        { 0x4fbdb8, BLOCKTYPE(168,  1) },   // Prismarine bricks
        { 0x406edc, BLOCKTYPE( 22,  0) },   // Lapis lazuli block
        { 0x00bb32, BLOCKTYPE(133,  0) },   // Emerald block
        { 0x12111b, BLOCKTYPE(126,  1) },   // Spruce Wood Slab
        { 0x610200, BLOCKTYPE( 87,  0) },   // Netherrack
        { 0x000000, BLOCKTYPE(0,0) },       // terminator
    };

    // colors only using cheaply available materials
    cmap_t CMAP_MAP_CHEAP[] = {
        { 0x6e9a30, BLOCKTYPE(  2,  0) },   // Grass
        { 0xd5c98d, BLOCKTYPE( 44,  1) },   // Sandtone slab
        { 0x006b00, BLOCKTYPE( 18,  0) },   // Oak leaves
        { 0xdcdcdc, BLOCKTYPE(171,  0) },   // White carpet
        { 0x9e5b29, BLOCKTYPE(126,  3) },   // Jungle wood slab
        { 0x616161, BLOCKTYPE( 44,  3) },   // Cobblestone slab
        { 0x5a482b, BLOCKTYPE(126,  0) },   // Oak wood slab
        { 0xdcd9d3, BLOCKTYPE(  1,  3) },   // Diorite
        { 0xba6e2c, BLOCKTYPE(171,  1) },   // Orange carpet
        { 0x9a42ba, BLOCKTYPE(171,  2) },   // Magenta carpet
        { 0x5884ba, BLOCKTYPE(171,  3) },   // Light blue carpet
        { 0xc6c62c, BLOCKTYPE(171,  4) },   // Yellow carpet
        { 0x6eb016, BLOCKTYPE(171,  5) },   // Lime carpet
        { 0xd16e8e, BLOCKTYPE(171,  6) },   // Pink carpet
        { 0x424242, BLOCKTYPE(171,  7) },   // Gray carpet
        { 0x848484, BLOCKTYPE(171,  8) },   // Light gray carpet
        { 0x426e84, BLOCKTYPE(171,  9) },   // Cyan carpet
        { 0x6e369a, BLOCKTYPE(171, 10) },   // Purple carpet
        { 0x2c429a, BLOCKTYPE(171, 11) },   // Blue carpet
        { 0x58422c, BLOCKTYPE(171, 12) },   // Brown carpet
        { 0x586e2c, BLOCKTYPE(171, 13) },   // Green carpet
        { 0x842c2c, BLOCKTYPE(171, 14) },   // Red carpet
        { 0x161616, BLOCKTYPE(171, 15) },   // Black carpet
        { 0x12111b, BLOCKTYPE(126,  1) },   // Spruce Wood Slab
        { 0x610200, BLOCKTYPE( 87,  0) },   // Netherrack
        { 0x000000, BLOCKTYPE(0,0) },       // terminator
    };

    // colors suitable for grayscale images
    cmap_t CMAP_MAP_GRAY[] = {
        { 0xdcdcdc, BLOCKTYPE(171,  0) },   // White carpet
        { 0x616161, BLOCKTYPE( 44,  3) },   // Cobblestone slab
        { 0xdcd9d3, BLOCKTYPE(  1,  3) },   // Diorite
        { 0x424242, BLOCKTYPE(171,  7) },   // Gray carpet
        { 0x848484, BLOCKTYPE(171,  8) },   // Light gray carpet
        { 0x161616, BLOCKTYPE(171, 15) },   // Black carpet
        { 0x000000, BLOCKTYPE(0,0) },       // terminator
    };

    char fname[256];
    sprintf(fname, "png/%s.png", name);

    int i;
    cmap_t *set=NULL;
    if (!setname || !setname[0]) {
        set=CMAP_WOOL;
    }
    else {
        if (!strcasecmp(setname, "wool"))       set = CMAP_WOOL;
        if (!strcasecmp(setname, "clay"))       set = CMAP_CLAY;
        if (!strcasecmp(setname, "glass"))      set = CMAP_GLASS;
        if (!strcasecmp(setname, "map"))        set = CMAP_MAP_DEFAULT;
        if (!strcasecmp(setname, "mapall"))     set = CMAP_MAP_ALL;
        if (!strcasecmp(setname, "mapcheap"))   set = CMAP_MAP_CHEAP;
        if (!strcasecmp(setname, "mapgray"))    set = CMAP_MAP_GRAY;
        if (!set) LH_ERROR(NULL, "No such color set \"%s\"", setname);
    }

    lhimage *img = import_png_file(fname);
    if (!img) LH_ERROR(NULL, "Failed to open %s as PNG", fname);

    lh_create_obj(bplan, bp);
    int x,y;

    for (y=0; y<img->height; y++) {
        int yy=img->height-y-1;
        uint32_t *row = img->data+yy*img->stride;

        for (x=0; x<img->width; x++) {
            uint32_t pixel = row[x];
            if (pixel&0xff000000) continue; // skip transparent pixels
            bid_t mat = match_color(pixel, set);

            blkr *b = lh_arr_new(BP);
            b->x = x;
            b->y = y;
            b->z = 0;
            b->b=mat;
        }
    }

    return bp;
}
