#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LH_DECLARE_SHORT_NAMES 1

#include <lh_debug.h>
#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_buffers.h>
#include <lh_compress.h>

#include "mcp_gamestate.h"

gamestate gs;
static int gs_used = 0;

////////////////////////////////////////////////////////////////////////////////
// entity tracking

static inline int find_entity(int eid) {
    int i;
    for(i=0; i<C(gs.entity); i++)
        if (P(gs.entity)[i].id == eid)
            return i;
    return -1;
}

void dump_entities() {
    printf("Tracking %zd entities:\n",C(gs.entity));
    int i;
    for(i=0; i<C(gs.entity); i++) {
        entity *e = P(gs.entity)+i;
        printf("  %4d eid=%08x type=%-7s coord=%.1f,%.1f,%.1f\n",
               i,e->id, ENTITY_TYPES[e->type],
               (float)e->x/32,(float)e->y/32,(float)e->z/32);
    }
}

////////////////////////////////////////////////////////////////////////////////
// chunk storage

// extend world in blocks of this many chunks 
#define WORLDALLOC (1<<4)

int32_t find_chunk(gsworld *w, int32_t X, int32_t Z) {
    if (w->Xs==0 || w->Zs==0) {
        //printf("%d,%d\n",X,Z);
        // this is the very first allocation, just set our offset coords
        w->Xo = X&~(WORLDALLOC-1);
        w->Zo = Z&~(WORLDALLOC-1);
        w->Xs = WORLDALLOC;
        w->Zs = WORLDALLOC;
        lh_alloc_num(w->chunks,w->Xs*w->Zs);
        return CHUNKIDX(w, X, Z);
    }

    int nXo=w->Xo,nZo=w->Zo;
    int nXs=w->Xs,nZs=w->Zs;

    // check if we need to extend our world westward
    if (X < w->Xo) {
        nXo = X&~(WORLDALLOC-1);
        nXs+=(w->Xo-nXo);
        printf("extending WEST: X=%d: %d->%d , %d->%d\n",X,w->Xo,nXo,w->Xs,nXs);
    }

    // extending eastwards
    else if (X >= w->Xo+w->Xs) {
        nXs = (X|(WORLDALLOC-1))+1 - w->Xo;
        printf("extending EAST: X=%d: %d->%d , %d->%d\n",X,w->Xo,nXo,w->Xs,nXs);
    }

    // extending to the north
    if (Z < w->Zo) {
        nZo = Z&~(WORLDALLOC-1);
        nZs+=(w->Zo-nZo);
        printf("extending NORTH: Z=%d: %d->%d , %d->%d\n",Z,w->Zo,nZo,w->Zs,nZs);
    }

    // extending to the south
    else if (Z >= w->Zo+w->Zs) {
        nZs = (Z|(WORLDALLOC-1))+1 - w->Zo;
        printf("extending SOUTH: Z=%d: %d->%d , %d->%d\n",Z,w->Zo,nZo,w->Zs,nZs);
    }

    // if the size has changed, we need to resize the chunks array
    if (w->Xs != nXs || w->Zs != nZs) {
        //printf("%d,%d\n",X,Z);
        // allocate a new array for the chunk pointers
        lh_create_num(gschunk *, chunks, nXs*nZs);

        // this is the offset in the _new_ array, from
        // where we will start copying the chunks from the old one
        int32_t offset = (w->Xo-nXo) + (w->Zo-nZo)*nXs;

        int i;
        for(i=0; i<w->Zs; i++) {
            //printf("row %d, offset=%d\n",i,offset);
            gschunk ** oldrow = w->chunks+i*w->Xs;
            //printf("moving %d*%zd bytes from %p (w->chunks[%d]) to %p (chunks[%d])\n",
            //       w->Xs,sizeof(gschunk *),oldrow,i*w->Xs,chunks+offset,offset);
            memmove(chunks+offset, oldrow, w->Xs*sizeof(gschunk *));
            offset += nXs;
        }
        lh_free(w->chunks);
        w->chunks = chunks;

        w->Xo = nXo; w->Xs = nXs;
        w->Zo = nZo; w->Zs = nZs;
    }

    return CHUNKIDX(w, X, Z);
}

