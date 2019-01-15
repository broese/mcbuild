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
#include <curl/curl.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include "lh_buffers.h"
#include "lh_files.h"
#include "lh_dir.h"

#define TESTEXAMPLES //show test examples after loading db

char *databasefilepath = "./database";

lh_arr_declare(database_t,dbs);

database_t *activedb;  //pointer to the db for the current connection's protocol

//forward declaration
int try_to_get_missing_json_files(int protocol_id);
int save_db_to_file(database_t *db);
int load_db_from_file(database_t *db, FILE* fp);
int test_examples();

// Loads a db for this protocol into memory
int db_load(int protocol_id) {
    activedb = NULL;

    //check if the db for this protocol is already loaded
    for (int i=0; i<C(dbs); i++) {
        if ( P(dbs)[i].protocol == protocol_id ) {
            printf("Database %d already loaded, setting the activedb pointer.\n",i);
            activedb = &P(dbs)[i];
            #ifdef TESTEXAMPLES
                test_examples();
            #endif
            return 0; //success
        }
    }

    //need a new database in the array of databases
    database_t *newdb = lh_arr_new_c(GAR(dbs));

    //TODO: Protocol Specific
    assert(protocol_id == 404);

    char *blockjsonfilespec = malloc(strlen(databasefilepath)+20);
    sprintf(blockjsonfilespec, "%s/blocks_%d.json",databasefilepath,protocol_id);  //example "./database/blocks_404.json"

    char *itemjsonfilespec = malloc(strlen(databasefilepath)+20);
    sprintf(itemjsonfilespec, "%s/items_%d.json",databasefilepath,protocol_id);  //example: "./database/items_404.json"

    char *dbfilespec = malloc(strlen(databasefilepath)+20);
    sprintf(dbfilespec, "%s/mcb_db_%d.txt",databasefilepath,protocol_id);  //example: "./database/mcb_db_404.txt"

    //First Method: if the db file already exists, load it.
    FILE *dbfile = fopen(dbfilespec, "r");
    if (dbfile) {
        int rc = load_db_from_file(newdb, dbfile);
        fclose(dbfile);
        if (!rc) {
            printf("Database successfully loaded from %s\n",dbfilespec);
            activedb = newdb;
            #ifdef TESTEXAMPLES
                test_examples();
            #endif
            return 0; //success
        }
        else {
            //unload failed attempt move to method 2 or 3
            //db_unload();
        }
    }

    //Second Method: Check for the proper blocks.json and items.json in the database directory
    FILE *f1 = fopen(blockjsonfilespec, "r");
    FILE *f2 = fopen(itemjsonfilespec, "r");
    //TODO: Logic error leaves a file open if only 1 of the files exists
    if ( f1 && f2 ) {
        fclose(f1);
        fclose(f2);
        printf("Loading %s and %s\n",blockjsonfilespec,itemjsonfilespec);
    }
    else {
        printf("%s and/or %s not found or available.\n",blockjsonfilespec,itemjsonfilespec);
        //Third Method: Attempt to download server.jar and extract them from it
        int rc = try_to_get_missing_json_files(protocol_id);
        if (rc) {
            printf("Error couldnt load database\n");
            return -1; //return code - failure
        }
        //Now the files are there
    }

    newdb->protocol = protocol_id;

    //////////////////////////////////
    //  Load blocks.json into db    //
    //////////////////////////////////

    //load the blocks.json file into a parsed json object in memory
    json_object *jblkmain, *blockstatejson, *blockstatesarrayjson, *blockidjson, *blockpropjson, *blockdefaultidjson;
    jblkmain = json_object_from_file (blockjsonfilespec);

    int namecount = 0;
    //loop through each block(name) which is the top level of the json
    json_object_object_foreach(jblkmain, keymain, valmain) {

        //valmain is more json with { properties: ... , states: [ ... , ... ] } and we want the states.
        json_object_object_get_ex(valmain,"states", &blockstatesarrayjson);

        //first get the default blockid for this blockname -- needed when populating the database in the second loop below
        int defaultid = -1;
        //get the number of states (blockids) for this blockname
        int statesarrlen = json_object_array_length(blockstatesarrayjson);
        //loop through each blockid looking for which one is the default
        for (int i=0; i<statesarrlen; i++) {
            blockstatejson = json_object_array_get_idx(blockstatesarrayjson, i);
            //look for the default:true keypair
            json_object_object_get_ex(blockstatejson,"default", &blockdefaultidjson);
            if(json_object_get_boolean(blockdefaultidjson)) {
                //found the default:true keypair so get the block id of this state and save it as the default id
                json_object_object_get_ex(blockstatejson,"id", &blockdefaultidjson);
                defaultid = json_object_get_int(blockdefaultidjson);
                break;
            }
        }
        //now that we have the default id for this blockname, loop through every state again to populate the database
        //each state's json contains { id: ...,  [default]:..., [properties]: {...,...} }
        for (int i=0; i<statesarrlen; i++) {
            //allocate a new block in the db array
            block_t *blk = lh_arr_new_c(GAR(newdb->block));

            //get this block state's json
            blockstatejson = json_object_array_get_idx(blockstatesarrayjson, i);

            //get the block id within this state
            json_object_object_get_ex(blockstatejson,"id", &blockidjson);
            int blkid = json_object_get_int(blockidjson);

            //get the properties if they exist
            json_object_object_get_ex(blockstatejson,"properties", &blockpropjson);

            if (json_object_get_type(blockpropjson) == json_type_object) {
                //this block has properties -- loop through each property tp get the key : value
                json_object_object_foreach(blockpropjson, keyprop, valprop) {
                    //allocate a new property array element within this block record
                    prop_t *prp = lh_arr_new_c(GAR(blk->prop));

                    // Property Name: allocate memory , copy into memory, and store in database
                    char* propertyname = malloc(strlen(keyprop)+1);
                    strcpy(propertyname, keyprop);
                    prp->pname = propertyname;

                    // Property Value: allocate memory , copy into memory, and store in database
                    char* propertyval = malloc(strlen(json_object_get_string(valprop))+1);
                    strcpy(propertyval, json_object_get_string(valprop));
                    prp->pvalue = propertyval;
                }
            }
            // confirm that blockname begins with "minecraft:"
            assert(!strncmp(keymain,"minecraft:",10));
            // pointer arithmetic to get rid of the minecraft:
            blk->name = keymain+10;
            blk->id = blkid;
            blk->oldid = namecount;
            blk->defaultid = defaultid;
        }
    namecount++;
    }

    //////////////////////////////////
    //  Load items.json into db     //
    //////////////////////////////////

    json_object *jobj, *itemidstructurejson, *itemidjson;

    //load the items.json file into a parsed json object in memory
    jobj = json_object_from_file (itemjsonfilespec);

     //initialize an iterator. "it" is the iterator and "itEnd" is the end of the json where the iterations stop
    struct json_object_iterator it = json_object_iter_begin(jobj);
    struct json_object_iterator itEnd = json_object_iter_end(jobj);

    //loop through each record of the json
    //note structure is { item_name : { "protocol_id" : value } } so the value is nested within another json
    while (!json_object_iter_equal(&it, &itEnd)) {

        // the name of the json record is the item_name itself
        const char* itemname = json_object_iter_peek_name(&it);

        // the value of this pair is another json so go one level deeper for the item_id
        itemidstructurejson = json_object_iter_peek_value(&it);

        // process the internal json, look for the key "protocol_id" and get its value (the item_id)
        json_object_object_get_ex(itemidstructurejson,"protocol_id", &itemidjson);

        // convert the value of protocol_id from an abstract object into an integer
        int itemid = json_object_get_int(itemidjson);

        // confirm that item name begins with "minecraft:"
        assert(!strncmp(itemname,"minecraft:",10));

        //create another element in the item array
        item_t *itm = lh_arr_new_c(GAR(newdb->item));

        //store the item name into dbrecord using pointer arithmetic to get rid of the "minecraft:"
        itm->name = itemname + 10;

        //store the item_id into dbrecord
        itm->id = itemid;

        // iterate through the json iterator
        json_object_iter_next(&it);
    }
    save_db_to_file(newdb);
    activedb = newdb;
    #ifdef TESTEXAMPLES
        test_examples();
    #endif

    return 0; //success
}

// Gets the item id given the item name
int db_get_item_id(const char *name) {
    assert(activedb);
    int id = -1;
    for (int i =0; i < C(activedb->item); i++) {
        if (!strcmp(activedb->P(item)[i].name, name)) {
            id = activedb->P(item)[i].id;
            break;
        }
    }
    //TODO: if this is a blockname with a different itemname for the base material
    return id;
};

// Gets the item name given the item id
const char *db_get_item_name(int item_id) {
    assert (activedb);
    char *buf;
    if (item_id == -1) {
        return "Empty";
    }
    if (item_id < -1 || item_id >= C(activedb->item)) {
        return "ID out of bounds";
    }
    for (int i=0; i < C(activedb->item); i++) {
        if (item_id == activedb->P(item)[i].id) {
            buf = malloc(strlen(activedb->P(item)[i].name)+1);
            strcpy(buf,activedb->P(item)[i].name);
            return buf;
        }
    }
    return "ID not found";
};

// Gets the block name given the block id
const char *db_get_blk_name(blid_t id) {
    assert(activedb);
    char *buf;
    if (id >= C(activedb->block)) {
        return "ID out of bounds";
    }
    for (int i=0; i < C(activedb->block); i++) {
        if (id == P(activedb->block)[i].id) {
            buf = malloc(strlen( P(activedb->block)[i].name)+1);
            strcpy(buf, P(activedb->block)[i].name);
            return buf;
        }
    }
    return "ID not found";
};

// Gets the blockname from the old-style block ID (only relevant for the SP_BlockAction packet)
const char *db_get_blk_name_from_old_id(blid_t oldid) {
    assert(activedb);
    char *buf;
    if (oldid > P(activedb->block)[C(activedb->block)-1].oldid) {
        return "ID out of bounds";
    }
    for (int i=0; i < C(activedb->block); i++) {
        if (oldid == P(activedb->block)[i].oldid) {
            buf = malloc(strlen(P(activedb->block)[i].name)+1);
            strcpy(buf,P(activedb->block)[i].name);
            return buf;
        }
    }
    return "ID not found";
};

// Gets the block's default id given a block name
//  db_get_blk_id("cobblestone") => 14
//  db_get_blk_id("nether_brick_stairs") => 4540 // north,bottom,straight,false marked as default
blid_t db_get_blk_id(const char *name) {
    assert (activedb);
    for (int i =0; i < C(activedb->block); i++) {
        if (!strcmp(activedb->P(block)[i].name, name)) {
            return activedb->P(block)[i].defaultid;
        }
    }
    return UINT16_MAX;
}

