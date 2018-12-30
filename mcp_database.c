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

database_t db;

int get_default_blockid_from_block_state_json(json_object *blkstatearrjson) {
    json_object *blockstatejson, *blockdefaultidjson;
    int statesarrlen = json_object_array_length(blkstatearrjson);
    for (int i=0; i<statesarrlen; i++) {
        blockstatejson = json_object_array_get_idx(blkstatearrjson, i);
        json_object_object_get_ex(blockstatejson,"default", &blockdefaultidjson);        
        if(json_object_get_boolean(blockdefaultidjson)) {
            //found the default:true keypair so get the block id of this state
            json_object_object_get_ex(blockstatejson,"id", &blockdefaultidjson);
            int defaultid = json_object_get_int(blockdefaultidjson);
            return defaultid;
        }
    }
    return -1; //not found
}

const database_t *load_database(int protocol_id) {

    if (db.initialized) {
        return &db;
    }

    //Deal with multiple protocols later
    assert(protocol_id == 404);

    db.protocol = protocol_id;
    db.itemcount = 0;
    db.blockcount = 0;
    db.initialized = 1;

    //////////////////////////////////
    //  Load blocks.json into db    //
    //////////////////////////////////

    char *blockjsonfilepath = "./database/blocks.json";    //can become blocks_404.json 
    //load the blocks.json into memory
    uint8_t *bufblocks;
    ssize_t szblocks = lh_load_alloc(blockjsonfilepath, &bufblocks);
    lh_resize(bufblocks, szblocks+1);

    json_object *jblkmain, *blockstatejson, *blockstatesarrayjson, *blockidjson, *blockpropjson;
    jblkmain = json_tokener_parse(bufblocks);

    int namecount = 0;
    //loop through each block(name) which is the top level of the json
    json_object_object_foreach(jblkmain, keymain, valmain) {

        //valmain is more json with { Properties: ... , States: [ ... , ... ] } and we want the states.
        json_object_object_get_ex(valmain,"states", &blockstatesarrayjson);

        //get the default blockid for this blockname -- only in its own sub for clarity during programming
        int defaultid = get_default_blockid_from_block_state_json(blockstatesarrayjson);

        //how many states are there
        int numstates = json_object_array_length(blockstatesarrayjson);

        //loop through each state (blockid) of this blockname
        //each state's json contains { id: ...,  [default]:..., [properties]: {...,...} }
        for (int i=0; i<numstates; i++) {
            blockstatejson = json_object_array_get_idx(blockstatesarrayjson, i);

            //get the blockid
            json_object_object_get_ex(blockstatejson,"id", &blockidjson);
            int blkid = json_object_get_int(blockidjson);

            //get the properties if they exist
            json_object_object_get_ex(blockstatejson,"properties", &blockpropjson);

            int propcount = 0;
            if (json_object_get_type(blockpropjson) == json_type_object) { 
                //we got some properties -- loop through each property tp get the key : value
                json_object_object_foreach(blockpropjson, keyprop, valprop) {                       
                    // Property Name: allocate memory , copy into memory, and store in database
                    char* propertyname = malloc(strlen(keyprop)+1);
                    strcpy(propertyname, keyprop);
                    db.block[db.blockcount].prop[propcount].pname = propertyname;
                    // Property Value: allocate memory , copy into memory, and store in database
                    char* propertyval = malloc(strlen(json_object_get_string(valprop))+1);
                    strcpy(propertyval, json_object_get_string(valprop));
                    db.block[db.blockcount].prop[propcount].pvalue = propertyval;

                    propcount++;
                }
            }
            // confirm that blockname begins with "minecraft:"
            assert(!strncmp(keymain,"minecraft:",10));
            // pointer arithmetic to get rid of the minecraft:
            db.block[db.blockcount].name = keymain+10;
            db.block[db.blockcount].id = blkid;
            db.block[db.blockcount].oldid = namecount;
            db.block[db.blockcount].defaultid = defaultid;
            db.block[db.blockcount].propcount = propcount;
            db.blockcount++;
        }
    namecount++;
    }

    //////////////////////////////////
    //  Load items.json into db     //
    //////////////////////////////////

    //location of the server.jar generated items.json
    char *itemjsonfilepath = "./database/items.json";  //can become items_404.json

    //load the items.json into memory
    uint8_t *buf;
    ssize_t sz = lh_load_alloc(itemjsonfilepath, &buf);
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
    //printf("Database Initialized.\n");
    return &db;
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

const char *get_item_name_from_db(database_t *db, int item_id) {  //there's already a get_item_name()
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

const char * get_block_name(database_t *db, int id) {
    char *buf;
    if (id == -1) {
        return "Empty";
    }
    if (id < -1 || id > db->blockcount) {
        return "ID out of bounds";
    }
    for (int i=0; i < db->blockcount; i++) {
        if (id == db->block[i].id) {
            buf = malloc(strlen(db->block[i].name)+1);
            strcpy(buf,db->block[i].name);
            return buf;
        }
    }
    return "ID not found";
};

const char * get_block_name_from_old_id(database_t *db, int oldid) {
    char *buf;
    if (oldid == -1) {
        return "Empty";
    }
    if (oldid < -1 || oldid > db->block[db->blockcount-1].oldid) {
        return "ID out of bounds";
    }
    for (int i=0; i < db->blockcount; i++) {
        if (oldid == db->block[i].oldid) {
            buf = malloc(strlen(db->block[i].name)+1);
            strcpy(buf,db->block[i].name);
            return buf;
        }
    }
    return "ID not found";
};

int get_block_default_id(database_t *db, int id) {
    int defid = -1;
    for (int i=0; i < db->blockcount; i++) {
        if (id == db->block[i].id) {
            return db->block[i].defaultid;
        }
    }
    return defid;
}
void dump_db_blocks(database_t *db, int maxlines){
    printf("Dumping Blocks..\n");
    printf("%25s %05s %05s %05s %s\n","blockname","blkid","oldid","defid","#prop");
    for (int i=0; (i < db->blockcount) && (i < maxlines); i++) {
        printf("%25s ", db->block[i].name);
        printf("%05d ", db->block[i].id);
        printf("%05d ", db->block[i].oldid);
        printf("%05d ", db->block[i].defaultid);
        printf("%05d ", db->block[i].propcount);
        for (int j=0; j < db->block[i].propcount; j++) {
            printf("prop:%s val:%s, ", db->block[i].prop[j].pname, db->block[i].prop[j].pvalue);
        }
        printf("\n");
    }
}

const char * get_block_propval(database_t *db, int id, const char *propname);
// TODO
// (db,14,"facing") => NULL // no such property
// (db,1650,"facing") => "north"
// (db,1686,"half") => "bottom"
//struct prop_t { const char *pname, const char *pvalue };
// prop_t * defines a set of properties I'm interested in. It's a list no longer than 3 I guess
// how should we define the length? it could be {NULL,NULL} terminated or length explicitly given.
// many different IDs can match
//int get_matching_block_ids(database_t *db, const char *name, prop_t *match, int *ids);
// places all ids matching a set of propeties for the block name into array ids (can be assumed to be long enough) and returns the number of ids
