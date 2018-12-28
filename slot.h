/*
 Authors:
 Copyright 2012-2016 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

/*
  slot : data type representing inventory/window slots
*/

#include <stdint.h>

#include "nbt.h"

////////////////////////////////////////////////////////////////////////////////
// Slots and inventory

typedef struct {
    int         present;    // if true, slot is non-empty
    int32_t     item;       // item ID, set to -1 if present==0
    int16_t     count;      // actually int8_t, but we need to have a larger capacity to deal with crafting
    int16_t     damage;     // FIXME: legacy damage value, unused since 1.13
                            // left here in order to avoid conflicts in other modules
    nbt_t      *nbt;        // auxiliary data - enchantments etc.
} slot_t;

// print contents of a slot to stdout
void dump_slot(slot_t *s);

// clear slot, free any allocated data
void clear_slot(slot_t *s);

// ensure that an emptied slot is in a consistent state
void prune_slot(slot_t *s);

// make a copy of a slot, including deep-copied NBT
slot_t * clone_slot(slot_t *src, slot_t *dst);

// swap the contents of two slots
void swap_slots(slot_t *f, slot_t *t);

// read slot data from MC packet format
uint8_t * read_slot_legacy(uint8_t *p, slot_t *s);
uint8_t * read_slot(uint8_t *p, slot_t *s);

// write slot data to MC packet format
uint8_t * write_slot_legacy(uint8_t *w, slot_t *s);
uint8_t * write_slot(uint8_t *w, slot_t *s);