static void insert_chunk(chunk_t *c) {
    gsworld *w = gs.world;
    int32_t idx = find_chunk(w, c->X, c->Z);

    if (!w->chunks[idx])
        lh_alloc_obj(w->chunks[idx]);
    gschunk *gc = w->chunks[idx];

    int i;
    for(i=0; i<16; i++) {
        if (c->cubes[i]) {
            memmove(gc->blocks+i*4096,   c->cubes[i]->blocks,   4096*sizeof(bid_t));
            memmove(gc->light+i*2048,    c->cubes[i]->light,    2048*sizeof(light_t));
            memmove(gc->skylight+i*2048, c->cubes[i]->skylight, 2048*sizeof(light_t));
        }
        else {
            memset(gc->blocks+i*4096,   0, 4096*sizeof(bid_t));
            memset(gc->light+i*2048,    0, 2048*sizeof(light_t));
            memset(gc->skylight+i*2048, 0, 2048*sizeof(light_t));
        }
        memmove(gc->biome, c->biome, 256);
    }
}

static void remove_chunk(int32_t X, int32_t Z) {
    int32_t idx = find_chunk(gs.world, X, Z);
    lh_free(gs.world->chunks[idx]);
    //TODO: resize the chunk array
}

static void free_chunks(gsworld *w) {
    if (!w) return;

    if (w->chunks) {
        int32_t sz = w->Xs*w->Zs;
        int i;
        for(i=0; i<sz; i++)
            lh_free(w->chunks[i]);

        lh_free(w->chunks);
    }
}

static void dump_chunks(gsworld *w) {
    if (!w) return;

    if (w->chunks) {
        int x,z;
        for(z=0; z<w->Zs; z++) {
            int32_t off = w->Xs*z;
            printf("%4d ",z+w->Zo);
            for(x=0; x<w->Xs; x++)
                printf("%c", w->chunks[off+x]?'#':'.');
            printf("\n");
        }
    }
}

static void change_dimension(int dimension) {
    printf("Switching to dimension %d\n",dimension);

    // prune all chunks of the current dimension
    if (gs.opt.prune_chunks)
        free_chunks(gs.world);

    switch(dimension) {
        case 0:  gs.world = &gs.overworld; break;
        case -1: gs.world = &gs.nether; break;
        case 1:  gs.world = &gs.end; break;
    }
}

static void modify_blocks(int32_t X, int32_t Z, blkrec *blocks, int32_t count) {
    int32_t idx = find_chunk(gs.world, X, Z);

    // get the chunk and allocate if it's not allocated yet
    gschunk * gc = gs.world->chunks[idx];
    if (!gc) {
        lh_alloc_obj(gs.world->chunks[idx]);
        gc = gs.world->chunks[idx];
    }

    int i;
    for(i=0; i<count; i++) {
        blkrec *b = blocks+i;
        int32_t boff = ((int32_t)b->y<<8)+(b->z<<4)+b->x;
#if 0
        printf("Modify Block @ %d,%d,%d   %d(%d) => %d(%d)\n",
               (X<<4)+b->x,(Z<<4)+b->z,b->y,
               gc->blocks[boff].bid, gc->blocks[boff].meta,
               b->bid.bid, b->bid.meta);
#endif
        gc->blocks[boff] = b->bid;
    }
}

bid_t * export_cuboid(int32_t Xl, int32_t Xs, int32_t Zl, int32_t Zs,
                      int32_t yl, int32_t ys, bid_t *data) {

    int32_t sz = ys*Xs*Zs*256;  // total size of the cuboid in blocks

    // if user did not provide a buffer, allocate it for him
    if (!data) lh_alloc_num(data, sz);

    char buf[4096];
    lh_clear_obj(buf);

    int X,Z,j,k;
    for(X=Xl; X<Xl+Xs; X++) {
        for(Z=Zl; Z<Zl+Zs; Z++) {
            // get the chunk data
            int idx = find_chunk(gs.world,X,Z);
            gschunk *gc = gs.world->chunks[idx];
            if (!gc) continue;

            // offset of this chunk's data (in blocks)
            // where it will be placed in the cuboid
            int xoff = (X-Xl)*16;
            int zoff = (Z-Zl)*16;

            // block offset of the chunk's start in the cuboid buffer
            int boff = xoff + zoff*Xs*16;

            for(j=0; j<ys; j++) {
                // block offset of y slice in the chunk's block data
                int yoff = (j+yl)*256;
                int doff = boff + Xs*Zs*256*j;
                for(k=0; k<16; k++) {
                    memcpy(data+doff, gc->blocks+yoff, 16*sizeof(bid_t));
                    yoff += 16;
                    doff += Xs*16;
                }
            }
        }
    }

    return data;
}

////////////////////////////////////////////////////////////////////////////////
// Inventory tracking

