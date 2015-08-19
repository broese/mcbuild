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

////////////////////////////////////////////////////////////////////////////////
// Buildplan manipulation

int bplan_hollow(bplan *bp) {
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
                    v[off-size_xz] && v[off+size_xz])
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


////////////////////////////////////////////////////////////////////////////////

#if TEST

int main(int ac, char **av) {
    bplan *bp = bplan_floor(5,3,BLOCKTYPE(5,2));
    bplan_update(bp);
    bplan_dump(bp);
    bplan_free(bp);
}

#endif