// input is another block id, returning that block id's default id
blid_t db_get_blk_default_id(blid_t id) {
    assert (activedb);
    for (int i=0; i < C(activedb->block); i++) {
        if (id == P(activedb->block)[i].id) {
            return P(activedb->block)[i].defaultid;
        }
    }
    return UINT16_MAX;
}

// Gets the number of states a block has, given the block id
//  db_get_num_states(5) => 1 // polished_diorite
//  db_get_num_states(8) => 2 // grass_block
int db_get_num_states(blid_t block_id) {
    assert (activedb);
    int count = 0;
    //we could use defaultid or blockname since they are 1-1 correspondence
    int defid = db_get_blk_default_id(block_id);
    for (int i=0; i < C(activedb->block); i++) {
        if (P(activedb->block)[i].defaultid == defid) {
            count++;
        }
    }
    return count;
}

// Dumps blocks array to stdout
void db_dump_blocks(int maxlines){
    assert (activedb);
    printf("Dumping Blocks...\n");
    printf("%-30s %-5s %-5s %-5s %s\n","blockname","blkid","oldid","defid","#prop");
    for (int i=0; (i < C(activedb->block)) && (i < maxlines); i++) {
        printf("%30s ", P(activedb->block)[i].name);
        printf("%05u ", P(activedb->block)[i].id);
        printf("%05u ", P(activedb->block)[i].oldid);
        printf("%05u ", P(activedb->block)[i].defaultid);
        printf("%05zd ", P(activedb->block)[i].C(prop));
        for (int j=0; j < P(activedb->block)[i].C(prop); j++) {
            printf("prop:%s val:%s, ", P(activedb->block)[i].P(prop)[j].pname, P(activedb->block)[i].P(prop)[j].pvalue);
        }
        printf("\n");
    }
}

// Dumps items array to stdout
void db_dump_items(int maxlines){
    assert (activedb);
    printf("Dumping Items...\n");
    for (int i=0; (i < C(activedb->item)) && (i < maxlines) ; i++) {
        printf("%30s ", activedb->P(item)[i].name);
        printf("%05d ", activedb->P(item)[i].id);
        printf("\n");
    }
}