static inline int sameitem(slot_t *a, slot_t *b) {
    //Note: we don't need to consider sameness of items like
    // 0x4b/0x4c (Redstone Torch on/off), since one type
    // may only appear as a block in the map, never as item

    if (a->item != b->item) return 0;

    // non-stackable items are never "same"
    if (ITEMS[a->item].flags&I_NSTACK) return 0;

    // items w/o I_MTYPE do not use meta for type
    if (!(ITEMS[a->item].flags&I_MTYPE)) return 1;

    // other bits may be active on the map, but in the inventory
    // they will be same for the same material
    return a->damage == b->damage;
}

void dump_inventory() {
    int i;
    printf("Dumping inventory:\n");
    for(i=-1; i<45; i++) {
        slot_t *s;
        if (i<0) {
            s=&gs.inv.drag;
            printf(" DS : ");
        }
        else {
            s=&gs.inv.slots[i];
            printf(" %2d : ", i);
        }

        char buf[4096];
        if (get_item_name(buf, s))
            printf("%-20s x%-2d\n", buf, s->count);
        else
            printf("%4x x%-2d dmg=%d\n",s->item,s->count,s->damage);
    }
}

#define IAQ  GAR1(gs.inv.iaq)
#define pIAQ P(gs.inv.iaq)
#define cIAQ C(gs.inv.iaq)

static void dump_slot(slot_t *s) {
    char buf[256];
    printf("item=%d (%s)", s->item, get_item_name(buf, s));
    if (s->item != -1) {
        printf(", count=%d, damage=%d", s->count, s->damage);
    }
}

static int find_invaction(int aid) {
    int i;
    for (i=0; i<cIAQ; i++)
        if (pIAQ[i].aid == aid)
            return i;
    return -1;
}

// ensure that an emptied slot is in a consistent state
static inline void prune_slot(slot_t *s) {
    if (s->count <= 0 || s->item == -1) {
        s->count  = 0;
        s->item   = -1;
        s->damage = 0;
        //TODO: nbt data
    }
}

static int slot_transfer(slot_t *f, slot_t *t, int count) {
    assert(f->count >= count);
    assert(f->item > 0);

    printf("  From: "); dump_slot(f); printf("\n");
    printf("  To  : "); dump_slot(t); printf("\n");

    if (t->item == -1) {
        // destination slot is empty
        t->item = f->item;
        t->damage = f->damage;
        t->count = count;
        f->count-=count;

        printf("  put into empty slot, now: f:%d, t:%d\n",f->count,t->count);

        prune_slot(f);
        return 1;
    }

    if (sameitem(f,t)) {
        t->count+=count;
        f->count-=count;

        printf("  put into non-empty slot, now: f:%d, t:%d\n",f->count,t->count);

        prune_slot(f);
        return 1;
    }

    printf("Attempting slot_transfer with different item types:\n");

    assert(0);
}

static void slot_swap(slot_t *f, slot_t *t) {
    slot_t temp;
    temp = *t;
    *t = *f;
    *f = temp;
}

#define GREATERHALF(x) (((x)>>1)+((x)&1))

