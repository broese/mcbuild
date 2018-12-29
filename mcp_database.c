/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include "mcp_database.h"
#include <json-c/json.h>
#include <string.h>
#include "lh_buffers.h"
#include "lh_files.h"


const database_t load_database(int protocol_id) {
    database_t db;
    
    //Deal with multiple protocols later
    assert(protocol_id == 404);

    if (db.initialized) {
        return db;
    }

    //initialize the database
    db.protocol = protocol_id;
    db.itemcount = 0;
    db.blockcount = 0;
    db.initialized = 1;
    
    //location of the server.jar generated items.json
    char *jsonpath = "./database/items.json";

    //load the items.json into memory
    uint8_t *buf;
    ssize_t sz = lh_load_alloc(jsonpath, &buf);
    lh_resize(buf, sz+1);


    json_object *jobj, *itemidstructurejson, *itemidjson;
	
    //parse the items.json buffer into a json-c object
    jobj = json_tokener_parse(buf);

    //initialize an iterator. "it" is our iterator and "itEnd" is the end of the json where the iterations stop
    struct json_object_iterator it = json_object_iter_begin(jobj);
    struct json_object_iterator itEnd = json_object_iter_end(jobj);

    //loop through each record of the json 
    //note structure is { item_name : { "protocol_id" : value } } so our value is nested within another json
    while (!json_object_iter_equal(&it, &itEnd)) {

        // the name of the json record is the item_name itself
        const char* itemname = json_object_iter_peek_name(&it);

        // the value of this pair is another json so we need to go one level deeper for the item_id        
        itemidstructurejson = json_object_iter_peek_value(&it);

        // process the internal json, where we look for the key "protocol_id" and get its value (the item_id)
        json_object_object_get_ex(itemidstructurejson,"protocol_id", &itemidjson);

        // convert the value of protocol_id from an abstract object into an integer
        int itemid = json_object_get_int(itemidjson);

        // confirm that our item name begins with "minecraft:"
        assert(!strncmp(itemname,"minecraft:",10));

        //store the item name into our dbrecord using pointer arithmetic to get rid of the "minecraft:" 
        db.item[db.itemcount].name = itemname+10;

        //store the item_id into our dbrecord
        db.item[db.itemcount].id = itemid;

        // keep track of the element of the array we are on, and the final count will be useful when searching through the db
        db.itemcount++;

        // iterate through the json iterator
        json_object_iter_next(&it);
    }
    return db;
}

int get_item_id(database_t *db, const char *name) {
    int id = -1;
    for (int i =0; i < db->itemcount; i++) {
        if (!strcmp(db->item[i].name, name)) {
            id = db->item[i].id;
            break;
        }
    }
    return id;
};

const char *get_item_name_from_db(database_t *db, int item_id) { 
    char *buf;
    if (item_id == -1) {
        return "Empty";
    }
    if (item_id < -1 || item_id > db->itemcount) {
        return "ID out of bounds";
    }
    for (int i=0; i < db->itemcount; i++) {
        if (item_id == db->item[i].id) {
            buf = malloc(strlen(db->item[i].name)+1);
            strcpy(buf,db->item[i].name);
            return buf;
        }
    }
    return "ID not found";
};

