#include <stdio.h>
#include <assert.h>

#include <lh_files.h>
#include <lh_bytes.h>

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
            printf("Chunk %d (%d,%d), off=%d, len=%d\n", i, i%32, i/32, choff, (chlen>>12));
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