static void inv_click(int button, int16_t sid) {
    char name[256]; // buffer for the item name printing

    if (button!=0 && button!=1) {
        printf("button=%d not supported\n",button);
        return;
    }

    if (sid==0) {
        slot_t *s = &gs.inv.slots[0];
        slot_t *d = &gs.inv.drag;

        // nothing in the product slot, no-op
        if (s->item<0) {
            printf("*** No-op: nothing in the product slot\n");
            return;
        }

        if (d->item<0 || sameitem(s,d)) {
            // we can pick up items from the product slot
            int stacksize = ITEMS[s->item].flags&I_NSTACK ? 1 :
                ( ITEMS[s->item].flags&I_S16 ? 16 : 64 );

            if ( d->count + s->count > stacksize) {
                printf("*** No-op: can't pick up another %dx %s into dragslot from product slot\n",
                       s->count, get_item_name(name,s));
            }
            else {
                slot_transfer(s, d, s->count);

                // remove 1x of each source items from the crafting grid
                int i;
                for(i=1; i<5; i++) {
                    if (gs.inv.slots[i].item >= 0)
                        gs.inv.slots[i].count --;
                    prune_slot(&gs.inv.slots[i]);
                }
            }
        }
        return;
    }

    if (gs.inv.drag.item < 0) {
        // not dragging anything

        // click with nothing dragged outside of window - no action
        if (sid<0) return;

        slot_t *s = &gs.inv.slots[sid];

        // empty slot - no action
        if (s->item < 0) return;

        switch (button) {
            case 0:
                // left-click
                printf("*** Pick %dx %s from slot %d to drag-slot\n",
                       s->count, get_item_name(name,s), sid);
                slot_transfer(s, &gs.inv.drag, s->count);
                break;
            case 1:
                // right-click
                printf("*** Pick %dx %s from slot %d to drag-slot\n",
                       GREATERHALF(s->count), get_item_name(name,s), sid);
                slot_transfer(s, &gs.inv.drag, GREATERHALF(s->count));
                break;
        }

        return;
    }

    // clicking with a dragging hand

    if (sid<0) {
        // clicking outside of window - throwing items
        switch (button) {
            case 0:
                // left-click - throw all
                printf("*** Throw out %dx %s from drag slot\n",
                       gs.inv.drag.count, get_item_name(name,&gs.inv.drag) );
                gs.inv.drag.count = 0;
                break;
            case 1:
                // right-click - throw one
                printf("*** Throw out 1x %s from drag slot\n",
                       get_item_name(name,&gs.inv.drag));
                gs.inv.drag.count--;
                break;
        }
        prune_slot(&gs.inv.drag);
        return;
    }

    // clicking with a full hand on a slot

    slot_t *s = &gs.inv.slots[sid];
    if (s->item<0) {
        // the target slot is empty
        switch (button) {
            case 0:
                // left-click - put all
                printf("*** Put %dx %s from drag slot to slot %d\n",
                       gs.inv.drag.count, get_item_name(name,&gs.inv.drag), sid);
                slot_transfer(&gs.inv.drag, s, gs.inv.drag.count);
                break;
            case 1:
                // right-click - put one
                printf("*** Put 1x %s from drag slot to slot %d\n",
                       get_item_name(name,&gs.inv.drag), sid);
                slot_transfer(&gs.inv.drag, s, 1);
                break;
        }
        return;
    }

    // clicking with a full hand on a non-empty slot

    if (!sameitem(s,&gs.inv.drag) ) {
        char name2[256];
        // the slot conains a different type of item, or the items
        // are non-stackable (e.g. weapons) - swap items
        printf("*** Swap %dx %s in the drag slot with %dx %s in slot %d\n",
               gs.inv.drag.count, get_item_name(name,&gs.inv.drag),
               s->count, get_item_name(name2,s), sid);

        slot_swap(s, &gs.inv.drag);
        return;
    }

    // clicking with a full hand on a non-empty slot - items are same and stackable

    // how many more items can we put into slot?
    int count = ( (ITEMS[s->item].flags&I_S16) ? 16 : 64 ) - s->count;
    if (count > gs.inv.drag.count) count=gs.inv.drag.count;
    if (button==1 && count>0) count=1;

    if (count > 0) {
        printf("*** Add %dx %s from drag slot to slot %d\n",
               count, get_item_name(name,&gs.inv.drag), sid);
        s->count += count;
        gs.inv.drag.count -= count;
    }
    else {
        printf("*** Can't put more %s from drag slot to slot %d - slot full\n",
               get_item_name(name,&gs.inv.drag), sid);
    }

    prune_slot(s);
    prune_slot(&gs.inv.drag);
}

int64_t find_stackable_slots(int64_t mask, slot_t *s) {
    // return empty mask if the item itself is non-stackable
    if (ITEMS[s->item].flags&I_NSTACK) return 0LL;

    int stacksize = (ITEMS[s->item].flags&I_S16)?16:64;

    int i;
    for(i=9; i<45; i++) {
        if (!(mask & (1LL<<i))) continue; // skip slots not in the mask
        slot_t *t = &gs.inv.slots[i];
        int capacity = stacksize - t->count;
        if (!sameitem(t,s) || capacity <=0 )
            // mask slots that have different type or no capacity
            mask &= ~(1LL<<i);
    }

    return mask;
}

#define SLOTS_QUICKBAR (0x1ffLL<<36)
#define SLOTS_MAINAREA (0x7ffffffLL<<9)
#define SLOTS_ALLINV   (SLOTS_QUICKBAR|SLOTS_MAINAREA)