// Dumps blocks array to a csv file
int db_dump_blocks_to_csv_file() {
    assert (activedb);
    char *blockcsvfilespec = malloc(strlen(databasefilepath)+30);
    sprintf(blockcsvfilespec, "%s/mcb_db_%d_blocks.csv",databasefilepath,activedb->protocol);
    FILE *fp = fopen(blockcsvfilespec,"w");
    if  ( fp == NULL ) {
        printf("Can't open csv file\n");
        return -1;
    }
    fprintf(fp, "%s, %s, %s, %s, %s\n","blockname","blkid","oldid","defid","#prop");
    for (int i=0; i < C(activedb->block) ; i++) {
        fprintf(fp, "%s,", P(activedb->block)[i].name);
        fprintf(fp, "%u,", P(activedb->block)[i].id);
        fprintf(fp, "%u,", P(activedb->block)[i].oldid);
        fprintf(fp, "%u,", P(activedb->block)[i].defaultid);
        fprintf(fp, "%zd", P(activedb->block)[i].C(prop));
        for (int j=0; j < P(activedb->block)[i].C(prop); j++) {
            fprintf(fp, ",prop:%s val:%s", P(activedb->block)[i].P(prop)[j].pname, P(activedb->block)[i].P(prop)[j].pvalue);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}

// Dumps items array to a csv file
int db_dump_items_to_csv_file() {
    assert (activedb);
    char *itemcsvfilespec = malloc(strlen(databasefilepath)+30);
    sprintf(itemcsvfilespec, "%s/mcb_db_%d_items.csv",databasefilepath,activedb->protocol);
    FILE *fp = fopen(itemcsvfilespec,"w");
    if  ( fp == NULL ) {
        printf("Can't open csv file\n");
        return -1;
    }
    fprintf(fp, "%s,%s\n", "itemname","id");
    for (int i=0; i < C(activedb->item) ; i++) {
        fprintf(fp, "%s,", activedb->P(item)[i].name);
        fprintf(fp, "%d\n", activedb->P(item)[i].id);
    }
    fclose(fp);
    return 0;
}

// Private: Saves a database struct to file so it can be loaded in the future
int save_db_to_file(database_t *db) {
    char *dbfilespec = malloc(strlen(databasefilepath)+20);
    sprintf(dbfilespec, "%s/mcb_db_%d.txt",databasefilepath,db->protocol);  //example: "./database/mcb_db_404.txt"
    FILE *fp = fopen(dbfilespec,"w");
    if  ( fp == NULL ) {
        printf("Can't open txt file\n");
        return -1;
    }
    fprintf(fp, "%d\n", db->protocol);
    fprintf(fp, "%zd\n", C(db->item));
    fprintf(fp, "%zd\n", C(db->block));
    for (int i=0; i < C(db->item); i++) {
        fprintf(fp, "%d\n",db->P(item)[i].id);
        fprintf(fp, "%s\n",db->P(item)[i].name);
    }
    for (int i=0; i < C(db->block); i++) {
        fprintf(fp, "%s\n",P(db->block)[i].name);
        fprintf(fp, "%d\n",P(db->block)[i].id);
        fprintf(fp, "%d\n",P(db->block)[i].oldid);
        fprintf(fp, "%d\n",P(db->block)[i].defaultid);
        fprintf(fp, "%zd\n",P(db->block)[i].C(prop));
        for (int j=0; j < P(db->block)[i].C(prop); j++) {
            fprintf(fp, "%s\n",P(db->block)[i].P(prop)[j].pname);
            fprintf(fp, "%s\n",P(db->block)[i].P(prop)[j].pvalue);
        }
    }
    fclose(fp);
    return 0;
}

// Private: Loads a previously-saved db file created by save_db_to_file
int load_db_from_file(database_t *db, FILE* fp) {

    char buf[100];
    int itemcount, blockcount,propcount;

    fgets(buf,100,fp);
    db->protocol = atoi(buf);
    fgets(buf,100,fp);
    itemcount = atoi(buf);
    fgets(buf,100,fp);
    blockcount = atoi(buf);
    for (int i=0; i < itemcount; i++) {
        item_t *itm = lh_arr_new_c(GAR(db->item));
        fgets(buf,100,fp);
        itm->id = atoi(buf);
        fgets(buf,100,fp);
        buf[strcspn(buf, "\n")] = 0;
        char *itemname = malloc(strlen(buf)+1);
        strcpy(itemname,buf);
        itm->name = itemname;
    }
    for (int i=0; i < blockcount; i++) {
        block_t *blk = lh_arr_new_c(GAR(db->block));
        fgets(buf,100,fp);
        buf[strcspn(buf, "\n")] = 0;
        char *blockname = malloc(strlen(buf)+1);
        strcpy(blockname,buf);
        blk->name = blockname;
        fgets(buf,100,fp);
        blk->id = atoi(buf);
        fgets(buf,100,fp);
        blk->oldid = atoi(buf);
        fgets(buf,100,fp);
        blk->defaultid = atoi(buf);
        fgets(buf,100,fp);
        propcount = atoi(buf);
        for (int j=0; j < propcount; j++) {
            prop_t *prp = lh_arr_new_c(GAR(blk->prop));
            fgets(buf,100,fp);
            buf[strcspn(buf, "\n")] = 0;
            char *propname = malloc(strlen(buf)+1);
            strcpy(propname,buf);
            prp->pname = propname;
            fgets(buf,100,fp);
            buf[strcspn(buf, "\n")] = 0;
            char *propvalue = malloc(strlen(buf)+1);
            strcpy(propvalue,buf);
            prp->pvalue = propvalue;
        }
    }
    if (fgets(buf,100,fp)) {
        printf("Error loading db from file..too many lines\n");
        return -1;
    }
    return 0;
}

// Unloads all databases from memory
void db_unload() {
    activedb = NULL;
    int dbcount = C(dbs);
    printf("Database array contains %d array(s).\n",dbcount);
    //process each database
    for (int i=0; i < dbcount; i++) {
        //give this database a convenient name
        database_t db = P(dbs)[i];
        int itemcount = db.C(item);
        int blockcount = db.C(block);
        //loop through each item freeing its name
        for (int j=0; j< itemcount; j++) {
            free((char*)db.P(item)[j].name);
        }
        //loop through each block
        for (int k=0; k < blockcount; k++) {
            assert (k < blockcount);
            //give this block a convenient name
            block_t blk = db.P(block)[k];
            //loop its properties to free propname/val
            for (int l=0; l<blk.C(prop); l++) {
                free((char*)blk.P(prop)[l].pname);
                free((char*)blk.P(prop)[l].pvalue);
            }
            //free this blocks prop array
            lh_arr_free(GAR(blk.prop));
            //free this blocks name string
            free((char*)blk.name);
        }
        //now all blocks and items are cleared, so free the arrays
        lh_arr_free(GAR(db.item));
        lh_arr_free(GAR(db.block));
    }
    lh_arr_free(GAR(dbs));
    return;
}

// Gets the property value of a property for this block id
//  db_get_blk_propval(db,14,"facing") => NULL // no such property
//  db_get_blk_propval(db,1650,"facing") => "north"
//  db_get_blk_propval(db,1686,"half") => "bottom"
const char * db_get_blk_propval(blid_t id, const char *propname) {
    assert (activedb);
    if (id < 0 || id >= C(activedb->block)) {
        return NULL;
    }
    for (int i=0; i < C(activedb->block); i++) {
        if (id ==  P(activedb->block)[i].id) {
            for (int j=0; j < P(activedb->block)[i].C(prop); j++) {
                if (!strcmp(P(activedb->block)[i].P(prop)[j].pname, propname)) {
                    char *buf = malloc(strlen(P(activedb->block)[i].P(prop)[j].pvalue)+1);
                    strcpy(buf,P(activedb->block)[i].P(prop)[j].pvalue);
                    return buf;
                }
            }
            break;
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

// block types we should exclude from scanning
int db_blk_is_noscan(blid_t blk_id) {
    const char *blk_name = db_get_blk_name(blk_id);
    if (!strcmp(blk_name, "air")) return 1;
    if (!strcmp(blk_name, "water")) return 1;
    if (!strcmp(blk_name, "lava")) return 1;
    if (!strcmp(blk_name, "grass")) return 1;
    if (!strcmp(blk_name, "seagrass")) return 1;
    if (!strcmp(blk_name, "tall_seagrass")) return 1;
    if (!strcmp(blk_name, "fire")) return 1;
    if (!strcmp(blk_name, "snow")) return 1;
    if (!strcmp(blk_name, "nether_portal")) return 1;
    if (!strcmp(blk_name, "end_portal")) return 1;
    if (!strcmp(blk_name, "carrots")) return 1;
    if (!strcmp(blk_name, "potatoes")) return 1;
    if (!strcmp(blk_name, "beetroots")) return 1;
    return 0;
}

// block types that are considered 'empty' for the block placement
int db_blk_is_empty(blid_t blk_id) {
    const char *blk_name = db_get_blk_name(blk_id);
    if (!strcmp(blk_name, "air")) return 1;
    if (!strcmp(blk_name, "water")) return 1;
    if (!strcmp(blk_name, "lava")) return 1;
    if (!strcmp(blk_name, "grass")) return 1;
    if (!strcmp(blk_name, "seagrass")) return 1;
    if (!strcmp(blk_name, "tall_seagrass")) return 1;
    if (!strcmp(blk_name, "fire")) return 1;
    if (!strcmp(blk_name, "snow")) return 1;
    return 0;
}

// blocks that are onwall -- cannot use item flags -- the block & item names dont match
int db_blk_is_onwall(blid_t blk_id) {
    const char *blk_name = db_get_blk_name(blk_id);
    if (!strcmp(blk_name, "wall_torch")) return 1;
    if (!strcmp(blk_name, "wall_sign")) return 1;
    if (!strcmp(blk_name, "redstone_wall_torch")) return 1;
    if (!strcmp(blk_name, "skeleton_wall_skull")) return 1;
    if (!strcmp(blk_name, "wither_skeleton_wall_skull")) return 1;
    if (!strcmp(blk_name, "zombie_wall_head")) return 1;
    if (!strcmp(blk_name, "player_wall_head")) return 1;
    if (!strcmp(blk_name, "creeper_wall_head")) return 1;
    if (!strcmp(blk_name, "dragon_wall_head")) return 1;
    if (!strcmp(blk_name, "white_wall_banner")) return 1;
    if (!strcmp(blk_name, "orange_wall_banner")) return 1;
    if (!strcmp(blk_name, "magenta_wall_banner")) return 1;
    if (!strcmp(blk_name, "light_blue_wall_banner")) return 1;
    if (!strcmp(blk_name, "yellow_wall_banner")) return 1;
    if (!strcmp(blk_name, "lime_wall_banner")) return 1;
    if (!strcmp(blk_name, "pink_wall_banner")) return 1;
    if (!strcmp(blk_name, "gray_wall_banner")) return 1;
    if (!strcmp(blk_name, "light_gray_wall_banner")) return 1;
    if (!strcmp(blk_name, "cyan_wall_banner")) return 1;
    if (!strcmp(blk_name, "purple_wall_banner")) return 1;
    if (!strcmp(blk_name, "blue_wall_banner")) return 1;
    if (!strcmp(blk_name, "brown_wall_banner")) return 1;
    if (!strcmp(blk_name, "green_wall_banner")) return 1;
    if (!strcmp(blk_name, "red_wall_banner")) return 1;
    if (!strcmp(blk_name, "black_wall_banner")) return 1;
    if (!strcmp(blk_name, "dead_tube_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "dead_brain_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "dead_bubble_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "dead_fire_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "dead_horn_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "tube_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "brain_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "bubble_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "fire_coral_wall_fan")) return 1;
    if (!strcmp(blk_name, "horn_coral_wall_fan")) return 1;
    return 0;
}

// takes a block_id and returns the id that matches it, except for the facing property rotated in degrees
blid_t db_get_rotated_block(blid_t blk_id, int degrees) {
    assert (activedb);
    assert (degrees == 90 || degrees == 180 || degrees == 270);

    block_t blk; // will be used to get the full block record from blk_id

    const char *currentdirection = db_get_blk_propval(blk_id,"facing");
    const char *currentaxis = db_get_blk_propval(blk_id,"axis");

    if (currentdirection) {
        char *desireddirection;
        //TODO: something more elegant
        if (!strcmp(currentdirection, "north")) {
            if (degrees == 90) desireddirection = "east";
            if (degrees == 180) desireddirection = "south";
            if (degrees == 270) desireddirection = "west";
        }
        else if (!strcmp(currentdirection, "east")) {
            if (degrees == 90) desireddirection = "south";
            if (degrees == 180) desireddirection = "west";
            if (degrees == 270) desireddirection = "north";
        }
        else if (!strcmp(currentdirection, "south")) {
            if (degrees == 90) desireddirection = "west";
            if (degrees == 180) desireddirection = "north";
            if (degrees == 270) desireddirection = "east";
        }
        else if (!strcmp(currentdirection, "west")) {
            if (degrees == 90) desireddirection = "north";
            if (degrees == 180) desireddirection = "east";
            if (degrees == 270) desireddirection = "south";
        }
        else return blk_id;  //block is facing up or down

        //find the block record for the blk_id parameter
        for (int i=0; i < C(activedb->block); i++) {
            if (blk_id ==  P(activedb->block)[i].id) {
                blk = P(activedb->block)[i];
                break;
            }
        }
        //TODO: move this to a lookup function
        for (int i=0; i < C(activedb->block); i++) {
            if (!strcmp( P(activedb->block)[i].name, blk.name) ) {
                //found the same block name - loop all properties for a match
                int matchcount = 0;
                for (int j=0; j < P(activedb->block)[i].C(prop); j++) {
                    if (!strcmp(P(activedb->block)[i].P(prop)[j].pname, "facing")) {
                        if (!strcmp(P(activedb->block)[i].P(prop)[j].pvalue, desireddirection)) matchcount++;  //matched our desired facing
                    }
                    else {
                        if (!strcmp(P(activedb->block)[i].P(prop)[j].pvalue, blk.P(prop)[j].pvalue)) matchcount++; //matched the other property
                    }
                }
                if ( matchcount == blk.C(prop) ) return P(activedb->block)[i].id;
            }
        }
    }
    else if (currentaxis) {
        char *desiredaxis;
        if (!strcmp(currentaxis, "x")) {
            if (degrees == 90) desiredaxis = "z";
            if (degrees == 180) return blk_id;
            if (degrees == 270) desiredaxis = "z";
        }
        else if (!strcmp(currentaxis, "z")) {
            if (degrees == 90) desiredaxis = "x";
            if (degrees == 180) return blk_id;
            if (degrees == 270) desiredaxis = "x";
        }
        else return blk_id; //block in z axis

        //find the block record for the blk_id parameter
        for (int i=0; i < C(activedb->block); i++) {
            if (blk_id ==  P(activedb->block)[i].id) {
                blk = P(activedb->block)[i];
                break;
            }
        }
        //TODO: move this to a lookup function
        for (int i=0; i < C(activedb->block); i++) {
            if (!strcmp( P(activedb->block)[i].name, blk.name) ) {
                //found the same block name - loop all properties for a match
                int matchcount = 0;
                for (int j=0; j < P(activedb->block)[i].C(prop); j++) {
                    if (!strcmp(P(activedb->block)[i].P(prop)[j].pname, "axis")) {
                        if (!strcmp(P(activedb->block)[i].P(prop)[j].pvalue, desiredaxis)) matchcount++;//matched our desired axis
                    }
                    else {
                        if (!strcmp(P(activedb->block)[i].P(prop)[j].pvalue, blk.P(prop)[j].pvalue)) matchcount++; //matched the other property
                    }
                }
                if ( matchcount == blk.C(prop) ) return P(activedb->block)[i].id;
            }
        }
    }
    return blk_id;  //block doesnt rotate or have an axis (or slipped thru horrible logic above)
}

////////////////////////////////////////////////////////////////////////////////

// Private:  Converts the protocol ID to its MC version string
char *protocol_to_ver_string(int protid){
    // TODO: use what we already have
    if (protid == 404) {
        return "1.13.2";
    }
    return "unknown";
}

// Private:  Saves a URL to a local file
int download_url_into_file(const char *url, const char *filespec) {
    CURL *curl = curl_easy_init();
    // set header options
    FILE *pagefile = fopen(filespec, "wb");
    if (!pagefile) return -1;
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);  //set to 1 to turn off progress
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);
    int rc = curl_easy_perform(curl);
    fclose(pagefile);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return rc;
}

// Private:  Finds the directory of the Java VM executable
char *findjavapath() {
    char *javapath = NULL;

#ifdef __CYGWIN__
    //all windows minecraft installs come with java.exe
    //hope their program files is default directory for now
    //but need a way to programmatically get the path of Minecraft install & the jre version it ships with
    DIR *dp;
    //Default location for 64-bit Windows OS with Minecraft 13.2
    javapath = "/cygdrive/c/Progra~2/Minecraft/runtime/jre-x64/1.8.0_51/bin";
    dp = opendir(javapath);
    if (!dp) {
        printf("findjavapath(): Directory not found\n");
        return NULL;
    }
    closedir(dp);
#endif

#ifdef __linux__
    //linux often has JAVA_HOME ready to go
    char *javahome = getenv("JAVA_HOME");
    if (!javahome) {
        return NULL;
    }
    javapath = malloc(strlen(javahome)+5);
    sprintf(javapath,"%s/bin",javahome);
#endif

    return javapath;
}

// Private:  Gets the server jar from Mojang and extract blocks.json & items.json
int try_to_get_missing_json_files(int protocol_id) {
    int rc;

    // Download version manifest from Mojang
    char *MANIFESTURL = "https://launchermeta.mojang.com/mc/game/version_manifest.json";
    char *manifestfilespec = malloc(strlen(databasefilepath)+30);
    sprintf(manifestfilespec, "%s/version_manifest.json",databasefilepath);
    rc = download_url_into_file(MANIFESTURL,manifestfilespec);
    if (rc) {
        printf ("Error downloading version manifest. %d\n",rc);
        return -1;
    }

    char *verid = protocol_to_ver_string(protocol_id);
    json_object *jman,*jver,*j132,*jid,*jurl;
    const char *urlversioninfo;

    // Parse version manifest to get the versioninfo download url for this version
    jman = json_object_from_file (manifestfilespec);
    json_object_object_get_ex(jman, "versions", &jver);
    int numversions = json_object_array_length(jver);
    for (int i=0; i<numversions; i++) {
        jid = json_object_array_get_idx(jver, i);
        json_object_object_get_ex(jid,"id", &j132);
        if (!strcmp(verid,json_object_get_string(j132))) {
            json_object_object_get_ex(jid,"url", &jurl);
            urlversioninfo = json_object_get_string(jurl);
            break;
        };
    };
    if (urlversioninfo == NULL) {
        printf ("Error parsing version manifest.\n");
        return -1;
    }

    // Download versioninfo json from Mojang
    char *verinfofilespec = malloc(strlen(databasefilepath)+30);
    sprintf(verinfofilespec, "%s/versioninfo.json",databasefilepath);
    rc = download_url_into_file(urlversioninfo,verinfofilespec);
    if (rc) {
        printf ("Error downloading versioninfo. %d\n",rc);
        return -1;
    }

    json_object *jvinfo,*jdl,*jserv,*jservurl;
    const char *urlserverjar;

    // Parse versioninfo json to get the server.jar download url for this version
    jvinfo = json_object_from_file(verinfofilespec);
    json_object_object_get_ex(jvinfo, "downloads", &jdl);
    json_object_object_get_ex(jdl, "server", &jserv);
    json_object_object_get_ex(jserv, "url", &jservurl);
    urlserverjar = json_object_get_string(jservurl);
    if (urlserverjar == NULL) {
        printf ("Error parsing versioninfo.\n");
        return -1;
    }

    char serverfilespec[50];
    snprintf(serverfilespec, sizeof(serverfilespec), "%s/server_%s.jar", databasefilepath, verid);

    // Download server.jar from Mojang
    rc = download_url_into_file(urlserverjar,serverfilespec);
    if (rc) {
        printf ("Error downloading server.jar. rc=%d\n",rc);
        return -1;
    }

    // Find path to java
    char *javafilepath = findjavapath();
    if (!javafilepath) {
        printf ("Error finding Java path.\n");
        return -1;
    }

    // run the server.jar to extract items.json and blocks.json
    char *cmd;
    cmd = malloc(strlen(javafilepath)+100);
#ifdef __CYGWIN__
    sprintf(cmd,"cd database; %s/java.exe -cp server_%s.jar net.minecraft.data.Main --all",javafilepath,verid);
#endif
#ifdef __linux__
    sprintf(cmd,"cd database; %s/java -cp server_%s.jar net.minecraft.data.Main --all",javafilepath,verid);
#endif
    FILE *fp = popen(cmd,"r");
    if (fp == NULL) {
        printf("Error launching java\n");
        return -1;
    }
    char buf[200];
    while (fgets(buf, 200, fp) != NULL) {
        printf("%s", buf);   //watch output of the java command
    }
    rc = pclose(fp);
    if (rc) {
        printf("Error processing java command %d\n",rc);
        return -1;
    }

    //Move the generated items.json and blocks.json into ./database
    char *blockjsonfilespec = malloc(strlen(databasefilepath)+20);
    sprintf(blockjsonfilespec, "%s/blocks_%d.json",databasefilepath,protocol_id);  //example "./database/blocks_404.json"

    char *itemjsonfilespec = malloc(strlen(databasefilepath)+20);
    sprintf(itemjsonfilespec, "%s/items_%d.json",databasefilepath,protocol_id);  //example: "./database/items_404.json"

    rc = rename("./database/generated/reports/items.json", itemjsonfilespec);
    rc |= rename("./database/generated/reports/blocks.json", blockjsonfilespec);
    if (rc) {
        printf("Error couldnt move the items blocks json to the database directory. %d\n",rc);
        return -1;
    }

    //Cleanup
    rc = system("rm -rf ./database/generated");
    rc |= system("rm -rf ./database/logs");
    rc |= remove(verinfofilespec);
    rc |= remove(manifestfilespec);
    // rc |= remove(serverfilespec);     //if we want to remove the server.jar too
    if (rc) {
        printf("Warning: Couldnt cleanup. %d",rc);
    }

    return 0;
}

// Private:  Dumps database tests to stdout (activated by #define TESTEXAMPLES)
int test_examples() {
    printf("Number of blocks: %zd\n",C(activedb->block));
    printf("Number of items: %zd\n", C(activedb->item));
    printf(" db_get_blk_propval(1686,\"half\")       = %s (bottom)\n",db_get_blk_propval(1686,"half"));
    printf(" db_get_item_id(\"heart_of_the_sea\")    = %d (789)\n", db_get_item_id("heart_of_the_sea"));
    printf(" db_get_item_name(788)                 = %s (nautilus_shell)\n", db_get_item_name(788));
    printf(" db_get_blk_name(8596)                 = %s (structure_block)\n", db_get_blk_name(8596));
    printf(" db_get_blk_default_id(8596)           = %d (8595)\n",db_get_blk_default_id(8596));
    printf(" db_get_blk_name_from_old_id(597)      = %s (structure_block)\n", db_get_blk_name_from_old_id(597));
    printf(" db_get_blk_id(\"nether_brick_stairs\")  = %d (4540)\n",db_get_blk_id("nether_brick_stairs"));
    printf(" db_get_num_states(5)                  = %d (1)\n",db_get_num_states(5));
    printf(" db_get_num_states(8)                  = %d (2)\n",db_get_num_states(8));
    printf(" db_get_num_states(2913)               = %d (1296)\n",db_get_num_states(2913));
    printf(" db_stacksize(db_get_item_id(\"ender_pearl\")) = %d (16)\n",db_stacksize(db_get_item_id("ender_pearl")));
    printf(" db_item_is_itemonly(703)              = %d (False) //skeleton_skull\n",db_item_is_itemonly(703));
    printf(" db_item_is_container(262)             = %d (True) //dropper\n",db_item_is_container(262));
    printf(" db_item_is_axis(32)                   = %d (True) //oak_log\n",db_item_is_axis(32));
    printf(" db_item_is_door(460)                  = %d (True) //iron_door\n",db_item_is_door(460));
    printf(" db_item_is_tdoor(280)                 = %d (True) //iron_trapdoor\n",db_item_is_tdoor(280));
    printf(" db_item_is_face(158)                  = %d (True) //lever\n",db_item_is_face(158));
    printf(" db_item_is_bed(596)                   = %d (True) //white_bed\n",db_item_is_bed(596));
    printf(" db_item_is_rsdev(67)                  = %d (True) //dispenser\n",db_item_is_rsdev(67));
    printf(" db_blk_is_onwall(3270)                = %d (True) //wall_sign\n",db_blk_is_onwall(3270));
    printf(" db_item_is_chest(250)                 = %d (True) //trapped_chest\n",db_item_is_chest(250));
    printf(" db_item_is_furnace(231)               = %d (True) //ender_chest\n",db_item_is_furnace(231));
    printf(" db_get_rotated_block(1649, 270)       = %d (1689) //oak_stairs rotated\n", db_get_rotated_block(1649, 270));
    printf(" db_get_rotated_block(74, 90)          = %d (72) //oak_log rotated from z to x\n", db_get_rotated_block(74, 90));
    printf("Now testing errors \n");
    printf(" db_get_blk_name(8599)                 = %s (out of bounds)\n", db_get_blk_name(8599));
    printf(" db_get_blk_name(8600)                 = %s (out of bounds)\n", db_get_blk_name(8600));
    printf(" db_get_blk_id(\"gold_nugget\")          = %d (UINT16_MAX meaning not found)\n",db_get_blk_id("gold_nugget"));
    printf(" db_get_num_states(8599)               = %d (0 meaning problem)\n",db_get_num_states(8599));

    return 0;
}

///////////////////////////////////////////////////
// Item flags

// the ID is an item, i.e. cannot be placed as a block
#define I_ITEM   (1ULL<<4)

// Item does not stack (or stack size=1)
#define I_NSTACK (1ULL<<5)

// item stacks only by 16 (e.g. enderpearls)
#define I_S16    (1ULL<<6)

// Container blocks (chests, furnaces, hoppers, etc. - dialog opens if right-clicked)
#define I_CONT   (1ULL<<12)

// slab-type block - I_MPOS lower/upper placement in the meta bit 3
#define I_SLAB   (1ULL<<16)

// stair-type block - I_MPOS straight/upside-down in the meta bit 2, direction in bits 0-1
#define I_STAIR (1ULL<<17)

// blocks with an axis property - wood log type blocks
#define I_AXIS (1ULL<<18)

// redstone devices (hoppers, dispensers, droppers, pistons, observers)
#define I_RSDEV (1ULL<<23)

// doors
#define I_DOOR (1ULL<<24)

// trapdoors
#define I_TDOOR (1ULL<<25)

// chests and trapped chests - oriented and doubleable containers (type: left/right/single)
#define I_CHEST (1ULL<<27)

// buttons and lever
#define I_FACE (1ULL<<33)

// beds
#define I_BED (1ULL<<34)

// furnace and enderchest - oriented containers
#define I_FURNACE (1ULL<<35)

// example - placeholder should each armor type get its own designation
#define I_ARMOR 0ULL

const uint64_t item_flags[] = {
    [0] = 0,                               //air
    [1] = 0,                               //stone
    [2] = 0,                               //granite
    [3] = 0,                               //polished_granite
    [4] = 0,                               //diorite
    [5] = 0,                               //polished_diorite
    [6] = 0,                               //andesite
    [7] = 0,                               //polished_andesite
    [8] = 0,                               //grass_block
    [9] = 0,                               //dirt
    [10] = 0,                              //coarse_dirt
    [11] = 0,                              //podzol
    [12] = 0,                              //cobblestone
    [13] = 0,                              //oak_planks
    [14] = 0,                              //spruce_planks
    [15] = 0,                              //birch_planks
    [16] = 0,                              //jungle_planks
    [17] = 0,                              //acacia_planks
    [18] = 0,                              //dark_oak_planks
    [19] = 0,                              //oak_sapling
    [20] = 0,                              //spruce_sapling
    [21] = 0,                              //birch_sapling
    [22] = 0,                              //jungle_sapling
    [23] = 0,                              //acacia_sapling
    [24] = 0,                              //dark_oak_sapling
    [25] = 0,                              //bedrock
    [26] = 0,                              //sand
    [27] = 0,                              //red_sand
    [28] = 0,                              //gravel
    [29] = 0,                              //gold_ore
    [30] = 0,                              //iron_ore
    [31] = 0,                              //coal_ore
    [32] = I_AXIS,                         //oak_log
    [33] = I_AXIS,                         //spruce_log
    [34] = I_AXIS,                         //birch_log
    [35] = I_AXIS,                         //jungle_log
    [36] = I_AXIS,                         //acacia_log
    [37] = I_AXIS,                         //dark_oak_log
    [38] = I_AXIS,                         //stripped_oak_log
    [39] = I_AXIS,                         //stripped_spruce_log
    [40] = I_AXIS,                         //stripped_birch_log
    [41] = I_AXIS,                         //stripped_jungle_log
    [42] = I_AXIS,                         //stripped_acacia_log
    [43] = I_AXIS,                         //stripped_dark_oak_log
    [44] = I_AXIS,                         //stripped_oak_wood
    [45] = I_AXIS,                         //stripped_spruce_wood
    [46] = I_AXIS,                         //stripped_birch_wood
    [47] = I_AXIS,                         //stripped_jungle_wood
    [48] = I_AXIS,                         //stripped_acacia_wood
    [49] = I_AXIS,                         //stripped_dark_oak_wood
    [50] = I_AXIS,                         //oak_wood
    [51] = I_AXIS,                         //spruce_wood
    [52] = I_AXIS,                         //birch_wood
    [53] = I_AXIS,                         //jungle_wood
    [54] = I_AXIS,                         //acacia_wood
    [55] = I_AXIS,                         //dark_oak_wood
    [56] = 0,                              //oak_leaves
    [57] = 0,                              //spruce_leaves
    [58] = 0,                              //birch_leaves
    [59] = 0,                              //jungle_leaves
    [60] = 0,                              //acacia_leaves
    [61] = 0,                              //dark_oak_leaves
    [62] = 0,                              //sponge
    [63] = 0,                              //wet_sponge
    [64] = 0,                              //glass
    [65] = 0,                              //lapis_ore
    [66] = 0,                              //lapis_block
    [67] = I_CONT | I_RSDEV,               //dispenser
    [68] = 0,                              //sandstone
    [69] = 0,                              //chiseled_sandstone
    [70] = 0,                              //cut_sandstone
    [71] = 0,                              //note_block
    [72] = 0,                              //powered_rail
    [73] = 0,                              //detector_rail
    [74] = I_RSDEV,                        //sticky_piston
    [75] = 0,                              //cobweb
    [76] = 0,                              //grass
    [77] = 0,                              //fern
    [78] = 0,                              //dead_bush
    [79] = 0,                              //seagrass
    [80] = 0,                              //sea_pickle
    [81] = I_RSDEV,                        //piston
    [82] = 0,                              //white_wool
    [83] = 0,                              //orange_wool
    [84] = 0,                              //magenta_wool
    [85] = 0,                              //light_blue_wool
    [86] = 0,                              //yellow_wool
    [87] = 0,                              //lime_wool
    [88] = 0,                              //pink_wool
    [89] = 0,                              //gray_wool
    [90] = 0,                              //light_gray_wool
    [91] = 0,                              //cyan_wool
    [92] = 0,                              //purple_wool
    [93] = 0,                              //blue_wool
    [94] = 0,                              //brown_wool
    [95] = 0,                              //green_wool
    [96] = 0,                              //red_wool
    [97] = 0,                              //black_wool
    [98] = 0,                              //dandelion
    [99] = 0,                              //poppy
    [100] = 0,                             //blue_orchid
    [101] = 0,                             //allium
    [102] = 0,                             //azure_bluet
    [103] = 0,                             //red_tulip
    [104] = 0,                             //orange_tulip
    [105] = 0,                             //white_tulip
    [106] = 0,                             //pink_tulip
    [107] = 0,                             //oxeye_daisy
    [108] = 0,                             //brown_mushroom
    [109] = 0,                             //red_mushroom
    [110] = 0,                             //gold_block
    [111] = 0,                             //iron_block
    [112] = I_SLAB,                        //oak_slab
    [113] = I_SLAB,                        //spruce_slab
    [114] = I_SLAB,                        //birch_slab
    [115] = I_SLAB,                        //jungle_slab
    [116] = I_SLAB,                        //acacia_slab
    [117] = I_SLAB,                        //dark_oak_slab
    [118] = I_SLAB,                        //stone_slab
    [119] = I_SLAB,                        //sandstone_slab
    [120] = I_SLAB,                        //petrified_oak_slab
    [121] = I_SLAB,                        //cobblestone_slab
    [122] = I_SLAB,                        //brick_slab
    [123] = I_SLAB,                        //stone_brick_slab
    [124] = I_SLAB,                        //nether_brick_slab
    [125] = I_SLAB,                        //quartz_slab
    [126] = I_SLAB,                        //red_sandstone_slab
    [127] = I_SLAB,                        //purpur_slab
    [128] = I_SLAB,                        //prismarine_slab
    [129] = I_SLAB,                        //prismarine_brick_slab
    [130] = I_SLAB,                        //dark_prismarine_slab
    [131] = 0,                             //smooth_quartz
    [132] = 0,                             //smooth_red_sandstone
    [133] = 0,                             //smooth_sandstone
    [134] = 0,                             //smooth_stone
    [135] = 0,                             //bricks
    [136] = 0,                             //tnt
    [137] = 0,                             //bookshelf
    [138] = 0,                             //mossy_cobblestone
    [139] = 0,                             //obsidian
    [140] = 0,                             //torch
    [141] = 0,                             //end_rod
    [142] = 0,                             //chorus_plant
    [143] = 0,                             //chorus_flower
    [144] = 0,                             //purpur_block
    [145] = I_AXIS,                        //purpur_pillar
    [146] = I_STAIR,                       //purpur_stairs
    [147] = 0,                             //spawner
    [148] = I_STAIR,                       //oak_stairs
    [149] = I_CONT | I_CHEST,              //chest
    [150] = 0,                             //diamond_ore
    [151] = 0,                             //diamond_block
    [152] = I_CONT,                        //crafting_table
    [153] = 0,                             //farmland
    [154] = I_CONT | I_FURNACE,            //furnace
    [155] = 0,                             //ladder
    [156] = 0,                             //rail
    [157] = I_STAIR,                       //cobblestone_stairs
    [158] = I_FACE,                        //lever
    [159] = 0,                             //stone_pressure_plate
    [160] = 0,                             //oak_pressure_plate
    [161] = 0,                             //spruce_pressure_plate
    [162] = 0,                             //birch_pressure_plate
    [163] = 0,                             //jungle_pressure_plate
    [164] = 0,                             //acacia_pressure_plate
    [165] = 0,                             //dark_oak_pressure_plate
    [166] = 0,                             //redstone_ore
    [167] = 0,                             //redstone_torch
    [168] = I_FACE,                        //stone_button
    [169] = 0,                             //snow
    [170] = 0,                             //ice
    [171] = 0,                             //snow_block
    [172] = 0,                             //cactus
    [173] = 0,                             //clay
    [174] = 0,                             //jukebox
    [175] = 0,                             //oak_fence
    [176] = 0,                             //spruce_fence
    [177] = 0,                             //birch_fence
    [178] = 0,                             //jungle_fence
    [179] = 0,                             //acacia_fence
    [180] = 0,                             //dark_oak_fence
    [181] = 0,                             //pumpkin
    [182] = 0,                             //carved_pumpkin
    [183] = 0,                             //netherrack
    [184] = 0,                             //soul_sand
    [185] = 0,                             //glowstone
    [186] = 0,                             //jack_o_lantern
    [187] = I_TDOOR,                       //oak_trapdoor
    [188] = I_TDOOR,                       //spruce_trapdoor
    [189] = I_TDOOR,                       //birch_trapdoor
    [190] = I_TDOOR,                       //jungle_trapdoor
    [191] = I_TDOOR,                       //acacia_trapdoor
    [192] = I_TDOOR,                       //dark_oak_trapdoor
    [193] = 0,                             //infested_stone
    [194] = 0,                             //infested_cobblestone
    [195] = 0,                             //infested_stone_bricks
    [196] = 0,                             //infested_mossy_stone_bricks
    [197] = 0,                             //infested_cracked_stone_bricks
    [198] = 0,                             //infested_chiseled_stone_bricks
    [199] = 0,                             //stone_bricks
    [200] = 0,                             //mossy_stone_bricks
    [201] = 0,                             //cracked_stone_bricks
    [202] = 0,                             //chiseled_stone_bricks
    [203] = 0,                             //brown_mushroom_block
    [204] = 0,                             //red_mushroom_block
    [205] = 0,                             //mushroom_stem
    [206] = 0,                             //iron_bars
    [207] = 0,                             //glass_pane
    [208] = 0,                             //melon
    [209] = 0,                             //vine
    [210] = 0,                             //oak_fence_gate
    [211] = 0,                             //spruce_fence_gate
    [212] = 0,                             //birch_fence_gate
    [213] = 0,                             //jungle_fence_gate
    [214] = 0,                             //acacia_fence_gate
    [215] = 0,                             //dark_oak_fence_gate
    [216] = I_STAIR,                       //brick_stairs
    [217] = I_STAIR,                       //stone_brick_stairs
    [218] = 0,                             //mycelium
    [219] = 0,                             //lily_pad
    [220] = 0,                             //nether_bricks
    [221] = 0,                             //nether_brick_fence
    [222] = I_STAIR,                       //nether_brick_stairs
    [223] = I_CONT,                        //enchanting_table
    [224] = 0,                             //end_portal_frame
    [225] = 0,                             //end_stone
    [226] = 0,                             //end_stone_bricks
    [227] = 0,                             //dragon_egg
    [228] = 0,                             //redstone_lamp
    [229] = I_STAIR,                       //sandstone_stairs
    [230] = 0,                             //emerald_ore
    [231] = I_CONT | I_FURNACE,            //ender_chest
    [232] = 0,                             //tripwire_hook
    [233] = 0,                             //emerald_block
    [234] = I_STAIR,                       //spruce_stairs
    [235] = I_STAIR,                       //birch_stairs
    [236] = I_STAIR,                       //jungle_stairs
    [237] = I_CONT,                        //command_block
    [238] = I_CONT,                        //beacon
    [239] = 0,                             //cobblestone_wall
    [240] = 0,                             //mossy_cobblestone_wall
    [241] = I_FACE,                        //oak_button
    [242] = I_FACE,                        //spruce_button
    [243] = I_FACE,                        //birch_button
    [244] = I_FACE,                        //jungle_button
    [245] = I_FACE,                        //acacia_button
    [246] = I_FACE,                        //dark_oak_button
    [247] = I_CONT,                        //anvil
    [248] = I_CONT,                        //chipped_anvil
    [249] = I_CONT,                        //damaged_anvil
    [250] = I_CONT | I_CHEST,              //trapped_chest
    [251] = 0,                             //light_weighted_pressure_plate
    [252] = 0,                             //heavy_weighted_pressure_plate
    [253] = 0,                             //daylight_detector
    [254] = 0,                             //redstone_block
    [255] = 0,                             //nether_quartz_ore
    [256] = I_CONT | I_RSDEV,              //hopper
    [257] = 0,                             //chiseled_quartz_block
    [258] = 0,                             //quartz_block
    [259] = I_AXIS,                        //quartz_pillar
    [260] = I_STAIR,                       //quartz_stairs
    [261] = 0,                             //activator_rail
    [262] = I_CONT | I_RSDEV,              //dropper
    [263] = 0,                             //white_terracotta
    [264] = 0,                             //orange_terracotta
    [265] = 0,                             //magenta_terracotta
    [266] = 0,                             //light_blue_terracotta
    [267] = 0,                             //yellow_terracotta
    [268] = 0,                             //lime_terracotta
    [269] = 0,                             //pink_terracotta
    [270] = 0,                             //gray_terracotta
    [271] = 0,                             //light_gray_terracotta
    [272] = 0,                             //cyan_terracotta
    [273] = 0,                             //purple_terracotta
    [274] = 0,                             //blue_terracotta
    [275] = 0,                             //brown_terracotta
    [276] = 0,                             //green_terracotta
    [277] = 0,                             //red_terracotta
    [278] = 0,                             //black_terracotta
    [279] = 0,                             //barrier
    [280] = I_TDOOR,                       //iron_trapdoor
    [281] = I_AXIS,                        //hay_block
    [282] = 0,                             //white_carpet
    [283] = 0,                             //orange_carpet
    [284] = 0,                             //magenta_carpet
    [285] = 0,                             //light_blue_carpet
    [286] = 0,                             //yellow_carpet
    [287] = 0,                             //lime_carpet
    [288] = 0,                             //pink_carpet
    [289] = 0,                             //gray_carpet
    [290] = 0,                             //light_gray_carpet
    [291] = 0,                             //cyan_carpet
    [292] = 0,                             //purple_carpet
    [293] = 0,                             //blue_carpet
    [294] = 0,                             //brown_carpet
    [295] = 0,                             //green_carpet
    [296] = 0,                             //red_carpet
    [297] = 0,                             //black_carpet
    [298] = 0,                             //terracotta
    [299] = 0,                             //coal_block
    [300] = 0,                             //packed_ice
    [301] = I_STAIR,                       //acacia_stairs
    [302] = I_STAIR,                       //dark_oak_stairs
    [303] = 0,                             //slime_block
    [304] = 0,                             //grass_path
    [305] = 0,                             //sunflower
    [306] = 0,                             //lilac
    [307] = 0,                             //rose_bush
    [308] = 0,                             //peony
    [309] = 0,                             //tall_grass
    [310] = 0,                             //large_fern
    [311] = 0,                             //white_stained_glass
    [312] = 0,                             //orange_stained_glass
    [313] = 0,                             //magenta_stained_glass
    [314] = 0,                             //light_blue_stained_glass
    [315] = 0,                             //yellow_stained_glass
    [316] = 0,                             //lime_stained_glass
    [317] = 0,                             //pink_stained_glass
    [318] = 0,                             //gray_stained_glass
    [319] = 0,                             //light_gray_stained_glass
    [320] = 0,                             //cyan_stained_glass
    [321] = 0,                             //purple_stained_glass
    [322] = 0,                             //blue_stained_glass
    [323] = 0,                             //brown_stained_glass
    [324] = 0,                             //green_stained_glass
    [325] = 0,                             //red_stained_glass
    [326] = 0,                             //black_stained_glass
    [327] = 0,                             //white_stained_glass_pane
    [328] = 0,                             //orange_stained_glass_pane
    [329] = 0,                             //magenta_stained_glass_pane
    [330] = 0,                             //light_blue_stained_glass_pane
    [331] = 0,                             //yellow_stained_glass_pane
    [332] = 0,                             //lime_stained_glass_pane
    [333] = 0,                             //pink_stained_glass_pane
    [334] = 0,                             //gray_stained_glass_pane
    [335] = 0,                             //light_gray_stained_glass_pane
    [336] = 0,                             //cyan_stained_glass_pane
    [337] = 0,                             //purple_stained_glass_pane
    [338] = 0,                             //blue_stained_glass_pane
    [339] = 0,                             //brown_stained_glass_pane
    [340] = 0,                             //green_stained_glass_pane
    [341] = 0,                             //red_stained_glass_pane
    [342] = 0,                             //black_stained_glass_pane
    [343] = 0,                             //prismarine
    [344] = 0,                             //prismarine_bricks
    [345] = 0,                             //dark_prismarine
    [346] = I_STAIR,                       //prismarine_stairs
    [347] = I_STAIR,                       //prismarine_brick_stairs
    [348] = I_STAIR,                       //dark_prismarine_stairs
    [349] = 0,                             //sea_lantern
    [350] = 0,                             //red_sandstone
    [351] = 0,                             //chiseled_red_sandstone
    [352] = 0,                             //cut_red_sandstone
    [353] = I_STAIR,                       //red_sandstone_stairs
    [354] = I_CONT,                        //repeating_command_block
    [355] = I_CONT,                        //chain_command_block
    [356] = 0,                             //magma_block
    [357] = 0,                             //nether_wart_block
    [358] = 0,                             //red_nether_bricks
    [359] = I_AXIS,                        //bone_block
    [360] = 0,                             //structure_void
    [361] = I_RSDEV,                       //observer
    [362] = I_NSTACK | I_CONT,             //shulker_box
    [363] = I_NSTACK | I_CONT,             //white_shulker_box
    [364] = I_NSTACK | I_CONT,             //orange_shulker_box
    [365] = I_NSTACK | I_CONT,             //magenta_shulker_box
    [366] = I_NSTACK | I_CONT,             //light_blue_shulker_box
    [367] = I_NSTACK | I_CONT,             //yellow_shulker_box
    [368] = I_NSTACK | I_CONT,             //lime_shulker_box
    [369] = I_NSTACK | I_CONT,             //pink_shulker_box
    [370] = I_NSTACK | I_CONT,             //gray_shulker_box
    [371] = I_NSTACK | I_CONT,             //light_gray_shulker_box
    [372] = I_NSTACK | I_CONT,             //cyan_shulker_box
    [373] = I_NSTACK | I_CONT,             //purple_shulker_box
    [374] = I_NSTACK | I_CONT,             //blue_shulker_box
    [375] = I_NSTACK | I_CONT,             //brown_shulker_box
    [376] = I_NSTACK | I_CONT,             //green_shulker_box
    [377] = I_NSTACK | I_CONT,             //red_shulker_box
    [378] = I_NSTACK | I_CONT,             //black_shulker_box
    [379] = 0,                             //white_glazed_terracotta
    [380] = 0,                             //orange_glazed_terracotta
    [381] = 0,                             //magenta_glazed_terracotta
    [382] = 0,                             //light_blue_glazed_terracotta
    [383] = 0,                             //yellow_glazed_terracotta
    [384] = 0,                             //lime_glazed_terracotta
    [385] = 0,                             //pink_glazed_terracotta
    [386] = 0,                             //gray_glazed_terracotta
    [387] = 0,                             //light_gray_glazed_terracotta
    [388] = 0,                             //cyan_glazed_terracotta
    [389] = 0,                             //purple_glazed_terracotta
    [390] = 0,                             //blue_glazed_terracotta
    [391] = 0,                             //brown_glazed_terracotta
    [392] = 0,                             //green_glazed_terracotta
    [393] = 0,                             //red_glazed_terracotta
    [394] = 0,                             //black_glazed_terracotta
    [395] = 0,                             //white_concrete
    [396] = 0,                             //orange_concrete
    [397] = 0,                             //magenta_concrete
    [398] = 0,                             //light_blue_concrete
    [399] = 0,                             //yellow_concrete
    [400] = 0,                             //lime_concrete
    [401] = 0,                             //pink_concrete
    [402] = 0,                             //gray_concrete
    [403] = 0,                             //light_gray_concrete
    [404] = 0,                             //cyan_concrete
    [405] = 0,                             //purple_concrete
    [406] = 0,                             //blue_concrete
    [407] = 0,                             //brown_concrete
    [408] = 0,                             //green_concrete
    [409] = 0,                             //red_concrete
    [410] = 0,                             //black_concrete
    [411] = 0,                             //white_concrete_powder
    [412] = 0,                             //orange_concrete_powder
    [413] = 0,                             //magenta_concrete_powder
    [414] = 0,                             //light_blue_concrete_powder
    [415] = 0,                             //yellow_concrete_powder
    [416] = 0,                             //lime_concrete_powder
    [417] = 0,                             //pink_concrete_powder
    [418] = 0,                             //gray_concrete_powder
    [419] = 0,                             //light_gray_concrete_powder
    [420] = 0,                             //cyan_concrete_powder
    [421] = 0,                             //purple_concrete_powder
    [422] = 0,                             //blue_concrete_powder
    [423] = 0,                             //brown_concrete_powder
    [424] = 0,                             //green_concrete_powder
    [425] = 0,                             //red_concrete_powder
    [426] = 0,                             //black_concrete_powder
    [427] = 0,                             //turtle_egg
    [428] = 0,                             //dead_tube_coral_block
    [429] = 0,                             //dead_brain_coral_block
    [430] = 0,                             //dead_bubble_coral_block
    [431] = 0,                             //dead_fire_coral_block
    [432] = 0,                             //dead_horn_coral_block
    [433] = 0,                             //tube_coral_block
    [434] = 0,                             //brain_coral_block
    [435] = 0,                             //bubble_coral_block
    [436] = 0,                             //fire_coral_block
    [437] = 0,                             //horn_coral_block
    [438] = 0,                             //tube_coral
    [439] = 0,                             //brain_coral
    [440] = 0,                             //bubble_coral
    [441] = 0,                             //fire_coral
    [442] = 0,                             //horn_coral
    [443] = 0,                             //dead_brain_coral
    [444] = 0,                             //dead_bubble_coral
    [445] = 0,                             //dead_fire_coral
    [446] = 0,                             //dead_horn_coral
    [447] = 0,                             //dead_tube_coral
    [448] = 0,                             //tube_coral_fan
    [449] = 0,                             //brain_coral_fan
    [450] = 0,                             //bubble_coral_fan
    [451] = 0,                             //fire_coral_fan
    [452] = 0,                             //horn_coral_fan
    [453] = 0,                             //dead_tube_coral_fan
    [454] = 0,                             //dead_brain_coral_fan
    [455] = 0,                             //dead_bubble_coral_fan
    [456] = 0,                             //dead_fire_coral_fan
    [457] = 0,                             //dead_horn_coral_fan
    [458] = 0,                             //blue_ice
    [459] = 0,                             //conduit
    [460] = I_DOOR,                        //iron_door
    [461] = I_DOOR,                        //oak_door
    [462] = I_DOOR,                        //spruce_door
    [463] = I_DOOR,                        //birch_door
    [464] = I_DOOR,                        //jungle_door
    [465] = I_DOOR,                        //acacia_door
    [466] = I_DOOR,                        //dark_oak_door
    [467] = 0,                             //repeater
    [468] = 0,                             //comparator
    [469] = 0,                             //structure_block
    [470] = I_NSTACK | I_ITEM | I_ARMOR,   //turtle_helmet
    [471] = I_ITEM,                        //scute
    [472] = I_NSTACK | I_ITEM,             //iron_shovel
    [473] = I_NSTACK | I_ITEM,             //iron_pickaxe
    [474] = I_NSTACK | I_ITEM,             //iron_axe
    [475] = I_NSTACK | I_ITEM,             //flint_and_steel
    [476] = I_ITEM,                        //apple
    [477] = I_NSTACK | I_ITEM,             //bow
    [478] = I_ITEM,                        //arrow
    [479] = I_ITEM,                        //coal
    [480] = I_ITEM,                        //charcoal
    [481] = I_ITEM,                        //diamond
    [482] = I_ITEM,                        //iron_ingot
    [483] = I_ITEM,                        //gold_ingot
    [484] = I_NSTACK | I_ITEM,             //iron_sword
    [485] = I_NSTACK | I_ITEM,             //wooden_sword
    [486] = I_NSTACK | I_ITEM,             //wooden_shovel
    [487] = I_NSTACK | I_ITEM,             //wooden_pickaxe
    [488] = I_NSTACK | I_ITEM,             //wooden_axe
    [489] = I_NSTACK | I_ITEM,             //stone_sword
    [490] = I_NSTACK | I_ITEM,             //stone_shovel
    [491] = I_NSTACK | I_ITEM,             //stone_pickaxe
    [492] = I_NSTACK | I_ITEM,             //stone_axe
    [493] = I_NSTACK | I_ITEM,             //diamond_sword
    [494] = I_NSTACK | I_ITEM,             //diamond_shovel
    [495] = I_NSTACK | I_ITEM,             //diamond_pickaxe
    [496] = I_NSTACK | I_ITEM,             //diamond_axe
    [497] = I_ITEM,                        //stick
    [498] = I_ITEM,                        //bowl
    [499] = I_NSTACK | I_ITEM,             //mushroom_stew
    [500] = I_NSTACK | I_ITEM,             //golden_sword
    [501] = I_NSTACK | I_ITEM,             //golden_shovel
    [502] = I_NSTACK | I_ITEM,             //golden_pickaxe
    [503] = I_NSTACK | I_ITEM,             //golden_axe
    [504] = I_ITEM,                        //string
    [505] = I_ITEM,                        //feather
    [506] = I_ITEM,                        //gunpowder
    [507] = I_NSTACK | I_ITEM,             //wooden_hoe
    [508] = I_NSTACK | I_ITEM,             //stone_hoe
    [509] = I_NSTACK | I_ITEM,             //iron_hoe
    [510] = I_NSTACK | I_ITEM,             //diamond_hoe
    [511] = I_NSTACK | I_ITEM,             //golden_hoe
    [512] = I_ITEM,                        //wheat_seeds
    [513] = 0,                             //wheat
    [514] = I_ITEM,                        //bread
    [515] = I_NSTACK | I_ITEM | I_ARMOR,   //leather_helmet
    [516] = I_NSTACK | I_ITEM | I_ARMOR,   //leather_chestplate
    [517] = I_NSTACK | I_ITEM | I_ARMOR,   //leather_leggings
    [518] = I_NSTACK | I_ITEM | I_ARMOR,   //leather_boots
    [519] = I_NSTACK | I_ITEM | I_ARMOR,   //chainmail_helmet
    [520] = I_NSTACK | I_ITEM | I_ARMOR,   //chainmail_chestplate
    [521] = I_NSTACK | I_ITEM | I_ARMOR,   //chainmail_leggings
    [522] = I_NSTACK | I_ITEM | I_ARMOR,   //chainmail_boots
    [523] = I_NSTACK | I_ITEM | I_ARMOR,   //iron_helmet
    [524] = I_NSTACK | I_ITEM | I_ARMOR,   //iron_chestplate
    [525] = I_NSTACK | I_ITEM | I_ARMOR,   //iron_leggings
    [526] = I_NSTACK | I_ITEM | I_ARMOR,   //iron_boots
    [527] = I_NSTACK | I_ITEM | I_ARMOR,   //diamond_helmet
    [528] = I_NSTACK | I_ITEM | I_ARMOR,   //diamond_chestplate
    [529] = I_NSTACK | I_ITEM | I_ARMOR,   //diamond_leggings
    [530] = I_NSTACK | I_ITEM | I_ARMOR,   //diamond_boots
    [531] = I_NSTACK | I_ITEM | I_ARMOR,   //golden_helmet
    [532] = I_NSTACK | I_ITEM | I_ARMOR,   //golden_chestplate
    [533] = I_NSTACK | I_ITEM | I_ARMOR,   //golden_leggings
    [534] = I_NSTACK | I_ITEM | I_ARMOR,   //golden_boots
    [535] = I_ITEM,                        //flint
    [536] = I_ITEM,                        //porkchop
    [537] = I_ITEM,                        //cooked_porkchop
    [538] = I_ITEM,                        //painting
    [539] = I_ITEM,                        //golden_apple
    [540] = I_ITEM,                        //enchanted_golden_apple
    [541] = I_S16,                         //sign
    [542] = I_S16 | I_ITEM,                //bucket
    [543] = I_NSTACK | I_ITEM,             //water_bucket
    [544] = I_NSTACK | I_ITEM,             //lava_bucket
    [545] = I_NSTACK | I_ITEM,             //minecart
    [546] = I_NSTACK | I_ITEM,             //saddle
    [547] = I_ITEM,                        //redstone
    [548] = I_S16 | I_ITEM,                //snowball
    [549] = I_NSTACK | I_ITEM,             //oak_boat
    [550] = I_ITEM,                        //leather
    [551] = I_NSTACK | I_ITEM,             //milk_bucket
    [552] = I_NSTACK | I_ITEM,             //pufferfish_bucket
    [553] = I_NSTACK | I_ITEM,             //salmon_bucket
    [554] = I_NSTACK | I_ITEM,             //cod_bucket
    [555] = I_NSTACK | I_ITEM,             //tropical_fish_bucket
    [556] = I_ITEM,                        //brick
    [557] = I_ITEM,                        //clay_ball
    [558] = 0,                             //sugar_cane
    [559] = 0,                             //kelp
    [560] = 0,                             //dried_kelp_block
    [561] = I_ITEM,                        //paper
    [562] = I_ITEM,                        //book
    [563] = I_ITEM,                        //slime_ball
    [564] = I_NSTACK | I_ITEM,             //chest_minecart
    [565] = I_NSTACK | I_ITEM,             //furnace_minecart
    [566] = I_S16 | I_ITEM,                //egg
    [567] = I_ITEM,                        //compass
    [568] = I_NSTACK | I_ITEM,             //fishing_rod
    [569] = I_ITEM,                        //clock
    [570] = I_ITEM,                        //glowstone_dust
    [571] = I_ITEM,                        //cod
    [572] = I_ITEM,                        //salmon
    [573] = I_ITEM,                        //tropical_fish
    [574] = I_ITEM,                        //pufferfish
    [575] = I_ITEM,                        //cooked_cod
    [576] = I_ITEM,                        //cooked_salmon
    [577] = I_ITEM,                        //ink_sac
    [578] = I_ITEM,                        //rose_red
    [579] = I_ITEM,                        //cactus_green
    [580] = I_ITEM,                        //cocoa_beans
    [581] = I_ITEM,                        //lapis_lazuli
    [582] = I_ITEM,                        //purple_dye
    [583] = I_ITEM,                        //cyan_dye
    [584] = I_ITEM,                        //light_gray_dye
    [585] = I_ITEM,                        //gray_dye
    [586] = I_ITEM,                        //pink_dye
    [587] = I_ITEM,                        //lime_dye
    [588] = I_ITEM,                        //dandelion_yellow
    [589] = I_ITEM,                        //light_blue_dye
    [590] = I_ITEM,                        //magenta_dye
    [591] = I_ITEM,                        //orange_dye
    [592] = I_ITEM,                        //bone_meal
    [593] = I_ITEM,                        //bone
    [594] = I_ITEM,                        //sugar
    [595] = I_NSTACK,                      //cake
    [596] = I_NSTACK | I_BED,              //white_bed
    [597] = I_NSTACK | I_BED,              //orange_bed
    [598] = I_NSTACK | I_BED,              //magenta_bed
    [599] = I_NSTACK | I_BED,              //light_blue_bed
    [600] = I_NSTACK | I_BED,              //yellow_bed
    [601] = I_NSTACK | I_BED,              //lime_bed
    [602] = I_NSTACK | I_BED,              //pink_bed
    [603] = I_NSTACK | I_BED,              //gray_bed
    [604] = I_NSTACK | I_BED,              //light_gray_bed
    [605] = I_NSTACK | I_BED,              //cyan_bed
    [606] = I_NSTACK | I_BED,              //purple_bed
    [607] = I_NSTACK | I_BED,              //blue_bed
    [608] = I_NSTACK | I_BED,              //brown_bed
    [609] = I_NSTACK | I_BED,              //green_bed
    [610] = I_NSTACK | I_BED,              //red_bed
    [611] = I_NSTACK | I_BED,              //black_bed
    [612] = I_ITEM,                        //cookie
    [613] = I_ITEM,                        //filled_map
    [614] = I_NSTACK | I_ITEM,             //shears
    [615] = I_ITEM,                        //melon_slice
    [616] = I_ITEM,                        //dried_kelp
    [617] = I_ITEM,                        //pumpkin_seeds
    [618] = I_ITEM,                        //melon_seeds
    [619] = I_ITEM,                        //beef
    [620] = I_ITEM,                        //cooked_beef
    [621] = I_ITEM,                        //chicken
    [622] = I_ITEM,                        //cooked_chicken
    [623] = I_ITEM,                        //rotten_flesh
    [624] = I_S16 | I_ITEM,                //ender_pearl
    [625] = I_ITEM,                        //blaze_rod
    [626] = I_ITEM,                        //ghast_tear
    [627] = I_ITEM,                        //gold_nugget
    [628] = 0,                             //nether_wart
    [629] = I_NSTACK | I_ITEM,             //potion
    [630] = I_ITEM,                        //glass_bottle
    [631] = I_ITEM,                        //spider_eye
    [632] = I_ITEM,                        //fermented_spider_eye
    [633] = I_ITEM,                        //blaze_powder
    [634] = I_ITEM,                        //magma_cream
    [635] = I_CONT,                        //brewing_stand
    [636] = 0,                             //cauldron
    [637] = I_ITEM,                        //ender_eye
    [638] = I_ITEM,                        //glistering_melon_slice
    [639] = I_ITEM,                        //bat_spawn_egg
    [640] = I_ITEM,                        //blaze_spawn_egg
    [641] = I_ITEM,                        //cave_spider_spawn_egg
    [642] = I_ITEM,                        //chicken_spawn_egg
    [643] = I_ITEM,                        //cod_spawn_egg
    [644] = I_ITEM,                        //cow_spawn_egg
    [645] = I_ITEM,                        //creeper_spawn_egg
    [646] = I_ITEM,                        //dolphin_spawn_egg
    [647] = I_ITEM,                        //donkey_spawn_egg
    [648] = I_ITEM,                        //drowned_spawn_egg
    [649] = I_ITEM,                        //elder_guardian_spawn_egg
    [650] = I_ITEM,                        //enderman_spawn_egg
    [651] = I_ITEM,                        //endermite_spawn_egg
    [652] = I_ITEM,                        //evoker_spawn_egg
    [653] = I_ITEM,                        //ghast_spawn_egg
    [654] = I_ITEM,                        //guardian_spawn_egg
    [655] = I_ITEM,                        //horse_spawn_egg
    [656] = I_ITEM,                        //husk_spawn_egg
    [657] = I_ITEM,                        //llama_spawn_egg
    [658] = I_ITEM,                        //magma_cube_spawn_egg
    [659] = I_ITEM,                        //mooshroom_spawn_egg
    [660] = I_ITEM,                        //mule_spawn_egg
    [661] = I_ITEM,                        //ocelot_spawn_egg
    [662] = I_ITEM,                        //parrot_spawn_egg
    [663] = I_ITEM,                        //phantom_spawn_egg
    [664] = I_ITEM,                        //pig_spawn_egg
    [665] = I_ITEM,                        //polar_bear_spawn_egg
    [666] = I_ITEM,                        //pufferfish_spawn_egg
    [667] = I_ITEM,                        //rabbit_spawn_egg
    [668] = I_ITEM,                        //salmon_spawn_egg
    [669] = I_ITEM,                        //sheep_spawn_egg
    [670] = I_ITEM,                        //shulker_spawn_egg
    [671] = I_ITEM,                        //silverfish_spawn_egg
    [672] = I_ITEM,                        //skeleton_spawn_egg
    [673] = I_ITEM,                        //skeleton_horse_spawn_egg
    [674] = I_ITEM,                        //slime_spawn_egg
    [675] = I_ITEM,                        //spider_spawn_egg
    [676] = I_ITEM,                        //squid_spawn_egg
    [677] = I_ITEM,                        //stray_spawn_egg
    [678] = I_ITEM,                        //tropical_fish_spawn_egg
    [679] = I_ITEM,                        //turtle_spawn_egg
    [680] = I_ITEM,                        //vex_spawn_egg
    [681] = I_ITEM,                        //villager_spawn_egg
    [682] = I_ITEM,                        //vindicator_spawn_egg
    [683] = I_ITEM,                        //witch_spawn_egg
    [684] = I_ITEM,                        //wither_skeleton_spawn_egg
    [685] = I_ITEM,                        //wolf_spawn_egg
    [686] = I_ITEM,                        //zombie_spawn_egg
    [687] = I_ITEM,                        //zombie_horse_spawn_egg
    [688] = I_ITEM,                        //zombie_pigman_spawn_egg
    [689] = I_ITEM,                        //zombie_villager_spawn_egg
    [690] = I_ITEM,                        //experience_bottle
    [691] = I_ITEM,                        //fire_charge
    [692] = I_NSTACK | I_ITEM,             //writable_book
    [693] = I_S16 | I_ITEM,                //written_book
    [694] = I_ITEM,                        //emerald
    [695] = I_ITEM,                        //item_frame
    [696] = 0,                             //flower_pot
    [697] = I_ITEM,                        //carrot
    [698] = I_ITEM,                        //potato
    [699] = I_ITEM,                        //baked_potato
    [700] = I_ITEM,                        //poisonous_potato
    [701] = I_ITEM,                        //map
    [702] = I_ITEM,                        //golden_carrot
    [703] = 0,                             //skeleton_skull
    [704] = 0,                             //wither_skeleton_skull
    [705] = 0,                             //player_head
    [706] = 0,                             //zombie_head
    [707] = 0,                             //creeper_head
    [708] = 0,                             //dragon_head
    [709] = I_NSTACK | I_ITEM,             //carrot_on_a_stick
    [710] = I_ITEM,                        //nether_star
    [711] = I_ITEM,                        //pumpkin_pie
    [712] = I_ITEM,                        //firework_rocket
    [713] = I_ITEM,                        //firework_star
    [714] = I_NSTACK | I_ITEM,             //enchanted_book
    [715] = I_ITEM,                        //nether_brick
    [716] = I_ITEM,                        //quartz
    [717] = I_NSTACK | I_ITEM,             //tnt_minecart
    [718] = I_NSTACK | I_ITEM,             //hopper_minecart
    [719] = I_ITEM,                        //prismarine_shard
    [720] = I_ITEM,                        //prismarine_crystals
    [721] = I_ITEM,                        //rabbit
    [722] = I_ITEM,                        //cooked_rabbit
    [723] = I_NSTACK | I_ITEM,             //rabbit_stew
    [724] = I_ITEM,                        //rabbit_foot
    [725] = I_ITEM,                        //rabbit_hide
    [726] = I_S16 | I_ITEM,                //armor_stand
    [727] = I_NSTACK | I_ITEM,             //iron_horse_armor
    [728] = I_NSTACK | I_ITEM,             //golden_horse_armor
    [729] = I_NSTACK | I_ITEM,             //diamond_horse_armor
    [730] = I_ITEM,                        //lead
    [731] = I_ITEM,                        //name_tag
    [732] = I_NSTACK | I_ITEM,             //command_block_minecart
    [733] = I_ITEM,                        //mutton
    [734] = I_ITEM,                        //cooked_mutton
    [735] = I_S16,                         //white_banner
    [736] = I_S16,                         //orange_banner
    [737] = I_S16,                         //magenta_banner
    [738] = I_S16,                         //light_blue_banner
    [739] = I_S16,                         //yellow_banner
    [740] = I_S16,                         //lime_banner
    [741] = I_S16,                         //pink_banner
    [742] = I_S16,                         //gray_banner
    [743] = I_S16,                         //light_gray_banner
    [744] = I_S16,                         //cyan_banner
    [745] = I_S16,                         //purple_banner
    [746] = I_S16,                         //blue_banner
    [747] = I_S16,                         //brown_banner
    [748] = I_S16,                         //green_banner
    [749] = I_S16,                         //red_banner
    [750] = I_S16,                         //black_banner
    [751] = I_ITEM,                        //end_crystal
    [752] = I_ITEM,                        //chorus_fruit
    [753] = I_ITEM,                        //popped_chorus_fruit
    [754] = I_ITEM,                        //beetroot
    [755] = I_ITEM,                        //beetroot_seeds
    [756] = I_NSTACK | I_ITEM,             //beetroot_soup
    [757] = I_ITEM,                        //dragon_breath
    [758] = I_NSTACK | I_ITEM,             //splash_potion
    [759] = I_NSTACK | I_ITEM,             //spectral_arrow
    [760] = I_NSTACK | I_ITEM,             //tipped_arrow
    [761] = I_NSTACK | I_ITEM,             //lingering_potion
    [762] = I_NSTACK | I_ITEM | I_ARMOR,   //shield
    [763] = I_NSTACK | I_ITEM | I_ARMOR,   //elytra
    [764] = I_NSTACK | I_ITEM,             //spruce_boat
    [765] = I_NSTACK | I_ITEM,             //birch_boat
    [766] = I_NSTACK | I_ITEM,             //jungle_boat
    [767] = I_NSTACK | I_ITEM,             //acacia_boat
    [768] = I_NSTACK | I_ITEM,             //dark_oak_boat
    [769] = I_NSTACK | I_ITEM,             //totem_of_undying
    [770] = I_ITEM,                        //shulker_shell
    [771] = I_ITEM,                        //iron_nugget
    [772] = I_NSTACK | I_ITEM,             //knowledge_book
    [773] = I_NSTACK | I_ITEM,             //debug_stick
    [774] = I_NSTACK | I_ITEM,             //music_disc_13
    [775] = I_NSTACK | I_ITEM,             //music_disc_cat
    [776] = I_NSTACK | I_ITEM,             //music_disc_blocks
    [777] = I_NSTACK | I_ITEM,             //music_disc_chirp
    [778] = I_NSTACK | I_ITEM,             //music_disc_far
    [779] = I_NSTACK | I_ITEM,             //music_disc_mall
    [780] = I_NSTACK | I_ITEM,             //music_disc_mellohi
    [781] = I_NSTACK | I_ITEM,             //music_disc_stal
    [782] = I_NSTACK | I_ITEM,             //music_disc_strad
    [783] = I_NSTACK | I_ITEM,             //music_disc_ward
    [784] = I_NSTACK | I_ITEM,             //music_disc_11
    [785] = I_NSTACK | I_ITEM,             //music_disc_wait
    [786] = I_NSTACK | I_ITEM,             //trident
    [787] = I_ITEM,                        //phantom_membrane
    [788] = I_ITEM,                        //nautilus_shell
    [789] = I_ITEM                         //heart_of_the_sea
};

const int db_num_items = sizeof(item_flags)/sizeof(uint64_t);

// Returns stacksize of an item
int db_stacksize (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_NSTACK) {
        return 1; //doesnt stack
    }
    if (item_flags[item_id] & I_S16) {
        return 16;
    }
    return 64;
}

// True if item exists as item only
int db_item_is_itemonly (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_ITEM) {
        return 1;
    }
    return 0;
}

// True if item is a container (opens a dialog window)
int db_item_is_container (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_CONT) {
        return 1;
    }
    return 0;
}

