/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <stdint.h>

#include "mcp_types.h"
#include "slot.h"
#include "mcp_database.h"

////////////////////////////////////////////////////////////////////////////////
// Block type flags

// bitmasks of the meta bits used as a state (i.e. affected by the game
// but not directly adjustable by the player)
#define I_STATE_MASK  15

#define I_STATE_1 1
#define I_STATE_4 4
#define I_STATE_8 8
#define I_STATE_C 12
#define I_STATE_F 15

// the ID is an item, i.e. cannot be placed as a block
#define I_ITEM   (1<<4)

// Item does not stack (or stack size=1)
#define I_NSTACK (1<<5)

// item stacks only by 16 (e.g. enderpearls)
#define I_S16    (1<<6)

// (blocks or inventory) the metadata defines the subtype like type, color or
// material. The subtype names are available in mname array
#define I_MTYPE  (1<<7)

// the block type normally has a block entity data attached to it, e.g. signs
#define I_BENTITY (1<<8)

// blocks that are completely opaque - i.e. fill out the entire block
// and do not have transparent areas
#define I_OPAQUE (1<<9)

// edible (and preferred) foods
#define I_FOOD (1<<10)

// metadata defines the placement orientation
#define I_MPOS   (1<<11)

// Container blocks (chests, furnaces, hoppers, etc. - dialog opens if right-clicked)
#define I_CONT   (1<<12)

// Blocks with adjustable setting (through right-click)
#define I_ADJ    (1<<13)

// slab-type block - I_MPOS lower/upper placement in the meta bit 3
#define I_SLAB   (1<<16)

// stair-type block - I_MPOS straight/upside-down in the meta bit 2, direction in bits 0-1
#define I_STAIR (1<<17)

// wood log type blocks
#define I_LOG (1<<18)

// torches and redstone torches
#define I_TORCH (1<<19)

// ladders, wall signs and wall banners
#define I_ONWALL (1<<20)

// double-slabs
#define I_DSLAB (1<<21)

// redstone switches
#define I_RSRC (1<<22)

// redstone devices (hoppers, dispensers, droppers, pistons)
#define I_RSDEV (1<<23)

// doors
#define I_DOOR (1<<24)

// trapdoors
#define I_TDOOR (1<<25)

// crops (planted on farmland)
#define I_PLANT (1<<26)

// oriented containers - chests and furnaces
#define I_CHEST (1<<27)

// fence gates
#define I_GATE (1<<28)

// observer
#define I_OBSERVER (1<<29)

// glazed terracota
#define I_TERRACOTA (1<<30)

// macros to determine armor type
#define I_HELMET(id)     ((id)==0x12a || (id)==0x12e || (id)==0x132 || (id)==0x136 || (id)==0x13a)
#define I_CHESTPLATE(id) ((id)==0x12b || (id)==0x12f || (id)==0x133 || (id)==0x137 || (id)==0x13b)
#define I_LEGGINGS(id)   ((id)==0x12c || (id)==0x130 || (id)==0x134 || (id)==0x138 || (id)==0x13c)
#define I_BOOTS(id)      ((id)==0x12d || (id)==0x131 || (id)==0x135 || (id)==0x139 || (id)==0x13d)
#define I_ELYTRA(id)     ((id)==0x1bb)
#define I_ARMOR(id)      (I_HELMET(id) || I_CHESTPLATE(id) || I_LEGGINGS(id) || I_BOOTS(id) || I_ELYTRA(id))

typedef struct _item_id {
    const char * name;
    uint64_t     flags;
    const char * mname[16];
} item_id;

extern const item_id ITEMS[];

#define BLOCKTYPE(b,m) (bid_t){ { { .bid = b, .meta = m } } }

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    const char *name;
    uint32_t    color;
} biome_id;

extern const biome_id BIOMES[];

////////////////////////////////////////////////////////////////////////////////

#include "mcp_types.h"

int get_base_material(blid_t mat);
uint8_t get_state_mask(int bid);
bid_t get_base_block_material(bid_t mat);

bid_t rotate_meta(bid_t b, int times);
int numrot(int from_dir, int to_dir);
bid_t flip_meta(bid_t b, char mode);
bid_t flip_meta_y(bid_t b);