static void inv_shiftclick(int button, int16_t sid) {
    char name[256]; // buffer for the item name printing

    if (button!=0 && button!=1) {
        printf("button=%d not supported\n",button);
        return;
    }

    slot_t *f = &gs.inv.slots[sid];
    if (f->item<0) return; // shift-click on an empty slot - no-op

    int i;

    // handle armor-type items
    int armorslot = -1;

    if (I_HELMET(f->item))     armorslot = 5;
    if (I_CHESTPLATE(f->item)) armorslot = 6;
    if (I_LEGGINGS(f->item))   armorslot = 7;
    if (I_BOOTS(f->item))      armorslot = 8;

    slot_t *as = (armorslot>0) ? &gs.inv.slots[armorslot] : NULL;

    // armor item and the appropriate slot is empty?
    if (as && as->item<0) {
        slot_transfer(f,as,1);
        return;
    }


    // set the bitmask suitable to the inventory area where the item can be moved
    int64_t mask;
    if (sid>=9 && sid<=36) {
        // main area - try to move to the quickbar
        mask = SLOTS_QUICKBAR;
    }
    else if (sid>36) {
        // quickbar - try to move to the main area
        mask = SLOTS_MAINAREA;
    }
    else {
        // armor/crafting/product slots - try to make to main or quickbar area
        mask = SLOTS_ALLINV;
    }

    // bitmask of the stackable slots with available capacity
    int64_t smask = find_stackable_slots(mask, f);

    int stacksize = (ITEMS[gs.inv.slots[sid].item].flags&I_NSTACK) ? 1 :
        ( (ITEMS[gs.inv.slots[sid].item].flags&I_S16) ? 16 : 64 );

    // if we distribute from the product slot, search in the
    // opposite direction, so the quickbar gets filled first
    int start = (sid==0) ? 44 : 8;
    int end   = (sid==0) ? 9 : 45;
    int inc   = (sid==0) ? -1 : 1;

    if (sid == 0) {
        // we are crafting

        // calculate how many times the item can be crafted
        int craft_output = gs.inv.slots[0].count;
        int craft_times=64;
        // we can craft at most as many times as the least source material item
        // in the crafting slots (1..4)
        for(i=1; i<5; i++)
            if (gs.inv.slots[i].item >= 0 && gs.inv.slots[i].count<craft_times)
                craft_times=gs.inv.slots[i].count;

        // calculate our entire capacity
        int capacity = 0;
        for(i=9; i<45; i++) {
            if (smask & (1LL<<i)) { // stackable slots
                capacity += (stacksize - gs.inv.slots[i].count);
            }
            else if (gs.inv.slots[i].item < 0) { // empty slots
                capacity += stacksize;
            }
        }

        // recalculate how many times we can craft w/o exceeding our total capacity
        while(craft_output*craft_times > capacity+(craft_output-1)) craft_times--;
        // (craft_output-1) is necessary to mimic the notchian client behavior -
        // it will try to fill up all available slots to the full capacity even if
        // it means losing some remaining items from the last crafted batch

        // remove as many source items
        for(i=1; i<5; i++) {
            if (gs.inv.slots[i].item >= 0)
                gs.inv.slots[i].count -= craft_times;
            prune_slot(&gs.inv.slots[i]);
        }

        // set our slot 0 to hold that many items
        gs.inv.slots[0].count = craft_output*craft_times;
        if (gs.inv.slots[0].count > capacity) gs.inv.slots[0].count=capacity;
    }

    if (smask) {
        while (f->count>0 && smask) {
            for(i=start; i!=end; i+=inc) {
                if (!(smask & (1LL<<i))) continue;
                int capacity = stacksize-gs.inv.slots[i].count;

                //put as much we can into that slot and update the smask
                int amount = (f->count > capacity) ? capacity : f->count;
                printf("*** Distribute %dx %s from slot %d to slot %d (move to stack)\n",
                       amount, get_item_name(name,f), sid, i);
                slot_transfer(f, &gs.inv.slots[i], amount);
                smask &= ~(1LL<<i);
                prune_slot(f);

                break;
            }
        }

        if (sid > 0) // except when we are crafting -
            return;  // do not seek other slots to distribute items even if some remain
    }

    // next try to find an empty slot
    for(i=start; i!=end && f->count>0; i+=inc) {
        if (mask & (1LL<<i)) {
            slot_t *t = &gs.inv.slots[i];
            if (t->item == -1) {
                int amount = f->count > stacksize ? stacksize : f->count;

                printf("*** Distribute %dx %s from slot %d to slot %d (move to empty)\n",
                       amount, get_item_name(name,f), sid, i);
                slot_transfer(f,t,amount);
            }
        }
    }

    // if no trasfer was possible for all items, the remaining items simply stay in the source slot
}