// True if item is placed on an axis (logs, wood, stripped, quartz & purper pillars, hay & bone)
int db_item_is_axis (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_AXIS) {
        return 1;
    }
    return 0;
}

// True if item is a slab
int db_item_is_slab (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_SLAB) {
        return 1;
    }
    return 0;
}

// True if item is a stair
int db_item_is_stair (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_STAIR) {
        return 1;
    }
    return 0;
}
// True if item is a door
int db_item_is_door (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_DOOR) {
        return 1;
    }
    return 0;
}

// True if item is a trapdoor
int db_item_is_tdoor (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_TDOOR) {
        return 1;
    }
    return 0;
}

// True if item has the face property (buttons, lever, upcoming grindstone)
int db_item_is_face (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_FACE) {
        return 1;
    }
    return 0;
}

// True if item is a bed
int db_item_is_bed (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_BED) {
        return 1;
    }
    return 0;
}

// True if item is a redstone device
int db_item_is_rsdev (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_RSDEV) {
        return 1;
    }
    return 0;
}

// True if item is a chest or trapped chest (single/left/right orientable containers)
int db_item_is_chest (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_CHEST) {
        return 1;
    }
    return 0;
}

// True if item is a enderchest or furnace  (orientable containers)
int db_item_is_furnace (int item_id) {
    assert ( item_id >= 0 && item_id < db_num_items );
    if (item_flags[item_id] & I_FURNACE) {
        return 1;
    }
    return 0;
}
