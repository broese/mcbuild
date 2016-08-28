/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

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

// return pointer to a gschunk with chunk coords X,Z
// NULL, if chunk, or its region/superregion are not allocated
gschunk * find_chunk(gsworld *w, int32_t X, int32_t Z, int allocate) {
    int32_t si = CC_2(X,Z);
    if (!w->sreg[si]) {
        if (!allocate) return NULL;
        lh_alloc_obj(w->sreg[si]);
    }
    gssreg * sreg = w->sreg[si];

    int32_t ri = CC_1(X,Z);
    if (!sreg->region[ri]) {
        if (!allocate) return NULL;
        lh_alloc_obj(sreg->region[ri]);
    }
    gsregion * region = sreg->region[ri];

    int32_t ci = CC_0(X,Z);
    if (!region->chunk[ci]) {
        if (!allocate) return NULL;
        lh_alloc_obj(region->chunk[ci]);
    }
    gschunk * chunk = region->chunk[ci];

    return chunk;
}

// add/replace chunk data, allocating storage if necessary
// return pointer to the chunk
static gschunk * insert_chunk(chunk_t *c, int cont) {
    gschunk * gc = find_chunk(gs.world, c->X, c->Z, 1);

    int i;
    for(i=0; i<16; i++) {
        if (c->cubes[i]) {
            memmove(gc->blocks+i*4096,   c->cubes[i]->blocks,   4096*sizeof(bid_t));
            memmove(gc->light+i*2048,    c->cubes[i]->light,    2048*sizeof(light_t));
            memmove(gc->skylight+i*2048, c->cubes[i]->skylight, 2048*sizeof(light_t));
        }
        else if (cont) {
            memset(gc->blocks+i*4096,   0, 4096*sizeof(bid_t));
            memset(gc->light+i*2048,    0, 2048*sizeof(light_t));
            memset(gc->skylight+i*2048, 0, 2048*sizeof(light_t));
        }
    }

    if (cont)
        memmove(gc->biome, c->biome, 256);
    return gc;
}

static void remove_chunk(int32_t X, int32_t Z) {
    gsworld *w = gs.world;

    int32_t si = CC_2(X,Z);
    if (!w->sreg[si]) return;
    gssreg * sreg = w->sreg[si];

    int32_t ri = CC_1(X,Z);
    if (!sreg->region[ri]) return;
    gsregion * region = sreg->region[ri];

    int32_t ci = CC_0(X,Z);
    if (region->chunk[ci])
        nbt_free(region->chunk[ci]->tent);
    lh_free(region->chunk[ci]);

    //TODO: deallocate regions/superregions that become empty
}

static void free_chunks(gsworld *w) {
    if (!w) return;

    int si,ri,ci;
    for(si=0; si<512*512; si++) {
        if (w->sreg[si]) {
            gssreg * sreg = w->sreg[si];

            for(ri=0; ri<256*256; ri++) {
                if (sreg->region[ri]) {
                    gsregion * region = sreg->region[ri];

                    for(ci=0; ci<32*32; ci++) {
                        if (region->chunk[ci])
                            nbt_free(region->chunk[ci]->tent);
                        lh_free(region->chunk[ci]);
                    }
                }
            }
        }
    }
}

static void change_dimension(int dimension) {
    //printf("Switching to dimension %d\n",dimension);

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
    gschunk * gc = find_chunk(gs.world, X, Z, 1);

    int i;
    for(i=0; i<count; i++) {
        blkrec *b = blocks+i;
        int32_t boff = ((int32_t)b->y<<8)+(b->z<<4)+b->x;
        gc->blocks[boff] = b->bid;
    }
}

// return the dimensions of the are of stared chunks
int get_stored_area(gsworld *w, int32_t *Xmin, int32_t *Xmax, int32_t *Zmin, int32_t *Zmax) {
    int si,ri,ci,set=0;
    for(si=0; si<512*512; si++) {
        gssreg * sreg = w->sreg[si];
        if (!sreg) continue;

        for(ri=0; ri<256*256; ri++) {
            gsregion * region = sreg->region[ri];
            if (!region) continue;

            for(ci=0; ci<32*32; ci++) {
                gschunk * gc = region->chunk[ci];
                if (!gc) continue;

                int32_t X = CC_X(si,ri,ci);
                int32_t Z = CC_Z(si,ri,ci);

                if (!set) {
                    *Xmin = *Xmax = X;
                    *Zmin = *Zmax = Z;
                    set = 1;
                }
                else {
                    if (X < *Xmin) *Xmin = X;
                    if (X > *Xmax) *Xmax = X;
                    if (Z < *Zmin) *Zmin = Z;
                    if (Z > *Zmax) *Zmax = Z;
                }
            }
        }
    }

    return set;
}

