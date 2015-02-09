#include <math.h>
#include <time.h>
#include <unistd.h>

#define LH_DECLARE_SHORT_NAMES 1

#include "lh_bytes.h"

#include "mcp_ids.h"
#include "mcp_gamestate.h"
#include "mcp_game.h"

////////////////////////////////////////////////////////////////////////////////
// helpers

uint64_t gettimestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ts = (uint64_t)tv.tv_sec*1000000+(uint64_t)tv.tv_usec;
}

#define MIN(a,b) ((a<b)?(a):(b))
#define MAX(a,b) ((a>b)?(a):(b))
#define SGN(x) (((x)>=0)?1:-1)
#define SQ(x) ((x)*(x))
static inline int sqdist(int x, int y, int z, int x2, int y2, int z2) {
    return SQ(x-x2)+SQ(y-y2)+SQ(z-z2);
}

#define NEWPACKET(type,name)                    \
    lh_create_obj(MCPacket,name);               \
    pkt->pid = type;                            \
    type##_pkt *t##name = &name->_##type;

////////////////////////////////////////////////////////////////////////////////

#define GMP(name)                               \
    case name: {                                \
        name##_pkt *tpkt = &pkt->_##name;

#define _GMP }

void gm_packet(MCPacket *pkt, MCPacketQueue *tq, MCPacketQueue *bq) {
    switch (pkt->pid) {

        default:
            // by default - just forward the packet
            queue_packet(pkt, tq);
    }
}
