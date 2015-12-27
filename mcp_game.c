#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#define LH_DECLARE_SHORT_NAMES 1

#include <lh_bytes.h>
#include <lh_debug.h>

#include "mcp_ids.h"
#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_build.h"
#include "mcp_arg.h"
#include "mcp_types.h"

// from mcproxy.c
void drop_connection();

// Various options
struct {
    int autokill;
    int grinding;
    int maxlevel;
    int holeradar;
    int build;
    int antiafk;
    int antispam;
    int autoshear;
    int autoeat;
    int bright;
} opt;

// loaded base locations - for thunder protection
#define MAXBASES 256
#define BASESFILE "bases.txt"

typedef struct {
    int32_t x,z,r;
    char name[256];
} wcoord_t;
wcoord_t bases[MAXBASES];
int nbases=0;

// friend/foe recognition
#define MAXUUIDS 256
#define UUIDSFILE "players.txt"
#define UUID_NEUTRAL 0
#define UUID_FRIEND  1
#define UUID_ENEMY   2
#define UUID_DANGER  3

typedef struct {
    uuid_t uuid;
    int    status;
    char   name[256];
} puuid;
puuid uuids[MAXUUIDS];
int nuuids = 0;

////////////////////////////////////////////////////////////////////////////////
// helpers

uint64_t gettimestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ts = (uint64_t)tv.tv_sec*1000000+(uint64_t)tv.tv_usec;
    return ts;
}

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
// Autoshear

// use same constants from Autokill

uint64_t last_autoshear = 0;