static void inv_paint(int button, int16_t sid) {
    char name[256]; // buffer for the item name printing

    if (!((button>=0 && button<=2) || (button>=4 && button<=6))) {
        printf("button=%d not supported\n",button);
        return;
    }

    if (gs.inv.drag.item < 0) {
        printf("Painting mode with nothing in drag slot - ignoring\n");
        gs.inv.inconsistent = 1;
        return;
    }

    int i;

    switch(button) {
        case 0:
        case 4:
            printf("Paint start\n");
            assert(gs.inv.pcount==0);
            assert(sid<0);
            if (gs.inv.ptype!=0) {
                printf("Incorrect paint sequence, resetting\n");
                gs.inv.ptype = 0;
                gs.inv.pcount = 0;
                break;
            }
            gs.inv.ptype = button+1;
            break;

        case 1:
        case 5:
            if (gs.inv.ptype!=button) {
                printf("Incorrect paint sequence, resetting\n");
                gs.inv.ptype = 0;
                gs.inv.pcount = 0;
                break;
            }
            printf("Paint add slot %d\n",sid);
            gs.inv.pslots[gs.inv.pcount++] = sid;
            break;

        case 2:
        case 6:
            assert(sid<0);
            if (gs.inv.ptype!=button-1) {
                printf("Incorrect paint sequence, resetting\n");
                gs.inv.ptype = 0;
                gs.inv.pcount = 0;
                break;
            }
            printf("Paint end, %d slots\n", gs.inv.pcount);

            if (gs.inv.pcount>0) {
                int amount = 1;
                if (button==2)
                    amount = gs.inv.drag.count / gs.inv.pcount;

                int stacksize = ITEMS[gs.inv.drag.item].flags&I_NSTACK ? 1 :
                    ( ITEMS[gs.inv.drag.item].flags&I_S16 ? 16 : 64 );

                for(i=0; i<gs.inv.pcount; i++) {
                    int idx = gs.inv.pslots[i];
                    int slot_amount = amount;
                    if (gs.inv.slots[idx].count + slot_amount > stacksize)
                        slot_amount = stacksize - gs.inv.slots[idx].count;

                    printf("*** Paint %dx %s from drag slot to slot %d\n",
                           slot_amount, get_item_name(name,&gs.inv.drag), gs.inv.pslots[i]);
                    slot_transfer(&gs.inv.drag, &gs.inv.slots[gs.inv.pslots[i]], slot_amount);
                    printf("%d remain in the dragslot\n",gs.inv.drag.count);
                }
            }

            gs.inv.pcount = 0;
            gs.inv.ptype  = 0;
            break;
    }
}

static void inv_throw(int button, int16_t sid) {
    char name[256]; // buffer for the item name printing

    if (button!=0 && button!=1) {
        printf("button=%d not supported\n",button);
        return;
    }

    if (sid<0) return; // outside of the window - no operation

    slot_t *s = &gs.inv.slots[sid];
    if (s->item == -1) return; // empty slot - no operation

    int amount = button ? s->count : 1;

    printf("*** Throw out %dx %s from slot %d\n",
           amount, get_item_name(name,s), sid);

    s->count -= amount;
    prune_slot(s);
}

////////////////////////////////////////////////////////////////////////////////
// Packet processing

