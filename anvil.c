/*
 Authors:
 Copyright 2012-2016 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

/*
  anvil : handling of Anvil on-disk file format
*/

#include <stdio.h>
#include <assert.h>

#include <lh_buffers.h>
#include <lh_debug.h>
#include <lh_files.h>
#include <lh_bytes.h>
#include <lh_compress.h>

#include "anvil.h"

// create an empty region
mca * anvil_create() {
    lh_create_obj(mca, region);
    return region;
}

// free a region include all chunks
void anvil_free(mca * region) {
    int i;
    for(i=0; i<REGCHUNKS; i++)
        lh_free(region->data[i]);
    lh_free(region);
}

// show allocated chunks of a region
void anvil_dump(mca * region) {
    int i,j;
    for(i=0; i<REGCHUNKS; i+=32) {
        for(j=i; j<i+32; j++) {
            printf("%c", region->data[j] ? '#' : '.');
        }
        printf("\n");
    }
}

// load an anvil region from disk
mca * anvil_load(const char *path) {
    uint8_t *buf;
    ssize_t sz = lh_load_alloc(path, &buf);
    if (sz<=0) return NULL;

    lh_create_obj(mca, region);

    int i;
    uint8_t *p = buf;
    uint8_t *t = buf+4096;
    for(i=0; i<REGCHUNKS; i++) {
        uint32_t choff = lh_read_int_be(p);
        region->ts[i] = lh_read_int_be(t);
        if (choff) { // this chunk is non-empty
            ssize_t clen = (choff&0xff)<<12;
            lh_alloc_buf(region->data[i], clen);
            memmove(region->data[i], buf+((choff>>8)<<12), clen);
            region->len[i] = clen;
        }
    }

    lh_free(buf);
    return region;
}

// save a region to disk
ssize_t anvil_save(mca *region, const char *path) {
    // generate chunk table first, looking at chunks availability and length
    uint8_t buf[8192];
    int i;
    int choff=2;
    uint8_t *w = buf;

    // fill out the chunk offset and timstamp tables
    for(i=0; i<REGCHUNKS; i++) {
        if (region->data[i]) {
            int chlen = lh_align(region->len[i], 4096);
            lh_write_int_be(w, (choff<<8) + (chlen>>12));
            //printf("Chunk %d (%d,%d), off=%d, len=%d\n", i, i%32, i/32, choff, (chlen>>12));
            choff+=(chlen>>12);
        }
        else {
            lh_write_int_be(w, 0);
        }
    }

    for(i=0; i<REGCHUNKS; i++)
        lh_write_int_be(w, region->ts[i]);

    // write out the header tables
    int fd=lh_open_write(path);
    if (fd<0) return -1;
    lh_write(fd, buf, sizeof(buf));

    // write out chunk data, padded with zeros to page size
    lh_clear_obj(buf);
    ssize_t sz = 8192;
    for(i=0; i<REGCHUNKS; i++) {
        if (region->data[i]) {
            int chlen = lh_align(region->len[i], 4096);
            lh_write(fd, region->data[i], region->len[i]);
            if (chlen > region->len[i])
                lh_write(fd, buf, (chlen-region->len[i]));
            sz += chlen;
        }
    }
    close(fd);
    return sz;
}

static uint8_t nbtdata[2<<24]; // static buffer for NBT data
static uint8_t cdata[2<<22];   // static buffer for compressed data

// return decoded NBT data of a chunk from the region
nbt_t * anvil_get_chunk(mca * region, int32_t X, int32_t Z) {
    // chunk index in the region - we can accept local and global coordinates
    int idx = (X&0x1f)+((Z&0x1f)<<5);

    // chunk is not available
    if (!region->data[idx]) return NULL;

    // parse chunk header
    uint8_t *p = region->data[idx];
    uint32_t len = lh_read_int_be(p)-1;
    assert(region->len[idx]>=len+5);
    uint8_t ctype = lh_read_char(p);
    assert(ctype==1 || ctype==2);

    ssize_t dlen;
    if (ctype==1)
        dlen = lh_gzip_decode_to(p, len, nbtdata, sizeof(nbtdata));
    else
        dlen = lh_zlib_decode_to(p, len, nbtdata, sizeof(nbtdata));

    p = nbtdata;
    nbt_t * nbt = nbt_parse(&p);

    return nbt;
}

