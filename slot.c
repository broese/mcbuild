/*
 Authors:
 Copyright 2012-2016 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

/*
  slot : data type representing inventory/window slots
*/

#include <stdio.h>

#define LH_DECLARE_SHORT_NAMES 1
#include <lh_buffers.h>
#include <lh_bytes.h>

#include "slot.h"
#include "mcp_ids.h"


////////////////////////////////////////////////////////////////////////////////
// Window slots

// print contents of a slot to stdout
void dump_slot(slot_t *s) {
    char buf[256];
    printf("item=%d (%s)", s->item, get_item_name(buf,s));
    if (s->item != -1) {
        printf(", count=%d, damage=%d", s->count, s->damage);
        if (s->nbt) {
            printf(", nbt=available:\n");
            //nbt_dump(s->nbt);
        }
    }
}

// clear slot, free any allocated data
void clear_slot(slot_t *s) {
    if (s->nbt) {
        nbt_free(s->nbt);
        s->nbt = NULL;
    }
    s->item = -1;
    s->count = 0;
    s->damage = 0;
}

// ensure that an emptied slot is in a consistent state
void prune_slot(slot_t *s) {
    if (s->count <= 0 || s->item == -1)
        clear_slot(s);
}

// make a copy of a slot, including deep-copied NBT
slot_t * clone_slot(slot_t *src, slot_t *dst) {
    if (!dst)
        lh_alloc_obj(dst);

    clear_slot(dst);

    dst->item = src->item;
    dst->count = src->count;
    dst->damage = src->damage;
    dst->nbt = nbt_clone(src->nbt);

    return dst;
}

// swap the contents of two slots
void swap_slots(slot_t *f, slot_t *t) {
    slot_t temp;
    temp = *t;
    *t = *f;
    *f = temp;
}

// read slot data from MC packet format
uint8_t * read_slot(uint8_t *p, slot_t *s) {
    s->item = lh_read_short_be(p);

    if (s->item != -1) {
        s->count  = lh_read_char(p);
        s->damage = lh_read_short_be(p);
        s->nbt    = nbt_parse(&p);
    }
    else {
        s->count = 0;
        s->damage= 0;
        s->nbt = NULL;
    }
    return p;
}

// write slot data to MC packet format
uint8_t * write_slot(uint8_t *w, slot_t *s) {
    lh_write_short_be(w, s->item);
    if (s->item == -1) return w;

    lh_write_char(w, s->count);
    lh_write_short_be(w, s->damage);

    nbt_write(&w, s->nbt);

    return w;
}
