#include <assert.h>

#include "mcp_bplan.h"

#define BPP   P(bp->plan)
#define BPC   C(bp->plan)
#define BPLAN GAR(bp->plan)

#define MAX(x,y) (((x)>(y))?(x):(y))
#define MIN(x,y) (((x)<(y))?(x):(y))

void bplan_free(bplan * bp) {
    if (!bp) return;
    lh_arr_free(BPLAN);
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
