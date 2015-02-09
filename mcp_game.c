#include <math.h>
#include <time.h>
#include <unistd.h>

#define LH_DECLARE_SHORT_NAMES 1

#include "lh_bytes.h"

#include "mcp_ids.h"
#include "mcp_gamestate.h"
#include "mcp_game.h"

// Various options
struct {
    int autokill;
    int grinding;
    int maxlevel;
    int holeradar;
    int build;
} opt;

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

void chat_message(const char *str, MCPacketQueue *q, const char *color, int pos) {
    uint8_t jreply[32768];

    NEWPACKET(SP_ChatMessage,pkt);

    ssize_t jlen;
    if (color)
        jlen = sprintf(tpkt->json,
                       "{\"extra\":[{\"color\":\"%s\",\"text\":\"\\u003cMCP\\u003e %s\"}],"
                       "\"text\":\"\"}",
                       color,str);
    else
        jlen = sprintf(tpkt->json,
                       "{\"extra\":[\"\\u003cMCP\\u003e %s\"],"
                       "\"text\":\"\"}",
                       str);

    tpkt->pos = pos;

    queue_packet(pkt,q);
}

void handle_command(char *str, MCPacketQueue *tq, MCPacketQueue *bq) {
    // tokenize
    char *words[256];
    CLEAR(words);
    int w=0;

    char wbuf[4096];
    strncpy(wbuf, str, sizeof(wbuf));
    char *wsave;

    char *wstr = wbuf;
    do {
        words[w++] = strtok_r(wstr, " ", &wsave);
        wstr = NULL;
    } while(words[w-1]);
    w--;
    if (w==0) return;

    ////////////////////////////////////////////////////////////////////////

    uint8_t reply[32768];
    reply[0] = 0;
    int rpos = 0;

    if (!strcmp(words[0],"test")) {
        sprintf(reply,"Chat test response");
    }
    else if (!strcmp(words[0],"entities")) {
        sprintf(reply,"Tracking %zd entities",gs.C(entity));
    }
    else if (!strcmp(words[0],"autokill")) {
        opt.autokill = !opt.autokill;
        sprintf(reply,"Autokill is %s",opt.autokill?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(words[0],"coords")) {
        sprintf(reply,"coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f, onground=%d",
                gs.own.x,gs.own.y,gs.own.z,
                gs.own.yaw,gs.own.pitch,gs.own.onground);
    }


    // send an immediate reply if any was given
    if (reply[0]) chat_message(reply, bq, "gold", rpos);


}

////////////////////////////////////////////////////////////////////////////////

#define GMP(name)                               \
    case name: {                                \
        name##_pkt *tpkt = &pkt->_##name;

#define _GMP }

void gm_packet(MCPacket *pkt, MCPacketQueue *tq, MCPacketQueue *bq) {
    switch (pkt->pid) {

        GMP(CP_ChatMessage) {
            if (tpkt->str[0] == '#' && tpkt->str[1] != '#') {
                handle_command(tpkt->str+1,tq,bq);
                free_packet(pkt);
            }
            else {
                // forward other chat messages
                queue_packet(pkt, tq);
            }
            break;
        } _GMP;

        default:
            // by default - just forward the packet
            queue_packet(pkt, tq);
    }
}

void gm_reset() {
    CLEAR(opt);
}

void gm_async(MCPacketQueue *sq, MCPacketQueue *cq) {
    
}
