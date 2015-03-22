#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#define LH_DECLARE_SHORT_NAMES 1

#include "lh_bytes.h"

#include "mcp_ids.h"
#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_build.h"

// from mcproxy.c
void drop_connection();

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
    return ts;
}

#define MIN(a,b) ((a<b)?(a):(b))
#define MAX(a,b) ((a>b)?(a):(b))
#define SGN(x) (((x)>=0)?1:-1)
#define SQ(x) ((x)*(x))
#define HEADPOSY(y) ((y)+32*162/100)

static inline int mydist(fixp x, fixp y, fixp z) {
    return SQ(gs.own.x-x)+SQ(HEADPOSY(gs.own.y)-y)+SQ(gs.own.z-z);
}

////////////////////////////////////////////////////////////////////////////////
// Autokill

#define MAX_ENTITIES     256
#define MIN_ENTITY_DELAY 250000  // minimum interval between hitting the same entity (us)
#define MIN_ATTACK_DELAY 50000   // minimum interval between attacking any entity
#define MAX_ATTACK       1       // how many entities to attack at once
#define REACH_RANGE      4

uint64_t ak_last_attack = 0;

static void autokill(MCPacketQueue *sq) {
    // skip if we used autokill less than MIN_ATTACK_DELAY us ago
    uint64_t ts = gettimestamp();
    if ((ts-ak_last_attack)<MIN_ATTACK_DELAY) return;

    // calculate list of hostile entities in range
    uint32_t hent[MAX_ENTITIES];

    int i,hi=0;
    for(i=0; i<C(gs.entity) && hi<MAX_ENTITIES; i++) {
        entity *e = P(gs.entity)+i;

        // skip non-hostile entities
        if (!e->hostile) continue;

        // skip entities we hit only recently
        if ((ts-e->lasthit) < MIN_ENTITY_DELAY) continue;

        // only take entities that are within our reach
        int sd = mydist(e->x, e->y, e->z) >> 10;
        if (sd<=SQ(REACH_RANGE))
            hent[hi++] = i;
    }
    //TODO: sort entities by how dangerous and how close they are
    //TODO: check for obstruction

    for(i=0; i<hi && i<MAX_ATTACK; i++) {
        entity *e = P(gs.entity)+hent[i];
        printf("Attacking entity %08x\n",e->id);

        e->lasthit = ts;
        ak_last_attack = ts;

        // Attack entity
        NEWPACKET(CP_UseEntity, atk);
        tatk->target = e->id;
        tatk->action = 1; // attack
        queue_packet(atk, sq);

        // Wave arm
        NEWPACKET(CP_Animation, anim);
        queue_packet(anim, sq);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Holeradar

#define HR_DISTC 2
#define HR_DISTB (HR_DISTC<<4)

static void hole_radar(MCPacketQueue *sq) {
    int x = gs.own.lx;
    int y = gs.own.ly-1;
    int z = gs.own.lz;

    int lx=-(int)(65536*sin(gs.own.yaw/180*M_PI));
    int lz=(int)(65536*cos(gs.own.yaw/180*M_PI));

    int X=x>>4;
    int Z=z>>4;

    bid_t data[(HR_DISTC+1)*256];
    int off,sh;

    switch(player_direction()) {
        case DIR_WEST:
            lz=0;
            lx=-1;
            export_cuboid(X-HR_DISTC,HR_DISTC+1,Z,1,y,1,data);
            off = (x&0x0f)+HR_DISTB+(z&0x0f)*(HR_DISTB+16);
            sh=-1;
            break;
        case DIR_EAST:
            lz=0;
            lx=1;
            export_cuboid(X,HR_DISTC+1,Z,1,y,1,data);
            off = (x&0x0f)+(z&0x0f)*(HR_DISTB+16);
            sh=1;
            break;
        case DIR_NORTH:
            lx=0;
            lz=-1;
            export_cuboid(X,1,Z-HR_DISTC,HR_DISTC+1,y,1,data);
            off = (x&0x0f)+((z&0x0f)+HR_DISTB)*16;
            sh=-16;
            break;
        case DIR_SOUTH:
            lx=0;
            lz=1;
            export_cuboid(X,1,Z,HR_DISTC+1,y,1,data);
            off = (x&0x0f)+(z&0x0f)*16;
            sh=16;
            break;
    }

    int i;
    for(i=1; i<=HR_DISTB; i++) {
        off+=sh;
        //TODO: check for other block types
        if (data[off].bid==0) {
            char reply[32768];
            sprintf(reply, "*** HOLE *** : %d,%d y=%d d=%d",
                    x+lx*i,z+lz*i,y,i);
            chat_message(reply, sq, "gold", 2);
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Inventory Handling

// change the currently selected quickbar slot
void gmi_change_held(MCPacketQueue *sq, MCPacketQueue *cq, int sid, int notify_client) {
    assert(sid>=0 && sid<=8);

    // send command to the server
    NEWPACKET(CP_HeldItemChange, cp);
    tcp->sid = sid;
    queue_packet(cp, sq);

    // notify gamestate
    gs_packet(cp);

    if (notify_client) {
        // send command to the client if we want to keep it updated
        NEWPACKET(SP_HeldItemChange, sp);
        tsp->sid = sid;
        queue_packet(sp, cq);
    }
}

// swap the contents of two slots - will also transfer item from a slot into
// empty slot if one of them is empty
void gmi_swap_slots(MCPacketQueue *sq, MCPacketQueue *cq, int sa, int sb) {
    assert(sa>=9 && sa<45);
    assert(sb>=9 && sb<45);

    slot_t *a = &gs.inv.slots[sa];
    slot_t *b = &gs.inv.slots[sb];

    assert(!sameitem(a,b)); // ensure the items are not same (or not stackable),
                            // so our clickery will actually swap them

    // 1. Click on the first slot
    NEWPACKET(CP_ClickWindow, pick);
    tpick->wid = 0;
    tpick->sid = sa;
    tpick->button = 0; // Left-click, mode 0 - pick up all items
    tpick->aid = 10000;
    tpick->mode = 0;
    clone_slot(a, &tpick->slot);
    queue_packet(pick, sq);
    gs_packet(pick);

    // 2. Click on the second slot - swap items
    NEWPACKET(CP_ClickWindow, swap);
    tswap->wid = 0;
    tswap->sid = sb;
    tswap->button = 0;
    tswap->aid = 10001;
    tswap->mode = 0;
    clone_slot(b, &tswap->slot);
    queue_packet(swap, sq);
    gs_packet(swap);

    // 3. Click again on the first slot - drop the swapped items
    NEWPACKET(CP_ClickWindow, put);
    tput->wid = 0;
    tput->sid = sa;
    tput->button = 0;
    tput->aid = 10002;
    tput->mode = 0;
    clone_slot(a, &tput->slot);
    queue_packet(put, sq);
    gs_packet(put);

    // 4. Close window
    NEWPACKET(CP_CloseWindow, cwin);
    tcwin->wid=0;
    queue_packet(cwin, sq);
    dump_packet(cwin);

    // 5. Update Client (Slot A)
    NEWPACKET(SP_SetSlot, cla);
    tcla->wid = 0;
    tcla->sid = sa;
    clone_slot(a, &tcla->slot);
    queue_packet(cla, cq);

    // 6. Update Client (Slot B)
    NEWPACKET(SP_SetSlot, clb);
    tclb->wid = 0;
    tclb->sid = sb;
    clone_slot(b, &tclb->slot);
    queue_packet(clb, cq);
}

////////////////////////////////////////////////////////////////////////////////
// Chat/Commandline

void chat_message(const char *str, MCPacketQueue *q, const char *color, int pos) {
    uint8_t jreply[32768];

    NEWPACKET(SP_ChatMessage,pkt);

    ssize_t jlen;
    char msg[65536];
    if (color)
        jlen = sprintf(msg,
                       "{\"extra\":[{\"color\":\"%s\",\"text\":\"\\u003cMCP\\u003e %s\"}],"
                       "\"text\":\"\"}",
                       color,str);
    else
        jlen = sprintf(msg,
                       "{\"extra\":[\"\\u003cMCP\\u003e %s\"],"
                       "\"text\":\"\"}",
                       str);

    tpkt->pos = pos;
    tpkt->json = strdup(msg);

    queue_packet(pkt,q);
}

static void handle_command(char *str, MCPacketQueue *tq, MCPacketQueue *bq) {
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

    char reply[32768];
    reply[0] = 0;
    int rpos = 0;

    if (!strcmp(words[0],"test")) {
        sprintf(reply,"Chat test response");
    }
    else if (!strcmp(words[0],"entities")) {
        sprintf(reply,"Tracking %zd entities",gs.C(entity));
        printf("Tracking %zd entities",gs.C(entity));
        dump_entities();
    }
    else if (!strcmp(words[0],"ak") || !strcmp(words[0],"autokill")) {
        opt.autokill = !opt.autokill;
        sprintf(reply,"Autokill is %s",opt.autokill?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(words[0],"coords")) {
        sprintf(reply,"coord=%d,%d,%d, rot=%.1f,%.1f, onground=%d",
                gs.own.x>>5,gs.own.y>>5,gs.own.z>>5,
                gs.own.yaw,gs.own.pitch,gs.own.onground);
    }
    else if (!strcmp(words[0],"inv")) {
        dump_inventory();
    }
    else if (!strcmp(words[0],"grind")) {
        int maxlevel=30;
        int start = 1;

        if (words[1]) {
            if (!strcmp(words[1],"stop")) {
                opt.grinding = 0;
                opt.autokill = 0;
                sprintf(reply,"Grinding is stopped");
                start = 0;
            }
            else if (sscanf(words[1],"%d",&maxlevel)!=1) {
                maxlevel = 30;
            }
        }

        if (start) {
            opt.grinding = 1;
            opt.autokill = 1;
            opt.maxlevel = maxlevel;
            sprintf(reply,"Grinding to level %d",opt.maxlevel);
        }
    }
    else if (!strcmp(words[0],"build") || !strcmp(words[0],"bu")) {
        build_cmd(words, tq, bq);
    }
    else if (!strcmp(words[0],"hr") || !strcmp(words[0],"holeradar")) {
        opt.holeradar = !opt.holeradar;
        sprintf(reply,"Hole radar is %s",opt.holeradar?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(words[0],"changeheld")) {
        if (!words[1] || !words[2]) {
            sprintf(reply,"Usage: changeheld <sid> <notify_client>");
        }
        else {
            gmi_change_held(tq, bq, atoi(words[1]), atoi(words[2]));
        }
    }
    else if (!strcmp(words[0],"swapslots")) {
        if (!words[1] || !words[2]) {
            sprintf(reply,"Usage: swapslots <sid1> <sid2>");
        }
        else {
            gmi_swap_slots(tq, bq, atoi(words[1]), atoi(words[2]));
        }
    }

    // send an immediate reply if any was given
    if (reply[0]) chat_message(reply, bq, "gold", rpos);
}

////////////////////////////////////////////////////////////////////////////////

#define GMP(name)                               \
    case name: {                                \
        name##_pkt *tpkt = &pkt->_##name;

#define _GMP break; }

void gm_packet(MCPacket *pkt, MCPacketQueue *tq, MCPacketQueue *bq) {
    dump_packet(pkt);
    switch (pkt->pid) {

        ////////////////////////////////////////////////////////////////
        // Chat

        GMP(CP_ChatMessage) {
            if (tpkt->str[0] == '#' && tpkt->str[1] != '#') {
                handle_command(tpkt->str+1,tq,bq);
                free_packet(pkt);
            }
            else {
                // forward other chat messages
                queue_packet(pkt, tq);
            }
        } _GMP;

        ////////////////////////////////////////////////////////////////
        // Misc effects

        GMP(SP_Effect) {
            if (tpkt->id == 1013) {
                char buf[4096];
                sprintf(buf, "WITHER SPAWN at %d,%d,%d",
                        tpkt->loc.x,tpkt->loc.y,tpkt->loc.z);

                printf("**** %s ****\n",buf);

                queue_packet(pkt, tq);
                chat_message(buf, tq, "gold", 0);
                chat_message(buf, tq, "gold", 2);
            }
        } _GMP;

        GMP(SP_SoundEffect) {
            if (!strcmp(tpkt->name,"ambient.weather.thunder")) {
                printf("**** THUNDER ****\n"
                       "coords=%d,%d,%d vol=%.4f pitch=%d\n",
                       tpkt->x/8,tpkt->y/8,tpkt->z/8,
                       tpkt->vol,tpkt->pitch);
                drop_connection();
            }

            if (strcmp(tpkt->name,"mob.sheep.say") &&
                strcmp(tpkt->name,"mob.sheep.step")) {
                queue_packet(pkt, tq);
            }
        } _GMP;

        ////////////////////////////////////////////////////////////////
        // Player

        GMP(SP_SetExperience) {
            if (opt.grinding && tpkt->level >= opt.maxlevel) {
                opt.grinding = 0;
                opt.autokill = 0;

                char buf[4096];
                sprintf(buf, "Grinding finished at level %d",tpkt->level);
                chat_message(buf, tq, "green", 0);
            }
            queue_packet(pkt, tq);
        } _GMP;

        // use case statements since we don't really need these packets,
        // but just as a trigger that position or world has updated
        case SP_PlayerPositionLook:
        case CP_PlayerPositionLook:
        case CP_PlayerPosition:
        case CP_PlayerLook:
        case SP_MultiBlockChange:
        case SP_BlockChange:
        case CP_PlayerBlockPlacement:
        case SP_Explosion: {

            // check if our position or orientation have changed
            int32_t x = (int32_t)gs.own.x>>5;
            int32_t y = (int32_t)gs.own.y>>5;
            int32_t z = (int32_t)gs.own.z>>5;
            int yaw = (int)(round(gs.own.yaw/90));

            if (x!= gs.own.lx || y!= gs.own.ly ||
                z!= gs.own.lz || yaw!= gs.own.lo ) {
                gs.own.lx = x;
                gs.own.ly = y;
                gs.own.lz = z;
                gs.own.lo = yaw;
                gs.own.pos_change = 1;
            }

            if (opt.holeradar && gs.own.pos_change)
                hole_radar(pkt->cl?bq:tq);

            build_update();
            build_packet(pkt);

            gs.own.pos_change = 0;

            queue_packet(pkt, tq);
            break;
        }

        ////////////////////////////////////////////////////////////////

        default:
            // by default - just forward the packet
            queue_packet(pkt, tq);
    }
}

void gm_reset() {
    lh_clear_obj(opt);
    build_clear();
}

void gm_async(MCPacketQueue *sq, MCPacketQueue *cq) {
    if (opt.autokill) autokill(sq);

    build_progress(sq, cq);
}
