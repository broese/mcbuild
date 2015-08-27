#include <assert.h>

#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_compress.h>

#include "mcp_bplan.h"
#include "mcp_ids.h"

#define BPP P(bp->plan)
#define BPC C(bp->plan)
#define BP  GAR(bp->plan)

#define MAX(x,y) (((x)>(y))?(x):(y))
#define MIN(x,y) (((x)<(y))?(x):(y))
#define SQ(x)    ((x)*(x))

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
        printf("%3d %+4d,%+4d,%3d %3x/%02x (%s)\n",
               i, b->x, b->z, b->y, b->b.bid, b->b.meta,
               get_bid_name(buf, get_base_material(b->b)));
    }
}

////////////////////////////////////////////////////////////////////////////////

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

bplan * bplan_disk(int32_t diam, bid_t mat) {
    lh_create_obj(bplan, bp);

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

bplan * bplan_ball(int32_t diam, bid_t mat) {
    lh_create_obj(bplan, bp);

    int x,y,z,min,max;
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

    for(y=min; y<=max; y++) {
        for(x=min; x<=max; x++) {
            for(z=min; z<=max; z++) {
                float sqdist = SQ((float)x-c)+SQ((float)y-c)+SQ((float)z-c);
                if (sqdist < SQ(((float)diam)/2)) {
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

    //TODO: different material for the stairs base,
    //      or even automatically match material
    //      (e.g. Sandstone stairs => Sandstone)

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
                b->b = mat;
                b->x = x;
                b->z = -floor-1;
                b->y = floor;
            }
        }
    }
    return bp;
}

////////////////////////////////////////////////////////////////////////////////
// Buildplan manipulation

int bplan_hollow(bplan *bp, int flat) {
    assert(bp);
    bplan_update(bp);

    int i,j;

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

        // TODO: set the blocks not occupying the whole block (like slabs,
        //       stairs, torches, etc) as empty as they don't seal the surface
        v[off] = b->b.bid ? 1 : 0;
    }

    // mark the blocks completely surrounded by other blocks for removal
    int x,y,z;
    for(y=1; y<bp->sy+1; y++) {
        int32_t off_y = size_xz*y;
        for(z=1; z<bp->sz+1; z++) {
            for(x=1; x<bp->sx+1; x++) {
                int32_t off = off_y + x + z*(bp->sx+2);
                if (v[off] &&
                    v[off-1] && v[off+1] &&
                    v[off-(bp->sx+2)] && v[off+(bp->sx+2)] &&
                    ((v[off-size_xz] && v[off+size_xz]) || flat) )
                    v[off] = -1;
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

        if (v[off] == 1) {
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

int bplan_replace(bplan *bp, bid_t mat1, bid_t mat2) {
    // TODO: handle material replacement for the orientation-dependent metas

    // if mat2=Air, blocks will be removed.
    // this array will store all blocks we keep
    lh_arr_declare_i(blkr, keep);

    int i, count=0;
    for(i=0; i<BPC; i++) {
        if (BPP[i].b.raw == mat1.raw) {
            count++;
            if (mat2.bid != 0) {
                blkr *k = lh_arr_new(GAR(keep));
                *k = BPP[i];
                k->b = mat2;
            }
            // else - the block will be removed
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

////////////////////////////////////////////////////////////////////////////////

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
    return bp;
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
                if (!NOSCAN(blocks[i])) {
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

////////////////////////////////////////////////////////////////////////////////

#if TEST

int main(int ac, char **av) {
    bplan *bp = bplan_floor(5,3,BLOCKTYPE(5,2));
    bplan_update(bp);
    bplan_dump(bp);
    bplan_free(bp);
}

#endif