// Add a tile entity to the chunk data
int store_tile_entity(int32_t X, int32_t Z, nbt_t *ent) {
    gschunk * gc = find_chunk(gs.world, X, Z, 0);
    if (!gc) return 0;

    // allocate TE list if not done yet
    if (!gc->tent)
        gc->tent = nbt_new(NBT_LIST, "TileEntities", 0);

    if (ent->name) lh_free(ent->name);

    nbt_t *xc = nbt_hget(ent, "x");
    nbt_t *yc = nbt_hget(ent, "y");
    nbt_t *zc = nbt_hget(ent, "z");
    assert(xc && yc && zc);

    // if an entity with these coordinates is present in the list, replace it
    int i;
    for(i=0; i<gc->tent->count; i++) {
        nbt_t *e = nbt_aget(gc->tent, i);
        nbt_t *xe = nbt_hget(e, "x");
        nbt_t *ye = nbt_hget(e, "y");
        nbt_t *ze = nbt_hget(e, "z");
        assert(xe && ye && ze);

        if (xe->i==xc->i && ye->i==yc->i && ze->i==zc->i) {
            nbt_free(e);
            gc->tent->li[i] = ent;
            return 1;
        }
    }

    nbt_add(gc->tent, ent);
    return 1;
}

////////////////////////////////////////////////////////////////////////////////

cuboid_t export_cuboid_extent(extent_t ex) {
    int X,Z,y,k;

    // calculate extent sizes in chunks
    int32_t Xl=ex.min.x>>4, Xh=ex.max.x>>4, Xs=Xh-Xl+1;
    int32_t Zl=ex.min.z>>4, Zh=ex.max.z>>4, Zs=Zh-Zl+1;
    int32_t yl=ex.min.y,    yh=ex.max.y,    ys=yh-yl+1;

    // init cuboid_t
    cuboid_t c;
    lh_clear_obj(c);
    c.sr = (size3_t) { ex.max.x-ex.min.x+1, ex.max.y-ex.min.y+1, ex.max.z-ex.min.z+1 };
    c.sa = (size3_t) { Xs*16, ys, Zs*16 };
    c.boff = (ex.min.x-Xl*16) + (ex.min.z-Zl*16)*(Xs*16);

    // allocate memory for the block slices
    for(y=0; y<ys; y++) {
        lh_alloc_num(c.data[y],Xs*Zs*256);
    }

    for(X=Xl; X<=Xh; X++) {
        for(Z=Zl; Z<=Zh; Z++) {
            // get the chunk data
            gschunk *gc = find_chunk(gs.world, X, Z, 0);
            if (!gc) continue;

            // offset of this chunk's data (in blocks)
            // where it will be placed in the cuboid
            int xoff = (X-Xl)*16;
            int zoff = (Z-Zl)*16;

            // block offset of the chunk's start in the cuboid buffer
            int boff = xoff + zoff*c.sa.x;

            for(y=0; y<ys; y++) {
                int yoff = (y+yl)*256;
                for(k=0; k<16; k++) {
                    memcpy(c.data[y]+boff+k*c.sa.x, gc->blocks+yoff, 16*sizeof(bid_t));
                    yoff += 16;
                }
            }
        }
    }

    return c;
}

// get just a single block value at given coordinates
bid_t get_block_at(int32_t x, int32_t z, int32_t y) {
    gschunk *gc = find_chunk(gs.world, x>>4, z>>4, 0);
    if (!gc) return BLOCKTYPE(0,0);

    return gc->blocks[y*256+(z&15)*16+(x&15)];
}

////////////////////////////////////////////////////////////////////////////////
// Inventory tracking

#define DEBUG_INVENTORY 0