#define GSP(name)                               \
    case name: {                                \
        name##_pkt *tpkt = &pkt->_##name;

#define _GSP break; }

int gs_packet(MCPacket *pkt) {
    switch (pkt->pid) {

        ////////////////////////////////////////////////////////////////
        // Entities tracking

        GSP(SP_SpawnPlayer) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_PLAYER;
            //TODO: name
            //TODO: mark players hostile/neutral/friendly depending on the faglist
        } _GSP;

        GSP(SP_SpawnObject) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_OBJECT;
        } _GSP;

        GSP(SP_SpawnMob) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_MOB;

            e->mtype = tpkt->mobtype;
            // mark all monster mobs hostile except pigmen (too dangerous)
            // and bats (too cute)
            if (e->mtype >= 50 && e->mtype <90 && e->mtype!=57 && e->mtype!=65)
                e->hostile = 1;
            // mark creepers extra hostile to make them priority targets
            if (e->mtype == 50)
                e->hostile = 2;
        } _GSP;

        GSP(SP_SpawnPainting) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->pos.x*32;
            e->y  = tpkt->pos.y*32;
            e->z  = tpkt->pos.z*32;
            e->type = ENTITY_OTHER;
        } _GSP;

        GSP(SP_SpawnExperienceOrb) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_OTHER;
        } _GSP;

        GSP(SP_DestroyEntities) {
            int i;
            for(i=0; i<tpkt->count; i++) {
                int idx = find_entity(tpkt->eids[i]);
                if (idx<0) continue;
                lh_arr_delete(GAR(gs.entity),idx);
            }
        } _GSP;

        GSP(SP_EntityRelMove) {
            int idx = find_entity(tpkt->eid);
            if (idx<0) break;
            entity *e = P(gs.entity)+idx;
            e->x += tpkt->dx;
            e->y += tpkt->dy;
            e->z += tpkt->dz;
        } _GSP;

        GSP(SP_EntityLookRelMove) {
            int idx = find_entity(tpkt->eid);
            if (idx<0) break;
            entity *e = P(gs.entity)+idx;
            e->x += tpkt->dx;
            e->y += tpkt->dy;
            e->z += tpkt->dz;
        } _GSP;

        GSP(SP_EntityTeleport) {
            int idx = find_entity(tpkt->eid);
            if (idx<0) break;
            entity *e = P(gs.entity)+idx;
            e->x = tpkt->x;
            e->y = tpkt->y;
            e->z = tpkt->z;
        } _GSP;

        ////////////////////////////////////////////////////////////////
        // Player coordinates

        GSP(SP_PlayerPositionLook) {
            if (tpkt->flags != 0) {
                // more Mojang retardedness
                printf("SP_PlayerPositionLook with relative values, ignoring packet\n");
                break;
            }
            gs.own.x     = (int)(tpkt->x*32);
            gs.own.y     = (int)(tpkt->y*32);
            gs.own.z     = (int)(tpkt->z*32);
            gs.own.yaw   = tpkt->yaw;
            gs.own.pitch = tpkt->pitch;
        } _GSP;

        GSP(CP_Player) {
            gs.own.onground = tpkt->onground;
        } _GSP;

        GSP(CP_PlayerPosition) {
            gs.own.x     = (int)(tpkt->x*32);
            gs.own.y     = (int)(tpkt->y*32);
            gs.own.z     = (int)(tpkt->z*32);
            gs.own.onground = tpkt->onground;
        } _GSP;

        GSP(CP_PlayerLook) {
            gs.own.yaw   = tpkt->yaw;
            gs.own.pitch = tpkt->pitch;
            gs.own.onground = tpkt->onground;
        } _GSP;

        GSP(CP_PlayerPositionLook) {
            gs.own.x     = (int)(tpkt->x*32);
            gs.own.y     = (int)(tpkt->y*32);
            gs.own.z     = (int)(tpkt->z*32);
            gs.own.yaw   = tpkt->yaw;
            gs.own.pitch = tpkt->pitch;
            gs.own.onground = tpkt->onground;
        } _GSP;

        GSP(SP_JoinGame) {
            change_dimension(tpkt->dimension);
        } _GSP;

        GSP(SP_Respawn) {
            change_dimension(tpkt->dimension);
        } _GSP;

        ////////////////////////////////////////////////////////////////
        // Chunks

        GSP(SP_ChunkData) {
            if (!tpkt->chunk.mask) {
                if (gs.opt.prune_chunks) {
                    remove_chunk(tpkt->chunk.X,tpkt->chunk.Z);
                }
            }
            else {
                insert_chunk(&tpkt->chunk);
            }
        } _GSP;

        GSP(SP_MapChunkBulk) {
            int i;
            for(i=0; i<tpkt->nchunks; i++)
                insert_chunk(&tpkt->chunk[i]);
        } _GSP;

        GSP(SP_BlockChange) {
            blkrec block = {
                .x = tpkt->pos.x&0xf,
                .z = tpkt->pos.z&0xf,
                .y = (uint8_t)tpkt->pos.y,
                .bid = tpkt->block,
            };
            modify_blocks(tpkt->pos.x>>4,tpkt->pos.z>>4,&block,1);
        } _GSP;

        GSP(SP_MultiBlockChange) {
            modify_blocks(tpkt->X,tpkt->Z,tpkt->blocks,tpkt->count);
        } _GSP;

        GSP(SP_Explosion) {
            int xc = (int)(tpkt->x);
            int yc = (int)(tpkt->y);
            int zc = (int)(tpkt->z);

            int i;
            for(i=0; i<tpkt->count; i++) {
                int x = xc +tpkt->blocks[i].dx;
                int y = yc +tpkt->blocks[i].dy;
                int z = zc +tpkt->blocks[i].dz;
                blkrec block = {
                    .x = x&0xf,
                    .z = z&0xf,
                    .y = (uint8_t)y,
                    .bid = 0,
                };
                modify_blocks(x>>4,z>>4,&block,1);
            }
        } _GSP;

        ////////////////////////////////////////////////////////////////
        // Inventory

        GSP(SP_HeldItemChange) {
            gs.inv.held = (uint8_t)tpkt->sid;
        } _GSP;

        GSP(CP_HeldItemChange) {
            gs.inv.held = (uint8_t)tpkt->sid;
        } _GSP;

        GSP(SP_SetSlot) {
            switch(tpkt->wid) {
                case 0:
                    dump_packet(pkt);
                    // main inventory window (wid=0)
                    assert(tpkt->sid>=0 && tpkt->sid<45);

                    // copy the slot to our inventory slot
                    gs.inv.slots[tpkt->sid] = tpkt->slot;
                    break;
                case 255:
                    // drag-slot
                    dump_packet(pkt);
                    if (gs.inv.drag.item != tpkt->slot.item ||
                        gs.inv.drag.count != tpkt->slot.count ) {
                        printf("*** drag slot corrected by the server\n");
                        gs.inv.drag = tpkt->slot;
                    }
#if 0
                    else {
                        printf("drag slot consistent\n");
                    }
#endif
                    break;
            }
        } _GSP;

        GSP(CP_ClickWindow) {
            // ignore non-inventory windows - we will receive an explicit update
            // for those via SP_SetSlot messages after the window closes,
            // but the main inventory window (wid=0) needs to be tracked
            if (tpkt->wid != 0) break;
            dump_packet(pkt);

            // in this function we do not actually modify the the inventory
            // but just record the action for later - actual change occurs
            // when we receive the SP_ConfirmTransaction packet

            invact * a = lh_arr_new(IAQ);
            a->aid    = tpkt->aid;
            a->mode   = tpkt->mode;
            a->button = tpkt->button;
            a->sid    = tpkt->sid;

            switch (a->mode) {
                case 0:
                    printf("Inventory Click aid=%d, mode=%d, button=%d, sid=%d\n",
                           a->aid, a->mode, a->button, a->sid);
                    inv_click(a->button, a->sid);
                    break;

                case 1:
                    printf("Inventory Shift-Click aid=%d, mode=%d, button=%d, sid=%d\n",
                           a->aid, a->mode, a->button, a->sid);
                    inv_shiftclick(a->button, a->sid);
                    break;

                case 4:
                    printf("Inventory Throw aid=%d, mode=%d, button=%d, sid=%d\n",
                           a->aid, a->mode, a->button, a->sid);
                    inv_throw(a->button, a->sid);
                    break;

                case 5:
                    printf("Inventory Paint aid=%d, mode=%d, button=%d, sid=%d\n",
                           a->aid, a->mode, a->button, a->sid);
                    inv_paint(a->button, a->sid);
                    break;

                default:
                    printf("Unsupported inventory action mode %d\n",a->mode);
                    gs.inv.inconsistent = 1;
            }
        } _GSP;

        GSP(SP_ConfirmTransaction) {
            dump_packet(pkt);
            if (tpkt->wid != 0) break;

            int idx = find_invaction(tpkt->aid);
            if (idx < 0) {
                printf("Warning: action %d not found!\n",tpkt->aid);
                break;
            }
            invact a = pIAQ[idx];
            lh_arr_delete(IAQ, idx);

            if (!tpkt->accepted) {
                printf("Warning: action %d was not accepted!\n",tpkt->aid);
                gs.inv.inconsistent = 1;
                break;
            }
        } _GSP;

        case CP_CloseWindow:
        case SP_CloseWindow: {
            dump_packet(pkt);
            // discard anything in dragslot - if you happen to drag
            // some items when the window is closed, they will be thrown
            gs.inv.drag.item = -1;
            prune_slot(&gs.inv.drag);

            // discard the crafting and the product slot
            int i;
            for(i=0; i<5; i++) {
                gs.inv.slots[i].item = -1;
                prune_slot(&gs.inv.slots[i]);
            }

            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void gs_reset() {
    int i;

    if (gs_used)
        gs_destroy();

    CLEAR(gs);

    // set all items in the inventory to -1 to define them as empty
    for(i=0; i<64; i++) {
        gs.inv.slots[i].item = -1;
    }
    // reset the currently dragged item to none
    gs.inv.drag.item = -1;

    gs_used = 1;
}

void gs_destroy() {
    // delete tracked entities
    lh_free(P(gs.entity));

    //dump_chunks(&gs.overworld);

    free_chunks(&gs.overworld);
    free_chunks(&gs.nether);
    free_chunks(&gs.end);

    lh_arr_free(IAQ);
}

int gs_setopt(int optid, int value) {
    switch (optid) {
        case GSOP_PRUNE_CHUNKS:
            gs.opt.prune_chunks = value;
            break;
        case GSOP_SEARCH_SPAWNERS:
            gs.opt.search_spawners = value;
            break;
        case GSOP_TRACK_ENTITIES:
            gs.opt.track_entities = value;
            break;

        default:
            LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }

    return 0;
}

int gs_getopt(int optid) {
    switch (optid) {
        case GSOP_PRUNE_CHUNKS:
            return gs.opt.prune_chunks;
        case GSOP_SEARCH_SPAWNERS:
            return gs.opt.search_spawners;
        case GSOP_TRACK_ENTITIES:
            return gs.opt.track_entities;

        default:
            LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }
}
