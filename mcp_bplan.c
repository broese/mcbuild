#include <assert.h>

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

////////////////////////////////////////////////////////////////////////////////

#if TEST

int main(int ac, char **av) {
    bplan *bp = bplan_floor(5,3,BLOCKTYPE(5,2));
    bplan_update(bp);
    bplan_dump(bp);
    bplan_free(bp);
}

#endif