int sameitem(slot_t *a, slot_t *b) {
    // items with different item IDs are not same
    // Note: we don't consider sameness of items like
    // 0x4b/0x4c (Redstone Torch on/off), since one type
    // may only appear as a block in the map, never as item
    if (a->item != b->item) return 0;

    // non-stackable items are never "same"
    if (ITEMS[a->item].flags&I_NSTACK) return 0;

    // items with meta/damage-coded type are not same if their meta values differ
    if ((ITEMS[a->item].flags&I_MTYPE) && a->damage != b->damage) return 0;

    // if both items have no NBT data - they are identical at this point
    if (!a->nbt && !b->nbt) return 1;

    // if both items have NBT data attached - it must be identical for items to be stackable
    if (a->nbt && b->nbt) {
        // compare NBT by serializing and comparing resulting binary data
        //TODO: verify if this is sufficient or will we need a more sophisticated comparison?
        uint8_t ba[65536], bb[65536], *wa = ba, *wb = bb;
        nbt_write(&wa, a->nbt);
        nbt_write(&wb, b->nbt);
        ssize_t sa=wa-ba, sb=wb-bb;
        if (sa!=sb) return 0; // different NBT length
        return !memcmp(ba, bb, sa); // compare binary data
    }
    else
        // one item has NBT, the other does not - they are different
        return 0;
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

        //if (s->nbt) nbt_dump(s->nbt);
    }
}

