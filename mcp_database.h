/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "lh_arr.h"

///////////////////////////////////////////////////////////////
// constants

extern const int db_num_items;

///////////////////////////////////////////////////////////////
// block and item database

typedef uint16_t blid_t;

typedef struct {
  const char *name;
  int id;
}  item_t;

typedef struct {
  const char *pname;
  const char *pvalue;
}  prop_t;

typedef struct {
  const char *name;
  blid_t id;
  blid_t oldid;
  blid_t defaultid;
  lh_arr_declare(prop_t, prop);
}  block_t;

typedef struct {
  int protocol;
  lh_arr_declare(item_t, item);
  lh_arr_declare(block_t, block);
} database_t;

// Loads a db for this protocol into memory
int db_load(int protocol_id);

// Unloads all databases from memory
void db_unload();

// Gets the item id given the item name
int db_get_item_id(const char *name);

// Gets the item name given the item id
const char *db_get_item_name(int item_id);

// Gets the block's default id given a block name
//  db_get_blk_id("cobblestone") => 14
//  db_get_blk_id("nether_brick_stairs") => 4540 // north,bottom,straight,false marked as default
blid_t db_get_blk_id(const char *name);

// Gets the block name given the block id
const char *db_get_blk_name(blid_t id);

// Gets the blockname from the old-style block ID (only relevant for the SP_BlockAction packet)
const char *db_get_blk_name_from_old_id(blid_t oldid);

// input is another block id, returning that block id's default id
blid_t db_get_blk_default_id(blid_t id);

// true if this block should be excluded from scanning
int db_blk_is_noscan(blid_t blk_id);

// true if this block cannot be used for placement upon its faces
int db_blk_is_empty(blid_t blk_id);

// blocks that are onwall -- cannot use item flags -- the block & item names dont match
int db_blk_is_onwall(blid_t blk_id);

// Gets the block_id that matches another block_id, except for changing one property to a different value
blid_t db_blk_property_change(blid_t blk_id, const char* prop_name, const char* new_prop_value);

// takes a block_id and returns the id that matches it, except for the facing or axis property rotated in degrees
blid_t db_get_rotated_block(blid_t blk_id, int degrees);

// places all ids matching a set of propeties for the block name into array ids (can be assumed to be long enough) and returns the number of ids
int db_get_matching_block_ids(const char *name, prop_t *match, int propcount, blid_t *ids);

// Get all block ids matching a given blockname and places ids into an array returning the count
int db_get_all_ids_matching_blockname(const char *blockname, blid_t *idarray );

// Gets the property value of a property for this block id
//  db_get_blk_propval(14,"facing") => NULL // no such property
//  db_get_blk_propval(1650,"facing") => "north"
//  db_get_blk_propval(1686,"half") => "bottom"
const char *db_get_blk_propval(blid_t id, const char *propname);

// Gets the number of states a block has, given the block id
//  db_get_num_states(5) => 1 // polished_diorite
//  db_get_num_states(8) => 2 // grass_block
int db_get_num_states(blid_t block_id);

// Dumps blocks array to stdout
void db_dump_blocks(int maxlines);

// Dumps items array to stdout
void db_dump_items(int maxlines);

// Dumps blocks array to a csv file
int db_dump_blocks_to_csv_file();

// Dumps items array to a csv file
int db_dump_items_to_csv_file();

// Gets the stacksize given an item id
int db_stacksize (int item_id);

// True if item exists as item only
int db_item_is_itemonly (int item_id);

// True if item is a container (opens a dialog window)
int db_item_is_container (int item_id);

// True if item is placed on an axis (logs, wood, stripped, quartz & purper pillars, hay & bone)
int db_item_is_axis (int item_id);

// True if item is a slab
int db_item_is_slab (int item_id);

// True if item is a stair
int db_item_is_stair (int item_id);

// True if item is a door
int db_item_is_door (int item_id);

// True if item is a trapdoor
int db_item_is_tdoor (int item_id);

// True if item has the face property (buttons, lever, upcoming grindstone)
int db_item_is_face (int item_id);

// True if item is a bed
int db_item_is_bed (int item_id);

// True if item is a redstone device
int db_item_is_rsdev (int item_id);

// True if item is a chest or trapped chest (single/left/right orientable containers)
int db_item_is_chest (int item_id);

// True if item is a enderchest or furnace  (orientable containers)
int db_item_is_furnace (int item_id);

//TODO:
//struct prop_t { const char *pname, const char *pvalue };
// prop_t * defines a set of properties I'm interested in. It's a list no longer than 3 I guess
// how should we define the length? it could be {NULL,NULL} terminated or length explicitly given.
// many different IDs can match
//int get_matching_block_ids(database_t *db, const char *name, prop_t *match, int *ids);
// places all ids matching a set of propeties for the block name into array ids (can be assumed to be long enough) and returns the number of ids
