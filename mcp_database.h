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
  const char *name;
  //TODO: Properties
}  block_t;

typedef struct {
  int protocol;
  int itemcount;
  int blockcount;
  item_t item[1000];
  block_t block[10000];
} database_t;

const database_t load_database(int protocol_id);
int get_item_id(database_t *db, const char *name);
const char *get_item_name_from_db(database_t *db, int item_id);
