/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

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

typedef struct {
    uint64_t    last;       // last timestamp when an event was allowed by rate-limiting
    uint64_t    level;      // current number of tokens
    uint64_t    interval;   // average interval, in us
    uint64_t    burst;      // burst size, in tokens
} tokenbucket;

#define TBDEF(name, i, b)                                                      \
    tokenbucket name = { .last=0, .level=b, .interval=i, .burst=b };

tokenbucket * tb_init(tokenbucket *tb, int64_t interval, int64_t burst) {
    if (!tb) tb = (tokenbucket *)malloc(sizeof(tokenbucket));
    assert(tb);

    tb->last = gettimestamp();
    tb->level = burst;
    tb->interval = interval;
    tb->burst = burst;

    return tb;
}

int tb_event(tokenbucket *tb, uint64_t size) {
    uint64_t ts = gettimestamp();
    uint64_t level = tb->level + (ts-tb->last)/tb->interval; // currently available token level
    if (level > tb->burst) level = tb->burst;

    if (size > level) return 0; // disallow this event

    tb->level = level-size;
    tb->last = ts;
    return size;
}

#define HEADPOSY(y) ((double)(y)+1.62)

static inline double mydist(double x, double y, double z) {
    return sqrt(SQ(gs.own.x-x)+SQ(HEADPOSY(gs.own.y)-y)+SQ(gs.own.z-z));
}

////////////////////////////////////////////////////////////////////////////////
// Autokill

#define MAX_ENTITIES     256
#define MIN_ENTITY_DELAY 250000  // minimum interval between hitting the same entity (us)
#define MIN_ATTACK_DELAY 50000   // minimum interval between attacking any entity
#define MAX_ATTACK       1       // how many entities to attack at once
#define REACH_RANGE      4.5

TBDEF(tb_ak, MIN_ATTACK_DELAY, MAX_ATTACK);

