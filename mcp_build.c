#include <stdarg.h>

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

////////////////////////////////////////////////////////////////////////////////
// Structures

#define DIR_UP      0
#define DIR_DOWN    1
#define DIR_SOUTH   2
#define DIR_NORTH   3
#define DIR_EAST    4
#define DIR_WEST    5

// this structure is used to define an absolute block placement
// in the active building process
typedef struct {
    int32_t     x,y,z;          // coordinates of the block to place
    bid_t       b;              // block type, including the meta

    union {
        int8_t  state;
        struct {
            int8_t placed : 1;  // true if this block is already in place
            int8_t blocked : 1; // true if this block is obstructed by something else
            int8_t inreach : 1; // this block is close enough to place
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
} blk;

// this structure defines a relative block placement 
typedef struct {
    int32_t     x,y,z;  // coordinates of the block to place (relative to pivot)
    bid_t       b;      // block type, including the meta
                        // positional meta is north-oriented
} blkr;

struct {
    int active;
    lh_arr_declare(blk,task);  // current active building task
    lh_arr_declare(blkr,plan); // currently loaded/created buildplan
} build;

#define BTASK GAR(build.task)
#define BPLAN GAR(build.plan)

////////////////////////////////////////////////////////////////////////////////

void build_update() {
    // player position or look have changed - update our placeable blocks list
    if (!build.active) return;

    //TODO: recalculate placeable blocks list
}

void build_progress() {
    // time update - try to build any blocks from the placeable blocks list

    //TODO: select one of the blocks from the buildable list and place it
}

////////////////////////////////////////////////////////////////////////////////

static void build_floor(char **words, char *reply) {
    build_clear();

    int xsize,zsize;
    if (scan_opt(words, "size=%d,%d", &xsize, &zsize)!=2) {
        sprintf(reply, "Usage: build floor size=<xsize>,<zsize>");
        return;
    }
    if (xsize<=0 || zsize<=0) return;

    //TODO: material
    bid_t mat = { .bid=0x04, .meta=0 };

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
        printf("%3d %+5d,%+5d,%3d %3x/%02x (%s)\n",
               i, b->x, b->z, b->y, b->b.bid, b->b.meta, get_bid_name(buf, b->b));
    }
}

void build_clear() {
    build.active = 0;
    lh_arr_free(BTASK);
    lh_arr_free(BPLAN);
}

void build_cancel() {
    build.active = 0;
    lh_arr_free(BTASK);
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
    else if (!strcmp(words[1], "cancel")) {
        build_cancel();
    }
    else if (!strcmp(words[1], "dumpplan")) {
        build_dump_plan();
    }
    else if (!strcmp(words[1], "dumptask")) {
        build_dump_task();
    }

    if (reply[0]) chat_message(reply, cq, "green", 0);
}
