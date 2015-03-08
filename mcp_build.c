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

static void build_floor(char **words, char *reply) {
    build_clear();

    if (scan_opt(words, "size=%d,%d", &xsize, &zsize)!=2)
        sprintf(reply, "Usage: build floor size=<xsize>,<zsize>");

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

    if (reply[0]) chat_message(reply, cq, "green", 0);
}