static void autokill(MCPacketQueue *sq) {
    if (!tb_event(&tb_ak, 1)) return;

    // calculate list of hostile entities in range
    uint32_t hent[MAX_ENTITIES];

    int i,hi=0;
    for(i=0; i<C(gs.entity) && hi<MAX_ENTITIES; i++) {
        entity *e = P(gs.entity)+i;

        // skip non-hostile entities
        if (!e->hostile) continue;

        // skip pigmen unless -p option was specified
        if (opt.autokill==1 && e->mtype==57) continue;

        // skip entities we hit only recently
        if ((tb_ak.last-e->lasthit) < MIN_ENTITY_DELAY) continue;

        // only take entities that are within our reach
        if (mydist(e->x, e->y, e->z)<=REACH_RANGE)
            hent[hi++] = i;
    }
    //TODO: sort entities by how dangerous and how close they are
    //TODO: check for obstruction
    //TODO: adjust for cooldown time

    for(i=0; i<hi && i<MAX_ATTACK; i++) {
        entity *e = P(gs.entity)+hent[i];
        //printf("Attacking entity %08x\n",e->id);

        e->lasthit = tb_ak.last;

        // Attack entity
        NEWPACKET(CP_UseEntity, atk);
        tatk->target = e->id;
        tatk->action = 1; // attack
        queue_packet(atk, sq);

        // Wave arm
        NEWPACKET(CP_Animation, anim);
        tanim->hand  = 0; // right hand
        queue_packet(anim, sq);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Autoshear

// use same constants from Autokill
TBDEF(tb_ash, MIN_ATTACK_DELAY, MAX_ATTACK);

static void autoshear(MCPacketQueue *sq) {
    // player must hold shears as active item
    slot_t * islot = &gs.inv.slots[gs.inv.held+36];
    if (islot->item != 359) return;

    // rate-limit
    if (!tb_event(&tb_ash, 1)) return;

    // calculate list of usable entities in range
    uint32_t hent[MAX_ENTITIES];

    int i,hi=0;
    for(i=0; i<C(gs.entity) && hi<MAX_ENTITIES; i++) {
        entity *e = P(gs.entity)+i;

        // check if the entity is a sheep
        if (e->mtype != Sheep) continue;

        if (!e->mdata) continue;

        // skip sheared sheep
        assert(e->mdata[12].type == META_BYTE);
        if (e->mdata[12].b >= 0x10) continue;

        // skip baby sheep
        assert(e->mdata[11].type == META_BOOL);
        if (e->mdata[11].bool) continue;

        // only take entities that are within our reach
        if (mydist(e->x, e->y, e->z)<=REACH_RANGE)
            hent[hi++] = i;
    }

    for(i=0; i<hi && i<MAX_ATTACK; i++) {
        entity *e = P(gs.entity)+hent[i];
        //printf("Shearing entity %08x\n",e->id);

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
        bid_t bl[8] = {
            get_block_at(x+lx*i,    z+lz*i,    y+3),
            get_block_at(x+lx*i+lz, z+lz*i+lx, y+2),
            get_block_at(x+lx*i,    z+lz*i,    y+2),
            get_block_at(x+lx*i-lz, z+lz*i-lx, y+2),
            get_block_at(x+lx*i+lz, z+lz*i+lx, y+1),
            get_block_at(x+lx*i,    z+lz*i,    y+1),
            get_block_at(x+lx*i-lz, z+lz*i-lx, y+1),
            get_block_at(x+lx*i,    z+lz*i,    y)
        };

        int j;
        for(j=0; j<8; j++) {
            if (bl[j].bid == 10 || bl[j].bid == 11) {
                char reply[32768];
                sprintf(reply, "*** LAVA *** : d=%d", i);
                chat_message(reply, cq, "gold", 2);
                return;
            }
        }

        if (ISEMPTY(bl[7].bid)) {
            char reply[32768];
            sprintf(reply, "*** HOLE *** : %d,%d y=%d d=%d", x+lx*i,z+lz*i,y,i);
            chat_message(reply, cq, "gold", 2);
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Inventory Handling

#define DEBUG_INVENTORY 0

int aid=10000;

#define IASTATE_NONE            0
#define IASTATE_START           1
#define IASTATE_PICK_SENT       2
#define IASTATE_PICK_ACCEPTED   3
#define IASTATE_SWAP_SENT       4
#define IASTATE_SWAP_ACCEPTED   5
#define IASTATE_PUT_SENT        6
#define IASTATE_PUT_ACCEPTED    7

struct {
    int     state;          // current state of the invaction queue
    int     base_aid;       // action id of the first transaction
    int     sid_a;          // slot A ID
    int     sid_b;          // slot B ID
    slot_t  drag;           // temp drag slot
    int64_t start;          // timestamp when the action has started
} invq;

void gmi_click(MCPacketQueue *sq, int sid, int aid) {
    assert(sid>=9 && sid<45);

    slot_t *s = &gs.inv.slots[sid];

    NEWPACKET(CP_ClickWindow, click);
    tclick->wid = 0;
    tclick->sid = sid;
    tclick->button = 0; // Left-click, mode 0 - pick/put up all items
    tclick->aid = aid;
    tclick->mode = 0;
    clone_slot(s, &tclick->slot);
    queue_packet(click, sq);

    if (DEBUG_INVENTORY)
        printf("ClickWindow aid=%d, sid=%d, state=%d\n",
               aid, sid, invq.state);
}

#define INVQ_TIMEOUT 2000000

void gmi_failed(MCPacketQueue *sq, MCPacketQueue *cq) {
    // Close window
    NEWPACKET(CP_CloseWindow, cwin);
    tcwin->wid=0;
    queue_packet(cwin, sq);

    slot_t *a = &gs.inv.slots[invq.sid_a];
    slot_t *b = &gs.inv.slots[invq.sid_b];

    // Update Client (Slot A)
    NEWPACKET(SP_SetSlot, cla);
    tcla->wid = 0;
    tcla->sid = invq.sid_a;
    clone_slot(a, &tcla->slot);
    queue_packet(cla, cq);

    // Update Client (Slot B)
    NEWPACKET(SP_SetSlot, clb);
    tclb->wid = 0;
    tclb->sid = invq.sid_b;
    clone_slot(b, &tclb->slot);
    queue_packet(clb, cq);

    // Inventory action failed, inventory state might be corrupt
    clear_slot(&invq.drag);
    lh_clear_obj(invq);
    aid+=3;
    if (aid>60000) aid=10000;

    // Abort the building process for safety and notify user
    build_pause();
    chat_message("INV ACTION FAILED!!! Buildtask paused!", cq, "green", 2);
    chat_message("An inventory action has failed or timed out, inventory state may be inconsistent", cq, "green", 0);
    chat_message("Access any dialog (container/crafting table/etc.) to refresh the inventory", cq, "green", 0);
}

void gmi_process_queue(MCPacketQueue *sq, MCPacketQueue *cq) {
    assert(invq.state);

    // Watchdog for the timeouted tasks
    if (invq.state != IASTATE_START && gettimestamp()-invq.start > INVQ_TIMEOUT) {
        gmi_failed(sq, cq);
        return;
    }

    slot_t *a = &gs.inv.slots[invq.sid_a];
    slot_t *b = &gs.inv.slots[invq.sid_b];

    switch (invq.state) {
        case IASTATE_START: {
            invq.base_aid = aid;
            gmi_click(sq, invq.sid_a, invq.base_aid);
            invq.state = IASTATE_PICK_SENT;
            invq.start = gettimestamp();
            if (DEBUG_INVENTORY) {
                printf("*GMI: START -> PICK_SENT\n");
                dump_inventory();
                printf("  A: "); dump_slot(&gs.inv.slots[invq.sid_a]);
                printf("  B: "); dump_slot(&gs.inv.slots[invq.sid_b]);
                printf("  D: "); dump_slot(&invq.drag); printf("\n");
            }
            break;
        }
        case IASTATE_PICK_ACCEPTED: {
            gmi_click(sq, invq.sid_b, invq.base_aid+1);
            clone_slot(a, &invq.drag);
            clear_slot(a);
            invq.state = IASTATE_SWAP_SENT;
            if (DEBUG_INVENTORY) {
                printf("*GMI: PICK_ACCEPTED -> SWAP_SENT\n");
                dump_inventory();
                printf("  A: "); dump_slot(&gs.inv.slots[invq.sid_a]);
                printf("  B: "); dump_slot(&gs.inv.slots[invq.sid_b]);
                printf("  D: "); dump_slot(&invq.drag); printf("\n");
            }
            break;
        }
        case IASTATE_SWAP_ACCEPTED: {
            gmi_click(sq, invq.sid_a, invq.base_aid+2);
            slot_t temp;
            clone_slot(b, &temp);
            clear_slot(b);
            clone_slot(&invq.drag, b);
            clear_slot(&invq.drag);
            clone_slot(&temp,&invq.drag);
            clear_slot(&temp);
            invq.state = IASTATE_PUT_SENT;
            if (DEBUG_INVENTORY) {
                printf("*GMI: SWAP_ACCEPTED -> PUT_SENT\n");
                dump_inventory();
                printf("  A: "); dump_slot(&gs.inv.slots[invq.sid_a]);
                printf("  B: "); dump_slot(&gs.inv.slots[invq.sid_b]);
                printf("  D: "); dump_slot(&invq.drag); printf("\n");
            }
            break;
        }
        case IASTATE_PUT_ACCEPTED: {
            // Swap action complete, update client

            // Swap slots in our inventory state
            clone_slot(&invq.drag, a);
            clear_slot(&invq.drag);

            // Close window
            NEWPACKET(CP_CloseWindow, cwin);
            tcwin->wid=0;
            queue_packet(cwin, sq);

            // Update Client (Slot A)
            NEWPACKET(SP_SetSlot, cla);
            tcla->wid = 0;
            tcla->sid = invq.sid_a;
            clone_slot(a, &tcla->slot);
            queue_packet(cla, cq);

            // Update Client (Slot B)
            NEWPACKET(SP_SetSlot, clb);
            tclb->wid = 0;
            tclb->sid = invq.sid_b;
            clone_slot(b, &tclb->slot);
            queue_packet(clb, cq);

            // Clear the state
            aid+=3;
            if (aid>60000) aid=10000;
            lh_clear_obj(invq); // This also sets the state to IASTATE_NONE

            if (DEBUG_INVENTORY) {
                printf("*GMI: PUT_ACCEPTED -> IDLE\n");
                dump_inventory();
                printf("  A: "); dump_slot(&gs.inv.slots[invq.sid_a]);
                printf("  B: "); dump_slot(&gs.inv.slots[invq.sid_b]);
                printf("  D: "); dump_slot(&invq.drag); printf("\n");
            }

            break;
        }
    }
}

int gmi_confirm(SP_ConfirmTransaction_pkt *tpkt, MCPacketQueue *sq, MCPacketQueue *cq) {
    if ( !invq.state || tpkt->wid != 0) return 1;

    if (!tpkt->accepted) {
        printf("Inventory action not accepted (wid=%d aid=%d base_aid=%d sid_a=%d sid_b=%d)\n",
               tpkt->wid, tpkt->aid, invq.base_aid, invq.sid_a, invq.sid_b);
        if (DEBUG_INVENTORY) {
            printf("  A: "); dump_slot(&gs.inv.slots[invq.sid_a]);
            printf("  B: "); dump_slot(&gs.inv.slots[invq.sid_b]);
            printf("  D: "); dump_slot(&invq.drag); printf("\n");
            dump_inventory();
        }

        gmi_failed(sq, cq);
    }
    else {
        switch (invq.state) {
            case IASTATE_PICK_SENT:
                if(tpkt->aid != invq.base_aid) return 1;
                invq.state = IASTATE_PICK_ACCEPTED;
                if (DEBUG_INVENTORY) {
                    printf("*GMI: PICK_SENT -> PICK_ACCEPTED, aid=%d base_aid=%d sid_a=%d sid_b=%d\n",
                           tpkt->aid, invq.base_aid, invq.sid_a, invq.sid_b);
                    dump_inventory();
                    printf("  A: "); dump_slot(&gs.inv.slots[invq.sid_a]);
                    printf("  B: "); dump_slot(&gs.inv.slots[invq.sid_b]);
                    printf("  D: "); dump_slot(&invq.drag); printf("\n");
                }
                break;
            case IASTATE_SWAP_SENT:
                if(tpkt->aid != invq.base_aid+1) return 1;
                invq.state = IASTATE_SWAP_ACCEPTED;
                if (DEBUG_INVENTORY) {
                    printf("*GMI: SWAP_SENT -> SWAP_ACCEPTED, aid=%d base_aid=%d sid_a=%d sid_b=%d\n",
                           tpkt->aid, invq.base_aid, invq.sid_a, invq.sid_b);
                    dump_inventory();
                    printf("  A: "); dump_slot(&gs.inv.slots[invq.sid_a]);
                    printf("  B: "); dump_slot(&gs.inv.slots[invq.sid_b]);
                    printf("  D: "); dump_slot(&invq.drag); printf("\n");
                }
                break;
            case IASTATE_PUT_SENT:
                if(tpkt->aid != invq.base_aid+2) return 1;
                invq.state = IASTATE_PUT_ACCEPTED;
                if (DEBUG_INVENTORY) {
                    printf("*GMI: PUT_SENT -> PUT_ACCEPTED, aid=%d base_aid=%d sid_a=%d sid_b=%d\n",
                           tpkt->aid, invq.base_aid, invq.sid_a, invq.sid_b);
                    dump_inventory();
                    printf("  A: "); dump_slot(&gs.inv.slots[invq.sid_a]);
                    printf("  B: "); dump_slot(&gs.inv.slots[invq.sid_b]);
                    printf("  D: "); dump_slot(&invq.drag); printf("\n");
                }
                break;
        }
    }
    return 0;
}

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

void gmi_swap_slots(MCPacketQueue *sq, MCPacketQueue *cq, int sa, int sb) {
    assert(invq.state == IASTATE_NONE);
    assert(sa>=9 && sa<45);
    assert(sb>=9 && sb<45);

    slot_t *a = &gs.inv.slots[sa];
    slot_t *b = &gs.inv.slots[sb];

    assert(!sameitem(a,b)); // ensure the items are not same (or not stackable),
                            // so our clickery will actually swap them

    invq.state = IASTATE_START;
    invq.sid_a = sa;
    invq.sid_b = sb;
    clear_slot(&invq.drag);
}

////////////////////////////////////////////////////////////////////////////////
// Anti-AFK

#define AFK_TIMEOUT 60*1000000LL

TBDEF(tb_afk, AFK_TIMEOUT, 1);

static void antiafk(MCPacketQueue *sq, MCPacketQueue *cq) {
    char reply[256], bname[256];
    reply[0] = 0;

    if (!tb_event(&tb_afk, 1)) return;

    // don't interfere if the client already has a window open
    if (gs.inv.windowopen) return;

    int x=floor(gs.own.x);
    int y=floor(gs.own.y);
    int z=floor(gs.own.z);

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
            tpbp->hand   = 0;
            tpbp->cx     = 8;
            tpbp->cy     = 16;
            tpbp->cz     = 8;
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

    return;
}

#if 0
////////////////////////////////////////////////////////////////////////////////
// Auto-eat

uint64_t ae_last_eat = 0;
int ae_held = -1;

#define EAT_INTERVAL    5000000
#define EAT_THRESHOLD   8
#define EAT_MAX         20

TBDEF(tb_eat, EAT_INTERVAL, 1);

/*
  The problem of autoeating - start eating is just a single packet sent to the
  server (PlayerBlockPlacement with coords -1,-1,-1), but the eating is not
  finished until the server confirms it. If we switch away from the food slot
  too early, it will be aborted. So, after eating, we need to delay switching
  back to the old slot by 2 seconds.
*/

void autoeat(MCPacketQueue *sq, MCPacketQueue *cq) {
    if (!tb_event(&tb_eat, 1)) return;

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
    NEWPACKET(CP_UseItem, use);
    tuse->hand   = 0;
    queue_packet(use,sq);
    dump_packet(use);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Autowalk (work in progress)

#define DEFAULT_PITCH 20
#define DEFAULT_TELEPORT_ID 0x01234567

void face_direction(MCPacketQueue *sq, MCPacketQueue *cq, float yaw) {
    // coordinates are adjusted so we stand exactly in the middle of the block
    double x = floor(gs.own.x)+0.5;
    double z = floor(gs.own.z)+0.5;

    // packet to the server
    NEWPACKET(CP_PlayerPositionLook, s);
    ts->x = x;
    ts->z = z;
    ts->y = gs.own.y;
    ts->yaw = yaw;
    ts->pitch = DEFAULT_PITCH;
    ts->onground = gs.own.onground;
    queue_packet(s, sq);

    // packet to the client
    NEWPACKET(SP_PlayerPositionLook, c);
    tc->x = x;
    tc->z = z;
    tc->y = gs.own.y;
    tc->yaw = yaw;
    tc->pitch = DEFAULT_PITCH;
    tc->flags = 0;
    tc->tpid = DEFAULT_TELEPORT_ID;
    queue_packet(c, cq);
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
        if (words[1] && !strcmp(words[1],"-p"))
            opt.autokill = 2;
        else
            opt.autokill = !opt.autokill;
        sprintf(reply,"Autokill is %s",opt.autokill?((opt.autokill==2)?"ON (attacks pigmen)":"ON"):"OFF");
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
#if 0
    else if (!strcmp(words[0],"ae") || !strcmp(words[0],"autoeat")) {
        opt.autoeat = !opt.autoeat;
        sprintf(reply,"Autoeat is %s",opt.autoeat?"ON":"OFF");
        rpos = 2;
    }
#endif
    else if (!strcmp(words[0],"coords")) {
        sprintf(reply,"coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f, onground=%d",
                gs.own.x,gs.own.y,gs.own.z,
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
        float yaw = 0;
        if (!(words[1] && sscanf(words[1], "%f", &yaw) == 1)) {
            switch(player_direction()) {
                case DIR_NORTH: yaw = 180; break;
                case DIR_SOUTH: yaw = 0;   break;
                case DIR_EAST:  yaw = 270; break;
                case DIR_WEST:  yaw = 90;  break;
            }
        }

        face_direction(tq, bq, yaw);
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

    // skip unimplemented packets
    if (!pkt->ver) {
        queue_packet(pkt, tq);
        return;
    }

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
            // thunder protection
            if (tpkt->id == 262) { // entity.lightning.thunder
                printf("**** THUNDER **** coords=%d,%d,%d vol=%.2f pitch=%.2f\n",
                       tpkt->x/8,tpkt->y/8,tpkt->z/8,
                       tpkt->vol,tpkt->pitch);
                int i;
                for(i=0; i<nbases; i++) {
                    float dx = (float)(bases[i].x - gs.own.x);
                    float dz = (float)(bases[i].z - gs.own.z);
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
            if ( !((tpkt->id >= 296 && tpkt->id <= 303) ||
                  tpkt->id == 305 || tpkt->id == 345 ||
                 (tpkt->id >= 148 && tpkt->id <= 157)) ) {
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
            int32_t x = floor(gs.own.x);
            int32_t y = floor(gs.own.y);
            int32_t z = floor(gs.own.z);
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

        GMP(CP_TeleportConfirm) {
            // do not forward Teleport Confirm packets from client that
            // resulted from our own SP_PlayerPositionLook (e.g. in #align)
            if (tpkt->tpid != DEFAULT_TELEPORT_ID)
                queue_packet(pkt, tq);
        } _GMP;

        ////////////////////////////////////////////////////////////////
        // Inventory

        GMP(SP_ConfirmTransaction) {
            gmi_confirm(tpkt, sq, cq);
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

            pli * pl = NULL;
            for(i=0; i<C(gs.players); i++) {
                if (!memcmp(tpkt->uuid, P(gs.players)[i].uuid, 16)) {
                    pl = P(gs.players)+i;
                    break;
                }
            }

            if (!pl)
                sprintf(buf, "Player %02x%02x%02x%02x%02x%02x... at %d,%d/%d",
                        tpkt->uuid[0],tpkt->uuid[1],tpkt->uuid[2],
                        tpkt->uuid[3],tpkt->uuid[4],tpkt->uuid[5],
                        (int)tpkt->x,(int)tpkt->z,(int)tpkt->y);
            else
                sprintf(buf, "Player %s at %d,%d/%d",
                        pl->name, (int)tpkt->x, (int)tpkt->z, (int)tpkt->y);

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

#if 0
    int i;
    for(i=0; i<nbases; i++) {
        printf("%3d : %+6d,%+6d %5d %s\n",
               i,bases[i].x,bases[i].z,bases[i].r,bases[i].name);
    }
#endif
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

#if 0
    for(i=0; i<nuuids; i++) {
        printf("%3d : %d %-40s ",i,uuids[i].status,uuids[i].name);
        hexprint(uuids[i].uuid, 16);
    }
#endif
}

void gm_reset() {
    lh_clear_obj(opt);
    clear_slot(&invq.drag);
    lh_clear_obj(invq);

    build_clear(NULL,NULL);
    readbases();
    read_uuids();
}

void gm_async(MCPacketQueue *sq, MCPacketQueue *cq) {
    if (invq.state) {
        // do not attempt to do other things while inventory is handled
        gmi_process_queue(sq, cq);
        return;
    }

    if (opt.autokill)  autokill(sq);
    if (opt.autoshear) autoshear(sq);
    if (opt.antiafk)   antiafk(sq, cq);
    //if (opt.autoeat)   autoeat(sq, cq);

    build_preview_transmit(cq);
    build_progress(sq, cq);
}
