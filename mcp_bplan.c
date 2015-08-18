#include <assert.h>

#include "mcp_bplan.h"
#include "mcp_ids.h"

#define BPP P(bp->plan)
#define BPC C(bp->plan)
#define BP  GAR(bp->plan)

#define MAX(x,y) (((x)>(y))?(x):(y))
#define MIN(x,y) (((x)<(y))?(x):(y))

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

////////////////////////////////////////////////////////////////////////////////

#if TEST

int main(int ac, char **av) {
    bplan *bp = bplan_floor(5,3,BLOCKTYPE(5,2));
    bplan_update(bp);
    bplan_dump(bp);
    bplan_free(bp);
}

#endif