// add a chunk in NBT form to the region
void anvil_insert_chunk(mca * region, int32_t X, int32_t Z, nbt_t *nbt) {
    // chunk index in the region - we can accept local and global coordinates
    int idx = (X&0x1f)+((Z&0x1f)<<5);

    // chunk is available - delete it
    lh_free(region->data[idx]);

    // serialize and compress chunk NBT
    uint8_t *w = nbtdata;
    nbt_write(&w, nbt);
    ssize_t clen = lh_zlib_encode_to(nbtdata, w-nbtdata, cdata, sizeof(cdata));

    // store it in the region
    region->data[idx] = malloc(clen+5);
    region->len[idx]  = clen+5;
    w = region->data[idx];
    lh_write_int_be(w, (uint32_t)clen+1);
    lh_write_char(w, 2);
    memmove(w, cdata, clen);
}

nbt_t * anvil_tile_entities(gschunk * ch) {
    nbt_t *tent = NULL;
    if (ch->tent)
        tent = nbt_clone(ch->tent);
    else
        tent = nbt_new(NBT_LIST, "TileEntities", 0);
    return tent;
}

// generate chunk NBT from chunk_t data
nbt_t * anvil_chunk_create(gschunk * ch, int X, int Z) {
    int y,i;

    // TODO: export entities and tile entities
    nbt_t * ent = nbt_new(NBT_LIST, "Entities", 0);
    nbt_t * tent = anvil_tile_entities(ch);

    // Calculate the height map - and fill out the cube mask
    int hmap[256];
    lh_clear_obj(hmap);
    uint16_t mask = 0;

    for(i=65535; i>=0; i--) {
        if (ch->blocks[i].bid) {
            y=i>>8;
            mask |= (1<<(y>>4));
            if (!hmap[i&0xff]) hmap[i&0xff]=y;
        }
    }

    // Block data
    nbt_t * sections = nbt_new(NBT_LIST, "Sections", 0);

    for(y=0; y<16; y++) {
        if (!(mask&(1<<y))) continue;

        uint8_t blocks[4096];
        uint8_t data[2048];
        for(i=0; i<4096; i++) {
            blocks[i] = ch->blocks[i+(y<<12)].bid;
            uint8_t meta = ch->blocks[i+(y<<12)].meta;
            if (i&1)
                data[i/2] |= (meta<<4);
            else
                data[i/2] = meta;
        }

        nbt_t * cube = nbt_new(NBT_COMPOUND, NULL, 5,
            nbt_new(NBT_BYTE_ARRAY, "Blocks", blocks, 4096),
            nbt_new(NBT_BYTE_ARRAY, "SkyLight", ch->skylight+(y<<11), 2048),
            nbt_new(NBT_BYTE, "Y", y),
            nbt_new(NBT_BYTE_ARRAY, "BlockLight", ch->light+(y<<11), 2048),
            nbt_new(NBT_BYTE_ARRAY, "Data", data, 2048)
        );

        nbt_add(sections, cube);
    }

    // Chunk compound
    nbt_t *chunk = nbt_new(NBT_COMPOUND, "", 1,
        nbt_new(NBT_COMPOUND, "Level", 12,
            nbt_new(NBT_BYTE, "LightPopulated", 0),
            nbt_new(NBT_INT, "zPos", Z),
            nbt_new(NBT_INT_ARRAY, "HeightMap", hmap, 256),
            sections,        // Block/light data
            nbt_new(NBT_LONG, "LastUpdate", 1240000000), //TODO: adjust timestamp
            nbt_new(NBT_BYTE, "V", 1),
            nbt_new(NBT_BYTE_ARRAY, "Biomes", ch->biome, 256 ),
            nbt_new(NBT_LONG, "InhabitedTime", 0),
            nbt_new(NBT_INT, "xPos", X),
            nbt_new(NBT_BYTE, "TerrainPopulated", 1),
            tent,           // TileEntities
            ent             // Entities
        )
    );

    return chunk;
}