static void autoshear(MCPacketQueue *sq) {
    // player must hold shears as active item
    slot_t * islot = &gs.inv.slots[gs.inv.held+36];
    if (islot->item != 359) return;

    // skip if we used autoshear less than MIN_ATTACK_DELAY us ago
    uint64_t ts = gettimestamp();
    if ((ts-last_autoshear)<MIN_ATTACK_DELAY) return;

    // calculate list of usable entities in range
    uint32_t hent[MAX_ENTITIES];

    int i,hi=0;
    for(i=0; i<C(gs.entity) && hi<MAX_ENTITIES; i++) {
        entity *e = P(gs.entity)+i;

        // check if the entity is a sheep
        if (e->mtype != Sheep) continue;

        if (!e->mdata) continue;

        // skip sheared sheep
        assert(e->mdata[16].h != 0x7f);
        assert(e->mdata[16].type == META_BYTE);
        if (e->mdata[16].b >= 0x10) continue;

        // skip baby sheep
        assert(e->mdata[12].h != 0x7f);
        assert(e->mdata[12].type == META_BYTE);
        if (e->mdata[12].b < 0) continue;

        // skip entities we hit only recently
        if ((ts-e->lasthit) < MIN_ENTITY_DELAY) continue;

        // only take entities that are within our reach
        int sd = mydist(e->x, e->y, e->z) >> 10;
        if (sd<=SQ(REACH_RANGE))
            hent[hi++] = i;
    }

    for(i=0; i<hi && i<MAX_ATTACK; i++) {
        entity *e = P(gs.entity)+hent[i];
        printf("Shearing entity %08x\n",e->id);

        e->lasthit = ts;
        last_autoshear = ts;

        // Shear entity
        NEWPACKET(CP_UseEntity, atk);
        tatk->target = e->id;
        tatk->action = 0; // interact
        queue_packet(atk, sq);

        // Wave arm
        NEWPACKET(CP_Animation, anim);
        queue_packet(anim, sq);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Holeradar

#define HR_DIST 32

static void hole_radar(MCPacketQueue *cq) {
    int x = gs.own.lx;
    int y = gs.own.ly-1;
    int z = gs.own.lz;

    int i,lx=0,lz=1;
    switch(player_direction()) {
        case DIR_EAST: lx=1; lz=0; break;
        case DIR_WEST: lx=-1; lz=0; break;
        case DIR_SOUTH: lx=0; lz=1; break;
        case DIR_NORTH: lx=0; lz=-1; break;
    }

    for(i=0; i<HR_DIST; i++) {
        bid_t bl = get_block_at(x+lx*i, z+lz*i, y);
        if (ISEMPTY(bl.bid)) {
            char reply[32768];
            sprintf(reply, "*** HOLE *** : %d,%d y=%d d=%d", x+lx*i,z+lz*i,y,i);
            chat_message(reply, cq, "gold", 2);
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

int aid=10000;

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
    tpick->aid = aid;
    tpick->mode = 0;
    clone_slot(a, &tpick->slot);
    queue_packet(pick, sq);
    gs_packet(pick);

    // 2. Click on the second slot - swap items
    NEWPACKET(CP_ClickWindow, swap);
    tswap->wid = 0;
    tswap->sid = sb;
    tswap->button = 0;
    tswap->aid = aid+1;
    tswap->mode = 0;
    clone_slot(b, &tswap->slot);
    queue_packet(swap, sq);
    gs_packet(swap);

    // 3. Click again on the first slot - drop the swapped items
    NEWPACKET(CP_ClickWindow, put);
    tput->wid = 0;
    tput->sid = sa;
    tput->button = 0;
    tput->aid = aid+2;
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

    aid += 3;
    if (aid>60000) aid = 10000;
}

////////////////////////////////////////////////////////////////////////////////
// Anti-AFK

#define AFK_TIMEOUT 60*1000000LL

uint64_t last_antiafk = 0;

static void antiafk(MCPacketQueue *sq, MCPacketQueue *cq) {
    char reply[256], bname[256];
    reply[0] = 0;

    uint64_t ts = gettimestamp();
    if (ts - last_antiafk < AFK_TIMEOUT) return;

    // don't interfere if the client already has a window open
    if (gs.inv.windowopen) {
        last_antiafk = ts;
        return;
    }

    int x=gs.own.x>>5;
    int y=gs.own.y>>5;
    int z=gs.own.z>>5;

    bid_t b = get_block_at(x,z,y);

    if (b.bid == 0x32) {
        // We have the torch at our feet - break it
        NEWPACKET(CP_PlayerDigging, pd);
        tpd->status = 0;
        tpd->loc.x  = x;
        tpd->loc.y  = y;
        tpd->loc.z  = z;
        tpd->face   = 1;
        queue_packet(pd,sq);

        // Wave arm
        NEWPACKET(CP_Animation, anim);
        queue_packet(anim, sq);
    }
    else if (b.raw != 0) {
        // Something else at our feet - cannot continue
        sprintf(reply,"Anti-AFK: please remove any blocks at your feet (%d,%d/%d), found %s",
                x,z,y,get_bid_name(bname,b));
    }
    else {
        // Empty spot - place a torch
        int tslot = prefetch_material(sq, cq, BLOCKTYPE(0x32,0) );
        if (tslot<0) {
            sprintf(reply,"Anti-AFK: you need to have torches in your inventory");
        }
        else {
            int held=gs.inv.held;
            gmi_change_held(sq, cq, tslot, 1);

            // place torch
            NEWPACKET(CP_PlayerBlockPlacement, pbp);
            tpbp->bpos = POS(x,y-1,z);
            tpbp->face   = 1;
            tpbp->cx     = 8;
            tpbp->cy     = 16;
            tpbp->cz     = 8;
            clone_slot(&gs.inv.slots[tslot+36], &tpbp->item);
            queue_packet(pbp,sq);
            dump_packet(pbp);

            // Wave arm
            NEWPACKET(CP_Animation, anim);
            queue_packet(anim, sq);

            // switch back to whatever the client was holding
            if (held != gs.inv.held)
                gmi_change_held(sq, cq, held, 1);
        }
    }

    if (reply[0])
        chat_message(reply, cq, "gold", 0);

    last_antiafk = ts;
    return;
}

////////////////////////////////////////////////////////////////////////////////
// Auto-eat

uint64_t ae_last_eat = 0;
int ae_held = -1;

#define EAT_INTERVAL    5000000
#define EAT_THRESHOLD   8
#define EAT_MAX         20

/*
  The problem of autoeating - start eating is just a single packet sent to the
  server (PlayerBlockPlacement with coords -1,-1,-1), but the eating is not
  finished until the server confirms it. If we switch away from the food slot
  too early, it will be aborted. So, after eating, we need to delay switching
  back to the old slot by 2 seconds.
*/

void autoeat(MCPacketQueue *sq, MCPacketQueue *cq) {
    // skip if we used autoeat less than EAT_INTERVAL us ago
    uint64_t ts = gettimestamp();
    if ((ts-ae_last_eat)<EAT_INTERVAL) return;

    // if we have full health, start eating when food<EAT_THRESHOLD
    // if we're less than full health, eat until max food level
    if ( (gs.own.health >= 20.0 && (gs.own.food > EAT_THRESHOLD)) ||
         (gs.own.health < 20.0 && (gs.own.food >= EAT_MAX))) {
        // our hunder is OK, check if we need to restore an old item slot

        if (ae_held != -1) {
            // we had something else selected previously, restore the
            // selected hotbar slot.

            gmi_change_held(sq, cq, ae_held, 1);
            ae_held = -1;
        }
        return;
    }

    //printf("Hunger=%d Health=%f, gonna eat! Held=%d\n",
    //       gs.own.food, gs.own.health, gs.inv.held);

    slot_t *s = &gs.inv.slots[gs.inv.held+36];
    if (!(s->item>0 && ITEMS[s->item].flags&I_FOOD)) {
        // current slot is not edible, try to find food elsewhere in the
        // inventory, searching backwards to find items in the hotbar first
        int i;
        for(i=44; i>=9; i--) {
            s = &gs.inv.slots[i];
            if (s->item > 0 && ITEMS[s->item].flags&I_FOOD) break;
        }

        if (i<9) { // we could not find anything edible
            opt.autoeat = 0;
            chat_message("Autoeat disabled - out of food!", cq, "gold", 2);
            return;
        }

        // fetch the food item into hotbar if it's in the main inventory
        int eslot = i;
        if (i<36) {
            eslot = find_evictable_slot()+36;
            gmi_swap_slots(sq, cq, i, eslot);
        }

        // switch to the food item, but remember what was selected before
        ae_held=gs.inv.held;
        gmi_change_held(sq, cq, eslot-36, 1);
    }

    // send start eating packet to the server and exit, switch back to
    // the old slot will be done next time this function runs
    NEWPACKET(CP_PlayerBlockPlacement, pbp);
    tpbp->bpos = POS(-1,-1,-1);
    tpbp->face   = -1;
    tpbp->cx     = 0;
    tpbp->cy     = 0;
    tpbp->cz     = 0;
    clone_slot(s, &tpbp->item);
    queue_packet(pbp,sq);
    dump_packet(pbp);

    ae_last_eat = ts;
}

////////////////////////////////////////////////////////////////////////////////
// Autowalk (work in progress)

#define DEFAULT_PITCH 20

void face_direction(MCPacketQueue *sq, MCPacketQueue *cq, int dir) {
    printf("Facing to %d\n", dir);

    float yaw = 0;
    switch(dir) {
        case DIR_NORTH: yaw = 180; break;
        case DIR_SOUTH: yaw = 0;   break;
        case DIR_EAST:  yaw = 270; break;
        case DIR_WEST:  yaw = 90;  break;
    }

    // coordinates are adjusted so we stand exactly in the middle of the block
    int x=gs.own.x&0xfffffff0; x|=0x00000010;
    int z=gs.own.z&0xfffffff0; z|=0x00000010;

    // packet to the client
    NEWPACKET(CP_PlayerPositionLook, s);
    ts->x = (double)x/32.0;
    ts->z = (double)z/32.0;
    ts->y = (double)(gs.own.y>>5);
    ts->yaw = yaw;
    ts->pitch = DEFAULT_PITCH;
    ts->onground = gs.own.onground;
    queue_packet(s, sq);

    // packet to the server
    NEWPACKET(SP_PlayerPositionLook, c);
    tc->x = (double)x/32.0;
    tc->z = (double)z/32.0;
    tc->y = (double)(gs.own.y>>5);
    tc->yaw = yaw;
    tc->pitch = DEFAULT_PITCH;
    tc->flags = 0;
    queue_packet(c, cq);
}

////////////////////////////////////////////////////////////////////////////////
// Fullbright

void chunk_bright(chunk_t * chunk, int bincr) {
    int Y,i,level;
    for(Y=0; Y<16; Y++) {
        if (chunk->cubes[Y]) {
            light_t *light = chunk->cubes[Y]->light;
            for(i=0; i<2048; i++) {
                level = (int)light[i].l + bincr;
                light[i].l = (level<15) ? level : 15;
                level = (int)light[i].h + bincr;
                light[i].h = (level<15) ? level : 15;
            }
        }
    }
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
    else if (!strcmp(words[0],"afk") || !strcmp(words[0],"antiafk")) {
        opt.antiafk = !opt.antiafk;
        sprintf(reply,"Anti-AFK is %s",opt.antiafk?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(words[0],"ash") || !strcmp(words[0],"autoshear")) {
        opt.autoshear = !opt.autoshear;
        sprintf(reply,"Autoshear is %s",opt.autoshear?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(words[0],"ae") || !strcmp(words[0],"autoeat")) {
        opt.autoeat = !opt.autoeat;
        sprintf(reply,"Autoeat is %s",opt.autoeat?"ON":"OFF");
        rpos = 2;
    }
    else if (!strcmp(words[0],"as") || !strcmp(words[0],"antispam")) {
        opt.antispam = !opt.antispam;
        sprintf(reply,"Antispam filter is %s",opt.antispam?"ON":"OFF");
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
    else if (!strcmp(words[0],"align")) {
        face_direction(tq, bq, player_direction());
        //TODO: should be possible to specify direction as argument
    }
    else if (!strcmp(words[0],"br") || !strcmp(words[0],"bright")) {
        int bright = -1;
        if (words[1]) {
            if (sscanf(words[1], "%u", &bright)!=1) {
                bright = -1;
            }
        }
        else {
            bright = (opt.bright) ? 0 : 15;
        }

        if (bright<0) {
            sprintf(reply, "Usage: #bright [increment]");
        }
        else {
            if (bright > 15) bright=15;
            sprintf(reply,"Brightness increment: %d", bright);
            rpos = 2;
            opt.bright = bright;
        }
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

    MCPacketQueue *sq = pkt->cl ? tq : bq;
    MCPacketQueue *cq = pkt->cl ? bq : tq;

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

        GMP(SP_ChatMessage) {
            char name[256], message[256];
            int isspam=0;
            if (opt.antispam) {
                if (decode_chat_json(tpkt->json, name, message)) {
                    if (!strncmp(message, "I just ", 7) ||
                        !strncmp(message, "\xef\xbc\xa9 \xef\xbd\x8a\xef\xbd\x95\xef\xbd\x93\xef\xbd\x94 ", 17) )
                        isspam = 1;
                    if (!strncmp(message, "Hello, ", 7) ||
                        !strncmp(message, "Welcome, ", 9) ||
                        !strncmp(message, "Greetings, ", 11) )
                        isspam = 1;
                }
            }
            if (isspam)
                printf("Antispam: blocked [%s] %s\n", name, message);
            else
                queue_packet(pkt, tq);
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
            // thunder protection
            if (!strcmp(tpkt->name,"ambient.weather.thunder")) {
                printf("**** THUNDER **** coords=%d,%d,%d vol=%.4f pitch=%d\n",
                       tpkt->x/8,tpkt->y/8,tpkt->z/8,
                       tpkt->vol,tpkt->pitch);
                int i;
                for(i=0; i<nbases; i++) {
                    float dx = (float)(bases[i].x - gs.own.x/32);
                    float dz = (float)(bases[i].z - gs.own.z/32);
                    if ( sqrtf(dx*dx+dz*dz) < (float)bases[i].r  ) {
                        printf("Dropping connection because we are too close to base %s at %d,%d : "
                               "dx=%.0f dz=%.0f radius=%d\n",
                               bases[i].name, bases[i].x, bases[i].z, dx, dz, bases[i].r);
                        drop_connection();
                        break;
                    }
                }
            }

            // block annoying sounds
            if (strncmp(tpkt->name,"mob.sheep.",10) &&
                strncmp(tpkt->name,"mob.cow.",8) &&
                strncmp(tpkt->name,"mob.pig.",8) &&
                strncmp(tpkt->name,"mob.chicken.",12) &&
                strncmp(tpkt->name,"note.",5) ) {
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
        case SP_UpdateHealth:
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
                hole_radar(cq);

            build_update();

            gs.own.pos_change = 0;

            if (build_packet(pkt, sq, cq))
                queue_packet(pkt, tq);

            break;
        }

        ////////////////////////////////////////////////////////////////
        // Map data

        GMP(SP_ChunkData) {
            if (opt.bright) {
                chunk_bright(&tpkt->chunk, opt.bright);
                pkt->modified=1;
            }
            queue_packet(pkt, tq);
        } _GMP;

        GMP(SP_MapChunkBulk) {
            if (opt.bright) {
                int i;
                for(i=0; i<tpkt->nchunks; i++)
                    chunk_bright(&tpkt->chunk[i], opt.bright);
                pkt->modified=1;
            }
            queue_packet(pkt, tq);
        } _GMP;

        ////////////////////////////////////////////////////////////////
        // Other players

        GMP(SP_SpawnPlayer) {
            char buf[256];
            int i;

            puuid * pu = NULL;
            for(i=0; i<nuuids; i++) {
                if (!memcmp(tpkt->uuid, uuids[i].uuid, 16)) {
                    pu = &uuids[i];
                    break;
                }
            }

            if (!pu)
                sprintf(buf, "Player %02x%02x%02x%02x%02x%02x... at %d,%d/%d",
                        tpkt->uuid[0],tpkt->uuid[1],tpkt->uuid[2],
                        tpkt->uuid[3],tpkt->uuid[4],tpkt->uuid[5],
                        tpkt->x>>5,tpkt->z>>5,tpkt->y>>5);
            else
                sprintf(buf, "Player %s at %d,%d/%d",
                        pu->name, tpkt->x>>5, tpkt->z>>5, tpkt->y>>5);

            chat_message(buf, tq, "red", 0);
            queue_packet(pkt, tq);

            if (pu && pu->status==UUID_DANGER)
                drop_connection();
        } _GMP;

        default:
            // by default - just forward the packet
            queue_packet(pkt, tq);
    }
}

// load bases coords for thunder protection
void readbases() {
    FILE *fp = fopen(BASESFILE, "r");
    if (!fp) {
        printf("Cannot open file %s for reading: %s\n",BASESFILE,strerror(errno));
        return;
    }

    nbases=0;
    while (!feof(fp) && nbases<MAXBASES) {
        char buf[256];
        if (!fgets(buf, sizeof(buf), fp)) break;
        if (buf[0]=='#' || buf[0]==0x0d || buf[0]==0x0a) continue;

        int32_t x,z,r;
        char s[4096];
        if (sscanf(buf, "%d,%d,%d,%[^,]", &x, &z, &r, s)!=4) {
            printf("Error parsing line '%s' in %s\n",buf,BASESFILE);
            continue;
        }

        int i;
        for(i=0; i<sizeof(s)&&s[i]; i++) if (s[i]<0x20) s[i]=0;

        bases[nbases].x = x;
        bases[nbases].z = z;
        bases[nbases].r = r;
        strncpy(bases[nbases].name, s, sizeof(bases[nbases].name)-1);
        nbases++;
    }

    int i;
    for(i=0; i<nbases; i++) {
        printf("%3d : %+6d,%+6d %5d %s\n",
               i,bases[i].x,bases[i].z,bases[i].r,bases[i].name);
    }
    fflush(stdout);
}

// load friend/foe UUIDs
void read_uuids() {
    int i;

    FILE *fp = fopen(UUIDSFILE, "r");
    if (!fp) {
        printf("Cannot open file %s for reading: %s\n",UUIDSFILE,strerror(errno));
        return;
    }

    nuuids=0;
    while (!feof(fp) && nuuids<MAXUUIDS) {
        char buf[1024];
        if (!fgets(buf, sizeof(buf), fp)) break;
        if (buf[0]=='#' || buf[0]==0x0d || buf[0]==0x0a) continue;

        char ubuf[256];
        int status;
        char name[256];

        if (sscanf(buf, "%[^,],%d,%s", ubuf,&status,name)!=3) {
            printf("Error parsing line '%s' in %s\n",buf,UUIDSFILE);
            continue;
        }

        if (hex_import(ubuf, uuids[nuuids].uuid, 16)!=16) {
            printf("Error parsing UUID '%s' in %s\n",ubuf,UUIDSFILE);
            continue;
        }
        uuids[nuuids].status = status;
        strcpy(uuids[nuuids].name, name);
        nuuids++;
    }

    for(i=0; i<nuuids; i++) {
        printf("%3d : %d %-40s ",i,uuids[i].status,uuids[i].name);
        hexprint(uuids[i].uuid, 16);
    }
}

void gm_reset() {
    lh_clear_obj(opt);

    build_clear(NULL,NULL);
    readbases();
    read_uuids();
}

void gm_async(MCPacketQueue *sq, MCPacketQueue *cq) {
    if (opt.autokill)  autokill(sq);
    if (opt.antiafk)   antiafk(sq, cq);
    if (opt.autoshear) autoshear(sq);
    if (opt.autoeat)   autoeat(sq, cq);

    build_progress(sq, cq);
}
