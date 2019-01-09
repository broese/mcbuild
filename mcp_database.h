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
// block and item database

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
  int id;
  int oldid;
  int defaultid;
  lh_arr_declare(prop_t, prop);
}  block_t;

typedef struct {
  int protocol;
  lh_arr_declare(item_t, item);
  lh_arr_declare(block_t, block);
} database_t;

int db_load(int protocol_id);
void db_unload();
int db_get_item_id(const char *name);
const char *db_get_item_name(int item_id);
const char *db_get_blk_name(int id);

//db_get_blk_id(db, "cobblestone") => 14
//db_get_blk_id(db, "nether_brick_stairs") => 4540 // north,bottom,straight,false marked as default
int db_get_blk_id(const char *name); //input is a block name, returning that blockname's default id

//input is another block id, returning that block id's default id
int db_get_blk_default_id(int id);

// true if this block should be excluded from scanning
int db_blk_is_noscan(int blk_id);

// true if this block cannot be used for placement upon its faces
int db_blk_is_empty(int blk_id);

const char * db_get_blk_propval(int id, const char *propname);

//db_get_num_states(db,5) => 1 // polished_diorite
//db_get_num_states(db,8) => 2 // grass_block
int db_get_num_states(int block_id);

void db_dump_blocks(int maxlines);
void db_dump_items(int maxlines);
int db_dump_blocks_to_csv_file();
int db_dump_items_to_csv_file();
// (db,14,"facing") => NULL // no such property
// (db,1650,"facing") => "north"
// (db,1686,"half") => "bottom"
//struct prop_t { const char *pname, const char *pvalue };
// prop_t * defines a set of properties I'm interested in. It's a list no longer than 3 I guess
// how should we define the length? it could be {NULL,NULL} terminated or length explicitly given.
// many different IDs can match
//int get_matching_block_ids(database_t *db, const char *name, prop_t *match, int *ids);
// places all ids matching a set of propeties for the block name into array ids (can be assumed to be long enough) and returns the number of ids

//returns the stacksize
int db_stacksize (int item_id);

// True if item exists as item only
int db_item_is_itemonly (int item_id);

// Gets the blockname from the old-style block ID
// only relevant for the SP_BlockAction packet which uses old ids
const char *db_get_blk_name_from_old_id(int oldid);
