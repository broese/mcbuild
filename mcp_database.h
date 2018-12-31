/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

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
  int propcount;
  prop_t prop[7];
}  block_t;

typedef struct {
  int protocol;
  int initialized;
  int itemcount;
  int blockcount;
  item_t item[1000];
  block_t block[9000];
} database_t;

const database_t *load_database(int protocol_id);
int get_item_id(database_t *db, const char *name);
const char *get_item_name_from_db(database_t *db, int item_id);
const char * get_block_name(database_t *db, int id);
int get_block_default_id(database_t *db, int id);
const char * get_block_propval(database_t *db, int id, const char *propname);
void dump_db_blocks(database_t *db, int maxlines);
void dump_db_items(database_t *db, int maxlines);
int dump_db_blocks_to_csv_file(database_t *db);
int dump_db_items_to_csv_file(database_t *db);
int save_db_to_file(database_t *db);
// (db,14,"facing") => NULL // no such property
// (db,1650,"facing") => "north"
// (db,1686,"half") => "bottom"
//struct prop_t { const char *pname, const char *pvalue };
// prop_t * defines a set of properties I'm interested in. It's a list no longer than 3 I guess
// how should we define the length? it could be {NULL,NULL} terminated or length explicitly given.
// many different IDs can match
//int get_matching_block_ids(database_t *db, const char *name, prop_t *match, int *ids);
// places all ids matching a set of propeties for the block name into array ids (can be assumed to be long enough) and returns the number of ids