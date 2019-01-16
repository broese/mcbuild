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
#include "mcp_database.h"


////////////////////////////////////////////////////////////////////////////////
// Window slots

// print contents of a slot to stdout
void dump_slot(slot_t *s) {
    char buf[256];
    if (s->present) {
        printf("item=%d (%s) count=%d", s->item, db_get_item_name(s->item), s->count);
        if (s->nbt) {
            printf(", nbt=available");
            // printf(":\n"); nbt_dump(s->nbt);
        }
    }
    else {
        printf("empty");
    }
}

// clear slot, free any allocated data
void clear_slot(slot_t *s) {
    if (s->nbt) nbt_free(s->nbt);
    lh_clear_ptr(s);
    s->item = -1;
}

// ensure that an emptied slot is in a consistent state
void prune_slot(slot_t *s) {
    if (!s->present || s->count <= 0 || s->item == -1)
        clear_slot(s);
}

// make a copy of a slot, including deep-copied NBT
slot_t * clone_slot(slot_t *src, slot_t *dst) {
    if (!dst) lh_alloc_obj(dst);
    clear_slot(dst);
    *dst = *src;
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

// read slot data from MC packet format (v1.13 and above)
uint8_t * read_slot(uint8_t *p, slot_t *s) {
    clear_slot(s);
    s->present = lh_read_char(p);
    if (s->present) {
        s->item     = lh_read_varint(p);
        s->count    = lh_read_char(p);
        s->nbt      = nbt_parse(&p);
    }
    return p;
}

// write slot data to MC packet format (v1.13 and above)
uint8_t * write_slot(uint8_t *w, slot_t *s) {
    lh_write_char(w, s->present);
    if (s->present) {
        lh_write_varint(w, s->item);
        lh_write_char(w, s->count);
        nbt_write(&w, s->nbt);
    }
    return w;
}


