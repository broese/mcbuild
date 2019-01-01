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
#include "lh_buffers.h"
#include "lh_files.h"
#include "lh_dir.h"


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

//forward declaration
int try_to_get_missing_json_files(int protocol_id);

database_t *load_database(int protocol_id) {

    if (db.initialized) {
        return &db;
    }

    //TODO: Protocol Specific
    assert(protocol_id == 404);

    char *blockjsonfilepath = "./database/blocks.json";    //TODO: become blocks_404.json
    char *itemjsonfilepath = "./database/items.json";  //TODO: become items_404.json

    //Check for the proper blocks.json and items.json in the database directory
    FILE *f1 = fopen(blockjsonfilepath, "r");
    FILE *f2 = fopen(itemjsonfilepath, "r");
    if ( f1 && f2 ) {
        fclose(f1);
        fclose(f2);
    }
    else {
        //Attempt to download server.jar and extract them from it
        int rc = try_to_get_missing_json_files(protocol_id);
        if (rc) {
            printf("Error couldnt load database\n");
            return NULL;
        }
        //Now the files are there
    }

    db.protocol = protocol_id;
    db.itemcount = 0;
    db.blockcount = 0;
    db.initialized = 1;

    //////////////////////////////////
    //  Load blocks.json into db    //
    //////////////////////////////////

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
    printf("Dumping Blocks...\n");
    printf("%30s %05s %05s %05s %s\n","blockname","blkid","oldid","defid","#prop");
    for (int i=0; (i < db->blockcount) && (i < maxlines); i++) {
        printf("%30s ", db->block[i].name);
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

void dump_db_items(database_t *db, int maxlines){
    printf("Dumping Items...\n");
    for (int i=0; (i < db->itemcount) && (i < maxlines) ; i++) {
        printf("%30s ", db->item[i].name);
        printf("%05d ", db->item[i].id);
        printf("\n");
    }
}

int dump_db_blocks_to_csv_file(database_t *db) {
    FILE *fp = fopen("./database/mcb_db_404_blocks.csv","w");
    if  ( fp == NULL ) {
        printf("Can't open csv file\n");
        return -1;
    }
    fprintf(fp, "%s, %s, %s, %s, %s\n","blockname","blkid","oldid","defid","#prop");
    for (int i=0; i < db->blockcount ; i++) {
        fprintf(fp, "%s,", db->block[i].name);
        fprintf(fp, "%d,", db->block[i].id);
        fprintf(fp, "%d,", db->block[i].oldid);
        fprintf(fp, "%d,", db->block[i].defaultid);
        fprintf(fp, "%d", db->block[i].propcount);
        for (int j=0; j < db->block[i].propcount; j++) {
            fprintf(fp, ",prop:%s val:%s", db->block[i].prop[j].pname, db->block[i].prop[j].pvalue);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}

int dump_db_items_to_csv_file(database_t *db) {
    FILE *fp = fopen("./database/mcb_db_404_items.csv","w");
    if  ( fp == NULL ) {
        printf("Can't open csv file\n");
        return -1;
    }
    fprintf(fp, "%s,%s\n", "itemname","id");
    for (int i=0; i < db->itemcount ; i++) {
        fprintf(fp, "%s,", db->item[i].name);
        fprintf(fp, "%d\n", db->item[i].id);
    }
    fclose(fp);
    return 0;
}

int save_db_to_file(database_t *db) {
    FILE *fp = fopen("./database/mcb_db_404.tmp","w");
    if  ( fp == NULL ) {
        printf("Can't open temp file\n");
        return -1;
    }
    fprintf(fp, "%d\n", db->protocol);
    fprintf(fp, "%d\n", db->itemcount);
    fprintf(fp, "%d\n", db->blockcount);
    for (int i=0; i < db->itemcount; i++) {
        fprintf(fp, "%d\n",db->item[i].id);
        fprintf(fp, "%s\n",db->item[i].name);
    }
    for (int i=0; i < db->blockcount; i++) {
        fprintf(fp, "%s\n",db->block[i].name);
        fprintf(fp, "%d\n",db->block[i].id);
        fprintf(fp, "%d\n",db->block[i].oldid);
        fprintf(fp, "%d\n",db->block[i].defaultid);
        fprintf(fp, "%d\n",db->block[i].propcount);
        for (int j=0; j < db->block[i].propcount; j++) {
            fprintf(fp, "%s\n",db->block[i].prop[j].pname);
            fprintf(fp, "%s\n",db->block[i].prop[j].pvalue);
        }
    }
    fclose(fp);
    return 0;
}

// get_block_propval(db,14,"facing") => NULL // no such property
// get_block_propval(db,1650,"facing") => "north"
// get_block_propval(db,1686,"half") => "bottom"
const char * get_block_propval(database_t *db, int id, const char *propname) {
    if (id < 0 || id > db->blockcount) {
        return NULL;
    }
    for (int i=0; i < db->blockcount; i++) {
        if (id == db->block[i].id) {
            for (int j=0; j<db->block[i].propcount; j++) {
                if (!strcmp(db->block[i].prop[j].pname, propname)) {
                    char *buf = malloc(strlen(db->block[i].prop[j].pvalue)+1);
                    strcpy(buf,db->block[i].prop[j].pvalue);
                    return buf;
                }
            }
            break;
        }
    }
    return NULL;
}

// TODO: use what we already have
char *protocol_to_ver_string(int protid){
    if (protid == 404) {
        return "1.13.2";
    }
    return "unknown";
}

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

char *findjavapath() {
    char *javapath;
    char *os = getenv("OS");
    if(!strcmp(os,"Windows_NT")) {
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
        return javapath;
    }
    //linux often has JAVA_HOME ready to go
    char *javahome = getenv("JAVA_HOME");
    if (!javahome) {
        return NULL;
    }
    javapath = malloc(strlen(javahome)+5);
    sprintf(javapath,"%s/bin",javahome);
    return javapath;
}

//if blocks.json and items.json dont exist, use this to fetch the server jar and extract them
int try_to_get_missing_json_files(int protocol_id) {
    int rc;

    // Download version manifest from Mojang
    char *MANIFESTURL = "https://launchermeta.mojang.com/mc/game/version_manifest.json";
    char *MANIFESTFILESPEC = "./database/version_manifest.json";
    rc = download_url_into_file(MANIFESTURL,MANIFESTFILESPEC);
    if (rc) {
        printf ("Error downloading version manifest. %d\n",rc);
        return -1;
    }

    char *verid = protocol_to_ver_string(protocol_id);
    json_object *jman,*jver,*j132,*jid,*jurl;
    const char *urlversioninfo;

    // Parse version manifest to get the versioninfo download url for this version
    jman = json_object_from_file (MANIFESTFILESPEC);
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
    char *VERINFOFILESPEC = "./database/versioninfo.json";
    rc = download_url_into_file(urlversioninfo,VERINFOFILESPEC);
    if (rc) {
        printf ("Error downloading versioninfo. %d\n",rc);
        return -1;
    }

    json_object *jvinfo,*jdl,*jserv,*jservurl;
    const char *urlserverjar;

    // Parse versioninfo json to get the server.jar download url for this version
    jvinfo = json_object_from_file(VERINFOFILESPEC);
    json_object_object_get_ex(jvinfo, "downloads", &jdl);
    json_object_object_get_ex(jdl, "server", &jserv);
    json_object_object_get_ex(jserv, "url", &jservurl);
    urlserverjar = json_object_get_string(jservurl);
    if (urlserverjar == NULL) {
        printf ("Error parsing versioninfo.\n");
        return -1;
    }

    char serverfilespec[50];
    snprintf(serverfilespec, sizeof(serverfilespec), "./database/server_%s.jar", verid);

    // Download server.jar from Mojang
    rc = download_url_into_file(urlserverjar,serverfilespec);
    if (rc) {
        printf ("Error downloading server.jar. %d\n",rc);
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
    sprintf(cmd,"cd database; %s/java.exe -cp server_%s.jar net.minecraft.data.Main --all",javafilepath,verid);
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
    rc = rename("./database/generated/reports/items.json", "./database/items.json");
    rc |= rename("./database/generated/reports/blocks.json","./database/blocks.json");
    if (rc) {
        printf("Error couldnt move the items blocks json to the database directory. %d\n",rc);
        return -1;
    }

    //Cleanup
    rc = system("rm -rf ./database/generated");
    rc |= system("rm -rf ./database/logs");
    rc |= remove(VERINFOFILESPEC);
    rc |= remove(MANIFESTFILESPEC);
    // rc |= remove(serverfilespec);     //if we want to remove the server.jar too
    if (rc) {
        printf("Warning: Couldnt cleanup. %d",rc);
    }

    return 0;
}




//struct prop_t { const char *pname, const char *pvalue };
// prop_t * defines a set of properties I'm interested in. It's a list no longer than 3 I guess
// how should we define the length? it could be {NULL,NULL} terminated or length explicitly given.
// many different IDs can match
//int get_matching_block_ids(database_t *db, const char *name, prop_t *match, int *ids);
// places all ids matching a set of propeties for the block name into array ids (can be assumed to be long enough) and returns the number of ids