static void slot_transfer(slot_t *f, slot_t *t, int count) {
    if (DEBUG_INVENTORY) {
        printf("*** Slot transfer of %d items\n",count);
        printf("  From: "); dump_slot(f); printf("\n");
        printf("  To  : "); dump_slot(t); printf("\n");
    }

    assert(f->count >= count);
    assert(f->item > 0);

    if (t->item == -1) {
        // destination slot is empty
        t->item = f->item;
        t->damage = f->damage;
        t->count = count;
        f->count-=count;
        t->nbt = nbt_clone(f->nbt);

        if (DEBUG_INVENTORY)
            printf("  put into empty slot, now: f:%d, t:%d\n",f->count,t->count);

        prune_slot(f);
        return;
    }

    if (sameitem(f,t)) {
        t->count+=count;
        f->count-=count;

        if (DEBUG_INVENTORY)
            printf("  put into non-empty slot, now: f:%d, t:%d\n",f->count,t->count);

        prune_slot(f);
        return;
    }

    printf("Attempting slot_transfer with different item types:\n");

    assert(0);
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
            if (DEBUG_INVENTORY)
                printf("*** No-op: nothing in the product slot\n");
            return;
        }

        if (d->item<0 || sameitem(s,d)) {
            // we can pick up items from the product slot
            if ( d->count + s->count > STACKSIZE(s->item)) {
                if (DEBUG_INVENTORY)
                    printf("*** No-op: can't pick up another %dx %s into dragslot from product slot\n",
                           s->count, get_item_name(name,s));
            }
            else {
                slot_t pr;
                pr.item = s->item;
                pr.count = s->count;
                int recipe_valid = 1;

                slot_transfer(s, d, s->count);

                // remove 1x of each source items from the crafting grid
                int i;
                for(i=1; i<5; i++) {
                    if (DEBUG_INVENTORY) {
                        printf("  => remove 1 item from slot %d :\n",i);
                        printf("    Was: "); dump_slot(&gs.inv.slots[i]); printf("\n");
                    }
                    if (gs.inv.slots[i].item >= 0) {
                        gs.inv.slots[i].count --;
                        if (gs.inv.slots[i].count<=0) recipe_valid = 0;
                    }
                    prune_slot(&gs.inv.slots[i]);
                    if (DEBUG_INVENTORY) {
                        printf("    Now: "); dump_slot(&gs.inv.slots[i]); printf("\n");
                    }
                }

                // if our recipe is still valid, restore the product slot
                if (recipe_valid) {
                    s->count = pr.count;
                    s->item  = pr.item;
                    if (DEBUG_INVENTORY) {
                        printf("    Restored product slot:"); dump_slot(s); printf("\n");
                    }
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
                if (DEBUG_INVENTORY)
                    printf("*** Pick %dx %s from slot %d to drag-slot\n",
                           s->count, get_item_name(name,s), sid);
                slot_transfer(s, &gs.inv.drag, s->count);
                break;
            case 1:
                // right-click
                if (DEBUG_INVENTORY)
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
                if (DEBUG_INVENTORY)
                    printf("*** Throw out %dx %s from drag slot\n",
                           gs.inv.drag.count, get_item_name(name,&gs.inv.drag) );
                gs.inv.drag.count = 0;
                break;
            case 1:
                // right-click - throw one
                if (DEBUG_INVENTORY)
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
                if (DEBUG_INVENTORY)
                    printf("*** Put %dx %s from drag slot to slot %d\n",
                           gs.inv.drag.count, get_item_name(name,&gs.inv.drag), sid);
                slot_transfer(&gs.inv.drag, s, gs.inv.drag.count);
                break;
            case 1:
                // right-click - put one
                if (DEBUG_INVENTORY)
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
        if (DEBUG_INVENTORY)
            printf("*** Swap %dx %s in the drag slot with %dx %s in slot %d\n",
                   gs.inv.drag.count, get_item_name(name,&gs.inv.drag),
                   s->count, get_item_name(name2,s), sid);

        swap_slots(s, &gs.inv.drag);
        return;
    }

    // clicking with a full hand on a non-empty slot - items are same and stackable

    // how many more items can we put into slot?
    int count = ( (ITEMS[s->item].flags&I_S16) ? 16 : 64 ) - s->count;
    if (count > gs.inv.drag.count) count=gs.inv.drag.count;
    if (button==1 && count>0) count=1;

    if (count > 0) {
        if (DEBUG_INVENTORY)
            printf("*** Add %dx %s from drag slot to slot %d\n",
                   count, get_item_name(name,&gs.inv.drag), sid);
        s->count += count;
        gs.inv.drag.count -= count;
    }
    else {
        if (DEBUG_INVENTORY)
            printf("*** Can't put more %s from drag slot to slot %d - slot full\n",
                   get_item_name(name,&gs.inv.drag), sid);
    }

    prune_slot(s);
    prune_slot(&gs.inv.drag);
}

int64_t find_stackable_slots(int64_t mask, slot_t *s) {
    // return empty mask if the item itself is non-stackable
    if (ITEMS[s->item].flags&I_NSTACK) return 0LL;

    int i;
    for(i=9; i<45; i++) {
        if (!(mask & (1LL<<i))) continue; // skip slots not in the mask
        slot_t *t = &gs.inv.slots[i];
        int capacity = STACKSIZE(s->item) - t->count;
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

    //TODO: how do we handle a shift click on an armor item IN an armor slot?
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
    if (sid>=9 && sid<36) {
        // main area - try to move to the quickbar
        mask = SLOTS_QUICKBAR;
    }
    else if (sid>=36) {
        // quickbar - try to move to the main area
        mask = SLOTS_MAINAREA;
    }
    else {
        // armor/crafting/product slots - try to make to main or quickbar area
        mask = SLOTS_ALLINV;
    }

    // bitmask of the stackable slots with available capacity
    int64_t smask = find_stackable_slots(mask, f);

    int stacksize = STACKSIZE(gs.inv.slots[sid].item);

    //FIXME: the same should be valid for slots 1-4?
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
                if (DEBUG_INVENTORY)
                    printf("*** Distribute %dx %s from slot %d to slot %d (move to stack)\n",
                           amount, get_item_name(name,f), sid, i);
                slot_transfer(f, &gs.inv.slots[i], amount);
                smask &= ~(1LL<<i);
                prune_slot(f);

                break;
            }
        }
    }

    // if we have some items left after distributing these to partially
    // filled stacked slots, try to put the rest into an empty slot
    for(i=start; i!=end && f->count>0; i+=inc) {
        if (mask & (1LL<<i)) {
            slot_t *t = &gs.inv.slots[i];
            if (t->item == -1) {
                int amount = f->count > stacksize ? stacksize : f->count;

                if (DEBUG_INVENTORY)
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
        if (DEBUG_INVENTORY)
            printf("*** Painting mode with nothing in drag slot - ignoring\n");
        gs.inv.inconsistent = 1;
        return;
    }

    int i;

    switch(button) {
        case 0:
        case 4:
            if (DEBUG_INVENTORY)
                printf("*** Paint start\n");
            assert(gs.inv.pcount==0);
            assert(sid<0);
            if (gs.inv.ptype!=0) {
                if (DEBUG_INVENTORY)
                    printf("*** Incorrect paint sequence, resetting\n");
                gs.inv.ptype = 0;
                gs.inv.pcount = 0;
                break;
            }
            gs.inv.ptype = button+1;
            break;

        case 1:
        case 5:
            if (gs.inv.ptype!=button) {
                if (DEBUG_INVENTORY)
                    printf("*** Incorrect paint sequence, resetting\n");
                gs.inv.ptype = 0;
                gs.inv.pcount = 0;
                break;
            }
            if (DEBUG_INVENTORY)
                printf("*** Paint add slot %d\n",sid);
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
            if (DEBUG_INVENTORY)
                printf("*** Paint end, %d slots\n", gs.inv.pcount);

            if (gs.inv.pcount>0) {
                int amount = 1;
                if (button==2)
                    amount = gs.inv.drag.count / gs.inv.pcount;

                int stacksize = STACKSIZE(gs.inv.drag.item);

                for(i=0; i<gs.inv.pcount; i++) {
                    int idx = gs.inv.pslots[i];
                    int slot_amount = amount;
                    if (gs.inv.slots[idx].count + slot_amount > stacksize)
                        slot_amount = stacksize - gs.inv.slots[idx].count;

                    if (DEBUG_INVENTORY)
                        printf("*** Paint %dx %s from drag slot to slot %d\n",
                               slot_amount, get_item_name(name,&gs.inv.drag), gs.inv.pslots[i]);
                    slot_transfer(&gs.inv.drag, &gs.inv.slots[gs.inv.pslots[i]], slot_amount);
                    if (DEBUG_INVENTORY)
                        printf("*** %d remain in the dragslot\n",gs.inv.drag.count);
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

    if (DEBUG_INVENTORY)
        printf("*** Throw out %dx %s from slot %d\n",
               amount, get_item_name(name,s), sid);

    s->count -= amount;
    prune_slot(s);
}

////////////////////////////////////////////////////////////////////////////////
// Player

int player_direction() {
    int lx=-(int)(65536*sin(gs.own.yaw/180*M_PI));
    int lz=(int)(65536*cos(gs.own.yaw/180*M_PI));

    return (abs(lx) > abs(lz)) ?
        ( (lx<0) ? DIR_WEST : DIR_EAST ) :
        ( (lz<0) ? DIR_NORTH : DIR_SOUTH );
}

////////////////////////////////////////////////////////////////////////////////
// Packet processing

#define GSP(name)                               \
    case name: {                                \
        name##_pkt *tpkt = &pkt->_##name;

#define _GSP break; }

#define update_empty_lines(str) if (str[0]==0) sprintf(str, "\"\"");

void gs_packet(MCPacket *pkt) {
    switch (pkt->pid) {
        ////////////////////////////////////////////////////////////////
        // Gamestate

        GSP(SP_PlayerListItem) {
            int i,j;
            for(i=0; i<C(tpkt->list); i++) {
                pli_t * entry = P(tpkt->list)+i;

                switch(tpkt->action) {
                    case 0: {
                        pli *pli = lh_arr_new_c(GAR(gs.players));
                        memmove(pli->uuid, entry->uuid, 16);
                        pli->name = strdup(entry->name);
                        if (entry->has_dispname) {
                            pli->dispname = strdup(entry->dispname);
                        }
                        break;
                    }

                    case 4: {
                        for(j=0; j<C(gs.players); j++) {
                            if (memcmp(P(gs.players)[j].uuid, entry->uuid, 16)==0) {
                                lh_arr_delete(GAR(gs.players),j);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        } _GSP;

        GSP(SP_TimeUpdate) {
            gs.time = tpkt->time;
        } _GSP;

        ////////////////////////////////////////////////////////////////
        // Entities tracking

        GSP(SP_SpawnPlayer) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_PLAYER;
            e->mtype = Player;
            //TODO: name
            //TODO: mark players hostile/neutral/friendly depending on the faglist
            e->mdata = clone_metadata(tpkt->meta);
        } _GSP;

        GSP(SP_SpawnObject) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_OBJECT;
            e->mtype = Item;
            e->mdata = NULL; //TODO: object metadata
        } _GSP;

        GSP(SP_SpawnMob) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_MOB;

            e->mtype = tpkt->mobtype;
            // mark all monsters hostile, except bats
            if (e->mtype >= 50 && e->mtype <90 && e->mtype!=65)
                e->hostile = 1;
            // mark creepers extra hostile to make them priority targets
            if (e->mtype == 50)
                e->hostile = 2;

            e->mdata = clone_metadata(tpkt->meta);
        } _GSP;

        GSP(SP_SpawnPainting) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->pos.x*32;
            e->y  = tpkt->pos.y*32;
            e->z  = tpkt->pos.z*32;
            e->type = ENTITY_OTHER;
            e->mtype = Painting;
            e->mdata = NULL;
        } _GSP;

        GSP(SP_SpawnExperienceOrb) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_OTHER;
            e->mtype = ExperienceOrb;
            e->mdata = NULL;
        } _GSP;

        GSP(SP_DestroyEntities) {
            int i;
            for(i=0; i<tpkt->count; i++) {
                int idx = find_entity(tpkt->eids[i]);
                if (idx<0) continue;
                free_metadata(P(gs.entity)[idx].mdata);
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

        GSP(SP_EntityMetadata) {
            int idx = find_entity(tpkt->eid);
            if (idx<0) break;
            entity *e = P(gs.entity)+idx;

            if (!e->mdata) {
                e->mdata = clone_metadata(tpkt->meta);
            }
            else {
                int i;
                for(i=0; i<32; i++) {
                    if (tpkt->meta[i].h != 0x7f) {
                        // replace stored metadata with the one from the packet
                        if (e->mdata[i].type == META_SLOT)
                            clear_slot(&e->mdata[i].slot);
                        e->mdata[i] = tpkt->meta[i];

                        // for slot-type metadata we need to propely clone the slot
                        if (tpkt->meta[i].type == META_SLOT)
                            clone_slot(&tpkt->meta[i].slot, &e->mdata[i].slot);
                    }
                }
            }
        } _GSP;

        ////////////////////////////////////////////////////////////////
        // Player

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
            gs.own.eid = tpkt->eid;
            change_dimension(tpkt->dimension);
        } _GSP;

        GSP(SP_Respawn) {
            change_dimension(tpkt->dimension);
        } _GSP;

        GSP(SP_UpdateHealth) {
            gs.own.health = tpkt->health;
            gs.own.food   = tpkt->food;
        } _GSP;

        GSP(CP_EntityAction) {
            if (tpkt->eid == gs.own.eid) {
                switch (tpkt->action) {
                    case 0: gs.own.crouched = 1; break;
                    case 1: gs.own.crouched = 0; break;
                    //TODO: other actions
                }
            }
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
                insert_chunk(&tpkt->chunk, tpkt->cont);
            }
        } _GSP;

        GSP(SP_UpdateBlockEntity) {
            nbt_t *te = tpkt->nbt;
            store_tile_entity(tpkt->loc.x>>4, tpkt->loc.z>>4, nbt_clone(te));
        } _GSP;

        GSP(SP_MapChunkBulk) {
            int i;
            for(i=0; i<tpkt->nchunks; i++)
                insert_chunk(&tpkt->chunk[i], 1);
        } _GSP;

        GSP(SP_BlockChange) {
            blkrec block = {
                {{ tpkt->pos.z&0xf, tpkt->pos.x&0xf }},
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
                    {{ .x = x&0xf, .z = z&0xf }},
                    .y = (uint8_t)y,
                    .bid = BLOCKTYPE(0,0),
                };
                modify_blocks(x>>4,z>>4,&block,1);
            }
        } _GSP;

        GSP(SP_UpdateSign) {
            update_empty_lines(tpkt->line1);
            update_empty_lines(tpkt->line2);
            update_empty_lines(tpkt->line3);
            update_empty_lines(tpkt->line4);

            nbt_t *te = nbt_new(NBT_COMPOUND, NULL, 8,
                nbt_new(NBT_STRING, "id", "Sign"),
                nbt_new(NBT_INT, "x", tpkt->pos.x),
                nbt_new(NBT_INT, "y", tpkt->pos.y),
                nbt_new(NBT_INT, "z", tpkt->pos.z),
                nbt_new(NBT_STRING, "Text1", tpkt->line1),
                nbt_new(NBT_STRING, "Text2", tpkt->line2),
                nbt_new(NBT_STRING, "Text3", tpkt->line3),
                nbt_new(NBT_STRING, "Text4", tpkt->line4)
            );
            store_tile_entity(tpkt->pos.x>>4, tpkt->pos.z>>4, te);
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
            if (!gs.opt.track_inventory) break;
            switch(tpkt->wid) {
                case 0: {
                    //dump_packet(pkt);
                    // main inventory window (wid=0)
                    assert(tpkt->sid>=0 && tpkt->sid<45);

                    // copy the slot to our inventory slot
                    clone_slot(&tpkt->slot, &gs.inv.slots[tpkt->sid]);

                    if (DEBUG_INVENTORY) {
                        printf("*** set slot sid=%d: ", tpkt->sid);
                        dump_slot(&tpkt->slot);
                        printf("\n");
                    }

                    break;
                }
                case 255: {
                    // drag-slot
                    dump_packet(pkt);
                    slot_t * ds = &gs.inv.drag;
                    if (ds->item != tpkt->slot.item ||
                        ds->count != tpkt->slot.count ||
                        ds->damage != tpkt->slot.damage ||
                        (!ds->nbt) != (!tpkt->slot.nbt) ) {
                        if (DEBUG_INVENTORY) {
                            printf("*** drag slot corrected by the server:\n");
                            printf("  tracked: "); dump_slot(ds); printf("\n");
                            printf("  server:  "); dump_slot(&tpkt->slot); printf("\n");
                        }
                        clone_slot(&tpkt->slot, ds);
                    }
                    break;
                }
                default: { // some block with inventory capabilities
                    if (DEBUG_INVENTORY) {
                        printf("*** !!! set slot wid=%d sid=%d: ", tpkt->wid, tpkt->sid);
                        dump_slot(&tpkt->slot);
                        printf("\n");
                    }

                }
            }
        } _GSP;

        GSP(CP_ClickWindow) {
            if (!gs.opt.track_inventory) break;
            // set flag that the client has a window open right now
            gs.inv.windowopen = 1;

            // ignore non-inventory windows - we will receive an explicit update
            // for those via SP_SetSlot messages after the window closes,
            // but the main inventory window (wid=0) needs to be tracked
            //FIXME: sometimes this does not happen and the inventory gets corrupted
            // - needs further debugging
            if (DEBUG_INVENTORY)
                dump_packet(pkt);
            if (tpkt->wid != 0) break;

            switch (tpkt->mode) {
                case 0:
                    if (DEBUG_INVENTORY)
                        printf("Inventory Click aid=%d, mode=%d, button=%d, sid=%d\n",
                               tpkt->aid, tpkt->mode, tpkt->button, tpkt->sid);
                    inv_click(tpkt->button, tpkt->sid);
                    break;

                case 1:
                    if (DEBUG_INVENTORY)
                        printf("Inventory Shift-Click aid=%d, mode=%d, button=%d, sid=%d\n",
                               tpkt->aid, tpkt->mode, tpkt->button, tpkt->sid);
                    inv_shiftclick(tpkt->button, tpkt->sid);
                    break;

                case 4:
                    if (DEBUG_INVENTORY)
                        printf("Inventory Throw aid=%d, mode=%d, button=%d, sid=%d\n",
                               tpkt->aid, tpkt->mode, tpkt->button, tpkt->sid);
                    inv_throw(tpkt->button, tpkt->sid);
                    break;

                case 5:
                    if (DEBUG_INVENTORY)
                        printf("Inventory Paint aid=%d, mode=%d, button=%d, sid=%d\n",
                               tpkt->aid, tpkt->mode, tpkt->button, tpkt->sid);
                    inv_paint(tpkt->button, tpkt->sid);
                    break;

                default:
                    printf("Unsupported inventory action mode %d\n",tpkt->mode);
                    gs.inv.inconsistent = 1;
            }
        } _GSP;

        GSP(SP_OpenWindow) {
            if (DEBUG_INVENTORY) {
                printf("*** Open Window wid=%d type=%s\n",
                       tpkt->wid, tpkt->wtype);
            }

            gs.inv.windowopen = 1;
            gs.inv.wid = tpkt->wid;

            if (tpkt->nslots) {
                // this is a window with storable slots
                // we can use the direct offset specified in the packet

                if (!strcmp(tpkt->wtype, "minecraft:beacon")) {
                    // except when it's a beacon :/
                    gs.inv.woffset = 1;
                }
                else {
                    gs.inv.woffset = tpkt->nslots;
                }
            }
            else {
                // otherwise we need to determine the number of slots from
                // the window type. Mojang.... sigh
                if (!strcmp(tpkt->wtype, "minecraft:crafting_table")) {
                    gs.inv.woffset = 10;
                }
                else if (!strcmp(tpkt->wtype, "minecraft:enchanting_table")) {
                    gs.inv.woffset = 2;
                }
                else if (!strcmp(tpkt->wtype, "minecraft:anvil")) {
                    gs.inv.woffset = 3;
                }
                else {
                    printf("*** Unknown window type %s, inventory offset is likely incorrect\n",tpkt->wtype);
                }
            }

            if (DEBUG_INVENTORY) {
                printf("*** Inventory offset=%d\n",gs.inv.woffset);
            }
        } _GSP;

        case CP_CloseWindow:
        case SP_CloseWindow: {
            if (!gs.opt.track_inventory) break;
            SP_CloseWindow_pkt *tpkt = &pkt->_SP_CloseWindow;

            if (DEBUG_INVENTORY) {
                printf("*** Close Window (%s), wid=%d\n",
                       pkt->cl?"Client":"Server", tpkt->wid);
                if (gs.inv.drag.item!=-1) {
                    printf("  discarding dragslot: ");
                    dump_slot(&gs.inv.drag);
                    printf("\n");
                }
            }

            // discard anything in dragslot - if you happen to drag
            // some items when the window is closed, they will be thrown
            gs.inv.drag.item = -1;
            prune_slot(&gs.inv.drag);

            // discard the crafting and the product slot
            int i;
            for(i=0; i<5; i++) {
                if (DEBUG_INVENTORY && gs.inv.slots[i].item!=-1) {
                    printf("  discarding crafting slot %d: ",i);
                    dump_slot(&gs.inv.slots[i]);
                    printf("\n");
                }
                gs.inv.slots[i].item = -1;
                prune_slot(&gs.inv.slots[i]);
            }

            gs.inv.windowopen = 0;
            gs.inv.wid = 0;
            gs.inv.woffset = 0;

            break;
        }

        GSP(SP_WindowItems) {
            // Ignore this update if the window was not opened yet
            // This happens when accessing villagers - it sends me
            // SP_WindowItems first, and then SP_OpenWindow
            if (tpkt->wid != gs.inv.wid) break;

            int woffset = (tpkt->wid == 0) ? 0 : gs.inv.woffset;
            int ioffset = (tpkt->wid == 0) ? 0 : 9;
            int nslots  = (tpkt->wid == 0) ? 45 : 36;

            if (DEBUG_INVENTORY)
                printf("*** WindowItems, woffset=%d, ioffset=%d, nslots=%d\n",
                       woffset, ioffset, nslots);

            int i;
            for(i=0; i<nslots; i++) {
                slot_t * islot = &gs.inv.slots[i+ioffset];
                slot_t * wslot = &tpkt->slots[i+woffset];

                if (DEBUG_INVENTORY) {
                    printf("  %2d => %2d : ",i+woffset,i+ioffset);
                    dump_slot(wslot);
                    printf("\n");
                }

                clear_slot(islot);
                clone_slot(wslot, islot);
            }
        } _GSP;
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
    gs.inv.windowopen = 0;

    gs_used = 1;
}

void gs_destroy() {
    int i;

    // delete tracked entities
    for(i=0; i<C(gs.entity); i++)
        free_metadata(P(gs.entity)[i].mdata);
    lh_free(P(gs.entity));

    for(i=0; i<45; i++)
        clear_slot(&gs.inv.slots[i]);

    //dump_chunks(&gs.overworld);

    free_chunks(&gs.overworld);
    free_chunks(&gs.nether);
    free_chunks(&gs.end);
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
        case GSOP_TRACK_INVENTORY:
            gs.opt.track_inventory = value;
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
        case GSOP_TRACK_INVENTORY:
            return gs.opt.track_entities;

        default:
            LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }
}
